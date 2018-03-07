// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Toolkits/IToolkitHost.h"
#include "Misc/NotifyHook.h"
#include "EditorUndoClient.h"
#include "IStaticMeshEditor.h"
#include "ISocketManager.h"

class FStaticMeshDetails;
class IDetailsView;
class SAdvancedPreviewDetailsTab;
class SConvexDecomposition;
class SDockableTab;
class SStaticMeshEditorViewport;
class UStaticMesh;
class UStaticMeshComponent;
class UStaticMeshSocket;
struct FPropertyChangedEvent;

/**
 * StaticMesh Editor class
 */
class FStaticMeshEditor : public IStaticMeshEditor, public FGCObject, public FEditorUndoClient, public FNotifyHook
{
public:
	FStaticMeshEditor()
		: StaticMesh(NULL)
		, NumLODLevels(0)
		, MinPrimSize(0.5f)
		, OverlapNudge(10.0f)
		, CurrentViewedUVChannel(0)
	{}

	~FStaticMeshEditor();

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/**
	 * Edits the specified static mesh object
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	ObjectToEdit			The static mesh to edit
	 */
	void InitStaticMeshEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UStaticMesh* ObjectToEdit);

	/** Creates details for a static mesh */
	TSharedRef<class IDetailCustomization> MakeStaticMeshDetails();

	//~ Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~ End FGCObject Interface

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("Engine/Content/Types/StaticMeshes/Editor"));
	}

	/** @return Returns the color and opacity to use for the color that appears behind the tab text for this toolkit's tab in world-centric mode. */
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** IStaticMeshEditor interface */
	virtual UStaticMesh* GetStaticMesh() override { return StaticMesh; }
	virtual UStaticMeshComponent* GetStaticMeshComponent() const override;

	virtual UStaticMeshSocket* GetSelectedSocket() const override;
	virtual void SetSelectedSocket(UStaticMeshSocket* InSelectedSocket) override;
	virtual void DuplicateSelectedSocket() override;
	virtual void RequestRenameSelectedSocket() override;

	virtual bool IsPrimValid(const FPrimData& InPrimData) const override;
	virtual bool HasSelectedPrims() const override;
	virtual void AddSelectedPrim(const FPrimData& InPrimData, bool bClearSelection) override;
	virtual void RemoveSelectedPrim(const FPrimData& InPrimData) override;
	virtual void RemoveInvalidPrims() override;
	virtual bool IsSelectedPrim(const FPrimData& InPrimData) const override;
	virtual void ClearSelectedPrims() override;
	virtual void DuplicateSelectedPrims(const FVector* InOffset) override;
	virtual void TranslateSelectedPrims(const FVector& InDrag) override;
	virtual void RotateSelectedPrims(const FRotator& InRot) override;
	virtual void ScaleSelectedPrims(const FVector& InScale) override;
	virtual bool CalcSelectedPrimsAABB(FBox &OutBox) const override;
	virtual bool GetLastSelectedPrimTransform(FTransform& OutTransform) const override;
	FTransform GetPrimTransform(const FPrimData& InPrimData) const override;
	void SetPrimTransform(const FPrimData& InPrimData, const FTransform& InPrimTransform) const override;
	bool OverlapsExistingPrim(const FPrimData& InPrimData) const;

	virtual int32 GetNumTriangles(int32 LODLevel = 0) const override;
	virtual int32 GetNumVertices(int32 LODLevel = 0) const override;
	virtual int32 GetNumUVChannels(int32 LODLevel = 0) const override;

	virtual int32 GetCurrentUVChannel() override;
	virtual int32 GetCurrentLODLevel() override;
	virtual int32 GetCurrentLODIndex() override;

	virtual void RefreshTool() override;
	virtual void RefreshViewport() override;
	virtual void DoDecomp(float InAccuracy, int32 InMaxHullVerts) override;

	virtual TSet< int32 >& GetSelectedEdges() override;
	// End of IStaticMeshEditor

	/** Extends the toolbar menu to include static mesh editor options */
	void ExtendMenu();

	/** Registers a delegate to be called after an Undo operation */
	virtual void RegisterOnPostUndo(const FOnPostUndo& Delegate) override;

	/** Unregisters a delegate to be called after an Undo operation */
	virtual void UnregisterOnPostUndo(SWidget* Widget) override;

	/** From FNotifyHook */
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged) override;

	/** Get the names of the LOD for menus */
	TArray< TSharedPtr< FString > >& GetLODLevels() { return LODLevels; }
	const TArray< TSharedPtr< FString > >& GetLODLevels() const { return LODLevels; }

	/** Get the active view mode */
	virtual EViewModeIndex GetViewMode() const override;

	virtual void RegisterOnSelectedLODChanged(const FOnSelectedLODChanged &Delegate, bool UnregisterOnRefresh) override
	{
		if (!UnregisterOnRefresh)
		{
			OnSelectedLODChanged.Add(Delegate);
		}
		else
		{
			OnSelectedLODChangedResetOnRefresh.Add(Delegate);
		}
	}

	virtual void UnRegisterOnSelectedLODChanged(void* Thing) override
	{
		OnSelectedLODChanged.RemoveAll(Thing);
		OnSelectedLODChangedResetOnRefresh.RemoveAll(Thing);
	}

	class FStaticMeshEditorViewportClient& GetViewportClient();
	const class FStaticMeshEditorViewportClient& GetViewportClient() const;
