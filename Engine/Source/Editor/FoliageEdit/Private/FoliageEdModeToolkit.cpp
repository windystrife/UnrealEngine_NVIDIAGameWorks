// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FoliageEdModeToolkit.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "SFoliageEdit.h"

#define LOCTEXT_NAMESPACE "FoliageEditMode"

void FFoliageEdModeToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{

}

void FFoliageEdModeToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{

}

void FFoliageEdModeToolkit::Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost)
{
	FoliageEdWidget = SNew(SFoliageEdit);

	FModeToolkit::Init(InitToolkitHost);
}

FName FFoliageEdModeToolkit::GetToolkitFName() const
{
	return FName("FoliageEditMode");
}

FText FFoliageEdModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT( "ToolkitName", "Foliage Edit Mode" );
}

class FEdMode* FFoliageEdModeToolkit::GetEditorMode() const
{
	return GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);
}

TSharedPtr<SWidget> FFoliageEdModeToolkit::GetInlineContent() const
{
	return FoliageEdWidget;
}

void FFoliageEdModeToolkit::RefreshFullList()
{
	FoliageEdWidget->RefreshFullList();
}

void FFoliageEdModeToolkit::NotifyFoliageTypeMeshChanged(class UFoliageType* FoliageType)
{
	FoliageEdWidget->NotifyFoliageTypeMeshChanged(FoliageType);
}

#undef LOCTEXT_NAMESPACE
