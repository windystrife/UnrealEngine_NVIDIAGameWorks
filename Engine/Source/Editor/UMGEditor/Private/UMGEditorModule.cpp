// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "Editor.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Settings/WidgetDesignerSettings.h"
#include "WidgetBlueprint.h"

#include "AssetToolsModule.h"
#include "IAssetTypeActions.h"
#include "AssetTypeActions_WidgetBlueprint.h"
#include "KismetCompilerModule.h"
#include "WidgetBlueprintCompiler.h"

#include "ISequencerModule.h"
#include "Animation/MarginTrackEditor.h"
#include "Animation/Sequencer2DTransformTrackEditor.h"
#include "Animation/WidgetMaterialTrackEditor.h"
#include "IUMGModule.h"
#include "ComponentReregisterContext.h"
#include "Components/WidgetComponent.h"
#include "Designer/DesignerCommands.h"

#include "ClassIconFinder.h"

#include "UMGEditorProjectSettings.h"
#include "ISettingsModule.h"
#include "SequencerSettings.h"

#define LOCTEXT_NAMESPACE "UMG"

const FName UMGEditorAppIdentifier = FName(TEXT("UMGEditorApp"));

PRAGMA_DISABLE_DEPRECATION_WARNINGS
const FSlateBrush* GetEditorIcon_Deprecated(UWidget* Widget)
{
	const FSlateBrush* Brush = Widget->GetEditorIcon();
	return Brush ? Brush : FClassIconFinder::FindIconForClass(Widget->GetClass());
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

class FUMGEditorModule : public IUMGEditorModule, public IBlueprintCompiler, public FGCObject
{
public:
	/** Constructor, set up console commands and variables **/
	FUMGEditorModule()
		: ReRegister(nullptr)
		, CompileCount(0)
		, Settings(nullptr)
	{
	}

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override
	{
		FModuleManager::LoadModuleChecked<IUMGModule>("UMG");

		if (GIsEditor)
		{
			FDesignerCommands::Register();
		}

		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager());
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager());

		// Register widget blueprint compiler we do this no matter what.
		IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
		KismetCompilerModule.GetCompilers().Add(this);

		// Register asset types
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_WidgetBlueprint()));

		// Register with the sequencer module that we provide auto-key handlers.
		ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
		MarginTrackEditorCreateTrackEditorHandle          = SequencerModule.RegisterPropertyTrackEditor<FMarginTrackEditor>();
		TransformTrackEditorCreateTrackEditorHandle       = SequencerModule.RegisterPropertyTrackEditor<F2DTransformTrackEditor>();
		WidgetMaterialTrackEditorCreateTrackEditorHandle  = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FWidgetMaterialTrackEditor::CreateTrackEditor));

		RegisterSettings();
	}

	/** Called before the module is unloaded, right before the module object is destroyed. */
	virtual void ShutdownModule() override
	{
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();

		// Unregister all the asset types that we registered
		if ( FModuleManager::Get().IsModuleLoaded("AssetTools") )
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
			for ( int32 Index = 0; Index < CreatedAssetTypeActions.Num(); ++Index )
			{
				AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeActions[Index].ToSharedRef());
			}
		}
		CreatedAssetTypeActions.Empty();

		// Unregister sequencer track creation delegates
		ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>( "Sequencer" );
		if ( SequencerModule != nullptr )
		{
			SequencerModule->UnRegisterTrackEditor( MarginTrackEditorCreateTrackEditorHandle );
			SequencerModule->UnRegisterTrackEditor( TransformTrackEditorCreateTrackEditorHandle );
			SequencerModule->UnRegisterTrackEditor( WidgetMaterialTrackEditorCreateTrackEditorHandle );
		}

		UnregisterSettings();

		//// Unregister the setting
		//ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		//if ( SettingsModule != nullptr )
		//{
		//	SettingsModule->UnregisterSettings("Editor", "ContentEditors", "WidgetDesigner");
		//	SettingsModule->UnregisterSettings("Project", "Editor", "UMGEditor");
		//}
	}

	bool CanCompile(const UBlueprint* Blueprint) override
	{
		return Cast<UWidgetBlueprint>(Blueprint) != nullptr;
	}

	void PreCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions) override
	{
		if ( ReRegister == nullptr 
			&& CanCompile(Blueprint) 
			&& (CompileOptions.CompileType == EKismetCompileType::Full || CompileOptions.CompileType == EKismetCompileType::Cpp))
		{
			ReRegister = new TComponentReregisterContext<UWidgetComponent>();
		}

		CompileCount++;
	}

	void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TArray<UObject*>* ObjLoaded) override
	{
		if ( UWidgetBlueprint* WidgetBlueprint = CastChecked<UWidgetBlueprint>(Blueprint) )
		{
			FWidgetBlueprintCompiler Compiler(WidgetBlueprint, Results, CompileOptions, ObjLoaded);
			Compiler.Compile();
			check(Compiler.NewClass);
		}
	}

	void PostCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions) override
	{
		CompileCount--;

		if ( CompileCount == 0 && ReRegister )
		{
			delete ReRegister;
			ReRegister = nullptr;

			if ( GIsEditor && GEditor )
			{
				GEditor->RedrawAllViewports(true);
			}
		}
	}

	bool GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const override
	{
		if ( ParentClass == UUserWidget::StaticClass() || ParentClass->IsChildOf(UUserWidget::StaticClass()) )
		{
			OutBlueprintClass = UWidgetBlueprint::StaticClass();
			OutBlueprintGeneratedClass = UWidgetBlueprintGeneratedClass::StaticClass();
			return true;
		}

		return false;
	}

	/** Gets the extensibility managers for outside entities to extend gui page editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

	/** Register settings objects. */
	void RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			Settings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(TEXT("UMGSequencerSettings"));

			SettingsModule->RegisterSettings("Editor", "ContentEditors", "UMGSequencerSettings",
				LOCTEXT("UMGSequencerSettingsSettingsName", "UMG Sequence Editor"),
				LOCTEXT("UMGSequencerSettingsSettingsDescription", "Configure the look and feel of the UMG Sequence Editor."),
				Settings);	
		}
	}

	/** Unregister settings objects. */
	void UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Editor", "ContentEditors", "UMGSequencerSettings");
		}
	}

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		if (Settings)
		{
			Collector.AddReferencedObject(Settings);
		}
	}

private:
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
	{
		AssetTools.RegisterAssetTypeActions(Action);
		CreatedAssetTypeActions.Add(Action);
	}

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	FDelegateHandle MarginTrackEditorCreateTrackEditorHandle;
	FDelegateHandle TransformTrackEditorCreateTrackEditorHandle;
	FDelegateHandle WidgetMaterialTrackEditorCreateTrackEditorHandle;

	/** All created asset type actions.  Cached here so that we can unregister it during shutdown. */
	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;

	/** The temporary variable that captures and reinstances components after compiling finishes. */
	TComponentReregisterContext<UWidgetComponent>* ReRegister;

	/**
	 * The current count on the number of compiles that have occurred.  We don't want to re-register components until all
	 * compiling has stopped.
	 */
	int32 CompileCount;

	USequencerSettings* Settings;
};

IMPLEMENT_MODULE(FUMGEditorModule, UMGEditor);

#undef LOCTEXT_NAMESPACE
