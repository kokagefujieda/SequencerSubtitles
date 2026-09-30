// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "SeqImageTypes.generated.h"

/** Screen anchor of an image. */
UENUM(BlueprintType)
enum class ESeqImageAnchor : uint8
{
	TopLeft      UMETA(DisplayName = "Top Left"),
	TopCenter    UMETA(DisplayName = "Top Center"),
	TopRight     UMETA(DisplayName = "Top Right"),
	CenterLeft   UMETA(DisplayName = "Center Left"),
	Center       UMETA(DisplayName = "Center"),
	CenterRight  UMETA(DisplayName = "Center Right"),
	BottomLeft   UMETA(DisplayName = "Bottom Left"),
	BottomCenter UMETA(DisplayName = "Bottom Center"),
	BottomRight  UMETA(DisplayName = "Bottom Right"),
};

/** How the displayed image size is determined. */
UENUM(BlueprintType)
enum class ESeqImageSizeMode : uint8
{
	/** Texture pixels (1 px = 1 Slate unit at 1080p). */
	Native              UMETA(DisplayName = "Native"),
	/** FixedSize in pixels. A 0 component keeps the aspect ratio. */
	Fixed               UMETA(DisplayName = "Fixed Size"),
	/** Width = ScreenPercent of the screen width, height keeps the aspect ratio. */
	ScreenWidthPercent  UMETA(DisplayName = "Percent of Screen Width"),
	/** Height = ScreenPercent of the screen height, width keeps the aspect ratio. */
	ScreenHeightPercent UMETA(DisplayName = "Percent of Screen Height"),
	/** Largest size that fits inside the screen (contain). */
	FitScreen           UMETA(DisplayName = "Fit Screen"),
	/** Smallest size that covers the whole screen (cover). */
	FillScreen          UMETA(DisplayName = "Fill Screen"),
};

/** Draw order relative to the subtitles. */
UENUM(BlueprintType)
enum class ESeqImageLayer : uint8
{
	BehindSubtitles   UMETA(DisplayName = "Behind Subtitles"),
	InFrontOfSubtitles UMETA(DisplayName = "In Front of Subtitles"),
};

/** Entrance / exit effect of an image. */
UENUM(BlueprintType)
enum class ESeqImageEffect : uint8
{
	None     UMETA(DisplayName = "None"),
	Fade     UMETA(DisplayName = "Fade"),
	/** Moves in from (or out to) a side of the screen. */
	Slide    UMETA(DisplayName = "Slide"),
	/** Reveals the image from one side with a soft edge. */
	Wipe     UMETA(DisplayName = "Wipe"),
	/** The image is cut in two halves that come together (or fly apart). */
	Split    UMETA(DisplayName = "Split"),
	/** Opens from the center line outward. */
	BarnDoor UMETA(DisplayName = "Barn Door"),
	/** Scales from StartScale. Use Easing = Back or Bounce for a pop. */
	Zoom     UMETA(DisplayName = "Zoom"),
	/** Spins by Angle while scaling from StartScale. */
	Rotate   UMETA(DisplayName = "Rotate"),
	/** Card flip (the image is squashed along one axis). */
	Flip     UMETA(DisplayName = "Flip"),
	/** Circle opening from the center. */
	Iris     UMETA(DisplayName = "Iris"),
	/** Strips revealed one after another. */
	Blinds   UMETA(DisplayName = "Blinds"),
};

/** A side of the image / screen. */
UENUM(BlueprintType)
enum class ESeqImageDirection : uint8
{
	Left   UMETA(DisplayName = "Left"),
	Right  UMETA(DisplayName = "Right"),
	Top    UMETA(DisplayName = "Top"),
	Bottom UMETA(DisplayName = "Bottom"),
};

/** Horizontal = left / right, Vertical = top / bottom. */
UENUM(BlueprintType)
enum class ESeqImageAxis : uint8
{
	Horizontal UMETA(DisplayName = "Horizontal"),
	Vertical   UMETA(DisplayName = "Vertical"),
};

