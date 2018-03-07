// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "EditorStyleSet.h"
#include "EditorModeRegistry.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LandscapeFileFormatInterface.h"
#include "LandscapeProxy.h"
#include "LandscapeEdMode.h"
#include "Landscape.h"
#include "LandscapeEditorCommands.h"
#include "Classes/ActorFactoryLandscape.h"
#include "LandscapeFileFormatPng.h"
#include "LandscapeFileFormatRaw.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "PropertyEditorModule.h"
#include "LandscapeEditorDetails.h"
#include "LandscapeEditorDetailCustomization_NewLandscape.h"
#include "LandscapeEditorDetailCustomization_CopyPaste.h"
#include "LandscapeSplineDetails.h"

#include "LevelEditor.h"

#include "LandscapeRender.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor"

struct FRegisteredLandscapeHeightmapFileFormat
{
	TSharedRef<ILandscapeHeightmapFileFormat> FileFormat;
	FLandscapeFileTypeInfo FileTypeInfo;
	FString ConcatenatedFileExtensions;

	FRegisteredLandscapeHeightmapFileFormat(TSharedRef<ILandscapeHeightmapFileFormat> InFileFormat);
};

struct FRegisteredLandscapeWeightmapFileFormat
{
	TSharedRef<ILandscapeWeightmapFileFormat> FileFormat;
	FLandscapeFileTypeInfo FileTypeInfo;
	FString ConcatenatedFileExtensions;

	FRegisteredLandscapeWeightmapFileFormat(TSharedRef<ILandscapeWeightmapFileFormat> InFileFormat);
};

