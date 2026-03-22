// Copyright 2026 kokage. All Rights Reserved.

#include "SubtitleTrackEditor.h"
#include "SubtitleTrack.h"
#include "SubtitleSection.h"
#include "SubtitleSettings.h"

#include "ISequencer.h"
#include "SequencerSectionPainter.h"
#include "MovieScene.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "HAL/PlatformApplicationMisc.h"
#if ENGINE_MINOR_VERSION >= 7
#include "MVVM/Views/ViewUtilities.h"
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

FText FSubtitleSectionUI::GetSectionToolTip() const
{
	const UMovieSceneSeqSubtitleSection* SubtitleSection = Cast<UMovieSceneSeqSubtitleSection>(
		const_cast<FSubtitleSectionUI*>(this)->GetSectionObject());
	if (SubtitleSection && !SubtitleSection->SubtitleText.IsEmptyOrWhitespace())
	{
		return SubtitleSection->SubtitleText;
	}
	return LOCTEXT("NoTextTip", "(Empty)");
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

	TSharedPtr<SMultiLineEditableTextBox> EditableTextBox;

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
					SAssignNew(EditableTextBox, SMultiLineEditableTextBox)
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

	if (EditableTextBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(EditableTextBox, EFocusCause::SetDirectly);
	}

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

	// --- "+" Add Section button ---
	TSharedRef<SWidget> AddButton =
#if ENGINE_MINOR_VERSION >= 7
		UE::Sequencer::MakeAddButton(
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
		SNew(SButton)
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
		.ToolTipText(LOCTEXT("AddSubtitleSectionTooltip", "Add a new subtitle section"))
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "NormalText.Important")
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.Text(LOCTEXT("AddSubtitleSection", "Subtitle"))
		];
#endif

	const FVector2D IconSize(14.0f, 14.0f);

	// --- Export button ---
	TSharedRef<SWidget> ExportButton =
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "FlatButton")
		.ContentPadding(FMargin(0))
		.OnClicked_Lambda([this, WeakTrack]() -> FReply
		{
			if (UMovieSceneTrack* TrackPtr = WeakTrack.Get())
			{
				ExportSectionsToClipboard(TrackPtr);
			}
			return FReply::Handled();
		})
		.ToolTipText(LOCTEXT("ExportTooltip", "Export all subtitle texts to clipboard (TSV with timing)"))
		[
			SNew(SImage)
#if ENGINE_MINOR_VERSION >= 7
			.Image(FAppStyle::Get().GetBrush("Icons.Export"))
#else
			.Image(FAppStyle::Get().GetBrush("Icons.ArrowUp"))
#endif
			.DesiredSizeOverride(IconSize)
			.ColorAndOpacity(FSlateColor::UseForeground())
		];

	// --- Import button ---
	TSharedRef<SWidget> ImportButton =
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "FlatButton")
		.ContentPadding(FMargin(0))
		.OnClicked_Lambda([this, WeakTrack]() -> FReply
		{
			if (UMovieSceneTrack* TrackPtr = WeakTrack.Get())
			{
				ImportSectionsFromClipboard(TrackPtr);
			}
			return FReply::Handled();
		})
		.ToolTipText(LOCTEXT("ImportTooltip", "Import subtitle texts from clipboard (TSV or plain text)"))
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.Import"))
			.DesiredSizeOverride(IconSize)
			.ColorAndOpacity(FSlateColor::UseForeground())
		];

	// --- Color preset dropdown ---
	TSharedRef<SWidget> ColorButton =
		SNew(SComboButton)
		.ButtonStyle(FAppStyle::Get(), "FlatButton")
		.ContentPadding(FMargin(0))
		.HasDownArrow(false)
		.OnGetMenuContent_Lambda([this, WeakTrack]() -> TSharedRef<SWidget>
		{
			FMenuBuilder MenuBuilder(true, nullptr);
			if (UMovieSceneTrack* TrackPtr = WeakTrack.Get())
			{
				BuildColorPresetMenu(MenuBuilder, TrackPtr);
			}
			return MenuBuilder.MakeWidget();
		})
		.ToolTipText(LOCTEXT("ColorPresetTooltip", "Change track color (applies to new sections)"))
		.ButtonContent()
		[
			SNew(SColorBlock)
			.Color_Lambda([WeakTrack]() -> FLinearColor
			{
				if (UMovieSceneSubtitleTrack* SubTrack = Cast<UMovieSceneSubtitleTrack>(WeakTrack.Get()))
				{
					return SubTrack->TrackColor;
				}
				return FLinearColor::White;
			})
			.Size(FVector2D(14.0f, 14.0f))
		];

	// --- Compose all buttons ---
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			AddButton
		]
		// Spacer to push utility buttons toward center
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			ExportButton
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2, 0, 0, 0)
		[
			ImportButton
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4, 0, 0, 0)
		[
			ColorButton
		];
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

	const USubtitleSettings* Settings = GetDefault<USubtitleSettings>();
	const float DefaultDuration = Settings ? Settings->DefaultSectionDuration : 3.0f;
	const float MaxDuration = Settings ? Settings->MaxDefaultSectionDuration : 5.0f;

	// Find the nearest section that starts after CurrentTime on this track
	FFrameNumber NearestNextStart = TNumericLimits<int32>::Max();
	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		if (Section && Section->HasStartFrame())
		{
			const FFrameNumber SectionStart = Section->GetInclusiveStartFrame();
			if (SectionStart > CurrentTime && SectionStart < NearestNextStart)
			{
				NearestNextStart = SectionStart;
			}
		}
	}

	FFrameNumber Duration;
	if (NearestNextStart < FFrameNumber(TNumericLimits<int32>::Max()))
	{
		const FFrameNumber GapDuration = NearestNextStart - CurrentTime;
		const FFrameNumber MaxFrames = (static_cast<double>(MaxDuration) * TickResolution).FloorToFrame();
		Duration = FMath::Min(GapDuration, MaxFrames);
	}
	else
	{
		Duration = (static_cast<double>(DefaultDuration) * TickResolution).FloorToFrame();
	}

	NewSection->SetRange(TRange<FFrameNumber>(CurrentTime, CurrentTime + Duration));

	// Inherit TrackColor as the section's BarColor
	if (UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(NewSection))
	{
		if (const UMovieSceneSubtitleTrack* SubTrack = Cast<UMovieSceneSubtitleTrack>(Track))
		{
			SubSection->BarColor = SubTrack->TrackColor;
		}
	}

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

