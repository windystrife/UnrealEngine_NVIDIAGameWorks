// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Components/ActorComponent.h"
#include "HitProxies.h"

class AActor;
class FCanvas;
class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
class SWidget;
struct FViewportClick;

struct HComponentVisProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(UNREALED_API);

	HComponentVisProxy(const UActorComponent* InComponent, EHitProxyPriority InPriority = HPP_Wireframe) 
	: HHitProxy(InPriority)
	, Component(InComponent)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	TWeakObjectPtr<const UActorComponent> Component;
};


/** Base class for a component visualizer, that draw editor information for a particular component class */
class UNREALED_API FComponentVisualizer : public TSharedFromThis<FComponentVisualizer>
{
public:
	FComponentVisualizer() {}
	virtual ~FComponentVisualizer() {}

	/** */
	virtual void OnRegister() {}
	/** Draw visualization for the supplied component */
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) {}
	/** Draw HUD on viewport for the supplied component */
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) {}
	/** */
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) { return false; }
	/** */
	virtual void EndEditing() {}
	/** */
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const { return false; }
	/** */
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const { return false; }
	/** */
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltalRotate, FVector& DeltaScale) { return false; }
	/** */
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient,FViewport* Viewport,FKey Key,EInputEvent Event) { return false; }
	/** */
	virtual TSharedPtr<SWidget> GenerateContextMenu() const { return TSharedPtr<SWidget>(); }
	/** */
	virtual bool IsVisualizingArchetype() const { return false; }

	struct FPropertyNameAndIndex
	{
		FPropertyNameAndIndex()
			: Name(NAME_None)
			, Index(INDEX_NONE)
		{}

		explicit FPropertyNameAndIndex(FName InName, int32 InIndex = 0)
			: Name(InName)
			, Index(InIndex)
		{}

		bool IsValid() const { return Name != NAME_None && Index != INDEX_NONE; }

		void Clear()
		{
			Name = NAME_None;
			Index = INDEX_NONE;
		}

		FName Name;
		int32 Index;
	};

	/** Find the name of the property that points to this component */
	static FPropertyNameAndIndex GetComponentPropertyName(const UActorComponent* Component);

	/** Get a component pointer from the property name */
	static UActorComponent* GetComponentFromPropertyName(const AActor* CompOwner, const FPropertyNameAndIndex& Property);

	/** Notify that a component property has been modified */
	static void NotifyPropertyModified(UActorComponent* Component, UProperty* Property);

	/** Notify that many component properties have been modified */
	static void NotifyPropertiesModified(UActorComponent* Component, const TArray<UProperty*>& Properties);
};

struct FCachedComponentVisualizer
{
	TWeakObjectPtr<UActorComponent> Component;
	TSharedPtr<FComponentVisualizer> Visualizer;
	
	FCachedComponentVisualizer(UActorComponent* InComponent, TSharedPtr<FComponentVisualizer>& InVisualizer)
		: Component(InComponent)
		, Visualizer(InVisualizer)
	{}
};
