// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SubtitleSettings.h"
#include "SeqImageTypes.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Containers/Ticker.h"
#include "SubtitleSubsystem.generated.h"

class STextBlock;
class SBorder;
class SBox;
class USoundBase;
class UGameViewportClient;
class SSubtitleSeparatorLine;
class SSeqImage;
class UMovieSceneSeqSubtitleSection;
class UMovieSceneSeqImageSection;
#if WITH_EDITOR
class SSubtitleDragHandle;
class IAssetViewport;
#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnSubtitleStarted,
	const FText&, SubtitleText,
	FLinearColor, BarColor,
	const FText&, SpeakerName
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSubtitleEnded);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubtitlePageAdvanced, int32, NewPageIndex);

/** A subtitle started. SlotID identifies it until OnSubtitleSlotEnded (0 = ShowMessage). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnSubtitleSlotStarted,
	int32, SlotID,
	const FText&, SubtitleText,
	const FText&, SpeakerName,
	const FSubtitleAppearance&, Appearance
);

/** The visible text of a subtitle changed (typewriter progress). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSubtitleSlotTextChanged, int32, SlotID, const FText&, VisibleText);

/** A subtitle was removed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubtitleSlotEnded, int32, SlotID);

/**
 * Per-slot widget set and state for one simultaneously active subtitle.
 * One instance is created per active sequencer section (keyed by section UniqueID).
 */
struct FSubtitleSlot
{
	uint32                             SlotID = 0;

	// --- Slate widgets for this entry (none when the built-in display is off) ---
	/** Widget added to the position group: the drag handle in the editor viewport, otherwise EntryVBox. */
	TSharedPtr<SWidget>                RootWidget;
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
	/** Transparent copies of every typewriter page; reserve the final text size while characters are revealed. */
	TSharedPtr<SOverlay>               TypewriterSizerOverlay;
	FSlateBrush                        CustomSeparatorBrush;
	FSlateBrush                        WindowBrush;
	SVerticalBox::FSlot*               SpeakerNameSlot = nullptr;
	SVerticalBox::FSlot*               SeparatorSlot   = nullptr;

	// --- Outline text layers (subtitle) ---
	// Blur layers use a 4-step Gaussian-like alpha curve for smooth glow.
	static constexpr int32 NumBlurSteps = 4;
	TSharedPtr<SOverlay>               SubtitleTextOverlay;
	TSharedPtr<STextBlock>             OuterBlurTextBlocks[NumBlurSteps];
	TSharedPtr<STextBlock>             OuterOutlineTextBlock;
	TSharedPtr<STextBlock>             InnerBlurTextBlocks[NumBlurSteps];
	TSharedPtr<STextBlock>             InnerOutlineTextBlock;

	// --- Outline text layers (speaker name) ---
	TSharedPtr<SOverlay>               SpeakerTextOverlay;
	TSharedPtr<STextBlock>             SpeakerOuterBlurTextBlocks[NumBlurSteps];
	TSharedPtr<STextBlock>             SpeakerOuterOutlineTextBlock;
	TSharedPtr<STextBlock>             SpeakerInnerBlurTextBlocks[NumBlurSteps];
	TSharedPtr<STextBlock>             SpeakerInnerOutlineTextBlock;

	// --- Position group (slots with the same position share one) ---
	FString                            GroupKey;
	bool                               bInGroup = false;

	// --- Subtitle state ---
	/** Appearance as authored (track / section / ShowMessage). */
	FSubtitleAppearance                SourceAppearance;
	/** SourceAppearance with the player settings (text scale, background opacity) applied. */
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
	int32                              LastSoundCharIndex   = -1;
	double                             LastSoundPlayTime    = 0.0;
	/** Visible character count last reported through OnSubtitleSlotTextChanged. */
	int32                              LastVisibleCharCount = -1;
	/** Text currently shown (full text, or the typewriter part). */
	FText                              VisibleText;

	// --- Clock (entrance / exit / tremble are computed from it) ---
	/** true = ShowMessage: the subsystem advances LocalTime itself. false = driven by the sequence time. */
	bool                               bSelfClocked = false;
	/** Seconds since the subtitle started. */
	float                              LocalTime    = 0.f;
	/** Total display time in seconds; the exit finishes at this time. < 0 = until HideMessage (self-clocked). */
	float                              Duration     = 0.f;

#if WITH_EDITOR
	// Per-slot drag handle (editor only)
	TSharedPtr<SSubtitleDragHandle>    DragHandle;
#endif
};