// --- Export / Import ---

void FSubtitleTrackEditor::ExportSectionsToClipboard(UMovieSceneTrack* Track)
{
	if (!Track) return;

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid()) return;

	UMovieScene* MovieScene = GetFocusedMovieScene();
	if (!MovieScene) return;

	const FFrameRate TickResolution = MovieScene->GetTickResolution();

	// Collect and sort sections by start time
	TArray<UMovieSceneSection*> Sections = Track->GetAllSections();
	Sections.Sort([](const UMovieSceneSection& A, const UMovieSceneSection& B)
	{
		return A.GetInclusiveStartFrame() < B.GetInclusiveStartFrame();
	});

	FString Result;
	for (const UMovieSceneSection* Section : Sections)
	{
		const UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(Section);
		if (!SubSection) continue;

		const double StartSeconds = TickResolution.AsSeconds(Section->GetInclusiveStartFrame());
		const double EndSeconds = TickResolution.AsSeconds(Section->GetExclusiveEndFrame());

		// Escape newlines in subtitle text as literal \n
		FString Text = SubSection->SubtitleText.ToString();
		Text.ReplaceInline(TEXT("\n"), TEXT("\\n"));

		Result += FString::Printf(TEXT("%.1f\t%.1f\t%s\n"), StartSeconds, EndSeconds, *Text);
	}

	FPlatformApplicationMisc::ClipboardCopy(*Result);
}

