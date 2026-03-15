// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "DialogueSettings.h"
#include "DialogueEvalTemplate.generated.h"

/** Runtime evaluation template for dialogue sections. */
USTRUCT()
struct SEQUENCERSUBTITLES_API FDialogueEvalTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FDialogueEvalTemplate() = default;
	explicit FDialogueEvalTemplate(const class UMovieSceneDialogueSection& InSection);

	UPROPERTY()
	FText SpeakerName;

	UPROPERTY()
	FText DialogueText;

	UPROPERTY()
	FLinearColor BarColor;

	UPROPERTY()
	FDialogueAppearance Appearance;

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

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	virtual void SetupOverrides() override;
	virtual void TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
};
