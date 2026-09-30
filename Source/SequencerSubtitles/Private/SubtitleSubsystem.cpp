// Copyright 2026 kokage. All Rights Reserved.

#include "SubtitleSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/Layout/SBorder.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Styling/CoreStyle.h"
#include "Engine/UserInterfaceSettings.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "SSubtitleSeparatorLine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Modules/ModuleManager.h"
#include "Slate/SceneViewport.h"
#include "SSubtitleDragHandle.h"
#include "SubtitleSection.h"
#include "SubtitleTrack.h"
#endif

// ---------------------------------------------------------------------------
// EnsureSlateWidgets
// ---------------------------------------------------------------------------

void USubtitleSubsystem::EnsureSlateWidgets()
{
	if (WidgetOverlay.IsValid())
	{
		return;
	}

	// ContentVerticalBox stacks all active subtitle entries
	ContentVerticalBox = SNew(SVerticalBox);

	WidgetOverlay = SNew(SOverlay)
		+ SOverlay::Slot()
		.Expose(OverlaySlot)
		.VAlign(VAlign_Bottom)
		.Padding(40.f, 20.f)
		[
			ContentVerticalBox.ToSharedRef()
		];

	WidgetOverlay->SetVisibility(EVisibility::Hidden);

	DPIScalerWidget = SNew(SDPIScaler)
		.DPIScale_UObject(this, &USubtitleSubsystem::GetSubtitleDPIScale)
		[
			WidgetOverlay.ToSharedRef()
		];
}

// ---------------------------------------------------------------------------
// CreateSlotWidget — builds one subtitle entry and appends it to ContentVerticalBox
// ---------------------------------------------------------------------------

void USubtitleSubsystem::CreateSlotWidget(uint32 SlotID, FSubtitleSlot& Slot)
{
	// Speaker name — outline layers (back-to-front: outer blur → outer outline → inner blur → inner outline → text)
	for (int32 i = FSubtitleSlot::NumBlurSteps - 1; i >= 0; --i)
	{
		Slot.SpeakerOuterBlurTextBlocks[i] = SNew(STextBlock).Justification(ETextJustify::Center).Visibility(EVisibility::Collapsed);
	}
	Slot.SpeakerOuterOutlineTextBlock = SNew(STextBlock).Justification(ETextJustify::Center).Visibility(EVisibility::Collapsed);
	for (int32 i = FSubtitleSlot::NumBlurSteps - 1; i >= 0; --i)
	{
		Slot.SpeakerInnerBlurTextBlocks[i] = SNew(STextBlock).Justification(ETextJustify::Center).Visibility(EVisibility::Collapsed);
	}
	Slot.SpeakerInnerOutlineTextBlock = SNew(STextBlock).Justification(ETextJustify::Center).Visibility(EVisibility::Collapsed);

	Slot.SpeakerTextBlock = SNew(STextBlock)
		.Justification(ETextJustify::Center)
		.Visibility(EVisibility::Collapsed);

	// Build SOverlay: outer blur[N-1..0] → outer outline → inner blur[N-1..0] → inner outline → text
	Slot.SpeakerTextOverlay = SNew(SOverlay);
	for (int32 i = FSubtitleSlot::NumBlurSteps - 1; i >= 0; --i)
		Slot.SpeakerTextOverlay->AddSlot()[ Slot.SpeakerOuterBlurTextBlocks[i].ToSharedRef() ];
	Slot.SpeakerTextOverlay->AddSlot()[ Slot.SpeakerOuterOutlineTextBlock.ToSharedRef() ];
	for (int32 i = FSubtitleSlot::NumBlurSteps - 1; i >= 0; --i)
		Slot.SpeakerTextOverlay->AddSlot()[ Slot.SpeakerInnerBlurTextBlocks[i].ToSharedRef() ];
	Slot.SpeakerTextOverlay->AddSlot()[ Slot.SpeakerInnerOutlineTextBlock.ToSharedRef() ];
	Slot.SpeakerTextOverlay->AddSlot()[ Slot.SpeakerTextBlock.ToSharedRef() ];

	// Separator
	Slot.SeparatorLineWidget = SNew(SSubtitleSeparatorLine);

	Slot.SeparatorLineBorder = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::White)
		.Padding(0)
		.Visibility(EVisibility::Collapsed);

	Slot.SeparatorOverlay = SNew(SOverlay)
		+ SOverlay::Slot()[ Slot.SeparatorLineWidget.ToSharedRef() ]
		+ SOverlay::Slot()[ Slot.SeparatorLineBorder.ToSharedRef() ];

	Slot.SeparatorBox = SNew(SBox)
		.HeightOverride(1.0f)
		.Visibility(EVisibility::Collapsed)
		[
			Slot.SeparatorOverlay.ToSharedRef()
		];

	// Subtitle text — outline layers (back-to-front)
	for (int32 i = FSubtitleSlot::NumBlurSteps - 1; i >= 0; --i)
	{
		Slot.OuterBlurTextBlocks[i] = SNew(STextBlock).AutoWrapText(true).Justification(ETextJustify::Center).Visibility(EVisibility::Collapsed);
	}
	Slot.OuterOutlineTextBlock = SNew(STextBlock).AutoWrapText(true).Justification(ETextJustify::Center).Visibility(EVisibility::Collapsed);
	for (int32 i = FSubtitleSlot::NumBlurSteps - 1; i >= 0; --i)
	{
		Slot.InnerBlurTextBlocks[i] = SNew(STextBlock).AutoWrapText(true).Justification(ETextJustify::Center).Visibility(EVisibility::Collapsed);
	}
	Slot.InnerOutlineTextBlock = SNew(STextBlock).AutoWrapText(true).Justification(ETextJustify::Center).Visibility(EVisibility::Collapsed);

	Slot.SubtitleTextBlock = SNew(STextBlock)
		.AutoWrapText(true)
		.Justification(ETextJustify::Center);

	// Build SOverlay (typewriter sizer at the back; it only reserves space)
	Slot.TypewriterSizerOverlay = SNew(SOverlay).Visibility(EVisibility::HitTestInvisible);
	Slot.SubtitleTextOverlay = SNew(SOverlay);
	Slot.SubtitleTextOverlay->AddSlot()[ Slot.TypewriterSizerOverlay.ToSharedRef() ];
	for (int32 i = FSubtitleSlot::NumBlurSteps - 1; i >= 0; --i)
		Slot.SubtitleTextOverlay->AddSlot()[ Slot.OuterBlurTextBlocks[i].ToSharedRef() ];
	Slot.SubtitleTextOverlay->AddSlot()[ Slot.OuterOutlineTextBlock.ToSharedRef() ];
	for (int32 i = FSubtitleSlot::NumBlurSteps - 1; i >= 0; --i)
		Slot.SubtitleTextOverlay->AddSlot()[ Slot.InnerBlurTextBlocks[i].ToSharedRef() ];
	Slot.SubtitleTextOverlay->AddSlot()[ Slot.InnerOutlineTextBlock.ToSharedRef() ];
	Slot.SubtitleTextOverlay->AddSlot()[ Slot.SubtitleTextBlock.ToSharedRef() ];

	Slot.TypewriterSizerBox = SNew(SBox)
		[
			Slot.SubtitleTextOverlay.ToSharedRef()
		];

	Slot.SubtitleBorder = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.Padding(FMargin(12.f, 6.f))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			Slot.TypewriterSizerBox.ToSharedRef()
		];

	Slot.MessageWindowBox = SNew(SBox)
		[
			Slot.SubtitleBorder.ToSharedRef()
		];

	// Vertical layout: Speaker → Separator → Subtitle
	Slot.EntryVBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(0, 0, 0, 2)
		.Expose(Slot.SpeakerNameSlot)
		[
			Slot.SpeakerTextOverlay.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(20, 2, 20, 4)
		.Expose(Slot.SeparatorSlot)
		[
			Slot.SeparatorBox.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			Slot.MessageWindowBox.ToSharedRef()
		];

#if WITH_EDITOR
	// Wrap in a per-slot drag handle so each subtitle can be positioned independently
	Slot.DragHandle = SNew(SSubtitleDragHandle)
		[
			Slot.EntryVBox.ToSharedRef()
		];
	Slot.DragHandle->SetOnDragFinished(FOnSubtitleDragFinished::CreateWeakLambda(this,
		[this, SlotID](FVector2D NewOffset)
		{
			OnSlotDragOffsetChanged(SlotID, NewOffset);
		}
	));
	Slot.DragHandle->SetViewportWidget(WidgetOverlay);

	ContentVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			Slot.DragHandle.ToSharedRef()
		];
#else
	ContentVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			Slot.EntryVBox.ToSharedRef()
		];
#endif
}

// ---------------------------------------------------------------------------
// RemoveSlotWidget — removes one entry from ContentVerticalBox
// ---------------------------------------------------------------------------

