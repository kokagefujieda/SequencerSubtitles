// Copyright 2026 kokage. All Rights Reserved.

#include "SubtitleEvalTemplate.h"
#include "SubtitleSection.h"
#include "SubtitleTrack.h"
#include "SubtitleSubsystem.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
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
	uint32        SlotID           = 0;
	/** Seconds since the section start, and the section length (entrance / exit / tremble use them). */
	float         LocalTime        = 0.f;
	float         Duration         = 0.f;
#if WITH_EDITOR
	TWeakObjectPtr<UMovieSceneSeqSubtitleSection> SourceSection;
#endif

	FSubtitleExecutionToken(const FText& InSpeakerName, const FText& InText, FLinearColor InColor,
		const FSubtitleAppearance& InAppearance, int32 InVisibleCharCount, uint32 InSlotID)
		: SpeakerName(InSpeakerName)
		, SubtitleText(InText)
		, Color(InColor)
		, Appearance(InAppearance)
		, VisibleCharCount(InVisibleCharCount)
		, SlotID(InSlotID)
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

		// Outside the section (e.g. nearest-section evaluation in a gap): nothing to show
		if (Duration <= 0.f || LocalTime < 0.f || LocalTime >= Duration)
		{
			if (Subsystem->IsSlotActive(SlotID))
			{
				Subsystem->NotifySubtitleEnded(SlotID);
			}
			return;
		}

#if WITH_EDITOR
		Subsystem->SetActiveSection(SlotID, SourceSection.Get());
#endif

		// New subtitle — start it
		if (!Subsystem->IsSlotActive(SlotID))
		{
			Subsystem->NotifySubtitleStarted(SlotID, SubtitleText, Color, Appearance, SpeakerName);
		}

		// Entrance / exit / tremble follow the sequence time (scrubbing and Movie Render Queue match playback)
		Subsystem->UpdateSubtitleTime(SlotID, LocalTime, Duration);

		// Typewriter progress (before Slate renders)
		if (VisibleCharCount >= 0)
		{
			Subsystem->UpdateTypewriterProgress(SlotID, VisibleCharCount);
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
		if (UMovieScene* MovieScene = Track->GetTypedOuter<UMovieScene>())
		{
			for (const FMovieSceneBinding& Binding : const_cast<const UMovieScene*>(MovieScene)->GetBindings())
			{
				if (Binding.GetTracks().Contains(Track))
				{
					const FGuid& BindingGuid = Binding.GetObjectGuid();
					FString BindingName;
					if (FMovieScenePossessable* Possessable = MovieScene->FindPossessable(BindingGuid))
					{
						BindingName = Possessable->GetName();
					}
					else if (FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(BindingGuid))
					{
						BindingName = Spawnable->GetName();
					}
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

#if WITH_EDITOR
	SourceSection = const_cast<UMovieSceneSeqSubtitleSection*>(&InSection);
#endif

	// Section range and tick resolution (used for the animation clock and the typewriter)
	const TRange<FFrameNumber>& Range = InSection.GetRange();
	if (Range.HasLowerBound()) { TypewriterSectionStart = Range.GetLowerBoundValue(); }
	if (Range.HasUpperBound()) { TypewriterSectionEnd   = Range.GetUpperBoundValue(); }

	if (const UMovieScene* MovieScene = InSection.GetTypedOuter<UMovieScene>())
	{
		TypewriterTickResolution = MovieScene->GetTickResolution();
	}

	bTypewriterEffect = InSection.bTypewriterEffect;
	if (bTypewriterEffect)
	{
		TypewriterCharInterval = FMath::Max(InSection.TypewriterCharInterval, 0.01f);
	}

	SlotID = InSection.GetUniqueID();
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

	Subsystem->NotifySubtitleEnded(SlotID);
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

			// Section-forced: ensures all chars show before the exit animation starts
			// (the exit plays at the end of the section). The range is [Start, End)
			// (exclusive upper), so the last frame has Elapsed = SectionDuration - 1.
			const float  ExitSec = (Appearance.GetEffectiveExitType() != ESubtitleEntranceType::None)
				? FMath::Max(Appearance.GetEffectiveExitDuration(), 0.f) : 0.f;
			const int32  TypingDuration    = FMath::Max(SectionDuration - FMath::RoundToInt(ExitSec * TicksPerSec), 1);
			const int32  EffectiveDuration = FMath::Max(TypingDuration - 1, 1);
			const float  Progress    = FMath::Clamp((float)Elapsed / EffectiveDuration, 0.0f, 1.0f);
			const int32  BySection   = FMath::CeilToInt(Progress * TotalChars);

			// Use whichever reveals more characters (interval wins normally, section wins if short)
			VisibleCharCount = FMath::Clamp(FMath::Max(ByInterval, BySection), 0, TotalChars);
		}
	}

	FSubtitleExecutionToken Token(SpeakerName, SubtitleText, BarColor, Appearance, VisibleCharCount, SlotID);

	// Sub-frame accurate section-local time
	const double TicksPerSec = TypewriterTickResolution.AsDecimal();
	if (TicksPerSec > 0.0)
	{
		const FFrameTime LocalTicks = Context.GetTime() - FFrameTime(TypewriterSectionStart);
		Token.LocalTime = static_cast<float>(LocalTicks.AsDecimal() / TicksPerSec);
		Token.Duration  = static_cast<float>((TypewriterSectionEnd - TypewriterSectionStart).Value / TicksPerSec);
	}
#if WITH_EDITOR
	Token.SourceSection = SourceSection;
#endif
	ExecutionTokens.Add(MoveTemp(Token));
}
