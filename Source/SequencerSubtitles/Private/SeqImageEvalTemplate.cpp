// Copyright 2026 kokage. All Rights Reserved.

#include "SeqImageEvalTemplate.h"
#include "SeqImageSection.h"
#include "SubtitleSubsystem.h"
#include "MovieScene.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneExecutionToken.h"
#include "Engine/World.h"

namespace
{
	USubtitleSubsystem* GetSubsystemFromPlayer(IMovieScenePlayer& Player)
	{
		UObject* PlaybackContext = Player.GetPlaybackContext();
		UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
		return World ? World->GetSubsystem<USubtitleSubsystem>() : nullptr;
	}
}

/** Execution token: pushes the image state for the current time to USubtitleSubsystem. */
struct FSeqImageExecutionToken : IMovieSceneExecutionToken
{
	FSeqImageParams Params;
	FSeqImageKeyedValues Keyed;
	float  LocalTime = 0.f;
	float  Duration  = 0.f;
	uint32 SlotID    = 0;
#if WITH_EDITOR
	TWeakObjectPtr<UMovieSceneSeqImageSection> SourceSection;
#endif

	FSeqImageExecutionToken(const FSeqImageParams& InParams, float InLocalTime, float InDuration, uint32 InSlotID)
		: Params(InParams), LocalTime(InLocalTime), Duration(InDuration), SlotID(InSlotID)
	{
	}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		USubtitleSubsystem* Subsystem = GetSubsystemFromPlayer(Player);
		if (!Subsystem) { return; }

		UMovieSceneSeqImageSection* Section = nullptr;
#if WITH_EDITOR
		Section = SourceSection.Get();
#endif
		Subsystem->UpdateImage(SlotID, Params, Keyed, LocalTime, Duration, Section);
	}
};

FSeqImageEvalTemplate::FSeqImageEvalTemplate(const UMovieSceneSeqImageSection& InSection)
{
	Params = InSection.MakeParams();

	OffsetXCurve  = InSection.OffsetXCurve;
	OffsetYCurve  = InSection.OffsetYCurve;
	ScaleCurve    = InSection.ScaleCurve;
	RotationCurve = InSection.RotationCurve;
	OpacityCurve  = InSection.OpacityCurve;

	const TRange<FFrameNumber>& Range = InSection.GetRange();
	if (Range.HasLowerBound()) { SectionStart = Range.GetLowerBoundValue(); }
	if (Range.HasUpperBound()) { SectionEnd   = Range.GetUpperBoundValue(); }

	if (const UMovieScene* MovieScene = InSection.GetTypedOuter<UMovieScene>())
	{
		TickResolution = MovieScene->GetTickResolution();
	}

	SlotID = InSection.GetUniqueID();

#if WITH_EDITOR
	SourceSection = const_cast<UMovieSceneSeqImageSection*>(&InSection);
#endif
}

void FSeqImageEvalTemplate::SetupOverrides()
{
	EnableOverrides(RequiresTearDownFlag);
}

void FSeqImageEvalTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	if (USubtitleSubsystem* Subsystem = GetSubsystemFromPlayer(Player))
	{
		Subsystem->RemoveImage(SlotID);
	}
}

void FSeqImageEvalTemplate::Evaluate(
	const FMovieSceneEvaluationOperand& Operand,
	const FMovieSceneContext& Context,
	const FPersistentEvaluationData& PersistentData,
	FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const double TicksPerSec = TickResolution.AsDecimal();
	if (TicksPerSec <= 0.0) { return; }

	// Sub-frame accurate time so effects stay smooth at any frame rate
	const FFrameTime LocalTicks = Context.GetTime() - FFrameTime(SectionStart);
	const float LocalTime = static_cast<float>(LocalTicks.AsDecimal() / TicksPerSec);
	const float Duration  = static_cast<float>((SectionEnd - SectionStart).Value / TicksPerSec);

	FSeqImageExecutionToken Token(Params, LocalTime, Duration, SlotID);

	// Keyframes at the current (sequence) time; channels without keys return their neutral default
	const FFrameTime Time = Context.GetTime();
	float Value = 0.f;
	if (OffsetXCurve.Evaluate(Time, Value))  { Token.Keyed.Offset.X = Value; }
	if (OffsetYCurve.Evaluate(Time, Value))  { Token.Keyed.Offset.Y = Value; }
	if (ScaleCurve.Evaluate(Time, Value))    { Token.Keyed.Scale    = Value; }
	if (RotationCurve.Evaluate(Time, Value)) { Token.Keyed.Rotation = Value; }
	if (OpacityCurve.Evaluate(Time, Value))  { Token.Keyed.Opacity  = Value; }
#if WITH_EDITOR
	Token.SourceSection = SourceSection;
#endif
	ExecutionTokens.Add(MoveTemp(Token));
}
