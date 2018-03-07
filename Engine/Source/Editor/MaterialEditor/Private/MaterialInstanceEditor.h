// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Misc/NotifyHook.h"
#include "EditorUndoClient.h"
#include "Toolkits/IToolkitHost.h"
#include "IMaterialEditor.h"
#include "IDetailsView.h"
#include "SMaterialEditorViewport.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class FCanvas;
class UMaterialEditorInstanceConstant;
class UMaterialInterface;
template <typename ItemType> class SListView;

/**
 * Material Instance Editor class
 */
class FMaterialInstanceEditor : public IMaterialEditor, public FNotifyHook, public FGCObject, public FEditorUndoClient
{
public:
	
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/**
	 * Edits the specified material instance object
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	ObjectToEdit			The material instance to edit
	 */
	void InitMaterialInstanceEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit );

	FMaterialInstanceEditor();

	virtual ~FMaterialInstanceEditor();

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;

	/** @return Returns the color and opacity to use for the color that appears behind the tab text for this toolkit's tab in world-centric mode. */
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** The material instance applied to the preview mesh. */
	virtual UMaterialInterface* GetMaterialInterface() const override;

	/** Pre edit change notify for properties. */
	virtual void NotifyPreChange( UProperty* PropertyAboutToChange ) override;

	/** Post edit change notify for properties. */
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged ) override;

	void PreSavePackage(UPackage* Obj);

	/** Rebuilds the inheritance list for this material instance. */
	void RebuildInheritanceList();

	/** Rebuilds the editor when the original material changes */
	void RebuildMaterialInstanceEditor();

	/**
	 * Draws messages on the specified viewport and canvas.
	 */
	virtual void DrawMessages( FViewport* Viewport, FCanvas* Canvas ) override;

	/**
	 * Draws sampler/texture mismatch warning strings.
	 * @param Canvas - The canvas on which to draw.
	 * @param DrawPositionY - The Y position at which to draw. Upon return contains the Y value following the last line of text drawn.
	 */
	void DrawSamplerWarningStrings(FCanvas* Canvas, int32& DrawPositionY);

	/** Passes instructions to the preview viewport */
	bool SetPreviewAsset(UObject* InAsset);
	bool SetPreviewAssetByName(const TCHAR* InMeshName);
	void SetPreviewMaterial(UMaterialInterface* InMaterialInterface);

	/** Returns true if hidden parameters should be shown */
	void GetShowHiddenParameters(bool& bShowHiddenParameters);

	/** Gets the extensibility managers for outside entities to extend material editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() { return ToolBarExtensibilityManager; }

protected:
	/** Saves editor settings. */
	void SaveSettings();

	/** Loads editor settings. */
	void LoadSettings();

	/** Syncs the GB to the selected parent in the inheritance list. */
	void SyncSelectedParentToGB();

	/** Opens the editor for the selected parent. */
	void OpenSelectedParentEditor(UMaterialInterface* InMaterialInterface);

	/** Updated the properties pane */
	void UpdatePropertyWindow();

	virtual UObject* GetSyncObject();

private:
	/** Binds our UI commands to delegates */
	void BindCommands();

	/** Commands for the ShowAllMaterialParametersEnabled button */
	void ToggleShowAllMaterialParameters();
	bool IsShowAllMaterialParametersChecked() const;

	/** Commands for the ToggleMobileStats button */
	void ToggleMobileStats();
	bool IsToggleMobileStatsChecked() const;

	/** Commands for the Parents menu */
	void OnOpenMaterial();
	void OnShowInContentBrowser();

	/** Commands for the Inheritance list */
	void OnInheritanceListDoubleClick( TWeakObjectPtr<UMaterialInterface> InMaterialInterface );
	TSharedPtr<SWidget> OnInheritanceListRightClick();
	TSharedRef<ITableRow> OnInheritanceListGenerateRow( TWeakObjectPtr<UMaterialInterface> InMaterialInterface, const TSharedRef<STableViewBase>& OwnerTable );

	/** Returns the currently selected item from the parents list */
	UMaterialInterface* GetSelectedParent() const;

	/** Creates all internal widgets for the tabs to point at */
	void CreateInternalWidgets();

	/** Updates the 3D and UI preview viewport visibility based on material domain */
	void UpdatePreviewViewportsVisibility();

	/** Builds the toolbar widget for the material editor */
	void ExtendToolbar();

	//~ Begin IMaterialEditor Interface
	virtual bool ApproveSetPreviewAsset(UObject* InAsset) override;

	/**	Spawns the preview tab */
	TSharedRef<SDockTab> SpawnTab_Preview( const FSpawnTabArgs& Args );

	/**	Spawns the properties tab */
	TSharedRef<SDockTab> SpawnTab_Properties( const FSpawnTabArgs& Args );

	/**	Spawns the parents tab */
	TSharedRef<SDockTab> SpawnTab_Parents( const FSpawnTabArgs& Args );

	/** Spawns the advanced preview settings tab */
	TSharedRef<SDockTab> SpawnTab_PreviewSettings(const FSpawnTabArgs& Args);

	/**	Caches the specified tab for later retrieval */
	void AddToSpawnedToolPanels( const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab );

	/**	Refresh the viewport and property window */
	void Refresh();

	/** Refreshes the preview asset */
	void RefreshPreviewAsset();

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo( bool bSuccess ) override;
	virtual void PostRedo( bool bSuccess ) override;
	// End of FEditorUndoClient

private:

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<class SDockTab> > SpawnedToolPanels;

	/** Preview Viewport widget */
	TSharedPtr<class SMaterialEditor3DPreviewViewport> PreviewVC;

	/** Preview viewport widget used for UI materials */
	TSharedPtr<class SMaterialEditorUIPreviewViewport> PreviewUIViewport;

	/** Property View */
	TSharedPtr<class IDetailsView> MaterialInstanceDetails;

	/** Parent View */
	TSharedPtr<class SListView<TWeakObjectPtr<UMaterialInterface>>> MaterialInstanceParentsList;

	/** Object that stores all of the possible parameters we can edit. */
	UMaterialEditorInstanceConstant* MaterialEditorInstance;

	/** List of parents used to populate the inheritance list chain. */
	TArray< TWeakObjectPtr<UMaterialInterface> > ParentList;

	/** Whether or not we should be displaying all the material parameters */
	bool bShowAllMaterialParameters;

	/** Whether to show mobile material stats. */
	bool bShowMobileStats;

	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	/**	The ids for the tabs spawned by this toolkit */
	static const FName PreviewTabId;		
	static const FName PropertiesTabId;	
	static const FName ParentsTabId;	
	static const FName PreviewSettingsTabId;
};
