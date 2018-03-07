// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"

class FCachedWidgetNode;
class FHittestGrid;
class FSlateRect;
class ICustomHitTestPath;
class ILayoutCache;
class SWidget;
struct FGeometry;

/**
 * SWidget::OnPaint and SWidget::Paint use FPaintArgs as their
 * sole parameter in order to ease the burden of passing
 * through multiple fields.
 */
class SLATECORE_API FPaintArgs
{
public:
	FPaintArgs( const SWidget& Parent, FHittestGrid& InHittestGrid, FVector2D InWindowOffset, double InCurrentTime, float InDeltaTime );
	
	FORCEINLINE FPaintArgs WithNewParent(const SWidget* Parent) const
	{
		checkSlow(Parent);
		return WithNewParent(*Parent);
	}

	FORCEINLINE_DEBUGGABLE FPaintArgs WithNewParent(const SWidget& Parent) const
	{
		FPaintArgs Args = FPaintArgs(Parent, this->Grid, this->WindowOffset, this->CurrentTime, this->DeltaTime);
		Args.LastHittestIndex = this->LastHittestIndex;
		Args.LastRecordedVisibility = this->LastRecordedVisibility;
		Args.LayoutCache = this->LayoutCache;
		Args.ParentCacheNode = this->ParentCacheNode;
		Args.bIsCaching = this->bIsCaching;
		Args.bIsVolatilityPass = this->bIsVolatilityPass;

		return Args;
	}

	FPaintArgs EnableCaching(const TWeakPtr<ILayoutCache>& InLayoutCache, FCachedWidgetNode* InParentCacheNode, bool bEnableCaching, bool bEnableVolatility) const;
	FPaintArgs WithNewTime(double InCurrentTime, float InDeltaTime) const;
	FPaintArgs RecordHittestGeometry(const SWidget* Widget, const FGeometry& WidgetGeometry, int32 LayerId) const;
	FPaintArgs InsertCustomHitTestPath( TSharedRef<ICustomHitTestPath> CustomHitTestPath, int32 HitTestIndex ) const;

	FHittestGrid& GetGrid() const { return Grid; }
	int32 GetLastHitTestIndex() const { return LastHittestIndex; }
	EVisibility GetLastRecordedVisibility() const { return LastRecordedVisibility; }
	FVector2D GetWindowToDesktopTransform() const { return WindowOffset; }
	double GetCurrentTime() const { return CurrentTime; }
	float GetDeltaTime() const { return DeltaTime; }
	bool IsCaching() const { return bIsCaching; }
	bool IsVolatilityPass() const { return bIsVolatilityPass; }
	const TWeakPtr<ILayoutCache>& GetLayoutCache() const { return LayoutCache; }
	FCachedWidgetNode* GetParentCacheNode() const { return ParentCacheNode; }

private:
	const SWidget& ParentPtr;
	FHittestGrid& Grid;
	int32 LastHittestIndex;
	EVisibility LastRecordedVisibility;
	FVector2D WindowOffset;
	double CurrentTime;
	float DeltaTime;
	bool bIsCaching;
	bool bIsVolatilityPass;
	TWeakPtr<ILayoutCache> LayoutCache;
	FCachedWidgetNode* ParentCacheNode;
};
