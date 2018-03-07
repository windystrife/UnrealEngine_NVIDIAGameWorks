// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Misc/ITransaction.h"
#include "UObject/UnrealType.h"
#include "Engine/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Blueprint/BlueprintSupport.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "UserDefinedStructEditorData"

void FStructVariableDescription::PostSerialize(const FArchive& Ar)
{
	if (ContainerType == EPinContainerType::None)
	{
		ContainerType = FEdGraphPinType::ToPinContainerType(bIsArray_DEPRECATED, bIsSet_DEPRECATED, bIsMap_DEPRECATED);
	}
}

bool FStructVariableDescription::SetPinType(const FEdGraphPinType& VarType)
{
	Category = VarType.PinCategory;
	SubCategory = VarType.PinSubCategory;
	SubCategoryObject = VarType.PinSubCategoryObject.Get();
	PinValueType = VarType.PinValueType;
	ContainerType = VarType.ContainerType;

	return !VarType.bIsReference && !VarType.bIsWeakPointer;
}

FEdGraphPinType FStructVariableDescription::ToPinType() const
{
	return FEdGraphPinType(Category, SubCategory, SubCategoryObject.LoadSynchronous(), ContainerType, false, PinValueType);
}

UUserDefinedStructEditorData::UUserDefinedStructEditorData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UUserDefinedStruct* ScriptStruct = GetOwnerStruct();
	if (ScriptStruct)
	{
		DefaultStructInstance.SetPackage(ScriptStruct->GetOutermost());
	}
}

uint32 UUserDefinedStructEditorData::GenerateUniqueNameIdForMemberVariable()
{
	const uint32 Result = UniqueNameId;
	++UniqueNameId;
	return Result;
}

UUserDefinedStruct* UUserDefinedStructEditorData::GetOwnerStruct() const
{
	return Cast<UUserDefinedStruct>(GetOuter());
}

void UUserDefinedStructEditorData::PostUndo(bool bSuccess)
{
	GEditor->UnregisterForUndo(this);
	// TODO: In the undo case we might want to flip the change type since an add is now a remove and vice versa
	FStructureEditorUtils::OnStructureChanged(GetOwnerStruct(), CachedStructureChange);
	CachedStructureChange = FStructureEditorUtils::Unknown;
}

void UUserDefinedStructEditorData::ConsolidatedPostEditUndo(const FStructureEditorUtils::EStructureEditorChangeInfo TransactedStructureChange)
{
	ensure(CachedStructureChange == FStructureEditorUtils::Unknown);
	CachedStructureChange = TransactedStructureChange;
	GEditor->RegisterForUndo(this);
}

void UUserDefinedStructEditorData::PostEditUndo()
{
	Super::PostEditUndo();
	ConsolidatedPostEditUndo(FStructureEditorUtils::Unknown);
}

class FStructureTransactionAnnotation : public ITransactionObjectAnnotation
{
public:
	FStructureTransactionAnnotation(FStructureEditorUtils::EStructureEditorChangeInfo ChangeInfo)
		: ActiveChange(ChangeInfo)
	{
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { /** Don't need this functionality for now */ }

	FStructureEditorUtils::EStructureEditorChangeInfo GetActiveChange()
	{
		return ActiveChange;
	}

protected:
	FStructureEditorUtils::EStructureEditorChangeInfo ActiveChange;
};

TSharedPtr<ITransactionObjectAnnotation> UUserDefinedStructEditorData::GetTransactionAnnotation() const
{
	return MakeShareable(new FStructureTransactionAnnotation(FStructureEditorUtils::FStructEditorManager::ActiveChange));
}

void UUserDefinedStructEditorData::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
{
	Super::PostEditUndo();
	FStructureEditorUtils::EStructureEditorChangeInfo TransactedStructureChange = FStructureEditorUtils::Unknown;

	if (TransactionAnnotation.IsValid())
	{
		TSharedPtr<FStructureTransactionAnnotation> StructAnnotation = StaticCastSharedPtr<FStructureTransactionAnnotation>(TransactionAnnotation);
		if (StructAnnotation.IsValid())
		{
			TransactedStructureChange = StructAnnotation->GetActiveChange();
		}
	}
	ConsolidatedPostEditUndo(TransactedStructureChange);
}

void UUserDefinedStructEditorData::PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph)
{
	Super::PostLoadSubobjects(OuterInstanceGraph);

	for (FStructVariableDescription& VarDesc : VariablesDescriptions)
	{
		VarDesc.bInvalidMember = !FStructureEditorUtils::CanHaveAMemberVariableOfType(GetOwnerStruct(), VarDesc.ToPinType());
	}
}

