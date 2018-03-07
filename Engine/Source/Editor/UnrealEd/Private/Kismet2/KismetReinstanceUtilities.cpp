// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet2/KismetReinstanceUtilities.h"
#include "ComponentInstanceDataCache.h"
#include "Engine/Blueprint.h"
#include "Stats/StatsMisc.h"
#include "UObject/Package.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/Engine.h"
#include "Editor/EditorEngine.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "FileHelpers.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Layers/ILayers.h"
#include "Editor.h"
#include "Serialization/ArchiveHasReferences.h"
#include "Toolkits/AssetEditorManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Serialization/FindObjectReferencers.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "BlueprintEditor.h"
#include "Engine/Selection.h"
#include "BlueprintEditorSettings.h"

extern COREUOBJECT_API bool GBlueprintUseCompilationManager;

DECLARE_CYCLE_STAT(TEXT("Replace Instances"), EKismetReinstancerStats_ReplaceInstancesOfClass, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Find Referencers"), EKismetReinstancerStats_FindReferencers, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Replace References"), EKismetReinstancerStats_ReplaceReferences, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Construct Replacements"), EKismetReinstancerStats_ReplacementConstruction, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Update Bytecode References"), EKismetReinstancerStats_UpdateBytecodeReferences, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Recompile Child Classes"), EKismetReinstancerStats_RecompileChildClasses, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Replace Classes Without Reinstancing"), EKismetReinstancerStats_ReplaceClassNoReinsancing, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Reinstance Objects"), EKismetCompilerStats_ReinstanceObjects, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Refresh Dependent Blueprints In Reinstancer"), EKismetCompilerStats_RefreshDependentBlueprintsInReinstancer, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Recreate UberGraphPersistentFrame"), EKismetCompilerStats_RecreateUberGraphPersistentFrame, STATGROUP_KismetCompiler);

struct FReplaceReferenceHelper
{
	static void IncludeCDO(UClass* OldClass, UClass* NewClass, TMap<UObject*, UObject*> &OldToNewInstanceMap, TArray<UObject*> &SourceObjects, UObject* OriginalCDO)
	{
		UObject* OldCDO = OldClass->GetDefaultObject();
		UObject* NewCDO = NewClass->GetDefaultObject();

		// Add the old->new CDO mapping into the fixup map
		OldToNewInstanceMap.Add(OldCDO, NewCDO);
		// Add in the old CDO to this pass, so CDO references are fixed up
		SourceObjects.Add(OldCDO);

		if (OriginalCDO)
		{
			OldToNewInstanceMap.Add(OriginalCDO, NewCDO);
			SourceObjects.Add(OriginalCDO);
		}
	}

	static void IncludeClass(UClass* OldClass, UClass* NewClass, TMap<UObject*, UObject*> &OldToNewInstanceMap, TArray<UObject*> &SourceObjects, TArray<UObject*> &ObjectsToReplace)
	{
		OldToNewInstanceMap.Add(OldClass, NewClass);
		SourceObjects.Add(OldClass);

		if (UObject* OldCDO = OldClass->GetDefaultObject(false))
		{
			ObjectsToReplace.Add(OldCDO);
		}
	}

	static void FindAndReplaceReferences(const TArray<UObject*>& SourceObjects, TSet<UObject*>* ObjectsThatShouldUseOldStuff, const TArray<UObject*>& ObjectsToReplace, const TMap<UObject*, UObject*>& OldToNewInstanceMap, const TMap<FSoftObjectPath, UObject*>& ReinstancedObjectsWeakReferenceMap)
	{
		if(SourceObjects.Num() == 0 && ObjectsToReplace.Num() == 0 )
		{
			return;
		}

		// Find everything that references these objects
		TArray<UObject *> Targets;
		{
			BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_FindReferencers);

			Targets = FArchiveHasReferences::GetAllReferencers(SourceObjects, ObjectsThatShouldUseOldStuff);
		}

		{
			BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_ReplaceReferences);

			for (UObject* Obj : Targets)
			{
				if (!ObjectsToReplace.Contains(Obj)) // Don't bother trying to fix old objects, this would break them
				{
					// The class for finding and replacing weak references.
					// We can't relay on "standard" weak references replacement as
					// it depends on FSoftObjectPath::ResolveObject, which
					// tries to find the object with the stored path. It is
					// impossible, cause above we deleted old actors (after
					// spawning new ones), so during objects traverse we have to
					// find FSoftObjectPath with the raw given path taken
					// before deletion of old actors and fix them.
					class ReferenceReplace : public FArchiveReplaceObjectRef < UObject >
					{
					public:
						ReferenceReplace(UObject* InSearchObject, const TMap<UObject*, UObject*>& InReplacementMap, const TMap<FSoftObjectPath, UObject*>& InWeakReferencesMap)
							: FArchiveReplaceObjectRef<UObject>(InSearchObject, InReplacementMap, false, false, false, true), WeakReferencesMap(InWeakReferencesMap)
						{
							SerializeSearchObject();
						}

						FArchive& operator<<(FSoftObjectPath& Ref) override
						{
							const UObject*const* PtrToObjPtr = WeakReferencesMap.Find(Ref);

							if (PtrToObjPtr != nullptr)
							{
								Ref = *PtrToObjPtr;
							}

							return *this;
						}

						FArchive& operator<<(FSoftObjectPtr& Ref) override
						{
							return operator<<(Ref.GetUniqueID());
						}

					private:
						const TMap<FSoftObjectPath, UObject*>& WeakReferencesMap;
					};

					ReferenceReplace ReplaceAr(Obj, OldToNewInstanceMap, ReinstancedObjectsWeakReferenceMap);
				}
			}
		}
	}
};

struct FArchetypeReinstanceHelper
{
	/** Returns the full set of archetypes rooted at a single archetype object, with additional object flags (optional) */
	static void GetArchetypeObjects(UObject* InObject, TArray<UObject*>& OutArchetypeObjects, EObjectFlags SubArchetypeFlags = RF_NoFlags)
	{
		OutArchetypeObjects.Empty();

		if (InObject != nullptr && InObject->HasAllFlags(RF_ArchetypeObject))
		{
			OutArchetypeObjects.Add(InObject);

			TArray<UObject*> ArchetypeInstances;
			InObject->GetArchetypeInstances(ArchetypeInstances);

			for (int32 Idx = 0; Idx < ArchetypeInstances.Num(); ++Idx)
			{
				UObject* ArchetypeInstance = ArchetypeInstances[Idx];
				if (ArchetypeInstance != nullptr && !ArchetypeInstance->IsPendingKill() && ArchetypeInstance->HasAllFlags(RF_ArchetypeObject | SubArchetypeFlags))
				{
					OutArchetypeObjects.Add(ArchetypeInstance);

					TArray<UObject*> SubArchetypeInstances;
					ArchetypeInstance->GetArchetypeInstances(SubArchetypeInstances);

					if (SubArchetypeInstances.Num() > 0)
					{
						ArchetypeInstances.Append(SubArchetypeInstances);
					}
				}
			}
		}
	}

	/** Returns an object name that's found to be unique within the given set of archetype objects */
	static FName FindUniqueArchetypeObjectName(TArray<UObject*>& InArchetypeObjects)
	{
		FName OutName = NAME_None;

		if (InArchetypeObjects.Num() > 0)
		{
			while (OutName == NAME_None)
			{
				UObject* ArchetypeObject = InArchetypeObjects[0];
				OutName = MakeUniqueObjectName(ArchetypeObject->GetOuter(), ArchetypeObject->GetClass());
				for (int32 ObjIdx = 1; ObjIdx < InArchetypeObjects.Num(); ++ObjIdx)
				{
					ArchetypeObject = InArchetypeObjects[ObjIdx];
					if (StaticFindObjectFast(ArchetypeObject->GetClass(), ArchetypeObject->GetOuter(), OutName))
					{
						OutName = NAME_None;
						break;
					}
				}
			}
		}

		return OutName;
	}
};

/////////////////////////////////////////////////////////////////////////////////
// FBlueprintCompileReinstancer

TSet<TWeakObjectPtr<UBlueprint>> FBlueprintCompileReinstancer::DependentBlueprintsToRefresh = TSet<TWeakObjectPtr<UBlueprint>>();
TSet<TWeakObjectPtr<UBlueprint>> FBlueprintCompileReinstancer::DependentBlueprintsToRecompile = TSet<TWeakObjectPtr<UBlueprint>>();
TSet<TWeakObjectPtr<UBlueprint>> FBlueprintCompileReinstancer::DependentBlueprintsToByteRecompile = TSet<TWeakObjectPtr<UBlueprint>>();
TSet<TWeakObjectPtr<UBlueprint>> FBlueprintCompileReinstancer::CompiledBlueprintsToSave = TSet<TWeakObjectPtr<UBlueprint>>();

UClass* FBlueprintCompileReinstancer::HotReloadedOldClass = nullptr;
UClass* FBlueprintCompileReinstancer::HotReloadedNewClass = nullptr;

