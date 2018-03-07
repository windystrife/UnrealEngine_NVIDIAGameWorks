// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SkeletalMeshTypes.h"
#include "PreviewScene.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/ShapeElem.h"
#include "Preferences/PhysicsAssetEditorOptions.h"

class UBodySetup;
class UPhysicsAssetEditorSkeletalMeshComponent;
class UPhysicsAsset;
class UPhysicsConstraintTemplate;
class UPhysicsHandleComponent;
class USkeletalMesh;
class UStaticMeshComponent;
class IPersonaPreviewScene;

#define DEBUG_CLICK_VIEWPORT 0

/*-----------------------------------------------------------------------------
   FPhysicsAssetEditorSharedData
-----------------------------------------------------------------------------*/

class FPhysicsAssetEditorSharedData
{
public:
	/** Constructor/Destructor */
	FPhysicsAssetEditorSharedData();
	virtual ~FPhysicsAssetEditorSharedData();

	enum EPhysicsAssetEditorConstraintType
	{
		PCT_Swing1,
		PCT_Swing2,
		PCT_Twist,
	};

	/** Encapsulates a selected set of bodies or constraints */
	struct FSelection
	{
		int32 Index;
		EAggCollisionShape::Type PrimitiveType;
		int32 PrimitiveIndex;
		FTransform WidgetTM;
		FTransform ManipulateTM;

		FSelection(int32 GivenBodyIndex, EAggCollisionShape::Type GivenPrimitiveType, int32 GivenPrimitiveIndex) :
			Index(GivenBodyIndex), PrimitiveType(GivenPrimitiveType), PrimitiveIndex(GivenPrimitiveIndex),
			WidgetTM(FTransform::Identity), ManipulateTM(FTransform::Identity)
		{
		}

		bool operator==(const FSelection& rhs) const
		{
			return Index == rhs.Index && PrimitiveType == rhs.PrimitiveType && PrimitiveIndex == rhs.PrimitiveIndex;
		}
	};

	/** Initializes members */
	void Initialize(const TSharedRef<IPersonaPreviewScene>& InPreviewScene);

	/** Caches a preview mesh. Sets us to a default mesh if none is set yet (or if an older one got deleted) */
	void CachePreviewMesh();

	/** Accessor for mesh view mode, allows access for simulation and non-simulation modes */
	EPhysicsAssetEditorRenderMode GetCurrentMeshViewMode(bool bSimulation);

	/** Accessor for collision view mode, allows access for simulation and non-simulation modes */
	EPhysicsAssetEditorRenderMode GetCurrentCollisionViewMode(bool bSimulation);

	/** Accessor for constraint view mode, allows access for simulation and non-simulation modes */
	EPhysicsAssetEditorConstraintViewMode GetCurrentConstraintViewMode(bool bSimulation);
	
	/** Clear all of the selected constraints */
	void ClearSelectedConstraints();

	/** Set the selection state of a constraint */
	void SetSelectedConstraint(int32 ConstraintIndex, bool bSelected);

	/** Check whether the constraint at the specified index is selected */
	bool IsConstraintSelected(int32 ConstraintIndex) const;

	/** Get the world transform of the specified selected constraint */
	FTransform GetConstraintWorldTM(const FSelection* Constraint, EConstraintFrame::Type Frame) const;

	/** Get the world transform of the specified constraint */
	FTransform GetConstraintWorldTM(const UPhysicsConstraintTemplate* ConstraintSetup, EConstraintFrame::Type Frame, float Scale = 1.f) const;

	/** Get the world transform of the specified constraint */
	FTransform GetConstraintMatrix(int32 ConstraintIndex, EConstraintFrame::Type Frame, float Scale) const;
	
	/** Get the body transform of the specified constraint */
	FTransform GetConstraintBodyTM(const UPhysicsConstraintTemplate* ConstraintSetup, EConstraintFrame::Type Frame) const;

	/** Set the constraint relative transform */
    void SetConstraintRelTM(const FSelection* Constraint, const FTransform& RelTM);

