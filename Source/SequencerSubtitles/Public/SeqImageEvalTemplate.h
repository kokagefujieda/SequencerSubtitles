// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "SeqImageTypes.h"
#include "SeqImageEvalTemplate.generated.h"

class UMovieSceneSeqImageSection;

/** Runtime evaluation template for image sections. Effects are driven by the sequence time. */
USTRUCT()
struct SEQUENCERSUBTITLES_API FSeqImageEvalTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FSeqImageEvalTemplate() = default;
	explicit FSeqImageEvalTemplate(const UMovieSceneSeqImageSection& InSection);

	UPROPERTY()
	FSeqImageParams Params;

	UPROPERTY()
	FFrameNumber SectionStart;

	UPROPERTY()
	FFrameNumber SectionEnd;

	UPROPERTY()
	FFrameRate TickResolution = FFrameRate(24000, 1);

	/** Unique ID of the source section — used as the image slot key. */
	UPROPERTY()
	uint32 SlotID = 0;

#if WITH_EDITOR
	/** Transient pointer to the source section for drag-based position editing. */
	TWeakObjectPtr<UMovieSceneSeqImageSection> SourceSection;
#endif

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	virtual void SetupOverrides() override;
	virtual void TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
};
