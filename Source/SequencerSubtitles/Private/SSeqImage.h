// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"
#include "SeqImageTypes.h"

class UTexture2D;

/** What SSeqImage should draw this frame. Movement, scale and rotation are applied as a render transform instead. */
struct FSeqImageDrawState
{
	/** Size of the image in local (Slate) units. */
	FVector2D DrawSize = FVector2D::ZeroVector;

	/** Tint including opacity. */
	FLinearColor Color = FLinearColor::White;

	/** Effect drawn by masking the image: Wipe, BarnDoor, Iris, Blinds, Split. Anything else draws the whole image. */
	ESeqImageEffect MaskEffect = ESeqImageEffect::None;

	/** 0 = hidden, 1 = fully shown. */
	float Progress = 1.f;

	ESeqImageDirection Direction = ESeqImageDirection::Bottom;
	ESeqImageAxis      Axis      = ESeqImageAxis::Horizontal;

	/** Soft edge width in local units. */
	float Softness = 0.f;

	/** Split: distance of each half from its final position. */
	float SplitOffset = 0.f;

	int32 BlindsCount   = 8;
	float BlindsStagger = 0.5f;
};

/**
 * Draws a texture with mask-type effects (wipe, barn door, iris, blinds, split).
 * Masks are drawn as texture sub-rectangles (UV regions); soft edges are drawn as thin slices
 * with decreasing opacity, so no material asset is needed.
 * The image is centered in the allotted area and may be larger than it (Fill Screen).
 */
class SSeqImage : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSeqImage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Set the texture (nullptr = draw nothing). */
	void SetTexture(UTexture2D* InTexture);

	/** Imported size of the current texture in pixels (zero if none). */
	FVector2D GetTextureSize() const { return TextureSize; }

	void SetDrawState(const FSeqImageDrawState& InState);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	FSlateBrush        Brush;
	FVector2D          TextureSize = FVector2D::ZeroVector;
	FSeqImageDrawState State;
};
