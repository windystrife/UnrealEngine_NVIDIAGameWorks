// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Tools/SequencerEditTool.h"
#include "Tools/DelayedDrag.h"

class SSequencer;
struct ISequencerHotspot;

class FSequencerEditTool_Movement
	: public FSequencerEditTool
{
public:

	/** Static identifier for this edit tool */
	static const FName Identifier;

	/** Create and initialize a new instance. */
	FSequencerEditTool_Movement(FSequencer& InSequencer);

public:

	// ISequencerEditTool interface

	virtual FReply OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost() override;
	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FName GetIdentifier() const override;
	virtual bool CanDeactivate() const override;
	virtual const ISequencerHotspot* GetDragHotspot() const override;

protected:

	FString TimeToString(float Time, bool IsDelta) const;

private:

	TSharedPtr<ISequencerEditToolDragOperation> CreateDrag(const FPointerEvent& MouseEvent);

	bool GetHotspotTime(float& HotspotTime) const;
	float GetHotspotOffsetTime(float CurrentTime) const;

	struct FDelayedDrag_Hotspot : FDelayedDrag
	{
		FDelayedDrag_Hotspot(FVector2D InInitialPosition, FKey InApplicableKey, TSharedPtr<ISequencerHotspot> InHotspot)
			: FDelayedDrag(InInitialPosition, InApplicableKey)
			, Hotspot(MoveTemp(InHotspot))
		{ }

		TSharedPtr<ISequencerHotspot> Hotspot;
	};

	/** Helper class responsible for handling delayed dragging */
	TOptional<FDelayedDrag_Hotspot> DelayedDrag;

	/** Current drag operation if any */
	TSharedPtr<ISequencerEditToolDragOperation> DragOperation;

	/** Current local position the mouse is dragged to. */
	FVector2D DragPosition;

	/** The hotspot's time before dragging started. */
	float OriginalHotspotTime;
};
