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
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Modules/ModuleManager.h"
#include "Slate/SceneViewport.h"
#endif

void USubtitleSubsystem::EnsureSlateWidgets()
{
	if (WidgetOverlay.IsValid())
	{
		return;
	}

	// --- Speaker name ---
	SpeakerNameTextBlock = SNew(STextBlock)
		.Justification(ETextJustify::Center)
		.Visibility(EVisibility::Collapsed);

	// --- Separator line / image ---
	SeparatorLineBorder = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::White)
		.Padding(0);

	SeparatorBox = SNew(SBox)
		.HeightOverride(1.0f)
		.Visibility(EVisibility::Collapsed)
		[
			SeparatorLineBorder.ToSharedRef()
		];

	// --- Subtitle text ---
	SubtitleTextBlock = SNew(STextBlock)
		.AutoWrapText(true)
		.Justification(ETextJustify::Center);

	TypewriterSizerBox = SNew(SBox)
		[
			SubtitleTextBlock.ToSharedRef()
		];

	SubtitleBorder = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.Padding(FMargin(12.f, 6.f))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			TypewriterSizerBox.ToSharedRef()
		];

	// MessageWindowBox: optional fixed-height wrapper (height set in ApplyAppearance)
	MessageWindowBox = SNew(SBox)
		[
			SubtitleBorder.ToSharedRef()
		];

	// --- Vertical layout: Speaker → Separator → Subtitle ---
	ContentVerticalBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0, 0, 0, 2)
		[
			SpeakerNameTextBlock.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(20, 2, 20, 4)
		[
			SeparatorBox.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MessageWindowBox.ToSharedRef()
		];

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
	if (bIsEditorViewport && FModuleManager::Get().IsModuleLoaded("LevelEditor"))
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

void USubtitleSubsystem::ApplySpeakerAndSeparator(const FSubtitleAppearance& InAppearance, const FText& InSpeakerName)
{
	const bool bHasSpeaker = !InSpeakerName.IsEmptyOrWhitespace();

	// --- Speaker name ---
	if (SpeakerNameTextBlock.IsValid())
	{
		if (bHasSpeaker)
		{
			SpeakerNameTextBlock->SetText(InSpeakerName);
			SpeakerNameTextBlock->SetColorAndOpacity(FSlateColor(InAppearance.SpeakerNameColor));

			// Reuse same font family as subtitle, with speaker-specific size
			UObject* LoadedFont = InAppearance.FontAsset.LoadSynchronous();
			FSlateFontInfo SpeakerFont = LoadedFont
				? FSlateFontInfo(LoadedFont, InAppearance.SpeakerNameFontSize)
				: FCoreStyle::GetDefaultFontStyle("Bold", InAppearance.SpeakerNameFontSize);
			SpeakerNameTextBlock->SetFont(SpeakerFont);
			SpeakerNameTextBlock->SetVisibility(EVisibility::SelfHitTestInvisible);
		}
		else
		{
			SpeakerNameTextBlock->SetVisibility(EVisibility::Collapsed);
		}
	}

	// --- Separator line ---
	if (SeparatorBox.IsValid() && SeparatorLineBorder.IsValid())
	{
		if (bHasSpeaker && InAppearance.bShowSeparatorLine)
		{
			if (InAppearance.bUseLineImage)
			{
				// Custom image
				UTexture2D* Tex = InAppearance.LineImage.LoadSynchronous();
				if (Tex)
				{
					CustomSeparatorBrush = FSlateBrush();
					CustomSeparatorBrush.SetResourceObject(Tex);
					CustomSeparatorBrush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
					CustomSeparatorBrush.DrawAs = ESlateBrushDrawType::Image;
					SeparatorLineBorder->SetBorderImage(&CustomSeparatorBrush);
					SeparatorBox->SetHeightOverride(FOptionalSize()); // auto from image
				}
			}
			else
			{
				// Default generated line
				SeparatorLineBorder->SetBorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"));
				SeparatorLineBorder->SetBorderBackgroundColor(InAppearance.SeparatorLineColor);
				SeparatorBox->SetHeightOverride(InAppearance.SeparatorLineThickness);
			}
			SeparatorBox->SetVisibility(EVisibility::SelfHitTestInvisible);
		}
		else
		{
			SeparatorBox->SetVisibility(EVisibility::Collapsed);
		}
	}
}

FString USubtitleSubsystem::WrapTextByCharLimit(const FString& InText, int32 MaxCharsPerLine)
{
	if (MaxCharsPerLine <= 0)
	{
		return InText;
	}

	FString Result;
	// Process each existing line independently
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

		// Wrap long lines
		int32 Pos = 0;
		while (Pos < Line.Len())
		{
			if (Pos > 0)
			{
				Result += TEXT("\n");
			}

			const int32 Remaining = Line.Len() - Pos;
			if (Remaining <= MaxCharsPerLine)
			{
				Result += Line.Mid(Pos);
				break;
			}

			// Take MaxCharsPerLine characters
			Result += Line.Mid(Pos, MaxCharsPerLine);
			Pos += MaxCharsPerLine;
		}
	}

	return Result;
}

