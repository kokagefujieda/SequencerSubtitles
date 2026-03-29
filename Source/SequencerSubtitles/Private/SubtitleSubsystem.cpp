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
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
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
	// Speaker name
	Slot.SpeakerTextBlock = SNew(STextBlock)
		.Justification(ETextJustify::Center)
		.Visibility(EVisibility::Collapsed);

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

	// Subtitle text
	Slot.SubtitleTextBlock = SNew(STextBlock)
		.AutoWrapText(true)
		.Justification(ETextJustify::Center);

	Slot.TypewriterSizerBox = SNew(SBox)
		[
			Slot.SubtitleTextBlock.ToSharedRef()
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
			Slot.SpeakerTextBlock.ToSharedRef()
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
	Slot.DragHandle->SetOnDragFinished(FOnSubtitleDragFinished::CreateLambda(
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

#if WITH_EDITOR
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
					bAddedToViewport = true;
					bIsEditorViewport = true;
					return;
				}
			}
		}
	}
#endif

	if (GEngine && GEngine->GameViewport && DPIScalerWidget.IsValid())
	{
		GEngine->GameViewport->AddViewportWidgetContent(DPIScalerWidget.ToSharedRef(), 100);
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
		// Try to remove from the editor viewport (module may already be unloaded during shutdown)
		if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
			if (LevelEditor.IsValid())
			{
				TSharedPtr<IAssetViewport> ActiveLevelViewport = LevelEditor->GetActiveViewportInterface();
				if (ActiveLevelViewport.IsValid() && DPIScalerWidget.IsValid())
				{
					ActiveLevelViewport->RemoveOverlayWidget(DPIScalerWidget.ToSharedRef());
				}
			}
		}
		// Always reset the flag whether or not removal succeeded (viewport may already be gone)
		bAddedToViewport = false;
		return;
	}
#endif

	if (GEngine && GEngine->GameViewport && DPIScalerWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(DPIScalerWidget.ToSharedRef());
	}
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
	Slot.TypewriterFullWidth = 0.f;
	Slot.TypewriterFullHeight= 0.f;
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
		if (Slot.TypewriterSizerBox.IsValid())
		{
			Slot.TypewriterSizerBox->SetWidthOverride(FOptionalSize());
			Slot.TypewriterSizerBox->SetHeightOverride(FOptionalSize());
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

	PreMeasureSlotText(Slot, InSubtitleText, InAppearance);

	WidgetOverlay->SetVisibility(EVisibility::SelfHitTestInvisible);
	StartSlotAnimation(Slot, SlotID, InAppearance.EntranceType, InAppearance.EntranceDuration, false);

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

	Slot.SubtitleTextBlock->SetText(FText::FromString(PageText.Left(ClampedLocal)));

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

	// Measure full width for SBox anchoring
	if (FSlateApplication::IsInitialized())
	{
		const TSharedRef<FSlateFontMeasure> FM =
			FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		for (const FString& Line : AllLines)
		{
			const float LineWidth = FM->Measure(FText::FromString(Line), Slot.FontInfo).X;
			Slot.TypewriterFullWidth = FMath::Max(Slot.TypewriterFullWidth, LineWidth);
		}

		const int32 DisplayLineCount = (MaxLines > 0)
			? FMath::Min(MaxLines, AllLines.Num())
			: AllLines.Num();
		const float LineHeight    = FM->GetMaxCharacterHeight(Slot.FontInfo, 1.0f);
		Slot.TypewriterFullHeight = LineHeight * static_cast<float>(DisplayLineCount);
	}

	if (Slot.TypewriterSizerBox.IsValid() && Slot.TypewriterFullWidth > 0.f)
	{
		Slot.TypewriterSizerBox->SetWidthOverride(Slot.TypewriterFullWidth);
		if (Slot.TypewriterFullHeight > 0.f)
		{
			Slot.TypewriterSizerBox->SetHeightOverride(Slot.TypewriterFullHeight);
		}
		Slot.SubtitleBorder->SetHAlign(HAlign_Center);
	}

	Slot.SubtitleTextBlock->SetJustification(ETextJustify::Left);
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
				FWidgetActiveTimerDelegate::CreateLambda(
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

	/** Apply font, size, color and justification to a slot's SubtitleTextBlock. */
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
		}

		UObject* LoadedFont = InAppearance.FontAsset.LoadSynchronous();
		const FSlateFontInfo FontInfo = LoadedFont
			? FSlateFontInfo(LoadedFont, static_cast<float>(InAppearance.FontSize))
			: FCoreStyle::GetDefaultFontStyle("Regular", InAppearance.FontSize);

		Slot.FontInfo = FontInfo;
		Slot.SubtitleTextBlock->SetFont(FontInfo);
		Slot.SubtitleTextBlock->SetColorAndOpacity(FSlateColor(InAppearance.TextColor));
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

	if (Slot.MessageWindowBox.IsValid())
	{
		if (InAppearance.MessageWindowHeight > 0.0f)
			Slot.MessageWindowBox->SetHeightOverride(InAppearance.MessageWindowHeight);
		else
			Slot.MessageWindowBox->SetHeightOverride(FOptionalSize());
	}

	ApplyTextStyling(Slot, InAppearance);
}