void FSubtitleTrackEditor::ImportSectionsFromClipboard(UMovieSceneTrack* Track)
{
	if (!Track) return;

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid()) return;

	UMovieScene* MovieScene = GetFocusedMovieScene();
	if (!MovieScene || MovieScene->IsReadOnly()) return;

	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	if (ClipboardText.IsEmpty()) return;

	const FFrameRate TickResolution = MovieScene->GetTickResolution();

	// Parse lines
	TArray<FString> Lines;
	ClipboardText.ParseIntoArrayLines(Lines);

	// Remove empty lines
	Lines.RemoveAll([](const FString& Line) { return Line.TrimStartAndEnd().IsEmpty(); });
	if (Lines.Num() == 0) return;

	// Detect format: TSV (start\tend\ttext) or plain text
	struct FImportEntry
	{
		double StartSeconds = -1.0;
		double EndSeconds = -1.0;
		FString Text;
	};
	TArray<FImportEntry> Entries;

	for (const FString& Line : Lines)
	{
		FImportEntry Entry;

		TArray<FString> Tabs;
		Line.ParseIntoArray(Tabs, TEXT("\t"));

		if (Tabs.Num() >= 3)
		{
			// TSV format: start \t end \t text
			Entry.StartSeconds = FCString::Atod(*Tabs[0]);
			Entry.EndSeconds = FCString::Atod(*Tabs[1]);

			// Rejoin remaining tabs (text may contain tabs)
			Entry.Text = Tabs[2];
			for (int32 i = 3; i < Tabs.Num(); ++i)
			{
				Entry.Text += TEXT("\t") + Tabs[i];
			}
		}
		else
		{
			// Plain text format
			Entry.Text = Line;
		}

		// Unescape \n to real newlines
		Entry.Text.ReplaceInline(TEXT("\\n"), TEXT("\n"));
		Entries.Add(MoveTemp(Entry));
	}

	const FScopedTransaction Transaction(LOCTEXT("ImportSubtitleSections_Transaction", "Import Subtitle Sections"));
	MovieScene->Modify();
	Track->Modify();

	// Collect existing sections sorted by start time
	TArray<UMovieSceneSection*> ExistingSections = Track->GetAllSections();
	ExistingSections.Sort([](const UMovieSceneSection& A, const UMovieSceneSection& B)
	{
		return A.GetInclusiveStartFrame() < B.GetInclusiveStartFrame();
	});

	const USubtitleSettings* Settings = GetDefault<USubtitleSettings>();
	const float DefaultDuration = Settings ? Settings->DefaultSectionDuration : 3.0f;

	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FImportEntry& Entry = Entries[i];

		if (i < ExistingSections.Num())
		{
			// Overwrite existing section text
			if (UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(ExistingSections[i]))
			{
				SubSection->Modify();
				SubSection->SubtitleText = FText::FromString(Entry.Text);

				// If TSV, also update timing
				if (Entry.StartSeconds >= 0.0)
				{
					const FFrameNumber Start = (Entry.StartSeconds * TickResolution).FloorToFrame();
					const FFrameNumber End = (Entry.EndSeconds * TickResolution).FloorToFrame();
					SubSection->SetRange(TRange<FFrameNumber>(Start, End));
				}
			}
		}
		else
		{
			// Create new section
			UMovieSceneSection* NewSection = Track->CreateNewSection();
			if (!NewSection) continue;

			if (Entry.StartSeconds >= 0.0)
			{
				// TSV: use specified timing
				const FFrameNumber Start = (Entry.StartSeconds * TickResolution).FloorToFrame();
				const FFrameNumber End = (Entry.EndSeconds * TickResolution).FloorToFrame();
				NewSection->SetRange(TRange<FFrameNumber>(Start, End));
			}
			else
			{
				// Plain text: place sequentially after the last section
				FFrameNumber Start;
				if (ExistingSections.Num() > 0 || i > 0)
				{
					// Find the last section end
					UMovieSceneSection* LastSection = (i > 0 && i - 1 < ExistingSections.Num())
						? ExistingSections.Last()
						: (ExistingSections.Num() > 0 ? ExistingSections.Last() : nullptr);

					if (i > 0)
					{
						// We just added sections above this index, find from track
						TArray<UMovieSceneSection*> AllNow = Track->GetAllSections();
						if (AllNow.Num() > 0)
						{
							FFrameNumber MaxEnd = TNumericLimits<int32>::Min();
							for (const UMovieSceneSection* S : AllNow)
							{
								if (S->HasEndFrame())
								{
									MaxEnd = FMath::Max(MaxEnd, S->GetExclusiveEndFrame());
								}
							}
							Start = MaxEnd;
						}
					}
					else if (LastSection && LastSection->HasEndFrame())
					{
						Start = LastSection->GetExclusiveEndFrame();
					}
					else
					{
						Start = FFrameNumber(0);
					}
				}
				else
				{
					Start = MovieScene->GetPlaybackRange().HasLowerBound()
						? MovieScene->GetPlaybackRange().GetLowerBoundValue()
						: FFrameNumber(0);
				}

				const FFrameNumber Duration = (static_cast<double>(DefaultDuration) * TickResolution).FloorToFrame();
				NewSection->SetRange(TRange<FFrameNumber>(Start, Start + Duration));
			}

			if (UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(NewSection))
			{
				SubSection->SubtitleText = FText::FromString(Entry.Text);

				if (const UMovieSceneSubtitleTrack* SubTrack = Cast<UMovieSceneSubtitleTrack>(Track))
				{
					SubSection->BarColor = SubTrack->TrackColor;
				}
			}

			int32 OverlapPriority = 0;
			for (UMovieSceneSection* S : Track->GetAllSections())
			{
				OverlapPriority = FMath::Max(S->GetOverlapPriority() + 1, OverlapPriority);
			}
			NewSection->SetOverlapPriority(OverlapPriority);

			Track->AddSection(*NewSection);
		}
	}

	SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

