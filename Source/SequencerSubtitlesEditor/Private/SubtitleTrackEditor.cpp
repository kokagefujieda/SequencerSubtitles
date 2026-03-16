// Copyright 2026 kokage. All Rights Reserved.

#include "SubtitleTrackEditor.h"
#include "SubtitleTrack.h"
#include "SubtitleSection.h"

#include "ISequencer.h"
#include "SequencerSectionPainter.h"
#include "MovieScene.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#if ENGINE_MINOR_VERSION >= 7
#include "MVVM/Views/ViewUtilities.h"
#else
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#endif

#define LOCTEXT_NAMESPACE "SubtitleTrackEditor"

// --- FSubtitleSectionUI ---

FSubtitleSectionUI::FSubtitleSectionUI(UMovieSceneSection& InSection)
	: FSequencerSection(InSection)
{
}

int32 FSubtitleSectionUI::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	UMovieSceneSeqSubtitleSection* SubtitleSection = Cast<UMovieSceneSeqSubtitleSection>(
		const_cast<FSubtitleSectionUI*>(this)->GetSectionObject());
	if (!SubtitleSection)
	{
		return Painter.PaintSectionBackground();
	}

	return Painter.PaintSectionBackground(SubtitleSection->BarColor);
}

FText FSubtitleSectionUI::GetSectionTitle() const
{
	const UMovieSceneSeqSubtitleSection* SubtitleSection = Cast<UMovieSceneSeqSubtitleSection>(
		const_cast<FSubtitleSectionUI*>(this)->GetSectionObject());
	if (SubtitleSection)
	{
		return SubtitleSection->GetDisplayText();
	}
	return LOCTEXT("NoText", "(Empty)");
}

#if ENGINE_MINOR_VERSION >= 7
float FSubtitleSectionUI::GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const
#else
float FSubtitleSectionUI::GetSectionHeight() const
#endif
{
	return 24.0f;
}

FReply FSubtitleSectionUI::OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent)
{
	UMovieSceneSeqSubtitleSection* SubtitleSection = Cast<UMovieSceneSeqSubtitleSection>(GetSectionObject());
	if (!SubtitleSection)
	{
		return FReply::Unhandled();
	}

	TWeakObjectPtr<UMovieSceneSeqSubtitleSection> WeakSubtitleSection = SubtitleSection;

	// Enter: 確定, Shift+Enter: 改行（ModiferKeyForNewLine で制御）
	TSharedRef<SBox> PopupContent =
		SNew(SBox)
		.WidthOverride(320.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EditSubtitleTextLabel", "Subtitle Text"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SMultiLineEditableTextBox)
					.Text(SubtitleSection->SubtitleText)
					.ModiferKeyForNewLine(EModifierKey::Shift)
					.AutoWrapText(false)
					.OnTextCommitted(FOnTextCommitted::CreateLambda(
						[WeakSubtitleSection](const FText& InText, ETextCommit::Type CommitType)
						{
							if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
							{
								if (UMovieSceneSeqSubtitleSection* Section = WeakSubtitleSection.Get())
								{
									const FScopedTransaction Transaction(LOCTEXT("EditSubtitleText_Transaction", "Edit Subtitle Text"));
									Section->Modify();
									Section->SubtitleText = InText;
								}
							}
							FSlateApplication::Get().DismissAllMenus();
						}))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShiftEnterHint", "Enter: Confirm  |  Shift+Enter: New Line"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				]
			]
		];

	FSlateApplication::Get().PushMenu(
		FSlateApplication::Get().GetActiveTopLevelRegularWindow().ToSharedRef(),
		FWidgetPath(),
		PopupContent,
		MouseEvent.GetScreenSpacePosition(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));

	return FReply::Handled();
}

// --- FSubtitleTrackEditor ---

TSharedRef<ISequencerTrackEditor> FSubtitleTrackEditor::CreateTrackEditor(
	TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShareable(new FSubtitleTrackEditor(OwningSequencer));
}

FSubtitleTrackEditor::FSubtitleTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

#if ENGINE_MINOR_VERSION >= 6
FText FSubtitleTrackEditor::GetDisplayName() const
{
	return LOCTEXT("TrackDisplayName", "Subtitles Track");
}
#endif

bool FSubtitleTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass == UMovieSceneSubtitleTrack::StaticClass();
}

TSharedRef<ISequencerSection> FSubtitleTrackEditor::MakeSectionInterface(
	UMovieSceneSection& SectionObject,
	UMovieSceneTrack& Track,
	FGuid ObjectBinding)
{
	return MakeShared<FSubtitleSectionUI>(SectionObject);
}

void FSubtitleTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddSubtitleTrack", "Subtitles Track"),
		LOCTEXT("AddSubtitleTrackTooltip", "Add a subtitles track for subtitle display"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DialogueWave"),
		FUIAction(FExecuteAction::CreateSP(this, &FSubtitleTrackEditor::HandleAddSubtitleTrack))
	);
}

TSharedPtr<SWidget> FSubtitleTrackEditor::BuildOutlinerEditWidget(
	const FGuid& ObjectBinding,
	UMovieSceneTrack* Track,
	const FBuildEditWidgetParams& Params)
{
	UMovieSceneSubtitleTrack* SubtitleTrack = Cast<UMovieSceneSubtitleTrack>(Track);
	if (!SubtitleTrack)
	{
		return TSharedPtr<SWidget>();
	}

	TWeakObjectPtr<UMovieSceneTrack> WeakTrack = Track;

#if ENGINE_MINOR_VERSION >= 7
	return UE::Sequencer::MakeAddButton(
		LOCTEXT("AddSubtitleSection", "Subtitle"),
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
		.ToolTipText(LOCTEXT("AddSubtitleSectionTooltip", "Add a new subtitle section"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "NormalText.Important")
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.Text(LOCTEXT("AddSubtitleSection", "Subtitle"))
			]
		];
#endif
}

void FSubtitleTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	MenuBuilder.BeginSection(TEXT("SubtitleSections"), LOCTEXT("SubtitleSectionMenu", "Sections"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddSubtitleSectionCtx", "Add Subtitle Section"),
			LOCTEXT("AddSubtitleSectionCtxTooltip", "Add a new subtitle section at the current playhead position"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FSubtitleTrackEditor::AddNewSectionToTrack, Track))
		);
	}
	MenuBuilder.EndSection();
}

void FSubtitleTrackEditor::AddNewSectionToTrack(UMovieSceneTrack* Track)
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

	const FScopedTransaction Transaction(LOCTEXT("AddSubtitleSection_Transaction", "Add Subtitle Section"));
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

void FSubtitleTrackEditor::HandleAddSubtitleTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!FocusedMovieScene || FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddSubtitleTrack_Transaction", "Add Subtitle Track"));
	FocusedMovieScene->Modify();

	UMovieSceneSubtitleTrack* NewTrack = FocusedMovieScene->AddTrack<UMovieSceneSubtitleTrack>();
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
