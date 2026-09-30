// Copyright 2026 kokage. All Rights Reserved.

// Image Track support of USubtitleSubsystem: images share the subtitle overlay (and its DPI handling)
// and are drawn on layers behind or in front of the subtitles. Everything is driven by the section time,
// so scrubbing, reverse playback and Movie Render Queue give the same result.

#include "SubtitleSubsystem.h"
#include "SSeqImage.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Engine/Texture2D.h"

#if WITH_EDITOR
#include "SSubtitleDragHandle.h"
#include "SeqImageSection.h"
#include "ScopedTransaction.h"
#endif

namespace SeqImage
{
	/** Map 0..1 through an easing curve. Back / Elastic may overshoot 1. */
	static float Ease(ESeqImageEasing Easing, float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		switch (Easing)
		{
		case ESeqImageEasing::EaseIn:    return FMath::InterpEaseIn(0.f, 1.f, T, 2.f);
		case ESeqImageEasing::EaseOut:   return FMath::InterpEaseOut(0.f, 1.f, T, 2.f);
		case ESeqImageEasing::EaseInOut: return FMath::InterpEaseInOut(0.f, 1.f, T, 2.f);
		case ESeqImageEasing::Back:
		{
			constexpr float C1 = 1.70158f;
			constexpr float C3 = C1 + 1.f;
			const float U = T - 1.f;
			return 1.f + C3 * U * U * U + C1 * U * U;
		}
		case ESeqImageEasing::Bounce:
		{
			constexpr float N1 = 7.5625f;
			constexpr float D1 = 2.75f;
			if (T < 1.f / D1)   { return N1 * T * T; }
			if (T < 2.f / D1)   { T -= 1.5f / D1;   return N1 * T * T + 0.75f; }
			if (T < 2.5f / D1)  { T -= 2.25f / D1;  return N1 * T * T + 0.9375f; }
			T -= 2.625f / D1;
			return N1 * T * T + 0.984375f;
		}
		case ESeqImageEasing::Elastic:
		{
			if (T <= 0.f || T >= 1.f) { return T; }
			constexpr float C4 = UE_TWO_PI / 3.f;
			return FMath::Pow(2.f, -10.f * T) * FMath::Sin((T * 10.f - 0.75f) * C4) + 1.f;
		}
		default:
			return T;
		}
	}

	/** Unit vector pointing from the image toward a side. */
	static FVector2D DirectionVector(ESeqImageDirection Direction)
	{
		switch (Direction)
		{
		case ESeqImageDirection::Left:  return FVector2D(-1.0, 0.0);
		case ESeqImageDirection::Right: return FVector2D(1.0, 0.0);
		case ESeqImageDirection::Top:   return FVector2D(0.0, -1.0);
		default:                        return FVector2D(0.0, 1.0);
		}
	}

	/** Displayed size of the image in Slate units. */
	static FVector2D ComputeImageSize(const FSeqImageLayout& Layout, const FVector2D& TextureSize, const FVector2D& ScreenSize)
	{
		if (TextureSize.X <= 0.0 || TextureSize.Y <= 0.0) { return FVector2D::ZeroVector; }

		const double Aspect = TextureSize.X / TextureSize.Y;
		FVector2D Size = TextureSize;

		switch (Layout.SizeMode)
		{
		case ESeqImageSizeMode::Fixed:
			Size = Layout.FixedSize;
			if (Size.X <= 0.0 && Size.Y <= 0.0) { Size = TextureSize; }
			else if (Size.X <= 0.0)            { Size.X = Size.Y * Aspect; }
			else if (Size.Y <= 0.0)            { Size.Y = Size.X / Aspect; }
			break;
		case ESeqImageSizeMode::ScreenWidthPercent:
			Size.X = ScreenSize.X * Layout.ScreenPercent / 100.0;
			Size.Y = Size.X / Aspect;
			break;
		case ESeqImageSizeMode::ScreenHeightPercent:
			Size.Y = ScreenSize.Y * Layout.ScreenPercent / 100.0;
			Size.X = Size.Y * Aspect;
			break;
		case ESeqImageSizeMode::FitScreen:
			Size = TextureSize * FMath::Min(ScreenSize.X / TextureSize.X, ScreenSize.Y / TextureSize.Y);
			break;
		case ESeqImageSizeMode::FillScreen:
			Size = TextureSize * FMath::Max(ScreenSize.X / TextureSize.X, ScreenSize.Y / TextureSize.Y);
			break;
		default:
			break;
		}

		return Size * FMath::Max(Layout.Scale, 0.f);
	}

