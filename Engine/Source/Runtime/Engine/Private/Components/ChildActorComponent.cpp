// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ChildActorComponent.h"
#include "Engine/World.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "UObject/PropertyPortFlags.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogChildActorComponent, Warning, All);

UChildActorComponent::UChildActorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowReregistration = false;
}

void UChildActorComponent::OnRegister()
{
	Super::OnRegister();

	if (ChildActor)
	{
		if (bNeedsRecreate || ChildActor->GetClass() != ChildActorClass)
		{
			bNeedsRecreate = false;
			DestroyChildActor();
			CreateChildActor();
		}
		else
		{
			ChildActorName = ChildActor->GetFName();
			
			USceneComponent* ChildRoot = ChildActor->GetRootComponent();
			if (ChildRoot && ChildRoot->GetAttachParent() != this)
			{
				// attach new actor to this component
				// we can't attach in CreateChildActor since it has intermediate Mobility set up
				// causing spam with inconsistent mobility set up
				// so moving Attach to happen in Register
				ChildRoot->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			}

			// Ensure the components replication is correctly initialized
			SetIsReplicated(ChildActor->GetIsReplicated());
		}
	}
	else if (ChildActorClass)
	{
		CreateChildActor();
	}
}

void UChildActorComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.HasAllPortFlags(PPF_DuplicateForPIE))
	{
		// PIE duplication should just work normally
		Ar << ChildActorTemplate;
	}
	else if (Ar.HasAllPortFlags(PPF_Duplicate))
	{
		if (GIsEditor && Ar.IsLoading() && !IsTemplate())
		{
			// If we're not a template then we do not want the duplicate so serialize manually and destroy the template that was created for us
			Ar.Serialize(&ChildActorTemplate, sizeof(UObject*));

			if (AActor* UnwantedDuplicate = static_cast<AActor*>(FindObjectWithOuter(this, AActor::StaticClass())))
			{
				UnwantedDuplicate->MarkPendingKill();
			}
		}
		else if (!GIsEditor && !Ar.IsLoading() && !GIsDuplicatingClassForReinstancing)
		{
			// Avoid the archiver in the duplicate writer case because we want to avoid the duplicate being created
			Ar.Serialize(&ChildActorTemplate, sizeof(UObject*));
		}
		else
		{
			// When we're loading outside of the editor we won't have created the duplicate, so its fine to just use the normal path
			// When we're loading a template then we want the duplicate, so it is fine to use normal archiver
			// When we're saving in the editor we'll create the duplicate, but on loading decide whether to take it or not
			Ar << ChildActorTemplate;
		}
	}

#if WITH_EDITOR
	if (ChildActorClass == nullptr)
	{
		// It is unknown how this state can come to be, so for now we'll simply correct the issue and record that it occurs and 
		// and if it is occurring frequently, then investigate how the state comes to pass
		if (!ensureAlwaysMsgf(ChildActorTemplate == nullptr, TEXT("Found unexpected ChildActorTemplate %s when ChildActorClass is null"), *ChildActorTemplate->GetFullName()))
		{
			ChildActorTemplate = nullptr;
		}
	}
	// Since we sometimes serialize properties in instead of using duplication and we can end up pointing at the wrong template
	else if (!Ar.IsPersistent() && ChildActorTemplate)
	{
		if (IsTemplate())
		{
			// If we are a template and are not pointing at a component we own we'll need to fix that
			if (ChildActorTemplate->GetOuter() != this)
			{
				const FString TemplateName = FString::Printf(TEXT("%s_%s_CAT"), *GetName(), *ChildActorClass->GetName());
				ChildActorTemplate = CastChecked<AActor>(StaticDuplicateObject(ChildActorTemplate, this, *TemplateName));
			}
		}
		else
		{
			// Because the template may have fixed itself up, the tagged property delta serialized for 
			// the instance may point at a trashed template, so always repoint us to the archetypes template
			ChildActorTemplate = CastChecked<UChildActorComponent>(GetArchetype())->ChildActorTemplate;
		}
	}
#endif
}

