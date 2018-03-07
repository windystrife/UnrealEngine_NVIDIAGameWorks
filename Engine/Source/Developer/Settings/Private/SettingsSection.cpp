// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SettingsSection.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/Class.h"


/* FSettingsSection structors
 *****************************************************************************/

FSettingsSection::FSettingsSection( const ISettingsCategoryRef& InCategory, const FName& InName, const FText& InDisplayName, const FText& InDescription, const TWeakObjectPtr<UObject>& InSettingsObject )
	: Category(InCategory)
	, Description(InDescription)
	, DisplayName(InDisplayName)
	, Name(InName)
	, SettingsObject(InSettingsObject)
{ }


FSettingsSection::FSettingsSection( const ISettingsCategoryRef& InCategory, const FName& InName, const FText& InDisplayName, const FText& InDescription, const TSharedRef<SWidget>& InCustomWidget )
	: Category(InCategory)
	, CustomWidget(InCustomWidget)
	, Description(InDescription)
	, DisplayName(InDisplayName)
	, Name(InName)
{ }


/* ISettingsSection interface
 *****************************************************************************/

bool FSettingsSection::CanEdit() const
{
	if (CanEditDelegate.IsBound())
	{
		return CanEditDelegate.Execute();
	}

	return true;
}


bool FSettingsSection::CanExport() const
{
	return (ExportDelegate.IsBound() || (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config)));
}


bool FSettingsSection::CanImport() const
{
	return (ImportDelegate.IsBound() || (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config)));
}


bool FSettingsSection::CanResetDefaults() const
{
	return (ResetDefaultsDelegate.IsBound() || (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config) && !SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig | CLASS_GlobalUserConfig)));
}


bool FSettingsSection::CanSave() const
{
	return (SaveDelegate.IsBound() || (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config)));
}


bool FSettingsSection::CanSaveDefaults() const
{
	return (SaveDefaultsDelegate.IsBound() || (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config) && !SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig | CLASS_GlobalUserConfig)));
}


bool FSettingsSection::Export( const FString& Filename )
{
	if (ExportDelegate.IsBound())
	{
		return ExportDelegate.Execute(Filename);
	}

	if (SettingsObject.IsValid())
	{
		SettingsObject->SaveConfig(CPF_Config, *Filename);

		return true;
	}

	return false;
}


TWeakPtr<ISettingsCategory> FSettingsSection::GetCategory()
{
	return Category;
}


TWeakPtr<SWidget> FSettingsSection::GetCustomWidget() const
{
	return CustomWidget;
}


const FText& FSettingsSection::GetDescription() const
{
	return Description;
}


const FText& FSettingsSection::GetDisplayName() const
{
	return DisplayName;
}


const FName& FSettingsSection::GetName() const
{
	return Name;
}


TWeakObjectPtr<UObject> FSettingsSection::GetSettingsObject() const
{
	return SettingsObject;
}


FText FSettingsSection::GetStatus() const
{
	if (StatusDelegate.IsBound())
	{
		return StatusDelegate.Execute();
	}

	return FText::GetEmpty();
}


bool FSettingsSection::HasDefaultSettingsObject()
{
	if (!SettingsObject.IsValid())
	{
		return false;
	}

	// @todo userconfig: Should we add GlobalUserConfig here?
	return SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig);
}


bool FSettingsSection::Import( const FString& Filename )
{
	if (ImportDelegate.IsBound())
	{
		return ImportDelegate.Execute(Filename);
	}

	if (SettingsObject.IsValid())
	{
		SettingsObject->LoadConfig(SettingsObject->GetClass(), *Filename, UE4::LCPF_PropagateToInstances);

		return true;
	}

	return false;	
}


bool FSettingsSection::ResetDefaults()
{
	if (ResetDefaultsDelegate.IsBound())
	{
		return ResetDefaultsDelegate.Execute();
	}

	if (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config) && !SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig | CLASS_GlobalUserConfig))
	{
		FString ConfigName = SettingsObject->GetClass()->GetConfigName();

		GConfig->EmptySection(*SettingsObject->GetClass()->GetPathName(), ConfigName);
		GConfig->Flush(false);

		FConfigCacheIni::LoadGlobalIniFile(ConfigName, *FPaths::GetBaseFilename(ConfigName), nullptr, true);

		SettingsObject->ReloadConfig(nullptr, nullptr, UE4::LCPF_PropagateToInstances|UE4::LCPF_PropagateToChildDefaultObjects);

		return true;
	}

	return false;
}


bool FSettingsSection::Save()
{
	if (ModifiedDelegate.IsBound() && !ModifiedDelegate.Execute())
	{
		return false;
	}

	if (SaveDelegate.IsBound())
	{
		return SaveDelegate.Execute();
	}

	if (SettingsObject.IsValid())
	{
		if (SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig))
		{
			SettingsObject->UpdateDefaultConfigFile();
		}
		else if (SettingsObject->GetClass()->HasAnyClassFlags(CLASS_GlobalUserConfig))
		{
			SettingsObject->UpdateGlobalUserConfigFile();
		}
		else
		{
			SettingsObject->SaveConfig();
		}

		return true;
	}

	return false;
}


bool FSettingsSection::SaveDefaults()
{
	if (SaveDefaultsDelegate.IsBound())
	{
		return SaveDefaultsDelegate.Execute();
	}

	if (SettingsObject.IsValid())
	{
		SettingsObject->UpdateDefaultConfigFile();
		SettingsObject->ReloadConfig(nullptr, nullptr, UE4::LCPF_PropagateToInstances);

		return true;			
	}

	return false;
}
