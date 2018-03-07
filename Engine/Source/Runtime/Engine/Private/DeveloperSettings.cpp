// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/DeveloperSettings.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/PropertyPortFlags.h"

UDeveloperSettings::UDeveloperSettings(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
{
	CategoryName = NAME_None;
	SectionName = NAME_None;
}

FName UDeveloperSettings::GetContainerName() const
{
	static const FName ProjectName(TEXT("Project"));
	static const FName EditorName(TEXT("Editor"));

	static const FName EditorSettingsName(TEXT("EditorSettings"));
	static const FName EditorPerProjectUserSettingsName(TEXT("EditorPerProjectUserSettings"));

	FName ConfigName = GetClass()->ClassConfigName;

	if ( ConfigName == EditorSettingsName || ConfigName == EditorPerProjectUserSettingsName )
	{
		return EditorName;
	}
	
	return ProjectName;
}

FName UDeveloperSettings::GetCategoryName() const
{
	static const FName GeneralName(TEXT("General"));
	static const FName EditorSettingsName(TEXT("EditorSettings"));
	static const FName EditorPerProjectUserSettingsName(TEXT("EditorPerProjectUserSettings"));

	if ( CategoryName != NAME_None )
	{
		return CategoryName;
	}

	FName ConfigName = GetClass()->ClassConfigName;
	if ( ConfigName == NAME_Engine || ConfigName == NAME_Input )
	{
		return NAME_Engine;
	}
	else if ( ConfigName == EditorSettingsName || ConfigName == EditorPerProjectUserSettingsName )
	{
		return GeneralName;
	}
	else if ( ConfigName == NAME_Editor || ConfigName == NAME_EditorSettings || ConfigName == NAME_EditorLayout || ConfigName == NAME_EditorKeyBindings )
	{
		return NAME_Editor;
	}
	else if ( ConfigName == NAME_Game )
	{
		return NAME_Game;
	}

	return NAME_Engine;
}

FName UDeveloperSettings::GetSectionName() const
{
	if ( SectionName != NAME_None )
	{
		return SectionName;
	}

	return GetClass()->GetFName();
}

#if WITH_EDITOR
FText UDeveloperSettings::GetSectionText() const
{
	return GetClass()->GetDisplayNameText();
}

FText UDeveloperSettings::GetSectionDescription() const
{
	return GetClass()->GetToolTipText();
}
#endif

TSharedPtr<SWidget> UDeveloperSettings::GetCustomSettingsWidget() const
{
	return TSharedPtr<SWidget>();
}

#if WITH_EDITOR

static FName DeveloperSettingsConsoleVariableMetaFName(TEXT("ConsoleVariable"));

void UDeveloperSettings::ImportConsoleVariableValues()
{
	for (UProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Config))
		{
			continue;
		}

		const FString& CVarName = Property->GetMetaData(DeveloperSettingsConsoleVariableMetaFName);
		if (!CVarName.IsEmpty())
		{
			IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
			if (CVar)
			{
				if (Property->ImportText(*CVar->GetString(), Property->ContainerPtrToValuePtr<uint8>(this, 0), PPF_ConsoleVariable, this) == NULL)
				{
					UE_LOG(LogTemp, Error, TEXT("%s import failed for %s on console variable %s (=%s)"), *GetClass()->GetName(), *Property->GetName(), *CVarName, *CVar->GetString());
				}
			}
			else
			{
				UE_LOG(LogTemp, Fatal, TEXT("%s failed to find console variable %s for %s"), *GetClass()->GetName(), *CVarName, *Property->GetName());
			}
		}
	}
}

void UDeveloperSettings::ExportValuesToConsoleVariables(UProperty* PropertyThatChanged)
{
	if(PropertyThatChanged)
	{
		const FString& CVarName = PropertyThatChanged->GetMetaData(DeveloperSettingsConsoleVariableMetaFName);
		if (!CVarName.IsEmpty())
		{
			IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
			if (CVar && (CVar->GetFlags() & ECVF_ReadOnly) == 0)
			{
				UByteProperty* ByteProperty = Cast<UByteProperty>(PropertyThatChanged);
				if (ByteProperty != NULL && ByteProperty->Enum != NULL)
				{
					CVar->Set(ByteProperty->GetPropertyValue_InContainer(this), ECVF_SetByProjectSetting);
				}
				else if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(PropertyThatChanged))
				{
					UNumericProperty* UnderlyingProp = EnumProperty->GetUnderlyingProperty();
					void* PropertyAddress = EnumProperty->ContainerPtrToValuePtr<void>(this);
					CVar->Set((int32)UnderlyingProp->GetSignedIntPropertyValue(PropertyAddress), ECVF_SetByProjectSetting);
				}
				else if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(PropertyThatChanged))
				{
					CVar->Set((int32)BoolProperty->GetPropertyValue_InContainer(this), ECVF_SetByProjectSetting);
				}
				else if (UIntProperty* IntProperty = Cast<UIntProperty>(PropertyThatChanged))
				{
					CVar->Set(IntProperty->GetPropertyValue_InContainer(this), ECVF_SetByProjectSetting);
				}
				else if (UFloatProperty* FloatProperty = Cast<UFloatProperty>(PropertyThatChanged))
				{
					CVar->Set(FloatProperty->GetPropertyValue_InContainer(this), ECVF_SetByProjectSetting);
				}
				else if (UStrProperty* StringProperty = Cast<UStrProperty>(PropertyThatChanged))
				{
					CVar->Set(*StringProperty->GetPropertyValue_InContainer(this), ECVF_SetByProjectSetting);
				}
				else if (UNameProperty* NameProperty = Cast<UNameProperty>(PropertyThatChanged))
				{
					CVar->Set(*NameProperty->GetPropertyValue_InContainer(this).ToString(), ECVF_SetByProjectSetting);
				}

			}
			else
			{
				UE_LOG(LogInit, Warning, TEXT("CVar named '%s' marked up in %s was not found or is set to read-only"), *CVarName, *GetClass()->GetName());
			}
		}
	}
}

#endif
