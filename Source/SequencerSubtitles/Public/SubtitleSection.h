// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "SubtitleSettings.h"
#include "SubtitleSection.generated.h"

/** Single subtitle section on the Sequencer timeline. */
UCLASS()
class SEQUENCERSUBTITLES_API UMovieSceneSeqSubtitleSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneSeqSubtitleSection();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Override the track-level speaker name for this section only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle")
	bool bOverrideSpeakerName = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle",
		meta = (EditCondition = "bOverrideSpeakerName"))
	FText SpeakerNameOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle",
		meta = (MultiLine = "true"))
	FText SubtitleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle")
	FLinearColor BarColor;

	/**
	 * When enabled, characters are revealed one by one at the rate set by TypewriterCharInterval.
	 * If the section is shorter than the time needed to reveal all characters,
	 * the text is forced to complete by the section end.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle")
	bool bTypewriterEffect = false;

	/**
	 * Seconds between each character reveal (typewriter speed).
	 * Default 0.1 = 10 characters per second.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle",
		meta = (EditCondition = "bTypewriterEffect", ClampMin = "0.01", UIMin = "0.01"))
	float TypewriterCharInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance")
	bool bOverrideAppearance = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance",
		meta = (EditCondition = "bOverrideAppearance", ShowOnlyInnerProperties))
	FSubtitleAppearance AppearanceOverride;

	FORCEINLINE FText GetDisplayText() const
	{
		return SubtitleText.IsEmptyOrWhitespace() ? NSLOCTEXT("SequencerSubtitles", "EmptySubtitle", "(Empty)") : SubtitleText;
	}
};
