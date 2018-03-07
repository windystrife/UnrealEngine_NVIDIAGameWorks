// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WorkflowOrientedApp/ApplicationMode.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Docking/LayoutService.h"

#define LOCTEXT_NAMESPACE "ApplicationMode"

/////////////////////////////////////////////////////
// FApplicationMode

FApplicationMode::FApplicationMode(FName InModeName)
	: ModeName(InModeName)
{
	ToolbarExtender = MakeShareable(new FExtender);
	WorkspaceMenuCategory = FWorkspaceItem::NewGroup(LOCTEXT("WorkspaceMenu_AssetEditor", "Asset Editor"));
}

FApplicationMode::FApplicationMode(FName InModeName, FText(*GetLocalizedMode)(const FName))
	: ModeName(InModeName)
{
	ToolbarExtender = MakeShareable(new FExtender);
	WorkspaceMenuCategory = FWorkspaceItem::NewGroup(FText::Format(LOCTEXT("WorkspaceMenu_ApplicationMode", "{0} Editor"), GetLocalizedMode(InModeName)));
}

void FApplicationMode::DeactivateMode(TSharedPtr<FTabManager> InTabManager)
{
	// Save the layout to INI
	check(InTabManager.IsValid());
	FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, InTabManager->PersistLayout());
		
	// Unregister the tabs
	/*
	for (int32 Index = 0; Index < AllowableTabs.Num(); ++Index)
	{
		const FName TabName = AllowableTabs[Index];

		PinnedTabManager->UnregisterTabSpawner(TabName);
	}
	*/
}

TSharedRef<FTabManager::FLayout> FApplicationMode::ActivateMode(TSharedPtr<FTabManager> InTabManager)
{
	check(InTabManager.IsValid());
	RegisterTabFactories(InTabManager);

	/*
	for (int32 Index = 0; Index < AllowableTabs.Num(); ++Index)
	{
		const FName TabName = AllowableTabs[Index];

		PinnedTabManager->RegisterTabSpawner(TabName, FOnSpawnTab::CreateSP(this, &FApplicationMode::SpawnTab, TabName));
	}
	*/

	// Try loading the layout from INI
	check(TabLayout.IsValid());
	return FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, TabLayout.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE
