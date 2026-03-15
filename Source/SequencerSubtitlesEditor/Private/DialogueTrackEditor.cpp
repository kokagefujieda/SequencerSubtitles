// Copyright 2026 kokage. All Rights Reserved.

#include "DialogueTrackEditor.h"
#include "DialogueTrack.h"
#include "DialogueSection.h"

#include "ISequencer.h"
#include "SequencerSectionPainter.h"
#include "MovieScene.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Styling/AppStyle.h"
#if ENGINE_MINOR_VERSION >= 7
#include "MVVM/Views/ViewUtilities.h"
#else
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#endif

#define LOCTEXT_NAMESPACE "DialogueTrackEditor"

// --- FDialogueSectionUI ---

FDialogueSectionUI::FDialogueSectionUI(UMovieSceneSection& InSection)
	: FSequencerSection(InSection)
{
}

int32 FDialogueSectionUI::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	UMovieSceneDialogueSection* DialogueSection = Cast<UMovieSceneDialogueSection>(
		const_cast<FDialogueSectionUI*>(this)->GetSectionObject());
	if (!DialogueSection)
	{
		return Painter.PaintSectionBackground();
	}

	return Painter.PaintSectionBackground(DialogueSection->BarColor);
}

FText FDialogueSectionUI::GetSectionTitle() const
{
	const UMovieSceneDialogueSection* DialogueSection = Cast<UMovieSceneDialogueSection>(
		const_cast<FDialogueSectionUI*>(this)->GetSectionObject());
	if (DialogueSection)
	{
		return DialogueSection->GetDisplayText();
	}
	return LOCTEXT("NoText", "(Empty)");
}

#if ENGINE_MINOR_VERSION >= 7
float FDialogueSectionUI::GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const
#else
float FDialogueSectionUI::GetSectionHeight() const
#endif
{
	return 24.0f;
}

FReply FDialogueSectionUI::OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent)
{
	UMovieSceneDialogueSection* DialogueSection = Cast<UMovieSceneDialogueSection>(GetSectionObject());
	if (!DialogueSection)
	{
		return FReply::Unhandled();
	}

	TWeakObjectPtr<UMovieSceneDialogueSection> WeakDialogueSection = DialogueSection;

	TSharedRef<STextEntryPopup> TextEntryPopup = SNew(STextEntryPopup)
		.Label(LOCTEXT("EditDialogueTextLabel", "Subtitle Text"))
		.DefaultText(DialogueSection->DialogueText)
		.SelectAllTextWhenFocused(true)
		.OnTextCommitted(FOnTextCommitted::CreateLambda(
			[WeakDialogueSection](const FText& InText, ETextCommit::Type CommitType)
			{
				if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
				{
					if (UMovieSceneDialogueSection* Section = WeakDialogueSection.Get())
					{
						const FScopedTransaction Transaction(LOCTEXT("EditDialogueText_Transaction", "Edit Subtitle Text"));
						Section->Modify();
						Section->DialogueText = InText;
					}
				}
				FSlateApplication::Get().DismissAllMenus();
			}));

	FSlateApplication::Get().PushMenu(
		FSlateApplication::Get().GetActiveTopLevelRegularWindow().ToSharedRef(),
		FWidgetPath(),
		TextEntryPopup,
		MouseEvent.GetScreenSpacePosition(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));

	return FReply::Handled();
}

// --- FDialogueTrackEditor ---

TSharedRef<ISequencerTrackEditor> FDialogueTrackEditor::CreateTrackEditor(
	TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShareable(new FDialogueTrackEditor(OwningSequencer));
}

FDialogueTrackEditor::FDialogueTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

FText FDialogueTrackEditor::GetDisplayName() const
{
	return LOCTEXT("TrackDisplayName", "Subtitles Track");
}

bool FDialogueTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass == UMovieSceneDialogueTrack::StaticClass();
}

TSharedRef<ISequencerSection> FDialogueTrackEditor::MakeSectionInterface(
	UMovieSceneSection& SectionObject,
	UMovieSceneTrack& Track,
	FGuid ObjectBinding)
{
	return MakeShared<FDialogueSectionUI>(SectionObject);
}

void FDialogueTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddDialogueTrack", "Subtitles Track"),
		LOCTEXT("AddDialogueTrackTooltip", "Add a subtitles track for subtitle display"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DialogueWave"),
		FUIAction(FExecuteAction::CreateSP(this, &FDialogueTrackEditor::HandleAddDialogueTrack))
	);
}

TSharedPtr<SWidget> FDialogueTrackEditor::BuildOutlinerEditWidget(
	const FGuid& ObjectBinding,
	UMovieSceneTrack* Track,
	const FBuildEditWidgetParams& Params)
{
	UMovieSceneDialogueTrack* DialogueTrack = Cast<UMovieSceneDialogueTrack>(Track);
	if (!DialogueTrack)
	{
		return TSharedPtr<SWidget>();
	}

	TWeakObjectPtr<UMovieSceneTrack> WeakTrack = Track;

#if ENGINE_MINOR_VERSION >= 7
	return UE::Sequencer::MakeAddButton(
		LOCTEXT("AddDialogueSection", "Dialogue"),
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
	auto OnClickedLambda = [this, WeakTrack]() -> FReply
	{
		if (UMovieSceneTrack* TrackPtr = WeakTrack.Get())
		{
			AddNewSectionToTrack(TrackPtr);
		}
		return FReply::Handled();
	};

	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "FlatButton")
		.ContentPadding(FMargin(2, 0))
		.OnClicked_Lambda(OnClickedLambda)
		.ToolTipText(LOCTEXT("AddDialogueSectionTooltip", "Add a new dialogue section"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "NormalText.Important")
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.Text(LOCTEXT("AddDialogueSection", "Dialogue"))
			]
		];
#endif
}

void FDialogueTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	MenuBuilder.BeginSection(TEXT("DialogueSections"), LOCTEXT("DialogueSectionMenu", "Sections"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddDialogueSectionCtx", "Add Dialogue Section"),
			LOCTEXT("AddDialogueSectionCtxTooltip", "Add a new dialogue section at the current playhead position"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FDialogueTrackEditor::AddNewSectionToTrack, Track))
		);
	}
	MenuBuilder.EndSection();
}

void FDialogueTrackEditor::AddNewSectionToTrack(UMovieSceneTrack* Track)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid())
	{
		return;
	}

	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!FocusedMovieScene || FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddDialogueSection_Transaction", "Add Dialogue Section"));
	FocusedMovieScene->Modify();
	Track->Modify();

	UMovieSceneSection* NewSection = Track->CreateNewSection();
	if (!NewSection)
	{
		return;
	}

	const FFrameNumber CurrentTime = SequencerPtr->GetLocalTime().Time.GetFrame();
	const FFrameRate TickResolution = FocusedMovieScene->GetTickResolution();
	const FFrameNumber Duration = (3.0 * TickResolution).FloorToFrame();
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

void FDialogueTrackEditor::HandleAddDialogueTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!FocusedMovieScene || FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddDialogueTrack_Transaction", "Add Dialogue Track"));
	FocusedMovieScene->Modify();

	UMovieSceneDialogueTrack* NewTrack = FocusedMovieScene->AddTrack<UMovieSceneDialogueTrack>();
	if (NewTrack)
	{
		UMovieSceneSection* NewSection = NewTrack->CreateNewSection();
		if (NewSection)
		{
			NewTrack->AddSection(*NewSection);

			if (FocusedMovieScene->GetPlaybackRange().HasLowerBound())
			{
				const FFrameNumber Start = FocusedMovieScene->GetPlaybackRange().GetLowerBoundValue();
				const FFrameRate TickResolution = FocusedMovieScene->GetTickResolution();
				const FFrameNumber Duration = (3.0 * TickResolution).FloorToFrame();
				NewSection->SetRange(TRange<FFrameNumber>(Start, Start + Duration));
			}
		}

		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

#undef LOCTEXT_NAMESPACE
