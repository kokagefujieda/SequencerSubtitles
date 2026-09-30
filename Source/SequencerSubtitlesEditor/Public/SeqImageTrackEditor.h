// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrackEditor.h"
#include "ISequencerSection.h"
#include "Styling/SlateBrush.h"

class UTexture2D;
class UMovieSceneSeqImageSection;

/** Section UI: bar background, image name and a thumbnail at the right end. */
class FSeqImageSectionUI : public FSequencerSection
{
public:
	FSeqImageSectionUI(UMovieSceneSection& InSection);

	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
	virtual FText GetSectionTitle() const override;
#if ENGINE_MINOR_VERSION >= 7
	virtual float GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const override;
#else
	virtual float GetSectionHeight() const override;
#endif

private:
	/** Brush for the thumbnail (re-pointed at the section's texture when painting). */
	mutable FSlateBrush ThumbnailBrush;
};

/** Track editor for the Image Track: menu registration + section creation. */
class FSeqImageTrackEditor : public FMovieSceneTrackEditor
{
public:
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

	FSeqImageTrackEditor(TSharedRef<ISequencer> InSequencer);

#if ENGINE_MINOR_VERSION >= 6
	virtual FText GetDisplayName() const override;
#endif
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(
		UMovieSceneSection& SectionObject,
		UMovieSceneTrack& Track,
		FGuid ObjectBinding) override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(
		const FGuid& ObjectBinding,
		UMovieSceneTrack* Track,
		const FBuildEditWidgetParams& Params) override;

	/** A texture dropped from the Content Browser becomes an image section at the playhead. */
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;

private:
	void HandleAddImageTrack();

	/** Add a section at the playhead (with Texture if given). Returns the new section. */
	UMovieSceneSeqImageSection* AddNewSectionToTrack(UMovieSceneTrack* Track, UTexture2D* Texture = nullptr);
};
