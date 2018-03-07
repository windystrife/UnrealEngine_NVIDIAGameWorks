// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "SBlastMeshEditorViewport.h"
#include "Widgets/Input/SComboBox.h"
#include "EditorStyleSet.h"
#include "Toolkits/IToolkitHost.h"
#include "UObject/GCObject.h"
#include "IBlastMeshEditor.h"
#include "BlastMeshEditorStyle.h"

/** Viewer/editor for a BlastMesh */
class FBlastMeshEditor : public IBlastMeshEditor, public FGCObject
{
public:

	/**
	 * Edits the specified mesh
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	BlastMesh		The mesh to edit
	 */
	void InitBlastMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UBlastMesh* BlastMesh );

	/** Destructor */
	virtual ~FBlastMeshEditor();

	/** IToolkit interface */
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	virtual FString GetDocumentationLink() const override
	{
		// @Todo: revert to Engine/Physics/Blasts once docs exist. 
		return FString(TEXT("Engine/Physics"));
	}

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FGCObject interface

	/** IBlastMeshEditor interface */
	virtual UBlastMesh* GetBlastMesh();
	virtual int32 GetCurrentPreviewDepth() const;
	virtual void RefreshTool();
	virtual void RefreshViewport();
	virtual void UpdateChunkSelection();
	virtual TSet<int32>& GetSelectedChunkIndices()
	{
		return SelectedChunkIndices;
	}
	virtual TArray<FBlastChunkEditorModelPtr>& GetChunkEditorModels()
	{
		return ChunkEditorModels;
	}
	virtual UBlastFractureSettings*	GetFractureSettings() override
	{
		return FractureSettings;
	}
	virtual void RemoveChildren(int32 ChunkId = INDEX_NONE) override;

private:

	TSharedRef<SDockTab> SpawnTab_ChunkHierarchy(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Properties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_FractureSettings(const FSpawnTabArgs& Args);
	//TSharedRef<SDockTab> SpawnTab_FractureHistory(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_ChunkParameters(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_AdvancedPreview(const FSpawnTabArgs& Args);

	void SetBlastMesh(UBlastMesh* InBlastMesh);

	void OnBlastMeshReloaded();

	void OnFractureMethodChanged();

	/**	Binds our UI commands to delegates */
	void BindCommands();

	/** Builds the Blast Mesh Editor toolbar. */
	void ExtendToolbar();

	/** Change the mesh the editor is viewing. */
	void OnChangeMesh();

	/** Callback when an object has been reimported, and whether it worked */
	void OnPostReimport(UObject* InObject, bool bSuccess);

	/** Slider label. */
	FText GetButtonLabel() const;

	/** A callback for when the preview depth is selected, refreshes the viewport. */
	void PreviewDepthSelectionChanged( int32 );

	/** A callback to read the current explode amount slider position. */
	float GetExplodeAmountSliderPosition() const;

	/** A callback for when the explode amount slider position is changed. */
	void OnSetExplodeAmount(float NewValue);

	//TSharedRef<ITableRow> OnGenerateRowForFractureHistory(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Callback for getting fracture state. */
	bool IsFractured();

	/** Callback for fracturing. */
	void Fracture();

	/** Callback for import root from static mesh */
	void ImportRootFromStaticMesh();

	/** Callback to check if the Blast mesh root can be imported from static mesh. */
	bool CanImportRootFromStaticMesh();

	/** Callback for fixing chunk hierarchy (uniteChunks) */
	void FixChunkHierarchy();

	/** Callback for fit uv coordinates */
	void FitUvCoordinates();

	void ExportAssetToFile();

	/** Callback for rebuilding collision mesh. It might be very expensive if user used VHACD for collision decomposition. */
	void RebuildCollisionMesh();

private:
	
	/** Chunk hierarchy tree view */
	TSharedPtr<class SBlastChunkTree>					ChunkHierarchy;
	
	/** Preview Viewport widget */
	TSharedPtr<class SBlastMeshEditorViewport>			Viewport;

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap<FName, TWeakPtr<class SDockableTab>>			SpawnedToolPanels;

	/** Property View */
	TSharedPtr<class IDetailsView>						BlastMeshDetailsView;
	TSharedPtr<class SDockTab>							BlastMeshDetailsViewTab;

	/** FractureSettings View */
	TSharedPtr<class IDetailsView>                      FractureSettingsView;

	/** FractureSettings View */
	TSharedPtr<class IDetailsView>                      FractureSettingsCustomView;

	/** Fracture history View */
	//TSharedPtr<SListView<TSharedPtr<FString>>>			FractureHistoryView;

	/** Chunk Parameters View */
	TSharedPtr<class IDetailsView>						ChunkParametersView;
	TSharedPtr<class SDockTab>							ChunkParametersViewTab;

	TArray<FBlastChunkEditorModelPtr>					ChunkEditorModels;

	/** List of chunk indices currently selected */
	TSet<int32>											SelectedChunkIndices;

	/* List of currently selected chunks */
	TArray<class UBlastChunkParamsProxy*>				SelectedChunks;

	/** Pool of currently unused chunk proxies */
	TArray<class UBlastChunkParamsProxy*>				UnusedProxies;

	/** Widget for displaying the available preview depths. */
	TSharedPtr<class SBlastDepthFilter>					PreviewDepthWidget;


	/** Widget for displaying the available fracture scripts and actions for it. */
	TSharedPtr<SWidget>									FractureScriptsWidget;

	/** Widget for adjusting the explode amount. */
	TSharedPtr<class SSlider>							ExplodeAmountSlider;

	/** The current explode amount (scaled to the maximum explode range), as a fraction of the mesh size. */
	float												ExplodeFractionOfRange;

	/* The BlastMesh that is active in the editor */
	UBlastMesh*											BlastMesh;

	TSharedPtr<class FBlastFracture>					Fracturer;
	UBlastFractureSettings*								FractureSettings;

	/**	The tab ids for all the tabs used */
	static const FName ChunkHierarchyTabId;
	static const FName ViewportTabId;
	static const FName PropertiesTabId;
	static const FName FractureSettingsTabId;
	//static const FName FractureHistoryTabId;
	static const FName ChunkParametersTabId;
	static const FName AdvancedPreviewTabId;
};
