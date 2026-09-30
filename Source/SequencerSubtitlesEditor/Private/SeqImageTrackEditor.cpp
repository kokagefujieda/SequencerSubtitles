// Copyright 2026 kokage. All Rights Reserved.

#include "SeqImageTrackEditor.h"
#include "SeqImageTrack.h"
#include "SeqImageSection.h"
#include "SubtitleSettings.h"

#include "ISequencer.h"
#include "SequencerSectionPainter.h"
#include "MovieScene.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#if ENGINE_MINOR_VERSION >= 7
#include "MVVM/Views/ViewUtilities.h"
#endif

#define LOCTEXT_NAMESPACE "SeqImageTrackEditor"

// --- FSeqImageSectionUI ---

FSeqImageSectionUI::FSeqImageSectionUI(UMovieSceneSection& InSection)
	: FSequencerSection(InSection)
{
}

int32 FSeqImageSectionUI::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	return Painter.PaintSectionBackground();
}

FText FSeqImageSectionUI::GetSectionTitle() const
{
	const UMovieSceneSeqImageSection* ImageSection = Cast<UMovieSceneSeqImageSection>(
		const_cast<FSeqImageSectionUI*>(this)->GetSectionObject());
	if (ImageSection && !ImageSection->Image.IsNull())
	{
		return FText::FromString(ImageSection->Image.GetAssetName());
	}
	return LOCTEXT("NoImage", "(No Image)");
}

// --- FSeqImageTrackEditor ---

TSharedRef<ISequencerTrackEditor> FSeqImageTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShareable(new FSeqImageTrackEditor(OwningSequencer));
}

FSeqImageTrackEditor::FSeqImageTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

#if ENGINE_MINOR_VERSION >= 6
FText FSeqImageTrackEditor::GetDisplayName() const
{
	return LOCTEXT("TrackDisplayName", "Image Track");
}
#endif

bool FSeqImageTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass == UMovieSceneSeqImageTrack::StaticClass();
}

TSharedRef<ISequencerSection> FSeqImageTrackEditor::MakeSectionInterface(
	UMovieSceneSection& SectionObject,
	UMovieSceneTrack& Track,
	FGuid ObjectBinding)
{
	return MakeShared<FSeqImageSectionUI>(SectionObject);
}

void FSeqImageTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddImageTrack", "Image Track"),
		LOCTEXT("AddImageTrackTooltip", "Add a track that shows images on screen with entrance / exit effects"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
		FUIAction(FExecuteAction::CreateSP(this, &FSeqImageTrackEditor::HandleAddImageTrack))
	);
}

TSharedPtr<SWidget> FSeqImageTrackEditor::BuildOutlinerEditWidget(
	const FGuid& ObjectBinding,
	UMovieSceneTrack* Track,
	const FBuildEditWidgetParams& Params)
{
	if (!Cast<UMovieSceneSeqImageTrack>(Track))
	{
		return TSharedPtr<SWidget>();
	}

	TWeakObjectPtr<UMovieSceneTrack> WeakTrack = Track;

	// --- "+" Add Section button (same style as the subtitle track) ---
#if ENGINE_MINOR_VERSION >= 7
	return UE::Sequencer::MakeAddButton(
		LOCTEXT("AddImageSection", "Image"),
		FOnClicked::CreateLambda([this, WeakTrack]() -> FReply
		{
			if (UMovieSceneTrack* TrackPtr = WeakTrack.Get())
			{
				AddNewSectionToTrack(TrackPtr);
			}
			return FReply::Handled();
		}),
		Params.ViewModel);
#else
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "FlatButton")
		.ContentPadding(FMargin(2, 0))
		.OnClicked_Lambda([this, WeakTrack]() -> FReply
		{
			if (UMovieSceneTrack* TrackPtr = WeakTrack.Get())
			{
				AddNewSectionToTrack(TrackPtr);
			}
			return FReply::Handled();
		})
		.ToolTipText(LOCTEXT("AddImageSectionTooltip", "Add a new image section"))
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "NormalText.Important")
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.Text(LOCTEXT("AddImageSectionButton", "+ Section"))
		];
#endif
}

void FSeqImageTrackEditor::AddNewSectionToTrack(UMovieSceneTrack* Track)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!SequencerPtr.IsValid() || !Track || !FocusedMovieScene || FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddImageSection_Transaction", "Add Image Section"));
	FocusedMovieScene->Modify();
	Track->Modify();

	UMovieSceneSection* NewSection = Track->CreateNewSection();
	if (!NewSection)
	{
		return;
	}

	// Start at the playhead with the default subtitle section duration
	const FFrameNumber CurrentTime    = SequencerPtr->GetLocalTime().Time.GetFrame();
	const FFrameRate   TickResolution = FocusedMovieScene->GetTickResolution();
	const USubtitleSettings* Settings = GetDefault<USubtitleSettings>();
	const float DefaultDuration = Settings ? Settings->DefaultSectionDuration : 3.0f;
	const FFrameNumber Duration = (static_cast<double>(DefaultDuration) * TickResolution).FloorToFrame();
	NewSection->SetRange(TRange<FFrameNumber>(CurrentTime, CurrentTime + Duration));

	int32 OverlapPriority = 0;
	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		OverlapPriority = FMath::Max(Section->GetOverlapPriority() + 1, OverlapPriority);
	}
	NewSection->SetOverlapPriority(OverlapPriority);

	Track->AddSection(*NewSection);

	SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	SequencerPtr->EmptySelection();
	SequencerPtr->SelectSection(NewSection);
	SequencerPtr->ThrobSectionSelection();
}

void FSeqImageTrackEditor::HandleAddImageTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!FocusedMovieScene || FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddImageTrack_Transaction", "Add Image Track"));
	FocusedMovieScene->Modify();

	UMovieSceneSeqImageTrack* NewTrack = FocusedMovieScene->AddTrack<UMovieSceneSeqImageTrack>();
	if (!NewTrack)
	{
		return;
	}

	// One section at the start of the playback range
	if (UMovieSceneSection* NewSection = NewTrack->CreateNewSection())
	{
		if (FocusedMovieScene->GetPlaybackRange().HasLowerBound())
		{
			const FFrameNumber Start = FocusedMovieScene->GetPlaybackRange().GetLowerBoundValue();
			const FFrameRate TickResolution = FocusedMovieScene->GetTickResolution();
			const USubtitleSettings* Settings = GetDefault<USubtitleSettings>();
			const float DefaultDuration = Settings ? Settings->DefaultSectionDuration : 3.0f;
			const FFrameNumber Duration = (static_cast<double>(DefaultDuration) * TickResolution).FloorToFrame();
			NewSection->SetRange(TRange<FFrameNumber>(Start, Start + Duration));
		}
		NewTrack->AddSection(*NewSection);
	}

	if (TSharedPtr<ISequencer> SequencerPtr = GetSequencer())
	{
		SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

#undef LOCTEXT_NAMESPACE
