// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "SubtitleSettings.h"
#include "SubtitleEvalTemplate.generated.h"

/** Runtime evaluation template for subtitle sections. */
USTRUCT()
struct SEQUENCERSUBTITLES_API FSubtitleEvalTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FSubtitleEvalTemplate() = default;
	explicit FSubtitleEvalTemplate(const class UMovieSceneSeqSubtitleSection& InSection);

	UPROPERTY()
	FText SpeakerName;

	UPROPERTY()
	FText SubtitleText;

	UPROPERTY()
	FLinearColor BarColor = FLinearColor::Black;

	UPROPERTY()
	FSubtitleAppearance Appearance;

	UPROPERTY()
	bool bTypewriterEffect = false;

	UPROPERTY()
	FFrameNumber TypewriterSectionStart;

	UPROPERTY()
	FFrameNumber TypewriterSectionEnd;

	UPROPERTY()
	float TypewriterCharInterval = 0.1f;

	UPROPERTY()
	FFrameRate TypewriterTickResolution = FFrameRate(24000, 1);

	/** Unique ID of the source section — used as slot key for multi-subtitle support. */
	UPROPERTY()
	uint32 SlotID = 0;

#if WITH_EDITOR
	/** Transient pointer to source section for drag-based position editing. */
	TWeakObjectPtr<UMovieSceneSeqSubtitleSection> SourceSection;
#endif

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	virtual void SetupOverrides() override;
	virtual void TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
};
