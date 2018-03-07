// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IInputBindingEditorModule.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Misc/Paths.h"
#include "Widgets/SInputBindingEditorPanel.h"
#include "UnrealEdMisc.h"
#include "Logging/MessageLog.h"
#include "Dialogs/Dialogs.h"
#include "IDetailCustomization.h"
#include "ISettingsModule.h"
#include "PropertyEditorModule.h"
#include "EditorKeyboardShortcutSettings.h"
#include "ISettingsSection.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "InputBindingEditor"

static FName SettingsModuleName("Settings");
static FName PropertyEditorModuleName("PropertyEditor");

class FEditorKeyboardShortcutSettings : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FEditorKeyboardShortcutSettings );
	}

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
	{
		EditorPanel = MakeShareable( new FInputBindingEditorPanel );

		EditorPanel->Initialize(DetailBuilder);
	}
private:
	TSharedPtr<FInputBindingEditorPanel> EditorPanel;
};

class FInputBindingEditorModule
	: public IInputBindingEditorModule
{
public:

	// IInputBindingEditorModule interface
	virtual void StartupModule() override
	{
		ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>(SettingsModuleName);

		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(PropertyEditorModuleName);

		EditorKeyboardShortcutSettingsName = UEditorKeyboardShortcutSettings::StaticClass()->GetFName();
		PropertyEditor.RegisterCustomClassLayout(EditorKeyboardShortcutSettingsName, FOnGetDetailCustomizationInstance::CreateStatic(&FEditorKeyboardShortcutSettings::MakeInstance));

		// input bindings
		ISettingsSectionPtr InputBindingSettingsSection = SettingsModule.RegisterSettings("Editor", "General", "InputBindings",
			LOCTEXT("InputBindingsSettingsName", "Keyboard Shortcuts"),
			LOCTEXT("InputBindingsSettingsDescription", "Configure keyboard shortcuts to quickly invoke operations."),
			GetMutableDefault<UEditorKeyboardShortcutSettings>()
		);

		if(InputBindingSettingsSection.IsValid())
		{
			InputBindingSettingsSection->OnExport().BindRaw(this, &FInputBindingEditorModule::HandleInputBindingsExport);
			InputBindingSettingsSection->OnImport().BindRaw(this, &FInputBindingEditorModule::HandleInputBindingsImport);
			InputBindingSettingsSection->OnResetDefaults().BindRaw(this, &FInputBindingEditorModule::HandleInputBindingsResetToDefault);
			InputBindingSettingsSection->OnSave().BindRaw(this, &FInputBindingEditorModule::HandleInputBindingsSave);
		}
	}

	virtual void ShutdownModule() override
	{
		if(FModuleManager::Get().IsModuleLoaded(PropertyEditorModuleName))
		{
			FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditorModuleName);

			PropertyEditor.UnregisterCustomClassLayout(EditorKeyboardShortcutSettingsName);
		}

	}

private:
	// Show a warning that the editor will require a restart and return its result
	EAppReturnType::Type ShowRestartWarning(const FText& Title) const
	{
		return OpenMsgDlgInt(EAppMsgType::OkCancel, LOCTEXT("ActionRestartMsg", "Imported settings won't be applied until the editor is restarted. Do you wish to restart now (you will be prompted to save any changes)?"), Title);
	}

	// Backup a file
	bool BackupFile(const FString& SrcFilename, const FString& DstFilename)
	{
		if(IFileManager::Get().Copy(*DstFilename, *SrcFilename) == COPY_OK)
		{
			return true;
		}

		// log error	
		FMessageLog EditorErrors("EditorErrors");
		if(!FPaths::FileExists(SrcFilename))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("FileName"), FText::FromString(SrcFilename));
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_NoExist_Notification", "Unsuccessful backup! {FileName} does not exist!"), Arguments));
		}
		else if(IFileManager::Get().IsReadOnly(*DstFilename))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("FileName"), FText::FromString(DstFilename));
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_ReadOnly_Notification", "Unsuccessful backup! {FileName} is read-only!"), Arguments));
		}
		else
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("SourceFileName"), FText::FromString(SrcFilename));
			Arguments.Add(TEXT("BackupFileName"), FText::FromString(DstFilename));
			// We don't specifically know why it failed, this is a fallback.
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_Fallback_Notification", "Unsuccessful backup of {SourceFileName} to {BackupFileName}"), Arguments));
		}
		EditorErrors.Notify(LOCTEXT("BackupUnsuccessful_Title", "Backup Unsuccessful!"));

		return false;
	}


	// Handles exporting input bindings to a file
	bool HandleInputBindingsExport(const FString& Filename)
	{
		FInputBindingManager::Get().SaveInputBindings();
		GConfig->Flush(false, GEditorKeyBindingsIni);
		return BackupFile(GEditorKeyBindingsIni, Filename);
	}

	// Handles importing input bindings from a file
	bool HandleInputBindingsImport(const FString& Filename)
	{
		if(EAppReturnType::Ok == ShowRestartWarning(LOCTEXT("ImportKeyBindings_Title", "Import Key Bindings")))
		{
			FUnrealEdMisc::Get().SetConfigRestoreFilename(Filename, GEditorKeyBindingsIni);
			FUnrealEdMisc::Get().RestartEditor(false);

			return true;
		}

		return false;
	}

	// Handles resetting input bindings back to the defaults
	bool HandleInputBindingsResetToDefault()
	{
		if(EAppReturnType::Ok == ShowRestartWarning(LOCTEXT("ResetKeyBindings_Title", "Reset Key Bindings")))
		{
			FInputBindingManager::Get().RemoveUserDefinedChords();
			GConfig->Flush(false, GEditorKeyBindingsIni);
			FUnrealEdMisc::Get().RestartEditor(false);

			return true;
		}

		return false;
	}

	// Handles saving default input bindings.
	bool HandleInputBindingsSave()
	{
		FInputBindingManager::Get().RemoveUserDefinedChords();
		GConfig->Flush(false, GEditorKeyBindingsIni);
		return true;
	}
private:

	/** Holds the collection of created binding editor panels. */
	TArray<TSharedPtr<SWidget> > BindingEditorPanels;

	/** Captured name of the UEditorKeyboardShortcutSettings class */
	FName EditorKeyboardShortcutSettingsName;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FInputBindingEditorModule, InputBindingEditor);