void USubtitleSubsystem::NotifySubtitleStarted(const FText& InSubtitleText, FLinearColor InBarColor, const FSubtitleAppearance& InAppearance, const FText& InSpeakerName)
{
	bIsSubtitleActive = true;
	CurrentSubtitleText = InSubtitleText;
	CurrentSpeakerName = InSpeakerName;
	CurrentAppearance = InAppearance;

	// Reset typewriter state for each new subtitle
	bTypewriterActive    = false;
	TypewriterFullWidth  = 0.f;
	TypewriterFullHeight = 0.f;
	CachedTypewriterSound = nullptr;
	LastSoundCharIndex   = -1;
	LastSoundPlayTime    = 0.0;
	CurrentPageIndex     = 0;
	TypewriterPages.Empty();
	TypewriterPageCharStarts.Empty();
	if (TypewriterSizerBox.IsValid())
	{
		TypewriterSizerBox->SetWidthOverride(FOptionalSize());
		TypewriterSizerBox->SetHeightOverride(FOptionalSize());
	}
	if (SubtitleBorder.IsValid())
	{
		SubtitleBorder->SetHAlign(HAlign_Fill);
	}

	EnsureSlateWidgets();
	AddToViewport();

	ApplyAppearance(CurrentAppearance);
	ApplySpeakerAndSeparator(CurrentAppearance, InSpeakerName);
	if (SubtitleTextBlock.IsValid())
	{
		SubtitleTextBlock->SetText(InSubtitleText);
	}
	WidgetOverlay->SetVisibility(EVisibility::SelfHitTestInvisible);
	StartAnimation(InAppearance.EntranceType, InAppearance.EntranceDuration, false);

	OnSubtitleStarted.Broadcast(InSubtitleText, InBarColor);
}

void USubtitleSubsystem::ShowMessage(const FText& Text, float Duration, ESubtitleEntranceType Animation, float AnimationDuration)
{
	FSubtitleAppearance Appearance;
	// ShowMessage convenience default: larger than FSubtitleAppearance default (24)
	Appearance.FontSize = 50;
	Appearance.TextColor = FLinearColor::White;
	Appearance.BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);
	Appearance.VerticalPosition = ESubtitleVerticalPosition::Bottom;
	Appearance.ScreenPadding = FMargin(40.0f, 20.0f);
	Appearance.EntranceType = Animation;
	Appearance.EntranceDuration = AnimationDuration;
	Appearance.bOverrideExitAnimation = false;

	ShowMessageEx(Text, Duration, Appearance);
}

void USubtitleSubsystem::ShowMessageEx(const FText& Text, float Duration, const FSubtitleAppearance& Appearance)
{
	// Cancel any previous auto-hide timer
	if (AutoHideTimerHandle.IsValid() && SubtitleBorder.IsValid())
	{
		SubtitleBorder->UnRegisterActiveTimer(AutoHideTimerHandle.ToSharedRef());
		AutoHideTimerHandle.Reset();
	}

	bIsShowMessageActive = true;

	// Reuse existing display pipeline
	NotifySubtitleStarted(Text, FLinearColor::White, Appearance);

	// Duration <= 0 means persistent display (until HideMessage is called)
	if (Duration > 0.0f)
	{
		// Schedule real-time auto-hide (entrance animation + visible duration)
		AutoHideRemaining = Appearance.EntranceDuration + Duration;

		EnsureSlateWidgets();
		if (SubtitleBorder.IsValid())
		{
			AutoHideTimerHandle = SubtitleBorder->RegisterActiveTimer(
				0.0f,
				FWidgetActiveTimerDelegate::CreateUObject(this, &USubtitleSubsystem::TickAutoHide)
			);
		}
	}
}

