// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "Exporters/FbxExportOption.h"

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/UObjectHash.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "ConfigCacheIni.h"

UFbxExportOption::UFbxExportOption(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FbxExportCompatibility = EFbxExportCompatibility::FBX_2013;
	bForceFrontXAxis = false;
	LevelOfDetail = true;
	Collision = true;
	VertexColor = true;
	WeldedVertices = true; // By default we want to weld verts, but provide option to not weld
	MapSkeletalMotionToRoot = false;
}

void UFbxExportOption::ResetToDefault()
{
	ReloadConfig();
}

/** Load UI settings from ini file */
void UFbxExportOption::LoadOptions()
{
	int32 PortFlags = 0;

	for (UProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Config))
		{
			continue;
		}
		FString Section = TEXT("FBX_Export_UI_Option_") + GetClass()->GetName();
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
					}
				}
			}
		}
	}
}

/** Save UI settings to ini file */
void UFbxExportOption::SaveOptions()
{
	int32 PortFlags = 0;

	for (UProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Config))
		{
			continue;
		}
		FString Section = TEXT("FBX_Export_UI_Option_") + GetClass()->GetName();
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
