// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class UDestructibleMesh;

/** DestructibleMesh Editor public interface */
class IDestructibleMeshEditor : public FAssetEditorToolkit
{

public:
	/** Returns the UDestructibleMesh being edited. */
	virtual UDestructibleMesh* GetDestructibleMesh() { return NULL; }

	/** Returns the current preview depth selected in the UI */
	virtual int32 GetCurrentPreviewDepth() const { return 0; }

	/** Sets the current preview depth */
	virtual void SetCurrentPreviewDepth(uint32 InPreviewDepthDepth) {}

	/** Refreshes the Destructible Mesh Editor's viewport. */
	virtual void RefreshViewport() {}

	/** Refreshes everything in the Destructible Mesh Editor. */
	virtual void RefreshTool() {}

	/** Called after a data table was reloaded */
	virtual void OnDestructibleMeshReloaded(){}

};


