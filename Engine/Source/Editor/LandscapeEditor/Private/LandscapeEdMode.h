// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UnrealWidget.h"
#include "LandscapeProxy.h"
#include "EdMode.h"
#include "LandscapeToolInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeGizmoActiveActor.h"

class ALandscape;
class FCanvas;
class FEditorViewportClient;
class FLandscapeToolSplines;
class FPrimitiveDrawInterface;
class FSceneView;
class FUICommandList;
class FViewport;
class ULandscapeComponent;
class ULandscapeEditorObject;
class UViewportInteractor;
struct FHeightmapToolTarget;
struct FViewportActionKeyInput;
struct FViewportClick;
template<class ToolTarget> class FLandscapeToolCopyPaste;

DECLARE_LOG_CATEGORY_EXTERN(LogLandscapeEdMode, Log, All);

// Forward declarations
class ULandscapeEditorObject;
class ULandscapeLayerInfoObject;
class FLandscapeToolSplines;
class UViewportInteractor;
struct FViewportActionKeyInput;

struct FHeightmapToolTarget;
template<typename TargetType> class FLandscapeToolCopyPaste;

struct FLandscapeToolMode
{
	const FName				ToolModeName;
	int32					SupportedTargetTypes; // ELandscapeToolTargetTypeMask::Type

	TArray<FName>			ValidTools;
	FName					CurrentToolName;

	FLandscapeToolMode(FName InToolModeName, int32 InSupportedTargetTypes)
		: ToolModeName(InToolModeName)
		, SupportedTargetTypes(InSupportedTargetTypes)
	{
	}
};

struct FLandscapeTargetListInfo
{
	FText TargetName;
	ELandscapeToolTargetType::Type TargetType;
	TWeakObjectPtr<ULandscapeInfo> LandscapeInfo;

	//Values cloned from FLandscapeLayerStruct LayerStruct;			// ignored for heightmap
	TWeakObjectPtr<ULandscapeLayerInfoObject> LayerInfoObj;			// ignored for heightmap
	FName LayerName;												// ignored for heightmap
	TWeakObjectPtr<class ALandscapeProxy> Owner;					// ignored for heightmap
	TWeakObjectPtr<class UMaterialInstanceConstant> ThumbnailMIC;	// ignored for heightmap
	int32 DebugColorChannel;										// ignored for heightmap
	uint32 bValid : 1;												// ignored for heightmap

	FLandscapeTargetListInfo(FText InTargetName, ELandscapeToolTargetType::Type InTargetType, const FLandscapeInfoLayerSettings& InLayerSettings)
		: TargetName(InTargetName)
		, TargetType(InTargetType)
		, LandscapeInfo(InLayerSettings.Owner->GetLandscapeInfo())
		, LayerInfoObj(InLayerSettings.LayerInfoObj)
		, LayerName(InLayerSettings.LayerName)
		, Owner(InLayerSettings.Owner)
		, ThumbnailMIC(InLayerSettings.ThumbnailMIC)
		, DebugColorChannel(InLayerSettings.DebugColorChannel)
		, bValid(InLayerSettings.bValid)
	{
	}

	FLandscapeTargetListInfo(FText InTargetName, ELandscapeToolTargetType::Type InTargetType, ULandscapeInfo* InLandscapeInfo)
		: TargetName(InTargetName)
		, TargetType(InTargetType)
		, LandscapeInfo(InLandscapeInfo)
		, LayerInfoObj(NULL)
		, LayerName(NAME_None)
		, Owner(NULL)
		, ThumbnailMIC(NULL)
		, bValid(true)
	{
	}

	FLandscapeInfoLayerSettings* GetLandscapeInfoLayerSettings() const
	{
		if (TargetType == ELandscapeToolTargetType::Weightmap)
		{
			int32 Index = INDEX_NONE;
			if (LayerInfoObj.IsValid())
			{
				Index = LandscapeInfo->GetLayerInfoIndex(LayerInfoObj.Get(), Owner.Get());
			}
			else
			{
				Index = LandscapeInfo->GetLayerInfoIndex(LayerName, Owner.Get());
			}
			if (ensure(Index != INDEX_NONE))
			{
				return &LandscapeInfo->Layers[Index];
			}
		}
		return NULL;
	}

