// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Sound/SoundBase.h"
#include "Engine/Texture2D.h"
#include "Engine/DeveloperSettings.h"
#include "SubtitleSettings.generated.h"

/** Vertical anchor for subtitle placement. */
UENUM(BlueprintType)
enum class ESubtitleVerticalPosition : uint8
{
	Top    UMETA(DisplayName = "Top"),
	Center UMETA(DisplayName = "Center"),
	Bottom UMETA(DisplayName = "Bottom"),
};

/** Horizontal anchor for subtitle placement. */
UENUM(BlueprintType)
enum class ESubtitleHorizontalPosition : uint8
{
	Left   UMETA(DisplayName = "Left"),
	Center UMETA(DisplayName = "Center"),
	Right  UMETA(DisplayName = "Right"),
};

/** Entrance animation type for subtitle appearance. */
UENUM(BlueprintType)
enum class ESubtitleEntranceType : uint8
{
	None          UMETA(DisplayName = "None"),
	FadeIn        UMETA(DisplayName = "Fade In"),
	SlideLeft     UMETA(DisplayName = "Slide Left"),
	SlideRight    UMETA(DisplayName = "Slide Right"),
	SlideTop      UMETA(DisplayName = "Slide Top"),
	SlideBottom   UMETA(DisplayName = "Slide Bottom"),
	ScaleVertical UMETA(DisplayName = "Scale Vertical"),
	ScaleUp       UMETA(DisplayName = "Scale Up"),
};

/** Horizontal text alignment for subtitle display. */
UENUM(BlueprintType)
enum class ESubtitleTextAlignment : uint8
{
	Left   UMETA(DisplayName = "Left"),
	Center UMETA(DisplayName = "Center"),
	Right  UMETA(DisplayName = "Right"),
};

/** Background style of the message window. */
UENUM(BlueprintType)
enum class EMessageWindowStyle : uint8
{
	Square  UMETA(DisplayName = "Square"),
	Rounded UMETA(DisplayName = "Rounded"),
	Image   UMETA(DisplayName = "Image"),
};

/** Tiling/scaling mode for a custom separator image. */
UENUM(BlueprintType)
enum class ELineImageTiling : uint8
{
	Stretch        UMETA(DisplayName = "Stretch (fill box)"),
	TileHorizontal UMETA(DisplayName = "Tile Horizontal"),
	TileVertical   UMETA(DisplayName = "Tile Vertical"),
	TileBoth       UMETA(DisplayName = "Tile Both"),
};

/** Alignment of the speaker name row. FollowSubtitle mirrors the subtitle's TextAlignment. */
UENUM(BlueprintType)
enum class ESpeakerNameAlignment : uint8
{
	FollowSubtitle UMETA(DisplayName = "Follow Subtitle (Default)"),
	Left           UMETA(DisplayName = "Left"),
	Center         UMETA(DisplayName = "Center"),
	Right          UMETA(DisplayName = "Right"),
};

/** Per-track / per-section subtitle visual configuration. */
USTRUCT(BlueprintType)
struct SEQUENCERSUBTITLES_API FSubtitleAppearance
{
	GENERATED_BODY()

	// ---- Layout ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	ESubtitleVerticalPosition VerticalPosition = ESubtitleVerticalPosition::Bottom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	ESubtitleHorizontalPosition HorizontalPosition = ESubtitleHorizontalPosition::Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FMargin ScreenPadding = FMargin(40.0f, 20.0f);

