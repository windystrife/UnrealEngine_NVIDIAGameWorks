// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/InheritableComponentHandler.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/LinkerLoad.h"
#include "UObject/BlueprintsObjectVersion.h"
#include "UObject/UObjectHash.h" // for FindObjectWithOuter()

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#endif // WITH_EDITOR

// UInheritableComponentHandler

const FString UInheritableComponentHandler::SCSDefaultSceneRootOverrideNamePrefix(TEXT("ICH-"));

void UInheritableComponentHandler::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FBlueprintsObjectVersion::GUID);
}

void UInheritableComponentHandler::PostLoad()
{
	Super::PostLoad();

	if (!GIsDuplicatingClassForReinstancing)
	{
		for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
		{
			FComponentOverrideRecord& Record = Records[Index];
			if (Record.ComponentTemplate)
			{
				if (GetLinkerCustomVersion(FBlueprintsObjectVersion::GUID) < FBlueprintsObjectVersion::SCSHasComponentTemplateClass)
				{
					// Fix up component class on load, if it's not already set.
					if (Record.ComponentClass == nullptr)
					{
						Record.ComponentClass = Record.ComponentTemplate->GetClass();
					}

					// Fix up component template name on load, if it doesn't match the original template name. Otherwise, archetype lookups will fail for this template.
					// For example, this can occur after a component variable rename in a parent BP class, but before a child BP class with an override template is loaded.
					if (UActorComponent* OriginalTemplate = Record.ComponentKey.GetOriginalTemplate())
					{
						FString ExpectedTemplateName = OriginalTemplate->GetName();
						if (USCS_Node* SCSNode = Record.ComponentKey.FindSCSNode())
						{
							// We append a prefix onto SCS default scene root node overrides. This is done to ensure that the override template does not collide with our owner's own SCS default scene root node template.
							if (SCSNode == SCSNode->GetSCS()->GetDefaultSceneRootNode())
							{
								ExpectedTemplateName = SCSDefaultSceneRootOverrideNamePrefix + ExpectedTemplateName;
							}
						}

						if (ExpectedTemplateName != Record.ComponentTemplate->GetName())
						{
							FixComponentTemplateName(Record.ComponentTemplate, ExpectedTemplateName);
						}
					}
				}

				if (!CastChecked<UActorComponent>(Record.ComponentTemplate->GetArchetype())->IsEditableWhenInherited())
				{
					Record.ComponentTemplate->MarkPendingKill(); // hack needed to be able to identify if NewObject returns this back to us in the future
					Records.RemoveAtSwap(Index);
				}
				else if (Record.CookedComponentInstancingData.bIsValid)
				{
					// Generate "fast path" instancing data. This data may also be used to override components inherited from a nativized BP parent class.
					Record.CookedComponentInstancingData.LoadCachedPropertyDataForSerialization(Record.ComponentTemplate);
				}
			}
		}
	}
}

void UInheritableComponentHandler::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	for (auto Record : Records)
	{
		OutDeps.Add(Record.ComponentTemplate);
	}
}