	/** Apply an entrance / exit effect at Alpha (0 = hidden, 1 = shown). */
	static void ApplyTransition(const FSeqImageTransition& Transition, float Alpha,
		const FVector2D& ScreenSize, const FVector2D& ImageSize,
		FSeqImageDrawState& DrawState, FVector2D& Translate, FVector2D& Scale, float& AngleDeg, float& Opacity)
	{
		const float Alpha01 = FMath::Clamp(Alpha, 0.f, 1.f);
		const bool  bHorizontal = Transition.Axis == ESeqImageAxis::Horizontal;

		if (Transition.bFade && Transition.Effect != ESeqImageEffect::Fade)
		{
			Opacity *= Alpha01;
		}

		switch (Transition.Effect)
		{
		case ESeqImageEffect::Fade:
			Opacity *= Alpha01;
			break;

		case ESeqImageEffect::Slide:
		{
			// Distance 0 = start completely outside the screen
			const FVector2D Dir = DirectionVector(Transition.Direction);
			const double Dist = Transition.Distance > 0.f
				? Transition.Distance
				: (Dir.X != 0.0 ? ScreenSize.X + ImageSize.X : ScreenSize.Y + ImageSize.Y);
			Translate += Dir * Dist * (1.0 - Alpha);
			break;
		}

		case ESeqImageEffect::Split:
		{
			const double Dist = Transition.Distance > 0.f
				? Transition.Distance
				: (bHorizontal ? ScreenSize.X + ImageSize.X : ScreenSize.Y + ImageSize.Y) * 0.5;
			DrawState.MaskEffect  = ESeqImageEffect::Split;
			DrawState.Axis        = Transition.Axis;
			DrawState.SplitOffset = static_cast<float>(Dist * (1.0 - Alpha));
			break;
		}

		case ESeqImageEffect::Wipe:
		case ESeqImageEffect::BarnDoor:
		case ESeqImageEffect::Iris:
		case ESeqImageEffect::Blinds:
			DrawState.MaskEffect    = Transition.Effect;
			DrawState.Progress      = Alpha;
			DrawState.Direction     = Transition.Direction;
			DrawState.Axis          = Transition.Axis;
			DrawState.Softness      = Transition.Softness;
			DrawState.BlindsCount   = Transition.BlindsCount;
			DrawState.BlindsStagger = Transition.BlindsStagger;
			break;

		case ESeqImageEffect::Zoom:
			Scale *= FMath::Lerp(static_cast<double>(Transition.StartScale), 1.0, static_cast<double>(Alpha));
			break;

		case ESeqImageEffect::Rotate:
			AngleDeg += Transition.Angle * (1.f - Alpha);
			Scale *= FMath::Lerp(static_cast<double>(Transition.StartScale), 1.0, static_cast<double>(Alpha));
			break;

		case ESeqImageEffect::Flip:
			if (bHorizontal) { Scale.X *= Alpha; }
			else             { Scale.Y *= Alpha; }
			break;

		default:
			break;
		}
	}

	/** Apply the effect that runs for the whole section. */
	static void ApplyContinuous(const FSeqImageContinuous& Continuous, float LocalTime, float Duration,
		FVector2D& Translate, FVector2D& Scale)
	{
		const float Phase = LocalTime * Continuous.Frequency * UE_TWO_PI;

		switch (Continuous.Effect)
		{
		case ESeqImageContinuousEffect::Tremble:
			// Same shape as the subtitle tremble: sin / cos with slightly different frequencies
			Translate += FVector2D(FMath::Sin(Phase), FMath::Cos(Phase * 1.3f)) * Continuous.Amplitude;
			break;
		case ESeqImageContinuousEffect::Float:
			Translate.Y += FMath::Sin(Phase) * Continuous.Amplitude;
			break;
		case ESeqImageContinuousEffect::Pulse:
			Scale *= 1.0 + FMath::Sin(Phase) * Continuous.PulseScale;
			break;
		case ESeqImageContinuousEffect::KenBurns:
		{
			const float U = Duration > 0.f ? FMath::Clamp(LocalTime / Duration, 0.f, 1.f) : 0.f;
			Scale *= 1.0 + Continuous.KenBurnsZoom * U;
			Translate += Continuous.KenBurnsPan * U;
			break;
		}
		default:
			break;
		}
	}
}