// ---------------------------------------------------------------------------
// ApplySpeakerAndSeparatorToSlot — static helpers
// ---------------------------------------------------------------------------

namespace
{
	/** Show/hide and style the speaker name text block. */
	static void ApplySpeakerName(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance,
		const FText& InSpeakerName, bool bHasSpeaker)
	{
		if (!Slot.SpeakerTextBlock.IsValid()) { return; }

		if (!bHasSpeaker)
		{
			Slot.SpeakerTextBlock->SetVisibility(EVisibility::Collapsed);
			return;
		}

		Slot.SpeakerTextBlock->SetText(InSpeakerName);
		Slot.SpeakerTextBlock->SetColorAndOpacity(FSlateColor(InAppearance.SpeakerNameColor));

		UObject* LoadedFont = InAppearance.FontAsset.LoadSynchronous();
		const FSlateFontInfo SpeakerFont = LoadedFont
			? FSlateFontInfo(LoadedFont, InAppearance.SpeakerNameFontSize)
			: FCoreStyle::GetDefaultFontStyle("Bold", InAppearance.SpeakerNameFontSize);
		Slot.SpeakerTextBlock->SetFont(SpeakerFont);
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
// PreMeasureSlotText — prevents layout flicker on first frame
// ---------------------------------------------------------------------------

void USubtitleSubsystem::PreMeasureSlotText(FSubtitleSlot& Slot, const FText& InSubtitleText,
	const FSubtitleAppearance& InAppearance)
{
	if (!Slot.TypewriterSizerBox.IsValid() || !Slot.SubtitleBorder.IsValid()) { return; }
	if (!FSlateApplication::IsInitialized()) { return; }

	TArray<FString> Lines;
	InSubtitleText.ToString().ParseIntoArray(Lines, TEXT("\n"), /*bCullEmpty=*/false);
	if (Lines.Num() <= 1) { return; }

	const TSharedRef<FSlateFontMeasure> FM =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const float LineHeight = FM->GetMaxCharacterHeight(Slot.FontInfo, 1.0f);
	Slot.TypewriterSizerBox->SetHeightOverride(LineHeight * static_cast<float>(Lines.Num()));

	if (InAppearance.TextAlignment != ESubtitleTextAlignment::Left)
	{
		float MaxLineWidth = 0.f;
		for (const FString& Line : Lines)
		{
			MaxLineWidth = FMath::Max(MaxLineWidth, FM->Measure(FText::FromString(Line), Slot.FontInfo).X);
		}
		if (MaxLineWidth > 0.f)
		{
			Slot.TypewriterSizerBox->SetWidthOverride(MaxLineWidth);
			Slot.SubtitleBorder->SetHAlign(
				InAppearance.TextAlignment == ESubtitleTextAlignment::Right ? HAlign_Right : HAlign_Center);
		}
	}
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
	if (Slot.SpeakerTextBlock.IsValid()) { Slot.SpeakerTextBlock->SetRenderOpacity(1.0f); Slot.SpeakerTextBlock->SetRenderTransform(FSlateRenderTransform()); }
	if (Slot.SeparatorBox.IsValid())     { Slot.SeparatorBox->SetRenderOpacity(1.0f);     Slot.SeparatorBox->SetRenderTransform(FSlateRenderTransform()); }

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

	// Compute viewport-relative slide offsets
	Slot.SlideOffsetX = 2000.f;
	Slot.SlideOffsetY = 1200.f;
	if (GEngine && GEngine->GameViewport && !bIsEditorViewport)
	{
		FVector2D VPSize;
		GEngine->GameViewport->GetViewportSize(VPSize);
		float Scale = GetSubtitleDPIScale();
		if (Scale < 0.01f) Scale = 1.0f;
		Slot.SlideOffsetX = static_cast<float>(VPSize.X) / Scale;
		Slot.SlideOffsetY = static_cast<float>(VPSize.Y) / Scale;
	}
#if WITH_EDITOR
	else if (bIsEditorViewport && FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
		if (LevelEditor.IsValid())
		{
			TSharedPtr<IAssetViewport> ActiveLevelViewport = LevelEditor->GetActiveViewportInterface();
			if (ActiveLevelViewport.IsValid() && ActiveLevelViewport->GetActiveViewport())
			{
				FIntPoint EdSize = ActiveLevelViewport->GetActiveViewport()->GetSizeXY();
				float Scale = GetSubtitleDPIScale();
				if (Scale < 0.01f) Scale = 1.0f;
				Slot.SlideOffsetX = static_cast<float>(EdSize.X) / Scale;
				Slot.SlideOffsetY = static_cast<float>(EdSize.Y) / Scale;
			}
		}
	}
#endif

	ApplySlotAnimationAlpha(Slot, bReverse ? 1.0f : 0.0f);

	// Stop any previous animation timer for this slot
	if (Slot.AnimTimerHandle.IsValid())
	{
		Slot.SubtitleBorder->UnRegisterActiveTimer(Slot.AnimTimerHandle.ToSharedRef());
		Slot.AnimTimerHandle.Reset();
	}

	Slot.AnimTimerHandle = Slot.SubtitleBorder->RegisterActiveTimer(
		0.0f,
		FWidgetActiveTimerDelegate::CreateLambda(
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
		if (Slot.SpeakerTextBlock.IsValid()) Slot.SpeakerTextBlock->SetRenderOpacity(O);
		if (Slot.SeparatorBox.IsValid())     Slot.SeparatorBox->SetRenderOpacity(O);
	};

	// Apply the same render transform to every visible component
	auto SetTransformAll = [&](const FSlateRenderTransform& T)
	{
		Slot.SubtitleBorder->SetRenderTransform(T);
		if (Slot.SpeakerTextBlock.IsValid()) Slot.SpeakerTextBlock->SetRenderTransform(T);
		if (Slot.SeparatorBox.IsValid())     Slot.SeparatorBox->SetRenderTransform(T);
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
		if (Slot.SpeakerTextBlock.IsValid()) { Slot.SpeakerTextBlock->SetRenderOpacity(1.0f); Slot.SpeakerTextBlock->SetRenderTransform(FSlateRenderTransform()); }
		if (Slot.SeparatorBox.IsValid())     { Slot.SeparatorBox->SetRenderOpacity(1.0f);     Slot.SeparatorBox->SetRenderTransform(FSlateRenderTransform()); }
		if (Slot.EntryVBox.IsValid())        { Slot.EntryVBox->SetRenderTransform(FSlateRenderTransform(Slot.Appearance.ScreenOffset)); }
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

// ---------------------------------------------------------------------------
// GetSubtitleDPIScale
// ---------------------------------------------------------------------------

float USubtitleSubsystem::GetSubtitleDPIScale() const
{
#if WITH_EDITOR
	if (bIsEditorViewport && FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
		if (LevelEditor.IsValid())
		{
			TSharedPtr<IAssetViewport> ActiveLevelViewport = LevelEditor->GetActiveViewportInterface();
			if (ActiveLevelViewport.IsValid() && ActiveLevelViewport->GetActiveViewport())
			{
				FIntPoint EdSize = ActiveLevelViewport->GetActiveViewport()->GetSizeXY();
				if (EdSize.X > 0 && EdSize.Y > 0)
				{
					if (const UUserInterfaceSettings* UISettings = GetDefault<UUserInterfaceSettings>())
					{
						return UISettings->GetDPIScaleBasedOnSize(EdSize);
					}
				}
			}
		}
	}
#endif

	if (GEngine && GEngine->GameViewport)
	{
		FVector2D VPSize;
		GEngine->GameViewport->GetViewportSize(VPSize);
		if (VPSize.X > 0 && VPSize.Y > 0)
		{
			if (const UUserInterfaceSettings* UISettings = GetDefault<UUserInterfaceSettings>())
			{
				return UISettings->GetDPIScaleBasedOnSize(FIntPoint((int32)VPSize.X, (int32)VPSize.Y));
			}
		}
	}

	return 1.0f;
}