#if WITH_EDITOR
UActorComponent* UInheritableComponentHandler::CreateOverridenComponentTemplate(FComponentKey Key)
{
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		FComponentOverrideRecord& Record = Records[Index];
		if (Record.ComponentKey.Match(Key))
		{
			if (Record.ComponentTemplate)
			{
				return Record.ComponentTemplate;
			}
			Records.RemoveAtSwap(Index);
			break;
		}
	}

	auto BestArchetype = FindBestArchetype(Key);
	if (!BestArchetype)
	{
		UE_LOG(LogBlueprint, Warning, TEXT("CreateOverridenComponentTemplate '%s': cannot find archetype for component '%s' from '%s'"),
			*GetPathNameSafe(this), *Key.GetSCSVariableName().ToString(), *GetPathNameSafe(Key.GetComponentOwner()));
		return NULL;
	}
	
	FName NewComponentTemplateName = BestArchetype->GetFName();
	if (USCS_Node* SCSNode = Key.FindSCSNode())
	{
		// If this template will override an inherited DefaultSceneRoot node from a parent class's SCS, adjust the template name so that we don't reallocate our owner class's SCS DefaultSceneRoot node template.
		// Note: This is currently the only case where a child class can have both an SCS node template and an override template associated with the same variable name, that is not considered to be a collision.
		if (SCSNode == SCSNode->GetSCS()->GetDefaultSceneRootNode())
		{
			NewComponentTemplateName = FName(*(SCSDefaultSceneRootOverrideNamePrefix + BestArchetype->GetName()));
		}
	}

	ensure(Cast<UBlueprintGeneratedClass>(GetOuter()));
	auto NewComponentTemplate = NewObject<UActorComponent>(
		GetOuter(), BestArchetype->GetClass(), NewComponentTemplateName, RF_ArchetypeObject | RF_Public | RF_InheritableComponentTemplate, BestArchetype);

	// HACK: NewObject can return a pre-existing object which will not have been initialized to the archetype.  When we remove the old handlers, we mark them pending
	//       kill so we can identify that situation here (see UE-13987/UE-13990)
	if (NewComponentTemplate->IsPendingKill())
	{
		NewComponentTemplate->ClearPendingKill();
		UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
		CopyParams.bDoDelta = false;
		UEngine::CopyPropertiesForUnrelatedObjects(BestArchetype, NewComponentTemplate, CopyParams);
	}

	// Clear transient flag if it was transient before and re copy off archetype
	if (NewComponentTemplate->HasAnyFlags(RF_Transient) && UnnecessaryComponents.Contains(NewComponentTemplate))
	{
		NewComponentTemplate->ClearFlags(RF_Transient);
		UnnecessaryComponents.Remove(NewComponentTemplate);

		UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
		CopyParams.bDoDelta = false;
		UEngine::CopyPropertiesForUnrelatedObjects(BestArchetype, NewComponentTemplate, CopyParams);
	}

	FComponentOverrideRecord NewRecord;
	NewRecord.ComponentKey = Key;
	NewRecord.ComponentClass = NewComponentTemplate->GetClass();
	NewRecord.ComponentTemplate = NewComponentTemplate;
	Records.Add(NewRecord);

	return NewComponentTemplate;
}

void UInheritableComponentHandler::RemoveOverridenComponentTemplate(FComponentKey Key)
{
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		FComponentOverrideRecord& Record = Records[Index];
		if (Record.ComponentKey.Match(Key))
		{
			Record.ComponentTemplate->MarkPendingKill(); // hack needed to be able to identify if NewObject returns this back to us in the future
			Records.RemoveAtSwap(Index);
			break;
		}
	}
}

void UInheritableComponentHandler::UpdateOwnerClass(UBlueprintGeneratedClass* OwnerClass)
{
	for (auto& Record : Records)
	{
		auto OldComponentTemplate = Record.ComponentTemplate;
		if (OldComponentTemplate && (OwnerClass != OldComponentTemplate->GetOuter()))
		{
			Record.ComponentTemplate = DuplicateObject(OldComponentTemplate, OwnerClass, OldComponentTemplate->GetFName());
		}
	}
}

