//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioEditorModule.h"

#include "ISettingsModule.h"
#include "Engine/World.h"
#include "SlateStyleRegistry.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "LevelEditorViewport.h"
#include "LevelEditor.h"
#include "PropertyEditorModule.h"
#include "SlateStyle.h"
#include "IPluginManager.h"
#include "ClassIconFinder.h"
#include "Editor.h"

#include "BakeIndirectWindow.h"
#include "TickableNotification.h"
#include "PhononScene.h"
#include "SteamAudioSettings.h"
#include "PhononProbeVolume.h"
#include "PhononProbeVolumeDetails.h"
#include "PhononScene.h"
#include "PhononSceneDetails.h"
#include "PhononProbeComponent.h"
#include "PhononProbeComponentVisualizer.h"
#include "PhononSourceComponent.h"
#include "PhononSourceComponentDetails.h"
#include "PhononSourceComponentVisualizer.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistryModule.h"

#include <atomic>

DEFINE_LOG_CATEGORY(LogSteamAudioEditor);

IMPLEMENT_MODULE(SteamAudio::FSteamAudioEditorModule, SteamAudioEditor)

namespace SteamAudio
{
	//==============================================================================================================================================
	// FSteamAudioEditorModule
	//==============================================================================================================================================

	void FSteamAudioEditorModule::StartupModule()
	{
		// Register detail customizations
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.RegisterCustomClassLayout("PhononProbeVolume", FOnGetDetailCustomizationInstance::CreateStatic(&FPhononProbeVolumeDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("PhononScene", FOnGetDetailCustomizationInstance::CreateStatic(&FPhononSceneDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("PhononSourceComponent",  FOnGetDetailCustomizationInstance::CreateStatic(&FPhononSourceComponentDetails::MakeInstance));

		// Extend the toolbar build menu with custom actions
		if (!IsRunningCommandlet())
		{
			FLevelEditorModule* LevelEditorModule = FModuleManager::LoadModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
			if (LevelEditorModule)
			{
				auto BuildMenuExtender = FLevelEditorModule::FLevelEditorMenuExtender::CreateRaw(this, &FSteamAudioEditorModule::OnExtendLevelEditorBuildMenu);

				LevelEditorModule->GetAllLevelEditorToolbarBuildMenuExtenders().Add(BuildMenuExtender);
			}
		}

		// Register plugin settings
		ISettingsModule* SettingsModule = FModuleManager::Get().GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule)
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "Steam Audio", NSLOCTEXT("SteamAudio", "Steam Audio", "Steam Audio"),
				NSLOCTEXT("SteamAudio", "Configure Steam Audio settings", "Configure Steam Audio settings"), GetMutableDefault<USteamAudioSettings>());
		}

		// Create bake indirect window
		BakeIndirectWindow = MakeShareable(new FBakeIndirectWindow());

		// Create and register custom slate style
		FString SteamAudioContent = IPluginManager::Get().FindPlugin("SteamAudio")->GetBaseDir() + "/Content";
		FVector2D Vec16 = FVector2D(16.0f, 16.0f);
		FVector2D Vec64 = FVector2D(64.0f, 64.0f);

		SteamAudioStyleSet = MakeShareable(new FSlateStyleSet("SteamAudio"));
		SteamAudioStyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
		SteamAudioStyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
		SteamAudioStyleSet->Set("ClassIcon.PhononSourceComponent", new FSlateImageBrush(SteamAudioContent + "/S_PhononSource_16.png", Vec16));
		SteamAudioStyleSet->Set("ClassIcon.PhononGeometryComponent", new FSlateImageBrush(SteamAudioContent + "/S_PhononGeometry_16.png", Vec16));
		SteamAudioStyleSet->Set("ClassIcon.PhononMaterialComponent", new FSlateImageBrush(SteamAudioContent + "/S_PhononMaterial_16.png", Vec16));

		SteamAudioStyleSet->Set("ClassIcon.PhononSpatializationSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononSpatializationSourceSettings_16.png", Vec16));
		SteamAudioStyleSet->Set("ClassThumbnail.PhononSpatializationSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononSpatializationSourceSettings_64.png", Vec64));
		
		SteamAudioStyleSet->Set("ClassIcon.PhononOcclusionSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononOcclusionSourceSettings_16.png", Vec16));
		SteamAudioStyleSet->Set("ClassThumbnail.PhononOcclusionSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononOcclusionSourceSettings_64.png", Vec64));
		
		SteamAudioStyleSet->Set("ClassIcon.PhononReverbSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononReverbSourceSettings_16.png", Vec16));
		SteamAudioStyleSet->Set("ClassThumbnail.PhononReverbSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononReverbSourceSettings_64.png", Vec64));
		FSlateStyleRegistry::RegisterSlateStyle(*SteamAudioStyleSet.Get());

		// Register component visualizers
		RegisterComponentVisualizer(UPhononSourceComponent::StaticClass()->GetFName(), MakeShareable(new FPhononSourceComponentVisualizer()));
		RegisterComponentVisualizer(UPhononProbeComponent::StaticClass()->GetFName(), MakeShareable(new FPhononProbeComponentVisualizer()));
	}

	void FSteamAudioEditorModule::ShutdownModule()
	{
		// Unregister component visualizers
		if (GUnrealEd)
		{
			for (FName& ClassName : RegisteredComponentClassNames)
			{
				GUnrealEd->UnregisterComponentVisualizer(ClassName);
			}
		}

		FSlateStyleRegistry::UnRegisterSlateStyle(*SteamAudioStyleSet.Get());
	}

	void FSteamAudioEditorModule::BakeIndirect()
	{
		BakeIndirectWindow->Invoke();
	}

	void FSteamAudioEditorModule::RegisterComponentVisualizer(const FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer)
	{
		if (GUnrealEd)
		{
			GUnrealEd->RegisterComponentVisualizer(ComponentClassName, Visualizer);
		}

		RegisteredComponentClassNames.Add(ComponentClassName);

		if (Visualizer.IsValid())
		{
			Visualizer->OnRegister();
		}
	}

	TSharedRef<FExtender> FSteamAudioEditorModule::OnExtendLevelEditorBuildMenu(const TSharedRef<FUICommandList> CommandList)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		Extender->AddMenuExtension("LevelEditorNavigation", EExtensionHook::After, nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FSteamAudioEditorModule::CreateBuildMenu));

		return Extender;
	}

	void FSteamAudioEditorModule::CreateBuildMenu(FMenuBuilder& Builder)
	{
		FUIAction ActionBakeIndirect(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::BakeIndirect),
			FCanExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::IsReadyToBakeIndirect));

		Builder.BeginSection("LevelEditorIR", NSLOCTEXT("SteamAudio", "Steam Audio", "Steam Audio"));

		Builder.AddMenuEntry(NSLOCTEXT("SteamAudio", "Bake Indirect Sound...", "Bake Indirect Sound..."),
			NSLOCTEXT("SteamAudio", "Opens indirect baking manager.", "Opens indirect baking manager."), FSlateIcon(), ActionBakeIndirect,
			NAME_None, EUserInterfaceActionType::Button);

		Builder.EndSection();
	}

	bool FSteamAudioEditorModule::IsReadyToBakeIndirect() const
	{
		return true;
	}
}