#if WITH_EDITOR
void UChildActorComponent::PostEditImport()
{
	Super::PostEditImport();

	if (IsTemplate())
	{
		TArray<UObject*> Children;
		GetObjectsWithOuter(this, Children, false);

		for (UObject* Child : Children)
		{
			if (Child->GetClass() == ChildActorClass)
			{
				ChildActorTemplate = CastChecked<AActor>(Child);
				break;
			}
		}
	}
	else
	{
		ChildActorTemplate = CastChecked<UChildActorComponent>(GetArchetype())->ChildActorTemplate;
	}

	// Any cached instance data is invalid if we've had data imported in to us
	if (CachedInstanceData)
	{
		delete CachedInstanceData;
		CachedInstanceData = nullptr;
	}
}

void UChildActorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UChildActorComponent, ChildActorClass))
	{
		ChildActorName = NAME_None;

		if (IsTemplate())
		{
			// This case is necessary to catch the situation where we are propogating the change down to child blueprints
			SetChildActorClass(ChildActorClass);
		}
		else
		{
			UChildActorComponent* Archetype = CastChecked<UChildActorComponent>(GetArchetype());
			ChildActorTemplate = (Archetype->ChildActorClass == ChildActorClass ? Archetype->ChildActorTemplate : nullptr);
		}

		// If this was created by construction script, the post edit change super call will destroy it anyways
		if (!IsCreatedByConstructionScript())
		{
			DestroyChildActor();
			CreateChildActor();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UChildActorComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UChildActorComponent, ChildActorClass))
	{
		if (IsTemplate())
		{
			SetChildActorClass(ChildActorClass);
		}
		else
		{
			ChildActorTemplate = CastChecked<UChildActorComponent>(GetArchetype())->ChildActorTemplate;
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void UChildActorComponent::PostEditUndo()
{
	Super::PostEditUndo();

	// This hack exists to fix up known cases where the AttachChildren array is broken in very problematic ways.
	// The correct fix will be to use a Transaction Annotation at the SceneComponent level, however, it is too risky
	// to do right now, so this will go away when that is done.
	for (USceneComponent*& Component : FDirectAttachChildrenAccessor::Get(this))
	{
		if (Component)
		{
			if (Component->IsPendingKill() && Component->GetOwner() == ChildActor)
			{
				Component = ChildActor->GetRootComponent();
			}
		}
	}
	
}
#endif

void UChildActorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UChildActorComponent, ChildActor);
}

struct FActorParentComponentSetter
{
private:
	static void Set(AActor* ChildActor, UChildActorComponent* ParentComponent)
	{
		ChildActor->ParentComponent = ParentComponent;
	}

	friend UChildActorComponent;
};

void UChildActorComponent::PostRepNotifies()
{
	Super::PostRepNotifies();

	if (ChildActor)
	{
		FActorParentComponentSetter::Set(ChildActor, this);

		ChildActorClass = ChildActor->GetClass();
		ChildActorName = ChildActor->GetFName();
	}
	else
	{
		ChildActorClass = nullptr;
		ChildActorName = NAME_None;
	}
}

void UChildActorComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	DestroyChildActor();
}

void UChildActorComponent::OnUnregister()
{
	Super::OnUnregister();

	DestroyChildActor();
}

FChildActorComponentInstanceData::FChildActorComponentInstanceData(const UChildActorComponent* Component)
	: FSceneComponentInstanceData(Component)
	, ChildActorName(Component->GetChildActorName())
	, ComponentInstanceData(nullptr)
{
	if (Component->GetChildActor())
	{
		ComponentInstanceData = new FComponentInstanceDataCache(Component->GetChildActor());
		// If it is empty dump it
		if (!ComponentInstanceData->HasInstanceData())
		{
			delete ComponentInstanceData;
			ComponentInstanceData = nullptr;
		}

		USceneComponent* ChildRootComponent = Component->GetChildActor()->GetRootComponent();
		if (ChildRootComponent)
		{
			for (USceneComponent* AttachedComponent : ChildRootComponent->GetAttachChildren())
			{
				if (AttachedComponent)
				{
					AActor* AttachedActor = AttachedComponent->GetOwner();
					if (AttachedActor != Component->GetChildActor())
					{
						FAttachedActorInfo Info;
						Info.Actor = AttachedActor;
						Info.SocketName = AttachedComponent->GetAttachSocketName();
						Info.RelativeTransform = AttachedComponent->GetRelativeTransform();
						AttachedActors.Add(Info);
					}
				}
			}
		}
	}
}

