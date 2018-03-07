// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkitHost.h"
#include "Toolkits/BaseToolkit.h"

/**
* Mode Toolkit for the Mesh Paint Mode
*/
class FMeshPaintModeToolKit : public FModeToolkit
{
public:
	FMeshPaintModeToolKit(class FEdModeMeshPaint* InOwningMode);
	
	/** Initializes the geometry mode toolkit */
	virtual void Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override;

private:
	/** Owning editor mode */
	class FEdModeMeshPaint* MeshPaintEdMode;
};
