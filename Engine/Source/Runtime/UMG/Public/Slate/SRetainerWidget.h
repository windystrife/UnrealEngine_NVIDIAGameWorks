// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Layout/Visibility.h"
#include "Layout/SlateRect.h"
#include "Layout/Geometry.h"
#include "Input/Events.h"
#include "Layout/ArrangedWidget.h"
#include "Widgets/SWidget.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Children.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"
#include "Input/HittestGrid.h"
#include "Slate/WidgetRenderer.h"

class FArrangedChildren;
class UMaterialInstanceDynamic;
class UMaterialInterface;

DECLARE_MULTICAST_DELEGATE( FOnRetainedModeChanged );

/**
 * The SRetainerWidget renders children widgets to a render target first before
 * later rendering that render target to the screen.  This allows both frequency
 * and phase to be controlled so that the UI can actually render less often than the
 * frequency of the main game render.  It also has the side benefit of allow materials
 * to be applied to the render target after drawing the widgets to apply a simple post process.
 */
class UMG_API SRetainerWidget : public SCompoundWidget, public FGCObject, public ILayoutCache
{
public:
	SLATE_BEGIN_ARGS(SRetainerWidget)
	{
		_Visibility = EVisibility::Visible;
		_Phase = 0;
		_PhaseCount = 1;
		_RenderOnPhase = true;
		_RenderOnInvalidation = false;
	}
	SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(bool, RenderOnPhase)
		SLATE_ARGUMENT(bool, RenderOnInvalidation)
		SLATE_ARGUMENT(int32, Phase)
		SLATE_ARGUMENT(int32, PhaseCount)
		SLATE_ARGUMENT(FName, StatId)
	SLATE_END_ARGS()

	SRetainerWidget();
	~SRetainerWidget();

	/** Constructor */
	void Construct(const FArguments& Args);

	/** Requests that the retainer redraw the hosted content next time it's painted. */
	void RequestRender();

	void SetRetainedRendering(bool bRetainRendering);

	void SetContent(const TSharedRef< SWidget >& InContent);

	UMaterialInstanceDynamic* GetEffectMaterial() const;

	void SetEffectMaterial(UMaterialInterface* EffectMaterial);

	void SetTextureParameter(FName TextureParameter);

	// ILayoutCache overrides
	virtual void InvalidateWidget(SWidget* InvalidateWidget) override;
	virtual FCachedWidgetNode* CreateCacheNode() const override;
	// End ILayoutCache

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// FGCObject

	// SWidget
	virtual FChildren* GetChildren() override;
	virtual bool ComputeVolatility() const override;
	// SWidget

	virtual bool PaintRetainedContent(const FPaintArgs& Args);

	FORCEINLINE const FGeometry& GetCachedAllottedGeometry() const
	{
		return CachedAllottedGeometry;
	}

	void SetWorld(UWorld* World);

protected:
	// BEGIN SLeafWidget interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float Scale) const override;
	// END SLeafWidget interface

	void RefreshRenderingMode();
	bool ShouldBeRenderingOffscreen() const;
	bool IsAnythingVisibleToRender() const;
	void OnRetainerModeChanged();

private:
#if !UE_BUILD_SHIPPING
	static void OnRetainerModeCVarChanged( IConsoleVariable* CVar );
	static FOnRetainedModeChanged OnRetainerModeChangedDelegate;
#endif

	mutable FGeometry CachedAllottedGeometry;
	mutable FVector2D CachedWindowToDesktopTransform;

	FSimpleSlot EmptyChildSlot;

	mutable FSlateBrush SurfaceBrush;

	void UpdateWidgetRenderer();
	mutable TSharedPtr<class FWidgetRenderer> WidgetRenderer;
	mutable class UTextureRenderTarget2D* RenderTarget;
	mutable TSharedPtr<SWidget> MyWidget;

	bool bEnableRetainedRenderingDesire;
	bool bEnableRetainedRendering;
	
	int32 Phase;
	int32 PhaseCount;

	bool RenderOnPhase;
	bool RenderOnInvalidation;

	bool bRenderRequested;

	double LastDrawTime;
	int64 LastTickedFrame;

	TSharedPtr<SVirtualWindow> Window;
	TWeakObjectPtr<UWorld> OuterWorld;

	STAT(TStatId MyStatId;)

	FSlateBrush DynaicBrush;
	UMaterialInstanceDynamic* DynamicEffect;
	FName DynamicEffectTextureParameter;

	mutable FCachedWidgetNode* RootCacheNode;
	mutable TArray< FCachedWidgetNode* > NodePool;
	mutable int32 LastUsedCachedNodeIndex;
};
