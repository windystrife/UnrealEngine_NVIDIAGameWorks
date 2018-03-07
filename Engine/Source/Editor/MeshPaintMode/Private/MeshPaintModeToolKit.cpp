// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintModeToolKit.h"
#include "MeshPaintEdMode.h"
#include "IMeshPainter.h"

#define LOCTEXT_NAMESPACE "MeshPaintToolKit"

FMeshPaintModeToolKit::FMeshPaintModeToolKit(class FEdModeMeshPaint* InOwningMode)
	: MeshPaintEdMode(InOwningMode)
{
}

void FMeshPaintModeToolKit::Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost)
{
	FModeToolkit::Init(InitToolkitHost);
}

FName FMeshPaintModeToolKit::GetToolkitFName() const
{
	return FName("MeshPaintMode");
}

FText FMeshPaintModeToolKit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Mesh Paint");
}

class FEdMode* FMeshPaintModeToolKit::GetEditorMode() const
{
	return MeshPaintEdMode;
}

TSharedPtr<SWidget> FMeshPaintModeToolKit::GetInlineContent() const
{
	return MeshPaintEdMode->GetMeshPainter()->GetWidget();
}

#undef LOCTEXT_NAMESPACE // "MeshPaintToolKit"