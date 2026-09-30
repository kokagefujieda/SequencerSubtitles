// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrackEditor.h"
#include "ISequencerSection.h"

/** Section UI: bar background + image name on the timeline. */
class FSeqImageSectionUI : public FSequencerSection
{
public:
	FSeqImageSectionUI(UMovieSceneSection& InSection);

	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
	virtual FText GetSectionTitle() const override;
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

private:
	void HandleAddImageTrack();
	void AddNewSectionToTrack(UMovieSceneTrack* Track);
};