// --- Typewriter Toggle ---

void FSubtitleTrackEditor::ToggleTypewriterOnAllSections(UMovieSceneTrack* Track)
{
	if (!Track) return;

	const bool bAnyEnabled = HasAnyTypewriterEnabled(Track);
	const bool bNewValue = !bAnyEnabled;

	const FScopedTransaction Transaction(LOCTEXT("ToggleTypewriter_Transaction", "Toggle Typewriter Effect"));
	Track->Modify();

	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		if (UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(Section))
		{
			SubSection->Modify();
			SubSection->bTypewriterEffect = bNewValue;
		}
	}

	if (TSharedPtr<ISequencer> SequencerPtr = GetSequencer())
	{
		SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
}

bool FSubtitleTrackEditor::HasAnyTypewriterEnabled(UMovieSceneTrack* Track) const
{
	if (!Track) return false;

	for (const UMovieSceneSection* Section : Track->GetAllSections())
	{
		if (const UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(Section))
		{
			if (SubSection->bTypewriterEffect) return true;
		}
	}
	return false;
}

// --- Color Presets ---

void FSubtitleTrackEditor::BuildColorPresetMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	struct FColorPreset
	{
		FText Name;
		FLinearColor Color;
	};

	static const FColorPreset Presets[] =
	{
		{ LOCTEXT("ColorBlue",    "Blue"),    FLinearColor(0.2f, 0.6f, 0.9f, 1.0f) },
		{ LOCTEXT("ColorOrange",  "Orange"),  FLinearColor(0.9f, 0.5f, 0.1f, 1.0f) },
		{ LOCTEXT("ColorPink",    "Pink"),    FLinearColor(0.9f, 0.3f, 0.6f, 1.0f) },
		{ LOCTEXT("ColorGreen",   "Green"),   FLinearColor(0.3f, 0.8f, 0.4f, 1.0f) },
		{ LOCTEXT("ColorYellow",  "Yellow"),  FLinearColor(0.9f, 0.8f, 0.2f, 1.0f) },
		{ LOCTEXT("ColorPurple",  "Purple"),  FLinearColor(0.6f, 0.3f, 0.9f, 1.0f) },
		{ LOCTEXT("ColorRed",     "Red"),     FLinearColor(0.9f, 0.2f, 0.2f, 1.0f) },
		{ LOCTEXT("ColorCyan",    "Cyan"),    FLinearColor(0.2f, 0.8f, 0.8f, 1.0f) },
		{ LOCTEXT("ColorWhite",   "White"),   FLinearColor(0.9f, 0.9f, 0.9f, 1.0f) },
	};

	TWeakObjectPtr<UMovieSceneTrack> WeakTrack = Track;

	MenuBuilder.BeginSection(TEXT("ColorPresets"), LOCTEXT("ColorPresetsHeader", "Track Color"));
	for (const FColorPreset& Preset : Presets)
	{
		MenuBuilder.AddMenuEntry(
			Preset.Name,
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, WeakTrack, Color = Preset.Color]()
			{
				if (UMovieSceneSubtitleTrack* SubTrack = Cast<UMovieSceneSubtitleTrack>(WeakTrack.Get()))
				{
					const FScopedTransaction Transaction(LOCTEXT("ChangeTrackColor_Transaction", "Change Track Color"));
					SubTrack->Modify();
					SubTrack->TrackColor = Color;

					if (TSharedPtr<ISequencer> SequencerPtr = GetSequencer())
					{
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
					}
				}
			}))
		);
	}
	MenuBuilder.EndSection();
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
			// Inherit TrackColor as the section's BarColor
			if (UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(NewSection))
			{
				SubSection->BarColor = NewTrack->TrackColor;
			}

			NewTrack->AddSection(*NewSection);

			if (FocusedMovieScene->GetPlaybackRange().HasLowerBound())
			{
				const FFrameNumber Start = FocusedMovieScene->GetPlaybackRange().GetLowerBoundValue();
				const FFrameRate TickResolution = FocusedMovieScene->GetTickResolution();
				const USubtitleSettings* TrackSettings = GetDefault<USubtitleSettings>();
				const float TrackDefaultDuration = TrackSettings ? TrackSettings->DefaultSectionDuration : 3.0f;
				const FFrameNumber Duration = (static_cast<double>(TrackDefaultDuration) * TickResolution).FloorToFrame();
				NewSection->SetRange(TRange<FFrameNumber>(Start, Start + Duration));
			}
		}

		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

#undef LOCTEXT_NAMESPACE