void USubtitleSubsystem::RemoveSlotWidget(uint32 SlotID)
{
	TSharedPtr<FSubtitleSlot>* Found = ActiveSlots.Find(SlotID);
	if (!Found || !Found->IsValid()) { return; }

	FSubtitleSlot& Slot = **Found;

	// Stop any running timers
	if (Slot.AnimTimerHandle.IsValid() && Slot.SubtitleBorder.IsValid())
	{
		Slot.SubtitleBorder->UnRegisterActiveTimer(Slot.AnimTimerHandle.ToSharedRef());
		Slot.AnimTimerHandle.Reset();
	}
	if (Slot.AutoHideTimerHandle.IsValid() && Slot.SubtitleBorder.IsValid())
	{
		Slot.SubtitleBorder->UnRegisterActiveTimer(Slot.AutoHideTimerHandle.ToSharedRef());
		Slot.AutoHideTimerHandle.Reset();
	}
	StopTremble(Slot);

	// Remove widget from the stack
	if (ContentVerticalBox.IsValid())
	{
#if WITH_EDITOR
		if (Slot.DragHandle.IsValid())
			ContentVerticalBox->RemoveSlot(Slot.DragHandle.ToSharedRef());
		else if (Slot.EntryVBox.IsValid())
			ContentVerticalBox->RemoveSlot(Slot.EntryVBox.ToSharedRef());
#else
		if (Slot.EntryVBox.IsValid())
			ContentVerticalBox->RemoveSlot(Slot.EntryVBox.ToSharedRef());
#endif
	}

	ActiveSlots.Remove(SlotID);
	SlotSoundCache.Remove(SlotID);

	// Hide overlay when no slots remain
	if (ActiveSlots.IsEmpty() && WidgetOverlay.IsValid())
	{
		WidgetOverlay->SetVisibility(EVisibility::Hidden);
	}

	// Update BP-compat state
	if (ActiveSlots.IsEmpty())
	{
		bIsSubtitleActive = false;
		CurrentSubtitleText = FText::GetEmpty();
	}
}

// ---------------------------------------------------------------------------
// AddToViewport / RemoveFromViewport
// ---------------------------------------------------------------------------

void USubtitleSubsystem::AddToViewport()
{
	if (bAddedToViewport)
	{
		return;
	}

	UWorld* World = GetWorld();
	bIsEditorViewport = false;
	HostGameViewport.Reset();

#if WITH_EDITOR
	HostEditorViewport.Reset();
	if (World && (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview))
	{
		if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
			if (LevelEditor.IsValid())
			{
				TSharedPtr<IAssetViewport> ActiveLevelViewport = LevelEditor->GetActiveViewportInterface();
				if (ActiveLevelViewport.IsValid() && DPIScalerWidget.IsValid())
				{
					ActiveLevelViewport->AddOverlayWidget(DPIScalerWidget.ToSharedRef());
					HostEditorViewport = ActiveLevelViewport;
					bAddedToViewport = true;
					bIsEditorViewport = true;
					return;
				}
			}
		}
	}
#endif

	UGameViewportClient* GameViewport = World ? World->GetGameViewport() : nullptr;
	if (!GameViewport && GEngine)
	{
		GameViewport = GEngine->GameViewport;
	}
	if (GameViewport && DPIScalerWidget.IsValid())
	{
		GameViewport->AddViewportWidgetContent(DPIScalerWidget.ToSharedRef(), 100);
		HostGameViewport = GameViewport;
		bAddedToViewport = true;
	}
}

void USubtitleSubsystem::RemoveFromViewport()
{
	if (!bAddedToViewport || !WidgetOverlay.IsValid())
	{
		return;
	}

#if WITH_EDITOR
	if (bIsEditorViewport)
	{
		// Remove from the viewport it was added to, not the currently active one
		// (module may already be unloaded during shutdown)
		if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			TSharedPtr<IAssetViewport> EditorViewport = HostEditorViewport.Pin();
			if (EditorViewport.IsValid() && DPIScalerWidget.IsValid())
			{
				EditorViewport->RemoveOverlayWidget(DPIScalerWidget.ToSharedRef());
			}
		}
		// Always reset whether or not removal succeeded (viewport may already be gone)
		HostEditorViewport.Reset();
		bAddedToViewport = false;
		return;
	}
#endif

	UGameViewportClient* GameViewport = HostGameViewport.Get();
	if (GameViewport && DPIScalerWidget.IsValid())
	{
		GameViewport->RemoveViewportWidgetContent(DPIScalerWidget.ToSharedRef());
	}
	HostGameViewport.Reset();
	bAddedToViewport = false;
}

// ---------------------------------------------------------------------------
// WrapTextByCharLimit (unchanged)
// ---------------------------------------------------------------------------

FString USubtitleSubsystem::WrapTextByCharLimit(const FString& InText, int32 MaxCharsPerLine)
{
	if (MaxCharsPerLine <= 0)
	{
		return InText;
	}

	FString Result;
	TArray<FString> Lines;
	InText.ParseIntoArray(Lines, TEXT("\n"), /*bCullEmpty=*/false);

	for (int32 LineIdx = 0; LineIdx < Lines.Num(); ++LineIdx)
	{
		if (LineIdx > 0)
		{
			Result += TEXT("\n");
		}

		const FString& Line = Lines[LineIdx];
		if (Line.Len() <= MaxCharsPerLine)
		{
			Result += Line;
			continue;
		}

		int32 Pos = 0;
		while (Pos < Line.Len())
		{
			if (Pos > 0) { Result += TEXT("\n"); }

			const int32 Remaining = Line.Len() - Pos;
			if (Remaining <= MaxCharsPerLine)
			{
				Result += Line.Mid(Pos);
				break;
			}

			Result += Line.Mid(Pos, MaxCharsPerLine);
			Pos += MaxCharsPerLine;
		}
	}

	return Result;
}

// ---------------------------------------------------------------------------
// NotifySubtitleStarted (multi-slot)
// ---------------------------------------------------------------------------

void USubtitleSubsystem::NotifySubtitleStarted(uint32 SlotID, const FText& InSubtitleText, FLinearColor InBarColor,
	const FSubtitleAppearance& InAppearance, const FText& InSpeakerName)
{
	// Update BP-compat state to reflect the latest subtitle
	bIsSubtitleActive   = true;
	CurrentSubtitleText = InSubtitleText;
	CurrentSpeakerName  = InSpeakerName;
	CurrentAppearance   = InAppearance;

	EnsureSlateWidgets();
	AddToViewport();

	// Get or create the slot
	TSharedPtr<FSubtitleSlot>& SlotPtr = ActiveSlots.FindOrAdd(SlotID);
	if (!SlotPtr.IsValid())
	{
		SlotPtr = MakeShared<FSubtitleSlot>();
	}
	FSubtitleSlot& Slot = *SlotPtr;

	// Reset per-slot state
	Slot.Text                = InSubtitleText;
	Slot.SpeakerName         = InSpeakerName;
	Slot.BarColor            = InBarColor;
	Slot.Appearance          = InAppearance;
	Slot.bTypewriterActive   = false;
	Slot.LastSoundCharIndex  = -1;
	Slot.LastSoundPlayTime   = 0.0;
	Slot.CurrentPageIndex    = 0;
	Slot.TypewriterPages.Empty();
	Slot.TypewriterPageCharStarts.Empty();
	Slot.bPendingRemoval     = false;

	if (!Slot.EntryVBox.IsValid())
	{
		CreateSlotWidget(SlotID, Slot);
	}
	else
	{
		// Reset typewriter sizing if reusing an existing widget
		if (Slot.TypewriterSizerOverlay.IsValid())
		{
			Slot.TypewriterSizerOverlay->ClearChildren();
		}
		if (Slot.SubtitleBorder.IsValid())
		{
			Slot.SubtitleBorder->SetHAlign(HAlign_Fill);
		}
	}

	ApplyAppearanceToSlot(Slot, InAppearance);
	ApplySpeakerAndSeparatorToSlot(Slot, InAppearance, InSpeakerName);

	if (Slot.SubtitleTextBlock.IsValid())
	{
		Slot.SubtitleTextBlock->SetText(InSubtitleText);
	}
	// Sync text to outline layers
	if (Slot.InnerOutlineTextBlock.IsValid()) Slot.InnerOutlineTextBlock->SetText(InSubtitleText);
	if (Slot.OuterOutlineTextBlock.IsValid()) Slot.OuterOutlineTextBlock->SetText(InSubtitleText);
	for (int32 i = 0; i < FSubtitleSlot::NumBlurSteps; ++i)
	{
		if (Slot.InnerBlurTextBlocks[i].IsValid()) Slot.InnerBlurTextBlocks[i]->SetText(InSubtitleText);
		if (Slot.OuterBlurTextBlocks[i].IsValid()) Slot.OuterBlurTextBlocks[i]->SetText(InSubtitleText);
	}

	WidgetOverlay->SetVisibility(EVisibility::SelfHitTestInvisible);
	StartSlotAnimation(Slot, SlotID, InAppearance.EntranceType, InAppearance.EntranceDuration, false);
	StartTremble(Slot, SlotID);

	OnSubtitleStarted.Broadcast(InSubtitleText, InBarColor, InSpeakerName);
}

// Legacy (SlotID=0)
void USubtitleSubsystem::NotifySubtitleStarted(const FText& InSubtitleText, FLinearColor InBarColor,
	const FSubtitleAppearance& InAppearance, const FText& InSpeakerName)
{
	NotifySubtitleStarted(0, InSubtitleText, InBarColor, InAppearance, InSpeakerName);
}

// ---------------------------------------------------------------------------
// NotifySubtitleEnded (multi-slot)
// ---------------------------------------------------------------------------

