// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SubtitleSettings.h"
#include "Widgets/SOverlay.h"
#include "SubtitleSubsystem.generated.h"

class STextBlock;
class SBorder;
class UMovieSceneSeqSubtitleSection;
#if WITH_EDITOR
class SSubtitleDragHandle;
#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnSubtitleStarted,
	const FText&, SubtitleText,
	FLinearColor, BarColor
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSubtitleEnded);

/** Broadcasts subtitle start/end events from Sequencer evaluation to UI widgets. */
UCLASS()
class SEQUENCERSUBTITLES_API USubtitleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FOnSubtitleStarted OnSubtitleStarted;

	UPROPERTY()
	FOnSubtitleEnded OnSubtitleEnded;

	void NotifySubtitleStarted(const FText& InSubtitleText, FLinearColor InBarColor, const FSubtitleAppearance& InAppearance, const FText& InSpeakerName = FText::GetEmpty());

	UFUNCTION()
	void NotifySubtitleEnded();

	/** Update the number of visible characters for typewriter effect. */
	void UpdateTypewriterProgress(int32 VisibleCharCount);

	/**
	 * Display a message with default appearance. Auto-hides after Duration seconds (real time).
	 * If Duration <= 0, the message stays visible until HideMessage() is called.
	 * @param Text               The message to display
	 * @param Duration           Visible period in seconds (real time, unaffected by TimeDilation). 0 = persistent.
	 * @param Animation          Entrance/exit animation type
	 * @param AnimationDuration  Duration of entrance/exit animation in seconds
	 */
	UFUNCTION()
	void ShowMessage(const FText& Text, float Duration = 3.0f, ESubtitleEntranceType Animation = ESubtitleEntranceType::FadeIn, float AnimationDuration = 0.3f);

	/** Display a message with full appearance control. Auto-hides after Duration seconds (real time).
	 *  If Duration <= 0, the message stays visible until HideMessage() is called. */
	UFUNCTION()
	void ShowMessageEx(const FText& Text, float Duration, const FSubtitleAppearance& Appearance);

	/** Manually dismiss the current ShowMessage display. No-op if not active. */
	UFUNCTION()
	void HideMessage();

	// UWorldSubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	UPROPERTY()
	bool bIsSubtitleActive = false;

	UPROPERTY()
	FText CurrentSubtitleText;

	UPROPERTY()
	FSubtitleAppearance CurrentAppearance;

	const FText& GetCurrentSpeakerName() const { return CurrentSpeakerName; }

	/** Apply MaxCharsPerLine wrapping to a string. Returns the wrapped version. */
	static FString WrapTextByCharLimit(const FString& InText, int32 MaxCharsPerLine);

#if WITH_EDITOR
	/** Set the currently active section (called from eval token for drag write-back). */
	void SetActiveSection(UMovieSceneSeqSubtitleSection* InSection);

	/** Called by drag handle when the user finishes dragging. Writes offset back to section/track. */
	void OnDragOffsetChanged(FVector2D NewOffset);
#endif

private:
	void EnsureSlateWidgets();
	void AddToViewport();
	void RemoveFromViewport();
	void ApplyAppearance(const FSubtitleAppearance& InAppearance);
	float GetSubtitleDPIScale() const;
	void StartAnimation(ESubtitleEntranceType InType, float InDuration, bool bReverse);
	EActiveTimerReturnType TickAnimation(double InCurrentTime, float InDeltaTime);
	void ApplyAnimationAlpha(float EasedAlpha);

	void ApplySpeakerAndSeparator(const FSubtitleAppearance& InAppearance, const FText& InSpeakerName);

	TSharedPtr<SOverlay>         WidgetOverlay;
	TSharedPtr<class SDPIScaler> DPIScalerWidget;
	TSharedPtr<class SVerticalBox> ContentVerticalBox;
	TSharedPtr<STextBlock>       SpeakerNameTextBlock;
	TSharedPtr<class SBox>       SeparatorBox;
	TSharedPtr<SBorder>          SeparatorLineBorder;
	TSharedPtr<STextBlock>       SubtitleTextBlock;
	TSharedPtr<SBorder>          SubtitleBorder;
	TSharedPtr<class SBox>       MessageWindowBox;
	TSharedPtr<class SBox>       TypewriterSizerBox;
	SOverlay::FOverlaySlot*      OverlaySlot = nullptr;

	// Custom separator brush (kept alive for SBorder pointer stability)
	FSlateBrush CustomSeparatorBrush;

	UPROPERTY()
	FText CurrentSpeakerName;

	// Typewriter fixed-size anchoring
	FSlateFontInfo CurrentFontInfo;
	float          TypewriterFullWidth  = 0.f;
	float          TypewriterFullHeight = 0.f;
	bool           bTypewriterActive    = false;

	// Typewriter sound
	void PlayTypewriterSound(int32 CurrentCharIndex);
	UPROPERTY()
	TObjectPtr<USoundBase> CachedTypewriterSound = nullptr;
	int32  LastSoundCharIndex = -1;
	double LastSoundPlayTime  = 0.0;

	// BotW-style paging
	TArray<FString> TypewriterPages;
	TArray<int32>   TypewriterPageCharStarts;
	int32           CurrentPageIndex = 0;

	bool bAddedToViewport = false;
	bool bIsEditorViewport = false;

#if WITH_EDITOR
	TSharedPtr<SSubtitleDragHandle> DragHandle;
	TWeakObjectPtr<UMovieSceneSeqSubtitleSection> ActiveSection;
#endif

	bool bAnimating = false;
	bool bExitAnimation = false;
	float AnimElapsed = 0.0f;
	float AnimDuration = 0.3f;
	ESubtitleEntranceType AnimationType = ESubtitleEntranceType::None;
	TSharedPtr<FActiveTimerHandle> AnimTimerHandle;

	// Viewport-relative slide offsets (computed in StartAnimation)
	float SlideOffsetX = 2000.0f;
	float SlideOffsetY = 1200.0f;

	// ShowMessage auto-hide (real-time Slate active timer)
	EActiveTimerReturnType TickAutoHide(double InCurrentTime, float InDeltaTime);
	TSharedPtr<FActiveTimerHandle> AutoHideTimerHandle;
	float AutoHideRemaining = 0.0f;
	bool bIsShowMessageActive = false;
};