const uint8* UUserDefinedStructEditorData::GetDefaultInstance() const
{
	ensure(DefaultStructInstance.IsValid() && DefaultStructInstance.GetStruct() == GetOwnerStruct());
	return DefaultStructInstance.GetStructMemory();
}

void UUserDefinedStructEditorData::RecreateDefaultInstance(FString* OutLog)
{
	UStruct* ScriptStruct = GetOwnerStruct();
	DefaultStructInstance.Recreate(ScriptStruct);
	uint8* StructData = DefaultStructInstance.GetStructMemory();
	ensure(DefaultStructInstance.IsValid() && DefaultStructInstance.GetStruct() == ScriptStruct);
	if (DefaultStructInstance.IsValid() && StructData && ScriptStruct)
	{
		// When loading, the property's default value may end up being filled with a placeholder. 
		// This tracker object allows the linker to track the actual object that is being filled in 
		// so it can calculate an offset to the property and write in the placeholder value:
		FScopedPlaceholderRawContainerTracker TrackDefaultObject(StructData);

		DefaultStructInstance.SetPackage(ScriptStruct->GetOutermost());

		for (TFieldIterator<UProperty> It(ScriptStruct); It; ++It)
		{
			UProperty* Property = *It;
			if (Property)
			{
				FStructVariableDescription* VarDesc = VariablesDescriptions.FindByPredicate(FStructureEditorUtils::FFindByNameHelper<FStructVariableDescription>(Property->GetFName()));
				if (VarDesc && !VarDesc->CurrentDefaultValue.IsEmpty())
				{
					if (!FBlueprintEditorUtils::PropertyValueFromString(Property, VarDesc->CurrentDefaultValue, StructData))
					{
						const FString Message = FString::Printf(TEXT("Cannot parse value. Property: %s String: \"%s\" ")
							, *Property->GetDisplayNameText().ToString()
							, *VarDesc->CurrentDefaultValue);
						UE_LOG(LogClass, Warning, TEXT("UUserDefinedStructEditorData::RecreateDefaultInstance %s Struct: %s "), *Message, *GetPathNameSafe(ScriptStruct));
						if (OutLog)
						{
							OutLog->Append(Message);
						}
					}
				}
			}
		}
	}
}

void UUserDefinedStructEditorData::CleanDefaultInstance()
{
	ensure(!DefaultStructInstance.IsValid() || DefaultStructInstance.GetStruct() == GetOwnerStruct());
	DefaultStructInstance.Destroy();
}

void UUserDefinedStructEditorData::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UUserDefinedStructEditorData* This = CastChecked<UUserDefinedStructEditorData>(InThis);

	UStruct* ScriptStruct = This->GetOwnerStruct();
	ensure(!This->DefaultStructInstance.IsValid() || This->DefaultStructInstance.GetStruct() == ScriptStruct);
	uint8* StructData = This->DefaultStructInstance.GetStructMemory();
	if (StructData)
	{
		ScriptStruct->SerializeBin(Collector.GetVerySlowReferenceCollectorArchive(), StructData);
	}

	Super::AddReferencedObjects(This, Collector);
}

#undef LOCTEXT_NAMESPACE