void USubtitleSubsystem::NotifySubtitleEnded(uint32 SlotID)
{
	TSharedPtr<FSubtitleSlot>* Found = ActiveSlots.Find(SlotID);
	if (!Found || !Found->IsValid()) { return; }

	FSubtitleSlot& Slot = **Found;

	if (Slot.bPendingRemoval) { return; }

	const ESubtitleEntranceType ExitType     = Slot.Appearance.GetEffectiveExitType();
	const float                 ExitDuration = Slot.Appearance.GetEffectiveExitDuration();

	if (ExitType != ESubtitleEntranceType::None && ExitDuration > 0.0f)
	{
		// Deferred removal: animation tick will call RemoveSlotWidget when done
		Slot.bPendingRemoval = true;
		StartSlotAnimation(Slot, SlotID, ExitType, ExitDuration, true);
	}
	else
	{
		RemoveSlotWidget(SlotID);
	}

	OnSubtitleEnded.Broadcast();
}

// Legacy (SlotID=0)
void USubtitleSubsystem::NotifySubtitleEnded()
{
	NotifySubtitleEnded(0);
}

// ---------------------------------------------------------------------------
// UpdateTypewriterProgress (multi-slot)
// ---------------------------------------------------------------------------

void USubtitleSubsystem::UpdateTypewriterProgress(uint32 SlotID, int32 VisibleCharCount)
{
	TSharedPtr<FSubtitleSlot>* Found = ActiveSlots.Find(SlotID);
	if (!Found || !Found->IsValid()) { return; }

	FSubtitleSlot& Slot = **Found;

	if (!Slot.SubtitleTextBlock.IsValid()) { return; }

	const FString FullStr    = Slot.Text.ToString();
	const int32   TotalChars = FullStr.Len();
	const int32   ShowChars  = FMath::Clamp(VisibleCharCount, 0, TotalChars);

	// First call: set up fixed-width centering and paging
	if (!Slot.bTypewriterActive)
	{
		InitTypewriterState(Slot, SlotID, FullStr);
	}

	// Determine current page
	int32 PageIdx = 0;
	for (int32 i = Slot.TypewriterPages.Num() - 1; i >= 0; --i)
	{
		if (ShowChars >= Slot.TypewriterPageCharStarts[i])
		{
			PageIdx = i;
			break;
		}
	}

	if (PageIdx != Slot.CurrentPageIndex)
	{
		Slot.CurrentPageIndex = PageIdx;
		OnPageAdvanced.Broadcast(PageIdx);
	}

	const FString& PageText      = Slot.TypewriterPages[PageIdx];
	const int32    PageLocalChars = ShowChars - Slot.TypewriterPageCharStarts[PageIdx];
	const int32    ClampedLocal  = FMath::Clamp(PageLocalChars, 0, PageText.Len());

	PlayTypewriterSoundForSlot(Slot, SlotID, ShowChars);

	const FText TypewriterText = FText::FromString(PageText.Left(ClampedLocal));
	Slot.SubtitleTextBlock->SetText(TypewriterText);
	// Sync text to outline layers
	auto SetTextIfVisible = [&TypewriterText](const TSharedPtr<STextBlock>& TB)
	{
		if (TB.IsValid() && TB->GetVisibility() != EVisibility::Collapsed) TB->SetText(TypewriterText);
	};
	SetTextIfVisible(Slot.InnerOutlineTextBlock);
	SetTextIfVisible(Slot.OuterOutlineTextBlock);
	for (int32 i = 0; i < FSubtitleSlot::NumBlurSteps; ++i)
	{
		SetTextIfVisible(Slot.InnerBlurTextBlocks[i]);
		SetTextIfVisible(Slot.OuterBlurTextBlocks[i]);
	}

	if (ShowChars >= TotalChars)
	{
		SlotSoundCache.Remove(SlotID);
	}
}

// Legacy (SlotID=0)
void USubtitleSubsystem::UpdateTypewriterProgress(int32 VisibleCharCount)
{
	UpdateTypewriterProgress(0, VisibleCharCount);
}

// ---------------------------------------------------------------------------
// GetMaxOutlinePixels — file-scope helper for outline-aware text measurement
// ---------------------------------------------------------------------------

static int32 GetMaxOutlinePixels(const FSubtitleAppearance& InAppearance)
{
	if (!InAppearance.bEnableOutline1) { return 0; }
	int32 Max = InAppearance.OutlineSize1 + FMath::CeilToInt(InAppearance.OutlineBlur1);
	if (InAppearance.bEnableOutline2)
	{
		const int32 Outer = InAppearance.OutlineSize1 + InAppearance.OutlineSize2 + FMath::CeilToInt(InAppearance.OutlineBlur2);
		Max = FMath::Max(Max, Outer);
	}
	return Max;
}

// ---------------------------------------------------------------------------
// InitTypewriterState
// ---------------------------------------------------------------------------

void USubtitleSubsystem::InitTypewriterState(FSubtitleSlot& Slot, uint32 SlotID, const FString& FullStr)
{
	Slot.bTypewriterActive   = true;
	Slot.LastSoundCharIndex  = -1;
	Slot.LastSoundPlayTime   = 0.0;
	Slot.CurrentPageIndex    = 0;

	// Load and cache typewriter sound
	SlotSoundCache.Add(SlotID, Slot.Appearance.TypewriterSound.LoadSynchronous());

	// Split text into lines
	TArray<FString> AllLines;
	FullStr.ParseIntoArray(AllLines, TEXT("\n"), /*bCullEmpty=*/false);

	// Build pages (BotW-style paging)
	const int32 MaxLines = Slot.Appearance.MaxLinesPerPage;
	Slot.TypewriterPages.Empty();
	Slot.TypewriterPageCharStarts.Empty();

	if (MaxLines > 0 && AllLines.Num() > MaxLines)
	{
		int32 CharOffset = 0;
		for (int32 i = 0; i < AllLines.Num(); i += MaxLines)
		{
			FString PageText;
			for (int32 j = i; j < FMath::Min(i + MaxLines, AllLines.Num()); ++j)
			{
				if (j > i) { PageText += TEXT("\n"); }
				PageText += AllLines[j];
			}
			Slot.TypewriterPageCharStarts.Add(CharOffset);
			Slot.TypewriterPages.Add(PageText);
			CharOffset += PageText.Len();
			if (i + MaxLines < AllLines.Num()) { CharOffset += 1; }
		}
	}
	else
	{
		Slot.TypewriterPages.Add(FullStr);
		Slot.TypewriterPageCharStarts.Add(0);
	}

	// Reserve the final layout: a transparent copy of every page sits behind the visible text,
	// so the box is as wide as the widest line and as tall as the tallest page. Slate lays these
	// out at the actual render scale and re-wraps them on resize (include outline extent).
	if (Slot.TypewriterSizerOverlay.IsValid() && Slot.SubtitleBorder.IsValid())
	{
		Slot.TypewriterSizerOverlay->ClearChildren();

		FSlateFontInfo SizerFont = Slot.FontInfo;
		SizerFont.OutlineSettings.OutlineSize  = GetMaxOutlinePixels(Slot.Appearance);
		SizerFont.OutlineSettings.OutlineColor = FLinearColor::Transparent;

		for (const FString& Page : Slot.TypewriterPages)
		{
			Slot.TypewriterSizerOverlay->AddSlot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Page))
				.Font(SizerFont)
				.ColorAndOpacity(FLinearColor::Transparent)
				.AutoWrapText(true)
			];
		}
		Slot.SubtitleBorder->SetHAlign(HAlign_Center);
	}

	Slot.SubtitleTextBlock->SetJustification(ETextJustify::Left);
	if (Slot.InnerOutlineTextBlock.IsValid()) Slot.InnerOutlineTextBlock->SetJustification(ETextJustify::Left);
	if (Slot.OuterOutlineTextBlock.IsValid()) Slot.OuterOutlineTextBlock->SetJustification(ETextJustify::Left);
	for (int32 i = 0; i < FSubtitleSlot::NumBlurSteps; ++i)
	{
		if (Slot.InnerBlurTextBlocks[i].IsValid()) Slot.InnerBlurTextBlocks[i]->SetJustification(ETextJustify::Left);
		if (Slot.OuterBlurTextBlocks[i].IsValid()) Slot.OuterBlurTextBlocks[i]->SetJustification(ETextJustify::Left);
	}
}

// ---------------------------------------------------------------------------
// PlayTypewriterSoundForSlot
// ---------------------------------------------------------------------------

void USubtitleSubsystem::PlayTypewriterSoundForSlot(FSubtitleSlot& Slot, uint32 SlotID, int32 CurrentCharIndex)
{
	TObjectPtr<USoundBase>* SoundPtr = SlotSoundCache.Find(SlotID);
	if (!SoundPtr || !(*SoundPtr)) { return; }
	if (CurrentCharIndex <= Slot.LastSoundCharIndex) { return; }

	const double Now         = FPlatformTime::Seconds();
	const float  MinInterval = Slot.Appearance.TypewriterSoundInterval;

	if (Slot.LastSoundPlayTime > 0.0 && (Now - Slot.LastSoundPlayTime) < static_cast<double>(MinInterval))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		UGameplayStatics::PlaySound2D(World, *SoundPtr);
	}

	Slot.LastSoundCharIndex = CurrentCharIndex;
	Slot.LastSoundPlayTime  = Now;
}

// ---------------------------------------------------------------------------
// ShowMessage / ShowMessageEx / ShowPersistentMessage / HideMessage
// ---------------------------------------------------------------------------

