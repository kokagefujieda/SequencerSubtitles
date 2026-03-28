// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

/**
 * A horizontal separator line with optional fade-out on both ends.
 * Drawn via FSlateDrawElement::MakeGradient — no texture needed.
 */
class SSubtitleSeparatorLine : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSubtitleSeparatorLine)
		: _Color(FLinearColor::White)
		, _FadeLength(0.f)
	{}
		SLATE_ARGUMENT(FLinearColor, Color)
		/** Pixels to fade on each end (0 = no fade). */
		SLATE_ARGUMENT(float, FadeLength)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Color      = InArgs._Color;
		FadeLength = InArgs._FadeLength;
	}

	void SetColor(FLinearColor InColor)     { Color      = InColor;   Invalidate(EInvalidateWidgetReason::Paint); }
	void SetFadeLength(float InFadeLength)  { FadeLength = InFadeLength; Invalidate(EInvalidateWidgetReason::Paint); }

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const float W = Size.X;
		if (W <= 0.f) { return LayerId; }

		const float Fade = FMath::Clamp(FadeLength, 0.f, W * 0.5f);

		FLinearColor Transparent = Color;
		Transparent.A = 0.f;

		TArray<FSlateGradientStop> Stops;
		if (Fade > 0.f)
		{
			Stops.Add(FSlateGradientStop(FVector2D(0.f,      0.f), Transparent));
			Stops.Add(FSlateGradientStop(FVector2D(Fade,     0.f), Color));
			Stops.Add(FSlateGradientStop(FVector2D(W - Fade, 0.f), Color));
			Stops.Add(FSlateGradientStop(FVector2D(W,        0.f), Transparent));
		}
		else
		{
			Stops.Add(FSlateGradientStop(FVector2D(0.f, 0.f), Color));
			Stops.Add(FSlateGradientStop(FVector2D(W,   0.f), Color));
		}

		// Orient_Vertical = gradient bars are vertical lines → color varies left-to-right
		FSlateDrawElement::MakeGradient(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			Stops,
			EOrientation::Orient_Vertical,
			ESlateDrawEffect::None
		);

		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

private:
	FLinearColor Color      = FLinearColor::White;
	float        FadeLength = 0.f;
};
