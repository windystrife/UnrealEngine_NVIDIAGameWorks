// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperEditorViewportClient.h"

class FCanvas;
class UPaperTileSet;

//////////////////////////////////////////////////////////////////////////
// FTileSetEditorViewportClient

class FTileSetEditorViewportClient : public FPaperEditorViewportClient
{
public:
	FTileSetEditorViewportClient(UPaperTileSet* InTileSet);

	// FViewportClient interface
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	// End of FViewportClient interface

	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	// End of FEditorViewportClient interface

	void ToggleShowTilesWithCollision();
	bool IsShowTilesWithCollisionChecked() const;

	void ToggleShowTilesWithMetaData();
	bool IsShowTilesWithMetaDataChecked() const;
public:
	// Tile set
	TWeakObjectPtr<UPaperTileSet> TileSetBeingEdited;

	bool bShowTilesWithCollision;
	bool bShowTilesWithMetaData;
	bool bHasValidPaintRectangle;
	FViewportSelectionRectangle ValidPaintRectangle;
	int32 CurrentSelectedTileIndex;
};
