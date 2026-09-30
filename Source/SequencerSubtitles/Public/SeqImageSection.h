// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "SeqImageTypes.h"
#include "SeqImageSection.generated.h"

/** Single image on the Sequencer timeline. */
UCLASS()
class SEQUENCERSUBTITLES_API UMovieSceneSeqImageSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneSeqImageSection();

	/** Texture to display. Use the "UserInterface2D" texture group for crisp UI images. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image")
	TSoftObjectPtr<UTexture2D> Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ShowOnlyInnerProperties))
	FSeqImageLayout Layout;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance", meta = (ShowOnlyInnerProperties))
	FSeqImageTransition Entrance;

	/** false = the exit plays the entrance backwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	bool bOverrideExit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit", meta = (EditCondition = "bOverrideExit", ShowOnlyInnerProperties))
	FSeqImageTransition Exit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuous Effect", meta = (ShowOnlyInnerProperties))
	FSeqImageContinuous Continuous;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ShowOnlyInnerProperties))
	FSeqImageMotion Motion;

	// ---- Keyframes (combined with the settings above) ----

	/** Added to the offset (pixels). */
	UPROPERTY()
	FMovieSceneFloatChannel OffsetXCurve;

	/** Added to the offset (pixels). */
	UPROPERTY()
	FMovieSceneFloatChannel OffsetYCurve;

	/** Multiplies the scale. */
	UPROPERTY()
	FMovieSceneFloatChannel ScaleCurve;

	/** Added to the rotation (degrees). */
	UPROPERTY()
	FMovieSceneFloatChannel RotationCurve;

	/** Multiplies the opacity. */
	UPROPERTY()
	FMovieSceneFloatChannel OpacityCurve;

	/** Collect the display settings for evaluation. */
	FSeqImageParams MakeParams() const;

protected:
	virtual EMovieSceneChannelProxyType CacheChannelProxy() override;
};