void UInheritableComponentHandler::ValidateTemplates()
{
	for (int32 Index = 0; Index < Records.Num();)
	{
		bool bIsValidAndNecessary = false;
		{
			FComponentOverrideRecord& Record = Records[Index];
			FComponentKey& ComponentKey = Record.ComponentKey;
			
			FName VarName = ComponentKey.GetSCSVariableName();
			if (ComponentKey.RefreshVariableName())
			{
				FName NewName = ComponentKey.GetSCSVariableName();
				UE_LOG(LogBlueprint, Log, TEXT("ValidateTemplates '%s': variable old name '%s' new name '%s'"),
					*GetPathNameSafe(this), *VarName.ToString(), *NewName.ToString());
				VarName = NewName;

				MarkPackageDirty();
			}

			if (IsRecordValid(Record))
			{
				if (IsRecordNecessary(Record))
				{
					bIsValidAndNecessary = true;
				}
				else
				{
					// Set transient flag so this object does not get used as an archetype for subclasses
					if (Record.ComponentTemplate)
					{
						Record.ComponentTemplate->SetFlags(RF_Transient);
						UnnecessaryComponents.AddUnique(Record.ComponentTemplate);
					}

					UE_LOG(LogBlueprint, Log, TEXT("ValidateTemplates '%s': overridden template is unnecessary and will be removed - component '%s' from '%s'"),
						*GetPathNameSafe(this), *VarName.ToString(), *GetPathNameSafe(ComponentKey.GetComponentOwner()));
				}
			}
			else
			{
				UE_LOG(LogBlueprint, Warning, TEXT("ValidateTemplates '%s': overridden template is invalid and will be removed - component '%s' from '%s'"),
					*GetPathNameSafe(this), *VarName.ToString(), *GetPathNameSafe(ComponentKey.GetComponentOwner()));
			}
		}

		if (bIsValidAndNecessary)
		{
			++Index;
		}
		else
		{
			Records.RemoveAtSwap(Index);
		}
	}
}

bool UInheritableComponentHandler::IsValid() const
{
	for (auto& Record : Records)
	{
		if (!IsRecordValid(Record))
		{
			return false;
		}
	}
	return true;
}

bool UInheritableComponentHandler::IsRecordValid(const FComponentOverrideRecord& Record) const
{
	UClass* OwnerClass = Cast<UClass>(GetOuter());
	if (!ensure(OwnerClass))
	{
		return false;
	}

	if (!Record.ComponentTemplate)
	{
		// Note: We still consider the record to be valid, even if the template is missing, if we have valid class information. This typically will indicate that the template object was filtered at load time (to save space, e.g. dedicated server).
		return Record.ComponentClass != nullptr;
	}

	if (Record.ComponentTemplate->GetOuter() != OwnerClass)
	{
		return false;
	}

	if (!Record.ComponentKey.IsValid())
	{
		return false;
	}

	UClass* ComponentOwner = Record.ComponentKey.GetComponentOwner();
	if (!ComponentOwner || !OwnerClass->IsChildOf(ComponentOwner))
	{
		return false;
	}

	// Note: If the original template is missing, we consider the record to be unnecessary, but not invalid.
	UActorComponent* OriginalTemplate = Record.ComponentKey.GetOriginalTemplate();
	if (OriginalTemplate != nullptr && OriginalTemplate->GetClass() != Record.ComponentTemplate->GetClass())
	{
		return false;
	}
	
	return true;
}

struct FComponentComparisonHelper
{
	static bool AreIdentical(UObject* ObjectA, UObject* ObjectB)
	{
		if (!ObjectA || !ObjectB || (ObjectA->GetClass() != ObjectB->GetClass()))
		{
			return false;
		}

		bool Result = true;
		for (UProperty* Prop = ObjectA->GetClass()->PropertyLink; Prop && Result; Prop = Prop->PropertyLinkNext)
		{
			bool bConsiderProperty = Prop->ShouldDuplicateValue(); //Should the property be compared at all?
			if (bConsiderProperty)
			{
				for (int32 Idx = 0; (Idx < Prop->ArrayDim) && Result; Idx++)
				{
					if (!Prop->Identical_InContainer(ObjectA, ObjectB, Idx, PPF_DeepComparison))
					{
						Result = false;
					}
				}
			}
		}
		if (Result)
		{
			// Allow the component to compare its native/ intrinsic properties.
			Result = ObjectA->AreNativePropertiesIdenticalTo(ObjectB);
		}
		return Result;
	}
};

