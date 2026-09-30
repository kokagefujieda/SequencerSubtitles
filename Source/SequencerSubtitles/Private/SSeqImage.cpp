// Copyright 2026 kokage. All Rights Reserved.

#include "SSeqImage.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include <type_traits>

namespace
{
	/** Draws parts of the image during one OnPaint call. Coordinates are image-local (0..Size). */
	struct FSeqImagePainter
	{
		const FGeometry&         Geometry;
		FSlateWindowElementList& OutDrawElements;
		int32                    LayerId;
		const FSlateBrush&       Brush;
		FVector2D                Origin; // top-left of the image in widget-local space
		FVector2D                Size;
		FLinearColor             Color;

		/** Draw the part [Min, Max] of the image, moved by Shift, with extra opacity. */
		void DrawPart(FVector2D Min, FVector2D Max, float Alpha, FVector2D Shift = FVector2D::ZeroVector) const
		{
			Min.X = FMath::Clamp(Min.X, 0.0, Size.X);
			Min.Y = FMath::Clamp(Min.Y, 0.0, Size.Y);
			Max.X = FMath::Clamp(Max.X, 0.0, Size.X);
			Max.Y = FMath::Clamp(Max.Y, 0.0, Size.Y);
			Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
			if (Max.X - Min.X < 0.01 || Max.Y - Min.Y < 0.01 || Alpha <= 0.f) { return; }

			// Show only the matching part of the texture (UV region); the brush's box type differs between engine versions
			using FUVBox  = std::decay_t<decltype(Brush.GetUVRegion())>;
			using FUVVec  = decltype(FUVBox::Min);
			using FUVReal = decltype(FUVVec::X);
			FSlateBrush PartBrush = Brush;
			PartBrush.SetUVRegion(FUVBox(
				FUVVec(static_cast<FUVReal>(Min.X / Size.X), static_cast<FUVReal>(Min.Y / Size.Y)),
				FUVVec(static_cast<FUVReal>(Max.X / Size.X), static_cast<FUVReal>(Max.Y / Size.Y))));

			FLinearColor PartColor = Color;
			PartColor.A *= Alpha;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				Geometry.ToPaintGeometry(FVector2f(Max - Min), FSlateLayoutTransform(FVector2f(Origin + Min + Shift))),
				&PartBrush,
				ESlateDrawEffect::None,
				PartColor);
		}

		/**
		 * Draw [From, To] along one axis, [CrossMin, CrossMax] on the other axis.
		 * From / To are distances from the start side (bReverse = the start side is the far end).
		 * The range is cut into Steps slices whose opacity is AlphaAt(distance of the slice center).
		 */
		void DrawSpan(bool bAlongX, bool bReverse, double From, double To, double CrossMin, double CrossMax,
			int32 Steps, TFunctionRef<float(double)> AlphaAt) const
		{
			if (To <= From) { return; }

			const double Len  = bAlongX ? Size.X : Size.Y;
			Steps = FMath::Max(Steps, 1);
			const double Step = (To - From) / Steps;
			for (int32 i = 0; i < Steps; ++i)
			{
				const double A     = From + Step * i;
				const double B     = A + Step;
				const float  Alpha = AlphaAt(A + Step * 0.5);
				const double Lo    = bReverse ? Len - B : A;
				const double Hi    = bReverse ? Len - A : B;
				if (bAlongX) { DrawPart(FVector2D(Lo, CrossMin), FVector2D(Hi, CrossMax), Alpha); }
				else         { DrawPart(FVector2D(CrossMin, Lo), FVector2D(CrossMax, Hi), Alpha); }
			}
		}
	};

	/** Number of slices used to draw a soft edge of the given width. */
	int32 SoftSteps(double Width)
	{
		return FMath::Clamp(FMath::CeilToInt(static_cast<float>(Width) / 3.f), 1, 16);
	}

	/** Opacity at distance D from the start of a soft edge that begins at Edge and is Width wide. */
	float EdgeAlpha(double D, double Edge, double Width)
	{
		if (Width <= 0.0) { return D <= Edge ? 1.f : 0.f; }
		return static_cast<float>(FMath::Clamp((Edge + Width - D) / Width, 0.0, 1.0));
	}

	/** Opacity callback for fully visible ranges. */
	const auto Opaque = [](double) { return 1.f; };
}

void SSeqImage::Construct(const FArguments& InArgs)
{
}