class FLandscapeEditorModule : public ILandscapeEditorModule
{
public:

	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override
	{
		FLandscapeEditorCommands::Register();

		// register the editor mode
		FEditorModeRegistry::Get().RegisterMode<FEdModeLandscape>(
			FBuiltinEditorModes::EM_Landscape,
			NSLOCTEXT("EditorModes", "LandscapeMode", "Landscape"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.LandscapeMode", "LevelEditor.LandscapeMode.Small"),
			true,
			300
			);

		// register customizations
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("LandscapeEditorObject", FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeEditorDetails::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout("GizmoImportLayer", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLandscapeEditorStructCustomization_FGizmoImportLayer::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout("LandscapeImportLayer", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::MakeInstance));

		PropertyModule.RegisterCustomClassLayout("LandscapeSplineControlPoint", FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeSplineDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("LandscapeSplineSegment", FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeSplineDetails::MakeInstance));

		// add menu extension
		GlobalUICommandList = MakeShareable(new FUICommandList);
		const FLandscapeEditorCommands& LandscapeActions = FLandscapeEditorCommands::Get();
		GlobalUICommandList->MapAction(LandscapeActions.ViewModeNormal, FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::Normal), FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::Normal));
		GlobalUICommandList->MapAction(LandscapeActions.ViewModeLOD, FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::LOD), FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::LOD));
		GlobalUICommandList->MapAction(LandscapeActions.ViewModeLayerDensity, FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::LayerDensity), FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::LayerDensity));
		GlobalUICommandList->MapAction(LandscapeActions.ViewModeLayerDebug, FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::DebugLayer), FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::DebugLayer));
		GlobalUICommandList->MapAction(LandscapeActions.ViewModeWireframeOnTop, FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::WireframeOnTop), FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::WireframeOnTop));
		GlobalUICommandList->MapAction(LandscapeActions.ViewModeLayerUsage, FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::LayerUsage), FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::LayerUsage));

		ViewportMenuExtender = MakeShareable(new FExtender);
		ViewportMenuExtender->AddMenuExtension("LevelViewportLandscape", EExtensionHook::First, GlobalUICommandList, FMenuExtensionDelegate::CreateStatic(&ConstructLandscapeViewportMenu));
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(ViewportMenuExtender);

		// add actor factories
		UActorFactoryLandscape* LandscapeActorFactory = NewObject<UActorFactoryLandscape>();
		LandscapeActorFactory->NewActorClass = ALandscape::StaticClass();
		GEditor->ActorFactories.Add(LandscapeActorFactory);

		UActorFactoryLandscape* LandscapeProxyActorFactory = NewObject<UActorFactoryLandscape>();
		LandscapeProxyActorFactory->NewActorClass = ALandscapeProxy::StaticClass();
		GEditor->ActorFactories.Add(LandscapeProxyActorFactory);

		// Built-in File Formats
		RegisterHeightmapFileFormat(MakeShareable(new FLandscapeHeightmapFileFormat_Png()));
		RegisterWeightmapFileFormat(MakeShareable(new FLandscapeWeightmapFileFormat_Png()));
		RegisterHeightmapFileFormat(MakeShareable(new FLandscapeHeightmapFileFormat_Raw()));
		RegisterWeightmapFileFormat(MakeShareable(new FLandscapeWeightmapFileFormat_Raw()));
	}

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() override
	{
		FLandscapeEditorCommands::Unregister();

		// unregister the editor mode
		FEditorModeRegistry::Get().UnregisterMode(FBuiltinEditorModes::EM_Landscape);

		// unregister customizations
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("LandscapeEditorObject");
		PropertyModule.UnregisterCustomPropertyTypeLayout("GizmoImportLayer");
		PropertyModule.UnregisterCustomPropertyTypeLayout("LandscapeImportLayer");

		PropertyModule.UnregisterCustomClassLayout("LandscapeSplineControlPoint");
		PropertyModule.UnregisterCustomClassLayout("LandscapeSplineSegment");

		// remove menu extension
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(ViewportMenuExtender);
		ViewportMenuExtender = nullptr;
		GlobalUICommandList = nullptr;

		// remove actor factories
		// TODO - this crashes on shutdown
		// GEditor->ActorFactories.RemoveAll([](const UActorFactory* ActorFactory) { return ActorFactory->IsA<UActorFactoryLandscape>(); });
	}

	static void ConstructLandscapeViewportMenu(FMenuBuilder& MenuBuilder)
	{
		struct Local
		{
			static void BuildLandscapeVisualizersMenu(FMenuBuilder& InMenuBuilder)
			{
				const FLandscapeEditorCommands& LandscapeActions = FLandscapeEditorCommands::Get();

				InMenuBuilder.BeginSection("LandscapeVisualizers", LOCTEXT("LandscapeHeader", "Landscape Visualizers"));
				{
					InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeNormal, NAME_None, LOCTEXT("LandscapeViewModeNormal", "Normal"));
					InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeLOD, NAME_None, LOCTEXT("LandscapeViewModeLOD", "LOD"));
					InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeLayerDensity, NAME_None, LOCTEXT("LandscapeViewModeLayerDensity", "Layer Density"));
					if (GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Landscape))
					{
						InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeLayerUsage, NAME_None, LOCTEXT("LandscapeViewModeLayerUsage", "Layer Usage"));
						InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeLayerDebug, NAME_None, LOCTEXT("LandscapeViewModeLayerDebug", "Layer Debug"));
					}
					InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeWireframeOnTop, NAME_None, LOCTEXT("LandscapeViewModeWireframeOnTop", "Wireframe on Top"));
				}
				InMenuBuilder.EndSection();
			}
		};
		MenuBuilder.AddSubMenu(LOCTEXT("LandscapeSubMenu", "Visualizers"), LOCTEXT("LandscapeSubMenu_ToolTip", "Select a Landscape visualiser"), FNewMenuDelegate::CreateStatic(&Local::BuildLandscapeVisualizersMenu));
	}

	static void ChangeLandscapeViewMode(ELandscapeViewMode::Type ViewMode)
	{
		GLandscapeViewMode = ViewMode;
	}

	static bool IsLandscapeViewModeSelected(ELandscapeViewMode::Type ViewMode)
	{
		return GLandscapeViewMode == ViewMode;
	}

	virtual void RegisterHeightmapFileFormat(TSharedRef<ILandscapeHeightmapFileFormat> FileFormat) override
	{
		HeightmapFormats.Emplace(FileFormat);
		HeightmapImportDialogTypeString.Reset();
		HeightmapExportDialogTypeString.Reset();
	}

	virtual void RegisterWeightmapFileFormat(TSharedRef<ILandscapeWeightmapFileFormat> FileFormat) override
	{
		WeightmapFormats.Emplace(FileFormat);
		WeightmapImportDialogTypeString.Reset();
		WeightmapExportDialogTypeString.Reset();
	}

	virtual void UnregisterHeightmapFileFormat(TSharedRef<ILandscapeHeightmapFileFormat> FileFormat) override
	{
		int32 Index = HeightmapFormats.IndexOfByPredicate(
			[FileFormat](const FRegisteredLandscapeHeightmapFileFormat& RegisteredFileFormat)
			{
				return RegisteredFileFormat.FileFormat == FileFormat;
			});
		if (Index != INDEX_NONE)
		{
			HeightmapFormats.RemoveAt(Index);
			HeightmapImportDialogTypeString.Reset();
			HeightmapExportDialogTypeString.Reset();
		}
	}

	virtual void UnregisterWeightmapFileFormat(TSharedRef<ILandscapeWeightmapFileFormat> FileFormat) override
	{
		int32 Index = WeightmapFormats.IndexOfByPredicate(
			[FileFormat](const FRegisteredLandscapeWeightmapFileFormat& RegisteredFileFormat)
			{
				return RegisteredFileFormat.FileFormat == FileFormat;
			});
		if (Index != INDEX_NONE)
		{
			WeightmapFormats.RemoveAt(Index);
			WeightmapImportDialogTypeString.Reset();
			WeightmapExportDialogTypeString.Reset();
		}
	}

	virtual const TCHAR* GetHeightmapImportDialogTypeString() const override;
	virtual const TCHAR* GetWeightmapImportDialogTypeString() const override;

	virtual const TCHAR* GetHeightmapExportDialogTypeString() const override;
	virtual const TCHAR* GetWeightmapExportDialogTypeString() const override;

	virtual const ILandscapeHeightmapFileFormat* GetHeightmapFormatByExtension(const TCHAR* Extension) const override;
	virtual const ILandscapeWeightmapFileFormat* GetWeightmapFormatByExtension(const TCHAR* Extension) const override;

	virtual TSharedPtr<FUICommandList> GetLandscapeLevelViewportCommandList() const override;

