// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"

class UPhysicsAsset;

DECLARE_LOG_CATEGORY_EXTERN(LogPhysicsAssetEditor, Log, All);


/*-----------------------------------------------------------------------------
   IPhysicsAssetEditorModule
-----------------------------------------------------------------------------*/

class IPhysicsAssetEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/** Creates a new PhysicsAssetEditor instance */
	virtual TSharedRef<class IPhysicsAssetEditor> CreatePhysicsAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UPhysicsAsset* PhysicsAsset) = 0;

	/** Opens a "New Asset/Body" modal dialog window */
	virtual void OpenNewBodyDlg(EAppReturnType::Type* NewBodyResponse) = 0;
};