void USubtitleSubsystem::HideMessage()
{
	if (!bIsShowMessageActive)
	{
		return;
	}

	bIsShowMessageActive = false;

	if (AutoHideTimerHandle.IsValid() && SubtitleBorder.IsValid())
	{
		SubtitleBorder->UnRegisterActiveTimer(AutoHideTimerHandle.ToSharedRef());
		AutoHideTimerHandle.Reset();
	}

	NotifySubtitleEnded();
}

EActiveTimerReturnType USubtitleSubsystem::TickAutoHide(double InCurrentTime, float InDeltaTime)
{
	if (!bIsShowMessageActive)
	{
		return EActiveTimerReturnType::Stop;
	}

	AutoHideRemaining -= InDeltaTime;
	if (AutoHideRemaining <= 0.0f)
	{
		bIsShowMessageActive = false;
		NotifySubtitleEnded();
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

void USubtitleSubsystem::UpdateTypewriterProgress(int32 VisibleCharCount)
{
	if (!SubtitleTextBlock.IsValid() || !bIsSubtitleActive) { return; }

	const FString FullStr    = CurrentSubtitleText.ToString();
	const int32   TotalChars = FullStr.Len();
	const int32   ShowChars  = FMath::Clamp(VisibleCharCount, 0, TotalChars);

	// --- First call: set up fixed-width centering + paging ---
	if (!bTypewriterActive)
	{
		bTypewriterActive = true;
		LastSoundCharIndex = -1;
		LastSoundPlayTime  = 0.0;
		CurrentPageIndex   = 0;

		// Load and cache typewriter sound
		CachedTypewriterSound = CurrentAppearance.TypewriterSound.LoadSynchronous();

		// Split text into lines
		TArray<FString> AllLines;
		FullStr.ParseIntoArray(AllLines, TEXT("\n"), /*bCullEmpty=*/false);

		// Build pages (BotW-style paging)
		const int32 MaxLines = CurrentAppearance.MaxLinesPerPage;
		TypewriterPages.Empty();
		TypewriterPageCharStarts.Empty();

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
				TypewriterPageCharStarts.Add(CharOffset);
				TypewriterPages.Add(PageText);
				CharOffset += PageText.Len();
				// +1 for the \n separating this page from the next in FullStr
				if (i + MaxLines < AllLines.Num()) { CharOffset += 1; }
			}
		}
		else
		{
			TypewriterPages.Add(FullStr);
			TypewriterPageCharStarts.Add(0);
		}

		// Measure full text width (across all lines) for SBox anchoring
		if (FSlateApplication::IsInitialized())
		{
			const TSharedRef<FSlateFontMeasure> FM =
				FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

			for (const FString& Line : AllLines)
			{
				const float LineWidth = FM->Measure(FText::FromString(Line), CurrentFontInfo).X;
				TypewriterFullWidth = FMath::Max(TypewriterFullWidth, LineWidth);
			}

			// Height: based on MaxLinesPerPage (or all lines if no paging)
			const int32 DisplayLineCount = (MaxLines > 0)
				? FMath::Min(MaxLines, AllLines.Num())
				: AllLines.Num();
			const float LineHeight = FM->GetMaxCharacterHeight(CurrentFontInfo, 1.0f);
			TypewriterFullHeight = LineHeight * static_cast<float>(DisplayLineCount);
		}

		if (TypewriterSizerBox.IsValid() && TypewriterFullWidth > 0.f)
		{
			TypewriterSizerBox->SetWidthOverride(TypewriterFullWidth);
			if (TypewriterFullHeight > 0.f)
			{
				TypewriterSizerBox->SetHeightOverride(TypewriterFullHeight);
			}
			SubtitleBorder->SetHAlign(HAlign_Center);
		}

		SubtitleTextBlock->SetJustification(ETextJustify::Left);
	}

	// --- Determine current page ---
	int32 PageIdx = 0;
	for (int32 i = TypewriterPages.Num() - 1; i >= 0; --i)
	{
		if (ShowChars >= TypewriterPageCharStarts[i])
		{
			PageIdx = i;
			break;
		}
	}

	// Page transition: clear display briefly
	if (PageIdx != CurrentPageIndex)
	{
		CurrentPageIndex = PageIdx;
	}

	// Local char count within current page
	const FString& PageText = TypewriterPages[PageIdx];
	const int32 PageLocalChars = ShowChars - TypewriterPageCharStarts[PageIdx];
	const int32 ClampedLocal = FMath::Clamp(PageLocalChars, 0, PageText.Len());

	// --- Play typewriter sound ---
	PlayTypewriterSound(ShowChars);

	// --- Update displayed text ---
	SubtitleTextBlock->SetText(FText::FromString(PageText.Left(ClampedLocal)));

	// --- Fully revealed: keep layout as-is to prevent position shift ---
	if (ShowChars >= TotalChars)
	{
		CachedTypewriterSound = nullptr;
		// Keep bTypewriterActive, SBox overrides, and HAlign unchanged
		// so the text stays in the exact same position.
		// They will be reset in NotifySubtitleStarted / NotifySubtitleEnded.
	}
}

