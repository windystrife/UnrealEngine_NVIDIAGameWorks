// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Layout/Visibility.h"
#include "Layout/Geometry.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FCachedWidgetNode;
class FPaintArgs;
class FSlateRenderDataHandle;
class FSlateWindowElementList;
class SWindow;

class SLATE_API SInvalidationPanel : public SCompoundWidget, public FGCObject, public ILayoutCache
{
public:
	SLATE_BEGIN_ARGS( SInvalidationPanel )
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
		_CacheRelativeTransforms = false;
	}
		SLATE_DEFAULT_SLOT( FArguments, Content )
		SLATE_ARGUMENT( bool, CacheRelativeTransforms )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
	~SInvalidationPanel();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bool GetCanCache() const;
#else
	FORCEINLINE bool GetCanCache() const { return bCanCache; }
#endif

	void SetCanCache(bool InCanCache);

	FORCEINLINE void InvalidateCache() { bNeedsCaching = true; }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// ILayoutCache overrides
	virtual void InvalidateWidget(SWidget* InvalidateWidget) override;
	virtual FCachedWidgetNode* CreateCacheNode() const override;
	// End ILayoutCache

public:

	// SWidget overrides
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FChildren* GetChildren() override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	virtual bool ComputeVolatility() const override;
	// End SWidget

	void SetContent(const TSharedRef< SWidget >& InContent);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static bool IsInvalidationDebuggingEnabled();
	static void EnableInvalidationDebugging(bool bEnable);

	static bool GetEnableWidgetCaching();
	static void SetEnableWidgetCaching(bool bEnable);
#else
	static bool IsInvalidationDebuggingEnabled() { return false; }
	static void EnableInvalidationDebugging(bool bEnable) { }

	static bool GetEnableWidgetCaching() { return true; }
	static void SetEnableWidgetCaching(bool bEnable) { }
#endif

private:
	TSharedPtr< FSlateWindowElementList > GetNextCachedElementList(const TSharedPtr<SWindow>& CurrentWindow) const;
	void OnGlobalInvalidate();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bool IsCachingNeeded() const;
#else
	FORCEINLINE bool IsCachingNeeded() const { return bNeedsCaching; }
#endif
	

	bool IsCachingNeeded(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect) const;

private:
	mutable FGeometry LastAllottedGeometry;

	FSimpleSlot EmptyChildSlot;
	FVector2D CachedDesiredSize;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	mutable TMap<TWeakPtr<SWidget>, double> InvalidatorWidgets;
#endif

	mutable FCachedWidgetNode* RootCacheNode;
	mutable TSharedPtr< FSlateWindowElementList > CachedWindowElements;
	mutable TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> CachedRenderData;

	mutable TSet<UObject*> CachedResources;
	
	mutable FVector2D CachedAbsolutePosition;

	mutable TArray< FCachedWidgetNode* > NodePool;
	mutable int32 LastUsedCachedNodeIndex;
	mutable int32 LastHitTestIndex;
	mutable FVector2D LastClipRectSize;
	mutable int32 LastClippingIndex;
	mutable int32 LastClippingStateOffset;
	mutable TOptional<FSlateClippingState> LastClippingState;

	mutable int32 CachedMaxChildLayer;
	mutable bool bNeedsCaching;
	mutable bool bIsInvalidating;
	bool bCanCache;

	bool bCacheRelativeTransforms;
	bool bCacheRenderData;
};
