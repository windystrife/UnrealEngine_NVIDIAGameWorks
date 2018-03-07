// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Docking/TabManager.h"
#include "Toolkits/IToolkitHost.h"
#include "CodeEditorStyle.h"
#include "CodeProject.h"
#include "CodeProjectEditor.h"
#include "LevelEditor.h"

static const FName CodeEditorTabName( TEXT( "CodeEditor" ) );

#define LOCTEXT_NAMESPACE "CodeEditor"

class FCodeEditor : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		FCodeEditorStyle::Initialize();

		struct Local
		{
			static TSharedRef<SDockTab> SpawnCodeEditorTab(const FSpawnTabArgs& TabArgs)
			{
				TSharedRef<FCodeProjectEditor> NewCodeProjectEditor(new FCodeProjectEditor());
				NewCodeProjectEditor->InitCodeEditor(EToolkitMode::Standalone, TSharedPtr<class IToolkitHost>(), GetMutableDefault<UCodeProject>());

				return FGlobalTabmanager::Get()->GetMajorTabForTabManager(NewCodeProjectEditor->GetTabManager().ToSharedRef()).ToSharedRef();
			}

			static void OpenCodeEditor()
			{
				SpawnCodeEditorTab(FSpawnTabArgs(TSharedPtr<SWindow>(), FTabId()));	
			}

			static void ExtendMenu(class FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.AddMenuEntry
				(
					LOCTEXT( "CodeEditorTabTitle", "Edit Source Code" ),
					LOCTEXT( "CodeEditorTooltipText", "Open the Code Editor tab." ),
					FSlateIcon(FCodeEditorStyle::Get().GetStyleSetName(), "CodeEditor.TabIcon"),
					FUIAction
					(
						FExecuteAction::CreateStatic(&Local::OpenCodeEditor)
					)
				);
			}
		};

		Extender = MakeShareable(new FExtender());
	
		// Add code editor extension to main menu
		Extender->AddMenuExtension(
			"FileProject",
			EExtensionHook::First,
			TSharedPtr<FUICommandList>(),
			FMenuExtensionDelegate::CreateStatic( &Local::ExtendMenu ) );

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender( Extender );

		// Register a tab spawner so that our tab can be automatically restored from layout files
		FGlobalTabmanager::Get()->RegisterTabSpawner( CodeEditorTabName, FOnSpawnTab::CreateStatic( &Local::SpawnCodeEditorTab ) )
				.SetDisplayName( LOCTEXT( "CodeEditorTabTitle", "Edit Source Code" ) )
				.SetTooltipText( LOCTEXT( "CodeEditorTooltipText", "Open the Code Editor tab." ) )
				.SetIcon(FSlateIcon(FCodeEditorStyle::Get().GetStyleSetName(), "CodeEditor.TabIcon"));
	}

	virtual void ShutdownModule() override
	{
		// Unregister the tab spawner
		FGlobalTabmanager::Get()->UnregisterTabSpawner( CodeEditorTabName );

		if(FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender( Extender );		
		}

		FCodeEditorStyle::Shutdown();
	}

private:
	TSharedPtr<FExtender> Extender; 
};

IMPLEMENT_MODULE( FCodeEditor, CodeEditor )

#undef LOCTEXT_NAMESPACE
