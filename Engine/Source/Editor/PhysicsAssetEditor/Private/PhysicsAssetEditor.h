// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Widgets/SWidget.h"
#include "UObject/GCObject.h"
#include "Textures/SlateIcon.h"
#include "Editor/UnrealEdTypes.h"
#include "UnrealWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "TickableEditorObject.h"
#include "EditorUndoClient.h"
#include "Toolkits/IToolkitHost.h"
#include "IPhysicsAssetEditor.h"
#include "Editor/PhysicsAssetEditor/Private/PhysicsAssetEditorSharedData.h"
#include "PhysicsEngine/BodySetupEnums.h"
#include "ArrayView.h"

struct FAssetData;
class FPhysicsAssetEditorTreeInfo;
class IDetailsView;
class SComboButton;
class SDockableTab;
class SPhysicsAssetEditorPreviewViewport;
class UAnimationAsset;
class UAnimSequence;
class UPhysicsAsset;
class UAnimSequence;
class SPhysicsAssetGraph;
class IPersonaPreviewScene;
class ISkeletonTree;
class ISkeletonTreeItem;
class IPersonaToolkit;
class FPhysicsAssetEditorSkeletonTreeBuilder;
class USkeletalMesh;

namespace PhysicsAssetEditorModes
{
	extern const FName PhysicsAssetEditorMode;
}

enum EPhysicsAssetEditorConstraintType
{
	EPCT_BSJoint,
	EPCT_Hinge,
	EPCT_SkelJoint,
	EPCT_Prismatic
};

/*-----------------------------------------------------------------------------
   FPhysicsAssetEditor
-----------------------------------------------------------------------------*/

class FPhysicsAssetEditor : public IPhysicsAssetEditor, public FGCObject, public FEditorUndoClient, public FTickableEditorObject
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Destructor */
	virtual ~FPhysicsAssetEditor();

	/** Edits the specified PhysicsAsset object */
	void InitPhysicsAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UPhysicsAsset* ObjectToEdit);

	/** Shared data accessor */
	TSharedPtr<FPhysicsAssetEditorSharedData> GetSharedData() const;

	/** Handles a group selection change... assigns the proper object to the properties widget and the hierarchy tree view */
	void HandleViewportSelectionChanged(const TArray<FPhysicsAssetEditorSharedData::FSelection>& InSelectedBodies, const TArray<FPhysicsAssetEditorSharedData::FSelection>& InSelectedConstraints);

	/** Repopulates the hierarchy tree view */
	void RefreshHierachyTree();

	/** Refreshes the preview viewport */
	void RefreshPreviewViewport();

	/** Methods for building the various context menus */
	void BuildMenuWidgetBody(FMenuBuilder& InMenuBuilder);
	void BuildMenuWidgetPrimitives(FMenuBuilder& InMenuBuilder);
	void BuildMenuWidgetConstraint(FMenuBuilder& InMenuBuilder);
	void BuildMenuWidgetSelection(FMenuBuilder& InMenuBuilder);
	void BuildMenuWidgetNewConstraint(FMenuBuilder& InMenuBuilder);
	void BuildMenuWidgetNewConstraintForBody(FMenuBuilder& InMenuBuilder, int32 InSourceBodyIndex);
	void BuildMenuWidgetBone(FMenuBuilder& InMenuBuilder);
	TSharedRef<SWidget> BuildStaticMeshAssetPicker();

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("Engine/Physics/PhysicsAssetEditor"));
	}

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	//~ Begin FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject Interface

	void HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView);

	void HandlePhysicsAssetGraphCreated(const TSharedRef<SPhysicsAssetGraph>& InPhysicsAssetGraph);

	void HandleGraphObjectsSelected(const TArrayView<UObject*>& InObjects);

	void HandleSelectionChanged(const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type InSelectInfo);

	void HandleCreateNewConstraint(int32 BodyIndex0, int32 BodyIndex1);

	TSharedRef<IPersonaToolkit> GetPersonaToolkit() const { return PersonaToolkit.ToSharedRef(); }

	TSharedPtr<ISkeletonTree> GetSkeletonTree() const { return SkeletonTree.ToSharedRef(); }

	/** Reset bone collision for selected or regenerate all bodies if no bodies are selected */
	void ResetBoneCollision();

	/** Check whether we are out of simulation mode */
	bool IsNotSimulation() const;