FChildActorComponentInstanceData::~FChildActorComponentInstanceData()
{
	delete ComponentInstanceData;
}

void FChildActorComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
	CastChecked<UChildActorComponent>(Component)->ApplyComponentInstanceData(this, CacheApplyPhase);
}

void FChildActorComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	FSceneComponentInstanceData::AddReferencedObjects(Collector);

	if (ComponentInstanceData)
	{
		ComponentInstanceData->AddReferencedObjects(Collector);
	}
}

void UChildActorComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UChildActorComponent* This = CastChecked<UChildActorComponent>(InThis);

	if (This->CachedInstanceData)
	{
		This->CachedInstanceData->AddReferencedObjects(Collector);
	}

	Super::AddReferencedObjects(InThis, Collector);
}

void UChildActorComponent::BeginDestroy()
{
	Super::BeginDestroy();

	if (CachedInstanceData)
	{
		delete CachedInstanceData;
		CachedInstanceData = nullptr;
	}
}

FActorComponentInstanceData* UChildActorComponent::GetComponentInstanceData() const
{
	FChildActorComponentInstanceData* InstanceData = CachedInstanceData;

	if (CachedInstanceData)
	{
		// We've handed over ownership of the pointer to the instance cache, so drop our reference
		CachedInstanceData = nullptr;
	}
	else
	{
		InstanceData = new FChildActorComponentInstanceData(this);
	}
	
	return InstanceData;
}

void UChildActorComponent::ApplyComponentInstanceData(FChildActorComponentInstanceData* ChildActorInstanceData, const ECacheApplyPhase CacheApplyPhase)
{
	check(ChildActorInstanceData);

	ChildActorName = ChildActorInstanceData->ChildActorName;
	if (ChildActor)
	{
		// Only rename if it is safe to
		if(ChildActorName != NAME_None)
		{
			const FString ChildActorNameString = ChildActorName.ToString();
			if (ChildActor->Rename(*ChildActorNameString, nullptr, REN_Test))
			{
				ChildActor->Rename(*ChildActorNameString, nullptr, REN_DoNotDirty | (IsLoading() ? REN_ForceNoResetLoaders : REN_None));
			}
		}

		if (ChildActorInstanceData->ComponentInstanceData)
		{
			ChildActorInstanceData->ComponentInstanceData->ApplyToActor(ChildActor, CacheApplyPhase);
		}

		USceneComponent* ChildActorRoot = ChildActor->GetRootComponent();
		if (ChildActorRoot)
		{
			for (const FChildActorComponentInstanceData::FAttachedActorInfo& AttachInfo : ChildActorInstanceData->AttachedActors)
			{
				AActor* AttachedActor = AttachInfo.Actor.Get();
				if (AttachedActor)
				{
					USceneComponent* AttachedRootComponent = AttachedActor->GetRootComponent();
					if (AttachedRootComponent)
					{
						AttachedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
						AttachedRootComponent->AttachToComponent(ChildActorRoot, FAttachmentTransformRules::KeepWorldTransform, AttachInfo.SocketName);
						AttachedRootComponent->SetRelativeTransform(AttachInfo.RelativeTransform);
						AttachedRootComponent->UpdateComponentToWorld();
					}
				}
			}
		}
	}
}