void USubtitleSubsystem::PlayTypewriterSound(int32 CurrentCharIndex)
{
	if (!CachedTypewriterSound || CurrentCharIndex <= LastSoundCharIndex) { return; }

	const double Now = FPlatformTime::Seconds();
	const float MinInterval = CurrentAppearance.TypewriterSoundInterval;

	// Throttle: skip if too soon since last play
	if (LastSoundPlayTime > 0.0 && (Now - LastSoundPlayTime) < static_cast<double>(MinInterval))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		UGameplayStatics::PlaySound2D(World, CachedTypewriterSound);
	}

	LastSoundCharIndex = CurrentCharIndex;
	LastSoundPlayTime  = Now;
}

void USubtitleSubsystem::NotifySubtitleEnded()
{
	bIsSubtitleActive = false;
	CurrentSubtitleText = FText::GetEmpty();

	if (!WidgetOverlay.IsValid())
	{
		OnSubtitleEnded.Broadcast();
		return;
	}

	ESubtitleEntranceType ExitType = CurrentAppearance.GetEffectiveExitType();
	float ExitDuration = CurrentAppearance.GetEffectiveExitDuration();

	if (ExitType != ESubtitleEntranceType::None && ExitDuration > 0.0f)
	{
		StartAnimation(ExitType, ExitDuration, true);
	}
	else
	{
		WidgetOverlay->SetVisibility(EVisibility::Hidden);
	}

	OnSubtitleEnded.Broadcast();
}

bool USubtitleSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer();
}

void USubtitleSubsystem::Deinitialize()
{
	bAnimating = false;
	if (AnimTimerHandle.IsValid() && SubtitleBorder.IsValid())
	{
		SubtitleBorder->UnRegisterActiveTimer(AnimTimerHandle.ToSharedRef());
		AnimTimerHandle.Reset();
	}

	bIsShowMessageActive = false;
	if (AutoHideTimerHandle.IsValid() && SubtitleBorder.IsValid())
	{
		SubtitleBorder->UnRegisterActiveTimer(AutoHideTimerHandle.ToSharedRef());
		AutoHideTimerHandle.Reset();
	}

	RemoveFromViewport();
	DPIScalerWidget.Reset();
	WidgetOverlay.Reset();
	ContentVerticalBox.Reset();
	SpeakerNameTextBlock.Reset();
	SeparatorBox.Reset();
	SeparatorLineBorder.Reset();
	SubtitleTextBlock.Reset();
	TypewriterSizerBox.Reset();
	SubtitleBorder.Reset();
	MessageWindowBox.Reset();
	OverlaySlot = nullptr;
	bTypewriterActive    = false;
	TypewriterFullWidth  = 0.f;
	TypewriterFullHeight = 0.f;
	CachedTypewriterSound = nullptr;
	LastSoundCharIndex   = -1;
	LastSoundPlayTime    = 0.0;
	CurrentPageIndex     = 0;
	TypewriterPages.Empty();
	TypewriterPageCharStarts.Empty();

	Super::Deinitialize();
}