public:
	/** Delegate fired on undo/redo */
	FSimpleMulticastDelegate OnPostUndo;

private:

	enum EPhatHierarchyFilterMode
	{
		PHFM_All,
		PHFM_Bodies
	};

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient
	
	/** Builds the menu for the PhysicsAsset editor */
	void ExtendMenu();

	/** Builds the toolbar widget for the PhysicsAsset editor */
	void ExtendToolbar();
	
	/**	Binds our UI commands to delegates */
	void BindCommands();

	void OnAssetSelectedFromStaticMeshAssetPicker(const FAssetData& AssetData);

	TSharedRef<SWidget> BuildPhysicalMaterialAssetPicker(bool bForAllBodies);

	void OnAssetSelectedFromPhysicalMaterialAssetPicker(const FAssetData& AssetData, bool bForAllBodies);

	/** Call back for when bone/body properties are changed */
	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

	/** Helper function for SContentReference which tells it whether a particular asset is valid for the current skeleton */
	bool ShouldFilterAssetBasedOnSkeleton(const FAssetData& AssetData);

	/** Constraint editing helper methods */
	void SnapConstraintToBone(const FPhysicsAssetEditorSharedData::FSelection* Constraint)
	{
		SharedData->SnapConstraintToBone(Constraint->Index);
	}

	void CreateOrConvertConstraint(EPhysicsAssetEditorConstraintType ConstraintType);
	
	/** Collision editing helper methods */
	void AddNewPrimitive(EAggCollisionShape::Type PrimitiveType, bool bCopySelected = false);
	void SetBodiesBelowSelectedPhysicsType( EPhysicsType InPhysicsType, bool bMarkAsDirty);
	void SetBodiesBelowPhysicsType( EPhysicsType InPhysicsType, const TArray<int32> & Indices, bool bMarkAsDirty);

	/** Toolbar/menu command methods */
	bool HasSelectedBodyAndIsNotSimulation() const;
	bool CanEditConstraintProperties() const;
	bool HasSelectedConstraintAndIsNotSimulation() const;
	void OnChangeDefaultMesh(USkeletalMesh* OldPreviewMesh, USkeletalMesh* NewPreviewMesh);
	void OnCopyProperties();
	bool IsCopyProperties() const;
	bool CanCopyProperties() const;
	void OnPasteProperties();
	bool CanPasteProperties() const;
	bool IsSelectedEditMode() const;
	void OnRepeatLastSimulation();
	void OnToggleSimulation(bool bInSelected);
	void OnToggleSimulationNoGravity();
	bool IsNoGravitySimulationEnabled() const;
	void SetupSelectedSimulation();
	bool IsFullSimulation() const;
	bool IsSelectedSimulation() const;
	bool IsToggleSimulation() const;
	void OnMeshRenderingMode(EPhysicsAssetEditorRenderMode Mode, bool bSimulation);
	bool IsMeshRenderingMode(EPhysicsAssetEditorRenderMode Mode, bool bSimulation) const;
	void OnCollisionRenderingMode(EPhysicsAssetEditorRenderMode Mode, bool bSimulation);
	bool IsCollisionRenderingMode(EPhysicsAssetEditorRenderMode Mode, bool bSimulation) const;
	void OnConstraintRenderingMode(EPhysicsAssetEditorConstraintViewMode Mode, bool bSimulation);
	bool IsConstraintRenderingMode(EPhysicsAssetEditorConstraintViewMode Mode, bool bSimulation) const;
	void ToggleDrawConstraintsAsPoints();
	bool IsDrawingConstraintsAsPoints() const;
	void ToggleRenderOnlySelectedSolid();
	bool IsRenderingOnlySelectedSolid() const;
	void OnToggleMassProperties();
	bool IsToggleMassProperties() const;
	void OnSetCollision(bool bEnable);
	bool CanSetCollision(bool bEnable) const;
	void OnSetCollisionAll(bool bEnable);
	bool CanSetCollisionAll(bool bEnable) const;
	void OnWeldToBody();
	bool CanWeldToBody();
	void OnAddSphere();
	void OnAddSphyl();
	void OnAddBox();
	bool CanAddPrimitive() const;
	void OnDeletePrimitive();
	void OnDuplicatePrimitive();
	bool CanDuplicatePrimitive() const;
	void OnResetConstraint();
	void OnSnapConstraint();
	void OnConvertToBallAndSocket();
	void OnConvertToHinge();
	void OnConvertToPrismatic();
	void OnConvertToSkeletal();
	void OnDeleteConstraint();
	void OnViewType(ELevelViewportType ViewType);
	void OnSetBodyPhysicsType( EPhysicsType InPhysicsType );
	bool IsBodyPhysicsType( EPhysicsType InPhysicsType );
	void OnDeleteBody();
	void OnDeleteAllBodiesBelow();
	void OnDeleteSelection();
	void OnCycleConstraintOrientation();
	void OnCycleConstraintActive();
	void OnToggleSwing1();
	void OnToggleSwing2();
	void OnToggleTwist();
	bool IsSwing1Locked() const;
	bool IsSwing2Locked() const;
	bool IsTwistLocked() const;

	void Mirror();

	//menu commands
	void OnSelectAllBodies();
	void OnSelectAllConstraints();
	void OnDeselectAll();

	FText GetRepeatLastSimulationToolTip() const;
	FSlateIcon GetRepeatLastSimulationIcon() const;

	/** Handle initial preview scene setup */
	void HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene);

	/** Build context menu for tree items */
	void HandleExtendContextMenu(FMenuBuilder& InMenuBuilder);

	/** Extend the filter menu */
	void HandleExtendFilterMenu(FMenuBuilder& InMenuBuilder);

	/** Filter menu toggles */
	void HandleToggleShowBodies();
	void HandleToggleShowConstraints();
	void HandleToggleShowPrimitives();
	ECheckBoxState GetShowBodiesChecked() const;
	ECheckBoxState GetShowConstraintsChecked() const;
	ECheckBoxState GetShowPrimitivesChecked() const;

	/** Customize the filter label */
	void HandleGetFilterLabel(TArray<FText>& InOutItems) const;

	/** Invalidate convex meshes and recreate the physics state. Performed on property changes (etc) */
	void RecreatePhysicsState();