FBlueprintCompileReinstancer::FBlueprintCompileReinstancer(UClass* InClassToReinstance, EBlueprintCompileReinstancerFlags Flags)
	: ClassToReinstance(InClassToReinstance)
	, DuplicatedClass(nullptr)
	, OriginalCDO(nullptr)
	, bHasReinstanced(false)
	, ReinstClassType(RCT_Unknown)
	, ClassToReinstanceDefaultValuesCRC(0)
	, bIsRootReinstancer(false)
	, bAllowResaveAtTheEndIfRequested(false)
{
	if( InClassToReinstance != nullptr )
	{
		bool bAutoInferSaveOnCompile = !!(Flags & EBlueprintCompileReinstancerFlags::AutoInferSaveOnCompile);
		bool bIsBytecodeOnly = !!(Flags & EBlueprintCompileReinstancerFlags::BytecodeOnly);
		bool bAvoidCDODuplication = !!(Flags & EBlueprintCompileReinstancerFlags::AvoidCDODuplication);

		if (FKismetEditorUtilities::IsClassABlueprintSkeleton(ClassToReinstance))
		{
			ReinstClassType = RCT_BpSkeleton;
		}
		else if (ClassToReinstance->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
		{
			ReinstClassType = RCT_BpGenerated;
		}
		else if (ClassToReinstance->HasAnyClassFlags(CLASS_Native))
		{
			ReinstClassType = RCT_Native;
		}
		bAllowResaveAtTheEndIfRequested = bAutoInferSaveOnCompile && !bIsBytecodeOnly && (ReinstClassType != RCT_BpSkeleton);

		SaveClassFieldMapping(InClassToReinstance);

		// Remember the initial CDO for the class being resinstanced
		OriginalCDO = ClassToReinstance->GetDefaultObject();

		DuplicatedClass = MoveCDOToNewClass(ClassToReinstance, TMap<UClass*, UClass*>(), bAvoidCDODuplication);

		if(!bAvoidCDODuplication)
		{
			ensure( ClassToReinstance->ClassDefaultObject->GetClass() == DuplicatedClass );
			ClassToReinstance->ClassDefaultObject->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
		}
		
		// Note that we can't clear ClassToReinstance->ClassDefaultObject even though
		// we have moved it aside CleanAndSanitizeClass will want to grab the old CDO 
		// so it can propagate values to the new one note that until that happens we are 
		// in an extraordinary state: this class has a CDO of a different type

		ObjectsThatShouldUseOldStuff.Add(DuplicatedClass); //CDO of REINST_ class can be used as archetype

		if( !bIsBytecodeOnly )
		{
			TArray<UObject*> ObjectsToChange;
			const bool bIncludeDerivedClasses = false;
			GetObjectsOfClass(ClassToReinstance, ObjectsToChange, bIncludeDerivedClasses);
			for (UObject* ObjectToChange : ObjectsToChange)
			{
				ObjectToChange->SetClass(DuplicatedClass);
			}

			TArray<UClass*> ChildrenOfClass;
			GetDerivedClasses(ClassToReinstance, ChildrenOfClass);
			for ( UClass* ChildClass : ChildrenOfClass )
			{
				UBlueprint* ChildBP = Cast<UBlueprint>(ChildClass->ClassGeneratedBy);
				if (ChildBP)
				{
					const bool bClassIsDirectlyGeneratedByTheBlueprint = (ChildBP->GeneratedClass == ChildClass)
						|| (ChildBP->SkeletonGeneratedClass == ChildClass);

					if (ChildBP->HasAnyFlags(RF_BeingRegenerated) || !bClassIsDirectlyGeneratedByTheBlueprint)
					{
						if (ChildClass->GetSuperClass() == ClassToReinstance)
						{
							ReparentChild(ChildClass);
						}

						//TODO: some stronger condition would be nice
						if (!bClassIsDirectlyGeneratedByTheBlueprint)
						{
							ObjectsThatShouldUseOldStuff.Add(ChildClass);
						}
					}
					// If this is a direct child, change the parent and relink so the property chain is valid for reinstancing
					else if( !ChildBP->HasAnyFlags(RF_NeedLoad) )
					{
						if( ChildClass->GetSuperClass() == ClassToReinstance )
						{
							ReparentChild(ChildBP);
						}

						Children.AddUnique(ChildBP);
					}
					else
					{
						// If this is a child that caused the load of their parent, relink to the REINST class so that we can still serialize in the CDO, but do not add to later processing
						ReparentChild(ChildClass);
					}
				}
			}
		}

		// Pull the blueprint that generated this reinstance target, and gather the blueprints that are dependent on it
		UBlueprint* GeneratingBP = Cast<UBlueprint>(ClassToReinstance->ClassGeneratedBy);
		check(GeneratingBP || GIsAutomationTesting);
		if(!IsReinstancingSkeleton() && GeneratingBP)
		{
			ClassToReinstanceDefaultValuesCRC = GeneratingBP->CrcLastCompiledCDO;
			Dependencies.Empty();
			FBlueprintEditorUtils::GetDependentBlueprints(GeneratingBP, Dependencies);

			// Never queue for saving when regenerating on load
			if (!GeneratingBP->bIsRegeneratingOnLoad && !IsReinstancingSkeleton())
			{
				bool const bIsLevelPackage = (UWorld::FindWorldInPackage(GeneratingBP->GetOutermost()) != nullptr);
				// we don't want to save the entire level (especially if this 
				// compile was already kicked off as a result of a level save, as it
				// could cause a recursive save)... let the "SaveOnCompile" setting 
				// only save blueprint assets
				if (!bIsLevelPackage)
				{
					CompiledBlueprintsToSave.Add(GeneratingBP);
				}
			}
		}
	}
}

void FBlueprintCompileReinstancer::SaveClassFieldMapping(UClass* InClassToReinstance)
{
	check(InClassToReinstance);

	for (UProperty* Prop = InClassToReinstance->PropertyLink; Prop && (Prop->GetOuter() == InClassToReinstance); Prop = Prop->PropertyLinkNext)
	{
		PropertyMap.Add(Prop->GetFName(), Prop);
	}

	for (UFunction* Function : TFieldRange<UFunction>(InClassToReinstance, EFieldIteratorFlags::ExcludeSuper))
	{
		FunctionMap.Add(Function->GetFName(),Function);
	}
}

void FBlueprintCompileReinstancer::GenerateFieldMappings(TMap<UObject*, UObject*>& FieldMapping)
{
	check(ClassToReinstance);

	FieldMapping.Empty();

	for (TPair<FName, UProperty*>& Prop : PropertyMap)
	{
		FieldMapping.Add(Prop.Value, FindField<UProperty>(ClassToReinstance, *Prop.Key.ToString()));
	}

	for (TPair<FName, UFunction*>& Func : FunctionMap)
	{
		UFunction* NewFunction = ClassToReinstance->FindFunctionByName(Func.Key, EIncludeSuperFlag::ExcludeSuper);
		FieldMapping.Add(Func.Value, NewFunction);
	}

	UObject* NewCDO = ClassToReinstance->GetDefaultObject();
	FieldMapping.Add(OriginalCDO, NewCDO);
}

void FBlueprintCompileReinstancer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AllowEliminatingReferences(false);
	Collector.AddReferencedObject(OriginalCDO);
	Collector.AddReferencedObject(DuplicatedClass);
	Collector.AllowEliminatingReferences(true);

	// it's ok for these to get GC'd, but it is not ok for the memory to be reused (after a GC), 
	// for that reason we cannot allow these to be freed during the life of this reinstancer
	// 
	// for example, we saw this as a problem in UpdateBytecodeReferences() - if the GC'd function 
	// memory was used for a new (unrelated) function, then we were replacing references to the 
	// new function (bad), as well as any old stale references (both were using the same memory address)
	Collector.AddReferencedObjects(FunctionMap);
	Collector.AddReferencedObjects(PropertyMap);
}

void FBlueprintCompileReinstancer::OptionallyRefreshNodes(UBlueprint* CurrentBP)
{
	if (HotReloadedNewClass)
	{
		UPackage* const Package = CurrentBP->GetOutermost();
		const bool bStartedWithUnsavedChanges = Package != nullptr ? Package->IsDirty() : true;

		FBlueprintEditorUtils::RefreshExternalBlueprintDependencyNodes(CurrentBP, HotReloadedNewClass);

		if (Package != nullptr && Package->IsDirty() && !bStartedWithUnsavedChanges)
		{
			Package->SetDirtyFlag(false);
		}
	}
}

FBlueprintCompileReinstancer::~FBlueprintCompileReinstancer()
{
	if (bIsRootReinstancer && bAllowResaveAtTheEndIfRequested)
	{
		if (CompiledBlueprintsToSave.Num() > 0)
		{
			if ( !IsRunningCommandlet() && !GIsAutomationTesting )
			{
				TArray<UPackage*> PackagesToSave;
				for (TWeakObjectPtr<UBlueprint> BPPtr : CompiledBlueprintsToSave)
				{
					if (BPPtr.IsValid())
					{
						UBlueprint* BP = BPPtr.Get();

						UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
						const bool bShouldSaveOnCompile = ((Settings->SaveOnCompile == SoC_Always) || ((Settings->SaveOnCompile == SoC_SuccessOnly) && (BP->Status == BS_UpToDate)));

						if (bShouldSaveOnCompile)
						{
							PackagesToSave.Add(BP->GetOutermost());
						}
					}
				}

				FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty =*/true, /*bPromptToSave =*/false);
			}
			CompiledBlueprintsToSave.Empty();		
		}
	}
}

class FReinstanceFinalizer : public TSharedFromThis<FReinstanceFinalizer>
{
public:
	TSharedPtr<FBlueprintCompileReinstancer> Reinstancer;
	TArray<UObject*> ObjectsToReplace;
	TArray<UObject*> ObjectsToFinalize;
	TSet<UObject*> SelectedObjecs;
	UClass* ClassToReinstance;

	FReinstanceFinalizer(UClass* InClassToReinstance) : ClassToReinstance(InClassToReinstance)
	{
		check(ClassToReinstance);
	}

	void Finalize()
	{
		if (!ensure(Reinstancer.IsValid()))
		{
			return;
		}
		check(ClassToReinstance);

		const bool bIsActor = ClassToReinstance->IsChildOf<AActor>();
		if (bIsActor)
		{
			for (UObject* Obj : ObjectsToFinalize)
			{
				AActor* Actor = CastChecked<AActor>(Obj);

				UWorld* World = Actor->GetWorld();
				if (World)
				{
					// Remove any pending latent actions, as the compiled script code may have changed, and thus the
					// cached LinkInfo data may now be invalid. This could happen in the fast path, since the original
					// Actor instance will not be replaced in that case, and thus might still have latent actions pending.
					World->GetLatentActionManager().RemoveActionsForObject(Actor);

					// Drop any references to anim script components for skeletal mesh components, depending on how
					// the blueprints have changed during compile this could contain invalid data so we need to do
					// a full initialisation to ensure everything is set up correctly.
					TInlineComponentArray<USkeletalMeshComponent*> SkelComponents(Actor);
					for(USkeletalMeshComponent* SkelComponent : SkelComponents)
					{
						SkelComponent->AnimScriptInstance = nullptr;
					}

					Actor->ReregisterAllComponents();
					Actor->RerunConstructionScripts();

					if (SelectedObjecs.Contains(Obj) && GEditor)
					{
						GEditor->SelectActor(Actor, /*bInSelected =*/true, /*bNotify =*/true, false, true);
					}
				}
			}
		}

		const bool bIsAnimInstance = ClassToReinstance->IsChildOf<UAnimInstance>();
		//UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(ClassToReinstance);
		if(bIsAnimInstance)
		{
			for (UObject* Obj : ObjectsToFinalize)
			{
				if(USkeletalMeshComponent* SkelComponent = Cast<USkeletalMeshComponent>(Obj->GetOuter()))
				{
					// This snippet catches all of the exposed value handlers that will have invalid UFunctions
					// and clears the init flag so they will be reinitialized on the next call to InitAnim.
					// Unknown whether there are other unreachable properties so currently clearing the anim
					// instance below
					// #TODO investigate reinstancing anim blueprints to correctly catch all deep references

					//UAnimInstance* ActiveInstance = SkelComponent->GetAnimInstance();
					//if(AnimClass && ActiveInstance)
					//{
					//	for(UStructProperty* NodeProp : AnimClass->AnimNodeProperties)
					//	{
					//		// Guaranteed to have only FAnimNode_Base pointers added during compilation
					//		FAnimNode_Base* AnimNode = NodeProp->ContainerPtrToValuePtr<FAnimNode_Base>(ActiveInstance);
					//
					//		AnimNode->EvaluateGraphExposedInputs.bInitialized = false;
					//	}
					//}

					// Clear out the script instance on the component to force a rebuild during initialization.
					// This is necessary to correctly reinitialize certain properties that still reference the 
					// old class as they are unreachable during reinstancing.
					SkelComponent->AnimScriptInstance = nullptr;
					SkelComponent->InitAnim(true);
				}
			}
		}

		Reinstancer->FinalizeFastReinstancing(ObjectsToReplace);
	}
};

TSharedPtr<FReinstanceFinalizer> FBlueprintCompileReinstancer::ReinstanceFast()
{
	UE_LOG(LogBlueprint, Log, TEXT("BlueprintCompileReinstancer: Doing a fast path refresh on class '%s'."), *GetPathNameSafe(ClassToReinstance));

	TSharedPtr<FReinstanceFinalizer> Finalizer = MakeShareable(new FReinstanceFinalizer(ClassToReinstance));
	Finalizer->Reinstancer = SharedThis(this);

	GetObjectsOfClass(DuplicatedClass, Finalizer->ObjectsToReplace, /*bIncludeDerivedClasses=*/ false);

	const bool bIsActor = ClassToReinstance->IsChildOf<AActor>();
	const bool bIsComponent = ClassToReinstance->IsChildOf<UActorComponent>();
	for (UObject* Obj : Finalizer->ObjectsToReplace)
	{
		UE_LOG(LogBlueprint, Log, TEXT("  Fast path is refreshing (not replacing) %s"), *Obj->GetFullName());

		const bool bIsChildActorTemplate = (bIsActor ? CastChecked<AActor>(Obj)->GetOuter()->IsA<UChildActorComponent>() : false);
		if ((!Obj->IsTemplate() || bIsComponent || bIsChildActorTemplate) && !Obj->IsPendingKill())
		{
			if (bIsActor && Obj->IsSelected())
			{
				Finalizer->SelectedObjecs.Add(Obj);
			}

			Obj->SetClass(ClassToReinstance);

			Finalizer->ObjectsToFinalize.Push(Obj);
		}
	}

	return Finalizer;
}