protected:
	TSharedPtr<FExtender> ViewportMenuExtender;
	TSharedPtr<FUICommandList> GlobalUICommandList;
	TArray<FRegisteredLandscapeHeightmapFileFormat> HeightmapFormats;
	TArray<FRegisteredLandscapeWeightmapFileFormat> WeightmapFormats;
	mutable FString HeightmapImportDialogTypeString;
	mutable FString WeightmapImportDialogTypeString;
	mutable FString HeightmapExportDialogTypeString;
	mutable FString WeightmapExportDialogTypeString;
};

IMPLEMENT_MODULE(FLandscapeEditorModule, LandscapeEditor);

FRegisteredLandscapeHeightmapFileFormat::FRegisteredLandscapeHeightmapFileFormat(TSharedRef<ILandscapeHeightmapFileFormat> InFileFormat)
	: FileFormat(MoveTemp(InFileFormat))
	, FileTypeInfo(FileFormat->GetInfo())
{
	bool bJoin = false;
	for (const FString& Extension : FileTypeInfo.Extensions)
	{
		if (bJoin)
		{
			ConcatenatedFileExtensions += TEXT(';');
		}
		ConcatenatedFileExtensions += TEXT('*');
		ConcatenatedFileExtensions += Extension;
		bJoin = true;
	}
}

FRegisteredLandscapeWeightmapFileFormat::FRegisteredLandscapeWeightmapFileFormat(TSharedRef<ILandscapeWeightmapFileFormat> InFileFormat)
	: FileFormat(MoveTemp(InFileFormat))
	, FileTypeInfo(FileFormat->GetInfo())
{
	bool bJoin = false;
	for (const FString& Extension : FileTypeInfo.Extensions)
	{
		if (bJoin)
		{
			ConcatenatedFileExtensions += TEXT(';');
		}
		ConcatenatedFileExtensions += TEXT('*');
		ConcatenatedFileExtensions += Extension;
		bJoin = true;
	}
}

const TCHAR* FLandscapeEditorModule::GetHeightmapImportDialogTypeString() const
{
	if (HeightmapImportDialogTypeString.IsEmpty())
	{
		HeightmapImportDialogTypeString = TEXT("All Heightmap files|");
		bool bJoin = false;
		for (const FRegisteredLandscapeHeightmapFileFormat& HeightmapFormat : HeightmapFormats)
		{
			if (bJoin)
			{
				HeightmapImportDialogTypeString += TEXT(';');
			}
			HeightmapImportDialogTypeString += HeightmapFormat.ConcatenatedFileExtensions;
			bJoin = true;
		}
		HeightmapImportDialogTypeString += TEXT('|');
		for (const FRegisteredLandscapeHeightmapFileFormat& HeightmapFormat : HeightmapFormats)
		{
			HeightmapImportDialogTypeString += HeightmapFormat.FileTypeInfo.Description.ToString();
			HeightmapImportDialogTypeString += TEXT('|');
			HeightmapImportDialogTypeString += HeightmapFormat.ConcatenatedFileExtensions;
			HeightmapImportDialogTypeString += TEXT('|');
		}
		HeightmapImportDialogTypeString += TEXT("All Files (*.*)|*.*");
	}

	return *HeightmapImportDialogTypeString;
}

