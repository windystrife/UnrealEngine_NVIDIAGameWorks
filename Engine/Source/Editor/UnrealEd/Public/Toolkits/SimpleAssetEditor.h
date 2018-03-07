// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Editor/PropertyEditor/Public/PropertyEditorDelegates.h"

class IDetailsView;
class SDockableTab;

class UNREALED_API FSimpleAssetEditor : public FAssetEditorToolkit
{
public:
	/** Delegate that, given an array of assets, returns an array of objects to use in the details view of an FSimpleAssetEditor */
	DECLARE_DELEGATE_RetVal_OneParam(TArray<UObject*>, FGetDetailsViewObjects, const TArray<UObject*>&);

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;


	/**
	 * Edits the specified asset object
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	ObjectsToEdit			The object to edit
	 * @param	GetDetailsViewObjects	If bound, a delegate to get the array of objects to use in the details view; uses ObjectsToEdit if not bound
	 */
	void InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects );

	/** Destructor */
	virtual ~FSimpleAssetEditor();

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual bool IsPrimaryEditor() const override { return true; }
	
	/** Used to show or hide certain properties */
	void SetPropertyVisibilityDelegate(FIsPropertyVisible InVisibilityDelegate);

private:
	/** Create the properties tab and its content */
	TSharedRef<SDockTab> SpawnPropertiesTab( const FSpawnTabArgs& Args );

	/** Handles when an asset is imported */
	void HandleAssetPostImport(class UFactory* InFactory, UObject* InObject);

	/** Dockable tab for properties */
	TSharedPtr< SDockableTab > PropertiesTab;

	/** Details view */
	TSharedPtr< class IDetailsView > DetailsView;

	/** App Identifier. Technically, all simple editors are the same app, despite editing a variety of assets. */
	static const FName SimpleEditorAppIdentifier;

	/**	The tab ids for all the tabs used */
	static const FName PropertiesTabId;

	/** The objects open within this editor */
	TArray<UObject*> EditingObjects;

public:
	/** The name given to all instances of this type of editor */
	static const FName ToolkitFName;

	static TSharedRef<FSimpleAssetEditor> CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit, FGetDetailsViewObjects GetDetailsViewObjects = FGetDetailsViewObjects() );

	static TSharedRef<FSimpleAssetEditor> CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects = FGetDetailsViewObjects() );
};
