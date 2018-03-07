// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintEdMode.h"
#include "EdMode.h"
#include "MeshPaintModeToolKit.h"
#include "EditorModeManager.h"

#include "PaintModePainter.h"

#define LOCTEXT_NAMESPACE "EdModeMeshPaint"

void FEdModeMeshPaint::Initialize()
{
	MeshPainter = FPaintModePainter::Get();
}

TSharedPtr<class FModeToolkit> FEdModeMeshPaint::GetToolkit()
{
	return MakeShareable(new FMeshPaintModeToolKit(this));
}

#undef LOCTEXT_NAMESPACE // "FEdModeMeshPaint"
