// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SubtitleSettings.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "SubtitleSubsystem.generated.h"

class STextBlock;
class SBorder;
class SBox;
class USoundBase;
class SSubtitleSeparatorLine;
class UMovieSceneSeqSubtitleSection;
#if WITH_EDITOR
class SSubtitleDragHandle;
#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnSubtitleStarted,
	const FText&, SubtitleText,
	FLinearColor, BarColor,
	const FText&, SpeakerName
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSubtitleEnded);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubtitlePageAdvanced, int32, NewPageIndex);

/**
 * Per-slot widget set and state for one simultaneously active subtitle.
 * One instance is created per active sequencer section (keyed by section UniqueID).
 */
struct FSubtitleSlot
{
	// --- Slate widgets for this entry ---
	TSharedPtr<SVerticalBox>           EntryVBox;
	TSharedPtr<STextBlock>             SpeakerTextBlock;
	TSharedPtr<SBox>                   SeparatorBox;
	TSharedPtr<SOverlay>               SeparatorOverlay;
	TSharedPtr<SSubtitleSeparatorLine> SeparatorLineWidget;
	TSharedPtr<SBorder>                SeparatorLineBorder;
	TSharedPtr<STextBlock>             SubtitleTextBlock;
	TSharedPtr<SBorder>                SubtitleBorder;
	TSharedPtr<SBox>                   MessageWindowBox;
	TSharedPtr<SBox>                   TypewriterSizerBox;
	FSlateBrush                        CustomSeparatorBrush;
	FSlateBrush                        WindowBrush;
	SVerticalBox::FSlot*               SpeakerNameSlot = nullptr;
	SVerticalBox::FSlot*               SeparatorSlot   = nullptr;

	// --- Subtitle state ---
	FSubtitleAppearance                Appearance;
	FText                              Text;
	FText                              SpeakerName;
	FLinearColor                       BarColor;
	FSlateFontInfo                     FontInfo;

	// --- Typewriter state ---
	TArray<FString>                    TypewriterPages;
	TArray<int32>                      TypewriterPageCharStarts;
	int32                              CurrentPageIndex     = 0;
	bool                               bTypewriterActive    = false;
	float                              TypewriterFullWidth  = 0.f;
	float                              TypewriterFullHeight = 0.f;
	int32                              LastSoundCharIndex   = -1;
	double                             LastSoundPlayTime    = 0.0;

	// --- Animation state ---
	bool                               bAnimating    = false;
	bool                               bExitAnim     = false;
	bool                               bPendingRemoval = false;
	float                              AnimElapsed   = 0.f;
	float                              AnimDuration  = 0.3f;
	ESubtitleEntranceType              AnimType      = ESubtitleEntranceType::None;
	TSharedPtr<FActiveTimerHandle>     AnimTimerHandle;
	float                              SlideOffsetX  = 2000.f;
	float                              SlideOffsetY  = 1200.f;

	// --- ShowMessage auto-hide (SlotID=0 only) ---
	TSharedPtr<FActiveTimerHandle>     AutoHideTimerHandle;
	float                              AutoHideRemaining    = 0.f;
	bool                               bIsShowMessageActive = false;

#if WITH_EDITOR
	// Per-slot drag handle (editor only)
	TSharedPtr<SSubtitleDragHandle>    DragHandle;
#endif
};

