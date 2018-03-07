// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ConfigEditorPropertyDetails.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "IPropertyTable.h"
#include "IPropertyTableColumn.h"

#include "IConfigEditorModule.h"
#include "PropertyVisualization/ConfigPropertyColumn.h"
#include "ConfigPropertyHelper.h"


#define LOCTEXT_NAMESPACE "ConfigPropertyHelperDetails"

////////////////////////////////////////////////
// FConfigPropertyHelperDetails

void FConfigPropertyHelperDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedPtr<IPropertyHandle> PropertyHandle = DetailBuilder.GetProperty("EditProperty");
	DetailBuilder.HideProperty(PropertyHandle);

	UObject* PropValue;
	PropertyHandle->GetValue(PropValue);
	OriginalProperty = CastChecked<UProperty>(PropValue);

	// Create a runtime UClass with the provided property as the only member. We will use this in the details view for the config hierarchy.
	ConfigEditorPropertyViewClass = NewObject<UClass>(GetTransientPackage(), TEXT("TempConfigEditorUClass"), RF_Public|RF_Standalone);

	// Keep a record of the UProperty we are looking to update
	ConfigEditorCopyOfEditProperty = DuplicateObject<UProperty>(OriginalProperty, ConfigEditorPropertyViewClass, PropValue->GetFName());
	ConfigEditorPropertyViewClass->ClassConfigName = OriginalProperty->GetOwnerClass()->ClassConfigName;
	ConfigEditorPropertyViewClass->SetSuperStruct(UObject::StaticClass());
	ConfigEditorPropertyViewClass->ClassFlags |= (CLASS_DefaultConfig | CLASS_Config);
	ConfigEditorPropertyViewClass->AddCppProperty(ConfigEditorCopyOfEditProperty);
	ConfigEditorPropertyViewClass->Bind();
	ConfigEditorPropertyViewClass->StaticLink(true);
	ConfigEditorPropertyViewClass->AssembleReferenceTokenStream();
	ConfigEditorPropertyViewClass->AddToRoot();
	
	// Cache the CDO for the object
	ConfigEditorPropertyViewCDO = ConfigEditorPropertyViewClass->GetDefaultObject(true);
	ConfigEditorPropertyViewCDO->AddToRoot();

	// Get access to all of the config files where this property is configurable.
	ConfigFilesHandle = DetailBuilder.GetProperty("ConfigFilePropertyObjects");
	DetailBuilder.HideProperty(ConfigFilesHandle);

	// Add the properties to a property table so we can edit these.
	IDetailCategoryBuilder& ConfigHierarchyCategory = DetailBuilder.EditCategory("ConfigHierarchy");
	ConfigHierarchyCategory.AddCustomRow(LOCTEXT("ConfigHierarchy", "ConfigHierarchy"))
	[
		// Create a property table with the values.
		ConstructPropertyTable(DetailBuilder)
	];

	// Listen for changes to the properties, we handle these by updating the ini file associated.
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &FConfigPropertyHelperDetails::OnPropertyValueChanged);
}