void FBlueprintCompileReinstancer::FinalizeFastReinstancing(TArray<UObject*>& ObjectsToReplace)
{
	TArray<UObject*> SourceObjects;
	TMap<UObject*, UObject*> OldToNewInstanceMap;
	TMap<FSoftObjectPath, UObject*> ReinstancedObjectsWeakReferenceMap;
	FReplaceReferenceHelper::IncludeCDO(DuplicatedClass, ClassToReinstance, OldToNewInstanceMap, SourceObjects, OriginalCDO);

	if (IsClassObjectReplaced())
	{
		FReplaceReferenceHelper::IncludeClass(DuplicatedClass, ClassToReinstance, OldToNewInstanceMap, SourceObjects, ObjectsToReplace);
	}

	FReplaceReferenceHelper::FindAndReplaceReferences(SourceObjects, &ObjectsThatShouldUseOldStuff, ObjectsToReplace, OldToNewInstanceMap, ReinstancedObjectsWeakReferenceMap);

	if (ClassToReinstance->IsChildOf<UActorComponent>())
	{
		// ReplaceInstancesOfClass() handles this itself, if we had to re-instance
		ReconstructOwnerInstances(ClassToReinstance);
	}
}

void FBlueprintCompileReinstancer::CompileChildren()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_RecompileChildClasses);

	// Reparent all dependent blueprints, and recompile to ensure that they get reinstanced with the new memory layout
	for (UBlueprint* BP : Children)
	{
		if (BP->ParentClass == ClassToReinstance || BP->ParentClass == DuplicatedClass)
		{
			ReparentChild(BP);

			// avoid the skeleton compile if we don't need it - if the class 
			// we're reinstancing is a Blueprint class, then we assume sub-class
			// skeletons were kept in-sync (updated/reinstanced when the parent 
			// was updated); however, if this is a native class (like when hot-
			// reloading), then we want to make sure to update the skel as well
			EBlueprintCompileOptions Options = EBlueprintCompileOptions::SkipGarbageCollection;
			if(!ClassToReinstance->HasAnyClassFlags(CLASS_Native))
			{
				Options |= EBlueprintCompileOptions::SkeletonUpToDate;
			}
			FKismetEditorUtilities::CompileBlueprint(BP, Options);
		}
		else if (IsReinstancingSkeleton())
		{
			const bool bForceRegeneration = true;
			FKismetEditorUtilities::GenerateBlueprintSkeleton(BP, bForceRegeneration);
		}
	}
}

TSharedPtr<FReinstanceFinalizer> FBlueprintCompileReinstancer::ReinstanceInner(bool bForceAlwaysReinstance)
{
	TSharedPtr<FReinstanceFinalizer> Finalizer;
	if (ClassToReinstance && DuplicatedClass)
	{
		static const FBoolConfigValueHelper ReinstanceOnlyWhenNecessary(TEXT("Kismet"), TEXT("bReinstanceOnlyWhenNecessary"), GEngineIni);
		bool bShouldReinstance = true;
		// See if we need to do a full reinstance or can do the faster refresh path (when enabled or no values were modified, and the structures match)
		if (ReinstanceOnlyWhenNecessary && !bForceAlwaysReinstance)
		{
			BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_ReplaceClassNoReinsancing);

			const UBlueprintGeneratedClass* BPClassA = Cast<const UBlueprintGeneratedClass>(DuplicatedClass);
			const UBlueprintGeneratedClass* BPClassB = Cast<const UBlueprintGeneratedClass>(ClassToReinstance);
			const UBlueprint* BP = Cast<const UBlueprint>(ClassToReinstance->ClassGeneratedBy);

			const bool bTheSameDefaultValues = (BP != nullptr) && (ClassToReinstanceDefaultValuesCRC != 0) && (BP->CrcLastCompiledCDO == ClassToReinstanceDefaultValuesCRC);
			const bool bTheSameLayout = (BPClassA != nullptr) && (BPClassB != nullptr) && FStructUtils::TheSameLayout(BPClassA, BPClassB, true);
			const bool bAllowedToDoFastPath = bTheSameDefaultValues && bTheSameLayout;
			if (bAllowedToDoFastPath)
			{
				Finalizer = ReinstanceFast();
				bShouldReinstance = false;
			}
		}

		if (bShouldReinstance)
		{
			UE_LOG(LogBlueprint, Log, TEXT("BlueprintCompileReinstancer: Doing a full reinstance on class '%s'"), *GetPathNameSafe(ClassToReinstance));
			ReplaceInstancesOfClass(DuplicatedClass, ClassToReinstance, OriginalCDO, &ObjectsThatShouldUseOldStuff, IsClassObjectReplaced(), ShouldPreserveRootComponentOfReinstancedActor());
		}
	}
	return Finalizer;
}

void FBlueprintCompileReinstancer::ListDependentBlueprintsToRefresh(const TArray<UBlueprint*>& DependentBPs)
{
	for (UBlueprint* Element : DependentBPs)
	{
		DependentBlueprintsToRefresh.Add(Element);
	}
}

void FBlueprintCompileReinstancer::EnlistDependentBlueprintToRecompile(UBlueprint* BP, bool bBytecodeOnly)
{
	if (IsValid(BP))
	{
		if (bBytecodeOnly)
		{
			DependentBlueprintsToByteRecompile.Add(BP);
		}
		else
		{
			DependentBlueprintsToRecompile.Add(BP);
		}
	}
}

void FBlueprintCompileReinstancer::BlueprintWasRecompiled(UBlueprint* BP, bool bBytecodeOnly)
{
	if (IsValid(BP))
	{
		DependentBlueprintsToRefresh.Remove(BP);
		DependentBlueprintsToByteRecompile.Remove(BP);
		if (!bBytecodeOnly)
		{
			DependentBlueprintsToRecompile.Remove(BP);
		}
	}
}

extern UNREALED_API FSecondsCounterData BlueprintCompileAndLoadTimerData;

void FBlueprintCompileReinstancer::ReinstanceObjects(bool bForceAlwaysReinstance)
{
	FSecondsCounterScope Timer(BlueprintCompileAndLoadTimerData);

	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ReinstanceObjects);
	
	// Make sure we only reinstance classes once!
	static TArray<TSharedRef<FBlueprintCompileReinstancer>> QueueToReinstance;

	if (!bHasReinstanced)
	{
		TSharedRef<FBlueprintCompileReinstancer> SharedThis = AsShared();
		bool bAlreadyQueued = QueueToReinstance.Contains(SharedThis);

		// We may already be reinstancing this class, this happens when a dependent blueprint has a compile error and we try to reinstance the stub:
		if (!bAlreadyQueued)
		{
			for (const TSharedRef<FBlueprintCompileReinstancer>& Entry : QueueToReinstance)
			{
				if (Entry->ClassToReinstance == SharedThis->ClassToReinstance)
				{
					bAlreadyQueued = true;
					break;
				}
			}
		}

		if (!bAlreadyQueued)
		{
			QueueToReinstance.Push(SharedThis);

			if (ClassToReinstance && DuplicatedClass)
			{
				CompileChildren();
			}

			if (QueueToReinstance.Num() && (QueueToReinstance[0] == SharedThis))
			{
				// Mark it as the source reinstancer, no other reinstancer can get here until this Blueprint finishes compiling
				bIsRootReinstancer = true;

				TSet<TWeakObjectPtr<UBlueprint>> CompiledBlueprints;
				// Blueprints will enqueue dirty and erroring dependents, in case those states would be 
				// fixed up by having this dependency compiled first. However, this can result in an 
				// infinite loop where two Blueprints with errors (unrelated to each other) keep 
				// perpetually queuing the other. 
				//
				// To guard against this, we track the recompiled dependents (in order) and break the 
				// cycle when we see that we've already compiled a dependent after its dependency
				TArray<UBlueprint*> OrderedRecompiledDependents;

				TSet<TWeakObjectPtr<UBlueprint>> RecompilationQueue = DependentBlueprintsToRecompile;
				// empty the public facing queue so we can discern between old and new elements (added 
				// as the result of subsequent recompiles) 
				DependentBlueprintsToRecompile.Empty();

				while (RecompilationQueue.Num())
				{
					auto Iter = RecompilationQueue.CreateIterator();
					TWeakObjectPtr<UBlueprint> BPPtr = *Iter;
					Iter.RemoveCurrent();
					if (UBlueprint* BP = BPPtr.Get())
					{
						if (IsReinstancingSkeleton())
						{
							const bool bForceRegeneration = true;
							FKismetEditorUtilities::GenerateBlueprintSkeleton(BP, bForceRegeneration);
						}
						else
						{
							// it's unsafe to GC in the middle of reinstancing because there may be other reinstancers still alive with references to 
							// otherwise unreferenced classes:
							// Full compiles first recompile all skeleton classes, so they are up-to-date
							FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection | EBlueprintCompileOptions::SkeletonUpToDate);
							CompiledBlueprints.Add(BP);
						}

						OrderedRecompiledDependents.Add(BP);

						// if this BP compiled with an error, then I don't see any reason why we 
						// should attempt to recompile its dependencies; if a subsequent recompile 
						// would fix this up, then it'll get re-injected into the queue when that happens
						if (BP->Status != EBlueprintStatus::BS_Error)
						{
							for (TWeakObjectPtr<UBlueprint>& DependentPtr : DependentBlueprintsToRecompile)
							{
								if (!DependentPtr.IsValid())
								{
									continue;
								}
								UBlueprint* NewDependent = DependentPtr.Get();

								int32 DependentIndex = OrderedRecompiledDependents.FindLast(NewDependent);
								if (DependentIndex != INDEX_NONE)
								{
									// even though we just pushed BP into the list and know that it
									// exists as the last entry, we want to see if it was compiled 
									// earlier (once before 'NewDependent'); so we use Find() to search 
									// out the first entry
									int32 RecompilingBpIndex = OrderedRecompiledDependents.Find(BP);
									if (RecompilingBpIndex != INDEX_NONE && RecompilingBpIndex < DependentIndex)
									{
										// we've already recompiled this Blueprint once before (here in 
										// this loop), already after its dependency has been compiled too;
										// so, to avoid a potential infinite loop we cannot keep trying 
										// to compile this
										//
										// NOTE: this may result in some a compiler error that would have 
										//       been resolved in another subsequent compile (for example: 
										//       B depends on A, A is compiled, A has an error, B compiles 
										//       with an error as a result, C compiles and enqueues A as a 
										//       dependent, A is recompiled without error now, B is not 
										//       enqueued again because its already recompiled after A)
										// 
										// the true fix is to restructure the compiler so that these sort 
										// of scenarios don't happen - until then, this is a fair trade 
										// off... fallback to a byte code compile instead
										DependentBlueprintsToByteRecompile.Add(DependentPtr);
										continue;
									}
								}
								RecompilationQueue.Add(DependentPtr);
							}
						}
						DependentBlueprintsToRecompile.Empty();
					}
				}

				TArray<UBlueprint*> OrderedBytecodeRecompile;

				while (DependentBlueprintsToByteRecompile.Num())
				{
					auto Iter = DependentBlueprintsToByteRecompile.CreateIterator();
					if (UBlueprint* BP = Iter->Get())
					{
						OrderedBytecodeRecompile.Add(BP);
					}
					Iter.RemoveCurrent();
				}

				// Make sure we compile classes that are deeper in the class hierarchy later
				// than ones that are higher:
				OrderedBytecodeRecompile.Sort(
					[](const UBlueprint& LHS, const UBlueprint& RHS)
				{
					int32 LHS_Depth = 0;
					int32 RHS_Depth = 0;

					UStruct* Iter = LHS.ParentClass;
					while (Iter)
					{
						LHS_Depth += 1;
						Iter = Iter->GetSuperStruct();
					}

					Iter = RHS.ParentClass;
					while (Iter)
					{
						RHS_Depth += 1;
						Iter = Iter->GetSuperStruct();
					}

					// use name as tie breaker, just so we're stable
					// across editor sessions:
					return LHS_Depth != RHS_Depth ? (LHS_Depth < RHS_Depth) : LHS.GetName() < RHS.GetName();
				}
				);

				DependentBlueprintsToByteRecompile.Empty();

				for (int I = 0; I != OrderedBytecodeRecompile.Num(); ++I)
				{
					UBlueprint* BP = OrderedBytecodeRecompile[I];
					FKismetEditorUtilities::RecompileBlueprintBytecode(BP, nullptr, EBlueprintBytecodeRecompileOptions::BatchCompile);
					ensure(0 == DependentBlueprintsToRecompile.Num());
					CompiledBlueprints.Add(BP);

					// We won't re-instance (and thus won't finalize) objects of this type, but we still need to know if the current
					// Actor selection in the level editor includes instances of this type, and force a refresh on the selection if so.
					// Note: If this is a Component BP instead of an Actor BP, the refresh is handled by IWCE's OnCompiled() BP delegate.
					if (GEditor && BP->ParentClass && BP->ParentClass->IsChildOf<AActor>())
					{
						for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
						{
							AActor* ActorInstance = CastChecked<AActor>(*It);
							if (ActorInstance->IsA(BP->GeneratedClass))
							{
								GEditor->SelectActor(ActorInstance, /*bInSelected =*/true, /*bNotify =*/true, false, true);
							}
						}
					}
				}


				if (!IsReinstancingSkeleton())
				{
					TGuardValue<bool> ReinstancingGuard(GIsReinstancing, true);

					TArray<TSharedPtr<FReinstanceFinalizer>> Finalizers;

					// All children were recompiled. It's safe to reinstance.
					for (int32 Idx = 0; Idx < QueueToReinstance.Num(); ++Idx)
					{
						TSharedPtr<FReinstanceFinalizer> Finalizer = QueueToReinstance[Idx]->ReinstanceInner(bForceAlwaysReinstance);
						if (Finalizer.IsValid())
						{
							Finalizers.Push(Finalizer);
						}
						QueueToReinstance[Idx]->bHasReinstanced = true;
					}
					QueueToReinstance.Empty();

					for (TSharedPtr<FReinstanceFinalizer>& Finalizer : Finalizers)
					{
						if (Finalizer.IsValid())
						{
							Finalizer->Finalize();
						}
					}

					for (TWeakObjectPtr<UBlueprint>& CompiledBP : CompiledBlueprints)
					{
						CompiledBP->BroadcastCompiled();
					}

					{
						BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_RefreshDependentBlueprintsInReinstancer);
						for (TWeakObjectPtr<UBlueprint>& BPPtr : DependentBlueprintsToRefresh)
						{
							if (BPPtr.IsValid())
							{
								BPPtr->BroadcastChanged();
							}
						}
						DependentBlueprintsToRefresh.Empty();
					}

					if (GEditor)
					{
						GEditor->BroadcastBlueprintCompiled();
					}
				}
				else
				{
					QueueToReinstance.Empty();
					DependentBlueprintsToRefresh.Empty();
				}
			}
		}
	}
}



