// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterface.h"
#include "AssetEditorToolkit.h"

class IBlastMeshEditor;
class UBlastMesh;
class UStaticMesh;

/** BlastMesh Editor module */
class IBlastMeshEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{

public:
	/**
	 * Creates an instance of table editor object.  Only virtual so that it can be called across the DLL boundary.
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	Table					The table to start editing
	 *
	 * @return	Interface to the new table editor
	 */
	virtual TSharedRef<IBlastMeshEditor> CreateBlastMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UBlastMesh* Table ) = 0;
};