void UChildActorComponent::SetChildActorClass(TSubclassOf<AActor> Class)
{
	ChildActorClass = Class;
	if (IsTemplate())
	{
		if (ChildActorClass)
		{
			if (ChildActorTemplate == nullptr || (ChildActorTemplate->GetClass() != ChildActorClass))
			{
				Modify();

				AActor* NewChildActorTemplate = NewObject<AActor>(GetTransientPackage(), ChildActorClass, NAME_None, RF_ArchetypeObject | RF_Transactional | RF_Public);

				if (ChildActorTemplate)
				{
					UEngine::CopyPropertiesForUnrelatedObjects(ChildActorTemplate, NewChildActorTemplate);

					ChildActorTemplate->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
				}

				ChildActorTemplate = NewChildActorTemplate;

				// Record initial object state in case we're in a transaction context.
				ChildActorTemplate->Modify();

				// Now set the actual name and outer to the BPGC.
				const FString TemplateName = FString::Printf(TEXT("%s_%s_CAT"), *GetName(), *ChildActorClass->GetName());

				ChildActorTemplate->Rename(*TemplateName, this, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
			}
		}
		else if (ChildActorTemplate)
		{
			Modify();

			ChildActorTemplate->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
			ChildActorTemplate = nullptr;
		}
	}
	else if (IsRegistered())
	{
		DestroyChildActor();
		CreateChildActor();
	}
}

#if WITH_EDITOR
void UChildActorComponent::PostLoad()
{
	Super::PostLoad();

	// For a period of time the parent component property on Actor was not a UPROPERTY so this value was not set
	if (ChildActor)
	{
		// Since the template could have been changed we need to respawn the child actor
		// Don't do this if there is no linker which implies component was created via duplication
		if (ChildActorTemplate && GetLinker())
		{
			bNeedsRecreate = true;
		}
		else
		{
			FActorParentComponentSetter::Set(ChildActor, this);
			ChildActor->SetFlags(RF_TextExportTransient | RF_NonPIEDuplicateTransient);
		}
	}

}
#endif

void UChildActorComponent::CreateChildActor()
{
	AActor* MyOwner = GetOwner();

	if (MyOwner && !MyOwner->HasAuthority())
	{
		AActor* ChildClassCDO = (ChildActorClass ? ChildActorClass->GetDefaultObject<AActor>() : nullptr);
		if (ChildClassCDO && ChildClassCDO->GetIsReplicated())
		{
			// If we belong to an actor that is not authoritative and the child class is replicated then we expect that Actor will be replicated across so don't spawn one
			return;
		}
	}

	// Kill spawned actor if we have one
	DestroyChildActor();

	// If we have a class to spawn.
	if(ChildActorClass != nullptr)
	{
		UWorld* World = GetWorld();
		if(World != nullptr)
		{
			// Before we spawn let's try and prevent cyclic disaster
			bool bSpawn = true;
			AActor* Actor = MyOwner;
			while (Actor && bSpawn)
			{
				if (Actor->GetClass() == ChildActorClass)
				{
					bSpawn = false;
					UE_LOG(LogChildActorComponent, Error, TEXT("Found cycle in child actor component '%s'.  Not spawning Actor of class '%s' to break."), *GetPathName(), *ChildActorClass->GetName());
				}
				Actor = Actor->GetParentActor();
			}

			if (bSpawn)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Params.bDeferConstruction = true; // We defer construction so that we set ParentComponent prior to component registration so they appear selected
				Params.bAllowDuringConstructionScript = true;
				Params.OverrideLevel = (MyOwner ? MyOwner->GetLevel() : nullptr);
				Params.Name = ChildActorName;
				if (ChildActorTemplate && ChildActorTemplate->GetClass() == ChildActorClass)
				{
					Params.Template = ChildActorTemplate;
				}
				Params.ObjectFlags |= (RF_TextExportTransient | RF_NonPIEDuplicateTransient);
				if (!HasAllFlags(RF_Transactional))
				{
					Params.ObjectFlags &= ~RF_Transactional;
				}
				if (HasAllFlags(RF_Transient))
				{
					Params.ObjectFlags |= RF_Transient;
				}

				// Spawn actor of desired class
				ConditionalUpdateComponentToWorld();
				FVector Location = GetComponentLocation();
				FRotator Rotation = GetComponentRotation();
				ChildActor = World->SpawnActor(ChildActorClass, &Location, &Rotation, Params);

				// If spawn was successful, 
				if(ChildActor != nullptr)
				{
					ChildActorName = ChildActor->GetFName();

					// Remember which component spawned it (for selection in editor etc)
					FActorParentComponentSetter::Set(ChildActor, this);

					// Parts that we deferred from SpawnActor
					const FComponentInstanceDataCache* ComponentInstanceData = (CachedInstanceData ? CachedInstanceData->ComponentInstanceData : nullptr);
					ChildActor->FinishSpawning(GetComponentTransform(), false, ComponentInstanceData);

					ChildActor->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

					SetIsReplicated(ChildActor->GetIsReplicated());

					if (CachedInstanceData)
					{
						for (const FChildActorComponentInstanceData::FAttachedActorInfo& AttachedActorInfo : CachedInstanceData->AttachedActors)
						{
							AActor* AttachedActor = AttachedActorInfo.Actor.Get();

							if (AttachedActor && AttachedActor->GetAttachParentActor() == nullptr)
							{
								AttachedActor->AttachToActor(ChildActor, FAttachmentTransformRules::KeepWorldTransform, AttachedActorInfo.SocketName);
								AttachedActor->SetActorRelativeTransform(AttachedActorInfo.RelativeTransform);
							}
						}
					}

				}
			}
		}
	}

	// This is no longer needed
	if (CachedInstanceData)
	{
		delete CachedInstanceData;
		CachedInstanceData = nullptr;
	}
}