void FBlueprintCompileReinstancer::UpdateBytecodeReferences()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_UpdateBytecodeReferences);

	if(ClassToReinstance != nullptr)
	{
		TMap<UObject*, UObject*> FieldMappings;
		GenerateFieldMappings(FieldMappings);

		// Determine whether or not we will be updating references for an Animation Blueprint class.
		const bool bIsAnimBlueprintClass = !!Cast<UAnimBlueprint>(ClassToReinstance->ClassGeneratedBy);

		for( auto DependentBP = Dependencies.CreateIterator(); DependentBP; ++DependentBP )
		{
			UClass* BPClass = (*DependentBP)->GeneratedClass;

			// Skip cases where the class is junk, or haven't finished serializing in yet
			// Note that BPClass can be null for blueprints that can no longer be compiled:
			if (!BPClass
				|| (BPClass == ClassToReinstance)
				|| (BPClass->GetOutermost() == GetTransientPackage()) 
				|| BPClass->HasAnyClassFlags(CLASS_NewerVersionExists)
				|| (BPClass->ClassGeneratedBy && BPClass->ClassGeneratedBy->HasAnyFlags(RF_NeedLoad|RF_BeingRegenerated)) )
			{
				continue;
			}

			BPClass->ClearFunctionMapsCaches();

			// Ensure that Animation Blueprint child class dependencies are always re-linked, as the child may reference properties generated during
			// compilation of the parent class, which will have shifted to a TRASHCLASS Outer at this point (see UAnimBlueprintGeneratedClass::Link()).
			if(bIsAnimBlueprintClass && BPClass->IsChildOf(ClassToReinstance))
			{
				BPClass->StaticLink(true);
			}

			bool bBPWasChanged = false;
			// For each function defined in this blueprint, run through the bytecode, and update any refs from the old properties to the new
			for( TFieldIterator<UFunction> FuncIter(BPClass, EFieldIteratorFlags::ExcludeSuper); FuncIter; ++FuncIter )
			{
				UFunction* CurrentFunction = *FuncIter;
				FArchiveReplaceObjectRef<UObject> ReplaceAr(CurrentFunction, FieldMappings, /*bNullPrivateRefs=*/ false, /*bIgnoreOuterRef=*/ true, /*bIgnoreArchetypeRef=*/ true);
				bBPWasChanged |= (0 != ReplaceAr.GetCount());
			}

			FArchiveReplaceObjectRef<UObject> ReplaceInBPAr(*DependentBP, FieldMappings, false, true, true);
			if (ReplaceInBPAr.GetCount())
			{
				bBPWasChanged = true;
				UE_LOG(LogBlueprint, Log, TEXT("UpdateBytecodeReferences: %d references from %s was replaced in BP %s"), ReplaceInBPAr.GetCount(), *GetPathNameSafe(ClassToReinstance), *GetPathNameSafe(*DependentBP));
			}

			UBlueprint* CompiledBlueprint = UBlueprint::GetBlueprintFromClass(ClassToReinstance);
			if (bBPWasChanged && CompiledBlueprint && !CompiledBlueprint->bIsRegeneratingOnLoad)
			{
				DependentBlueprintsToRefresh.Add(*DependentBP);
			}
		}
	}
}

/** Lots of redundancy with ReattachActorsHelper */
struct FAttachedActorInfo
{
	FAttachedActorInfo()
		: AttachedActor(nullptr)
		, AttachedToSocket()
	{
	}

	AActor* AttachedActor;
	FName   AttachedToSocket;
};

struct FActorAttachmentData
{
	FActorAttachmentData();
	FActorAttachmentData(AActor* OldActor);
	FActorAttachmentData(const FActorAttachmentData&) = default;
	FActorAttachmentData& operator=(const FActorAttachmentData&) = default;
	FActorAttachmentData(FActorAttachmentData&&) = default;
	FActorAttachmentData& operator=(FActorAttachmentData&&) = default;
	~FActorAttachmentData() = default;

	AActor*          TargetAttachParent;
	USceneComponent* TargetParentComponent;
	FName            TargetAttachSocket;

	TArray<FAttachedActorInfo> PendingChildAttachments;
};

FActorAttachmentData::FActorAttachmentData()
	: TargetAttachParent(nullptr)
	, TargetParentComponent(nullptr)
	, TargetAttachSocket()
	, PendingChildAttachments()
{
}

FActorAttachmentData::FActorAttachmentData(AActor* OldActor)
{
	TargetAttachParent = nullptr;
	TargetParentComponent = nullptr;

	TArray<AActor*> AttachedActors;
	OldActor->GetAttachedActors(AttachedActors);

	// if there are attached objects detach them and store the socket names
	for (AActor* AttachedActor : AttachedActors)
	{
		USceneComponent* AttachedActorRoot = AttachedActor->GetRootComponent();
		if (AttachedActorRoot && AttachedActorRoot->GetAttachParent())
		{
			// Save info about actor to reattach
			FAttachedActorInfo Info;
			Info.AttachedActor = AttachedActor;
			Info.AttachedToSocket = AttachedActorRoot->GetAttachSocketName();
			PendingChildAttachments.Add(Info);
		}
	}

	if (USceneComponent* OldRootComponent = OldActor->GetRootComponent())
	{
		if (OldRootComponent->GetAttachParent() != nullptr)
		{
			TargetAttachParent = OldRootComponent->GetAttachParent()->GetOwner();
			// Root component should never be attached to another component in the same actor!
			if (TargetAttachParent == OldActor)
			{
				UE_LOG(LogBlueprint, Warning, TEXT("ReplaceInstancesOfClass: RootComponent (%s) attached to another component in this Actor (%s)."), *OldRootComponent->GetPathName(), *TargetAttachParent->GetPathName());
				TargetAttachParent = nullptr;
			}

			TargetAttachSocket = OldRootComponent->GetAttachSocketName();
			TargetParentComponent = OldRootComponent->GetAttachParent();
		}
	}
}

/** 
 * Utility struct that represents a single replacement actor. Used to cache off
 * attachment info for the old actor (the one being replaced), that will be
 * used later for the new actor (after all instances have been replaced).
 */
struct FActorReplacementHelper
{
	/** NOTE: this detaches OldActor from all child/parent attachments. */
	FActorReplacementHelper(AActor* InNewActor, AActor* OldActor, FActorAttachmentData&& InAttachmentData)
		: NewActor(InNewActor)
		, TargetWorldTransform(FTransform::Identity)
		, AttachmentData( MoveTemp(InAttachmentData) )
		, bSelectNewActor(OldActor->IsSelected())
	{
		CachedActorData = StaticCastSharedPtr<AActor::FActorTransactionAnnotation>(OldActor->GetTransactionAnnotation());
		TArray<AActor*> AttachedActors;
		OldActor->GetAttachedActors(AttachedActors);

		// if there are attached objects detach them and store the socket names
		for (AActor* AttachedActor : AttachedActors)
		{
			USceneComponent* AttachedActorRoot = AttachedActor->GetRootComponent();
			if (AttachedActorRoot && AttachedActorRoot->GetAttachParent())
			{
				AttachedActorRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}
		}

		if (USceneComponent* OldRootComponent = OldActor->GetRootComponent())
		{
			if (OldRootComponent->GetAttachParent() != nullptr)
			{
				// detach it to remove any scaling
				OldRootComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}

			// Save off transform
			TargetWorldTransform = OldRootComponent->GetComponentTransform();
			TargetWorldTransform.SetTranslation(OldRootComponent->GetComponentLocation()); // take into account any custom location
		}

		for (UActorComponent* OldActorComponent : OldActor->GetComponents())
		{
			if (OldActorComponent)
			{
				OldActorComponentNameMap.Add(OldActorComponent->GetFName(), OldActorComponent);
			}
		}
	}

	/**
	 * Runs construction scripts on the new actor and then finishes it off by
	 * attaching it to the same attachments that its predecessor was set with. 
	 */
	void Finalize(const TMap<UObject*, UObject*>& OldToNewInstanceMap, TSet<UObject*>* ObjectsThatShouldUseOldStuff, const TArray<UObject*>& ObjectsToReplace, const TMap<FSoftObjectPath, UObject*>& ReinstancedObjectsWeakReferenceMap);