	FLandscapeEditorLayerSettings* GetEditorLayerSettings() const
	{
		if (TargetType == ELandscapeToolTargetType::Weightmap)
		{
			check(LayerInfoObj.IsValid());
			ALandscapeProxy* Proxy = LandscapeInfo->GetLandscapeProxy();
			FLandscapeEditorLayerSettings* EditorLayerSettings = Proxy->EditorLayerSettings.FindByKey(LayerInfoObj.Get());
			if (EditorLayerSettings)
			{
				return EditorLayerSettings;
			}
			else
			{
				int32 Index = Proxy->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(LayerInfoObj.Get()));
				return &Proxy->EditorLayerSettings[Index];
			}
		}
		return NULL;
	}

	FName GetLayerName() const;

	FString& ReimportFilePath() const
	{
		if (TargetType == ELandscapeToolTargetType::Weightmap)
		{
			FLandscapeEditorLayerSettings* EditorLayerSettings = GetEditorLayerSettings();
			check(EditorLayerSettings);
			return EditorLayerSettings->ReimportLayerFilePath;
		}
		else //if (TargetType == ELandscapeToolTargetType::Heightmap)
		{
			return LandscapeInfo->GetLandscapeProxy()->ReimportHeightmapFilePath;
		}
	}
};

struct FLandscapeListInfo
{
	FString LandscapeName;
	ULandscapeInfo* Info;
	int32 ComponentQuads;
	int32 NumSubsections;
	int32 Width;
	int32 Height;

	FLandscapeListInfo(const TCHAR* InName, ULandscapeInfo* InInfo, int32 InComponentQuads, int32 InNumSubsections, int32 InWidth, int32 InHeight)
		: LandscapeName(InName)
		, Info(InInfo)
		, ComponentQuads(InComponentQuads)
		, NumSubsections(InNumSubsections)
		, Width(InWidth)
		, Height(InHeight)
	{
	}
};

struct FGizmoHistory
{
	ALandscapeGizmoActor* Gizmo;
	FString GizmoName;

	FGizmoHistory(ALandscapeGizmoActor* InGizmo)
		: Gizmo(InGizmo)
	{
		GizmoName = Gizmo->GetPathName();
	}

	FGizmoHistory(ALandscapeGizmoActiveActor* InGizmo)
	{
		// handle for ALandscapeGizmoActiveActor -> ALandscapeGizmoActor
		// ALandscapeGizmoActor is only for history, so it has limited data
		Gizmo = InGizmo->SpawnGizmoActor();
		GizmoName = Gizmo->GetPathName();
	}
};


namespace ELandscapeEdge
{
	enum Type
	{
		None,

		// Edges
		X_Negative,
		X_Positive,
		Y_Negative,
		Y_Positive,

		// Corners
		X_Negative_Y_Negative,
		X_Positive_Y_Negative,
		X_Negative_Y_Positive,
		X_Positive_Y_Positive,
	};
}

namespace ENewLandscapePreviewMode
{
	enum Type
	{
		None,
		NewLandscape,
		ImportLandscape,
	};
}

enum class ELandscapeEditingState : uint8
{
	Unknown,
	Enabled,
	BadFeatureLevel,
	PIEWorld,
	SIEWorld,
	NoLandscape,
};

/**
 * Landscape editor mode
 */
class FEdModeLandscape : public FEdMode
{
public:

	ULandscapeEditorObject* UISettings;

	FLandscapeToolMode* CurrentToolMode;
	FLandscapeTool* CurrentTool;
	FLandscapeBrush* CurrentBrush;
	FLandscapeToolTarget CurrentToolTarget;

	// GizmoBrush for Tick
	FLandscapeBrush* GizmoBrush;
	// UI setting for additional UI Tools
	int32 CurrentToolIndex;
	// UI setting for additional UI Tools
	int32 CurrentBrushSetIndex;

	ENewLandscapePreviewMode::Type NewLandscapePreviewMode;
	ELandscapeEdge::Type DraggingEdge;
	float DraggingEdge_Remainder;

	TWeakObjectPtr<ALandscapeGizmoActiveActor> CurrentGizmoActor;
	// UI callbacks for copy/paste tool
	FLandscapeToolCopyPaste<FHeightmapToolTarget>* CopyPasteTool;
	void CopyDataToGizmo();
	void PasteDataFromGizmo();

	// UI callbacks for splines tool
	FLandscapeToolSplines* SplinesTool;
	void ShowSplineProperties();
	virtual void SelectAllConnectedSplineControlPoints();
	virtual void SelectAllConnectedSplineSegments();
	virtual void SplineMoveToCurrentLevel();
	void SetbUseAutoRotateOnJoin(bool InbAutoRotateOnJoin);
	bool GetbUseAutoRotateOnJoin();

	// UI callbacks for ramp tool
	void ApplyRampTool();
	bool CanApplyRampTool();
	void ResetRampTool();

	// UI callbacks for mirror tool
	void ApplyMirrorTool();
	void CenterMirrorTool();

	/** Constructor */
	FEdModeLandscape();

