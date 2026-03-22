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
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
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

	// --- Export button ---
	TSharedRef<SWidget> ExportButton =
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "FlatButton")
		.ContentPadding(FMargin(2, 0))
		.OnClicked_Lambda([this, WeakTrack]() -> FReply
		{
			if (UMovieSceneTrack* TrackPtr = WeakTrack.Get())
			{
				ExportSectionsToClipboard(TrackPtr);
			}
			return FReply::Handled();
		})
		.ToolTipText(LOCTEXT("ExportTooltip", "Export all sections to clipboard as JSON (text, timing, appearance)"))
		[
#if ENGINE_MINOR_VERSION >= 7
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.Export"))
			.DesiredSizeOverride(FVector2D(14.0f, 14.0f))
			.ColorAndOpacity(FSlateColor::UseForeground())
#else
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.Download"))
			.DesiredSizeOverride(FVector2D(14.0f, 14.0f))
			.ColorAndOpacity(FSlateColor::UseForeground())
#endif
		];

	// --- Import button ---
	TSharedRef<SWidget> ImportButton =
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "FlatButton")
		.ContentPadding(FMargin(2, 0))
		.OnClicked_Lambda([this, WeakTrack]() -> FReply
		{
			if (UMovieSceneTrack* TrackPtr = WeakTrack.Get())
			{
				ImportSectionsFromClipboard(TrackPtr);
			}
			return FReply::Handled();
		})
		.ToolTipText(LOCTEXT("ImportTooltip", "Import sections from clipboard (JSON, TSV, or plain text)"))
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.Import"))
			.DesiredSizeOverride(FVector2D(14.0f, 14.0f))
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

	MenuBuilder.BeginSection(TEXT("SubtitleAllTracks"), LOCTEXT("SubtitleAllTracksMenu", "All Tracks"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExportAllTracks", "Export All Subtitle Tracks"),
			LOCTEXT("ExportAllTracksTooltip", "Export all subtitle tracks in this sequence to clipboard as JSON"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FSubtitleTrackEditor::ExportAllTracksToClipboard))
		);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ImportAllTracks", "Import All Subtitle Tracks"),
			LOCTEXT("ImportAllTracksTooltip", "Import subtitle tracks from clipboard JSON (creates new tracks)"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FSubtitleTrackEditor::ImportAllTracksFromClipboard))
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

static TSharedRef<FJsonObject> LinearColorToJson(const FLinearColor& Color)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("r"), Color.R);
	Obj->SetNumberField(TEXT("g"), Color.G);
	Obj->SetNumberField(TEXT("b"), Color.B);
	Obj->SetNumberField(TEXT("a"), Color.A);
	return Obj;
}

static FLinearColor JsonToLinearColor(const TSharedPtr<FJsonObject>& Obj, const FLinearColor& Default = FLinearColor::White)
{
	if (!Obj.IsValid()) return Default;
	return FLinearColor(
		Obj->GetNumberField(TEXT("r")),
		Obj->GetNumberField(TEXT("g")),
		Obj->GetNumberField(TEXT("b")),
		Obj->GetNumberField(TEXT("a"))
	);
}

