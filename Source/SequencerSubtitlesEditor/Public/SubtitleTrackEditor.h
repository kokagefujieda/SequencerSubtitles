// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrackEditor.h"
#include "ISequencerSection.h"

class UMovieSceneSubtitleTrack;
class UMovieSceneSeqSubtitleSection;

/** Section UI: bar background + subtitle text on timeline. */
class FSubtitleSectionUI : public FSequencerSection
{
public:
	FSubtitleSectionUI(UMovieSceneSection& InSection);

	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
	virtual FText GetSectionTitle() const override;
#if ENGINE_MINOR_VERSION >= 7
	virtual float GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const override;
#else
	virtual float GetSectionHeight() const override;
#endif
	virtual FReply OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent) override;
};

/** Track editor: menu registration + section creation. */
class FSubtitleTrackEditor : public FMovieSceneTrackEditor
{
public:
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

	FSubtitleTrackEditor(TSharedRef<ISequencer> InSequencer);

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
	virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override;

private:
	void HandleAddSubtitleTrack();
	void AddNewSectionToTrack(UMovieSceneTrack* Track);
};
