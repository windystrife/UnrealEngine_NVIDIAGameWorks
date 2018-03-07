// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "SpeedTreeImportData.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SCheckBox.h"

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/UObjectHash.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "SpeedTreeImportDataDetails"

DEFINE_LOG_CATEGORY_STATIC(LogSpeedTreeImportData, Log, All);

USpeedTreeImportData::USpeedTreeImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ImportGeometryType = IGT_3D;
	TreeScale = 30.48f;
	LODType = ILT_PaintedFoliage;
}

void USpeedTreeImportData::CopyFrom(USpeedTreeImportData* Other)
{
	TreeScale = Other->TreeScale;
	ImportGeometryType = Other->ImportGeometryType;
	LODType = Other->LODType;
	IncludeCollision = Other->IncludeCollision;
	MakeMaterialsCheck = Other->MakeMaterialsCheck;
	IncludeNormalMapCheck = Other->IncludeNormalMapCheck;
	IncludeDetailMapCheck = Other->IncludeDetailMapCheck;
	IncludeSpecularMapCheck = Other->IncludeSpecularMapCheck;
	IncludeBranchSeamSmoothing = Other->IncludeBranchSeamSmoothing;
	IncludeSpeedTreeAO = Other->IncludeSpeedTreeAO;
	IncludeColorAdjustment = Other->IncludeColorAdjustment;
	IncludeVertexProcessingCheck = Other->IncludeVertexProcessingCheck;
	IncludeWindCheck = Other->IncludeWindCheck;
	IncludeSmoothLODCheck = Other->IncludeSmoothLODCheck;
}

void USpeedTreeImportData::SaveOptions()
{
	int32 PortFlags = 0;

	for (UProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Config))
		{
			continue;
		}
		FString Section = TEXT("SpeedTree_Import_UI_Option_") + GetClass()->GetName();
		FString Key = Property->GetName();

		const bool bIsPropertyInherited = Property->GetOwnerClass() != GetClass();
		UObject* SuperClassDefaultObject = GetClass()->GetSuperClass()->GetDefaultObject();

		UArrayProperty* Array = dynamic_cast<UArrayProperty*>(Property);
		if (Array)
		{
			FConfigSection* Sec = GConfig->GetSectionPrivate(*Section, 1, 0, *GEditorPerProjectIni);
			check(Sec);
			Sec->Remove(*Key);

			FScriptArrayHelper_InContainer ArrayHelper(Array, this);
			for (int32 i = 0; i < ArrayHelper.Num(); i++)
			{
				FString	Buffer;
				Array->Inner->ExportTextItem(Buffer, ArrayHelper.GetRawPtr(i), ArrayHelper.GetRawPtr(i), this, PortFlags);
				Sec->Add(*Key, *Buffer);
			}
		}
		else
		{
			TCHAR TempKey[MAX_SPRINTF] = TEXT("");
			for (int32 Index = 0; Index < Property->ArrayDim; Index++)
			{
				if (Property->ArrayDim != 1)
				{
					FCString::Sprintf(TempKey, TEXT("%s[%i]"), *Property->GetName(), Index);
					Key = TempKey;
				}

				FString	Value;
				Property->ExportText_InContainer(Index, Value, this, this, this, PortFlags);
				GConfig->SetString(*Section, *Key, *Value, *GEditorPerProjectIni);
			}
		}
	}
	GConfig->Flush(0);
}

void USpeedTreeImportData::LoadOptions()
{
	int32 PortFlags = 0;

	for (UProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Config))
		{
			continue;
		}
		FString Section = TEXT("SpeedTree_Import_UI_Option_") + GetClass()->GetName();
		FString Key = Property->GetName();

		const bool bIsPropertyInherited = Property->GetOwnerClass() != GetClass();
		UObject* SuperClassDefaultObject = GetClass()->GetSuperClass()->GetDefaultObject();

		const FString& PropFileName = GEditorPerProjectIni;

		UArrayProperty* Array = dynamic_cast<UArrayProperty*>(Property);
		if (Array)
		{
			FConfigSection* Sec = GConfig->GetSectionPrivate(*Section, 0, 1, *GEditorPerProjectIni);
			if (Sec != nullptr)
			{
				TArray<FConfigValue> List;
				const FName KeyName(*Key, FNAME_Find);
				Sec->MultiFind(KeyName, List);

				FScriptArrayHelper_InContainer ArrayHelper(Array, this);
				// Only override default properties if there is something to override them with.
				if (List.Num() > 0)
				{
					ArrayHelper.EmptyAndAddValues(List.Num());
					for (int32 i = List.Num() - 1, c = 0; i >= 0; i--, c++)
					{
						Array->Inner->ImportText(*List[i].GetValue(), ArrayHelper.GetRawPtr(c), PortFlags, this);
					}
				}
				else
				{
					int32 Index = 0;
					const FConfigValue* ElementValue = nullptr;
					do
					{
						// Add array index number to end of key
						FString IndexedKey = FString::Printf(TEXT("%s[%i]"), *Key, Index);

						// Try to find value of key
						const FName IndexedName(*IndexedKey, FNAME_Find);
						if (IndexedName == NAME_None)
						{
							break;
						}
						ElementValue = Sec->Find(IndexedName);

						// If found, import the element
						if (ElementValue != nullptr)
						{
							// expand the array if necessary so that Index is a valid element
							ArrayHelper.ExpandForIndex(Index);
							Array->Inner->ImportText(*ElementValue->GetValue(), ArrayHelper.GetRawPtr(Index), PortFlags, this);
						}

						Index++;
					} while (ElementValue || Index < ArrayHelper.Num());
				}
			}
		}
		else
		{
			for (int32 i = 0; i < Property->ArrayDim; i++)
			{
				if (Property->ArrayDim != 1)
				{
					Key = FString::Printf(TEXT("%s[%i]"), *Property->GetName(), i);
				}

				FString Value;
				bool bFoundValue = GConfig->GetString(*Section, *Key, Value, *GEditorPerProjectIni);

				if (bFoundValue)
				{
					if (Property->ImportText(*Value, Property->ContainerPtrToValuePtr<uint8>(this, i), PortFlags, this) == NULL)
					{
						// this should be an error as the properties from the .ini / .int file are not correctly being read in and probably are affecting things in subtle ways
						UE_LOG(LogSpeedTreeImportData, Error, TEXT("SpeedTree Options LoadOptions (%s): failed for %s in: %s"), *GetPathName(), *Property->GetName(), *Value);
					}
				}
			}
		}
	}
}

