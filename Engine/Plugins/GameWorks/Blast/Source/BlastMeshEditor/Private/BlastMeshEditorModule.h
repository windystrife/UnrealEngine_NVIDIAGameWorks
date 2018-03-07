// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"
#include "IAssetTypeActions.h"
#include "EditorBuildUtils.h"
#include "LevelEditor.h"
#include "IBlastMeshEditorModule.h"

class IBlastMeshEditor;
class UBlastMesh;
class UStaticMesh;

extern const FName BlastMeshEditorAppIdentifier;

class IBlastMeshEditor;

DECLARE_LOG_CATEGORY_EXTERN(LogBlastMeshEditor, Verbose, All);

/** BlastMesh Editor module */
class FBlastMeshEditorModule : public IBlastMeshEditorModule
{

public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Creates an instance of table editor object.  Only virtual so that it can be called across the DLL boundary.
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	Table					The table to start editing
	 *
	 * @return	Interface to the new table editor
	 */
	virtual TSharedRef<IBlastMeshEditor> CreateBlastMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UBlastMesh* Table ) override;

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override {return ToolBarExtensibilityManager;}

	static const int32 MaxChunkDepth;

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};


