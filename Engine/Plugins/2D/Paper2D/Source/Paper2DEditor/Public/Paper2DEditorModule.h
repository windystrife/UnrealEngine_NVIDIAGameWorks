// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Modules/ModuleInterface.h"

#define PAPER2D_EDITOR_MODULE_NAME "Paper2DEditor"

//////////////////////////////////////////////////////////////////////////
// IPaper2DEditorModule

class IPaper2DEditorModule : public IModuleInterface
{
public:
	virtual TSharedPtr<class FExtensibilityManager> GetSpriteEditorMenuExtensibilityManager() { return nullptr; }
	virtual TSharedPtr<class FExtensibilityManager> GetSpriteEditorToolBarExtensibilityManager() { return nullptr; }

	virtual TSharedPtr<class FExtensibilityManager> GetFlipbookEditorMenuExtensibilityManager() { return nullptr; }
	virtual TSharedPtr<class FExtensibilityManager> GetFlipbookEditorToolBarExtensibilityManager() { return nullptr; }

	virtual uint32 GetPaper2DAssetCategory() const = 0;
};