/**
 * Widgets and state for one image shown by an Image Track section (keyed by section UniqueID).
 * Root (drag handle in the editor) -> OffsetBox (layout offset + motion) -> ImageWidget (effects).
 */
struct FSeqImageSlot
{
	TSharedPtr<SWidget>                RootWidget;
	TSharedPtr<SBox>                   OffsetBox;
	TSharedPtr<SSeqImage>              ImageWidget;
	SOverlay::FOverlaySlot*            LayerSlot = nullptr;
	bool                               bInFront  = false;
	int32                              ZOrder    = 0;
	ESeqImageAnchor                    Anchor    = ESeqImageAnchor::Center;
	FSoftObjectPath                    TexturePath;

	/** Settings of the last update. */
	FSeqImageParams                    Params;

	/** Eased motion progress of the last update (0 = at Layout.Offset, 1 = at Motion.EndOffset). */
	float                              MotionAlpha = 0.f;

	/** Keyframe values of the last update (combined with the layout / motion). */
	FSeqImageKeyedValues               Keyed;

#if WITH_EDITOR
	TSharedPtr<SSubtitleDragHandle>    DragHandle;
	TWeakObjectPtr<UMovieSceneSeqImageSection> Section;
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

	/** Like OnSubtitleStarted, with a slot ID to tell simultaneous subtitles apart (for custom UI). */
	UPROPERTY(BlueprintAssignable, Category="Subtitles")
	FOnSubtitleSlotStarted OnSubtitleSlotStarted;

	/** Visible text changed while the typewriter reveals it. Also fired when the built-in display is off. */
	UPROPERTY(BlueprintAssignable, Category="Subtitles")
	FOnSubtitleSlotTextChanged OnSubtitleSlotTextChanged;

	UPROPERTY(BlueprintAssignable, Category="Subtitles")
	FOnSubtitleSlotEnded OnSubtitleSlotEnded;

	// --- Multi-slot API (called by sequencer eval tokens) ---
	void NotifySubtitleStarted(uint32 SlotID, const FText& InSubtitleText, FLinearColor InBarColor, const FSubtitleAppearance& InAppearance, const FText& InSpeakerName = FText::GetEmpty());

	/**
	 * Sequencer subtitles: remove immediately (the exit already played inside the section).
	 * ShowMessage (self-clocked): start the exit animation.
	 */
	void NotifySubtitleEnded(uint32 SlotID);

	/** Update entrance / exit / tremble of a sequencer subtitle for LocalTime seconds into its section. */
	void UpdateSubtitleTime(uint32 SlotID, float LocalTime, float Duration);

	void UpdateTypewriterProgress(uint32 SlotID, int32 VisibleCharCount);

