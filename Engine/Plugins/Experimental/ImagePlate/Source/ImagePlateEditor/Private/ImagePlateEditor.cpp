// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ISequencerModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneImagePlateSection.h"
#include "PropertyEditorModule.h"
#include "SlateStyle.h"
#include "SlateStyleRegistry.h"
#include "ImagePlateTrackEditor.h"
#include "AssetTypeActions_ImagePlateFileSequence.h"
#include "IAssetTools.h"
#include "ISettingsModule.h"
#include "ImagePlateFileSequence.h"

#define LOCTEXT_NAMESPACE "ImagePlateEditor"

class FImagePlateEditorStyle : public FSlateStyleSet
{
public:
	FImagePlateEditorStyle() : FSlateStyleSet("ImagePlateEditorStyle")
	{
		const FVector2D Icon16x16(16.f, 16.f);
		const FVector2D Icon64x64(64.f, 64.f);

		FString PluginBasePath = FPaths::GetPath(FModuleManager::Get().GetModuleFilename("ImagePlate"));
		SetContentRoot(PluginBasePath / TEXT("../../Resources/Icons"));

		Set("ClassIcon.ImagePlate", new FSlateImageBrush(RootToContentDir(TEXT("ImagePlate_16x.png")), Icon16x16));
		Set("ClassThumbnail.ImagePlate", new FSlateImageBrush(RootToContentDir(TEXT("ImagePlate_64x.png")), Icon64x64));

		Set("ClassIcon.ImagePlateComponent", new FSlateImageBrush(RootToContentDir(TEXT("ImagePlate_16x.png")), Icon16x16));
		Set("ClassThumbnail.ImagePlateComponent", new FSlateImageBrush(RootToContentDir(TEXT("ImagePlate_64x.png")), Icon64x64));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	~FImagePlateEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

public:

	static FImagePlateEditorStyle& Get()
	{
		if (!Singleton.IsSet())
		{
			Singleton.Emplace();
		}
		return Singleton.GetValue();
	}
	
	static void Destroy()
	{
		Singleton.Reset();
	}

private:
	static TOptional<FImagePlateEditorStyle> Singleton;
};

TOptional<FImagePlateEditorStyle> FImagePlateEditorStyle::Singleton;

class FImagePlateEditorModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		FImagePlateEditorStyle::Get();

		ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
		TrackEditorBindingHandle = SequencerModule.RegisterPropertyTrackEditor<FImagePlateTrackEditor>();

		AssetTypeActions = MakeShared<FAssetTypeActions_ImagePlateFileSequence>();

		IAssetTools& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetToolsModule.RegisterAssetTypeActions(AssetTypeActions.ToSharedRef());

		ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
		SettingsModule.RegisterSettings("Project", "Plugins", "ImagePlate",
			LOCTEXT("ImagePlateEditorSettingsName", "Image Plate"),
			LOCTEXT("ImagePlateEditorSettingsDescription", "Configure settings for the Image Plate plugin."),
			GetMutableDefault<UImagePlateSettings>()
		);
	}
	
	virtual void ShutdownModule() override
	{
		FImagePlateEditorStyle::Destroy();

		ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");
		if (SequencerModulePtr)
		{
			SequencerModulePtr->UnRegisterTrackEditor(TrackEditorBindingHandle);
		}

		FAssetToolsModule* AssetToolsModule = FModuleManager::Get().GetModulePtr<FAssetToolsModule>("AssetTools");
		if (AssetToolsModule)
		{
			AssetToolsModule->Get().UnregisterAssetTypeActions(AssetTypeActions.ToSharedRef());
		}
		
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "ImagePlate");
		}

	}

	TSharedPtr<FAssetTypeActions_ImagePlateFileSequence> AssetTypeActions;
	FDelegateHandle TrackEditorBindingHandle;
};

IMPLEMENT_MODULE(FImagePlateEditorModule, ImagePlateEditor);

#undef LOCTEXT_NAMESPACE
