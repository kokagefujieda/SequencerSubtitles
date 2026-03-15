// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrackEditor.h"
#include "ISequencerSection.h"

class UMovieSceneDialogueTrack;
class UMovieSceneDialogueSection;

/** Section UI: bar background + dialogue text on timeline. */
class FDialogueSectionUI : public FSequencerSection
{
public:
	FDialogueSectionUI(UMovieSceneSection& InSection);

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
class FDialogueTrackEditor : public FMovieSceneTrackEditor
{
public:
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

	FDialogueTrackEditor(TSharedRef<ISequencer> InSequencer);

	virtual FText GetDisplayName() const override;
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
	void HandleAddDialogueTrack();
	void AddNewSectionToTrack(UMovieSceneTrack* Track);
};
