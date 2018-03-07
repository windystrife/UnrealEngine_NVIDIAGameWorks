// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Docking/TabManager.h"
#include "IBlutilityModule.h"
#include "EditorUtilityBlueprint.h"


#include "AssetToolsModule.h"
#include "PropertyEditorModule.h"
#include "AssetTypeActions_EditorUtilityBlueprint.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#include "BlutilityDetailsPanel.h"
#include "BlutilityShelf.h"
#include "Widgets/Docking/SDockTab.h"

/////////////////////////////////////////////////////

namespace BlutilityModule
{
	static const FName BlutilityShelfApp = FName(TEXT("BlutilityShelfApp"));
}

/////////////////////////////////////////////////////
// FBlutilityModule 

// Blutility module implementation (private)
class FBlutilityModule : public IBlutilityModule
{
public:
	/** Asset type actions for MovieScene assets.  Cached here so that we can unregister it during shutdown. */
	TSharedPtr<FAssetTypeActions_EditorUtilityBlueprint> EditorBlueprintAssetTypeActions;

public:
	virtual void StartupModule() override
	{
		// Register the asset type
		EditorBlueprintAssetTypeActions = MakeShareable(new FAssetTypeActions_EditorUtilityBlueprint);
		FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RegisterAssetTypeActions(EditorBlueprintAssetTypeActions.ToSharedRef());
		
		// Register the details customizer
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("PlacedEditorUtilityBase", FOnGetDetailCustomizationInstance::CreateStatic(&FEditorUtilityInstanceDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("GlobalEditorUtilityBase", FOnGetDetailCustomizationInstance::CreateStatic(&FEditorUtilityInstanceDetails::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();

		FGlobalTabmanager::Get()->RegisterTabSpawner(BlutilityModule::BlutilityShelfApp, FOnSpawnTab::CreateStatic(&SpawnBlutilityShelfTab))
			.SetDisplayName(NSLOCTEXT("BlutilityShelf", "TabTitle", "Blutility Shelf"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
	}

	virtual void ShutdownModule() override
	{
		if (!UObjectInitialized())
		{
			return;
		}

		FGlobalTabmanager::Get()->UnregisterTabSpawner(BlutilityModule::BlutilityShelfApp);

		// Only unregister if the asset tools module is loaded.  We don't want to forcibly load it during shutdown phase.
		check( EditorBlueprintAssetTypeActions.IsValid() );
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().UnregisterAssetTypeActions(EditorBlueprintAssetTypeActions.ToSharedRef());
		}
		EditorBlueprintAssetTypeActions.Reset();

		// Unregister the details customization
		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout("PlacedEditorUtilityBase");
			PropertyModule.UnregisterCustomClassLayout("GlobalEditorUtilityBase");
			PropertyModule.NotifyCustomizationModuleChanged();
		}
	}

	virtual bool IsBlutility( const UBlueprint* Blueprint ) const override
	{
		const UClass* BPClass = Blueprint ? Blueprint->GetClass() : nullptr;

		if( BPClass && BPClass->IsChildOf( UEditorUtilityBlueprint::StaticClass() ))
		{
			return true;
		}
		return false;
	}

protected:
	static TSharedRef<SDockTab> SpawnBlutilityShelfTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SBlutilityShelf)
			];
	}
};



IMPLEMENT_MODULE( FBlutilityModule, Blutility );
