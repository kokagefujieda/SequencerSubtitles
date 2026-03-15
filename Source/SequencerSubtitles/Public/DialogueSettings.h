// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Sound/SoundBase.h"
#include "Engine/Texture2D.h"
#include "Engine/DeveloperSettings.h"
#include "DialogueSettings.generated.h"

/** Vertical anchor for subtitle placement. */
UENUM(BlueprintType)
enum class ESubtitleVerticalPosition : uint8
{
	Top    UMETA(DisplayName = "Top"),
	Center UMETA(DisplayName = "Center"),
	Bottom UMETA(DisplayName = "Bottom"),
};

/** Entrance animation type for subtitle appearance. */
UENUM(BlueprintType)
enum class ESubtitleEntranceType : uint8
{
	None         UMETA(DisplayName = "None"),
	FadeIn       UMETA(DisplayName = "Fade In"),
	SlideLeft    UMETA(DisplayName = "Slide Left"),
	SlideRight   UMETA(DisplayName = "Slide Right"),
	SlideTop     UMETA(DisplayName = "Slide Top"),
	SlideBottom  UMETA(DisplayName = "Slide Bottom"),
	ScaleVertical UMETA(DisplayName = "Scale Vertical"),
	ScaleUp      UMETA(DisplayName = "Scale Up"),
};

/** Horizontal text alignment for subtitle display. */
UENUM(BlueprintType)
enum class ESubtitleTextAlignment : uint8
{
	Left   UMETA(DisplayName = "Left"),
	Center UMETA(DisplayName = "Center"),
	Right  UMETA(DisplayName = "Right"),
};

/** Per-track / per-section subtitle visual configuration. */
USTRUCT(BlueprintType)
struct SEQUENCERSUBTITLES_API FDialogueAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance",
		meta = (AllowedClasses = "/Script/Engine.Font,/Script/Engine.FontFace"))
	TSoftObjectPtr<UObject> FontAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance",
		meta = (ClampMin = "8", ClampMax = "128"))
	int32 FontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance")
	FLinearColor TextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance")
	ESubtitleVerticalPosition VerticalPosition = ESubtitleVerticalPosition::Bottom;

	/** Horizontal alignment of the subtitle text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance")
	ESubtitleTextAlignment TextAlignment = ESubtitleTextAlignment::Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance")
	FMargin ScreenPadding = FMargin(40.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance")
	ESubtitleEntranceType EntranceType = ESubtitleEntranceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance",
		meta = (ClampMin = "0.0", ClampMax = "3.0", EditCondition = "EntranceType != ESubtitleEntranceType::None"))
	float EntranceDuration = 0.3f;

	/** Sound to play per character during typewriter effect. Leave empty for silent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typewriter")
	TSoftObjectPtr<USoundBase> TypewriterSound;

	/** Minimum seconds between typewriter sound plays to prevent audio spam during fast playback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typewriter",
		meta = (ClampMin = "0.01", UIMin = "0.01"))
	float TypewriterSoundInterval = 0.05f;

	/**
	 * Maximum lines shown per page during typewriter effect (BotW-style paging).
	 * When text exceeds this, earlier lines are cleared and the next page begins.
	 * 0 = no limit (show all lines at once).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typewriter",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxLinesPerPage = 2;

	// ---- Speaker Name ----

	/** Color of the speaker name text displayed above the subtitle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker Name")
	FLinearColor SpeakerNameColor = FLinearColor(1.0f, 0.85f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker Name",
		meta = (ClampMin = "8", ClampMax = "128"))
	int32 SpeakerNameFontSize = 20;

	// ---- Separator Line ----

	/** Show a separator line between the speaker name and subtitle text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line")
	bool bShowSeparatorLine = true;

	/** Use a custom image instead of the default generated line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine"))
	bool bUseLineImage = false;

	/** Custom image to use as separator (replaces the default line). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine && bUseLineImage"))
	TSoftObjectPtr<UTexture2D> LineImage;

	/** Color of the default separator line (when not using a custom image). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine && !bUseLineImage"))
	FLinearColor SeparatorLineColor = FLinearColor(0.8f, 0.7f, 0.3f, 0.8f);

	/** Thickness of the default separator line in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine && !bUseLineImage", ClampMin = "1", ClampMax = "8"))
	float SeparatorLineThickness = 1.0f;

	/** When true, exit animation uses ExitType/ExitDuration instead of reversing EntranceType. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance")
	bool bOverrideExitAnimation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance",
		meta = (EditCondition = "bOverrideExitAnimation"))
	ESubtitleEntranceType ExitType = ESubtitleEntranceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance",
		meta = (ClampMin = "0.0", ClampMax = "3.0", EditCondition = "bOverrideExitAnimation && ExitType != ESubtitleEntranceType::None"))
	float ExitDuration = 0.3f;

	/** Returns the effective exit animation type. */
	ESubtitleEntranceType GetEffectiveExitType() const
	{
		return bOverrideExitAnimation ? ExitType : EntranceType;
	}

	/** Returns the effective exit animation duration. */
	float GetEffectiveExitDuration() const
	{
		return bOverrideExitAnimation ? ExitDuration : EntranceDuration;
	}
};

/** Project-wide settings for the Sequencer Subtitles plugin. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Sequencer Subtitles"))
class SEQUENCERSUBTITLES_API UDialogueSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDialogueSettings();
};