bool UInheritableComponentHandler::IsRecordNecessary(const FComponentOverrideRecord& Record) const
{
	// If the record's template was not loaded, check to see if the class information is valid.
	if (Record.ComponentTemplate == nullptr)
	{
		if (Record.ComponentClass != nullptr)
		{
			UObject* ComponentCDO = Record.ComponentClass->GetDefaultObject();
			if (ComponentCDO != nullptr)
			{
				// The record is considered necessary if the class information is valid but the template was not loaded due to client/server exclusion at load time (e.g. uncooked dedicated server).
				return !UObject::CanCreateInCurrentContext(ComponentCDO);
			}
		}
		
		// Otherwise, we don't need to keep the record if the template is NULL.
		return false;
	}
	else
	{
		// Consider the record to be unnecessary if the original template no longer exists.
		UActorComponent* OriginalTemplate = Record.ComponentKey.GetOriginalTemplate();
		if (OriginalTemplate == nullptr)
		{
			return false;
		}
	
		auto ChildComponentTemplate = Record.ComponentTemplate;
		auto ParentComponentTemplate = FindBestArchetype(Record.ComponentKey);
		check(ChildComponentTemplate && ParentComponentTemplate && (ParentComponentTemplate != ChildComponentTemplate));
		return !FComponentComparisonHelper::AreIdentical(ChildComponentTemplate, ParentComponentTemplate);
	}
}

UActorComponent* UInheritableComponentHandler::FindBestArchetype(FComponentKey Key) const
{
	UActorComponent* ClosestArchetype = nullptr;

	auto ActualBPGC = Cast<UBlueprintGeneratedClass>(GetOuter());
	if (ActualBPGC && Key.GetComponentOwner() && (ActualBPGC != Key.GetComponentOwner()))
	{
		ActualBPGC = Cast<UBlueprintGeneratedClass>(ActualBPGC->GetSuperClass());
		while (!ClosestArchetype && ActualBPGC)
		{
			if (ActualBPGC->InheritableComponentHandler)
			{
				ClosestArchetype = ActualBPGC->InheritableComponentHandler->GetOverridenComponentTemplate(Key);
			}
			ActualBPGC = Cast<UBlueprintGeneratedClass>(ActualBPGC->GetSuperClass());
		}

		if (!ClosestArchetype)
		{
			ClosestArchetype = Key.GetOriginalTemplate();
		}
	}

	return ClosestArchetype;
}

bool UInheritableComponentHandler::RefreshTemplateName(FComponentKey OldKey)
{
	for (auto& Record : Records)
	{
		if (Record.ComponentKey.Match(OldKey))
		{
			Record.ComponentKey.RefreshVariableName();
			return true;
		}
	}
	return false;
}

FComponentKey UInheritableComponentHandler::FindKey(UActorComponent* ComponentTemplate) const
{
	for (auto& Record : Records)
	{
		if (Record.ComponentTemplate == ComponentTemplate)
		{
			return Record.ComponentKey;
		}
	}
	return FComponentKey();
}

#endif

void UInheritableComponentHandler::PreloadAllTempates()
{
	for (auto Record : Records)
	{
		if (Record.ComponentTemplate && Record.ComponentTemplate->HasAllFlags(RF_NeedLoad))
		{
			auto Linker = Record.ComponentTemplate->GetLinker();
			if (Linker)
			{
				Linker->Preload(Record.ComponentTemplate);
			}
		}
	}
}

void UInheritableComponentHandler::PreloadAll()
{
	if (HasAllFlags(RF_NeedLoad))
	{
		auto Linker = GetLinker();
		if (Linker)
		{
			Linker->Preload(this);
		}
	}
	PreloadAllTempates();
}

FComponentKey UInheritableComponentHandler::FindKey(const FName VariableName) const
{
	for (const FComponentOverrideRecord& Record : Records)
	{
		if (Record.ComponentKey.GetSCSVariableName() == VariableName || (Record.ComponentTemplate && Record.ComponentTemplate->GetFName() == VariableName))
		{
			return Record.ComponentKey;
		}
	}
	return FComponentKey();
}

UActorComponent* UInheritableComponentHandler::GetOverridenComponentTemplate(FComponentKey Key) const
{
	auto Record = FindRecord(Key);
	return Record ? Record->ComponentTemplate : nullptr;
}

const FBlueprintCookedComponentInstancingData* UInheritableComponentHandler::GetOverridenComponentTemplateData(FComponentKey Key) const
{
	auto Record = FindRecord(Key);
	return Record ? &Record->CookedComponentInstancingData : nullptr;
}