void USubtitleSubsystem::ApplyAppearance(const FSubtitleAppearance& InAppearance)
{
	if (OverlaySlot)
	{
		switch (InAppearance.VerticalPosition)
		{
		case ESubtitleVerticalPosition::Top:
			OverlaySlot->SetVerticalAlignment(VAlign_Top);
			break;
		case ESubtitleVerticalPosition::Center:
			OverlaySlot->SetVerticalAlignment(VAlign_Center);
			break;
		case ESubtitleVerticalPosition::Bottom:
		default:
			OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
			break;
		}

		// HAlign_Fill preserves AutoWrapText width; Left/Right shrink-wrap the content.
		switch (InAppearance.HorizontalPosition)
		{
		case ESubtitleHorizontalPosition::Left:
			OverlaySlot->SetHorizontalAlignment(HAlign_Left);
			break;
		case ESubtitleHorizontalPosition::Right:
			OverlaySlot->SetHorizontalAlignment(HAlign_Right);
			break;
		case ESubtitleHorizontalPosition::Center:
		default:
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			break;
		}

		OverlaySlot->SetPadding(InAppearance.ScreenPadding);
	}

	if (SubtitleBorder.IsValid())
	{
		SubtitleBorder->SetBorderBackgroundColor(InAppearance.BackgroundColor);
	}

	if (MessageWindowBox.IsValid())
	{
		if (InAppearance.MessageWindowHeight > 0.0f)
		{
			MessageWindowBox->SetHeightOverride(InAppearance.MessageWindowHeight);
		}
		else
		{
			MessageWindowBox->SetHeightOverride(FOptionalSize());
		}
	}

	if (SubtitleTextBlock.IsValid())
	{
		// Text alignment (skipped during typewriter reveal — UpdateTypewriterProgress handles it)
		if (!bTypewriterActive)
		{
			ETextJustify::Type Justify = ETextJustify::Center;
			switch (InAppearance.TextAlignment)
			{
			case ESubtitleTextAlignment::Left:  Justify = ETextJustify::Left;   break;
			case ESubtitleTextAlignment::Right: Justify = ETextJustify::Right;  break;
			default: break;
			}
			SubtitleTextBlock->SetJustification(Justify);
		}

		float EffectiveFontSize = static_cast<float>(InAppearance.FontSize);

		FSlateFontInfo FontInfo;
		UObject* LoadedFont = InAppearance.FontAsset.LoadSynchronous();
		if (LoadedFont)
		{
			FontInfo = FSlateFontInfo(LoadedFont, EffectiveFontSize);
		}
		else
		{
			FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", static_cast<int32>(EffectiveFontSize));
		}

		CurrentFontInfo = FontInfo; // cache for typewriter width measurement
		SubtitleTextBlock->SetFont(FontInfo);
		SubtitleTextBlock->SetColorAndOpacity(FSlateColor(InAppearance.TextColor));
	}
}

void USubtitleSubsystem::StartAnimation(ESubtitleEntranceType InType, float InDuration, bool bReverse)
{
	if (!SubtitleBorder.IsValid())
	{
		return;
	}

	// Reset any previous animation state
	SubtitleBorder->SetRenderOpacity(1.0f);
	SubtitleBorder->SetRenderTransform(FSlateRenderTransform());

	if (InType == ESubtitleEntranceType::None || InDuration <= 0.0f)
	{
		bAnimating = false;
		if (bReverse && WidgetOverlay.IsValid())
		{
			WidgetOverlay->SetVisibility(EVisibility::Hidden);
		}
		return;
	}

	AnimationType = InType;
	AnimDuration = InDuration;
	AnimElapsed = 0.0f;
	bAnimating = true;
	bExitAnimation = bReverse;

	// Compute viewport-relative slide offsets (in Slate units, accounting for DPI)
	SlideOffsetX = 2000.0f;
	SlideOffsetY = 1200.0f;
	if (GEngine && GEngine->GameViewport && !bIsEditorViewport)
	{
		FVector2D VPSize;
		GEngine->GameViewport->GetViewportSize(VPSize);
		float DPIScale = GetSubtitleDPIScale();
		if (DPIScale < 0.01f) DPIScale = 1.0f;
		SlideOffsetX = static_cast<float>(VPSize.X) / DPIScale;
		SlideOffsetY = static_cast<float>(VPSize.Y) / DPIScale;
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
				SlideOffsetX = static_cast<float>(EdSize.X) / Scale;
				SlideOffsetY = static_cast<float>(EdSize.Y) / Scale;
			}
		}
	}