	/** Initialization */
	void InitializeBrushes();
	void InitializeTool_Paint();
	void InitializeTool_Smooth();
	void InitializeTool_Flatten();
	void InitializeTool_Erosion();
	void InitializeTool_HydraErosion();
	void InitializeTool_Noise();
	void InitializeTool_Retopologize();
	void InitializeTool_NewLandscape();
	void InitializeTool_ResizeLandscape();
	void InitializeTool_Select();
	void InitializeTool_AddComponent();
	void InitializeTool_DeleteComponent();
	void InitializeTool_MoveToLevel();
	void InitializeTool_Mask();
	void InitializeTool_CopyPaste();
	void InitializeTool_Visibility();
	void InitializeTool_Splines();
	void InitializeTool_Ramp();
	void InitializeTool_Mirror();
	void InitializeToolModes();

	/** Destructor */
	virtual ~FEdModeLandscape();

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual bool UsesToolkits() const override;

	TSharedRef<FUICommandList> GetUICommandList() const;

	/** FEdMode: Called when the mode is entered */
	virtual void Enter() override;

	/** FEdMode: Called when the mode is exited */
	virtual void Exit() override;

	/** FEdMode: Called when the mouse is moved over the viewport */
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;

	/**
	 * FEdMode: Called when the mouse is moved while a window input capture is in effect
	 *
	 * @param	InViewportClient	Level editor viewport client that captured the mouse input
	 * @param	InViewport			Viewport that captured the mouse input
	 * @param	InMouseX			New mouse cursor X coordinate
	 * @param	InMouseY			New mouse cursor Y coordinate
	 *
	 * @return	true if input was handled
	 */
	virtual bool CapturedMouseMove(FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY) override;

	/** FEdMode: Called when a mouse button is pressed */
	virtual bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;

	/** FEdMode: Called when a mouse button is released */
	virtual bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;

	/** FEdMode: Allow us to disable mouse delta tracking during painting */
	virtual bool DisallowMouseDeltaTracking() const override;

	/** FEdMode: Called once per frame */
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;

	/** FEdMode: Called when clicking on a hit proxy */
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;

	/** FEdMode: Called when a key is pressed */
	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override;

	/** FEdMode: Called when mouse drag input is applied */
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override;

	/** FEdMode: Render elements for the landscape tool */
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;

	/** FEdMode: Render HUD elements for this tool */
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;

	/** FEdMode: Handling SelectActor */
	virtual bool Select(AActor* InActor, bool bInSelected) override;

	/** FEdMode: Check to see if an actor can be selected in this mode - no side effects */
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;

	/** FEdMode: Called when the currently selected actor has changed */
	virtual void ActorSelectionChangeNotify() override;

	virtual void ActorMoveNotify() override;

	virtual void PostUndo() override;

	virtual EEditAction::Type GetActionEditDuplicate() override;
	virtual EEditAction::Type GetActionEditDelete() override;
	virtual EEditAction::Type GetActionEditCut() override;
	virtual EEditAction::Type GetActionEditCopy() override;
	virtual EEditAction::Type GetActionEditPaste() override;
	virtual bool ProcessEditDuplicate() override;
	virtual bool ProcessEditDelete() override;
	virtual bool ProcessEditCut() override;
	virtual bool ProcessEditCopy() override;
	virtual bool ProcessEditPaste() override;

	/** FEdMode: If the EdMode is handling InputDelta (ie returning true from it), this allows a mode to indicated whether or not the Widget should also move. */
	virtual bool AllowWidgetMove() override { return true; }

	/** FEdMode: Draw the transform widget while in this mode? */
	virtual bool ShouldDrawWidget() const override;

	/** FEdMode: Returns true if this mode uses the transform widget */
	virtual bool UsesTransformWidget() const override;

	virtual EAxisList::Type GetWidgetAxisToDraw(FWidget::EWidgetMode InWidgetMode) const override;

