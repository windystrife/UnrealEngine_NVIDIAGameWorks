// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ConfigEditorModule.h"
#include "UObject/UnrealType.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "SConfigEditor.h"

#include "Widgets/Docking/SDockTab.h"

DEFINE_LOG_CATEGORY_STATIC(ConfigEditor, Log, All);

/*-----------------------------------------------------------------------------
   FConfigEditorModule
-----------------------------------------------------------------------------*/

namespace ConfigEditorModule
{
	static const FName ConfigEditorId = FName(TEXT("ConfigEditor"));
}


void FConfigEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ConfigEditorModule::ConfigEditorId, FOnSpawnTab::CreateRaw(this, &FConfigEditorModule::SpawnConfigEditorTab))
		.SetDisplayName(NSLOCTEXT("ConfigEditorModule", "TabTitle", "Config Editor"))
		.SetTooltipText(NSLOCTEXT("ConfigEditorModule", "TooltipText", "Open the Config Editor tab."))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "ConfigEditor.TabIcon"));
}


void FConfigEditorModule::ShutdownModule()
{

}


void FConfigEditorModule::AddExternalPropertyValueWidgetAndConfigPairing(const FString& ConfigFile, const TSharedPtr<SWidget> ValueWidget)
{
	ExternalPropertyValueWidgetAndConfigPairings.Add(ConfigFile, ValueWidget);
}


TSharedRef<SWidget> FConfigEditorModule::GetValueWidgetForConfigProperty(const FString& ConfigFile)
{
	TSharedPtr<SWidget>* ValueWidget = ExternalPropertyValueWidgetAndConfigPairings.Find(ConfigFile);
	return (ValueWidget != nullptr && (*ValueWidget).IsValid()) ? (*ValueWidget).ToSharedRef() : SNullWidget::NullWidget;
}


TSharedRef<SDockTab> FConfigEditorModule::SpawnConfigEditorTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SAssignNew(PropertyConfigEditor, SConfigEditor, CachedPropertyToView)
		];
}


void FConfigEditorModule::CreateHierarchyEditor(UProperty* InEditProperty)
{
	CachedPropertyToView = InEditProperty;
}


IMPLEMENT_MODULE(FConfigEditorModule, ConfigEditor);