private:
	/** Physics asset properties tab */
	TSharedPtr<class IDetailsView> PhysAssetProperties;

	/** Data and methods shared across multiple classes */
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData;

	/** Toolbar extender - used repeatedly as the body/constraints mode will remove/add this when changed */
	TSharedPtr<FExtender> ToolbarExtender;

	/** Menu extender - used for commands like Select All */
	TSharedPtr<FExtender> MenuExtender;

	/** True if in OnTreeSelectionChanged()... protects against infinite recursion */
	bool bSelecting;

	/** True if we want to only simulate from selected body/constraint down*/
	bool SelectedSimulation;

	/** Used to keep track of the physics type before using Selected Simulation */
	TArray<EPhysicsType> PhysicsTypeState;

	/** The skeleton tree widget */
	TSharedPtr<ISkeletonTree> SkeletonTree;

	/** The skeleton tree builder */
	TSharedPtr<FPhysicsAssetEditorSkeletonTreeBuilder> SkeletonTreeBuilder;

	/** The persona toolkit */
	TSharedPtr<IPersonaToolkit> PersonaToolkit;

	/** The current physics asset graph, if any */
	TWeakPtr<SPhysicsAssetGraph> PhysicsAssetGraph;

	void FixPhysicsState();
	void ImpToggleSimulation();

	/** Records PhysicsAssetEditor related data - simulating or mode change */
	void OnAddPhatRecord(const FString& Action, bool bRecordSimulate, bool bRecordMode);
};