#endif

	// Set initial alpha: entrance starts at 0, exit starts at 1
	float InitAlpha = bReverse ? 1.0f : 0.0f;
	ApplyAnimationAlpha(InitAlpha);

	// Register active timer on the border widget to drive animation
	if (AnimTimerHandle.IsValid())
	{
		SubtitleBorder->UnRegisterActiveTimer(AnimTimerHandle.ToSharedRef());
		AnimTimerHandle.Reset();
	}

	AnimTimerHandle = SubtitleBorder->RegisterActiveTimer(
		0.0f,
		FWidgetActiveTimerDelegate::CreateUObject(this, &USubtitleSubsystem::TickAnimation)
	);
}

void USubtitleSubsystem::ApplyAnimationAlpha(float EasedAlpha)
{
	if (!SubtitleBorder.IsValid())
	{
		return;
	}

	switch (AnimationType)
	{
	case ESubtitleEntranceType::FadeIn:
		SubtitleBorder->SetRenderOpacity(EasedAlpha);
		break;
	case ESubtitleEntranceType::SlideLeft:
		SubtitleBorder->SetRenderTransform(FSlateRenderTransform(FVector2D(-SlideOffsetX * (1.0f - EasedAlpha), 0.0)));
		break;
	case ESubtitleEntranceType::SlideRight:
		SubtitleBorder->SetRenderTransform(FSlateRenderTransform(FVector2D(SlideOffsetX * (1.0f - EasedAlpha), 0.0)));
		break;
	case ESubtitleEntranceType::SlideTop:
		SubtitleBorder->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0, -SlideOffsetY * (1.0f - EasedAlpha))));
		break;
	case ESubtitleEntranceType::SlideBottom:
		SubtitleBorder->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0, SlideOffsetY * (1.0f - EasedAlpha))));
		break;
	case ESubtitleEntranceType::ScaleVertical:
		SubtitleBorder->SetRenderTransformPivot(FVector2D(0.5, 0.5));
		SubtitleBorder->SetRenderTransform(FSlateRenderTransform(FScale2D(1.0f, EasedAlpha)));
		break;
	case ESubtitleEntranceType::ScaleUp:
		SubtitleBorder->SetRenderTransformPivot(FVector2D(0.5, 0.5));
		SubtitleBorder->SetRenderTransform(FSlateRenderTransform(FScale2D(EasedAlpha, EasedAlpha)));
		break;
	default:
		break;
	}
}

EActiveTimerReturnType USubtitleSubsystem::TickAnimation(double InCurrentTime, float InDeltaTime)
{
	if (!bAnimating || !SubtitleBorder.IsValid())
	{
		return EActiveTimerReturnType::Stop;
	}

	AnimElapsed += InDeltaTime;
	const float Alpha = FMath::Clamp(AnimElapsed / AnimDuration, 0.0f, 1.0f);

	// Entrance: 0->1, Exit: 1->0
	float DirectionalAlpha = bExitAnimation ? (1.0f - Alpha) : Alpha;

	float EasedAlpha;
	if (bExitAnimation)
	{
		EasedAlpha = FMath::InterpEaseIn(0.0f, 1.0f, DirectionalAlpha, 2.0f);
	}
	else
	{
		EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, DirectionalAlpha, 2.0f);
	}

	ApplyAnimationAlpha(EasedAlpha);

	if (Alpha >= 1.0f)
	{
		bAnimating = false;
		if (bExitAnimation && WidgetOverlay.IsValid())
		{
			WidgetOverlay->SetVisibility(EVisibility::Hidden);
		}
		SubtitleBorder->SetRenderOpacity(1.0f);
		SubtitleBorder->SetRenderTransform(FSlateRenderTransform());
		bExitAnimation = false;
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

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