void UChildActorComponent::DestroyChildActor()
{
	// If we own an Actor, kill it now unless we don't have authority on it, for that we rely on the server
	// If the level that the child actor is being removed then don't destory the child actor so re-adding it doesn't
	// need to create a new actor
	if (ChildActor && ChildActor->HasAuthority() && !GetOwner()->GetLevel()->bIsBeingRemoved)
	{
		if (!GExitPurge)
		{
			// if still alive, destroy, otherwise just clear the pointer
			const bool bIsChildActorPendingKillOrUnreachable = ChildActor->IsPendingKillOrUnreachable();
			if (!bIsChildActorPendingKillOrUnreachable)
			{
#if WITH_EDITOR
				if (CachedInstanceData)
				{
					delete CachedInstanceData;
					CachedInstanceData = nullptr;
				}
#else
				check(!CachedInstanceData);
#endif
				// If we're already tearing down we won't be needing this
				if (!HasAnyFlags(RF_BeginDestroyed) && !IsUnreachable())
				{
					CachedInstanceData = new FChildActorComponentInstanceData(this);
				}
			}

			UWorld* World = ChildActor->GetWorld();
			// World may be nullptr during shutdown
			if (World != nullptr)
			{
				UClass* ChildClass = ChildActor->GetClass();

				// We would like to make certain that our name is not going to accidentally get taken from us while we're destroyed
				// so we increment ClassUnique beyond our index to be certain of it.  This is ... a bit hacky.
				int32& ClassUnique = ChildActor->GetOutermost()->ClassUniqueNameIndexMap.FindOrAdd(ChildClass->GetFName());
				ClassUnique = FMath::Max(ClassUnique, ChildActor->GetFName().GetNumber());

				// If we are getting here due to garbage collection we can't rename, so we'll have to abandon this child actor name and pick up a new one
				if (!IsGarbageCollecting())
				{
					const FString ObjectBaseName = FString::Printf(TEXT("DESTROYED_%s_CHILDACTOR"), *ChildClass->GetName());
					const ERenameFlags RenameFlags = ((GetWorld()->IsGameWorld() || IsLoading()) ? REN_DoNotDirty | REN_ForceNoResetLoaders : REN_DoNotDirty);
					ChildActor->Rename(*MakeUniqueObjectName(ChildActor->GetOuter(), ChildClass, *ObjectBaseName).ToString(), nullptr, RenameFlags);
				}
				else
				{
					ChildActorName = NAME_None;
					if (CachedInstanceData)
					{
						CachedInstanceData->ChildActorName = NAME_None;
					}
				}

				if (!bIsChildActorPendingKillOrUnreachable)
				{
					World->DestroyActor(ChildActor);
				}
			}
		}

		ChildActor = nullptr;
	}
}

void UChildActorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ChildActor && !ChildActor->HasActorBegunPlay())
	{
		ChildActor->DispatchBeginPlay();
	}
}
