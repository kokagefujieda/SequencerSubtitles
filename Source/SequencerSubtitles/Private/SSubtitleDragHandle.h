// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

DECLARE_DELEGATE_OneParam(FOnSubtitleDragFinished, FVector2D /* NewOffset */);

/**
 * Editor-only wrapper widget that enables mouse-drag positioning of subtitle content.
 * Wraps the ContentVerticalBox and translates it via RenderTransform during drag.
 * Automatically disables drag during PIE. Clamps to viewport bounds.
 */
class SSubtitleDragHandle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSubtitleDragHandle) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			InArgs._Content.Widget
		];

		SetVisibility(EVisibility::Visible); // hit-testable
	}

	void SetCurrentOffset(FVector2D InOffset)
	{
		CurrentOffset = InOffset;
	}

	void SetOnDragFinished(FOnSubtitleDragFinished InDelegate)
	{
		OnDragFinished = InDelegate;
	}

	void SetDragEnabled(bool bEnabled)
	{
		bDragEnabled = bEnabled;
	}

	/** Store a reference to the viewport overlay for clamp bounds calculation. */
	void SetViewportWidget(TSharedPtr<SWidget> InWidget)
	{
		ViewportWidget = InWidget;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		if (!IsDragAllowed())
		{
			ShowPIEWarning();
			return FReply::Handled(); // consume to prevent pass-through
		}

		bDragging = true;
		DragStartMousePos = MouseEvent.GetScreenSpacePosition();
		DragStartOffset = CurrentOffset;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bDragging && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			bDragging = false;
			OnDragFinished.ExecuteIfBound(CurrentOffset);
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bDragging)
		{
			// Compute delta in screen pixels, then convert to Slate local space
			const FVector2D ScreenDelta = MouseEvent.GetScreenSpacePosition() - DragStartMousePos;

			const float Scale = MyGeometry.GetAccumulatedLayoutTransform().GetScale();
			const FVector2D LocalDelta = (Scale > KINDA_SMALL_NUMBER) ? (ScreenDelta / Scale) : ScreenDelta;

			CurrentOffset = ClampToViewport(DragStartOffset + LocalDelta, MyGeometry);

			// Live preview: apply RenderTransform to child content
			ChildSlot.GetWidget()->SetRenderTransform(FSlateRenderTransform(CurrentOffset));

			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		if (!IsDragAllowed())
		{
			return FCursorReply::Cursor(EMouseCursor::SlashedCircle);
		}
		if (bDragEnabled)
		{
			return FCursorReply::Cursor(EMouseCursor::CardinalCross);
		}
		return FCursorReply::Unhandled();
	}

private:
	/** Returns true if dragging is allowed (not during PIE/SIE). */
	bool IsDragAllowed() const
	{
		if (!bDragEnabled)
		{
			return false;
		}

		// Check all world contexts for an active PIE or Simulate session
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
				{
					return false;
				}
			}
		}

		return true;
	}

	/**
	 * Clamp the proposed offset so the subtitle center stays within the viewport.
	 * Uses center-based clamping to allow free movement even when the widget
	 * fills the overlay width (HAlign_Fill / Center mode).
	 */
	FVector2D ClampToViewport(FVector2D ProposedOffset, const FGeometry& HandleGeometry) const
	{
		TSharedPtr<SWidget> VP = ViewportWidget.Pin();
		if (!VP.IsValid())
		{
			return ProposedOffset;
		}

		const float Scale = HandleGeometry.GetAccumulatedLayoutTransform().GetScale();
		if (Scale < KINDA_SMALL_NUMBER)
		{
			return ProposedOffset;
		}

		// Absolute-space bounds of the viewport overlay (fills the entire viewport)
		const FGeometry VPGeom = VP->GetPaintSpaceGeometry();
		const FVector2D VPAbsPos = VPGeom.GetAbsolutePosition();
		const FVector2D VPAbsSize = VPGeom.GetAbsoluteSize();

		if (VPAbsSize.X < 1.0f || VPAbsSize.Y < 1.0f)
		{
			return ProposedOffset; // not yet painted
		}

		// DragHandle's layout position and size in absolute space
		const FVector2D HandleAbsPos = HandleGeometry.GetAbsolutePosition();
		const FVector2D HandleAbsSize = HandleGeometry.GetAbsoluteSize();

		// Compute where the content CENTER would be with the proposed offset
		FVector2D CenterAbsPos = HandleAbsPos + HandleAbsSize * 0.5 + ProposedOffset * Scale;

		// Clamp the center to stay within the viewport bounds
		CenterAbsPos.X = FMath::Clamp(CenterAbsPos.X, VPAbsPos.X, VPAbsPos.X + VPAbsSize.X);
		CenterAbsPos.Y = FMath::Clamp(CenterAbsPos.Y, VPAbsPos.Y, VPAbsPos.Y + VPAbsSize.Y);

		// Back-solve to local offset
		return (CenterAbsPos - HandleAbsPos - HandleAbsSize * 0.5) / Scale;
	}

	/** Show a brief notification that dragging is disabled during PIE. */
	void ShowPIEWarning()
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastWarningTime < 3.0)
		{
			return;
		}
		LastWarningTime = Now;

		FNotificationInfo Info(FText::FromString(TEXT("Subtitle drag is disabled during Play-In-Editor")));
		Info.ExpireDuration = 2.0f;
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	bool bDragEnabled = true;
	bool bDragging = false;
	FVector2D DragStartMousePos = FVector2D::ZeroVector;
	FVector2D DragStartOffset = FVector2D::ZeroVector;
	FVector2D CurrentOffset = FVector2D::ZeroVector;
	FOnSubtitleDragFinished OnDragFinished;
	TWeakPtr<SWidget> ViewportWidget;
	double LastWarningTime = 0.0;
};

#endif // WITH_EDITOR