	bool IsSlotActive(uint32 SlotID) const
	{
		const TSharedPtr<FSubtitleSlot>* Slot = ActiveSlots.Find(SlotID);
		return Slot && Slot->IsValid();
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
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
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

	// --- Image API (called by image track eval tokens) ---

	/**
	 * Show or update an image at LocalTime seconds into its section.
	 * Outside [0, Duration) the image is removed.
	 */
	void UpdateImage(uint32 SlotID, const FSeqImageParams& Params, const FSeqImageKeyedValues& Keyed,
		float LocalTime, float Duration, UMovieSceneSeqImageSection* SourceSection = nullptr);

	/** Remove an image (no-op if not shown). */
	void RemoveImage(uint32 SlotID);

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

	/** Pixel size of the viewport the subtitles are drawn in (zero if unknown). */
	FIntPoint GetHostViewportSize() const;

	/** Scale applied by DPIScalerWidget (on top of whatever Slate already applies). */
	float GetSubtitleDPIScale() const;

	/** Viewport size in Slate units (zero if unknown). */
	FVector2D GetViewportSlateSize() const;

	/** Show the overlay while any subtitle or image is active. */
	void UpdateOverlayVisibility();

	/** Dragging in the editor viewport is possible (editor world + project setting). */
	bool IsViewportDragAllowed() const;

	/** The built-in display should show this subtitle (project setting; player setting for sequencer subtitles). */
	bool ShouldDisplaySlot(const FSubtitleSlot& Slot) const;

	// Image helpers
	void CreateImageSlotWidget(uint32 SlotID, FSeqImageSlot& Slot);
	void PlaceImageSlot(FSeqImageSlot& Slot);
	void ApplyImageState(FSeqImageSlot& Slot, float LocalTime, float Duration);

	// Per-slot widget management
	void CreateSlotWidget(uint32 SlotID, FSubtitleSlot& Slot);
	/** Remove a subtitle (widgets and state). Broadcasts the ended events when bBroadcast. */
	void RemoveSlot(uint32 SlotID, bool bBroadcast = true);

	// Position groups (F2)
	void PlaceSlotInGroup(FSubtitleSlot& Slot);
	void RemoveSlotFromGroup(FSubtitleSlot& Slot);

	// Per-slot appearance / speaker helpers
	FSubtitleAppearance MakeEffectiveAppearance(const FSubtitleAppearance& InAppearance) const;
	void ApplyAppearanceToSlot(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance);
	void ApplySpeakerAndSeparatorToSlot(FSubtitleSlot& Slot, const FSubtitleAppearance& InAppearance, const FText& InSpeakerName);

	// Typewriter: first-call initialization (paging, sizer, sound cache)
	void InitTypewriterState(FSubtitleSlot& Slot, uint32 SlotID, const FString& FullStr);
	void RebuildTypewriterSizer(FSubtitleSlot& Slot);

	/** Entrance / exit / tremble for the slot's clock (no timers: same result when scrubbing or rendering). */
	void ApplySubtitleVisual(FSubtitleSlot& Slot);

	// ShowMessage clock (real time)
	void StartSelfClock();
	void StopSelfClock();
	bool TickSelfClock(float DeltaTime);

	// Player settings changed: re-apply to the subtitles on screen
	void HandleUserSettingsChanged();

#if WITH_EDITOR
	// Per-slot drag callback
	void OnSlotDragOffsetChanged(uint32 SlotID, FVector2D NewOffset);

	// Image drag callback
	void OnImageDragFinished(uint32 SlotID, FVector2D NewOffset);
#endif

	// Typewriter sound for a slot
	void PlayTypewriterSoundForSlot(FSubtitleSlot& Slot, uint32 SlotID, int32 CurrentCharIndex);

	// --- Outer (shared) Slate structure ---
	TSharedPtr<SOverlay>               WidgetOverlay;
	TSharedPtr<class SDPIScaler>       DPIScalerWidget;

	/** One vertical stack per distinct subtitle position (key = position + padding). */
	TMap<FString, TSharedPtr<SVerticalBox>> SubtitleGroups;

	/** Ticker for the ShowMessage clock. */
	FTSTicker::FDelegateHandle         SelfClockHandle;

	/** Subscription to USubtitleUserSettings::OnChanged. */
	FDelegateHandle                    UserSettingsChangedHandle;

	// Image layers behind / in front of the subtitles (full screen)
	TSharedPtr<SOverlay>               ImageLayerBack;
	TSharedPtr<SOverlay>               ImageLayerFront;

	bool bAddedToViewport  = false;
	bool bIsEditorViewport = false;

	/** Game viewport the widget was added to (game / PIE). */
	TWeakObjectPtr<UGameViewportClient> HostGameViewport;

#if WITH_EDITOR
	/** Level editor viewport the widget was added to (the active viewport may change afterwards). */
	TWeakPtr<IAssetViewport> HostEditorViewport;
#endif

	// --- Active slots (keyed by section UniqueID; 0 = ShowMessage) ---
	TMap<uint32, TSharedPtr<FSubtitleSlot>> ActiveSlots;

	// Sound cache per slot (UPROPERTY to prevent GC)
	UPROPERTY()
	TMap<uint32, TObjectPtr<USoundBase>> SlotSoundCache;

	// Window / separator images of the shown subtitles (UPROPERTY to prevent GC while Slate draws them)
	UPROPERTY()
	TMap<uint32, TObjectPtr<UTexture2D>> SlotWindowTextures;

	UPROPERTY()
	TMap<uint32, TObjectPtr<UTexture2D>> SlotLineTextures;

	// --- Active images (keyed by image section UniqueID) ---
	TMap<uint32, TSharedPtr<FSeqImageSlot>> ActiveImages;

	// Textures of the shown images (UPROPERTY to prevent GC while Slate draws them)
	UPROPERTY()
	TMap<uint32, TObjectPtr<UTexture2D>> ImageTextures;

#if WITH_EDITOR
	TMap<uint32, TWeakObjectPtr<UMovieSceneSeqSubtitleSection>> ActiveSections;
#endif
};