/** Broadcasts subtitle start/end events from Sequencer evaluation to UI widgets. */
UCLASS()
class SEQUENCERSUBTITLES_API USubtitleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category="Subtitles")
	FOnSubtitleStarted OnSubtitleStarted;

	UPROPERTY(BlueprintAssignable, Category="Subtitles")
	FOnSubtitleEnded OnSubtitleEnded;

	UPROPERTY(BlueprintAssignable, Category="Subtitles")
	FOnSubtitlePageAdvanced OnPageAdvanced;

	// --- Multi-slot API (called by sequencer eval tokens) ---
	void NotifySubtitleStarted(uint32 SlotID, const FText& InSubtitleText, FLinearColor InBarColor, const FSubtitleAppearance& InAppearance, const FText& InSpeakerName = FText::GetEmpty());
	void NotifySubtitleEnded(uint32 SlotID);
	void UpdateTypewriterProgress(uint32 SlotID, int32 VisibleCharCount);

	/**
	 * Returns true if a slot is active and NOT pending removal.
	 * Slots in exit-animation are considered inactive so the sequencer
	 * can restart them (e.g. on scrub-back) without waiting for the fade to finish.
	 */
	bool IsSlotActive(uint32 SlotID) const
	{
		const TSharedPtr<FSubtitleSlot>* Slot = ActiveSlots.Find(SlotID);
		return Slot && Slot->IsValid() && !(*Slot)->bPendingRemoval;
	}

	// --- Legacy no-SlotID API (used by ShowMessage / HideMessage, maps to SlotID=0) ---
	void NotifySubtitleStarted(const FText& InSubtitleText, FLinearColor InBarColor, const FSubtitleAppearance& InAppearance, const FText& InSpeakerName = FText::GetEmpty());
	UFUNCTION()
	void NotifySubtitleEnded();
	void UpdateTypewriterProgress(int32 VisibleCharCount);

	/**
	 * Display a message with default appearance. Auto-hides after Duration seconds (real time).
	 * If Duration <= 0, the message stays visible until HideMessage() is called.
	 */
	UFUNCTION(BlueprintCallable, Category="Subtitles")
	void ShowMessage(const FText& Text, float Duration = 3.0f, ESubtitleEntranceType Animation = ESubtitleEntranceType::FadeIn, float AnimationDuration = 0.3f, const FText& SpeakerName = FText::GetEmpty());

	/** Display a message with full appearance control. Auto-hides after Duration seconds (real time). */
	UFUNCTION(BlueprintCallable, Category="Subtitles")
	void ShowMessageEx(const FText& Text, float Duration, const FSubtitleAppearance& Appearance, const FText& SpeakerName = FText::GetEmpty());

	/** Display a message that stays visible until HideMessage() is called. */
	UFUNCTION(BlueprintCallable, Category="Subtitles")
	void ShowPersistentMessage(const FText& Text, ESubtitleEntranceType Animation = ESubtitleEntranceType::FadeIn, float AnimationDuration = 0.3f, const FText& SpeakerName = FText::GetEmpty());

	/** Manually dismiss the current ShowMessage display. No-op if not active. */
	UFUNCTION(BlueprintCallable, Category="Subtitles")
	void HideMessage();

	// UWorldSubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// BP-readable state (reflects most recently started slot)
	UPROPERTY(BlueprintReadOnly, Category="Subtitles")
	bool bIsSubtitleActive = false;

	UPROPERTY(BlueprintReadOnly, Category="Subtitles")
	FText CurrentSubtitleText;

	UPROPERTY(BlueprintReadOnly, Category="Subtitles")
	FSubtitleAppearance CurrentAppearance;

	UPROPERTY(BlueprintReadOnly, Category="Subtitles")
	FText CurrentSpeakerName;

	/** Apply MaxCharsPerLine wrapping to a string. Returns the wrapped version. */
	static FString WrapTextByCharLimit(const FString& InText, int32 MaxCharsPerLine);

#if WITH_EDITOR
	/** Register the active section for a slot (called from eval token for drag write-back). */
	void SetActiveSection(uint32 SlotID, UMovieSceneSeqSubtitleSection* InSection);
#endif

private:
	void EnsureSlateWidgets();
	void AddToViewport();
	void RemoveFromViewport();
	float GetSubtitleDPIScale() const;

	// Per-slot widget management
	void CreateSlotWidget(uint32 SlotID, FSubtitleSlot& Slot);
	void RemoveSlotWidget(uint32 SlotID);

	// Per-slot appearance / speaker helpers
	void ApplyAppearanceToSlot(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance);
	void ApplyOverlayPosition(const FSubtitleAppearance& InAppearance);
	void ApplySpeakerAndSeparatorToSlot(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance, const FText& InSpeakerName);

	// Typewriter: first-call initialization (paging, measurement, sound cache)
	void InitTypewriterState(FSubtitleSlot& Slot, uint32 SlotID, const FString& FullStr);

	// Per-slot animation
	void StartSlotAnimation(FSubtitleSlot& Slot, uint32 SlotID, ESubtitleEntranceType InType, float InDuration, bool bReverse);
	void ApplySlotAnimationAlpha(FSubtitleSlot& Slot, float EasedAlpha);
	EActiveTimerReturnType TickSlotAnimation(uint32 SlotID, double InCurrentTime, float InDeltaTime);

#if WITH_EDITOR
	// Per-slot drag callback
	void OnSlotDragOffsetChanged(uint32 SlotID, FVector2D NewOffset);
#endif

	// Pre-measure multi-line text for a slot (prevents layout flicker)
	void PreMeasureSlotText(FSubtitleSlot& Slot, const FText& InSubtitleText, const FSubtitleAppearance& InAppearance);

	// Typewriter sound for a slot
	void PlayTypewriterSoundForSlot(FSubtitleSlot& Slot, uint32 SlotID, int32 CurrentCharIndex);

	// ShowMessage auto-hide timer
	EActiveTimerReturnType TickAutoHide(double InCurrentTime, float InDeltaTime);

	// --- Outer (shared) Slate structure ---
	TSharedPtr<SOverlay>               WidgetOverlay;
	TSharedPtr<class SDPIScaler>       DPIScalerWidget;
	TSharedPtr<SVerticalBox>           ContentVerticalBox;
	SOverlay::FOverlaySlot*            OverlaySlot = nullptr;

	bool bAddedToViewport  = false;
	bool bIsEditorViewport = false;

	// --- Active slots (keyed by section UniqueID; 0 = ShowMessage) ---
	TMap<uint32, TSharedPtr<FSubtitleSlot>> ActiveSlots;

	// Sound cache per slot (UPROPERTY to prevent GC)
	UPROPERTY()
	TMap<uint32, TObjectPtr<USoundBase>> SlotSoundCache;

#if WITH_EDITOR
	TMap<uint32, TWeakObjectPtr<UMovieSceneSeqSubtitleSection>> ActiveSections;
#endif
};