const FComponentOverrideRecord* UInheritableComponentHandler::FindRecord(const FComponentKey Key) const
{
	for (auto& Record : Records)
	{
		if (Record.ComponentKey.Match(Key))
		{
			return &Record;
		}
	}
	return nullptr;
}

void UInheritableComponentHandler::FixComponentTemplateName(UActorComponent* ComponentTemplate, const FString& NewName)
{
	// Override template names were not previously kept in sync w/ past node rename operations. Thus, we need to check for
	// and correct other (stale) template names. Otherwise, these could collide with the one we're trying to correct here.
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		FComponentOverrideRecord& Record = Records[Index];
		if (Record.ComponentTemplate && Record.ComponentTemplate != ComponentTemplate && Record.ComponentTemplate->GetName() == NewName)
		{
			if (UActorComponent* OriginalTemplate = Record.ComponentKey.GetOriginalTemplate())
			{
				if (OriginalTemplate->GetName() != Record.ComponentTemplate->GetName())
				{
					// Recursively fix up this record's component template name first to also match its original template, which will then free up the name.
					FixComponentTemplateName(Record.ComponentTemplate, OriginalTemplate->GetName());
				}
			}

			// There should only be at most one collision, so we'll stop looking now.
			break;
		}
	}

	// Precondition: There are no other objects in the same scope with this name.
	check(!FindObjectWithOuter(ComponentTemplate->GetOuter(), nullptr, FName(*NewName)));

	// Now that we're sure there are no collisions with other records, we can safely rename this one to its new name.
	ComponentTemplate->Rename(*NewName, nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
}

// FComponentOverrideRecord

FComponentKey::FComponentKey(const USCS_Node* SCSNode) : OwnerClass(nullptr)
{
	if (SCSNode)
	{
		const USimpleConstructionScript* ParentSCS = SCSNode->GetSCS();
		OwnerClass      = ParentSCS ? ParentSCS->GetOwnerClass() : nullptr;
		AssociatedGuid  = SCSNode->VariableGuid;
		SCSVariableName = SCSNode->GetVariableName();
	}
}

#if WITH_EDITOR
FComponentKey::FComponentKey(UBlueprint* Blueprint, const FUCSComponentId& UCSComponentID)
{
	OwnerClass     = Blueprint->GeneratedClass;
	AssociatedGuid = UCSComponentID.GetAssociatedGuid();
}
#endif // WITH_EDITOR

bool FComponentKey::Match(const FComponentKey OtherKey) const
{
	return (OwnerClass == OtherKey.OwnerClass) && (AssociatedGuid == OtherKey.AssociatedGuid);
}

USCS_Node* FComponentKey::FindSCSNode() const
{
	USimpleConstructionScript* ParentSCS = (OwnerClass && IsSCSKey()) ? CastChecked<UBlueprintGeneratedClass>(OwnerClass)->SimpleConstructionScript : nullptr;
	return ParentSCS ? ParentSCS->FindSCSNodeByGuid(AssociatedGuid) : nullptr;
}

UActorComponent* FComponentKey::GetOriginalTemplate() const
{
	UActorComponent* ComponentTemplate = nullptr;
	if (IsSCSKey())
	{
		if (USCS_Node* SCSNode = FindSCSNode())
		{
			ComponentTemplate = SCSNode->ComponentTemplate;
		}
	}
#if WITH_EDITOR
	else if (IsUCSKey())
	{
		ComponentTemplate = FBlueprintEditorUtils::FindUCSComponentTemplate(*this);
	}
#endif // WITH_EDITOR
	return ComponentTemplate;
}

bool FComponentKey::RefreshVariableName()
{
	if (IsValid() && IsSCSKey())
	{
		USCS_Node* SCSNode = FindSCSNode();
		const FName UpdatedName = SCSNode ? SCSNode->GetVariableName() : NAME_None;

		if (UpdatedName != SCSVariableName)
		{
			SCSVariableName = UpdatedName;
			return true;
		}
	}
	return false;
}
