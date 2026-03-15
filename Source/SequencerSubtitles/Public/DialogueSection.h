// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "DialogueSettings.h"
#include "DialogueSection.generated.h"

/** Single dialogue section on the Sequencer timeline. */
UCLASS()
class SEQUENCERSUBTITLES_API UMovieSceneDialogueSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneDialogueSection();

	/** Override the track-level speaker name for this section only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bOverrideSpeakerName = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue",
		meta = (EditCondition = "bOverrideSpeakerName"))
	FText SpeakerNameOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue",
		meta = (MultiLine = "true"))
	FText DialogueText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FLinearColor BarColor;

	/**
	 * When enabled, characters are revealed one by one at the rate set by TypewriterCharInterval.
	 * If the section is shorter than the time needed to reveal all characters,
	 * the text is forced to complete by the section end.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bTypewriterEffect = false;

	/**
	 * Seconds between each character reveal (typewriter speed).
	 * Default 0.1 = 10 characters per second.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue",
		meta = (EditCondition = "bTypewriterEffect", ClampMin = "0.01", UIMin = "0.01"))
	float TypewriterCharInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance")
	bool bOverrideAppearance = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance",
		meta = (EditCondition = "bOverrideAppearance", ShowOnlyInnerProperties))
	FDialogueAppearance AppearanceOverride;

	FORCEINLINE FText GetDisplayText() const
	{
		return DialogueText.IsEmptyOrWhitespace() ? NSLOCTEXT("SequencerSubtitles", "EmptyDialogue", "(Empty)") : DialogueText;
	}
};