private:
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Properties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SocketManager(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Collision(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PreviewSceneSettings(const FSpawnTabArgs& Args);

private:
	/** Binds commands associated with the Static Mesh Editor. */
	void BindCommands();

	/** Builds the Static Mesh Editor toolbar. */
	void ExtendToolBar();

	/** Builds the sub-tools that are a part of the static mesh editor. */
	void BuildSubTools();

	/**
	* Updates NumTriangles, NumVertices and NumUVChannels for the given LOD
	*/
	void UpdateLODStats(int32 CurrentLOD);

	/** A general callback for the combo boxes in the Static Mesh Editor to force a viewport refresh when a selection changes. */
	void ComboBoxSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** A callback for when the LOD is selected, forces an update to retrieve UV channels, triangles, vertices among other things. Refreshes the viewport. */
	void LODLevelsSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/**
	 *	Sets the editor's current mesh and refreshes various settings to correspond with the new data.
	 *
	 *	@param	InStaticMesh		The static mesh to use for the editor.
	 */
	void SetEditorMesh(UStaticMesh* InStaticMesh, bool bResetCamera = true);

	/** Helper function for generating K-DOP collision geometry. */
	void GenerateKDop(const FVector* Directions, uint32 NumDirections);

	/** Callback for creating box collision. */
	void OnCollisionBox();

	/** Callback for creating sphere collision. */
	void OnCollisionSphere();

	/** Callback for creating sphyl collision. */
	void OnCollisionSphyl();

	/**
	* Quick and dirty way of creating box vertices from a box collision representation
	* Grossly inefficient, but not time critical
	* @param BoxElem - Box collision to get the vertices for
	* @param Verts - output listing of vertex data from the box collision
	* @param Scale - scale to create this box at
	*/
	void CreateBoxVertsFromBoxCollision(const struct FKBoxElem& BoxElem, TArray<FVector>& Verts, float Scale);

	/** Converts the collision data for the static mesh */
	void OnConvertBoxToConvexCollision(void);

	/** Copy the collision data from the selected static mesh in Content Browser*/
	void OnCopyCollisionFromSelectedStaticMesh(void);

	/** Whether there is a valid static mesh to copy collision from selected in the content browser */
	bool CanCopyCollisionFromSelectedStaticMesh() const;

	/** Get the first static mesh selected in the content browser */
	UStaticMesh* GetFirstSelectedStaticMeshInContentBrowser() const;

	/** Clears the collision data for the static mesh */
	void OnRemoveCollision(void);

	/** Whether there is collision to remove from the static mesh */
	bool CanRemoveCollision();

	/** Change the mesh the editor is viewing. */
	void OnChangeMesh();

	/** Whether there is a static mesh selected in the content browser to change to*/
	bool CanChangeMesh() const;

	/** Replace the generated LODs in the original source mesh with the reduced versions.*/
	void OnSaveGeneratedLODs();

	/** Rebuilds the LOD combo list and sets it to "auto", a safe LOD level. */
	void RegenerateLODComboList();

	/** Rebuilds the UV Channel combo list and attempts to set it to the same channel. */
	TSharedRef<SWidget> GenerateUVChannelComboList();

	/** Delete whats currently selected */
	void DeleteSelected();

	/** Whether we currently have any selected that can be deleted */
	bool CanDeleteSelected() const;

	/** Delete the currently selected sockets */
	void DeleteSelectedSockets();

	/** Delete the currently selected prims */
	void DeleteSelectedPrims();

	/** Duplicate whats currently selected */
	void DuplicateSelected();

	/** Whether we currently have any selected that can be duplicated */
	bool CanDuplicateSelected() const;

	/** Whether we currently have any selected that can be renamed */
	bool CanRenameSelected() const;

	/** Handler for when FindInExplorer is selected */
	void ExecuteFindInExplorer();

	/** Returns true to allow execution of source file commands */
	bool CanExecuteSourceCommands() const;

	/** Callback when an object is reimported, handles steps needed to keep the editor up-to-date. */
	void OnObjectReimported(UObject* InObject);

	/** Opens the convex decomposition tab. */
	void OnConvexDecomposition();

	//~ Begin FAssetEditorToolkit Interface.
	virtual bool OnRequestClose() override;
	//~ End FAssetEditorToolkit Interface.

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

	/** Undo Action**/
	void UndoAction();

	/** Redo Action **/
	void RedoAction();

	/** Called when socket selection changes */
	void OnSocketSelectionChanged();

	/** Callback when an object has been reimported, and whether it worked */
	void OnPostReimport(UObject* InObject, bool bSuccess);

	void SetCurrentViewedUVChannel(int32 InNewUVChannel);

	ECheckBoxState GetUVChannelCheckState(int32 TestUVChannel) const;

private:
	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<class SDockableTab> > SpawnedToolPanels;

	/** Preview Viewport widget */
	TSharedPtr<class SStaticMeshEditorViewport> Viewport;

	/** Property View */
	TSharedPtr<class IDetailsView> StaticMeshDetailsView;

	/** Socket Manager widget. */
	TSharedPtr< class ISocketManager> SocketManager;

	/** Convex Decomposition widget */
	TSharedPtr< class SConvexDecomposition> ConvexDecomposition;

	/** Widget for displaying the available LOD. */
	TSharedPtr< class STextComboBox > LODLevelCombo;

	/** Static mesh editor detail customization */
	TWeakPtr<class FStaticMeshDetails> StaticMeshDetails;

	/** Named list of LODs for use in menus */
	TArray< TSharedPtr< FString > > LODLevels;

	/** The currently viewed Static Mesh. */
	UStaticMesh* StaticMesh;

	/** The number of triangles associated with the static mesh LOD. */
	TArray<int32> NumTriangles;

	/** The number of vertices associated with the static mesh LOD. */
	TArray<int32> NumVertices;

	/** The number of used UV channels. */
	TArray<int32> NumUVChannels;

	/** The number of LOD levels. */
	int32 NumLODLevels;

	/** Delegates called after an undo operation for child widgets to refresh */
	FOnPostUndoMulticaster OnPostUndo;	

	/** Information on the selected collision primitives */
	TArray<FPrimData> SelectedPrims;

	/** Scene preview settings widget */
	TSharedPtr<SWidget> AdvancedPreviewSettingsWidget;

	/** Misc consts */
	const float MinPrimSize;
	const FVector OverlapNudge;

	/** The current UV Channel we are viewing */
	int32 CurrentViewedUVChannel;

	FOnSelectedLODChangedMulticaster OnSelectedLODChanged;
	FOnSelectedLODChangedMulticaster OnSelectedLODChangedResetOnRefresh;

	/**	The tab ids for all the tabs used */
	static const FName ViewportTabId;
	static const FName PropertiesTabId;
	static const FName SocketManagerTabId;
	static const FName CollisionTabId;
	static const FName PreviewSceneSettingsTabId;
};
