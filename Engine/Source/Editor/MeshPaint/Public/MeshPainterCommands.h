// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

/** Base set of mesh painter commands */
class MESHPAINT_API FMeshPainterCommands : public TCommands<FMeshPainterCommands>
{

public:
	FMeshPainterCommands() : TCommands<FMeshPainterCommands>("MeshPainter", NSLOCTEXT("Contexts", "MeshPainter", "Mesh Painter"), NAME_None, FEditorStyle::GetStyleSetName()) {}

	/**
	* Initialize commands
	*/
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> IncreaseBrushSize;
	TSharedPtr<FUICommandInfo> DecreaseBrushSize;
	TArray<TSharedPtr<FUICommandInfo>> Commands;
};