/** Easing curve. Back / Bounce / Elastic overshoot or bounce at the end. */
UENUM(BlueprintType)
enum class ESeqImageEasing : uint8
{
	Linear    UMETA(DisplayName = "Linear"),
	EaseIn    UMETA(DisplayName = "Ease In"),
	EaseOut   UMETA(DisplayName = "Ease Out"),
	EaseInOut UMETA(DisplayName = "Ease In Out"),
	Back      UMETA(DisplayName = "Back (overshoot)"),
	Bounce    UMETA(DisplayName = "Bounce"),
	Elastic   UMETA(DisplayName = "Elastic"),
};

/** Effect applied for the whole time the image is shown. */
UENUM(BlueprintType)
enum class ESeqImageContinuousEffect : uint8
{
	None     UMETA(DisplayName = "None"),
	/** Random-looking shake. */
	Tremble  UMETA(DisplayName = "Tremble"),
	/** Gentle up and down bobbing. */
	Float    UMETA(DisplayName = "Float"),
	/** Scale pulsing. */
	Pulse    UMETA(DisplayName = "Pulse"),
	/** Slow zoom and pan over the section. */
	KenBurns UMETA(DisplayName = "Ken Burns"),
};

/** Placement and look of an image. */
USTRUCT(BlueprintType)
struct SEQUENCERSUBTITLES_API FSeqImageLayout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	ESeqImageAnchor Anchor = ESeqImageAnchor::Center;

	/** Pixel offset from the anchor. Can be set by dragging the image in the editor viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FVector2D Offset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	ESeqImageSizeMode SizeMode = ESeqImageSizeMode::Native;

	/** Size in pixels. A 0 component keeps the aspect ratio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout",
		meta = (EditCondition = "SizeMode == ESeqImageSizeMode::Fixed", EditConditionHides, ClampMin = "0"))
	FVector2D FixedSize = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout",
		meta = (EditCondition = "SizeMode == ESeqImageSizeMode::ScreenWidthPercent || SizeMode == ESeqImageSizeMode::ScreenHeightPercent",
			EditConditionHides, ClampMin = "0", UIMax = "100"))
	float ScreenPercent = 50.0f;

	/** Extra scale applied on top of the size mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0"))
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Opacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bFlipX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bFlipY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	ESeqImageLayer Layer = ESeqImageLayer::BehindSubtitles;

	/** Higher values are drawn on top of other images in the same layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0"))
	int32 ZOrder = 0;
};

/** Entrance or exit effect. The exit plays inside the end of the section. */
USTRUCT(BlueprintType)
struct SEQUENCERSUBTITLES_API FSeqImageTransition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	ESeqImageEffect Effect = ESeqImageEffect::Fade;

	/** Seconds. Shortened automatically if entrance + exit are longer than the section. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect != ESeqImageEffect::None", EditConditionHides, ClampMin = "0.0", UIMax = "3.0"))
	float Duration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect != ESeqImageEffect::None", EditConditionHides))
	ESeqImageEasing Easing = ESeqImageEasing::EaseOut;

	/**
	 * Slide: the side the image comes from (exit: goes to).
	 * Wipe / Blinds: the side the reveal starts from.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect == ESeqImageEffect::Slide || Effect == ESeqImageEffect::Wipe || Effect == ESeqImageEffect::Blinds",
			EditConditionHides))
	ESeqImageDirection Direction = ESeqImageDirection::Bottom;

	/** Split / Barn Door / Flip: Horizontal = left-right, Vertical = top-bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect == ESeqImageEffect::Split || Effect == ESeqImageEffect::BarnDoor || Effect == ESeqImageEffect::Flip",
			EditConditionHides))
	ESeqImageAxis Axis = ESeqImageAxis::Horizontal;

	/** Slide / Split: travel distance in pixels. 0 = from outside the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect == ESeqImageEffect::Slide || Effect == ESeqImageEffect::Split", EditConditionHides, ClampMin = "0"))
	float Distance = 0.0f;

	/** Wipe / Barn Door / Iris / Blinds: width of the soft edge in pixels. 0 = hard edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect == ESeqImageEffect::Wipe || Effect == ESeqImageEffect::BarnDoor || Effect == ESeqImageEffect::Iris || Effect == ESeqImageEffect::Blinds",
			EditConditionHides, ClampMin = "0"))
	float Softness = 32.0f;

	/** Zoom / Rotate: scale at the start (0 = from nothing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect == ESeqImageEffect::Zoom || Effect == ESeqImageEffect::Rotate", EditConditionHides, ClampMin = "0.0"))
	float StartScale = 0.0f;

	/** Rotate: rotation in degrees at the start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect == ESeqImageEffect::Rotate", EditConditionHides))
	float Angle = 360.0f;

	/** Blinds: number of strips. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect == ESeqImageEffect::Blinds", EditConditionHides, ClampMin = "1", ClampMax = "64"))
	int32 BlindsCount = 8;

	/** Blinds: 0 = all strips at once, close to 1 = one strip after another. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect == ESeqImageEffect::Blinds", EditConditionHides, ClampMin = "0.0", ClampMax = "0.95"))
	float BlindsStagger = 0.5f;

	/** Fade at the same time (for effects other than Fade). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition",
		meta = (EditCondition = "Effect != ESeqImageEffect::None && Effect != ESeqImageEffect::Fade", EditConditionHides))
	bool bFade = true;
};

/** Effect applied while the image is shown. */
USTRUCT(BlueprintType)
struct SEQUENCERSUBTITLES_API FSeqImageContinuous
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuous Effect")
	ESeqImageContinuousEffect Effect = ESeqImageContinuousEffect::None;

	/** Tremble / Float: movement in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuous Effect",
		meta = (EditCondition = "Effect == ESeqImageContinuousEffect::Tremble || Effect == ESeqImageContinuousEffect::Float", EditConditionHides, ClampMin = "0.0"))
	float Amplitude = 6.0f;

	/** Tremble / Float / Pulse: cycles per second (Tremble looks good around 8). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuous Effect",
		meta = (EditCondition = "Effect == ESeqImageContinuousEffect::Tremble || Effect == ESeqImageContinuousEffect::Float || Effect == ESeqImageContinuousEffect::Pulse",
			EditConditionHides, ClampMin = "0.01"))
	float Frequency = 1.0f;

	/** Pulse: scale change (0.05 = +-5%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuous Effect",
		meta = (EditCondition = "Effect == ESeqImageContinuousEffect::Pulse", EditConditionHides, ClampMin = "0.0"))
	float PulseScale = 0.05f;

	/** Ken Burns: extra zoom reached at the section end (0.1 = +10%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuous Effect",
		meta = (EditCondition = "Effect == ESeqImageContinuousEffect::KenBurns", EditConditionHides))
	float KenBurnsZoom = 0.1f;

	/** Ken Burns: movement in pixels reached at the section end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continuous Effect",
		meta = (EditCondition = "Effect == ESeqImageContinuousEffect::KenBurns", EditConditionHides))
	FVector2D KenBurnsPan = FVector2D::ZeroVector;
};

/** Moves the image from Layout.Offset to EndOffset while it is shown. */
USTRUCT(BlueprintType)
struct SEQUENCERSUBTITLES_API FSeqImageMotion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	bool bEnabled = false;

	/** Offset at the end of the motion. Dragging the image near the end of the motion edits this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "bEnabled"))
	FVector2D EndOffset = FVector2D::ZeroVector;

	/** Seconds from the section start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float StartTime = 0.0f;

	/** Seconds. 0 = until the section end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "bEnabled"))
	ESeqImageEasing Easing = ESeqImageEasing::EaseInOut;
};

/** Everything needed to display one image section (copied into the evaluation template). */
USTRUCT(BlueprintType)
struct SEQUENCERSUBTITLES_API FSeqImageParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image")
	TSoftObjectPtr<UTexture2D> Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image")
	FSeqImageLayout Layout;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image")
	FSeqImageTransition Entrance;

	/** false = the exit plays the entrance backwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image")
	bool bOverrideExit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image")
	FSeqImageTransition Exit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image")
	FSeqImageContinuous Continuous;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image")
	FSeqImageMotion Motion;
};

/** Values of the keyframe channels of an image section at the current time (combined with the layout). */
struct FSeqImageKeyedValues
{
	/** Added to the offset. */
	FVector2D Offset   = FVector2D::ZeroVector;
	/** Multiplies the scale. */
	float     Scale    = 1.0f;
	/** Added to the rotation (degrees). */
	float     Rotation = 0.0f;
	/** Multiplies the opacity. */
	float     Opacity  = 1.0f;
};