static TSharedRef<FJsonObject> AppearanceToJson(const FSubtitleAppearance& A)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();

	if (!A.FontAsset.IsNull())
	{
		Obj->SetStringField(TEXT("fontAsset"), A.FontAsset.ToString());
	}
	Obj->SetNumberField(TEXT("fontSize"), A.FontSize);
	Obj->SetObjectField(TEXT("textColor"), LinearColorToJson(A.TextColor));
	Obj->SetObjectField(TEXT("backgroundColor"), LinearColorToJson(A.BackgroundColor));

	Obj->SetStringField(TEXT("verticalPosition"), StaticEnum<ESubtitleVerticalPosition>()->GetNameStringByValue(static_cast<int64>(A.VerticalPosition)));
	Obj->SetStringField(TEXT("horizontalPosition"), StaticEnum<ESubtitleHorizontalPosition>()->GetNameStringByValue(static_cast<int64>(A.HorizontalPosition)));
	Obj->SetStringField(TEXT("textAlignment"), StaticEnum<ESubtitleTextAlignment>()->GetNameStringByValue(static_cast<int64>(A.TextAlignment)));

	// ScreenPadding
	{
		TSharedRef<FJsonObject> Pad = MakeShared<FJsonObject>();
		Pad->SetNumberField(TEXT("left"), A.ScreenPadding.Left);
		Pad->SetNumberField(TEXT("top"), A.ScreenPadding.Top);
		Pad->SetNumberField(TEXT("right"), A.ScreenPadding.Right);
		Pad->SetNumberField(TEXT("bottom"), A.ScreenPadding.Bottom);
		Obj->SetObjectField(TEXT("screenPadding"), Pad);
	}

	Obj->SetStringField(TEXT("entranceType"), StaticEnum<ESubtitleEntranceType>()->GetNameStringByValue(static_cast<int64>(A.EntranceType)));
	Obj->SetNumberField(TEXT("entranceDuration"), A.EntranceDuration);

	if (!A.TypewriterSound.IsNull())
	{
		Obj->SetStringField(TEXT("typewriterSound"), A.TypewriterSound.ToString());
	}
	Obj->SetNumberField(TEXT("typewriterSoundInterval"), A.TypewriterSoundInterval);
	Obj->SetNumberField(TEXT("maxLinesPerPage"), A.MaxLinesPerPage);
	Obj->SetNumberField(TEXT("maxCharsPerLine"), A.MaxCharsPerLine);

	Obj->SetObjectField(TEXT("speakerNameColor"), LinearColorToJson(A.SpeakerNameColor));
	Obj->SetNumberField(TEXT("speakerNameFontSize"), A.SpeakerNameFontSize);

	Obj->SetBoolField(TEXT("showSeparatorLine"), A.bShowSeparatorLine);
	Obj->SetBoolField(TEXT("useLineImage"), A.bUseLineImage);
	if (!A.LineImage.IsNull())
	{
		Obj->SetStringField(TEXT("lineImage"), A.LineImage.ToString());
	}
	Obj->SetObjectField(TEXT("separatorLineColor"), LinearColorToJson(A.SeparatorLineColor));
	Obj->SetNumberField(TEXT("separatorLineThickness"), A.SeparatorLineThickness);
	Obj->SetNumberField(TEXT("messageWindowHeight"), A.MessageWindowHeight);

	Obj->SetBoolField(TEXT("overrideExitAnimation"), A.bOverrideExitAnimation);
	Obj->SetStringField(TEXT("exitType"), StaticEnum<ESubtitleEntranceType>()->GetNameStringByValue(static_cast<int64>(A.ExitType)));
	Obj->SetNumberField(TEXT("exitDuration"), A.ExitDuration);

	return Obj;
}

