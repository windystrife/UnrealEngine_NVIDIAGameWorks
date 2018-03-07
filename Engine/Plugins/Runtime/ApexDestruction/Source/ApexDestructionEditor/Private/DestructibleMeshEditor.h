// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Framework/Commands/Commands.h"
#include "SDestructibleMeshEditorViewport.h"
#include "Widgets/Input/SComboBox.h"
#include "EditorStyleSet.h"
#include "Toolkits/IToolkitHost.h"
#include "IDestructibleMeshEditor.h"
#include "ApexDestructionStyle.h"

class IDetailsView;
class SDockableTab;
class SSlider;
class UDestructibleMesh;

class FDestructibleMeshEditorCommands : public TCommands<FDestructibleMeshEditorCommands>
{

public:
	FDestructibleMeshEditorCommands() : TCommands<FDestructibleMeshEditorCommands>
	(
		"DestructibleMeshEditor", // Context name for fast lookup
		NSLOCTEXT("Contexts", "DestructibleMeshEditor", "DestructibleMesh Editor"), // Localized context name for displaying
		NAME_None, // Parent
		FApexDestructionStyle::GetStyleSetName() // Icon Style Set
	)
	{
	}

	/**
	 * DestructibleMesh Editor Commands
	 */
	
	/**  */
	TSharedPtr< FUICommandInfo > Fracture;
	TSharedPtr< FUICommandInfo > Refresh;
	TSharedPtr< FUICommandInfo > ImportFBXChunks;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

public:
};

/** Viewer/editor for a DestructibleMesh */
class FDestructibleMeshEditor : public IDestructibleMeshEditor
{
public:

	/**
	 * Edits the specified mesh
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	DestructibleMesh		The mesh to edit
	 */
	void InitDestructibleMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDestructibleMesh* DestructibleMesh );

	/** Destructor */
	virtual ~FDestructibleMeshEditor();

	/** IToolkit interface */
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	virtual FString GetDocumentationLink() const override
	{
		// @Todo: revert to Engine/Physics/Destructibles once docs exist. 
		return FString(TEXT("Engine/Physics"));
	}

	/** IDestructibleMeshEditor interface */
	virtual UDestructibleMesh* GetDestructibleMesh() override;
	virtual int32 GetCurrentPreviewDepth() const override;
	virtual void SetCurrentPreviewDepth(uint32 InPreviewDepthDepth) override;
	virtual void RefreshTool() override;
	virtual void RefreshViewport() override;

	void SetSelectedChunks(const TArray<UObject*>& SelectedChunks);

private:
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Properties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_FractureSettings(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_ChunkParameters(const FSpawnTabArgs& Args);

private:

	/** Builds the Destructible Mesh Editor toolbar. */
	void ExtendToolbar();

	/**
	 *	Sets the editor's current mesh and refreshes various settings to correspond with the new data.
	 *
	 *	@param	InDestructibleMesh		The destructible mesh to use for the editor.
	 */
	void SetEditorMesh(UDestructibleMesh* InDestructibleMesh);

	/** Change the mesh the editor is viewing. */
	void OnChangeMesh();

	/** Callback when an object has been reimported, and whether it worked */
	void OnPostReimport(UObject* InObject, bool bSuccess);

	/** Label utility. */
	TSharedRef<SWidget> MakeWidgetFromString( TSharedPtr<FString> InItem );
	
	/** Slider label. */
	FText GetButtonLabel() const;

	/** Handles display text in preview depth combo box */
	FText HandlePreviewDepthComboBoxContent() const;

	/** A general callback for the combo boxes in the Destructible Mesh Editor to force a viewport refresh when a selection changes. */
	void ComboBoxSelectionChanged( TSharedPtr<FString> NewSelection );

	/** A callback for when the preview depth is selected, refreshes the viewport. */
	void PreviewDepthSelectionChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo  );

	/** A callback to read the current explode amount slider position. */
	float GetExplodeAmountSliderPosition() const;

	/** A callback for when the explode amount slider position is changed. */
	void OnSetExplodeAmount(float NewValue);

	/** Rebuilds the PreviewDepth combo list. */
	void RegeneratePreviewDepthComboList();

private:

	/** Preview Viewport widget */
	TSharedPtr<class SDestructibleMeshEditorViewport>	Viewport;

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<class SDockableTab> >			SpawnedToolPanels;

	/** Property View */
	TSharedPtr<class IDetailsView>						DestructibleMeshDetailsView;
	TSharedPtr<class SDockTab>							DestructibleMeshDetailsViewTab;

	/** FractureSettings View */
	TSharedPtr<class IDetailsView>						DestructibleFractureSettingsView;

	/** Chunk Parameters View */
	TSharedPtr<class IDetailsView>						ChunkParametersView;
	TSharedPtr<class SDockTab>							ChunkParametersViewTab;


	/** Widget for displaying the available LOD. */
	TSharedPtr<class SComboBox< TSharedPtr<FString> > > PreviewDepthCombo;

	/** List of LODs. */
	TArray< TSharedPtr< FString > >						PreviewDepths;

	/** Widget for adjusting the explode amount. */
	TSharedPtr< SSlider >								ExplodeAmountSlider;

	/** The current explode amount (scaled to the maximum explode range), as a fraction of the mesh size. */
	float												ExplodeFractionOfRange;

	/* The DestructibleMesh that is active in the editor */
	UDestructibleMesh*									DestructibleMesh;

	/**	The tab ids for all the tabs used */
	static const FName ViewportTabId;
	static const FName PropertiesTabId;
	static const FName FractureSettingsTabId;
	static const FName ChunkParametersTabId;
};