void FConfigPropertyHelperDetails::OnPropertyValueChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	UClass* OwnerClass = ConfigEditorCopyOfEditProperty->GetOwnerClass();
	if (Object->IsA(OwnerClass))
	{
		const FString* FileName = ConfigFileAndPropertySourcePairings.FindKey(Object);
		if (FileName != nullptr)
		{
			const FString& ConfigIniName = *FileName;

			// We should set this up to work with the UObject Config system, its difficult as the outer object isnt of the same type
			// create a sandbox FConfigCache
			FConfigCacheIni Config(EConfigCacheType::Temporary);

			// add an empty file to the config so it doesn't read in the original file (see FConfigCacheIni.Find())
			FConfigFile& NewFile = Config.Add(ConfigIniName, FConfigFile());

			// save the object properties to this file
			OriginalProperty->GetOwnerClass()->GetDefaultObject()->SaveConfig(CPF_Config, *ConfigIniName, &Config);

			// Take the saved section for this object and have the config system process and write out the one property we care about.
			ensureMsgf(Config.Num() == 1, TEXT("UObject::UpdateDefaultConfig() caused more files than expected in the Sandbox config cache!"));

			TArray<FString> Keys;
			NewFile.GetKeys(Keys);

			const FString SectionName = Keys[0];
			const FString PropertyName = ConfigEditorCopyOfEditProperty->GetName();
			FString	Value;
			ConfigEditorCopyOfEditProperty->ExportText_InContainer(0, Value, Object, Object, Object, 0);
			NewFile.SetString(*SectionName, *PropertyName, *Value);
			GConfig->SetString(*SectionName, *PropertyName, *Value, ConfigIniName);

			NewFile.UpdateSinglePropertyInSection(*ConfigIniName, *PropertyName, *SectionName);

			// reload the file, so that it refresh the cache internally.
			FString FinalIniFileName;
			GConfig->LoadGlobalIniFile(FinalIniFileName, *OriginalProperty->GetOwnerClass()->ClassConfigName.ToString(), NULL, true);

			// Update the CDO, as this change might have had an impact on it's value.
			OriginalProperty->GetOwnerClass()->GetDefaultObject()->ReloadConfig();
		}
	}
}


void FConfigPropertyHelperDetails::AddEditablePropertyForConfig(IDetailLayoutBuilder& DetailBuilder, const UPropertyConfigFileDisplayRow* ConfigFilePropertyRowObj)
{
	AssociatedConfigFileAndObjectPairings.Add(ConfigFilePropertyRowObj->ConfigFileName, (UObject*)ConfigFilePropertyRowObj);

	// Add the properties to a property table so we can edit these.
	IDetailCategoryBuilder& TempCategory = DetailBuilder.EditCategory("TempCategory");

	UObject* ConfigEntryObject = StaticDuplicateObject(ConfigEditorPropertyViewCDO, GetTransientPackage(), *(ConfigFilePropertyRowObj->ConfigFileName + TEXT("_cdoDupe")));
	ConfigEntryObject->AddToRoot();

	FString ExistingConfigEntryValue;
	static FString SectionName = OriginalProperty->GetOwnerClass()->GetPathName();
	static FString PropertyName = ConfigEditorCopyOfEditProperty->GetName();
	if (GConfig->GetString(*SectionName, *PropertyName, ExistingConfigEntryValue, ConfigFilePropertyRowObj->ConfigFileName))
	{
		ConfigEditorCopyOfEditProperty->ImportText(*ExistingConfigEntryValue, ConfigEditorCopyOfEditProperty->ContainerPtrToValuePtr<uint8>(ConfigEntryObject), 0, nullptr);
	}

	// Cache a reference for future usage.
	ConfigFileAndPropertySourcePairings.Add(ConfigFilePropertyRowObj->ConfigFileName, (UObject*)ConfigEntryObject);

	// We need to add a property row for each config file entry. 
	// This allows us to have an editable widget for each config file.
	TArray<UObject*> ConfigPropertyDisplayObjects;
	ConfigPropertyDisplayObjects.Add(ConfigEntryObject);
	if (IDetailPropertyRow* ExternalRow = TempCategory.AddExternalObjectProperty(ConfigPropertyDisplayObjects, ConfigEditorCopyOfEditProperty->GetFName()))
	{
		TSharedPtr<SWidget> NameWidget;
		TSharedPtr<SWidget> ValueWidget;
		ExternalRow->GetDefaultWidgets(NameWidget, ValueWidget);

		// Register the Value widget and config file pairing with the config editor.
		// The config editor needs this to determine what a cell presenter shows.
		IConfigEditorModule& ConfigEditor = FModuleManager::Get().LoadModuleChecked<IConfigEditorModule>("ConfigEditor");
		ConfigEditor.AddExternalPropertyValueWidgetAndConfigPairing(ConfigFilePropertyRowObj->ConfigFileName, ValueWidget);
		
		// now hide the property so it is not added to the property display view
		ExternalRow->Visibility(EVisibility::Hidden);
	}
}