	/** Set the constraint relative transform for a single selected constraint */
    inline void SetSelectedConstraintRelTM(const FTransform& RelTM)
    {
        SetConstraintRelTM(GetSelectedConstraint(), RelTM);
    }
	
	/** Snaps a constraint at the specified index to it's bone */
	void SnapConstraintToBone(int32 ConstraintIndex);

	/** Snaps the specified constraint to it's bone */
	void SnapConstraintToBone(FConstraintInstance& ConstraintInstance);

	/** Deletes the currently selected constraints */
	void DeleteCurrentConstraint();

	/** Paste the currently-copied constraint properties onto the single selected constraint */
	void PasteConstraintProperties();
	
	/** Cycles the rows of the transform matrix for the selected constraint. Assumes the selected constraint
	  * is valid and that we are in constraint editing mode*/
	void CycleCurrentConstraintOrientation();

	/** Cycles the active constraint*/
	void CycleCurrentConstraintActive();

	/** Cycles the active constraint between locked and limited */
	void ToggleConstraint(EPhysicsAssetEditorConstraintType Constraint);

	/** Gets whether the active constraint is locked */
	bool IsAngularConstraintLocked(EPhysicsAssetEditorConstraintType Constraint) const;

	/** Collision geometry editing */
	void ClearSelectedBody();
	void SetSelectedBody(const FSelection& Body, bool bSelected);
	bool IsBodySelected(const FSelection& Body) const;
	void SetSelectedBodyAnyPrim(int32 BodyIndex, bool bSelected);
	void DeleteCurrentPrim();
	void DeleteBody(int32 DelBodyIndex, bool bRefreshComponent=true);
	void RefreshPhysicsAssetChange(const UPhysicsAsset* InPhysAsset);
	void MakeNewBody(int32 NewBoneIndex, bool bAutoSelect = true);
	void MakeNewConstraint(int32 BodyIndex0, int32 BodyIndex1);
	void CopyBody();
	void CopyConstraint();
	void PasteBodyProperties();
	bool WeldSelectedBodies(bool bWeld = true);
	void Mirror();

	/** Toggle simulation on and off */
	void ToggleSimulation();

	/** Open a new body dialog */
	void OpenNewBodyDlg();

	/** Open a new body dialog, filling in NewBodyResponse when the dialog is closed */
	static void OpenNewBodyDlg(EAppReturnType::Type* NewBodyResponse);

	/** Helper function for creating the details panel widget and other controls that form the New body dialog (used by OpenNewBodyDlg and the tools tab) */
	static TSharedRef<SWidget> CreateGenerateBodiesWidget(const FSimpleDelegate& InOnCreate, const FSimpleDelegate& InOnCancel = FSimpleDelegate(), const TAttribute<bool>& InIsEnabled = TAttribute<bool>(), const TAttribute<FText>& InCreateButtonText = TAttribute<FText>(), bool bForNewAsset = false);

	/** Handle clicking on a body */
	void HitBone(int32 BodyIndex, EAggCollisionShape::Type PrimType, int32 PrimIndex, bool bGroupSelect);

	/** Handle clikcing on a constraint */
	void HitConstraint(int32 ConstraintIndex, bool bGroupSelect);

	/** Undo/Redo */
	void PostUndo();
	void Redo();

	/** Helpers to enable/disable collision on selected bodies */
	void SetCollisionBetweenSelected(bool bEnableCollision);
	bool CanSetCollisionBetweenSelected(bool bEnableCollision) const;
	void SetCollisionBetweenSelectedAndAll(bool bEnableCollision);
	bool CanSetCollisionBetweenSelectedAndAll(bool bEnableCollision) const;

	/** Prevents GC from collecting our objects */
	void AddReferencedObjects(FReferenceCollector& Collector);

	/** Enables and disables simulation. Used by ToggleSimulation */
	void EnableSimulation(bool bEnableSimulation);

	/** Force simulation off for all bodies, regardless of physics type */
	void ForceDisableSimulation();

private:
	/** Initializes a constraint setup */
	void InitConstraintSetup(UPhysicsConstraintTemplate* ConstraintSetup, int32 ChildBodyIndex, int32 ParentBodyIndex);