const TCHAR* FLandscapeEditorModule::GetWeightmapImportDialogTypeString() const
{
	if (WeightmapImportDialogTypeString.IsEmpty())
	{
		WeightmapImportDialogTypeString = TEXT("All Layer files|");
		bool bJoin = false;
		for (const FRegisteredLandscapeWeightmapFileFormat& WeightmapFormat : WeightmapFormats)
		{
			if (bJoin)
			{
				WeightmapImportDialogTypeString += TEXT(';');
			}
			WeightmapImportDialogTypeString += WeightmapFormat.ConcatenatedFileExtensions;
			bJoin = true;
		}
		WeightmapImportDialogTypeString += TEXT('|');
		for (const FRegisteredLandscapeWeightmapFileFormat& WeightmapFormat : WeightmapFormats)
		{
			WeightmapImportDialogTypeString += WeightmapFormat.FileTypeInfo.Description.ToString();
			WeightmapImportDialogTypeString += TEXT('|');
			WeightmapImportDialogTypeString += WeightmapFormat.ConcatenatedFileExtensions;
			WeightmapImportDialogTypeString += TEXT('|');
		}
		WeightmapImportDialogTypeString += TEXT("All Files (*.*)|*.*");
	}

	return *WeightmapImportDialogTypeString;
}

const TCHAR* FLandscapeEditorModule::GetHeightmapExportDialogTypeString() const
{
	if (HeightmapExportDialogTypeString.IsEmpty())
	{
		HeightmapExportDialogTypeString = TEXT("");

		for (const FRegisteredLandscapeHeightmapFileFormat& HeightmapFormat : HeightmapFormats)
		{
			if (!HeightmapFormat.FileTypeInfo.bSupportsExport)
			{
				continue;
			}
			HeightmapExportDialogTypeString += HeightmapFormat.FileTypeInfo.Description.ToString();
			HeightmapExportDialogTypeString += TEXT('|');
			HeightmapExportDialogTypeString += HeightmapFormat.ConcatenatedFileExtensions;
			HeightmapExportDialogTypeString += TEXT('|');
		}
		HeightmapExportDialogTypeString += TEXT("All Files (*.*)|*.*");
	}

	return *HeightmapExportDialogTypeString;
}

const TCHAR* FLandscapeEditorModule::GetWeightmapExportDialogTypeString() const
{
	if (WeightmapExportDialogTypeString.IsEmpty())
	{
		WeightmapExportDialogTypeString = TEXT("");
		for (const FRegisteredLandscapeWeightmapFileFormat& WeightmapFormat : WeightmapFormats)
		{
			if (!WeightmapFormat.FileTypeInfo.bSupportsExport)
			{
				continue;
			}
			WeightmapExportDialogTypeString += WeightmapFormat.FileTypeInfo.Description.ToString();
			WeightmapExportDialogTypeString += TEXT('|');
			WeightmapExportDialogTypeString += WeightmapFormat.ConcatenatedFileExtensions;
			WeightmapExportDialogTypeString += TEXT('|');
		}
		WeightmapExportDialogTypeString += TEXT("All Files (*.*)|*.*");
	}

	return *WeightmapExportDialogTypeString;
}

const ILandscapeHeightmapFileFormat* FLandscapeEditorModule::GetHeightmapFormatByExtension(const TCHAR* Extension) const
{
	auto* FoundFormat = HeightmapFormats.FindByPredicate(
		[Extension](const FRegisteredLandscapeHeightmapFileFormat& HeightmapFormat)
		{
			return HeightmapFormat.FileTypeInfo.Extensions.Contains(Extension);
		});

	return FoundFormat ? &FoundFormat->FileFormat.Get() : nullptr;
}

const ILandscapeWeightmapFileFormat* FLandscapeEditorModule::GetWeightmapFormatByExtension(const TCHAR* Extension) const
{
	auto* FoundFormat = WeightmapFormats.FindByPredicate(
		[Extension](const FRegisteredLandscapeWeightmapFileFormat& WeightmapFormat)
	{
		return WeightmapFormat.FileTypeInfo.Extensions.Contains(Extension);
	});

	return FoundFormat ? &FoundFormat->FileFormat.Get() : nullptr;
}

TSharedPtr<FUICommandList> FLandscapeEditorModule::GetLandscapeLevelViewportCommandList() const
{
	return GlobalUICommandList;
}

#undef LOCTEXT_NAMESPACE