TSharedRef<SWidget> FConfigPropertyHelperDetails::ConstructPropertyTable(IDetailLayoutBuilder& DetailBuilder)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyTable = PropertyEditorModule.CreatePropertyTable();
	PropertyTable->SetSelectionMode(ESelectionMode::None);
	PropertyTable->SetSelectionUnit(EPropertyTableSelectionUnit::None);
	PropertyTable->SetIsUserAllowedToChangeRoot(false);
	PropertyTable->SetShowObjectName(false);

	RepopulatePropertyTable(DetailBuilder);

	TArray< TSharedRef<class IPropertyTableCustomColumn>> CustomColumns;
	TSharedRef<FConfigPropertyCustomColumn> EditPropertyColumn = MakeShareable(new FConfigPropertyCustomColumn());
	EditPropertyColumn->EditProperty = ConfigEditorCopyOfEditProperty; // FindFieldChecked<UProperty>(UPropertyConfigFileDisplay::StaticClass(), TEXT("EditProperty"));
	CustomColumns.Add(EditPropertyColumn);

	//TSharedRef<FConfigPropertyConfigFileStateCustomColumn> ConfigFileStateColumn = MakeShareable(new FConfigPropertyConfigFileStateCustomColumn());
	//ConfigFileStateColumn->SupportedProperty = FindFieldChecked<UProperty>(UPropertyConfigFileDisplay::StaticClass(), TEXT("FileState"));
	//CustomColumns.Add(ConfigFileStateColumn);

	return PropertyEditorModule.CreatePropertyTableWidget(PropertyTable.ToSharedRef(), CustomColumns);
}


void FConfigPropertyHelperDetails::RepopulatePropertyTable(IDetailLayoutBuilder& DetailBuilder)
{
	// Clear out any previous entries from the table.
	AssociatedConfigFileAndObjectPairings.Empty();

	// Add an entry for each config so the value can be set in each of the config files independently.
	uint32 ConfigCount = 0;
	TSharedPtr<IPropertyHandleArray> ConfigFilesArrayHandle = ConfigFilesHandle->AsArray();
	ConfigFilesArrayHandle->GetNumElements(ConfigCount);

	// For each config file, add the capacity to edit this property.
	for (uint32 Index = 0; Index < ConfigCount; Index++)
	{
		FString ConfigFile;
		TSharedRef<IPropertyHandle> ConfigFileElementHandle = ConfigFilesArrayHandle->GetElement(Index);

		UObject* ConfigFileSinglePropertyHelperObj = nullptr;
		ensure(ConfigFileElementHandle->GetValue(ConfigFileSinglePropertyHelperObj) == FPropertyAccess::Success);

		UPropertyConfigFileDisplayRow* ConfigFileSinglePropertyHelper = CastChecked<UPropertyConfigFileDisplayRow>(ConfigFileSinglePropertyHelperObj);

		AddEditablePropertyForConfig(DetailBuilder, ConfigFileSinglePropertyHelper);
	}

	// We need a row for each config file
	TArray<UObject*> ConfigPropertyDisplayObjects;
	AssociatedConfigFileAndObjectPairings.GenerateValueArray(ConfigPropertyDisplayObjects);
	PropertyTable->SetObjects(ConfigPropertyDisplayObjects);

	// We need a column for each property in our Helper class.
	for (UProperty* NextProperty = UPropertyConfigFileDisplayRow::StaticClass()->PropertyLink; NextProperty; NextProperty = NextProperty->PropertyLinkNext)
	{
		PropertyTable->AddColumn((TWeakObjectPtr<UProperty>)NextProperty);
	}

	// Ensure the columns cannot be removed.
	TArray<TSharedRef<IPropertyTableColumn>> Columns = PropertyTable->GetColumns();
	for (TSharedRef<IPropertyTableColumn> Column : Columns)
	{
		Column->SetFrozen(true);
	}

	// Create the 'Config File' vs 'Property' table
	PropertyTable->RequestRefresh();
}


#undef LOCTEXT_NAMESPACE
