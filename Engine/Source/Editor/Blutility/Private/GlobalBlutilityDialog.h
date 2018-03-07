// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"

class IDetailsView;
class SDockableTab;
class UGlobalEditorUtilityBase;

//////////////////////////////////////////////////////////////////////////
// FGlobalBlutilityDialog

class FGlobalBlutilityDialog : public FAssetEditorToolkit, public FGCObject
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	TSharedRef<SDockTab> SpawnTab_DetailsPanel( const FSpawnTabArgs& SpawnTabArgs );

	/**
	 * Edits the specified sound class object
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	ObjectToEdit			The sound class to edit
	 */
	void InitBlutilityDialog(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit);

	/**
	 * Updates the editor's property window to contain sound class properties if any are selected.
	 */
	void UpdatePropertyWindow( const TArray<UObject*>& SelectedObjects );

	virtual ~FGlobalBlutilityDialog();

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;

	/** @return Returns the color and opacity to use for the color that appears behind the tab text for this toolkit's tab in world-centric mode. */
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

private:

	/** Creates all internal widgets for the tabs to point at */
	void CreateInternalWidgets();

private:
	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<class SDockableTab> > SpawnedToolPanels;

	/** Property View */
	TSharedPtr<class IDetailsView> DetailsView;

	TWeakObjectPtr<UGlobalEditorUtilityBase> BlutilityInstance;
};