// ---------------------------------------------------------------------------
// UpdateImage / RemoveImage
// ---------------------------------------------------------------------------

void USubtitleSubsystem::UpdateImage(uint32 SlotID, const FSeqImageParams& Params, float LocalTime, float Duration,
	UMovieSceneSeqImageSection* SourceSection)
{
	// Outside the section (e.g. nearest-section evaluation in a gap): nothing to show
	if (Duration <= 0.f || LocalTime < 0.f || LocalTime >= Duration)
	{
		RemoveImage(SlotID);
		return;
	}

	EnsureSlateWidgets();
	AddToViewport();

	TSharedPtr<FSeqImageSlot>& SlotPtr = ActiveImages.FindOrAdd(SlotID);
	if (!SlotPtr.IsValid())
	{
		SlotPtr = MakeShared<FSeqImageSlot>();
		CreateImageSlotWidget(SlotID, *SlotPtr);
	}
	FSeqImageSlot& Slot = *SlotPtr;

	Slot.Params = Params;
#if WITH_EDITOR
	Slot.Section = SourceSection;
#endif

	// Load the texture only when it changes
	const FSoftObjectPath TexturePath = Params.Image.ToSoftObjectPath();
	if (TexturePath != Slot.TexturePath)
	{
		Slot.TexturePath = TexturePath;
		UTexture2D* Texture = Params.Image.LoadSynchronous();
		if (Texture) { ImageTextures.Add(SlotID, Texture); }
		else         { ImageTextures.Remove(SlotID); }
		Slot.ImageWidget->SetTexture(Texture);
	}

	PlaceImageSlot(Slot);
	ApplyImageState(Slot, LocalTime, Duration);
	UpdateOverlayVisibility();
}

void USubtitleSubsystem::RemoveImage(uint32 SlotID)
{
	TSharedPtr<FSeqImageSlot> Slot;
	if (!ActiveImages.RemoveAndCopyValue(SlotID, Slot)) { return; }

	if (Slot.IsValid() && Slot->RootWidget.IsValid() && Slot->LayerSlot)
	{
		const TSharedPtr<SOverlay> Layer = Slot->bInFront ? ImageLayerFront : ImageLayerBack;
		if (Layer.IsValid())
		{
			Layer->RemoveSlot(Slot->RootWidget.ToSharedRef());
		}
	}

	ImageTextures.Remove(SlotID);
	UpdateOverlayVisibility();
}

// ---------------------------------------------------------------------------
// Image slot widgets
// ---------------------------------------------------------------------------

void USubtitleSubsystem::CreateImageSlotWidget(uint32 SlotID, FSeqImageSlot& Slot)
{
	Slot.ImageWidget = SNew(SSeqImage);
	Slot.OffsetBox = SNew(SBox)
		[
			Slot.ImageWidget.ToSharedRef()
		];
	Slot.RootWidget = Slot.OffsetBox;

#if WITH_EDITOR
	// Editor viewport: the image can be dragged to set its offset
	if (bIsEditorViewport)
	{
		Slot.DragHandle = SNew(SSubtitleDragHandle)
			[
				Slot.OffsetBox.ToSharedRef()
			];
		Slot.DragHandle->SetViewportWidget(WidgetOverlay);
		Slot.DragHandle->SetOnDragFinished(FOnSubtitleDragFinished::CreateWeakLambda(this,
			[this, SlotID](FVector2D NewOffset)
			{
				OnImageDragFinished(SlotID, NewOffset);
			}
		));
		Slot.RootWidget = Slot.DragHandle;
		return;
	}
#endif

	// In game, images never take mouse input
	Slot.RootWidget->SetVisibility(EVisibility::HitTestInvisible);
}

