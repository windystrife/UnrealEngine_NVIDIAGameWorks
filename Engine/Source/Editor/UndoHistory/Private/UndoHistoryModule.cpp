// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UndoHistoryModule.h"
#include "EditorStyleSet.h"
#include "Widgets/SUndoHistory.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FUndoHistoryModule"

void FUndoHistoryModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UndoHistoryTabName, FOnSpawnTab::CreateRaw(this, &FUndoHistoryModule::HandleSpawnSettingsTab))
		.SetDisplayName(NSLOCTEXT("FUndoHistoryModule", "UndoHistoryTabTitle", "Undo History"))
		.SetTooltipText(NSLOCTEXT("FUndoHistoryModule", "UndoHistoryTooltipText", "Open the Undo History tab."))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "UndoHistory.TabIcon"))
		.SetAutoGenerateMenuEntry(false);
}

void FUndoHistoryModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UndoHistoryTabName);
}

bool FUndoHistoryModule::SupportsDynamicReloading()
{
	return true;
}

TSharedRef<SDockTab> FUndoHistoryModule::HandleSpawnSettingsTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	DockTab->SetContent(SNew(SUndoHistory));

	return DockTab;
}


IMPLEMENT_MODULE(FUndoHistoryModule, UndoHistory);


#undef LOCTEXT_NAMESPACE