	/**
	* Takes the cached child actors, as well as the old AttachParent, and sets
	* up the new actor so that its attachment hierarchy reflects the old actor
	* that it is replacing. Must be called after *all* instances have been Finalized.
	*
	* @param OldToNewInstanceMap Mapping of reinstanced objects.
	*/
	void ApplyAttachments(const TMap<UObject*, UObject*>& OldToNewInstanceMap, TSet<UObject*>* ObjectsThatShouldUseOldStuff, const TArray<UObject*>& ObjectsToReplace, const TMap<FSoftObjectPath, UObject*>& ReinstancedObjectsWeakReferenceMap);

private:
	/**
	 * Takes the cached child actors, and attaches them under the new actor.
	 *
	 * @param  RootComponent	The new actor's root, which the child actors should attach to.
	 * @param  OldToNewInstanceMap	Mapping of reinstanced objects. Used for when child and parent actor are of the same type (and thus parent may have been reinstanced, so we can't reattach to the old instance).
	 */
	void AttachChildActors(USceneComponent* RootComponent, const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	AActor*          NewActor;
	FTransform       TargetWorldTransform;
	FActorAttachmentData AttachmentData;
	bool             bSelectNewActor;

	/** Holds actor component data, etc. that we use to apply */
	TSharedPtr<AActor::FActorTransactionAnnotation> CachedActorData;

	TMap<FName, UActorComponent*> OldActorComponentNameMap;
};

void FActorReplacementHelper::Finalize(const TMap<UObject*, UObject*>& OldToNewInstanceMap, TSet<UObject*>* ObjectsThatShouldUseOldStuff, const TArray<UObject*>& ObjectsToReplace, const TMap<FSoftObjectPath, UObject*>& ReinstancedObjectsWeakReferenceMap)
{
	
	// because this is an editor context it's important to use this execution guard
	FEditorScriptExecutionGuard ScriptGuard;

	// run the construction script, which will use the properties we just copied over
	if (NewActor->CurrentTransactionAnnotation.IsValid())
	{
		NewActor->CurrentTransactionAnnotation->ComponentInstanceData.FindAndReplaceInstances(OldToNewInstanceMap);
		NewActor->RerunConstructionScripts();
	}
	else if (CachedActorData.IsValid())
	{
		CachedActorData->ComponentInstanceData.FindAndReplaceInstances(OldToNewInstanceMap);
		const bool bErrorFree = NewActor->ExecuteConstruction(TargetWorldTransform, nullptr, &CachedActorData->ComponentInstanceData);
		if (!bErrorFree)
		{
			// Save off the cached actor data for once the blueprint has been fixed so we can reapply it
			NewActor->CurrentTransactionAnnotation = CachedActorData;
		}
	}
	else
	{
		FComponentInstanceDataCache DummyComponentData;
		NewActor->ExecuteConstruction(TargetWorldTransform, nullptr, &DummyComponentData);
	}	

	// make sure that the actor is properly hidden if it's in a hidden sublevel:
	bool bIsInHiddenLevel = false;
	if (ULevel* Level = NewActor->GetLevel())
	{
		bIsInHiddenLevel = !Level->bIsVisible;
	}

	if (bIsInHiddenLevel)
	{
		NewActor->bHiddenEdLevel = true;
		NewActor->MarkComponentsRenderStateDirty();
	}

	if (bSelectNewActor && GEditor)
	{
		GEditor->SelectActor(NewActor, /*bInSelected =*/true, /*bNotify =*/true);
	}

	TMap<UObject*, UObject*> ConstructedComponentReplacementMap;
	for (UActorComponent* NewActorComponent : NewActor->GetComponents())
	{
		if (NewActorComponent)
		{
			if (UActorComponent** OldActorComponent = OldActorComponentNameMap.Find(NewActorComponent->GetFName()))
			{
				ConstructedComponentReplacementMap.Add(*OldActorComponent, NewActorComponent);
			}
		}
	}
	if (GEditor)
	{
		GEditor->NotifyToolsOfObjectReplacement(ConstructedComponentReplacementMap);
	}

	// Make array of component subobjects that have been reinstanced as part of the new Actor.
	TArray<UObject*> SourceObjects;
	ConstructedComponentReplacementMap.GenerateKeyArray(SourceObjects);

	// Find and replace any outstanding references to the old Actor's component subobject instances that exist outside of the old Actor instance.
	// Note: This will typically be references held by the Editor's transaction buffer - we need to find and replace those as well since we also do this for the old->new Actor instance.
	FReplaceReferenceHelper::FindAndReplaceReferences(SourceObjects, ObjectsThatShouldUseOldStuff, ObjectsToReplace, ConstructedComponentReplacementMap, ReinstancedObjectsWeakReferenceMap);
	
	// Destroy actor and clear references.
	NewActor->Modify();
	if (GEditor && GEditor->Layers.IsValid())
	{
		GEditor->Layers->InitializeNewActorLayers(NewActor);
	}
}

void FActorReplacementHelper::ApplyAttachments(const TMap<UObject*, UObject*>& OldToNewInstanceMap, TSet<UObject*>* ObjectsThatShouldUseOldStuff, const TArray<UObject*>& ObjectsToReplace, const TMap<FSoftObjectPath, UObject*>& ReinstancedObjectsWeakReferenceMap)
{
	USceneComponent* NewRootComponent = NewActor->GetRootComponent();
	if (NewRootComponent == nullptr)
	{
		return;
	}

	if (AttachmentData.TargetAttachParent)
	{
		UObject* const* NewTargetAttachParent = OldToNewInstanceMap.Find(AttachmentData.TargetAttachParent);
		if (NewTargetAttachParent)
		{
			AttachmentData.TargetAttachParent = CastChecked<AActor>(*NewTargetAttachParent);
		}
	}
	if (AttachmentData.TargetParentComponent)
	{
		UObject* const* NewTargetParentComponent = OldToNewInstanceMap.Find(AttachmentData.TargetParentComponent);
		if (NewTargetParentComponent && *NewTargetParentComponent)
		{
			AttachmentData.TargetParentComponent = CastChecked<USceneComponent>(*NewTargetParentComponent);
		}
	}

	// attach the new instance to original parent
	if (AttachmentData.TargetAttachParent != nullptr)
	{
		if (AttachmentData.TargetParentComponent == nullptr)
		{
			AttachmentData.TargetParentComponent = AttachmentData.TargetAttachParent->GetRootComponent();
		}
		else if(!AttachmentData.TargetParentComponent->IsPendingKill())
		{
			NewRootComponent->AttachToComponent(AttachmentData.TargetParentComponent, FAttachmentTransformRules::KeepWorldTransform, AttachmentData.TargetAttachSocket);
		}
	}

	AttachChildActors(NewRootComponent, OldToNewInstanceMap);
}

void FActorReplacementHelper::AttachChildActors(USceneComponent* RootComponent, const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	// if we had attached children reattach them now - unless they are already attached
	for (FAttachedActorInfo& Info : AttachmentData.PendingChildAttachments)
	{
		// Check for a reinstanced attachment, and redirect to the new instance if found
		AActor* NewAttachedActor = Cast<AActor>(OldToNewInstanceMap.FindRef(Info.AttachedActor));
		if (NewAttachedActor)
		{
			Info.AttachedActor = NewAttachedActor;
		}

		// If this actor is no longer attached to anything, reattach
		if (!Info.AttachedActor->IsPendingKill() && Info.AttachedActor->GetAttachParentActor() == nullptr)
		{
			USceneComponent* ChildRoot = Info.AttachedActor->GetRootComponent();
			if (ChildRoot && ChildRoot->GetAttachParent() != RootComponent)
			{
				ChildRoot->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform, Info.AttachedToSocket);
				ChildRoot->UpdateComponentToWorld();
			}
		}
	}
}

// 
namespace InstancedPropertyUtils
{
	typedef TMap<FName, UObject*> FInstancedPropertyMap;

	/** 
	 * Aids in finding instanced property values that will not be duplicated nor
	 * copied in CopyPropertiesForUnRelatedObjects().
	 */
	class FArchiveInstancedSubObjCollector : public FArchiveUObject
	{
	public:
		//----------------------------------------------------------------------
		FArchiveInstancedSubObjCollector(UObject* TargetObj, FInstancedPropertyMap& PropertyMapOut, bool bAutoSerialize = true)
			: Target(TargetObj)
			, InstancedPropertyMap(PropertyMapOut)
		{
			ArIsObjectReferenceCollector = true;
			ArIsPersistent = false;
			ArIgnoreArchetypeRef = false;

			if (bAutoSerialize)
			{
				RunSerialization();
			}
		}

		//----------------------------------------------------------------------
		FArchive& operator<<(UObject*& Obj)
		{
			if (Obj != nullptr)
			{
				UProperty* SerializingProperty = GetSerializedProperty();
				const bool bHasInstancedValue = SerializingProperty->HasAnyPropertyFlags(CPF_PersistentInstance);

				// default sub-objects are handled by CopyPropertiesForUnrelatedObjects()
				if (bHasInstancedValue && !Obj->IsDefaultSubobject())
				{
					
					UObject* ObjOuter = Obj->GetOuter();
					bool bIsSubObject = (ObjOuter == Target);
					// @TODO: handle nested sub-objects when we're more clear on 
					//        how this'll affect the makeup of the reinstanced object
// 					while (!bIsSubObject && (ObjOuter != nullptr))
// 					{
// 						ObjOuter = ObjOuter->GetOuter();
// 						bIsSubObject |= (ObjOuter == Target);
// 					}

					if (bIsSubObject)
					{
						InstancedPropertyMap.Add(SerializingProperty->GetFName(), Obj);
					}
				}
			}
			return *this;
		}

		//----------------------------------------------------------------------
		void RunSerialization()
		{
			InstancedPropertyMap.Empty();
			if (Target != nullptr)
			{
				Target->Serialize(*this);
			}
		}

	private:
		UObject* Target;
		FInstancedPropertyMap& InstancedPropertyMap;
	};

	/** 
	 * Duplicates and assigns instanced property values that may have been 
	 * missed by CopyPropertiesForUnRelatedObjects().
	 */
	class FArchiveInsertInstancedSubObjects : public FArchiveUObject
	{
	public:
		//----------------------------------------------------------------------
		FArchiveInsertInstancedSubObjects(UObject* TargetObj, const FInstancedPropertyMap& OldInstancedSubObjs, bool bAutoSerialize = true)
			: TargetCDO(TargetObj->GetClass()->GetDefaultObject())
			, Target(TargetObj)
			, OldInstancedSubObjects(OldInstancedSubObjs)
		{
			ArIsObjectReferenceCollector = true;
			ArIsModifyingWeakAndStrongReferences = true;

			if (bAutoSerialize)
			{
				RunSerialization();
			}
		}

		//----------------------------------------------------------------------
		FArchive& operator<<(UObject*& Obj)
		{
			if (Obj == nullptr)
			{
				UProperty* SerializingProperty = GetSerializedProperty();
				if (UObject* const* OldInstancedObjPtr = OldInstancedSubObjects.Find(SerializingProperty->GetFName()))
				{
					const UObject* OldInstancedObj = *OldInstancedObjPtr;
					check(SerializingProperty->HasAnyPropertyFlags(CPF_PersistentInstance));

					UClass* TargetClass = TargetCDO->GetClass();
					// @TODO: Handle nested instances when we have more time to flush this all out  
					if ( TargetClass->IsChildOf(SerializingProperty->GetOwnerClass()) )
					{
						UObjectPropertyBase* SerializingObjProperty = CastChecked<UObjectPropertyBase>(SerializingProperty);
						// being extra careful, not to create our own instanced version when we expect one from the CDO
						if (SerializingObjProperty->GetObjectPropertyValue_InContainer(TargetCDO) == nullptr)
						{
							// @TODO: What if the instanced object is of the same type 
							//        that we're currently reinstancing
							Obj = StaticDuplicateObject(OldInstancedObj, Target);// NewObject<UObject>(Target, OldInstancedObj->GetClass()->GetAuthoritativeClass(), OldInstancedObj->GetFName());
						}
					}
				}
			}
			return *this;
		}

