// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Types/PaintArgs.h"
#include "Layout/ArrangedWidget.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetCaching.h"

FPaintArgs::FPaintArgs(const SWidget& Parent, FHittestGrid& InHittestGrid, FVector2D InWindowOffset, double InCurrentTime, float InDeltaTime)
: ParentPtr(Parent)
, Grid(InHittestGrid)
, LastHittestIndex(INDEX_NONE)
, LastRecordedVisibility( EVisibility::Visible )
, WindowOffset(InWindowOffset)
, CurrentTime(InCurrentTime)
, DeltaTime(InDeltaTime)
, bIsCaching(false)
, bIsVolatilityPass(false)
, LayoutCache(nullptr)
, ParentCacheNode(nullptr)
{
}

FPaintArgs FPaintArgs::EnableCaching(const TWeakPtr<ILayoutCache>& InLayoutCache, FCachedWidgetNode* InParentCacheNode, bool bEnableCaching, bool bEnableVolatile) const
{
	FPaintArgs UpdatedArgs(*this);
	UpdatedArgs.LayoutCache = InLayoutCache;
	UpdatedArgs.ParentCacheNode = InParentCacheNode;
	UpdatedArgs.bIsCaching = bEnableCaching;
	UpdatedArgs.bIsVolatilityPass = bEnableVolatile;

	return UpdatedArgs;
}

FPaintArgs FPaintArgs::WithNewTime(double InCurrentTime, float InDeltaTime) const
{
	FPaintArgs UpdatedArgs(*this);
	UpdatedArgs.CurrentTime = InCurrentTime;
	UpdatedArgs.DeltaTime = InDeltaTime;
	return UpdatedArgs;
}

FPaintArgs FPaintArgs::RecordHittestGeometry(const SWidget* Widget, const FGeometry& WidgetGeometry, int32 LayerId) const
{
	FPaintArgs UpdatedArgs(*this);

	if ( LastRecordedVisibility.AreChildrenHitTestVisible() )
	{
		TSharedRef<SWidget> SafeWidget = const_cast<SWidget*>(Widget)->AsShared();

		if ( bIsCaching )
		{
			TSharedPtr<ILayoutCache> SharedLayoutCache = LayoutCache.Pin();
			if (SharedLayoutCache.IsValid())
			{
				FCachedWidgetNode* CacheNode = SharedLayoutCache->CreateCacheNode();
				CacheNode->Initialize(*this, SafeWidget, WidgetGeometry);
				UpdatedArgs.ParentCacheNode->Children.Add(CacheNode);

				UpdatedArgs.ParentCacheNode = CacheNode;
			}
		}

		int32 RealLastHitTestIndex = LastHittestIndex;
		if ( bIsVolatilityPass && !bIsCaching && ParentCacheNode )
		{
			RealLastHitTestIndex = ParentCacheNode->LastRecordedHittestIndex;
			UpdatedArgs.ParentCacheNode = nullptr;
		}

		// When rendering volatile widgets, their parent widgets who have been cached 
		const EVisibility RecordedVisibility = Widget->GetVisibility();
		const int32 RecordedHittestIndex = Grid.InsertWidget(RealLastHitTestIndex, RecordedVisibility, FArrangedWidget(SafeWidget, WidgetGeometry), WindowOffset, LayerId);
		UpdatedArgs.LastHittestIndex = RecordedHittestIndex;
		UpdatedArgs.LastRecordedVisibility = RecordedVisibility;
	}
	else
	{
		UpdatedArgs.LastRecordedVisibility = LastRecordedVisibility;
	}

	return UpdatedArgs;
}

FPaintArgs FPaintArgs::InsertCustomHitTestPath( TSharedRef<ICustomHitTestPath> CustomHitTestPath, int32 InLastHittestIndex ) const
{
	const_cast<FHittestGrid&>(Grid).InsertCustomHitTestPath(CustomHitTestPath, InLastHittestIndex);
	return *this;
}
