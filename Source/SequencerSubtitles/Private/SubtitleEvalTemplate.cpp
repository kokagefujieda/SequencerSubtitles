// Copyright 2026 kokage. All Rights Reserved.

#include "SubtitleEvalTemplate.h"
#include "SubtitleSection.h"
#include "SubtitleTrack.h"
#include "SubtitleSubsystem.h"
#include "MovieScene.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneExecutionToken.h"
#include "Engine/World.h"

/** Execution token: notifies USubtitleSubsystem on game thread. */
struct FSubtitleExecutionToken : IMovieSceneExecutionToken
{
	FText         SpeakerName;
	FText         SubtitleText;
	FLinearColor  Color;
	FSubtitleAppearance Appearance;
	/** -1 = show full text; >= 0 = typewriter: show this many characters. */
	int32         VisibleCharCount = -1;

	FSubtitleExecutionToken(const FText& InSpeakerName, const FText& InText, FLinearColor InColor,
		const FSubtitleAppearance& InAppearance, int32 InVisibleCharCount)
		: SpeakerName(InSpeakerName)
		, SubtitleText(InText)
		, Color(InColor)
		, Appearance(InAppearance)
		, VisibleCharCount(InVisibleCharCount)
	{
	}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		UObject* PlaybackContext = Player.GetPlaybackContext();
		if (!PlaybackContext) { return; }

		UWorld* World = PlaybackContext->GetWorld();
		if (!World) { return; }

		USubtitleSubsystem* Subsystem = World->GetSubsystem<USubtitleSubsystem>();
		if (!Subsystem) { return; }

		// Already showing this subtitle
		if (Subsystem->bIsSubtitleActive && Subsystem->CurrentSubtitleText.EqualTo(SubtitleText))
		{
			// For typewriter: update visible character count every frame
			if (VisibleCharCount >= 0)
			{
				Subsystem->UpdateTypewriterProgress(VisibleCharCount);
			}
			return;
		}

		// New subtitle — start it
		Subsystem->NotifySubtitleStarted(SubtitleText, Color, Appearance, SpeakerName);

		// For typewriter: override initial display immediately (before Slate renders)
		if (VisibleCharCount >= 0)
		{
			Subsystem->UpdateTypewriterProgress(VisibleCharCount);
		}
	}
};

FSubtitleEvalTemplate::FSubtitleEvalTemplate(const UMovieSceneSeqSubtitleSection& InSection)
{
	BarColor     = InSection.BarColor;

	const UMovieSceneSubtitleTrack* Track = InSection.GetTypedOuter<UMovieSceneSubtitleTrack>();

	// Speaker name: Section override → Track setting → Binding Name → Track display name
	if (InSection.bOverrideSpeakerName && !InSection.SpeakerNameOverride.IsEmptyOrWhitespace())
	{
		SpeakerName = InSection.SpeakerNameOverride;
	}
	else if (Track && Track->bOverrideSpeakerName && !Track->SpeakerNameOverride.IsEmptyOrWhitespace())
	{
		SpeakerName = Track->SpeakerNameOverride;
	}
	else if (Track)
	{
		bool bFoundBindingName = false;
		if (const UMovieScene* MovieScene = Track->GetTypedOuter<UMovieScene>())
		{
			for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
			{
				if (Binding.GetTracks().Contains(Track))
				{
					FString BindingName = Binding.GetName();
					if (!BindingName.IsEmpty())
					{
						SpeakerName = FText::FromString(BindingName);
						bFoundBindingName = true;
					}
					break;
				}
			}
		}

		if (!bFoundBindingName)
		{
			SpeakerName = Track->GetEffectiveSpeakerName();
		}
	}

	// Appearance: Section override → Track setting
	if (InSection.bOverrideAppearance)
	{
		Appearance = InSection.AppearanceOverride;
	}
	else if (Track)
	{
		Appearance = Track->Appearance;
	}

	// Apply MaxCharsPerLine wrapping so TotalChars matches the wrapped text
	if (Appearance.MaxCharsPerLine > 0)
	{
		SubtitleText = FText::FromString(
			USubtitleSubsystem::WrapTextByCharLimit(InSection.SubtitleText.ToString(), Appearance.MaxCharsPerLine));
	}
	else
	{
		SubtitleText = InSection.SubtitleText;
	}

	bTypewriterEffect = InSection.bTypewriterEffect;
	if (bTypewriterEffect)
	{
		const TRange<FFrameNumber>& Range = InSection.GetRange();
		if (Range.HasLowerBound()) { TypewriterSectionStart = Range.GetLowerBoundValue(); }
		if (Range.HasUpperBound()) { TypewriterSectionEnd   = Range.GetUpperBoundValue(); }

		TypewriterCharInterval = FMath::Max(InSection.TypewriterCharInterval, 0.01f);

		if (const UMovieScene* MovieScene = InSection.GetTypedOuter<UMovieScene>())
		{
			TypewriterTickResolution = MovieScene->GetTickResolution();
		}
	}
}

void FSubtitleEvalTemplate::SetupOverrides()
{
	EnableOverrides(RequiresTearDownFlag);
}

void FSubtitleEvalTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	UObject* PlaybackContext = Player.GetPlaybackContext();
	if (!PlaybackContext) { return; }

	UWorld* World = PlaybackContext->GetWorld();
	if (!World) { return; }

	USubtitleSubsystem* Subsystem = World->GetSubsystem<USubtitleSubsystem>();
	if (!Subsystem) { return; }

	if (Subsystem->bIsSubtitleActive && Subsystem->CurrentSubtitleText.EqualTo(SubtitleText))
	{
		Subsystem->NotifySubtitleEnded();
	}
}

void FSubtitleEvalTemplate::Evaluate(
	const FMovieSceneEvaluationOperand& Operand,
	const FMovieSceneContext& Context,
	const FPersistentEvaluationData& PersistentData,
	FMovieSceneExecutionTokens& ExecutionTokens) const
{
	int32 VisibleCharCount = -1;

	if (bTypewriterEffect)
	{
		const int32 SectionDuration = TypewriterSectionEnd.Value - TypewriterSectionStart.Value;
		if (SectionDuration > 0)
		{
			const int32  Elapsed     = Context.GetTime().GetFrame().Value - TypewriterSectionStart.Value;
			const int32  TotalChars  = SubtitleText.ToString().Len();

			// Interval-based: 0.1 sec/char (or user-defined)
			const double TicksPerSec = TypewriterTickResolution.AsDecimal();
			const float  ElapsedSec  = (TicksPerSec > 0.0) ? (float)(Elapsed / TicksPerSec) : 0.f;
			const int32  ByInterval  = FMath::FloorToInt(ElapsedSec / TypewriterCharInterval);

			// Section-forced: ensures all chars show by the last evaluated frame.
			// The range is [Start, End) (exclusive upper), so the last frame has
			// Elapsed = SectionDuration - 1. Use (SectionDuration - 1) as the
			// denominator so Progress reaches 1.0 on that frame.
			const int32  EffectiveDuration = FMath::Max(SectionDuration - 1, 1);
			const float  Progress    = FMath::Clamp((float)Elapsed / EffectiveDuration, 0.0f, 1.0f);
			const int32  BySection   = FMath::CeilToInt(Progress * TotalChars);

			// Use whichever reveals more characters (interval wins normally, section wins if short)
			VisibleCharCount = FMath::Clamp(FMath::Max(ByInterval, BySection), 0, TotalChars);
		}
	}

	ExecutionTokens.Add(FSubtitleExecutionToken(SpeakerName, SubtitleText, BarColor, Appearance, VisibleCharCount));
}