		//----------------------------------------------------------------------
		void RunSerialization()
		{
			if ((Target != nullptr) && (OldInstancedSubObjects.Num() != 0))
			{
				Target->Serialize(*this);
			}
		}

	private:
		UObject* TargetCDO;
		UObject* Target;
		const FInstancedPropertyMap& OldInstancedSubObjects;
	};
}

void FBlueprintCompileReinstancer::ReplaceInstancesOfClass(UClass* OldClass, UClass* NewClass, UObject*	OriginalCDO, TSet<UObject*>* ObjectsThatShouldUseOldStuff, bool bClassObjectReplaced, bool bPreserveRootComponent)
{
	TMap<UClass*, UClass*> OldToNewClassMap;
	OldToNewClassMap.Add(OldClass, NewClass);
	ReplaceInstancesOfClass_Inner(OldToNewClassMap, OriginalCDO, ObjectsThatShouldUseOldStuff, bClassObjectReplaced, bPreserveRootComponent);
}

void FBlueprintCompileReinstancer::BatchReplaceInstancesOfClass(TMap<UClass*, UClass*>& InOldToNewClassMap, TSet<UObject*>* ObjectsThatShouldUseOldStuff, bool bClassObjectReplaced, bool bPreserveRootComponent)
{
	if (InOldToNewClassMap.Num() == 0)
	{
		return;
	}

	ReplaceInstancesOfClass_Inner(InOldToNewClassMap, nullptr, ObjectsThatShouldUseOldStuff, bClassObjectReplaced, bPreserveRootComponent);
}

UClass* FBlueprintCompileReinstancer::MoveCDOToNewClass(UClass* OwnerClass, const TMap<UClass*, UClass*>& OldToNewMap, bool bAvoidCDODuplication)
{
	GIsDuplicatingClassForReinstancing = true;
	OwnerClass->ClassFlags |= CLASS_NewerVersionExists;

	UObject* OldCDO = OwnerClass->ClassDefaultObject;
	const FName ReinstanceName = MakeUniqueObjectName(GetTransientPackage(), OwnerClass->GetClass(), *(FString(TEXT("REINST_")) + *OwnerClass->GetName()));
	UClass* CopyOfOwnerClass = CastChecked<UClass>(StaticDuplicateObject(OwnerClass, GetTransientPackage(), ReinstanceName, ~RF_Transactional));

	CopyOfOwnerClass->RemoveFromRoot();
	OwnerClass->ClassFlags &= ~CLASS_NewerVersionExists;
	GIsDuplicatingClassForReinstancing = false;

	UClass * const* OverridenParent = OldToNewMap.Find(CopyOfOwnerClass->GetSuperClass());
	if(OverridenParent && *OverridenParent)
	{
		CopyOfOwnerClass->SetSuperStruct(*OverridenParent);
	}

	UBlueprintGeneratedClass* BPClassToReinstance = Cast<UBlueprintGeneratedClass>(OwnerClass);
	UBlueprintGeneratedClass* BPGDuplicatedClass = Cast<UBlueprintGeneratedClass>(CopyOfOwnerClass);
	if (BPGDuplicatedClass && BPClassToReinstance && BPClassToReinstance->OverridenArchetypeForCDO)
	{
		BPGDuplicatedClass->OverridenArchetypeForCDO = BPClassToReinstance->OverridenArchetypeForCDO;
	}

	UFunction* DuplicatedClassUberGraphFunction = BPGDuplicatedClass ? BPGDuplicatedClass->UberGraphFunction : nullptr;
	if (DuplicatedClassUberGraphFunction)
	{
		DuplicatedClassUberGraphFunction->Bind();
		DuplicatedClassUberGraphFunction->StaticLink(true);
	}

	CopyOfOwnerClass->Bind();
	CopyOfOwnerClass->StaticLink(true);

	if(OldCDO)
	{
		// @todo: #dano, rename bAvoidCDODuplication because it's really a flag to move the CDO aside not 'prevent duplication':
		if(bAvoidCDODuplication)
		{
			OwnerClass->ClassDefaultObject = nullptr;
			OldCDO->Rename(nullptr, CopyOfOwnerClass->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
			CopyOfOwnerClass->ClassDefaultObject = OldCDO;
		}
		OldCDO->SetClass(CopyOfOwnerClass);
	}
	return CopyOfOwnerClass;
}

static void ReplaceObjectHelper(UObject*& OldObject, UClass* OldClass, UObject*& NewUObject, UClass* NewClass, TMap<UObject*, UObject*>& OldToNewInstanceMap, TMap<UObject*, FName>& OldToNewNameMap, int32 OldObjIndex, TArray<UObject*>& ObjectsToReplace, TArray<UObject*>& PotentialEditorsForRefreshing, TSet<AActor*>& OwnersToRerunConstructionScript, TFunctionRef<TArray<USceneComponent*>&(USceneComponent*)> GetAttachChildrenArray, bool bIsComponent)
{
	// If the old object was spawned from an archetype (i.e. not the CDO), we must use the new version of that archetype as the template object when constructing the new instance.
	UObject* OldArchetype = OldObject->GetArchetype();
	UObject* NewArchetype = OldToNewInstanceMap.FindRef(OldArchetype);

	bool bArchetypeReinstanced = (OldArchetype == OldClass->GetDefaultObject()) || (NewArchetype != nullptr);
	// if we don't have a updated archetype to spawn from, we need to update/reinstance it
	while (!bArchetypeReinstanced)
	{
		int32 ArchetypeIndex = ObjectsToReplace.Find(OldArchetype);
		if (ArchetypeIndex != INDEX_NONE)
		{
			if (ensure(ArchetypeIndex > OldObjIndex))
			{
				// if this object has an archetype, but it hasn't been 
				// reinstanced yet (but is queued to) then we need to swap out 
				// the two, and reinstance the archetype first
				ObjectsToReplace.Swap(ArchetypeIndex, OldObjIndex);
				OldObject = ObjectsToReplace[OldObjIndex];
				check(OldObject == OldArchetype);

				OldArchetype = OldObject->GetArchetype();
				NewArchetype = OldToNewInstanceMap.FindRef(OldArchetype);
				bArchetypeReinstanced = (OldArchetype == OldClass->GetDefaultObject()) || (NewArchetype != nullptr);
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
	// Check that either this was an instance of the class directly, or we found a new archetype for it
	ensureMsgf(bArchetypeReinstanced, TEXT("Reinstancing non-actor (%s); failed to resolve archetype object - property values may be lost."), *OldObject->GetPathName());

	EObjectFlags OldFlags = OldObject->GetFlags();

	FName OldName(OldObject->GetFName());

	// If the old object is in this table, we've already renamed it away in a previous iteration. Don't rename it again!
	if (!OldToNewNameMap.Contains(OldObject))
	{
		// If we're reinstancing a component template, we also need to rename any inherited templates that are found to be based on it, in order to preserve archetype paths.
		if (bIsComponent && OldObject->HasAllFlags(RF_ArchetypeObject) && OldObject->GetOuter()->IsA<UBlueprintGeneratedClass>())
		{
			// Gather all component templates from the current archetype to the farthest antecedent inherited template(s).
			TArray<UObject*> OldArchetypeObjects;
			FArchetypeReinstanceHelper::GetArchetypeObjects(OldObject, OldArchetypeObjects, RF_InheritableComponentTemplate);

			// Find a unique object name that does not conflict with anything in the scope of all outers in the template chain.
			const FString OldArchetypeName = FArchetypeReinstanceHelper::FindUniqueArchetypeObjectName(OldArchetypeObjects).ToString();

			for (UObject* OldArchetypeObject : OldArchetypeObjects)
			{
				OldToNewNameMap.Add(OldArchetypeObject, OldName);
				OldArchetypeObject->Rename(*OldArchetypeName, OldArchetypeObject->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | (GBlueprintUseCompilationManager ? REN_ForceNoResetLoaders : 0));
			}
		}
		else
		{
			OldObject->Rename(nullptr, OldObject->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | (GBlueprintUseCompilationManager ? REN_ForceNoResetLoaders : 0));
		}
	}
						
	{
		// We may have already renamed this object to temp space if it was an inherited archetype in a previous iteration; check for that here.
		FName NewName = OldToNewNameMap.FindRef(OldObject);
		if (NewName == NAME_None)
		{
			// Otherwise, just use the old object's current name.
			NewName = OldName;
		}

		FMakeClassSpawnableOnScope TemporarilySpawnable(NewClass);
		NewUObject = NewObject<UObject>(OldObject->GetOuter(), NewClass, NewName, RF_NoFlags, NewArchetype);
	}

	check(NewUObject != nullptr);

	const EObjectFlags FlagMask = RF_Public | RF_ArchetypeObject | RF_Transactional | RF_Transient | RF_TextExportTransient | RF_InheritableComponentTemplate | RF_Standalone; //TODO: what about RF_RootSet?
	NewUObject->SetFlags(OldFlags & FlagMask);

	InstancedPropertyUtils::FInstancedPropertyMap InstancedPropertyMap;
	InstancedPropertyUtils::FArchiveInstancedSubObjCollector  InstancedSubObjCollector(OldObject, InstancedPropertyMap);
	UEditorEngine::CopyPropertiesForUnrelatedObjects(OldObject, NewUObject);
	InstancedPropertyUtils::FArchiveInsertInstancedSubObjects InstancedSubObjSpawner(NewUObject, InstancedPropertyMap);

	if (UAnimInstance* AnimTree = Cast<UAnimInstance>(NewUObject))
	{
		// Initialising the anim instance isn't enough to correctly set up the skeletal mesh again in a
		// paused world, need to initialise the skeletal mesh component that contains the anim instance.
		if (USkeletalMeshComponent* SkelComponent = Cast<USkeletalMeshComponent>(AnimTree->GetOuter()))
		{
			SkelComponent->InitAnim(true);
		}
	}

	UWorld* RegisteredWorld = nullptr;
	bool bWasRegistered = false;
	if (bIsComponent)
	{
		UActorComponent* OldComponent = CastChecked<UActorComponent>(OldObject);
		if (OldComponent->IsRegistered())
		{
			bWasRegistered = true;
			RegisteredWorld = OldComponent->GetWorld();
			OldComponent->UnregisterComponent();
		}
	}

	OldObject->RemoveFromRoot();
	OldObject->MarkPendingKill();

	OldToNewInstanceMap.Add(OldObject, NewUObject);

	if (bIsComponent)
	{
		UActorComponent* Component = CastChecked<UActorComponent>(NewUObject);
		AActor* OwningActor = Component->GetOwner();
		if (OwningActor)
		{
			OwningActor->ResetOwnedComponents();

			// Check to see if they have an editor that potentially needs to be refreshed
			if (OwningActor->GetClass()->ClassGeneratedBy)
			{
				PotentialEditorsForRefreshing.AddUnique(OwningActor->GetClass()->ClassGeneratedBy);
			}

			// we need to keep track of actor instances that need 
			// their construction scripts re-ran (since we've just 
			// replaced a component they own)

			// Skipping CDOs as CSs are not allowed for them.
			if (!OwningActor->HasAnyFlags(RF_ClassDefaultObject))
			{
				OwnersToRerunConstructionScript.Add(OwningActor);
			}
		}

		if (bWasRegistered)
		{
			if (RegisteredWorld && OwningActor == nullptr)
			{
				// Thumbnail components are added to a World without an actor, so we must special case their
				// REINST to register them with the world again.
				// The old thumbnail component is GC'd and will ensure if all it's attachments are not released
				// @TODO: This special case can breakdown if the nature of thumbnail components changes and could
				// use a cleanup later.
				if (OldObject->GetOutermost() == GetTransientPackage())
				{
					if (USceneComponent* SceneComponent = Cast<USceneComponent>(OldObject))
					{
						GetAttachChildrenArray(SceneComponent).Empty();
						SceneComponent->SetupAttachment(nullptr);
					}
				}

				Component->RegisterComponentWithWorld(RegisteredWorld);
			}
			else
			{
				Component->RegisterComponent();
			}
		}
	}
}

static void ReplaceActorHelper(UObject* OldObject, UClass* OldClass, UObject*& NewUObject, UClass* NewClass, TMap<UObject*, UObject*>& OldToNewInstanceMap, TMap<UClass*, UClass*>& InOldToNewClassMap, AActor* OldActor, TMap<FStringAssetReference, UObject*>& ReinstancedObjectsWeakReferenceMap, TMap<UObject*, FActorAttachmentData>& ActorAttachmentData, TArray<FActorReplacementHelper>& ReplacementActors, bool bPreserveRootComponent, bool& bSelectionChanged)
{
	FVector  Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	if (USceneComponent* OldRootComponent = OldActor->GetRootComponent())
	{
		// We need to make sure that the GetComponentTransform() transform is up to date, but we don't want to run any initialization logic
		// so we silence the update, cache it off, revert the change (so no events are raised), and then directly update the transform
		// with the value calculated in ConditionalUpdateComponentToWorld:
		FScopedMovementUpdate SilenceMovement(OldRootComponent);

		OldRootComponent->ConditionalUpdateComponentToWorld();
		FTransform OldComponentToWorld = OldRootComponent->GetComponentTransform();
		SilenceMovement.RevertMove();

		OldRootComponent->SetComponentToWorld(OldComponentToWorld);
		Location = OldActor->GetActorLocation();
		Rotation = OldActor->GetActorRotation();
	}

	// If this actor was spawned from an Archetype, we spawn the new actor from the new version of that archetype
	UObject* OldArchetype = OldActor->GetArchetype();
	UWorld*  World = OldActor->GetWorld();
	AActor*  NewArchetype = Cast<AActor>(OldToNewInstanceMap.FindRef(OldArchetype));
	// Check that either this was an instance of the class directly, or we found a new archetype for it
	check(OldArchetype == OldClass->GetDefaultObject() || NewArchetype);

	// Spawn the new actor instance, in the same level as the original, but deferring running the construction script until we have transferred modified properties
	ULevel*  ActorLevel = OldActor->GetLevel();
	UClass** MappedClass = InOldToNewClassMap.Find(OldActor->GetClass());
	UClass*  SpawnClass = MappedClass ? *MappedClass : NewClass;

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.OverrideLevel = ActorLevel;
	SpawnInfo.Template = NewArchetype;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.bDeferConstruction = true;
	SpawnInfo.Name = OldActor->GetFName();

	OldActor->UObject::Rename(nullptr, OldObject->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | (GBlueprintUseCompilationManager ? REN_ForceNoResetLoaders : 0));

	AActor* NewActor = nullptr;
	{
		FMakeClassSpawnableOnScope TemporarilySpawnable(SpawnClass);
		NewActor = World->SpawnActor(SpawnClass, &Location, &Rotation, SpawnInfo);
	}

	if (OldActor->CurrentTransactionAnnotation.IsValid())
	{
		NewActor->CurrentTransactionAnnotation = OldActor->CurrentTransactionAnnotation;
	}

	check(NewActor != nullptr);
	NewUObject = NewActor;
	// store the new actor for the second pass (NOTE: this detaches 
	// OldActor from all child/parent attachments)
	//
	// running the NewActor's construction-script is saved for that 
	// second pass (because the construction-script may reference 
	// another instance that hasn't been replaced yet).
	FActorAttachmentData& CurrentAttachmentData = ActorAttachmentData.FindChecked(OldObject);
	ReplacementActors.Add(FActorReplacementHelper(NewActor, OldActor, MoveTemp(CurrentAttachmentData)));
	ActorAttachmentData.Remove(OldObject);

	ReinstancedObjectsWeakReferenceMap.Add(OldObject, NewUObject);

	OldActor->DestroyConstructedComponents(); // don't want to serialize components from the old actor
												// Unregister native components so we don't copy any sub-components they generate for themselves (like UCameraComponent does)
	OldActor->UnregisterAllComponents();

	// Unregister any native components, might have cached state based on properties we are going to overwrite
	NewActor->UnregisterAllComponents();

	UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
	Params.bPreserveRootComponent = bPreserveRootComponent;
	UEngine::CopyPropertiesForUnrelatedObjects(OldActor, NewActor, Params);

	// reset properties/streams
	NewActor->ResetPropertiesForConstruction();
	// register native components
	NewActor->RegisterAllComponents();

	// 
	// clean up the old actor (unselect it, remove it from the world, etc.)...

	if (OldActor->IsSelected())
	{
		if(GEditor)
		{
			GEditor->SelectActor(OldActor, /*bInSelected =*/false, /*bNotify =*/false);
		}
		bSelectionChanged = true;
	}
	if (GEditor && GEditor->Layers.IsValid())
	{
		GEditor->Layers->DisassociateActorFromLayers(OldActor);
	}

	World->EditorDestroyActor(OldActor, /*bShouldModifyLevel =*/true);
	OldToNewInstanceMap.Add(OldActor, NewActor);
}

void FBlueprintCompileReinstancer::ReplaceInstancesOfClass_Inner(TMap<UClass*, UClass*>& InOldToNewClassMap, UObject* InOriginalCDO, TSet<UObject*>* ObjectsThatShouldUseOldStuff, bool bClassObjectReplaced, bool bPreserveRootComponent)
{
	// If there is an original CDO, we are only reinstancing a single class
	check((InOriginalCDO != nullptr && InOldToNewClassMap.Num() == 1) || InOriginalCDO == nullptr); // (InOldToNewClassMap.Num() > 1 && InOriginalCDO == nullptr) || (InOldToNewClassMap.Num() == 1 && InOriginalCDO != nullptr));

	if (InOldToNewClassMap.Num() == 0)
	{
		return;
	}

	USelection* SelectedActors = nullptr;
	bool bSelectionChanged = false;
	TArray<UObject*> ObjectsToReplace;
	const bool bLogConversions = false; // for debugging

	// Map of old objects to new objects
	TMap<UObject*, UObject*> OldToNewInstanceMap;

	// Map of old objects to new name (used to assist with reinstancing archetypes)
	TMap<UObject*, FName> OldToNewNameMap;

	TMap<FSoftObjectPath, UObject*> ReinstancedObjectsWeakReferenceMap;

	// actors being replace
	TArray<FActorReplacementHelper> ReplacementActors;

	// A list of objects (e.g. Blueprints) that potentially have editors open that we need to refresh
	TArray<UObject*> PotentialEditorsForRefreshing;

	// A list of component owners that need their construction scripts re-ran (because a component of theirs has been reinstanced)
	TSet<AActor*> OwnersToRerunConstructionScript;

	// Set global flag to let system know we are reconstructing blueprint instances
	TGuardValue<bool> GuardTemplateNameFlag(GIsReconstructingBlueprintInstances, true);

	struct FObjectRemappingHelper
	{
		void OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacedObjects)
		{
			ReplacedObjects.Append(InReplacedObjects);
		}

		TMap<UObject*, UObject*> ReplacedObjects;
	} ObjectRemappingHelper;

	FDelegateHandle OnObjectsReplacedHandle = FDelegateHandle();
	if(GEditor)
	{
		OnObjectsReplacedHandle = GEditor->OnObjectsReplaced().AddRaw(&ObjectRemappingHelper, &FObjectRemappingHelper::OnObjectsReplaced);
	}

	auto UpdateObjectBeingDebugged = [](UObject* InOldObject, UObject* InNewObject)
	{
		if (UBlueprint* OldObjBlueprint = Cast<UBlueprint>(InOldObject->GetClass()->ClassGeneratedBy))
		{
			if (OldObjBlueprint->GetObjectBeingDebugged() == InOldObject)
			{
				OldObjBlueprint->SetObjectBeingDebugged(InNewObject);
			}
		}
	};

	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_ReplaceInstancesOfClass);
		if(GEditor && GEditor->GetSelectedActors())
		{
			SelectedActors = GEditor->GetSelectedActors();
			SelectedActors->BeginBatchSelectOperation();
			SelectedActors->Modify();
		}

		// WARNING: for (TPair<UClass*, UClass*> OldToNewClass : InOldToNewClassMap) duplicated below 
		// to handle reconstructing actors which need to be reinstanced aftertheir owned components 
		// have been updated:
		for (TPair<UClass*, UClass*> OldToNewClass : InOldToNewClassMap)
		{
			UClass* OldClass = OldToNewClass.Key;
			UClass* NewClass = OldToNewClass.Value;
			check(OldClass && NewClass);
#if WITH_HOT_RELOAD
			check(OldClass != NewClass || GIsHotReload);
#else
			check(OldClass != NewClass);
#endif
			{
				const bool bIncludeDerivedClasses = false;
				ObjectsToReplace.Empty();
				GetObjectsOfClass(OldClass, ObjectsToReplace, bIncludeDerivedClasses);
				// Then fix 'real' (non archetype) instances of the class
				for (int32 OldObjIndex = 0; OldObjIndex < ObjectsToReplace.Num(); ++OldObjIndex)
				{
					UObject* OldObject = ObjectsToReplace[OldObjIndex];
					AActor* OldActor = Cast<AActor>(OldObject);

					// Skip archetype instances, EXCEPT for component templates and child actor templates
					const bool bIsComponent = NewClass->IsChildOf(UActorComponent::StaticClass());
					const bool bIsChildActorTemplate = OldActor && OldActor->GetOuter()->IsA<UChildActorComponent>();
					if (OldObject->IsPendingKill() || 
						(!bIsComponent && !bIsChildActorTemplate && OldObject->IsTemplate()))
					{
						continue;
					}

					// WARNING: This loop only handles non-actor objects, actor objects are handled below:
					if (OldActor == nullptr)
					{
						UObject* NewUObject = nullptr;
						ReplaceObjectHelper(OldObject, OldClass, NewUObject, NewClass, OldToNewInstanceMap, OldToNewNameMap, OldObjIndex, ObjectsToReplace, PotentialEditorsForRefreshing, OwnersToRerunConstructionScript, &FDirectAttachChildrenAccessor::Get, bIsComponent);
						UpdateObjectBeingDebugged(OldObject, NewUObject);
						
						if (bLogConversions)
						{
							UE_LOG(LogBlueprint, Log, TEXT("Converted instance '%s' to '%s'"), *GetPathNameSafe(OldObject), *GetPathNameSafe(NewUObject));
						}
					}
				}
			}
		}

		// WARNING: for (TPair<UClass*, UClass*> OldToNewClass : InOldToNewClassMap) duplicated above 
		// this loop only handles actors - which need to be reconstructed *after* their owned components 
		// have been reinstanced:
		for (TPair<UClass*, UClass*> OldToNewClass : InOldToNewClassMap)
		{
			UClass* OldClass = OldToNewClass.Key;
			UClass* NewClass = OldToNewClass.Value;
			check(OldClass && NewClass);

			{
				const bool bIncludeDerivedClasses = false;
				ObjectsToReplace.Empty();
				GetObjectsOfClass(OldClass, ObjectsToReplace, bIncludeDerivedClasses);

				// store old attachment data before we mess with components, etc:
				TMap<UObject*, FActorAttachmentData> ActorAttachmentData;
				for (int32 OldObjIndex = 0; OldObjIndex < ObjectsToReplace.Num(); ++OldObjIndex)
				{
					UObject* OldObject = ObjectsToReplace[OldObjIndex];
					if (!OldObject->IsPendingKill())
					{
						if (AActor* OldActor = Cast<AActor>(OldObject))
						{
							ActorAttachmentData.Add(OldObject, FActorAttachmentData(OldActor));
						}
					}
				}

				// Then fix 'real' (non archetype) instances of the class
				for (int32 OldObjIndex = 0; OldObjIndex < ObjectsToReplace.Num(); ++OldObjIndex)
				{
					UObject* OldObject = ObjectsToReplace[OldObjIndex];
					AActor* OldActor = Cast<AActor>(OldObject);

					// Skip archetype instances, EXCEPT for child actor templates
					const bool bIsChildActorTemplate = OldActor && OldActor->GetOuter()->IsA<UChildActorComponent>();
					if (OldObject->IsPendingKill() || (!bIsChildActorTemplate && OldObject->IsTemplate()))
					{
						continue;
					}
					
					// WARNING: This loop only handles actor objects that are in a level, all other objects are
					// handled above
					if (OldActor != nullptr)
					{
						UObject* NewUObject = nullptr;
						if(OldActor->GetLevel())
						{
							ReplaceActorHelper(OldObject, OldClass, NewUObject, NewClass, OldToNewInstanceMap, InOldToNewClassMap, OldActor, ReinstancedObjectsWeakReferenceMap, ActorAttachmentData, ReplacementActors, bPreserveRootComponent, bSelectionChanged);
						}
						else
						{
							// Actors that are not in a level cannot be reconstructed, sequencer team decided to reinstance these as normal objects:
							ReplaceObjectHelper(OldObject, OldClass, NewUObject, NewClass, OldToNewInstanceMap, OldToNewNameMap, OldObjIndex, ObjectsToReplace, PotentialEditorsForRefreshing, OwnersToRerunConstructionScript, &FDirectAttachChildrenAccessor::Get, false);
						}
						UpdateObjectBeingDebugged(OldObject, NewUObject);
						
						if (bLogConversions)
						{
							UE_LOG(LogBlueprint, Log, TEXT("Converted instance '%s' to '%s'"), *GetPathNameSafe(OldObject), *GetPathNameSafe(NewUObject));
						}
					}
				}
			}
		}
	}

	if(GEditor)
	{
		GEditor->OnObjectsReplaced().Remove(OnObjectsReplacedHandle);
	}

	// Now replace any pointers to the old archetypes/instances with pointers to the new one
	TArray<UObject*> SourceObjects;
	TArray<UObject*> DstObjects;

	OldToNewInstanceMap.GenerateKeyArray(SourceObjects);
	OldToNewInstanceMap.GenerateValueArray(DstObjects); // Also look for references in new spawned objects.

	SourceObjects.Append(DstObjects);
	
	if (InOriginalCDO)
	{
		check(InOldToNewClassMap.Num() == 1);
		for (TPair<UClass*, UClass*> OldToNewClass : InOldToNewClassMap)
		{
			UClass* OldClass = OldToNewClass.Key;
			UClass* NewClass = OldToNewClass.Value;
			check(OldClass && NewClass);
#if WITH_HOT_RELOAD
			check(OldClass != NewClass || GIsHotReload);
#else
			check(OldClass != NewClass);
#endif

			FReplaceReferenceHelper::IncludeCDO(OldClass, NewClass, OldToNewInstanceMap, SourceObjects, InOriginalCDO);

			if (bClassObjectReplaced)
			{
				FReplaceReferenceHelper::IncludeClass(OldClass, NewClass, OldToNewInstanceMap, SourceObjects, ObjectsToReplace);
			}
		}
	}

	FReplaceReferenceHelper::FindAndReplaceReferences(SourceObjects, ObjectsThatShouldUseOldStuff, ObjectsToReplace, OldToNewInstanceMap, ReinstancedObjectsWeakReferenceMap);

	{ BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_ReplacementConstruction);

		// the process of setting up new replacement actors is split into two 
		// steps (this here, is the second)...
		// 
		// the "finalization" here runs the replacement actor's construction-
		// script and is left until late to account for a scenario where the 
		// construction-script attempts to modify another instance of the 
		// same class... if this were to happen above, in the ObjectsToReplace 
		// loop, then accessing that other instance would cause an assert in 
		// UProperty::ContainerPtrToValuePtrInternal() (which appropriatly 
		// complains that the other instance's type doesn't match because it 
		// hasn't been replaced yet... that's why we wait until after 
		// FArchiveReplaceObjectRef to run construction-scripts).
		for (FActorReplacementHelper& ReplacementActor : ReplacementActors)
		{
			ReplacementActor.Finalize(ObjectRemappingHelper.ReplacedObjects, ObjectsThatShouldUseOldStuff, ObjectsToReplace, ReinstancedObjectsWeakReferenceMap);
		}

		for (FActorReplacementHelper& ReplacementActor : ReplacementActors)
		{
			ReplacementActor.ApplyAttachments(ObjectRemappingHelper.ReplacedObjects, ObjectsThatShouldUseOldStuff, ObjectsToReplace, ReinstancedObjectsWeakReferenceMap);
		}
	}

	if(SelectedActors)
	{
		SelectedActors->EndBatchSelectOperation();
	}

	if (bSelectionChanged && GEditor)
	{
		GEditor->NoteSelectionChange();
	}

	if (GEditor)
	{
		// Refresh any editors for objects that we've updated components for
		for (UObject* BlueprintAsset : PotentialEditorsForRefreshing)
		{
			FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(FAssetEditorManager::Get().FindEditorForAsset(BlueprintAsset, /*bFocusIfOpen =*/false));
			if (BlueprintEditor)
			{
				BlueprintEditor->RefreshEditors();
			}
		}
	}

	// in the case where we're replacing component instances, we need to make 
	// sure to re-run their owner's construction scripts
	for (AActor* ActorInstance : OwnersToRerunConstructionScript)
	{
		ActorInstance->RerunConstructionScripts();
	}
}