void USubtitleSubsystem::PlaceImageSlot(FSeqImageSlot& Slot)
{
	const FSeqImageLayout& Layout = Slot.Params.Layout;
	const bool  bInFront = Layout.Layer == ESeqImageLayer::InFrontOfSubtitles;
	const int32 ZOrder   = FMath::Max(Layout.ZOrder, 0);

	const TSharedPtr<SOverlay> Layer = bInFront ? ImageLayerFront : ImageLayerBack;
	if (!Layer.IsValid() || !Slot.RootWidget.IsValid()) { return; }

	// (Re)insert when the layer or draw order changes
	const bool bNewlyPlaced = !Slot.LayerSlot || Slot.bInFront != bInFront || Slot.ZOrder != ZOrder;
	if (bNewlyPlaced)
	{
		if (Slot.LayerSlot)
		{
			const TSharedPtr<SOverlay> OldLayer = Slot.bInFront ? ImageLayerFront : ImageLayerBack;
			if (OldLayer.IsValid())
			{
				OldLayer->RemoveSlot(Slot.RootWidget.ToSharedRef());
			}
			Slot.LayerSlot = nullptr;
		}

		Layer->AddSlot(ZOrder)
			.Expose(Slot.LayerSlot)
			[
				Slot.RootWidget.ToSharedRef()
			];
		Slot.bInFront = bInFront;
		Slot.ZOrder   = ZOrder;
	}

	if (!Slot.LayerSlot) { return; }

	if (bNewlyPlaced || Slot.Anchor != Layout.Anchor)
	{
		Slot.Anchor = Layout.Anchor;

		EHorizontalAlignment HAlign = HAlign_Center;
		EVerticalAlignment   VAlign = VAlign_Center;
		switch (Layout.Anchor)
		{
		case ESeqImageAnchor::TopLeft:      HAlign = HAlign_Left;   VAlign = VAlign_Top;    break;
		case ESeqImageAnchor::TopCenter:    HAlign = HAlign_Center; VAlign = VAlign_Top;    break;
		case ESeqImageAnchor::TopRight:     HAlign = HAlign_Right;  VAlign = VAlign_Top;    break;
		case ESeqImageAnchor::CenterLeft:   HAlign = HAlign_Left;   VAlign = VAlign_Center; break;
		case ESeqImageAnchor::CenterRight:  HAlign = HAlign_Right;  VAlign = VAlign_Center; break;
		case ESeqImageAnchor::BottomLeft:   HAlign = HAlign_Left;   VAlign = VAlign_Bottom; break;
		case ESeqImageAnchor::BottomCenter: HAlign = HAlign_Center; VAlign = VAlign_Bottom; break;
		case ESeqImageAnchor::BottomRight:  HAlign = HAlign_Right;  VAlign = VAlign_Bottom; break;
		default: break;
		}
		Slot.LayerSlot->SetHorizontalAlignment(HAlign);
		Slot.LayerSlot->SetVerticalAlignment(VAlign);
	}
}

