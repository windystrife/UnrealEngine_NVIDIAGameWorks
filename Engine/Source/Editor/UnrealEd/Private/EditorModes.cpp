// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EditorModes.h"
#include "EdMode.h"


DEFINE_LOG_CATEGORY(LogEditorModes);

// Builtin editor mode constants
const FEditorModeID FBuiltinEditorModes::EM_None = NAME_None;
const FEditorModeID FBuiltinEditorModes::EM_Default(TEXT("EM_Default"));
const FEditorModeID FBuiltinEditorModes::EM_Placement(TEXT("PLACEMENT"));
const FEditorModeID FBuiltinEditorModes::EM_Bsp(TEXT("BSP"));
const FEditorModeID FBuiltinEditorModes::EM_Geometry(TEXT("EM_Geometry"));
const FEditorModeID FBuiltinEditorModes::EM_InterpEdit(TEXT("EM_InterpEdit"));
const FEditorModeID FBuiltinEditorModes::EM_Texture(TEXT("EM_Texture"));
const FEditorModeID FBuiltinEditorModes::EM_MeshPaint(TEXT("EM_MeshPaint"));
const FEditorModeID FBuiltinEditorModes::EM_Landscape(TEXT("EM_Landscape"));
const FEditorModeID FBuiltinEditorModes::EM_Foliage(TEXT("EM_Foliage"));
const FEditorModeID FBuiltinEditorModes::EM_Level(TEXT("EM_Level"));
const FEditorModeID FBuiltinEditorModes::EM_StreamingLevel(TEXT("EM_StreamingLevel"));
const FEditorModeID FBuiltinEditorModes::EM_Physics(TEXT("EM_Physics"));
const FEditorModeID FBuiltinEditorModes::EM_ActorPicker(TEXT("EM_ActorPicker"));
const FEditorModeID FBuiltinEditorModes::EM_SceneDepthPicker(TEXT("EM_SceneDepthPicker"));


/*------------------------------------------------------------------------------
	Default.
------------------------------------------------------------------------------*/

FEdModeDefault::FEdModeDefault()
{
	bDrawGrid = false;
	bDrawPivot = false;
	bDrawBaseInfo = false;
	bDrawWorldBox = false;
	bDrawKillZ = false;
}