	/** Pixel offset from the anchor position. Typically set by dragging the subtitle in the editor viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FVector2D ScreenOffset = FVector2D::ZeroVector;

	// ---- Subtitle Text ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Text",
		meta = (AllowedClasses = "/Script/Engine.Font,/Script/Engine.FontFace"))
	TSoftObjectPtr<UObject> FontAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Text",
		meta = (ClampMin = "8", ClampMax = "128"))
	int32 FontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Text")
	FLinearColor TextColor = FLinearColor::White;

	/** Horizontal alignment of the subtitle text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Text")
	ESubtitleTextAlignment TextAlignment = ESubtitleTextAlignment::Center;

	/**
	 * Maximum characters per line before automatic line wrapping.
	 * 0 = no limit (rely on auto-wrap).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Text",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxCharsPerLine = 0;

	// ---- Message Window ----

	/** Background style: Square, Rounded corners, or custom Image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message Window")
	EMessageWindowStyle WindowStyle = EMessageWindowStyle::Square;

	/** Background color (alpha controls opacity for Square and Rounded styles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message Window")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	/** Opacity of the message window background (0 = transparent, 1 = opaque). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message Window",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WindowOpacity = 0.5f;

	/** Corner radius in pixels when WindowStyle is Rounded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message Window",
		meta = (EditCondition = "WindowStyle == EMessageWindowStyle::Rounded", ClampMin = "0", UIMin = "0"))
	float WindowCornerRadius = 12.0f;

	/** Custom background image when WindowStyle is Image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message Window",
		meta = (EditCondition = "WindowStyle == EMessageWindowStyle::Image"))
	TSoftObjectPtr<UTexture2D> WindowImage;

	/**
	 * Fixed height of the message window in pixels (before DPI scaling).
	 * 0 = auto-size (expands with text).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message Window",
		meta = (ClampMin = "0", UIMin = "0"))
	float MessageWindowHeight = 120.0f;

	// ---- Entrance / Exit ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance / Exit")
	ESubtitleEntranceType EntranceType = ESubtitleEntranceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance / Exit",
		meta = (ClampMin = "0.0", ClampMax = "3.0", EditCondition = "EntranceType != ESubtitleEntranceType::None"))
	float EntranceDuration = 0.3f;

	/** When true, exit animation uses ExitType/ExitDuration instead of reversing EntranceType. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance / Exit")
	bool bOverrideExitAnimation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance / Exit",
		meta = (EditCondition = "bOverrideExitAnimation"))
	ESubtitleEntranceType ExitType = ESubtitleEntranceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance / Exit",
		meta = (ClampMin = "0.0", ClampMax = "3.0", EditCondition = "bOverrideExitAnimation && ExitType != ESubtitleEntranceType::None"))
	float ExitDuration = 0.3f;

	// ---- Typewriter ----

	/** Sound to play per character during typewriter effect. Leave empty for silent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typewriter")
	TSoftObjectPtr<USoundBase> TypewriterSound;

	/** Minimum seconds between typewriter sound plays to prevent audio spam during fast playback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typewriter",
		meta = (ClampMin = "0.01", UIMin = "0.01"))
	float TypewriterSoundInterval = 0.05f;

	/**
	 * Maximum lines shown per page during typewriter effect (BotW-style paging).
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

	/**
	 * Horizontal alignment of the speaker name and separator line.
	 * FollowSubtitle (default): automatically matches the subtitle's TextAlignment.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker Name")
	ESpeakerNameAlignment SpeakerNameAlignment = ESpeakerNameAlignment::FollowSubtitle;

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

	/** Color of the default separator line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine && !bUseLineImage"))
	FLinearColor SeparatorLineColor = FLinearColor(0.8f, 0.7f, 0.3f, 0.8f);

	/** Thickness of the default separator line in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine && !bUseLineImage", ClampMin = "1", ClampMax = "32"))
	float SeparatorLineThickness = 1.0f;

	/**
	 * Pixels to fade out on each end of the default line (0 = hard edge, no fade).
	 * Has no effect in image mode.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine && !bUseLineImage", ClampMin = "0", UIMin = "0"))
	float SeparatorFadeLength = 30.0f;

	/** Height of the separator image in pixels. 0 = use image's natural height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine && bUseLineImage", ClampMin = "0", UIMin = "0"))
	float LineImageHeight = 0.0f;

	/**
	 * Width of the separator in pixels.
	 * Image mode: 0 = use image's natural width.
	 * Line mode:  0 = fill available width.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine", ClampMin = "0", UIMin = "0"))
	float SeparatorWidth = 300.0f;

	/** How the image is tiled / scaled inside the display box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine && bUseLineImage"))
	ELineImageTiling LineImageTiling = ELineImageTiling::Stretch;

	/** Tint color applied to the separator image (white = no tint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine && bUseLineImage"))
	FLinearColor LineImageColor = FLinearColor::White;

	/** Padding around the separator (Left, Top, Right, Bottom). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separator Line",
		meta = (EditCondition = "bShowSeparatorLine"))
	FMargin SeparatorPadding = FMargin(20.0f, 2.0f, 20.0f, 4.0f);

	// ---- Helpers ----

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
class SEQUENCERSUBTITLES_API USubtitleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USubtitleSettings();

	/** Default duration (in seconds) for new subtitle sections. */
	UPROPERTY(Config, EditAnywhere, Category = "Editor",
		meta = (ClampMin = "0.5", ClampMax = "30.0", UIMin = "0.5", UIMax = "10.0"))
	float DefaultSectionDuration = 3.0f;

	/** Maximum duration (in seconds) when auto-sizing to fill gap before next section. */
	UPROPERTY(Config, EditAnywhere, Category = "Editor",
		meta = (ClampMin = "0.5", ClampMax = "60.0", UIMin = "1.0", UIMax = "15.0"))
	float MaxDefaultSectionDuration = 5.0f;

	/** Characters per second used to auto-size a new section when pasting from clipboard.
	 *  Duration = ClipboardTextLength / CharsPerSecond (clamped to gap when applicable). */
	UPROPERTY(Config, EditAnywhere, Category = "Editor",
		meta = (ClampMin = "1.0", ClampMax = "30.0", UIMin = "2.0", UIMax = "20.0"))
	float ClipboardPasteCharsPerSecond = 10.0f;

	// --- Section Creation Defaults (set via "Set as Default" on any section) ---

	UPROPERTY(Config, EditAnywhere, Category = "Section Defaults")
	bool bDefaultTypewriterEffect = false;

	UPROPERTY(Config, EditAnywhere, Category = "Section Defaults",
		meta = (EditCondition = "bDefaultTypewriterEffect", ClampMin = "0.01", UIMin = "0.01"))
	float DefaultTypewriterCharInterval = 0.1f;

	UPROPERTY(Config, EditAnywhere, Category = "Section Defaults")
	bool bDefaultOverrideSpeakerName = false;

	UPROPERTY(Config, EditAnywhere, Category = "Section Defaults",
		meta = (EditCondition = "bDefaultOverrideSpeakerName"))
	FText DefaultSpeakerNameOverride;

	UPROPERTY(Config, EditAnywhere, Category = "Section Defaults")
	bool bDefaultOverrideAppearance = false;

	UPROPERTY(Config, EditAnywhere, Category = "Section Defaults",
		meta = (EditCondition = "bDefaultOverrideAppearance", ShowOnlyInnerProperties))
	FSubtitleAppearance DefaultAppearanceOverride;
};