void FBlueprintCompileReinstancer::ReconstructOwnerInstances(TSubclassOf<UActorComponent> ComponentClass)
{
	if (ComponentClass == nullptr)
	{
		return;
	}

	TArray<UObject*> ComponentInstances;
	GetObjectsOfClass(ComponentClass, ComponentInstances, /*bIncludeDerivedClasses =*/false);

	TSet<AActor*> OwnerInstances;
	for (UObject* ComponentObj : ComponentInstances)
	{
	
		UActorComponent* Component = CastChecked<UActorComponent>(ComponentObj);
			
		if (AActor* OwningActor = Component->GetOwner())
		{
			// we don't just rerun construction here, because we could end up 
			// doing it twice for the same actor (if it had multiple components 
			// of this kind), so we put that off as a secondary pass
			OwnerInstances.Add(OwningActor);
		}
	}

	for (AActor* ComponentOwner : OwnerInstances)
	{
		ComponentOwner->RerunConstructionScripts();
	}
}

void FBlueprintCompileReinstancer::VerifyReplacement()
{
	TArray<UObject*> SourceObjects;

	// Find all instances of the old class
	for( TObjectIterator<UObject> it; it; ++it )
	{
		UObject* CurrentObj = *it;

		if( (CurrentObj->GetClass() == DuplicatedClass) )
		{
			SourceObjects.Add(CurrentObj);
		}
	}

	// For each instance, track down references
	if( SourceObjects.Num() > 0 )
	{
		TFindObjectReferencers<UObject> Referencers(SourceObjects, nullptr, false);
		for (TFindObjectReferencers<UObject>::TIterator It(Referencers); It; ++It)
		{
			UObject* CurrentObject = It.Key();
			UObject* ReferencedObj = It.Value();
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("- Object %s is referencing %s ---"), *CurrentObject->GetName(), *ReferencedObj->GetName());
		}
	}
}
void FBlueprintCompileReinstancer::ReparentChild(UBlueprint* ChildBP)
{
	check(ChildBP);

	UClass* SkeletonClass = ChildBP->SkeletonGeneratedClass;
	UClass* GeneratedClass = ChildBP->GeneratedClass;

	const bool ReparentGeneratedOnly = (ReinstClassType == RCT_BpGenerated);
	if( !ReparentGeneratedOnly && SkeletonClass )
	{
		ReparentChild(SkeletonClass);
	}

	const bool ReparentSkelOnly = (ReinstClassType == RCT_BpSkeleton);
	if( !ReparentSkelOnly && GeneratedClass )
	{
		ReparentChild(GeneratedClass);
	}
}

void FBlueprintCompileReinstancer::ReparentChild(UClass* ChildClass)
{
	check(ChildClass && ClassToReinstance && DuplicatedClass && ChildClass->GetSuperClass());
	bool bIsReallyAChild = ChildClass->GetSuperClass() == ClassToReinstance || ChildClass->GetSuperClass() == DuplicatedClass;
	const UBlueprint* SuperClassBP = Cast<UBlueprint>(ChildClass->GetSuperClass()->ClassGeneratedBy);
	if (SuperClassBP && !bIsReallyAChild)
	{
		bIsReallyAChild |= (SuperClassBP->SkeletonGeneratedClass == ClassToReinstance) || (SuperClassBP->SkeletonGeneratedClass == DuplicatedClass);
		bIsReallyAChild |= (SuperClassBP->GeneratedClass == ClassToReinstance) || (SuperClassBP->GeneratedClass == DuplicatedClass);
	}
	check(bIsReallyAChild);

	ChildClass->AssembleReferenceTokenStream();
	ChildClass->SetSuperStruct(DuplicatedClass);
	ChildClass->Bind();
	ChildClass->StaticLink(true);
}

void FBlueprintCompileReinstancer::CopyPropertiesForUnrelatedObjects(UObject* OldObject, UObject* NewObject, bool bClearExternalReferences)
{
	InstancedPropertyUtils::FInstancedPropertyMap InstancedPropertyMap;
	InstancedPropertyUtils::FArchiveInstancedSubObjCollector  InstancedSubObjCollector(OldObject, InstancedPropertyMap);

	UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
	Params.bAggressiveDefaultSubobjectReplacement = false;//true;
	Params.bDoDelta = !OldObject->HasAnyFlags(RF_ClassDefaultObject);
	Params.bCopyDeprecatedProperties = true;
	Params.bSkipCompilerGeneratedDefaults = true;
	Params.bClearReferences = bClearExternalReferences;
	UEngine::CopyPropertiesForUnrelatedObjects(OldObject, NewObject, Params);

	InstancedPropertyUtils::FArchiveInsertInstancedSubObjects InstancedSubObjSpawner(NewObject, InstancedPropertyMap);
}

FRecreateUberGraphFrameScope::FRecreateUberGraphFrameScope(UClass* InClass, bool bRecreate)
	: RecompiledClass(InClass)
{
	if (bRecreate && ensure(RecompiledClass))
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_RecreateUberGraphPersistentFrame);

		const bool bIncludeDerivedClasses = true;
		GetObjectsOfClass(RecompiledClass, Objects, bIncludeDerivedClasses, RF_NoFlags);

		for (UObject* Obj : Objects)
		{
			RecompiledClass->DestroyPersistentUberGraphFrame(Obj);
		}
	}
}

FRecreateUberGraphFrameScope::~FRecreateUberGraphFrameScope()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_RecreateUberGraphPersistentFrame);
	for (UObject* Obj : Objects)
	{
		if (IsValid(Obj))
		{
			RecompiledClass->CreatePersistentUberGraphFrame(Obj, false);
		}
	}
}