	/** Collision editing helper methods */
	void SetCollisionBetween(int32 Body1Index, int32 Body2Index, bool bEnableCollision);

	/** Update the cached array of bodies that do not collide with the current body selection */
	void UpdateNoCollisionBodies();

	/** Copy the properties of the one and only selected constraint */
	void CopyConstraintProperties(UPhysicsConstraintTemplate * FromConstraintSetup, UPhysicsConstraintTemplate * ToConstraintSetup);

public:
	/** Callback for handling selection changes */
	DECLARE_EVENT_TwoParams(FPhysicsAssetEditorSharedData, FSelectionChanged, const TArray<FSelection>&, const TArray<FSelection>&);
	FSelectionChanged SelectionChangedEvent;

	/** Callback for handling changes to the bone/body/constraint hierarchy */
	DECLARE_EVENT(FPhysicsAssetEditorSharedData, FHierarchyChanged);
	FHierarchyChanged HierarchyChangedEvent;

	/** Callback for handling changes to the current selection in the tree */
	DECLARE_EVENT(FPhysicsAssetEditorSharedData, FHierarchySelectionChangedEvent);
	FHierarchySelectionChangedEvent HierarchySelectionChangedEvent;
	

	/** Callback for triggering a refresh of the preview viewport */
	DECLARE_EVENT(FPhysicsAssetEditorSharedData, FPreviewChanged);
	FPreviewChanged PreviewChangedEvent;

	/** The PhysicsAsset asset being inspected */
	UPhysicsAsset* PhysicsAsset;

	/** PhysicsAssetEditor specific skeletal mesh component */
	UPhysicsAssetEditorSkeletalMeshComponent* EditorSkelComp;

	/** PhysicsAssetEditor specific physical animation component */
	class UPhysicalAnimationComponent* PhysicalAnimationComponent;

	/** Preview scene */
	TWeakPtr<IPersonaPreviewScene> PreviewScene;

	/** Editor options */
	UPhysicsAssetEditorOptions* EditorOptions;

	/** Results from the new body dialog */
	EAppReturnType::Type NewBodyResponse;

	/** Helps define how the asset behaves given user interaction in simulation mode*/
	UPhysicsHandleComponent* MouseHandle;

	/** Draw color for center of mass debug strings */
	const FColor COMRenderColor;

	/** List of bodies that don't collide with the currently selected collision body */
	TArray<int32> NoCollisionBodies;

	/** Bone info */
	TArray<FBoneVertInfo> DominantWeightBoneInfos;
	TArray<FBoneVertInfo> AnyWeightBoneInfos;

	TArray<FSelection> SelectedBodies;
	FSelection * GetSelectedBody()
	{
		int32 Count = SelectedBodies.Num();
		return Count ? &SelectedBodies[Count - 1] : NULL;
	}

	UBodySetup * CopiedBodySetup;
	UPhysicsConstraintTemplate * CopiedConstraintTemplate;

	/** Constraint editing */
	TArray<FSelection> SelectedConstraints;
	FSelection * GetSelectedConstraint()
	{
		int32 Count = SelectedConstraints.Num();
		return Count ? &SelectedConstraints[Count - 1] : NULL;
	}

	const FSelection * GetSelectedConstraint() const
	{
		int32 Count = SelectedConstraints.Num();
		return Count ? &SelectedConstraints[Count - 1] : NULL;
	}

	/** Show flags */
	bool bShowCOM;

	/** Misc toggles */
	bool bRunningSimulation;
	bool bNoGravitySimulation;
	
	/** Manipulation (rotate, translate, scale) */
	bool bManipulating;

	/** Used to prevent recursion with tree hierarchy ... needs to be rewritten! */
	int32 InsideSelChange;

	FTransform ResetTM;

#if DEBUG_CLICK_VIEWPORT
	FVector LastClickOrigin;
	FVector LastClickDirection;
#endif
	FIntPoint LastClickPos;
};