void SSeqImage::SetTexture(UTexture2D* InTexture)
{
	Brush = FSlateBrush();
	TextureSize = FVector2D::ZeroVector;

	if (InTexture)
	{
		// Imported size: the same in the editor and in cooked builds (independent of LOD / max size)
		const FIntPoint Imported = InTexture->GetImportedSize();
		TextureSize = (Imported.X > 0 && Imported.Y > 0)
			? FVector2D(Imported.X, Imported.Y)
			: FVector2D(InTexture->GetSizeX(), InTexture->GetSizeY());

		Brush.SetResourceObject(InTexture);
		Brush.ImageSize = TextureSize;
		Brush.DrawAs    = ESlateBrushDrawType::Image;
		Brush.Tiling    = ESlateBrushTileType::NoTile;
	}

	Invalidate(EInvalidateWidgetReason::Layout);
}

void SSeqImage::SetDrawState(const FSeqImageDrawState& InState)
{
	State = InState;
	Invalidate(EInvalidateWidgetReason::Layout);
}

FVector2D SSeqImage::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return State.DrawSize;
}

int32 SSeqImage::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const FVector2D Size = State.DrawSize;
	if (TextureSize.IsNearlyZero() || Size.X <= 0.0 || Size.Y <= 0.0) { return LayerId; }

	const FLinearColor Color = State.Color * InWidgetStyle.GetColorAndOpacityTint();
	if (Color.A <= 0.f) { return LayerId; }

	// Centered in the allotted area (the image may be larger than it, e.g. Fill Screen)
	const FVector2D Origin = (FVector2D(AllottedGeometry.GetLocalSize()) - Size) * 0.5;
	const FSeqImagePainter Painter{ AllottedGeometry, OutDrawElements, LayerId, Brush, Origin, Size, Color };

	const float  P = FMath::Clamp(State.Progress, 0.f, 1.f);
	const double W = FMath::Max(static_cast<double>(State.Softness), 0.0);

	const bool bMasked =
		(State.MaskEffect == ESeqImageEffect::Split && State.SplitOffset != 0.f) ||
		((State.MaskEffect == ESeqImageEffect::Wipe || State.MaskEffect == ESeqImageEffect::BarnDoor ||
		  State.MaskEffect == ESeqImageEffect::Iris || State.MaskEffect == ESeqImageEffect::Blinds) && P < 1.f);

	if (!bMasked)
	{
		Painter.DrawPart(FVector2D::ZeroVector, Size, 1.f);
		return LayerId;
	}

	switch (State.MaskEffect)
	{
	case ESeqImageEffect::Wipe:
	{
		// Reveal from the start side; the soft edge follows the visible part
		const bool   bAlongX  = State.Direction == ESeqImageDirection::Left || State.Direction == ESeqImageDirection::Right;
		const bool   bReverse = State.Direction == ESeqImageDirection::Right || State.Direction == ESeqImageDirection::Bottom;
		const double Len      = bAlongX ? Size.X : Size.Y;
		const double Cross    = bAlongX ? Size.Y : Size.X;
		const double Edge     = -W + P * (Len + W);
		auto AlphaAt = [Edge, W](double D) { return EdgeAlpha(D, Edge, W); };

		Painter.DrawSpan(bAlongX, bReverse, 0.0, Edge, 0.0, Cross, 1, Opaque);
		Painter.DrawSpan(bAlongX, bReverse, FMath::Max(Edge, 0.0), Edge + W, 0.0, Cross, SoftSteps(W), AlphaAt);
		break;
	}

	case ESeqImageEffect::BarnDoor:
	{
		// Opens from the center line toward both ends
		const bool   bAlongX = State.Axis == ESeqImageAxis::Horizontal;
		const double Len     = bAlongX ? Size.X : Size.Y;
		const double Cross   = bAlongX ? Size.Y : Size.X;
		const double C       = Len * 0.5;
		const double H       = -W + P * (C + W); // half width of the fully visible band
		auto AlphaFromCenter = [H, W](double Dist) { return EdgeAlpha(Dist, H, W); };

		Painter.DrawSpan(bAlongX, false, C - H, C + H, 0.0, Cross, 1, Opaque);
		// Soft edges on both sides (distances measured from the center)
		const double SoftFrom = FMath::Max(H, 0.0);
		Painter.DrawSpan(bAlongX, false, C + SoftFrom, C + H + W, 0.0, Cross, SoftSteps(W),
			[C, &AlphaFromCenter](double X) { return AlphaFromCenter(X - C); });
		Painter.DrawSpan(bAlongX, false, C - H - W, C - SoftFrom, 0.0, Cross, SoftSteps(W),
			[C, &AlphaFromCenter](double X) { return AlphaFromCenter(C - X); });
		break;
	}

	case ESeqImageEffect::Iris:
	{
		// Circle opening from the center, drawn as horizontal rows
		const FVector2D Center = Size * 0.5;
		const double    RMax   = Size.Size() * 0.5;
		const double    R      = -W + P * (RMax + W); // radius of the fully visible disc
		const double    ROuter = R + W;
		if (ROuter <= 0.0) { break; }

		const int32  Rows = FMath::Clamp(FMath::CeilToInt(static_cast<float>(Size.Y) / 4.f), 8, 200);
		const double RowH = Size.Y / Rows;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const double Y0 = RowH * Row;
			const double Y1 = Y0 + RowH;
			const double Dy = (Y0 + Y1) * 0.5 - Center.Y;
			if (FMath::Abs(Dy) >= ROuter) { continue; }

			const double Inner = (R > FMath::Abs(Dy)) ? FMath::Sqrt(R * R - Dy * Dy) : 0.0;
			const double Outer = FMath::Sqrt(ROuter * ROuter - Dy * Dy);
			auto AlphaAtX = [&Center, Dy, ROuter, W](double X)
			{
				const double Dist = FMath::Sqrt(FMath::Square(X - Center.X) + Dy * Dy);
				return EdgeAlpha(Dist, ROuter - W, W);
			};

			Painter.DrawSpan(true, false, Center.X - Inner, Center.X + Inner, Y0, Y1, 1, Opaque);
			if (W > 0.0)
			{
				Painter.DrawSpan(true, false, Center.X + Inner, Center.X + Outer, Y0, Y1, SoftSteps(Outer - Inner), AlphaAtX);
				Painter.DrawSpan(true, false, Center.X - Outer, Center.X - Inner, Y0, Y1, SoftSteps(Outer - Inner), AlphaAtX);
			}
		}
		break;
	}

	case ESeqImageEffect::Blinds:
	{
		// Strips revealed one after another, each one wiping from the start side
		const bool   bAlongX  = State.Direction == ESeqImageDirection::Left || State.Direction == ESeqImageDirection::Right;
		const bool   bReverse = State.Direction == ESeqImageDirection::Right || State.Direction == ESeqImageDirection::Bottom;
		const double Len      = bAlongX ? Size.X : Size.Y;
		const double Cross    = bAlongX ? Size.Y : Size.X;
		const int32  Count    = FMath::Max(State.BlindsCount, 1);
		const double StripLen = Len / Count;
		const float  Stagger  = FMath::Clamp(State.BlindsStagger, 0.f, 0.95f);
		const double StripW   = FMath::Min(W, StripLen);

		for (int32 i = 0; i < Count; ++i)
		{
			const float LocalP = (Count > 1)
				? FMath::Clamp((P - Stagger * i / (Count - 1)) / (1.f - Stagger), 0.f, 1.f)
				: P;
			if (LocalP <= 0.f) { continue; }

			const double Start = StripLen * i;
			const double End   = Start + StripLen;
			const double Edge  = Start - StripW + LocalP * (StripLen + StripW);
			auto AlphaAt = [Edge, StripW](double D) { return EdgeAlpha(D, Edge, StripW); };

			Painter.DrawSpan(bAlongX, bReverse, Start, FMath::Min(Edge, End), 0.0, Cross, 1, Opaque);
			Painter.DrawSpan(bAlongX, bReverse, FMath::Max(Edge, Start), FMath::Min(Edge + StripW, End), 0.0, Cross,
				SoftSteps(StripW), AlphaAt);
		}
		break;
	}

	case ESeqImageEffect::Split:
	{
		// Two halves, each moved away from the center by SplitOffset
		const double Off = State.SplitOffset;
		if (State.Axis == ESeqImageAxis::Horizontal)
		{
			Painter.DrawPart(FVector2D(0.0, 0.0), FVector2D(Size.X * 0.5, Size.Y), 1.f, FVector2D(-Off, 0.0));
			Painter.DrawPart(FVector2D(Size.X * 0.5, 0.0), Size, 1.f, FVector2D(Off, 0.0));
		}
		else
		{
			Painter.DrawPart(FVector2D(0.0, 0.0), FVector2D(Size.X, Size.Y * 0.5), 1.f, FVector2D(0.0, -Off));
			Painter.DrawPart(FVector2D(0.0, Size.Y * 0.5), Size, 1.f, FVector2D(0.0, Off));
		}
		break;
	}

	default:
		Painter.DrawPart(FVector2D::ZeroVector, Size, 1.f);
		break;
	}

	return LayerId;
}
