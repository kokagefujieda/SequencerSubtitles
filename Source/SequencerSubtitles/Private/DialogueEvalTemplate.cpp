// Copyright 2026 kokage. All Rights Reserved.

#include "DialogueEvalTemplate.h"
#include "DialogueSection.h"
#include "DialogueTrack.h"
#include "MovieScene.h"
#include "DialogueSubsystem.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneExecutionToken.h"
#include "Engine/World.h"

/** Execution token: notifies UDialogueSubsystem on game thread. */
struct FDialogueExecutionToken : IMovieSceneExecutionToken
{
	FText         SpeakerName;
	FText         DialogueText;
	FLinearColor  Color;
	FDialogueAppearance Appearance;
	/** -1 = show full text; >= 0 = typewriter: show this many characters. */
	int32         VisibleCharCount = -1;

	FDialogueExecutionToken(const FText& InSpeakerName, const FText& InText, FLinearColor InColor,
		const FDialogueAppearance& InAppearance, int32 InVisibleCharCount)
		: SpeakerName(InSpeakerName)
		, DialogueText(InText)
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

		UDialogueSubsystem* Subsystem = World->GetSubsystem<UDialogueSubsystem>();
		if (!Subsystem) { return; }

		// Already showing this dialogue
		if (Subsystem->bIsDialogueActive && Subsystem->CurrentDialogueText.EqualTo(DialogueText))
		{
			// For typewriter: update visible character count every frame
			if (VisibleCharCount >= 0)
			{
				Subsystem->UpdateTypewriterProgress(VisibleCharCount);
			}
			return;
		}

		// New dialogue — start it
		Subsystem->NotifyDialogueStarted(DialogueText, Color, Appearance, SpeakerName);

		// For typewriter: override initial display immediately (before Slate renders)
		if (VisibleCharCount >= 0)
		{
			Subsystem->UpdateTypewriterProgress(VisibleCharCount);
		}
	}
};

FDialogueEvalTemplate::FDialogueEvalTemplate(const UMovieSceneDialogueSection& InSection)
{
	DialogueText = InSection.DialogueText;
	BarColor     = InSection.BarColor;

	const UMovieSceneDialogueTrack* Track = InSection.GetTypedOuter<UMovieSceneDialogueTrack>();

	// Speaker name: Section override → Track setting → Track display name
	if (InSection.bOverrideSpeakerName && !InSection.SpeakerNameOverride.IsEmptyOrWhitespace())
	{
		SpeakerName = InSection.SpeakerNameOverride;
	}
	else if (Track)
	{
		SpeakerName = Track->GetEffectiveSpeakerName();
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

void FDialogueEvalTemplate::SetupOverrides()
{
	EnableOverrides(RequiresTearDownFlag);
}

void FDialogueEvalTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	UObject* PlaybackContext = Player.GetPlaybackContext();
	if (!PlaybackContext) { return; }

	UWorld* World = PlaybackContext->GetWorld();
	if (!World) { return; }

	UDialogueSubsystem* Subsystem = World->GetSubsystem<UDialogueSubsystem>();
	if (!Subsystem) { return; }

	if (Subsystem->bIsDialogueActive && Subsystem->CurrentDialogueText.EqualTo(DialogueText))
	{
		Subsystem->NotifyDialogueEnded();
	}
}

void FDialogueEvalTemplate::Evaluate(
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
			const int32  TotalChars  = DialogueText.ToString().Len();

			// Interval-based: 0.1 sec/char (or user-defined)
			const double TicksPerSec = TypewriterTickResolution.AsDecimal();
			const float  ElapsedSec  = (TicksPerSec > 0.0) ? (float)(Elapsed / TicksPerSec) : 0.f;
			const int32  ByInterval  = FMath::FloorToInt(ElapsedSec / TypewriterCharInterval);

			// Section-forced: ensures all chars show by section end (fallback if section is short)
			const float  Progress    = FMath::Clamp((float)Elapsed / SectionDuration, 0.0f, 1.0f);
			const int32  BySection   = FMath::FloorToInt(Progress * TotalChars);

			// Use whichever reveals more characters (interval wins normally, section wins if short)
			VisibleCharCount = FMath::Clamp(FMath::Max(ByInterval, BySection), 0, TotalChars);
		}
	}

	ExecutionTokens.Add(FDialogueExecutionToken(SpeakerName, DialogueText, BarColor, Appearance, VisibleCharCount));
}
