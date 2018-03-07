// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "HitProxies.h"
#include "ComponentVisualizer.h"

class AActor;
class FEditorViewportClient;
class FMenuBuilder;
class FPrimitiveDrawInterface;
class FSceneView;
class FUICommandList;
class FViewport;
class SWidget;
class USplineComponent;
struct FViewportClick;

/** Base class for clickable spline editing proxies */
struct HSplineVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HSplineVisProxy(const UActorComponent* InComponent)
	: HComponentVisProxy(InComponent, HPP_Wireframe)
	{}
};

/** Proxy for a spline key */
struct HSplineKeyProxy : public HSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HSplineKeyProxy(const UActorComponent* InComponent, int32 InKeyIndex) 
		: HSplineVisProxy(InComponent)
		, KeyIndex(InKeyIndex)
	{}

	int32 KeyIndex;
};

/** Proxy for a spline segment */
struct HSplineSegmentProxy : public HSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HSplineSegmentProxy(const UActorComponent* InComponent, int32 InSegmentIndex)
		: HSplineVisProxy(InComponent)
		, SegmentIndex(InSegmentIndex)
	{}

	int32 SegmentIndex;
};

/** Proxy for a tangent handle */
struct HSplineTangentHandleProxy : public HSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HSplineTangentHandleProxy(const UActorComponent* InComponent, int32 InKeyIndex, bool bInArriveTangent)
		: HSplineVisProxy(InComponent)
		, KeyIndex(InKeyIndex)
		, bArriveTangent(bInArriveTangent)
	{}

	int32 KeyIndex;
	bool bArriveTangent;
};

/** SplineComponent visualizer/edit functionality */
class COMPONENTVISUALIZERS_API FSplineComponentVisualizer : public FComponentVisualizer
{
public:
	FSplineComponentVisualizer();
	virtual ~FSplineComponentVisualizer();

	//~ Begin FComponentVisualizer Interface
	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;
	virtual bool IsVisualizingArchetype() const override;
	//~ End FComponentVisualizer Interface

	/** Get the spline component we are currently editing */
	USplineComponent* GetEditedSplineComponent() const;

	TSet<int32> GetSelectedKeys() const { return SelectedKeys; }

protected:

	/** Update the key selection state of the visualizer */
	void ChangeSelectionState(int32 Index, bool bIsCtrlHeld);

	/** Duplicates the selected spline key(s) */
	void DuplicateKey();

	void OnDeleteKey();
	bool CanDeleteKey() const;

	void OnDuplicateKey();
	bool IsKeySelectionValid() const;

	void OnAddKey();
	bool CanAddKey() const;

	void OnResetToAutomaticTangent(EInterpCurveMode Mode);
	bool CanResetToAutomaticTangent(EInterpCurveMode Mode) const;

	void OnSetKeyType(EInterpCurveMode Mode);
	bool IsKeyTypeSet(EInterpCurveMode Mode) const;

	void OnSetVisualizeRollAndScale();
	bool IsVisualizingRollAndScale() const;

	void OnSetDiscontinuousSpline();
	bool IsDiscontinuousSpline() const;

	void OnResetToDefault();
	bool CanResetToDefault() const;

	/** Generate the submenu containing the available point types */
	void GenerateSplinePointTypeSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Generate the submenu containing the available auto tangent types */
	void GenerateTangentTypeSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Output log commands */
	TSharedPtr<FUICommandList> SplineComponentVisualizerActions;

	/** Actor that owns the currently edited spline */
	TWeakObjectPtr<AActor> SplineOwningActor;

	/** Name of property on the actor that references the spline we are editing */
	FPropertyNameAndIndex SplineCompPropName;

	/** Index of keys we have selected */
	TSet<int32> SelectedKeys;

	/** Index of the last key we selected */
	int32 LastKeyIndexSelected;

	/** Index of segment we have selected */
	int32 SelectedSegmentIndex;

	/** Index of tangent handle we have selected */
	int32 SelectedTangentHandle;

	struct ESelectedTangentHandle
	{
		enum Type
		{
			None,
			Leave,
			Arrive
		};
	};

	/** The type of the selected tangent handle */
	ESelectedTangentHandle::Type SelectedTangentHandleType;

	/** Position on spline we have selected */
	FVector SelectedSplinePosition;

	/** Cached rotation for this point */
	FQuat CachedRotation;

	/** Whether we currently allow duplication when dragging */
	bool bAllowDuplication;

private:
	UProperty* SplineCurvesProperty;
};
