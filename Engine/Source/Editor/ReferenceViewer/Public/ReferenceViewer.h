// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "AssetData.h"

/**
 * The public interface to this module
 */
class IReferenceViewerModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IReferenceViewerModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IReferenceViewerModule >( "ReferenceViewer" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "ReferenceViewer" );
	}

	/** Invokes a major tab with a reference viewer within it */
	virtual void InvokeReferenceViewerTab(const TArray<FName>& GraphRootPackageNames)
	{
		TArray<FAssetIdentifier> Identifiers;
		for (FName Name : GraphRootPackageNames)
		{
			Identifiers.Add(FAssetIdentifier(Name));
		}

		InvokeReferenceViewerTab(Identifiers);
	}

	/** Invokes a major tab with a reference viewer within it */
	virtual void InvokeReferenceViewerTab(const TArray<FAssetIdentifier>& GraphRootIdentifiers) = 0;

	/** Call from a menu extender in game/plugin to get selected asset list. If this returns false, it wasn't called on the right node */
	virtual bool GetSelectedAssetsForMenuExtender(const class UEdGraph* Graph, const class UEdGraphNode* Node, TArray<FAssetIdentifier>& SelectedAssets) = 0;
};