	virtual FVector GetWidgetLocation() const override;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData) override;
	virtual bool GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData) override;

	virtual bool GetCursor(EMouseCursor::Type& OutCursor) const override;

	/** Forces real-time perspective viewports */
	void ForceRealTimeViewports(const bool bEnable, const bool bStoreCurrentState);

	/** Trace under the mouse cursor and return the landscape hit and the hit location (in landscape quad space) */
	bool LandscapeMouseTrace(FEditorViewportClient* ViewportClient, float& OutHitX, float& OutHitY);
	bool LandscapeMouseTrace(FEditorViewportClient* ViewportClient, FVector& OutHitLocation);

	/** Trace under the specified coordinates and return the landscape hit and the hit location (in landscape quad space) */
	bool LandscapeMouseTrace(FEditorViewportClient* ViewportClient, int32 MouseX, int32 MouseY, float& OutHitX, float& OutHitY);
	bool LandscapeMouseTrace(FEditorViewportClient* ViewportClient, int32 MouseX, int32 MouseY, FVector& OutHitLocation);

	/** Trace under the mouse cursor / specified screen coordinates against a world-space plane and return the hit location (in world space) */
	bool LandscapePlaneTrace(FEditorViewportClient* ViewportClient, const FPlane& Plane, FVector& OutHitLocation);
	bool LandscapePlaneTrace(FEditorViewportClient* ViewportClient, int32 MouseX, int32 MouseY, const FPlane& Plane, FVector& OutHitLocation);

	/** Trace under the specified laser start and direction and return the landscape hit and the hit location (in landscape quad space) */
	bool LandscapeTrace(const FVector& InRayOrigin, const FVector& InRayEnd, FVector& OutHitLocation);

	void SetCurrentToolMode(FName ToolModeName, bool bRestoreCurrentTool = true);

	/** Change current tool */
	void SetCurrentTool(FName ToolName);
	void SetCurrentTool(int32 ToolIdx);

	void SetCurrentBrushSet(FName BrushSetName);
	void SetCurrentBrushSet(int32 BrushSetIndex);

	void SetCurrentBrush(FName BrushName);
	void SetCurrentBrush(int32 BrushIndex);

	const TArray<TSharedRef<FLandscapeTargetListInfo>>& GetTargetList() const;
	const TArray<FName>* GetTargetDisplayOrderList() const;
	const TArray<FName>& GetTargetShownList() const;
	int32 GetTargetLayerStartingIndex() const;
	const TArray<FLandscapeListInfo>& GetLandscapeList();

	void AddLayerInfo(ULandscapeLayerInfoObject* LayerInfo);

	int32 UpdateLandscapeList();
	void UpdateTargetList();
	
	/** Update Display order list */
	void UpdateTargetLayerDisplayOrder(ELandscapeLayerDisplayMode InTargetDisplayOrder);
	void MoveTargetLayerDisplayOrder(int32 IndexToMove, int32 IndexToDestination);

	/** Update shown layer list */	
	void UpdateShownLayerList();
	bool ShouldShowLayer(TSharedRef<FLandscapeTargetListInfo> Target) const;
	void UpdateLayerUsageInformation(TWeakObjectPtr<ULandscapeLayerInfoObject>* LayerInfoObjectThatChanged = nullptr);
	void OnLandscapeMaterialChangedDelegate();
	void RefreshDetailPanel();

	DECLARE_EVENT(FEdModeLandscape, FTargetsListUpdated);
	static FTargetsListUpdated TargetsListUpdated;

	/** Called when the user presses a button on their motion controller device */
	void OnVRAction(FEditorViewportClient& ViewportClient, UViewportInteractor* Interactor, const FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled);

	void OnVRHoverUpdate(UViewportInteractor* Interactor, FVector& HoverImpactPoint, bool& bWasHandled);

	/** Handle notification that visible levels may have changed and we should update the editable landscapes list */
	void HandleLevelsChanged(bool ShouldExitMode);

	void OnMaterialCompilationFinished(UMaterialInterface* MaterialInterface);

	void ReimportData(const FLandscapeTargetListInfo& TargetInfo);
	void ImportData(const FLandscapeTargetListInfo& TargetInfo, const FString& Filename);

	/** Resample landscape to a different resolution or change the component size */
	ALandscape* ChangeComponentSetting(int32 NumComponentsX, int32 NumComponentsY, int32 InNumSubsections, int32 InSubsectionSizeQuads, bool bResample);

	/** Delete the specified landscape components */
	void DeleteLandscapeComponents(ULandscapeInfo* LandscapeInfo, TSet<ULandscapeComponent*> ComponentsToDelete);

	TArray<FLandscapeToolMode> LandscapeToolModes;
	TArray<TUniquePtr<FLandscapeTool>> LandscapeTools;
	TArray<FLandscapeBrushSet> LandscapeBrushSets;

	// For collision add visualization
	FLandscapeAddCollision* LandscapeRenderAddCollision;

	ELandscapeEditingState GetEditingState() const;

	bool IsEditingEnabled() const
	{
		return GetEditingState() == ELandscapeEditingState::Enabled;
	}

private:
	TArray<TSharedRef<FLandscapeTargetListInfo>> LandscapeTargetList;
	TArray<FLandscapeListInfo> LandscapeList;
	TArray<FName> ShownTargetLayerList;
	
	/** Represent the index offset of the target layer in LandscapeTargetList */
	int32 TargetLayerStartingIndex;

	UMaterialInterface* CachedLandscapeMaterial;

	const FViewport* ToolActiveViewport;

	FDelegateHandle OnWorldChangeDelegateHandle;
	FDelegateHandle OnLevelsChangedDelegateHandle;
	FDelegateHandle OnMaterialCompilationFinishedDelegateHandle;

	/** Check if we are painting using the VREditor */
	bool bIsPaintingInVR;

	/** The interactor that is currently painting, prevents multiple interactors from sculpting when one actually is */
	class UViewportInteractor* InteractorPainting;
};