static FSubtitleAppearance JsonToAppearance(const TSharedPtr<FJsonObject>& Obj)
{
	FSubtitleAppearance A;
	if (!Obj.IsValid()) return A;

	FString Str;
	if (Obj->TryGetStringField(TEXT("fontAsset"), Str))
	{
		A.FontAsset = TSoftObjectPtr<UObject>(FSoftObjectPath(Str));
	}
	A.FontSize = Obj->GetIntegerField(TEXT("fontSize"));
	A.TextColor = JsonToLinearColor(Obj->GetObjectField(TEXT("textColor")));
	A.BackgroundColor = JsonToLinearColor(Obj->GetObjectField(TEXT("backgroundColor")), FLinearColor(0, 0, 0, 0));

	if (Obj->TryGetStringField(TEXT("verticalPosition"), Str))
	{
		int64 Val = StaticEnum<ESubtitleVerticalPosition>()->GetValueByNameString(Str);
		if (Val != INDEX_NONE) A.VerticalPosition = static_cast<ESubtitleVerticalPosition>(Val);
	}
	if (Obj->TryGetStringField(TEXT("horizontalPosition"), Str))
	{
		int64 Val = StaticEnum<ESubtitleHorizontalPosition>()->GetValueByNameString(Str);
		if (Val != INDEX_NONE) A.HorizontalPosition = static_cast<ESubtitleHorizontalPosition>(Val);
	}
	if (Obj->TryGetStringField(TEXT("textAlignment"), Str))
	{
		int64 Val = StaticEnum<ESubtitleTextAlignment>()->GetValueByNameString(Str);
		if (Val != INDEX_NONE) A.TextAlignment = static_cast<ESubtitleTextAlignment>(Val);
	}

	if (const TSharedPtr<FJsonObject>* Pad = nullptr; Obj->TryGetObjectField(TEXT("screenPadding"), Pad))
	{
		A.ScreenPadding.Left = (*Pad)->GetNumberField(TEXT("left"));
		A.ScreenPadding.Top = (*Pad)->GetNumberField(TEXT("top"));
		A.ScreenPadding.Right = (*Pad)->GetNumberField(TEXT("right"));
		A.ScreenPadding.Bottom = (*Pad)->GetNumberField(TEXT("bottom"));
	}

	if (Obj->TryGetStringField(TEXT("entranceType"), Str))
	{
		int64 Val = StaticEnum<ESubtitleEntranceType>()->GetValueByNameString(Str);
		if (Val != INDEX_NONE) A.EntranceType = static_cast<ESubtitleEntranceType>(Val);
	}
	A.EntranceDuration = Obj->GetNumberField(TEXT("entranceDuration"));

	if (Obj->TryGetStringField(TEXT("typewriterSound"), Str))
	{
		A.TypewriterSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(Str));
	}
	A.TypewriterSoundInterval = Obj->GetNumberField(TEXT("typewriterSoundInterval"));
	A.MaxLinesPerPage = Obj->GetIntegerField(TEXT("maxLinesPerPage"));
	A.MaxCharsPerLine = Obj->GetIntegerField(TEXT("maxCharsPerLine"));

	A.SpeakerNameColor = JsonToLinearColor(Obj->GetObjectField(TEXT("speakerNameColor")), FLinearColor(1, 0.85f, 0, 1));
	A.SpeakerNameFontSize = Obj->GetIntegerField(TEXT("speakerNameFontSize"));

	A.bShowSeparatorLine = Obj->GetBoolField(TEXT("showSeparatorLine"));
	A.bUseLineImage = Obj->GetBoolField(TEXT("useLineImage"));
	if (Obj->TryGetStringField(TEXT("lineImage"), Str))
	{
		A.LineImage = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Str));
	}
	A.SeparatorLineColor = JsonToLinearColor(Obj->GetObjectField(TEXT("separatorLineColor")), FLinearColor(0.8f, 0.7f, 0.3f, 0.8f));
	A.SeparatorLineThickness = Obj->GetNumberField(TEXT("separatorLineThickness"));
	A.MessageWindowHeight = Obj->GetNumberField(TEXT("messageWindowHeight"));

	A.bOverrideExitAnimation = Obj->GetBoolField(TEXT("overrideExitAnimation"));
	if (Obj->TryGetStringField(TEXT("exitType"), Str))
	{
		int64 Val = StaticEnum<ESubtitleEntranceType>()->GetValueByNameString(Str);
		if (Val != INDEX_NONE) A.ExitType = static_cast<ESubtitleEntranceType>(Val);
	}
	A.ExitDuration = Obj->GetNumberField(TEXT("exitDuration"));

	return A;
}

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

	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetNumberField(TEXT("version"), 1);
	RootObj->SetStringField(TEXT("format"), TEXT("SequencerSubtitles"));

	TArray<TSharedPtr<FJsonValue>> SectionArray;
	for (const UMovieSceneSection* Section : Sections)
	{
		const UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(Section);
		if (!SubSection) continue;

		const double StartSeconds = TickResolution.AsSeconds(Section->GetInclusiveStartFrame());
		const double EndSeconds = TickResolution.AsSeconds(Section->GetExclusiveEndFrame());

		TSharedRef<FJsonObject> SObj = MakeShared<FJsonObject>();
		SObj->SetNumberField(TEXT("start"), StartSeconds);
		SObj->SetNumberField(TEXT("end"), EndSeconds);
		SObj->SetStringField(TEXT("text"), SubSection->SubtitleText.ToString());
		SObj->SetObjectField(TEXT("barColor"), LinearColorToJson(SubSection->BarColor));
		SObj->SetBoolField(TEXT("typewriterEffect"), SubSection->bTypewriterEffect);
		SObj->SetNumberField(TEXT("typewriterCharInterval"), SubSection->TypewriterCharInterval);

		SObj->SetBoolField(TEXT("overrideSpeakerName"), SubSection->bOverrideSpeakerName);
		if (SubSection->bOverrideSpeakerName)
		{
			SObj->SetStringField(TEXT("speakerNameOverride"), SubSection->SpeakerNameOverride.ToString());
		}

		SObj->SetBoolField(TEXT("overrideAppearance"), SubSection->bOverrideAppearance);
		if (SubSection->bOverrideAppearance)
		{
			SObj->SetObjectField(TEXT("appearance"), AppearanceToJson(SubSection->AppearanceOverride));
		}

		SectionArray.Add(MakeShared<FJsonValueObject>(SObj));
	}

	RootObj->SetArrayField(TEXT("sections"), SectionArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObj, Writer);

	FPlatformApplicationMisc::ClipboardCopy(*OutputString);
}