void USubtitleSubsystem::ApplyImageState(FSeqImageSlot& Slot, float LocalTime, float Duration)
{
	const FSeqImageParams& P = Slot.Params;

	FVector2D ScreenSize = GetViewportSlateSize();
	if (ScreenSize.X <= 0.0 || ScreenSize.Y <= 0.0)
	{
		ScreenSize = FVector2D(1920.0, 1080.0);
	}
	const FVector2D ImageSize = SeqImage::ComputeImageSize(P.Layout, Slot.ImageWidget->GetTextureSize(), ScreenSize);

	FSeqImageDrawState DrawState;
	DrawState.DrawSize = ImageSize;

	FVector2D Translate = FVector2D::ZeroVector;
	FVector2D Scale(1.0, 1.0);
	float     AngleDeg = 0.f;
	float     Opacity  = P.Layout.Opacity;

	// --- Entrance / exit (the exit finishes at the section end) ---
	const FSeqImageTransition& ExitTransition = P.bOverrideExit ? P.Exit : P.Entrance;
	float InDuration  = (P.Entrance.Effect != ESeqImageEffect::None)      ? FMath::Max(P.Entrance.Duration, 0.f)      : 0.f;
	float OutDuration = (ExitTransition.Effect != ESeqImageEffect::None) ? FMath::Max(ExitTransition.Duration, 0.f) : 0.f;
	if (InDuration + OutDuration > Duration)
	{
		// Section too short for both: shorten them proportionally
		const float Ratio = Duration / (InDuration + OutDuration);
		InDuration  *= Ratio;
		OutDuration *= Ratio;
	}

	if (InDuration > 0.f && LocalTime < InDuration)
	{
		const float Alpha = SeqImage::Ease(P.Entrance.Easing, LocalTime / InDuration);
		SeqImage::ApplyTransition(P.Entrance, Alpha, ScreenSize, ImageSize, DrawState, Translate, Scale, AngleDeg, Opacity);
	}
	else if (OutDuration > 0.f && LocalTime > Duration - OutDuration)
	{
		const float U = (LocalTime - (Duration - OutDuration)) / OutDuration;
		// Default exit = the entrance played backwards; an overridden exit uses its own easing forward
		const float Alpha = P.bOverrideExit
			? 1.f - SeqImage::Ease(ExitTransition.Easing, U)
			: SeqImage::Ease(ExitTransition.Easing, 1.f - U);
		SeqImage::ApplyTransition(ExitTransition, Alpha, ScreenSize, ImageSize, DrawState, Translate, Scale, AngleDeg, Opacity);
	}

	// --- Continuous effect ---
	SeqImage::ApplyContinuous(P.Continuous, LocalTime, Duration, Translate, Scale);

	// --- Mirroring ---
	if (P.Layout.bFlipX) { Scale.X = -Scale.X; }
	if (P.Layout.bFlipY) { Scale.Y = -Scale.Y; }

	DrawState.Color = P.Layout.Tint;
	DrawState.Color.A *= FMath::Clamp(Opacity, 0.f, 1.f);
	Slot.ImageWidget->SetDrawState(DrawState);

	// Scale, then rotate, around the image center, then move (row-vector matrix: v * S * R)
	const float Rad = FMath::DegreesToRadians(AngleDeg);
	const float Cos = FMath::Cos(Rad);
	const float Sin = FMath::Sin(Rad);
	const float Sx  = static_cast<float>(Scale.X);
	const float Sy  = static_cast<float>(Scale.Y);
	Slot.ImageWidget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Slot.ImageWidget->SetRenderTransform(FSlateRenderTransform(
		FMatrix2x2(Sx * Cos, Sx * Sin, -Sy * Sin, Sy * Cos),
		FVector2f(Translate)));

	// --- Offset and motion ---
	FVector2D Offset = P.Layout.Offset;
	Slot.MotionAlpha = 0.f;
	if (P.Motion.bEnabled)
	{
		const float MotionDuration = P.Motion.Duration > 0.f ? P.Motion.Duration : (Duration - P.Motion.StartTime);
		const float U = MotionDuration > 0.f
			? FMath::Clamp((LocalTime - P.Motion.StartTime) / MotionDuration, 0.f, 1.f)
			: 1.f;
		Slot.MotionAlpha = SeqImage::Ease(P.Motion.Easing, U);
		Offset = FMath::Lerp(P.Layout.Offset, P.Motion.EndOffset, static_cast<double>(Slot.MotionAlpha));
	}

#if WITH_EDITOR
	if (Slot.DragHandle.IsValid())
	{
		// While dragging, the handle moves the image itself
		if (Slot.DragHandle->IsDragging()) { return; }
		Slot.DragHandle->SetCurrentOffset(Offset);
	}
#endif

	Slot.OffsetBox->SetRenderTransform(FSlateRenderTransform(FVector2f(Offset)));
}

// ---------------------------------------------------------------------------
// Editor: drag to position
// ---------------------------------------------------------------------------

#if WITH_EDITOR
void USubtitleSubsystem::OnImageDragFinished(uint32 SlotID, FVector2D NewOffset)
{
	TSharedPtr<FSeqImageSlot>* Found = ActiveImages.Find(SlotID);
	if (!Found || !Found->IsValid()) { return; }
	FSeqImageSlot& Slot = **Found;

	FSeqImageParams& P = Slot.Params;
	FVector2D NewStart = P.Layout.Offset;
	FVector2D NewEnd   = P.Motion.EndOffset;

	if (!P.Motion.bEnabled)
	{
		NewStart = NewOffset;
	}
	else
	{
		// Keep the image where it was dropped at the current time: solve Lerp(Start, End, A) = NewOffset
		// for the end of the motion the current time is closer to
		const double A = Slot.MotionAlpha;
		if (A < 0.5)
		{
			NewStart = (NewOffset - P.Motion.EndOffset * A) / (1.0 - A);
		}
		else
		{
			NewEnd = (NewOffset - P.Layout.Offset * (1.0 - A)) / A;
		}
	}

	P.Layout.Offset    = NewStart;
	P.Motion.EndOffset = NewEnd;

	if (UMovieSceneSeqImageSection* Section = Slot.Section.Get())
	{
		const FScopedTransaction Transaction(NSLOCTEXT("SequencerSubtitles", "DragImage", "Move Image"));
		Section->Modify();
		Section->Layout.Offset    = NewStart;
		Section->Motion.EndOffset = NewEnd;
	}

	Slot.OffsetBox->SetRenderTransform(FSlateRenderTransform(FVector2f(NewOffset)));
}
#endif