void USubtitleSubsystem::ShowMessage(const FText& Text, float Duration, ESubtitleEntranceType Animation,
	float AnimationDuration, const FText& SpeakerName)
{
	FSubtitleAppearance Appearance;
	Appearance.FontSize         = 50;
	Appearance.TextColor        = FLinearColor::White;
	Appearance.BackgroundColor  = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	Appearance.WindowOpacity    = 0.5f;
	Appearance.VerticalPosition = ESubtitleVerticalPosition::Bottom;
	Appearance.ScreenPadding    = FMargin(40.0f, 20.0f);
	Appearance.EntranceType     = Animation;
	Appearance.EntranceDuration = AnimationDuration;
	Appearance.bOverrideExitAnimation = false;

	ShowMessageEx(Text, Duration, Appearance, SpeakerName);
}

void USubtitleSubsystem::ShowPersistentMessage(const FText& Text, ESubtitleEntranceType Animation,
	float AnimationDuration, const FText& SpeakerName)
{
	ShowMessage(Text, 0.0f, Animation, AnimationDuration, SpeakerName);
}

void USubtitleSubsystem::ShowMessageEx(const FText& Text, float Duration, const FSubtitleAppearance& Appearance,
	const FText& SpeakerName)
{
	// Cancel any previous auto-hide timer for slot 0
	if (TSharedPtr<FSubtitleSlot>* ExistingSlot = ActiveSlots.Find(0))
	{
		FSubtitleSlot& S = **ExistingSlot;
		if (S.AutoHideTimerHandle.IsValid() && S.SubtitleBorder.IsValid())
		{
			S.SubtitleBorder->UnRegisterActiveTimer(S.AutoHideTimerHandle.ToSharedRef());
			S.AutoHideTimerHandle.Reset();
		}
	}

	NotifySubtitleStarted(0, Text, FLinearColor::White, Appearance, SpeakerName);

	TSharedPtr<FSubtitleSlot>* SlotPtr = ActiveSlots.Find(0);
	if (!SlotPtr || !SlotPtr->IsValid()) { return; }
	FSubtitleSlot& Slot = **SlotPtr;

	Slot.bIsShowMessageActive = true;

	if (Duration > 0.0f)
	{
		Slot.AutoHideRemaining = Appearance.EntranceDuration + Duration;

		if (Slot.SubtitleBorder.IsValid())
		{
			Slot.AutoHideTimerHandle = Slot.SubtitleBorder->RegisterActiveTimer(
				0.0f,
				FWidgetActiveTimerDelegate::CreateWeakLambda(this,
					[this](double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType
					{
						return TickAutoHide(InCurrentTime, InDeltaTime);
					}
				)
			);
		}
	}
}

void USubtitleSubsystem::HideMessage()
{
	TSharedPtr<FSubtitleSlot>* SlotPtr = ActiveSlots.Find(0);
	if (!SlotPtr || !SlotPtr->IsValid()) { return; }

	FSubtitleSlot& Slot = **SlotPtr;
	if (!Slot.bIsShowMessageActive) { return; }

	Slot.bIsShowMessageActive = false;

	if (Slot.AutoHideTimerHandle.IsValid() && Slot.SubtitleBorder.IsValid())
	{
		Slot.SubtitleBorder->UnRegisterActiveTimer(Slot.AutoHideTimerHandle.ToSharedRef());
		Slot.AutoHideTimerHandle.Reset();
	}

	NotifySubtitleEnded(0);
}

EActiveTimerReturnType USubtitleSubsystem::TickAutoHide(double InCurrentTime, float InDeltaTime)
{
	TSharedPtr<FSubtitleSlot>* SlotPtr = ActiveSlots.Find(0);
	if (!SlotPtr || !SlotPtr->IsValid()) { return EActiveTimerReturnType::Stop; }

	FSubtitleSlot& Slot = **SlotPtr;
	if (!Slot.bIsShowMessageActive) { return EActiveTimerReturnType::Stop; }

	Slot.AutoHideRemaining -= InDeltaTime;
	if (Slot.AutoHideRemaining <= 0.0f)
	{
		Slot.bIsShowMessageActive = false;
		NotifySubtitleEnded(0);
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

// ---------------------------------------------------------------------------
// ShouldCreateSubsystem / Deinitialize
// ---------------------------------------------------------------------------

bool USubtitleSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer();
}

void USubtitleSubsystem::Deinitialize()
{
	// Clear all active slots (stops timers, releases Slate widgets)
	for (auto& Pair : ActiveSlots)
	{
		FSubtitleSlot& Slot = *Pair.Value;
		if (Slot.AnimTimerHandle.IsValid() && Slot.SubtitleBorder.IsValid())
		{
			Slot.SubtitleBorder->UnRegisterActiveTimer(Slot.AnimTimerHandle.ToSharedRef());
		}
		if (Slot.AutoHideTimerHandle.IsValid() && Slot.SubtitleBorder.IsValid())
		{
			Slot.SubtitleBorder->UnRegisterActiveTimer(Slot.AutoHideTimerHandle.ToSharedRef());
		}
		StopTremble(Slot);
	}
	ActiveSlots.Empty();
	SlotSoundCache.Empty();

	RemoveFromViewport();
#if WITH_EDITOR
	ActiveSections.Empty();
#endif
	DPIScalerWidget.Reset();
	WidgetOverlay.Reset();
	ContentVerticalBox.Reset();
	OverlaySlot = nullptr;

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Editor helpers
// ---------------------------------------------------------------------------

#if WITH_EDITOR
void USubtitleSubsystem::SetActiveSection(uint32 SlotID, UMovieSceneSeqSubtitleSection* InSection)
{
	// Skip the per-frame overhead if section hasn't changed
	TWeakObjectPtr<UMovieSceneSeqSubtitleSection>* Current = ActiveSections.Find(SlotID);
	if (Current && Current->Get() == InSection)
	{
		return;
	}

	ActiveSections.Add(SlotID, InSection);

	// Update drag-enabled state on all slot drag handles (only when section actually changes)
	const UWorld* World = GetWorld();
	const bool bIsGameWorld = World && World->IsGameWorld();
	for (auto& Pair : ActiveSlots)
	{
		if (Pair.Value.IsValid() && Pair.Value->DragHandle.IsValid())
		{
			Pair.Value->DragHandle->SetDragEnabled(!bIsGameWorld);
		}
	}
}

void USubtitleSubsystem::OnSlotDragOffsetChanged(uint32 SlotID, FVector2D NewOffset)
{
	// Update section data
	UMovieSceneSeqSubtitleSection* Section = ActiveSections.FindRef(SlotID).Get();
	if (Section)
	{
		if (Section->bOverrideAppearance)
		{
			Section->Modify();
			Section->AppearanceOverride.ScreenOffset = NewOffset;
		}
		else
		{
			UMovieSceneSubtitleTrack* Track = Section->GetTypedOuter<UMovieSceneSubtitleTrack>();
			if (Track)
			{
				Track->Modify();
				Track->Appearance.ScreenOffset = NewOffset;
			}
		}
	}

	// Update slot state
	if (TSharedPtr<FSubtitleSlot>* Found = ActiveSlots.Find(SlotID))
	{
		(*Found)->Appearance.ScreenOffset = NewOffset;
	}

	// Keep BP-compat CurrentAppearance in sync with the last dragged slot
	CurrentAppearance.ScreenOffset = NewOffset;
}
#endif

// ---------------------------------------------------------------------------
// ApplyAppearanceToSlot — static helpers
// ---------------------------------------------------------------------------

namespace
{
	/**
	 * Build the font for subtitle / speaker text. FSlateFontInfo needs a UFont (font provider);
	 * any other asset (e.g. a Font Face) falls back to the engine default font.
	 */
	static FSlateFontInfo MakeSubtitleFont(const FSubtitleAppearance& InAppearance, int32 InSize, FName DefaultTypeface)
	{
		if (const UFont* Font = Cast<UFont>(InAppearance.FontAsset.LoadSynchronous()))
		{
			return FSlateFontInfo(Font, static_cast<float>(InSize));
		}
		return FCoreStyle::GetDefaultFontStyle(DefaultTypeface, static_cast<float>(InSize));
	}

	/** Apply window background (solid / rounded / image) to a slot's SubtitleBorder. */
	static void ApplyWindowBackground(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance)
	{
		if (!Slot.SubtitleBorder.IsValid()) { return; }

		FLinearColor BgColor = InAppearance.BackgroundColor;
		BgColor.A *= InAppearance.WindowOpacity;

		switch (InAppearance.WindowStyle)
		{
		case EMessageWindowStyle::Rounded:
			Slot.WindowBrush = FSlateBrush();
			Slot.WindowBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Slot.WindowBrush.OutlineSettings.CornerRadii = FVector4(
				InAppearance.WindowCornerRadius, InAppearance.WindowCornerRadius,
				InAppearance.WindowCornerRadius, InAppearance.WindowCornerRadius);
			Slot.WindowBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Slot.WindowBrush.TintColor = FSlateColor(BgColor);
			Slot.SubtitleBorder->SetBorderImage(&Slot.WindowBrush);
			Slot.SubtitleBorder->SetBorderBackgroundColor(FLinearColor::White);
			break;

		case EMessageWindowStyle::Image:
		{
			UTexture2D* Tex = InAppearance.WindowImage.LoadSynchronous();
			if (Tex)
			{
				Slot.WindowBrush = FSlateBrush();
				Slot.WindowBrush.SetResourceObject(Tex);
				Slot.WindowBrush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
				Slot.WindowBrush.DrawAs    = ESlateBrushDrawType::Image;
				Slot.SubtitleBorder->SetBorderImage(&Slot.WindowBrush);
			}
			else
			{
				Slot.SubtitleBorder->SetBorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"));
			}
			Slot.SubtitleBorder->SetBorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, InAppearance.WindowOpacity));
			break;
		}
		default: // Square
			Slot.SubtitleBorder->SetBorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"));
			Slot.SubtitleBorder->SetBorderBackgroundColor(BgColor);
			break;
		}
	}

	/**
	 * Configure N blur layers for one outline using a Gaussian-like alpha curve.
	 * Layers are spread evenly from BlurRadius/N to BlurRadius, with alpha decaying
	 * as exp(-k * (i/N)^2) for a smooth soft-glow gradient.
	 */
	static void ConfigureBlurLayers(
		TSharedPtr<STextBlock>* BlurArray,
		int32 NumSteps,
		const FSlateFontInfo& BaseFontInfo,
		int32 BaseOutlineSize,
		float BlurRadius,
		const FLinearColor& OutlineColor,
		const FSlateColor& FillColor)
	{
		if (BlurRadius <= 0.f) { return; }

		// Gaussian decay constant — k=3.0 gives a natural falloff from ~0.72 to ~0.05
		constexpr float GaussK = 3.0f;

		for (int32 i = 0; i < NumSteps; ++i)
		{
			if (!BlurArray[i].IsValid()) { continue; }

			// t ranges from 1/N (innermost, smallest blur) to 1.0 (outermost, full blur)
			const float t = static_cast<float>(i + 1) / static_cast<float>(NumSteps);
			const int32 StepSize = FMath::CeilToInt(BlurRadius * t);
			const float StepAlpha = FMath::Exp(-GaussK * t * t);

			FSlateFontInfo Font = BaseFontInfo;
			Font.OutlineSettings.OutlineSize = BaseOutlineSize + StepSize;
			FLinearColor C = OutlineColor;
			C.A *= StepAlpha;
			Font.OutlineSettings.OutlineColor = C;
			Font.OutlineSettings.bSeparateFillAlpha = true;
			BlurArray[i]->SetFont(Font);
			BlurArray[i]->SetColorAndOpacity(FillColor);
			BlurArray[i]->SetVisibility(EVisibility::SelfHitTestInvisible);
		}
	}

	/**
	 * Configure outline layers for a set of text blocks.
	 * Supports: no outline, single outline (applied directly to main), and dual outline (layered).
	 * Each outline can have an N-step Gaussian blur gradient for a smooth soft glow effect.
	 */
	static void ConfigureOutlineLayers(
		TSharedPtr<STextBlock> MainTextBlock,
		TSharedPtr<STextBlock> InnerOutline,
		TSharedPtr<STextBlock>* InnerBlurArray,
		TSharedPtr<STextBlock> OuterOutline,
		TSharedPtr<STextBlock>* OuterBlurArray,
		int32 NumBlurSteps,
		const FSlateFontInfo& BaseFontInfo,
		const FSubtitleAppearance& Appearance,
		const FSlateColor& TextColor)
	{
		// Default: all outline layers collapsed
		auto CollapseIfValid = [](const TSharedPtr<STextBlock>& TB)
		{
			if (TB.IsValid()) TB->SetVisibility(EVisibility::Collapsed);
		};
		CollapseIfValid(InnerOutline);
		CollapseIfValid(OuterOutline);
		for (int32 i = 0; i < NumBlurSteps; ++i)
		{
			CollapseIfValid(InnerBlurArray[i]);
			CollapseIfValid(OuterBlurArray[i]);
		}

		if (!Appearance.bEnableOutline1)
		{
			// No outlines — main text uses base font as-is
			if (MainTextBlock.IsValid())
			{
				FSlateFontInfo Font = BaseFontInfo;
				Font.OutlineSettings.OutlineSize = 0;
				MainTextBlock->SetFont(Font);
				MainTextBlock->SetColorAndOpacity(TextColor);
			}
			return;
		}

		const bool bHasOutline2 = Appearance.bEnableOutline2;

		if (!bHasOutline2)
		{
			// Single outline only — apply directly to MainTextBlock
			FSlateFontInfo Font = BaseFontInfo;
			Font.OutlineSettings.OutlineSize = Appearance.OutlineSize1;
			Font.OutlineSettings.OutlineColor = Appearance.OutlineColor1;
			Font.OutlineSettings.bSeparateFillAlpha = false;
			MainTextBlock->SetFont(Font);
			MainTextBlock->SetColorAndOpacity(TextColor);

			// Blur for single outline (N-step Gaussian)
			ConfigureBlurLayers(InnerBlurArray, NumBlurSteps, BaseFontInfo,
				Appearance.OutlineSize1, Appearance.OutlineBlur1,
				Appearance.OutlineColor1, FSlateColor(FLinearColor::Transparent));
			return;
		}

		// --- Dual outline mode ---

		// Front layer (MainTextBlock): text only, no outline
		{
			FSlateFontInfo Font = BaseFontInfo;
			Font.OutlineSettings.OutlineSize = 0;
			MainTextBlock->SetFont(Font);
			MainTextBlock->SetColorAndOpacity(TextColor);
		}

		// Inner outline layer
		if (InnerOutline.IsValid())
		{
			FSlateFontInfo Font = BaseFontInfo;
			Font.OutlineSettings.OutlineSize = Appearance.OutlineSize1;
			Font.OutlineSettings.OutlineColor = Appearance.OutlineColor1;
			Font.OutlineSettings.bSeparateFillAlpha = true;
			InnerOutline->SetFont(Font);
			InnerOutline->SetColorAndOpacity(FSlateColor(FLinearColor::Transparent));
			InnerOutline->SetVisibility(EVisibility::SelfHitTestInvisible);
		}

		// Inner blur (N-step Gaussian)
		ConfigureBlurLayers(InnerBlurArray, NumBlurSteps, BaseFontInfo,
			Appearance.OutlineSize1, Appearance.OutlineBlur1,
			Appearance.OutlineColor1, FSlateColor(FLinearColor::Transparent));

		// Outer outline layer: size = outline1 + outline2, fill = OutlineColor1
		if (OuterOutline.IsValid())
		{
			FSlateFontInfo Font = BaseFontInfo;
			Font.OutlineSettings.OutlineSize = Appearance.OutlineSize1 + Appearance.OutlineSize2;
			Font.OutlineSettings.OutlineColor = Appearance.OutlineColor2;
			Font.OutlineSettings.bSeparateFillAlpha = true;
			OuterOutline->SetFont(Font);
			OuterOutline->SetColorAndOpacity(FSlateColor(Appearance.OutlineColor1));
			OuterOutline->SetVisibility(EVisibility::SelfHitTestInvisible);
		}

		// Outer blur (N-step Gaussian)
		ConfigureBlurLayers(OuterBlurArray, NumBlurSteps, BaseFontInfo,
			Appearance.OutlineSize1 + Appearance.OutlineSize2, Appearance.OutlineBlur2,
			Appearance.OutlineColor2, FSlateColor(FLinearColor::Transparent));
	}

	/** Apply font, size, color, justification and outline to a slot's SubtitleTextBlock. */
	static void ApplyTextStyling(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance)
	{
		if (!Slot.SubtitleTextBlock.IsValid()) { return; }

		// Justification is skipped during typewriter — UpdateTypewriterProgress owns it while active
		if (!Slot.bTypewriterActive)
		{
			ETextJustify::Type Justify = ETextJustify::Center;
			switch (InAppearance.TextAlignment)
			{
			case ESubtitleTextAlignment::Left:  Justify = ETextJustify::Left;  break;
			case ESubtitleTextAlignment::Right: Justify = ETextJustify::Right; break;
			default: break;
			}
			Slot.SubtitleTextBlock->SetJustification(Justify);
			// Apply same justification to all outline layers
			if (Slot.InnerOutlineTextBlock.IsValid()) Slot.InnerOutlineTextBlock->SetJustification(Justify);
			if (Slot.OuterOutlineTextBlock.IsValid()) Slot.OuterOutlineTextBlock->SetJustification(Justify);
			for (int32 i = 0; i < FSubtitleSlot::NumBlurSteps; ++i)
			{
				if (Slot.InnerBlurTextBlocks[i].IsValid()) Slot.InnerBlurTextBlocks[i]->SetJustification(Justify);
				if (Slot.OuterBlurTextBlocks[i].IsValid()) Slot.OuterBlurTextBlocks[i]->SetJustification(Justify);
			}
		}

		const FSlateFontInfo FontInfo = MakeSubtitleFont(InAppearance, InAppearance.FontSize, TEXT("Regular"));

		Slot.FontInfo = FontInfo;

		ConfigureOutlineLayers(
			Slot.SubtitleTextBlock,
			Slot.InnerOutlineTextBlock,
			Slot.InnerBlurTextBlocks,
			Slot.OuterOutlineTextBlock,
			Slot.OuterBlurTextBlocks,
			FSubtitleSlot::NumBlurSteps,
			FontInfo,
			InAppearance,
			FSlateColor(InAppearance.TextColor));
	}
} // namespace

// ---------------------------------------------------------------------------
// ApplyOverlayPosition — global overlay slot (shared by all slots)
// ---------------------------------------------------------------------------

void USubtitleSubsystem::ApplyOverlayPosition(const FSubtitleAppearance& InAppearance)
{
	if (!OverlaySlot) { return; }

	switch (InAppearance.VerticalPosition)
	{
	case ESubtitleVerticalPosition::Top:    OverlaySlot->SetVerticalAlignment(VAlign_Top);    break;
	case ESubtitleVerticalPosition::Center: OverlaySlot->SetVerticalAlignment(VAlign_Center); break;
	default:                                OverlaySlot->SetVerticalAlignment(VAlign_Bottom);  break;
	}

	switch (InAppearance.HorizontalPosition)
	{
	case ESubtitleHorizontalPosition::Left:  OverlaySlot->SetHorizontalAlignment(HAlign_Left);  break;
	case ESubtitleHorizontalPosition::Right: OverlaySlot->SetHorizontalAlignment(HAlign_Right); break;
	default:                                 OverlaySlot->SetHorizontalAlignment(HAlign_Fill);  break;
	}

	OverlaySlot->SetPadding(InAppearance.ScreenPadding);
}

// ---------------------------------------------------------------------------
// ApplyAppearanceToSlot
// ---------------------------------------------------------------------------

void USubtitleSubsystem::ApplyAppearanceToSlot(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance)
{
	ApplyOverlayPosition(InAppearance);

	// Per-slot screen offset — applied to AnimWrapper so it doesn't interfere with animation transforms
#if WITH_EDITOR
	if (Slot.DragHandle.IsValid())
	{
		Slot.DragHandle->SetCurrentOffset(InAppearance.ScreenOffset);
	}
	// In editor, DragHandle applies the offset to its child (EntryVBox) via OnMouseMove.
	// When not dragging we set it explicitly so the stored offset is reflected immediately.
	if (Slot.EntryVBox.IsValid())
	{
		Slot.EntryVBox->SetRenderTransform(FSlateRenderTransform(InAppearance.ScreenOffset));
	}
#else
	if (Slot.EntryVBox.IsValid())
	{
		Slot.EntryVBox->SetRenderTransform(FSlateRenderTransform(InAppearance.ScreenOffset));
	}
#endif

	ApplyWindowBackground(Slot, InAppearance);

	// MessageWindowHeight is a minimum: the window still grows to fit larger text or more lines
	if (Slot.MessageWindowBox.IsValid())
	{
		if (InAppearance.MessageWindowHeight > 0.0f)
			Slot.MessageWindowBox->SetMinDesiredHeight(InAppearance.MessageWindowHeight);
		else
			Slot.MessageWindowBox->SetMinDesiredHeight(FOptionalSize());
	}

	ApplyTextStyling(Slot, InAppearance);
}

// ---------------------------------------------------------------------------
// ApplySpeakerAndSeparatorToSlot — static helpers
// ---------------------------------------------------------------------------

namespace
{
	/** Show/hide and style the speaker name text block (with outline support). */
	static void ApplySpeakerName(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance,
		const FText& InSpeakerName, bool bHasSpeaker)
	{
		if (!Slot.SpeakerTextBlock.IsValid()) { return; }

		if (!bHasSpeaker)
		{
			Slot.SpeakerTextBlock->SetVisibility(EVisibility::Collapsed);
			if (Slot.SpeakerInnerOutlineTextBlock.IsValid()) Slot.SpeakerInnerOutlineTextBlock->SetVisibility(EVisibility::Collapsed);
			if (Slot.SpeakerOuterOutlineTextBlock.IsValid()) Slot.SpeakerOuterOutlineTextBlock->SetVisibility(EVisibility::Collapsed);
			for (int32 i = 0; i < FSubtitleSlot::NumBlurSteps; ++i)
			{
				if (Slot.SpeakerInnerBlurTextBlocks[i].IsValid()) Slot.SpeakerInnerBlurTextBlocks[i]->SetVisibility(EVisibility::Collapsed);
				if (Slot.SpeakerOuterBlurTextBlocks[i].IsValid()) Slot.SpeakerOuterBlurTextBlocks[i]->SetVisibility(EVisibility::Collapsed);
			}
			return;
		}

		Slot.SpeakerTextBlock->SetText(InSpeakerName);

		const FSlateFontInfo SpeakerFont = MakeSubtitleFont(InAppearance, InAppearance.SpeakerNameFontSize, TEXT("Bold"));

		// Set text on all speaker outline layers
		if (Slot.SpeakerInnerOutlineTextBlock.IsValid()) Slot.SpeakerInnerOutlineTextBlock->SetText(InSpeakerName);
		if (Slot.SpeakerOuterOutlineTextBlock.IsValid()) Slot.SpeakerOuterOutlineTextBlock->SetText(InSpeakerName);
		for (int32 i = 0; i < FSubtitleSlot::NumBlurSteps; ++i)
		{
			if (Slot.SpeakerInnerBlurTextBlocks[i].IsValid()) Slot.SpeakerInnerBlurTextBlocks[i]->SetText(InSpeakerName);
			if (Slot.SpeakerOuterBlurTextBlocks[i].IsValid()) Slot.SpeakerOuterBlurTextBlocks[i]->SetText(InSpeakerName);
		}

		// Configure outline layers (also sets font and color on main SpeakerTextBlock)
		ConfigureOutlineLayers(
			Slot.SpeakerTextBlock,
			Slot.SpeakerInnerOutlineTextBlock,
			Slot.SpeakerInnerBlurTextBlocks,
			Slot.SpeakerOuterOutlineTextBlock,
			Slot.SpeakerOuterBlurTextBlocks,
			FSubtitleSlot::NumBlurSteps,
			SpeakerFont,
			InAppearance,
			FSlateColor(InAppearance.SpeakerNameColor));

		Slot.SpeakerTextBlock->SetVisibility(EVisibility::SelfHitTestInvisible);
	}

	/** Apply image-based separator styling. */
	static void ApplySeparatorImage(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance)
	{
		if (Slot.SeparatorLineWidget.IsValid())
			Slot.SeparatorLineWidget->SetVisibility(EVisibility::Collapsed);

		UTexture2D* Tex   = InAppearance.LineImage.LoadSynchronous();
		const float NatW  = Tex ? static_cast<float>(Tex->GetSizeX()) : 64.f;
		const float NatH  = Tex ? static_cast<float>(Tex->GetSizeY()) : 4.f;

		if (Tex)
		{
			ESlateBrushTileType::Type TileType = ESlateBrushTileType::NoTile;
			switch (InAppearance.LineImageTiling)
			{
			case ELineImageTiling::TileHorizontal: TileType = ESlateBrushTileType::Horizontal; break;
			case ELineImageTiling::TileVertical:   TileType = ESlateBrushTileType::Vertical;   break;
			case ELineImageTiling::TileBoth:       TileType = ESlateBrushTileType::Both;        break;
			default:                               TileType = ESlateBrushTileType::NoTile;      break;
			}
			Slot.CustomSeparatorBrush = FSlateBrush();
			Slot.CustomSeparatorBrush.SetResourceObject(Tex);
			Slot.CustomSeparatorBrush.ImageSize = FVector2D(NatW, NatH);
			Slot.CustomSeparatorBrush.DrawAs    = ESlateBrushDrawType::Image;
			Slot.CustomSeparatorBrush.Tiling    = TileType;
			Slot.SeparatorLineBorder->SetBorderImage(&Slot.CustomSeparatorBrush);
			Slot.SeparatorLineBorder->SetBorderBackgroundColor(InAppearance.LineImageColor);
		}
		Slot.SeparatorLineBorder->SetVisibility(EVisibility::SelfHitTestInvisible);

		Slot.SeparatorBox->SetHeightOverride(InAppearance.LineImageHeight > 0.f ? InAppearance.LineImageHeight : NatH);
		Slot.SeparatorBox->SetWidthOverride(InAppearance.SeparatorWidth  > 0.f ? InAppearance.SeparatorWidth  : NatW);
	}

	/** Apply gradient line separator styling. */
	static void ApplySeparatorLine(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance)
	{
		Slot.SeparatorLineBorder->SetVisibility(EVisibility::Collapsed);

		if (Slot.SeparatorLineWidget.IsValid())
		{
			Slot.SeparatorLineWidget->SetColor(InAppearance.SeparatorLineColor);
			Slot.SeparatorLineWidget->SetFadeLength(InAppearance.SeparatorFadeLength);
			Slot.SeparatorLineWidget->SetVisibility(EVisibility::SelfHitTestInvisible);
		}

		Slot.SeparatorBox->SetHeightOverride(InAppearance.SeparatorLineThickness);
		if (InAppearance.SeparatorWidth > 0.f)
			Slot.SeparatorBox->SetWidthOverride(InAppearance.SeparatorWidth);
		else
			Slot.SeparatorBox->SetWidthOverride(FOptionalSize());
	}

	/**
	 * Apply horizontal alignment of the speaker name and separator.
	 * Alignment follows either the explicit SpeakerNameAlignment or the subtitle's TextAlignment.
	 */
	static void ApplyNameAndSeparatorAlignment(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance)
	{
		ESubtitleTextAlignment EffectiveAlign = InAppearance.TextAlignment;
		if (InAppearance.SpeakerNameAlignment != ESpeakerNameAlignment::FollowSubtitle)
		{
			switch (InAppearance.SpeakerNameAlignment)
			{
			case ESpeakerNameAlignment::Left:  EffectiveAlign = ESubtitleTextAlignment::Left;  break;
			case ESpeakerNameAlignment::Right: EffectiveAlign = ESubtitleTextAlignment::Right; break;
			default:                           EffectiveAlign = ESubtitleTextAlignment::Center; break;
			}
		}

		auto AlignToJustify = [](ESubtitleTextAlignment A) -> ETextJustify::Type
		{
			switch (A)
			{
			case ESubtitleTextAlignment::Left:  return ETextJustify::Left;
			case ESubtitleTextAlignment::Right: return ETextJustify::Right;
			default:                            return ETextJustify::Center;
			}
		};

		// Speaker name: always HAlign_Fill on the slot; text justification drives visual alignment
		if (Slot.SpeakerTextBlock.IsValid())
			Slot.SpeakerTextBlock->SetJustification(AlignToJustify(EffectiveAlign));
		if (Slot.SpeakerInnerOutlineTextBlock.IsValid()) Slot.SpeakerInnerOutlineTextBlock->SetJustification(AlignToJustify(EffectiveAlign));
		if (Slot.SpeakerOuterOutlineTextBlock.IsValid()) Slot.SpeakerOuterOutlineTextBlock->SetJustification(AlignToJustify(EffectiveAlign));
		for (int32 i = 0; i < FSubtitleSlot::NumBlurSteps; ++i)
		{
			if (Slot.SpeakerInnerBlurTextBlocks[i].IsValid()) Slot.SpeakerInnerBlurTextBlocks[i]->SetJustification(AlignToJustify(EffectiveAlign));
			if (Slot.SpeakerOuterBlurTextBlocks[i].IsValid()) Slot.SpeakerOuterBlurTextBlocks[i]->SetJustification(AlignToJustify(EffectiveAlign));
		}
		if (Slot.SpeakerNameSlot)
			Slot.SpeakerNameSlot->SetHorizontalAlignment(HAlign_Fill);

		// Separator: fixed-size boxes are aligned via HAlign; full-width lines use HAlign_Fill
		if (Slot.SeparatorSlot)
		{
			const bool bExplicitWidth = InAppearance.SeparatorWidth > 0.f || InAppearance.bUseLineImage;
			EHorizontalAlignment SepHAlign = HAlign_Fill;
			if (bExplicitWidth)
			{
				switch (EffectiveAlign)
				{
				case ESubtitleTextAlignment::Left:  SepHAlign = HAlign_Left;   break;
				case ESubtitleTextAlignment::Right: SepHAlign = HAlign_Right;  break;
				default:                            SepHAlign = HAlign_Center; break;
				}
			}
			Slot.SeparatorSlot->SetHorizontalAlignment(SepHAlign);
		}
	}
} // namespace

// ---------------------------------------------------------------------------
// ApplySpeakerAndSeparatorToSlot
// ---------------------------------------------------------------------------

void USubtitleSubsystem::ApplySpeakerAndSeparatorToSlot(FSubtitleSlot& Slot,
	const FSubtitleAppearance& InAppearance, const FText& InSpeakerName)
{
	const bool bHasSpeaker = !InSpeakerName.IsEmptyOrWhitespace();

	ApplySpeakerName(Slot, InAppearance, InSpeakerName, bHasSpeaker);

	if (Slot.SeparatorBox.IsValid() && Slot.SeparatorLineBorder.IsValid())
	{
		if (bHasSpeaker && InAppearance.bShowSeparatorLine)
		{
			if (InAppearance.bUseLineImage)
				ApplySeparatorImage(Slot, InAppearance);
			else
				ApplySeparatorLine(Slot, InAppearance);

			if (Slot.SeparatorSlot)
				Slot.SeparatorSlot->SetPadding(InAppearance.SeparatorPadding);

			Slot.SeparatorBox->SetVisibility(EVisibility::SelfHitTestInvisible);
		}
		else
		{
			Slot.SeparatorBox->SetVisibility(EVisibility::Collapsed);
		}
	}

	ApplyNameAndSeparatorAlignment(Slot, InAppearance);
}

// ---------------------------------------------------------------------------
// StartSlotAnimation / ApplySlotAnimationAlpha / TickSlotAnimation
// ---------------------------------------------------------------------------

void USubtitleSubsystem::StartSlotAnimation(FSubtitleSlot& Slot, uint32 SlotID,
	ESubtitleEntranceType InType, float InDuration, bool bReverse)
{
	if (!Slot.SubtitleBorder.IsValid()) { return; }

	Slot.SubtitleBorder->SetRenderOpacity(1.0f);
	Slot.SubtitleBorder->SetRenderTransform(FSlateRenderTransform());
	if (Slot.SpeakerTextOverlay.IsValid()) { Slot.SpeakerTextOverlay->SetRenderOpacity(1.0f); Slot.SpeakerTextOverlay->SetRenderTransform(FSlateRenderTransform()); }
	if (Slot.SeparatorBox.IsValid())       { Slot.SeparatorBox->SetRenderOpacity(1.0f);       Slot.SeparatorBox->SetRenderTransform(FSlateRenderTransform()); }

	if (InType == ESubtitleEntranceType::None || InDuration <= 0.0f)
	{
		Slot.bAnimating = false;
		if (bReverse)
		{
			// Exit with no animation — remove immediately
			RemoveSlotWidget(SlotID);
		}
		return;
	}

	Slot.AnimType     = InType;
	Slot.AnimDuration = InDuration;
	Slot.AnimElapsed  = 0.f;
	Slot.bAnimating   = true;
	Slot.bExitAnim    = bReverse;

	// Compute viewport-relative slide offsets in Slate units.
	// Viewport pixels per Slate unit = DPI curve value, both in game (engine game layer)
	// and in the editor viewport (GetSubtitleDPIScale).
	Slot.SlideOffsetX = 2000.f;
	Slot.SlideOffsetY = 1200.f;
	const FIntPoint VPSize = GetHostViewportSize();
	if (VPSize.X > 0 && VPSize.Y > 0)
	{
		if (const UUserInterfaceSettings* UISettings = GetDefault<UUserInterfaceSettings>())
		{
			const float UIScale = UISettings->GetDPIScaleBasedOnSize(VPSize);
			if (UIScale > 0.01f)
			{
				Slot.SlideOffsetX = static_cast<float>(VPSize.X) / UIScale;
				Slot.SlideOffsetY = static_cast<float>(VPSize.Y) / UIScale;
			}
		}
	}

	ApplySlotAnimationAlpha(Slot, bReverse ? 1.0f : 0.0f);

	// Stop any previous animation timer for this slot
	if (Slot.AnimTimerHandle.IsValid())
	{
		Slot.SubtitleBorder->UnRegisterActiveTimer(Slot.AnimTimerHandle.ToSharedRef());
		Slot.AnimTimerHandle.Reset();
	}

	Slot.AnimTimerHandle = Slot.SubtitleBorder->RegisterActiveTimer(
		0.0f,
		FWidgetActiveTimerDelegate::CreateWeakLambda(this,
			[this, SlotID](double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType
			{
				return TickSlotAnimation(SlotID, InCurrentTime, InDeltaTime);
			}
		)
	);
}

void USubtitleSubsystem::ApplySlotAnimationAlpha(FSubtitleSlot& Slot, float EasedAlpha)
{
	if (!Slot.SubtitleBorder.IsValid()) { return; }

	// Apply opacity to every visible component simultaneously
	auto SetOpacityAll = [&](float O)
	{
		Slot.SubtitleBorder->SetRenderOpacity(O);
		if (Slot.SpeakerTextOverlay.IsValid()) Slot.SpeakerTextOverlay->SetRenderOpacity(O);
		if (Slot.SeparatorBox.IsValid())       Slot.SeparatorBox->SetRenderOpacity(O);
	};

	// Apply the same render transform to every visible component
	auto SetTransformAll = [&](const FSlateRenderTransform& T)
	{
		Slot.SubtitleBorder->SetRenderTransform(T);
		if (Slot.SpeakerTextOverlay.IsValid()) Slot.SpeakerTextOverlay->SetRenderTransform(T);
		if (Slot.SeparatorBox.IsValid())       Slot.SeparatorBox->SetRenderTransform(T);
	};

	// For scale effects, apply via EntryVBox so the whole group scales from one pivot.
	// EntryVBox RenderTransform = Scale(pivot=center) combined with ScreenOffset translation.
	auto SetScaleAll = [&](const FScale2D& Scale)
	{
		if (Slot.EntryVBox.IsValid())
		{
			Slot.EntryVBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Slot.EntryVBox->SetRenderTransform(FSlateRenderTransform(Scale, Slot.Appearance.ScreenOffset));
		}
	};

	switch (Slot.AnimType)
	{
	case ESubtitleEntranceType::FadeIn:
		SetOpacityAll(EasedAlpha);
		break;
	case ESubtitleEntranceType::SlideLeft:
		SetTransformAll(FSlateRenderTransform(FVector2D(-Slot.SlideOffsetX * (1.f - EasedAlpha), 0.f)));
		break;
	case ESubtitleEntranceType::SlideRight:
		SetTransformAll(FSlateRenderTransform(FVector2D(Slot.SlideOffsetX * (1.f - EasedAlpha), 0.f)));
		break;
	case ESubtitleEntranceType::SlideTop:
		SetTransformAll(FSlateRenderTransform(FVector2D(0.f, -Slot.SlideOffsetY * (1.f - EasedAlpha))));
		break;
	case ESubtitleEntranceType::SlideBottom:
		SetTransformAll(FSlateRenderTransform(FVector2D(0.f, Slot.SlideOffsetY * (1.f - EasedAlpha))));
		break;
	case ESubtitleEntranceType::ScaleVertical:
		SetScaleAll(FScale2D(1.0f, EasedAlpha));
		break;
	case ESubtitleEntranceType::ScaleUp:
		SetScaleAll(FScale2D(EasedAlpha, EasedAlpha));
		break;
	default:
		break;
	}
}

EActiveTimerReturnType USubtitleSubsystem::TickSlotAnimation(uint32 SlotID, double InCurrentTime, float InDeltaTime)
{
	TSharedPtr<FSubtitleSlot>* Found = ActiveSlots.Find(SlotID);
	if (!Found || !Found->IsValid()) { return EActiveTimerReturnType::Stop; }

	FSubtitleSlot& Slot = **Found;

	if (!Slot.bAnimating || !Slot.SubtitleBorder.IsValid())
	{
		return EActiveTimerReturnType::Stop;
	}

	Slot.AnimElapsed += InDeltaTime;
	const float Alpha = FMath::Clamp(Slot.AnimElapsed / Slot.AnimDuration, 0.0f, 1.0f);
	const float DirectionalAlpha = Slot.bExitAnim ? (1.0f - Alpha) : Alpha;

	float EasedAlpha;
	EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, DirectionalAlpha, 2.0f);

	ApplySlotAnimationAlpha(Slot, EasedAlpha);

	if (Alpha >= 1.0f)
	{
		// Exit animation: remove slot BEFORE touching any slot state (slot is destroyed by RemoveSlotWidget)
		if (Slot.bExitAnim)
		{
			RemoveSlotWidget(SlotID);
			return EActiveTimerReturnType::Stop;
		}
		// Entrance animation complete: restore clean render state
		Slot.bAnimating = false;
		Slot.SubtitleBorder->SetRenderOpacity(1.0f);
		Slot.SubtitleBorder->SetRenderTransform(FSlateRenderTransform());
		if (Slot.SpeakerTextOverlay.IsValid()) { Slot.SpeakerTextOverlay->SetRenderOpacity(1.0f); Slot.SpeakerTextOverlay->SetRenderTransform(FSlateRenderTransform()); }
		if (Slot.SeparatorBox.IsValid())       { Slot.SeparatorBox->SetRenderOpacity(1.0f);       Slot.SeparatorBox->SetRenderTransform(FSlateRenderTransform()); }
		if (Slot.EntryVBox.IsValid())        { Slot.EntryVBox->SetRenderTransform(FSlateRenderTransform(Slot.Appearance.ScreenOffset)); }
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

// ---------------------------------------------------------------------------
// StartTremble / StopTremble / TickTremble — continuous shake effect
// ---------------------------------------------------------------------------

void USubtitleSubsystem::StartTremble(FSubtitleSlot& Slot, uint32 SlotID)
{
	StopTremble(Slot);

	if (!Slot.Appearance.bTremble || !Slot.SubtitleBorder.IsValid())
	{
		return;
	}

	Slot.TrembleStartTime = FPlatformTime::Seconds();

	Slot.TrembleTimerHandle = Slot.SubtitleBorder->RegisterActiveTimer(
		0.0f,
		FWidgetActiveTimerDelegate::CreateWeakLambda(this,
			[this, SlotID](double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType
			{
				return TickTremble(SlotID, InCurrentTime, InDeltaTime);
			}
		)
	);
}

void USubtitleSubsystem::StopTremble(FSubtitleSlot& Slot)
{
	if (Slot.TrembleTimerHandle.IsValid() && Slot.SubtitleBorder.IsValid())
	{
		Slot.SubtitleBorder->UnRegisterActiveTimer(Slot.TrembleTimerHandle.ToSharedRef());
		Slot.TrembleTimerHandle.Reset();
	}
}

EActiveTimerReturnType USubtitleSubsystem::TickTremble(uint32 SlotID, double InCurrentTime, float InDeltaTime)
{
	TSharedPtr<FSubtitleSlot>* Found = ActiveSlots.Find(SlotID);
	if (!Found || !Found->IsValid()) { return EActiveTimerReturnType::Stop; }

	FSubtitleSlot& Slot = **Found;

	if (!Slot.Appearance.bTremble || !Slot.SubtitleBorder.IsValid())
	{
		return EActiveTimerReturnType::Stop;
	}

	const double Elapsed = FPlatformTime::Seconds() - Slot.TrembleStartTime;
	const float Speed = Slot.Appearance.TrembleSpeed;
	const float Intensity = Slot.Appearance.TrembleIntensity;

	// Use sin/cos with slightly different frequencies for organic-feeling shake
	const float OffsetX = FMath::Sin(static_cast<float>(Elapsed) * Speed * UE_TWO_PI) * Intensity;
	const float OffsetY = FMath::Cos(static_cast<float>(Elapsed) * Speed * UE_TWO_PI * 1.3f) * Intensity;

	const FVector2D TrembleOffset(OffsetX, OffsetY);

	// During Slide entrance/exit, add tremble on top of the current slide offset.
	// During Scale or Fade animations (or no animation), just apply tremble alone.
	FVector2D BaseOffset = FVector2D::ZeroVector;
	if (Slot.bAnimating)
	{
		// Reconstruct the current slide offset from animation state (same easing as TickSlotAnimation)
		const float Alpha = FMath::Clamp(Slot.AnimElapsed / Slot.AnimDuration, 0.0f, 1.0f);
		const float DirectionalAlpha = Slot.bExitAnim ? (1.0f - Alpha) : Alpha;
		const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, DirectionalAlpha, 2.0f);
		switch (Slot.AnimType)
		{
		case ESubtitleEntranceType::SlideLeft:   BaseOffset.X = -Slot.SlideOffsetX * (1.f - EasedAlpha); break;
		case ESubtitleEntranceType::SlideRight:  BaseOffset.X =  Slot.SlideOffsetX * (1.f - EasedAlpha); break;
		case ESubtitleEntranceType::SlideTop:    BaseOffset.Y = -Slot.SlideOffsetY * (1.f - EasedAlpha); break;
		case ESubtitleEntranceType::SlideBottom: BaseOffset.Y =  Slot.SlideOffsetY * (1.f - EasedAlpha); break;
		default: break;
		}
	}

	const FSlateRenderTransform CombinedTransform(BaseOffset + TrembleOffset);

	// Apply to individual components (not EntryVBox) to avoid conflicting with Scale animations
	Slot.SubtitleBorder->SetRenderTransform(CombinedTransform);
	if (Slot.SpeakerTextOverlay.IsValid()) { Slot.SpeakerTextOverlay->SetRenderTransform(CombinedTransform); }
	if (Slot.SeparatorBox.IsValid())       { Slot.SeparatorBox->SetRenderTransform(CombinedTransform); }

	return EActiveTimerReturnType::Continue;
}

// ---------------------------------------------------------------------------
// GetHostViewportSize / GetSubtitleDPIScale
// ---------------------------------------------------------------------------

FIntPoint USubtitleSubsystem::GetHostViewportSize() const
{
#if WITH_EDITOR
	if (bIsEditorViewport)
	{
		const TSharedPtr<IAssetViewport> EditorViewport = HostEditorViewport.Pin();
		if (EditorViewport.IsValid() && EditorViewport->GetActiveViewport())
		{
			return EditorViewport->GetActiveViewport()->GetSizeXY();
		}
		return FIntPoint::ZeroValue;
	}
#endif

	if (UGameViewportClient* GameViewport = HostGameViewport.Get())
	{
		FVector2D VPSize;
		GameViewport->GetViewportSize(VPSize);
		return FIntPoint(FMath::RoundToInt(VPSize.X), FMath::RoundToInt(VPSize.Y));
	}
	return FIntPoint::ZeroValue;
}

float USubtitleSubsystem::GetSubtitleDPIScale() const
{
	// Game / PIE: viewport content already sits inside the engine's game layer DPI scaler
	// (same scale UMG uses), so applying the DPI curve here again would square it.
	if (!bIsEditorViewport)
	{
		return 1.0f;
	}

	// Editor viewport: nothing applies the DPI curve, so do it here like the game layer does:
	// curve value for the viewport's pixel size, divided by the scale Slate already applies
	// above us (OS display scaling).
	const FIntPoint VPSize = GetHostViewportSize();
	const UUserInterfaceSettings* UISettings = GetDefault<UUserInterfaceSettings>();
	if (VPSize.X <= 0 || VPSize.Y <= 0 || !UISettings)
	{
		return 1.0f;
	}

	float Scale = UISettings->GetDPIScaleBasedOnSize(VPSize);
	if (DPIScalerWidget.IsValid())
	{
		const float ParentScale = DPIScalerWidget->GetTickSpaceGeometry().GetAccumulatedLayoutTransform().GetScale();
		if (ParentScale > KINDA_SMALL_NUMBER)
		{
			Scale /= ParentScale;
		}
	}
	return Scale;
}