static void ApplyJsonSectionProperties(UMovieSceneSeqSubtitleSection* SubSection, const TSharedPtr<FJsonObject>& SObj)
{
	if (!SubSection || !SObj.IsValid()) return;

	SubSection->SubtitleText = FText::FromString(SObj->GetStringField(TEXT("text")));

	if (SObj->HasField(TEXT("barColor")))
	{
		SubSection->BarColor = JsonToLinearColor(SObj->GetObjectField(TEXT("barColor")));
	}
	if (SObj->HasField(TEXT("typewriterEffect")))
	{
		SubSection->bTypewriterEffect = SObj->GetBoolField(TEXT("typewriterEffect"));
	}
	if (SObj->HasField(TEXT("typewriterCharInterval")))
	{
		SubSection->TypewriterCharInterval = SObj->GetNumberField(TEXT("typewriterCharInterval"));
	}
	if (SObj->HasField(TEXT("overrideSpeakerName")))
	{
		SubSection->bOverrideSpeakerName = SObj->GetBoolField(TEXT("overrideSpeakerName"));
	}
	if (SObj->HasField(TEXT("speakerNameOverride")))
	{
		SubSection->SpeakerNameOverride = FText::FromString(SObj->GetStringField(TEXT("speakerNameOverride")));
	}
	if (SObj->HasField(TEXT("overrideAppearance")))
	{
		SubSection->bOverrideAppearance = SObj->GetBoolField(TEXT("overrideAppearance"));
	}
	if (SObj->HasField(TEXT("appearance")))
	{
		SubSection->AppearanceOverride = JsonToAppearance(SObj->GetObjectField(TEXT("appearance")));
	}
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

	// Try JSON format first
	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ClipboardText);
	if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid()
		&& RootObj->HasField(TEXT("sections")))
	{
		// --- JSON import ---
		const TArray<TSharedPtr<FJsonValue>>& SectionArray = RootObj->GetArrayField(TEXT("sections"));
		if (SectionArray.Num() == 0) return;

		const FScopedTransaction Transaction(LOCTEXT("ImportSubtitleSections_Transaction", "Import Subtitle Sections"));
		MovieScene->Modify();
		Track->Modify();

		// Collect existing sections sorted by start time
		TArray<UMovieSceneSection*> ExistingSections = Track->GetAllSections();
		ExistingSections.Sort([](const UMovieSceneSection& A, const UMovieSceneSection& B)
		{
			return A.GetInclusiveStartFrame() < B.GetInclusiveStartFrame();
		});

		for (int32 i = 0; i < SectionArray.Num(); ++i)
		{
			const TSharedPtr<FJsonObject>& SObj = SectionArray[i]->AsObject();
			if (!SObj.IsValid()) continue;

			const double StartSeconds = SObj->GetNumberField(TEXT("start"));
			const double EndSeconds = SObj->GetNumberField(TEXT("end"));
			const FFrameNumber Start = (StartSeconds * TickResolution).FloorToFrame();
			const FFrameNumber End = (EndSeconds * TickResolution).FloorToFrame();

			if (i < ExistingSections.Num())
			{
				// Overwrite existing section
				if (UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(ExistingSections[i]))
				{
					SubSection->Modify();
					SubSection->SetRange(TRange<FFrameNumber>(Start, End));
					ApplyJsonSectionProperties(SubSection, SObj);
				}
			}
			else
			{
				// Create new section
				UMovieSceneSection* NewSection = Track->CreateNewSection();
				if (!NewSection) continue;

				NewSection->SetRange(TRange<FFrameNumber>(Start, End));

				if (UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(NewSection))
				{
					ApplyJsonSectionProperties(SubSection, SObj);
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
		return;
	}

	// --- Legacy TSV / plain text import ---
	TArray<FString> Lines;
	ClipboardText.ParseIntoArrayLines(Lines);

	// Remove empty lines
	Lines.RemoveAll([](const FString& Line) { return Line.TrimStartAndEnd().IsEmpty(); });
	if (Lines.Num() == 0) return;

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
					if (i > 0)
					{
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
					else
					{
						UMovieSceneSection* LastSection = ExistingSections.Num() > 0 ? ExistingSections.Last() : nullptr;
						if (LastSection && LastSection->HasEndFrame())
						{
							Start = LastSection->GetExclusiveEndFrame();
						}
						else
						{
							Start = FFrameNumber(0);
						}
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

// --- Export / Import All Tracks ---

static TSharedRef<FJsonObject> ExportSingleTrackToJson(const UMovieSceneSubtitleTrack* SubTrack, const FFrameRate& TickResolution)
{
	TSharedRef<FJsonObject> TrackObj = MakeShared<FJsonObject>();
	TrackObj->SetStringField(TEXT("displayName"), SubTrack->GetDisplayName().ToString());
	TrackObj->SetObjectField(TEXT("trackColor"), LinearColorToJson(SubTrack->TrackColor));
	TrackObj->SetBoolField(TEXT("overrideSpeakerName"), SubTrack->bOverrideSpeakerName);
	if (SubTrack->bOverrideSpeakerName)
	{
		TrackObj->SetStringField(TEXT("speakerNameOverride"), SubTrack->SpeakerNameOverride.ToString());
	}
	TrackObj->SetObjectField(TEXT("appearance"), AppearanceToJson(SubTrack->Appearance));

	// Sections
	TArray<UMovieSceneSection*> Sections = SubTrack->GetAllSections();
	Sections.Sort([](const UMovieSceneSection& A, const UMovieSceneSection& B)
	{
		return A.GetInclusiveStartFrame() < B.GetInclusiveStartFrame();
	});

	TArray<TSharedPtr<FJsonValue>> SectionArray;
	for (const UMovieSceneSection* Section : Sections)
	{
		const UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(Section);
		if (!SubSection) continue;

		const double StartSeconds = TickResolution.AsSeconds(Section->GetInclusiveStartFrame());
		const double EndSeconds = TickResolution.AsSeconds(Section->GetExclusiveEndFrame());

		TSharedRef<FJsonObject> SObj = MakeShared<FJsonObject>();
		SObj->SetNumberField(TEXT("start"), StartSeconds);
		SObj->SetNumberField(TEXT("end"), EndSeconds);
		SObj->SetStringField(TEXT("text"), SubSection->SubtitleText.ToString());
		SObj->SetObjectField(TEXT("barColor"), LinearColorToJson(SubSection->BarColor));
		SObj->SetBoolField(TEXT("typewriterEffect"), SubSection->bTypewriterEffect);
		SObj->SetNumberField(TEXT("typewriterCharInterval"), SubSection->TypewriterCharInterval);
		SObj->SetBoolField(TEXT("overrideSpeakerName"), SubSection->bOverrideSpeakerName);
		if (SubSection->bOverrideSpeakerName)
		{
			SObj->SetStringField(TEXT("speakerNameOverride"), SubSection->SpeakerNameOverride.ToString());
		}
		SObj->SetBoolField(TEXT("overrideAppearance"), SubSection->bOverrideAppearance);
		if (SubSection->bOverrideAppearance)
		{
			SObj->SetObjectField(TEXT("appearance"), AppearanceToJson(SubSection->AppearanceOverride));
		}

		SectionArray.Add(MakeShared<FJsonValueObject>(SObj));
	}
	TrackObj->SetArrayField(TEXT("sections"), SectionArray);

	return TrackObj;
}

void FSubtitleTrackEditor::ExportAllTracksToClipboard()
{
	UMovieScene* MovieScene = GetFocusedMovieScene();
	if (!MovieScene) return;

	const FFrameRate TickResolution = MovieScene->GetTickResolution();

	// Collect all subtitle tracks
	TArray<TSharedPtr<FJsonValue>> TracksArray;
	for (UMovieSceneTrack* Track : MovieScene->GetTracks())
	{
		UMovieSceneSubtitleTrack* SubTrack = Cast<UMovieSceneSubtitleTrack>(Track);
		if (!SubTrack) continue;

		TracksArray.Add(MakeShared<FJsonValueObject>(ExportSingleTrackToJson(SubTrack, TickResolution)));
	}

	if (TracksArray.Num() == 0) return;

	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetNumberField(TEXT("version"), 1);
	RootObj->SetStringField(TEXT("format"), TEXT("SequencerSubtitles_AllTracks"));
	RootObj->SetArrayField(TEXT("tracks"), TracksArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObj, Writer);

	FPlatformApplicationMisc::ClipboardCopy(*OutputString);
}

void FSubtitleTrackEditor::ImportAllTracksFromClipboard()
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid()) return;

	UMovieScene* MovieScene = GetFocusedMovieScene();
	if (!MovieScene || MovieScene->IsReadOnly()) return;

	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	if (ClipboardText.IsEmpty()) return;

	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ClipboardText);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid()) return;
	if (!RootObj->HasField(TEXT("tracks"))) return;

	const TArray<TSharedPtr<FJsonValue>>& TracksArray = RootObj->GetArrayField(TEXT("tracks"));
	if (TracksArray.Num() == 0) return;

	const FFrameRate TickResolution = MovieScene->GetTickResolution();

	const FScopedTransaction Transaction(LOCTEXT("ImportAllTracks_Transaction", "Import All Subtitle Tracks"));
	MovieScene->Modify();

	for (const TSharedPtr<FJsonValue>& TrackValue : TracksArray)
	{
		const TSharedPtr<FJsonObject>& TrackObj = TrackValue->AsObject();
		if (!TrackObj.IsValid()) continue;

		// Create a new subtitle track
		UMovieSceneSubtitleTrack* NewTrack = MovieScene->AddTrack<UMovieSceneSubtitleTrack>();
		if (!NewTrack) continue;

		// Set display name
		FString DisplayName;
		if (TrackObj->TryGetStringField(TEXT("displayName"), DisplayName))
		{
			NewTrack->SetDisplayName(FText::FromString(DisplayName));
		}

		// Set track color
		if (TrackObj->HasField(TEXT("trackColor")))
		{
			NewTrack->TrackColor = JsonToLinearColor(TrackObj->GetObjectField(TEXT("trackColor")));
		}

		// Set speaker override
		if (TrackObj->HasField(TEXT("overrideSpeakerName")))
		{
			NewTrack->bOverrideSpeakerName = TrackObj->GetBoolField(TEXT("overrideSpeakerName"));
		}
		FString SpeakerStr;
		if (TrackObj->TryGetStringField(TEXT("speakerNameOverride"), SpeakerStr))
		{
			NewTrack->SpeakerNameOverride = FText::FromString(SpeakerStr);
		}

		// Set track-level appearance
		if (TrackObj->HasField(TEXT("appearance")))
		{
			NewTrack->Appearance = JsonToAppearance(TrackObj->GetObjectField(TEXT("appearance")));
		}

		// Create sections
		if (TrackObj->HasField(TEXT("sections")))
		{
			const TArray<TSharedPtr<FJsonValue>>& SectionArray = TrackObj->GetArrayField(TEXT("sections"));
			for (const TSharedPtr<FJsonValue>& SectionValue : SectionArray)
			{
				const TSharedPtr<FJsonObject>& SObj = SectionValue->AsObject();
				if (!SObj.IsValid()) continue;

				UMovieSceneSection* NewSection = NewTrack->CreateNewSection();
				if (!NewSection) continue;

				const double StartSeconds = SObj->GetNumberField(TEXT("start"));
				const double EndSeconds = SObj->GetNumberField(TEXT("end"));
				const FFrameNumber Start = (StartSeconds * TickResolution).FloorToFrame();
				const FFrameNumber End = (EndSeconds * TickResolution).FloorToFrame();
				NewSection->SetRange(TRange<FFrameNumber>(Start, End));

				if (UMovieSceneSeqSubtitleSection* SubSection = Cast<UMovieSceneSeqSubtitleSection>(NewSection))
				{
					ApplyJsonSectionProperties(SubSection, SObj);
				}

				int32 OverlapPriority = 0;
				for (UMovieSceneSection* S : NewTrack->GetAllSections())
				{
					OverlapPriority = FMath::Max(S->GetOverlapPriority() + 1, OverlapPriority);
				}
				NewSection->SetOverlapPriority(OverlapPriority);

				NewTrack->AddSection(*NewSection);
			}
		}
	}

	SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

#undef LOCTEXT_NAMESPACE
