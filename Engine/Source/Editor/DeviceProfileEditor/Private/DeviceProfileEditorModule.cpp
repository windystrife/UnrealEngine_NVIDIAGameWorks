// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DeviceProfileEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "SDeviceProfileEditor.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "DeviceProfileEditor"

IMPLEMENT_MODULE(FDeviceProfileEditorModule, DeviceProfileEditor);

static const FName DeviceProfileEditorName("DeviceProfileEditor");


void FDeviceProfileEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DeviceProfileEditorName, FOnSpawnTab::CreateStatic(&FDeviceProfileEditorModule::SpawnDeviceProfileEditorTab))
		.SetDisplayName( NSLOCTEXT("DeviceProfileEditor", "DeviceProfileEditorTitle", "Device Profiles") )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "DeviceDetails.Tabs.ProfileEditor"))
		.SetGroup( WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory() );
}


void FDeviceProfileEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DeviceProfileEditorName);
}


TSharedRef<SDockTab> FDeviceProfileEditorModule::SpawnDeviceProfileEditorTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab)
		.TabRole(ETabRole::MajorTab);

	const TSharedRef<SDeviceProfileEditor> DeviceProfileEditor = SNew(SDeviceProfileEditor);

	MajorTab->SetContent(DeviceProfileEditor);

	return MajorTab;
}


#undef LOCTEXT_NAMESPACE