FSpeedTreeImportDataDetails::FSpeedTreeImportDataDetails()
{
	SpeedTreeImportData = nullptr;
	CachedDetailBuilder = nullptr;
}

TSharedRef<IDetailCustomization> FSpeedTreeImportDataDetails::MakeInstance()
{
	return MakeShareable(new FSpeedTreeImportDataDetails);
}

/** IDetailCustomization interface */
void FSpeedTreeImportDataDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	CachedDetailBuilder = &DetailLayout;
	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailLayout.GetObjectsBeingCustomized(EditingObjects);
	check(EditingObjects.Num() == 1);
	SpeedTreeImportData = Cast<USpeedTreeImportData>(EditingObjects[0].Get());
	if (SpeedTreeImportData == nullptr)
	{
		return;
	}

	//We have to hide FilePath category
	DetailLayout.HideCategory(FName(TEXT("File Path")));
	
	//Mesh category Must be the first category (Important)
	DetailLayout.EditCategory(FName(TEXT("Mesh")), FText::GetEmpty(), ECategoryPriority::Important);

	//Get the Materials category
	IDetailCategoryBuilder& MaterialsCategoryBuilder = DetailLayout.EditCategory(FName(TEXT("Materials")));
	TArray<TSharedRef<IPropertyHandle>> MaterialCategoryDefaultProperties;
	MaterialsCategoryBuilder.GetDefaultProperties(MaterialCategoryDefaultProperties);
	
	//We have to make the logic for vertex processing
	TSharedRef<IPropertyHandle> MakeMaterialsCheckProp = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USpeedTreeImportData, MakeMaterialsCheck));
	MakeMaterialsCheckProp->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FSpeedTreeImportDataDetails::OnForceRefresh));

	TSharedRef<IPropertyHandle> IncludeVertexProcessingCheckProp = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USpeedTreeImportData, IncludeVertexProcessingCheck));
	IncludeVertexProcessingCheckProp->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FSpeedTreeImportDataDetails::OnForceRefresh));

	//Hide all properties, we will show them in the correct order with the correct grouping
	for (TSharedRef<IPropertyHandle> Handle : MaterialCategoryDefaultProperties)
	{
		DetailLayout.HideProperty(Handle);
	}

	MaterialsCategoryBuilder.AddProperty(MakeMaterialsCheckProp);
	if (SpeedTreeImportData->MakeMaterialsCheck)
	{
		for (TSharedRef<IPropertyHandle> Handle : MaterialCategoryDefaultProperties)
		{
			const FString& MetaData = Handle->GetMetaData(TEXT("EditCondition"));
			if (MetaData.Compare(TEXT("MakeMaterialsCheck")) == 0 && IncludeVertexProcessingCheckProp->GetProperty() != Handle->GetProperty())
			{
				MaterialsCategoryBuilder.AddProperty(Handle);
			}
		}
		IDetailGroup& VertexProcessingGroup = MaterialsCategoryBuilder.AddGroup(FName(TEXT("VertexProcessingGroup")), LOCTEXT("VertexProcessingGroup_DisplayName", "Vertex Processing"), false, true);
		VertexProcessingGroup.AddPropertyRow(IncludeVertexProcessingCheckProp);
		for (TSharedRef<IPropertyHandle> Handle : MaterialCategoryDefaultProperties)
		{
			const FString& MetaData = Handle->GetMetaData(TEXT("EditCondition"));
			if (MetaData.Compare(TEXT("IncludeVertexProcessingCheck")) == 0)
			{
				VertexProcessingGroup.AddPropertyRow(Handle);
			}
		}
	}
}

void FSpeedTreeImportDataDetails::OnForceRefresh()
{
	if (CachedDetailBuilder)
	{
		CachedDetailBuilder->ForceRefreshDetails();
	}
}

#undef LOCTEXT_NAMESPACE