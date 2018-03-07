// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Stats/Stats.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectThreadContext.h"
#include "Serialization/ObjectReader.h"
#include "Engine/EngineTypes.h"
#include "Engine/Blueprint.h"
#include "ComponentInstanceDataCache.h"
#include "HAL/IConsoleManager.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BillboardComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/CullDistanceVolume.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/ChildActorComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY(LogBlueprintUserMessages);

DECLARE_CYCLE_STAT(TEXT("InstanceActorComponent"), STAT_InstanceActorComponent, STATGROUP_Engine);

//////////////////////////////////////////////////////////////////////////
// AActor Blueprint Stuff

static TArray<FRandomStream*> FindRandomStreams(AActor* InActor)
{
	check(InActor);
	TArray<FRandomStream*> OutStreams;
	UScriptStruct* RandomStreamStruct = TBaseStructure<FRandomStream>::Get();
	for( TFieldIterator<UStructProperty> It(InActor->GetClass()) ; It ; ++It )
	{
		UStructProperty* StructProp = *It;
		if( StructProp->Struct == RandomStreamStruct )
		{
			FRandomStream* StreamPtr = StructProp->ContainerPtrToValuePtr<FRandomStream>(InActor);
			OutStreams.Add(StreamPtr);
		}
	}
	return OutStreams;
}

#if WITH_EDITOR
void AActor::SeedAllRandomStreams()
{
	TArray<FRandomStream*> Streams = FindRandomStreams(this);
	for(int32 i=0; i<Streams.Num(); i++)
	{
		Streams[i]->GenerateNewSeed();
	}
}
#endif //WITH_EDITOR

void AActor::ResetPropertiesForConstruction()
{
	// Get class CDO
	AActor* Default = GetClass()->GetDefaultObject<AActor>();
	// RandomStream struct name to compare against
	const FName RandomStreamName(TEXT("RandomStream"));

	// We don't want to reset references to world object
	UWorld* World = GetWorld();
	const bool bIsLevelScriptActor = IsA<ALevelScriptActor>();
	const bool bIsPlayInEditor = World && World->IsPlayInEditor();

	// Iterate over properties
	for( TFieldIterator<UProperty> It(GetClass()) ; It ; ++It )
	{
		UProperty* Prop = *It;
		UStructProperty* StructProp = Cast<UStructProperty>(Prop);
		UClass* PropClass = CastChecked<UClass>(Prop->GetOuter()); // get the class that added this property

		// First see if it is a random stream, if so reset before running construction script
		if( (StructProp != nullptr) && (StructProp->Struct != nullptr) && (StructProp->Struct->GetFName() == RandomStreamName) )
		{
			FRandomStream* StreamPtr =  StructProp->ContainerPtrToValuePtr<FRandomStream>(this);
			StreamPtr->Reset();
		}
		// If it is a blueprint exposed variable that is not editable per-instance, reset to default before running construction script
		else if (!bIsLevelScriptActor && !Prop->ContainsInstancedObjectProperty())
		{
			const bool bExposedOnSpawn = bIsPlayInEditor && Prop->HasAnyPropertyFlags(CPF_ExposeOnSpawn);
			const bool bCanEditInstanceValue = !Prop->HasAnyPropertyFlags(CPF_DisableEditOnInstance) && Prop->HasAnyPropertyFlags(CPF_Edit);
			const bool bCanBeSetInBlueprints = Prop->HasAnyPropertyFlags(CPF_BlueprintVisible) && !Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly);

			if (!bExposedOnSpawn
				&& !bCanEditInstanceValue
				&& bCanBeSetInBlueprints
				&& !Prop->IsA<UDelegateProperty>()
				&& !Prop->IsA<UMulticastDelegateProperty>())
			{
				Prop->CopyCompleteValue_InContainer(this, Default);
			}
		}
	}
}

 int32 CalcComponentAttachDepth(UActorComponent* InComp, TMap<UActorComponent*, int32>& ComponentDepthMap) 
 {
	int32 ComponentDepth = 0;
	int32* CachedComponentDepth = ComponentDepthMap.Find(InComp);
	if (CachedComponentDepth)
	{
		ComponentDepth = *CachedComponentDepth;
	}
	else
	{
		if (USceneComponent* SC = Cast<USceneComponent>(InComp))
		{
			if (SC->GetAttachParent() && SC->GetAttachParent()->GetOwner() == InComp->GetOwner())
			{
				ComponentDepth = CalcComponentAttachDepth(SC->GetAttachParent(), ComponentDepthMap) + 1;
			}
		}
		ComponentDepthMap.Add(InComp, ComponentDepth);
	}

	return ComponentDepth;
 }

//* Destroys the constructed components.
void AActor::DestroyConstructedComponents()
{
	// Remove all existing components
	TInlineComponentArray<UActorComponent*> PreviouslyAttachedComponents;
	GetComponents(PreviouslyAttachedComponents);

	TMap<UActorComponent*, int32> ComponentDepthMap;

	for (UActorComponent* Component : PreviouslyAttachedComponents)
	{
		if (Component)
		{
			CalcComponentAttachDepth(Component, ComponentDepthMap);
		}
	}

	ComponentDepthMap.ValueSort([](const int32& A, const int32& B)
	{
		return A > B;
	});

	for (const TPair<UActorComponent*,int32>& ComponentAndDepth : ComponentDepthMap)
	{
		UActorComponent* Component = ComponentAndDepth.Key;
		if (Component)
		{
			bool bDestroyComponent = false;
			if (Component->IsCreatedByConstructionScript())
			{
				bDestroyComponent = true;
			}
			else
			{
				UActorComponent* OuterComponent = Component->GetTypedOuter<UActorComponent>();
				while (OuterComponent)
				{
					if (OuterComponent->IsCreatedByConstructionScript())
					{
						bDestroyComponent = true;
						break;
					}
					OuterComponent = OuterComponent->GetTypedOuter<UActorComponent>();
				}
			}

			if (bDestroyComponent)
			{
				if (Component == RootComponent)
				{
					RootComponent = NULL;
				}

				Component->DestroyComponent();

				// Rename component to avoid naming conflicts in the case where we rerun the SCS and name the new components the same way.
				FName const NewBaseName( *(FString::Printf(TEXT("TRASH_%s"), *Component->GetClass()->GetName())) );
				FName const NewObjectName = MakeUniqueObjectName(this, GetClass(), NewBaseName);
				Component->Rename(*NewObjectName.ToString(), this, REN_ForceNoResetLoaders|REN_DontCreateRedirectors|REN_NonTransactional);
			}
		}
	}
}

void AActor::RerunConstructionScripts()
{
	checkf(!HasAnyFlags(RF_ClassDefaultObject), TEXT("RerunConstructionScripts should never be called on a CDO as it can mutate the transient data on the CDO which then propagates to instances!"));

	FEditorScriptExecutionGuard ScriptGuard;
	// don't allow (re)running construction scripts on dying actors and Actors that seamless traveled 
	// were constructed in the previous level and should not have construction scripts rerun
	bool bAllowReconstruction =  !bActorSeamlessTraveled && !IsPendingKill() && !HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed);
#if WITH_EDITOR
	if(bAllowReconstruction && GIsEditor)
	{
		// Generate the blueprint hierarchy for this actor
		TArray<UBlueprint*> ParentBPStack;
		bAllowReconstruction = UBlueprint::GetBlueprintHierarchyFromClass(GetClass(), ParentBPStack);
		if(bAllowReconstruction)
		{
			for(int i = ParentBPStack.Num() - 1; i > 0 && bAllowReconstruction; --i)
			{
				const UBlueprint* ParentBP = ParentBPStack[i];
				if(ParentBP && ParentBP->bBeingCompiled)
				{
					// don't allow (re)running construction scripts if a parent BP is being compiled
					bAllowReconstruction = false;
				}
			}
		}
	}
#endif
	if(bAllowReconstruction)
	{
		// Child Actors can be customized in many ways by their parents construction scripts and rerunning directly on them would wipe
		// that out. So instead we redirect up the hierarchy
		if (IsChildActor())
		{
			if (AActor* ParentActor = GetParentComponent()->GetOwner())
			{
				ParentActor->RerunConstructionScripts();
				return;
			}
		}

		// Set global flag to let system know we are reconstructing blueprint instances
		TGuardValue<bool> GuardTemplateNameFlag(GIsReconstructingBlueprintInstances, true);

		// Temporarily suspend the undo buffer; we don't need to record reconstructed component objects into the current transaction
		ITransaction* CurrentTransaction = GUndo;
		GUndo = nullptr;
		
		// Create cache to store component data across rerunning construction scripts
		FComponentInstanceDataCache* InstanceDataCache;
		
		FTransform OldTransform = FTransform::Identity;
		FRotationConversionCache OldTransformRotationCache; // Enforces using the same Rotator.
		FName  SocketName;
		AActor* Parent = nullptr;
		USceneComponent* AttachParentComponent = nullptr;

		bool bUseRootComponentProperties = true;

		// Struct to store info about attached actors
		struct FAttachedActorInfo
		{
			AActor* AttachedActor;
			FName AttachedToSocket;
			bool bSetRelativeTransform;
			FTransform RelativeTransform;
		};

		// Save info about attached actors
		TArray<FAttachedActorInfo> AttachedActorInfos;

#if WITH_EDITOR
		if (!CurrentTransactionAnnotation.IsValid())
		{
			CurrentTransactionAnnotation = MakeShareable(new FActorTransactionAnnotation(this, false));
		}
		FActorTransactionAnnotation* ActorTransactionAnnotation = CurrentTransactionAnnotation.Get();
		InstanceDataCache = &ActorTransactionAnnotation->ComponentInstanceData;

		if (ActorTransactionAnnotation->bRootComponentDataCached)
		{
			OldTransform = ActorTransactionAnnotation->RootComponentData.Transform;
			OldTransformRotationCache = ActorTransactionAnnotation->RootComponentData.TransformRotationCache;
			Parent = ActorTransactionAnnotation->RootComponentData.AttachedParentInfo.Actor.Get();
			if (Parent)
			{
				USceneComponent* AttachParent = ActorTransactionAnnotation->RootComponentData.AttachedParentInfo.AttachParent.Get();
				AttachParentComponent = (AttachParent ? AttachParent : FindObjectFast<USceneComponent>(Parent, ActorTransactionAnnotation->RootComponentData.AttachedParentInfo.AttachParentName));
				SocketName = ActorTransactionAnnotation->RootComponentData.AttachedParentInfo.SocketName;
				DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			}

			for (const FActorRootComponentReconstructionData::FAttachedActorInfo& CachedAttachInfo : ActorTransactionAnnotation->RootComponentData.AttachedToInfo)
			{
				AActor* AttachedActor = CachedAttachInfo.Actor.Get();
				if (AttachedActor)
				{
					FAttachedActorInfo Info;
					Info.AttachedActor = AttachedActor;
					Info.AttachedToSocket = CachedAttachInfo.SocketName;
					Info.bSetRelativeTransform = true;
					Info.RelativeTransform = CachedAttachInfo.RelativeTransform;
					AttachedActorInfos.Add(Info);

					AttachedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				}
			}

			bUseRootComponentProperties = false;
		}
#else
		InstanceDataCache = new FComponentInstanceDataCache(this);
#endif

		if (bUseRootComponentProperties)
		{
			// If there are attached objects detach them and store the socket names
			TArray<AActor*> AttachedActors;
			GetAttachedActors(AttachedActors);

			for (AActor* AttachedActor : AttachedActors)
			{
				// We don't need to detach child actors, that will be handled by component tear down
				if (!AttachedActor->IsChildActor())
				{
					USceneComponent* EachRoot = AttachedActor->GetRootComponent();
					// If the component we are attached to is about to go away...
					if (EachRoot && EachRoot->GetAttachParent() && EachRoot->GetAttachParent()->IsCreatedByConstructionScript())
					{
						// Save info about actor to reattach
						FAttachedActorInfo Info;
						Info.AttachedActor = AttachedActor;
						Info.AttachedToSocket = EachRoot->GetAttachSocketName();
						Info.bSetRelativeTransform = false;
						AttachedActorInfos.Add(Info);

						// Now detach it
						AttachedActor->Modify();
						EachRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
					}
				}
				else
				{
					check(AttachedActor->ParentComponent->GetOwner() == this);
				}
			}

			if (RootComponent != nullptr)
			{
				// Do not need to detach if root component is not going away
				if (RootComponent->GetAttachParent() != nullptr && RootComponent->IsCreatedByConstructionScript())
				{
					Parent = RootComponent->GetAttachParent()->GetOwner();
					// Root component should never be attached to another component in the same actor!
					if (Parent == this)
					{
						UE_LOG(LogActor, Warning, TEXT("RerunConstructionScripts: RootComponent (%s) attached to another component in this Actor (%s)."), *RootComponent->GetPathName(), *Parent->GetPathName());
						Parent = nullptr;
					}
					AttachParentComponent = RootComponent->GetAttachParent();
					SocketName = RootComponent->GetAttachSocketName();
					//detach it to remove any scaling 
					RootComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
				}

				// Update component transform and remember it so it can be reapplied to any new root component which exists after construction.
				// (Component transform may be stale if we are here following an Undo)
				RootComponent->UpdateComponentToWorld();
				OldTransform = RootComponent->GetComponentTransform();
				OldTransformRotationCache = RootComponent->GetRelativeRotationCache();
			}
		}

#if WITH_EDITOR

		// Return the component which was added by the construction script.
		// It may be the same as the argument, or a parent component if the argument was a native subobject.
		auto GetComponentAddedByConstructionScript = [](UActorComponent* Component) -> UActorComponent*
		{
			while (Component)
			{
				if (Component->IsCreatedByConstructionScript())
				{
					return Component;
				}

				Component = Component->GetTypedOuter<UActorComponent>();
			}

			return nullptr;
		};

		// Build a list of previously attached components which will be matched with their newly instanced counterparts.
		// Components which will be reinstanced may be created by the SCS or the UCS.
		// SCS components can only be matched by name, and outermost parent to resolve duplicated names.
		// UCS components remember a serialized index which is used to identify them in the case that the UCS adds many of the same type.

		TInlineComponentArray<UActorComponent*> PreviouslyAttachedComponents;
		GetComponents(PreviouslyAttachedComponents);

		struct FComponentData
		{
			UActorComponent* OldComponent;
			UActorComponent* OldOuter;
			UObject* OldArchetype;
			FName OldName;
			int32 UCSComponentIndex;
		};

		TArray<FComponentData> ComponentMapping;
		ComponentMapping.Reserve(PreviouslyAttachedComponents.Num());
		int32 IndexOffset = 0;

		for (UActorComponent* Component : PreviouslyAttachedComponents)
		{
			// Look for the outermost component object.
			// Normally components have their parent actor as their outer, but it's possible that a native component may construct a subobject component.
			// In this case we need to "tunnel out" to find the parent component which has been created by the construction script.
			if (UActorComponent* CSAddedComponent = GetComponentAddedByConstructionScript(Component))
			{
				// Determine if this component is an inner of a component added by the construction script
				const bool bIsInnerComponent = (CSAddedComponent != Component);

				// Poor man's topological sort - try to ensure that children are added to the list after the parents
				// IndexOffset specifies how many items from the end new items are added.
				const int32 Index = ComponentMapping.Num() - IndexOffset;
				if (bIsInnerComponent)
				{
					int32 OuterIndex = ComponentMapping.IndexOfByPredicate([CSAddedComponent](const FComponentData& CD) { return CD.OldComponent == CSAddedComponent; });
					if (OuterIndex == INDEX_NONE)
					{
						// If we find an item whose parent isn't yet in the list, we put it at the end, and then force all subsequent items to be added before.
						// TODO: improve this, it may fail in certain circumstances, but a full topological ordering is far more complicated a problem.
						IndexOffset++;
					}
				}

				// Add a new item
				ComponentMapping.Insert(FComponentData(), Index);
				ComponentMapping[Index].OldComponent = Component;
				ComponentMapping[Index].OldOuter = bIsInnerComponent ? CSAddedComponent : nullptr;
				ComponentMapping[Index].OldArchetype = Component->GetArchetype();
				ComponentMapping[Index].OldName = Component->GetFName();

				// If it's a UCS-created component, store a serialized index which will be used to match it to the reinstanced counterpart later
				int32 SerializedIndex = -1;
				if (Component->CreationMethod == EComponentCreationMethod::UserConstructionScript)
				{
					bool bFound = false;
					for (const UActorComponent* BlueprintCreatedComponent : BlueprintCreatedComponents)
					{
						if (BlueprintCreatedComponent)
						{
							if (BlueprintCreatedComponent == Component)
							{
								SerializedIndex++;
								bFound = true;
								break;
							}
							else if (BlueprintCreatedComponent->CreationMethod == EComponentCreationMethod::UserConstructionScript &&
									 BlueprintCreatedComponent->GetArchetype() == ComponentMapping[Index].OldArchetype)
							{
								SerializedIndex++;
							}
						}
					}

					if (!bFound)
					{
						SerializedIndex = -1;
					}
				}

				ComponentMapping[Index].UCSComponentIndex = SerializedIndex;
			}
		}
#endif

		// Destroy existing components
		DestroyConstructedComponents();

		// Reset random streams
		ResetPropertiesForConstruction();

		// Exchange net roles before running construction scripts
		UWorld *OwningWorld = GetWorld();
		if (OwningWorld && !OwningWorld->IsServer())
		{
			ExchangeNetRoles(true);
		}

		// Run the construction scripts
		const bool bErrorFree = ExecuteConstruction(OldTransform, &OldTransformRotationCache, InstanceDataCache);

		if(Parent)
		{
			USceneComponent* ChildRoot = GetRootComponent();
			if (AttachParentComponent == nullptr)
			{
				AttachParentComponent = Parent->GetRootComponent();
			}
			if (ChildRoot != nullptr && AttachParentComponent != nullptr)
			{
				ChildRoot->AttachToComponent(AttachParentComponent, FAttachmentTransformRules::KeepWorldTransform, SocketName);
			}
		}

		// If we had attached children reattach them now - unless they are already attached
		for(FAttachedActorInfo& Info : AttachedActorInfos)
		{
			// If this actor is no longer attached to anything, reattach
			if (!Info.AttachedActor->IsPendingKill() && Info.AttachedActor->GetAttachParentActor() == nullptr)
			{
				USceneComponent* ChildRoot = Info.AttachedActor->GetRootComponent();
				if (ChildRoot && ChildRoot->GetAttachParent() != RootComponent)
				{
					ChildRoot->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform, Info.AttachedToSocket);
					if (Info.bSetRelativeTransform)
					{
						ChildRoot->SetRelativeTransform(Info.RelativeTransform);
					}
					ChildRoot->UpdateComponentToWorld();
				}
			}
		}

		// Restore the undo buffer
		GUndo = CurrentTransaction;

#if WITH_EDITOR
		// Create the mapping of old->new components and notify the editor of the replacements
		TInlineComponentArray<UActorComponent*> NewComponents;
		GetComponents(NewComponents);

		TMap<UObject*, UObject*> OldToNewComponentMapping;
		OldToNewComponentMapping.Reserve(NewComponents.Num());

		// Build some quick lookup maps for speedy access.
		// The NameToNewComponent map is a multimap because names are not necessarily unique.
		// For example, there may be two components, subobjects of components added by the construction script, which have the same name, because they are unique in their scope.
		TMultiMap<FName, UActorComponent*> NameToNewComponent;
		TMap<UActorComponent*, UObject*> ComponentToArchetypeMap;
		NameToNewComponent.Reserve(NewComponents.Num());
		ComponentToArchetypeMap.Reserve(NewComponents.Num());

		for (UActorComponent* NewComponent : NewComponents)
		{
			if (GetComponentAddedByConstructionScript(NewComponent))
			{
				NameToNewComponent.Add(NewComponent->GetFName(), NewComponent);
				ComponentToArchetypeMap.Add(NewComponent, NewComponent->GetArchetype());
			}
		}

		// Now iterate through all previous construction script created components, looking for a match with reinstanced components.
		for (const FComponentData& ComponentData : ComponentMapping)
		{
			if (ComponentData.OldComponent->CreationMethod == EComponentCreationMethod::UserConstructionScript)
			{
				// If created by the UCS, look for a component whose class, archetype and serialized index matches
				for (UActorComponent* NewComponent : NewComponents)
				{
					if (NewComponent->CreationMethod == EComponentCreationMethod::UserConstructionScript &&
						ComponentData.OldComponent->GetClass() == NewComponent->GetClass() &&
						ComponentData.OldArchetype == NewComponent->GetArchetype() &&
						ComponentData.UCSComponentIndex >= 0)
					{
						int32 FoundSerializedIndex = -1;
						bool bMatches = false;
						for (UActorComponent* BlueprintCreatedComponent : BlueprintCreatedComponents)
						{
							if (BlueprintCreatedComponent && BlueprintCreatedComponent->CreationMethod == EComponentCreationMethod::UserConstructionScript)
							{
								UObject* BlueprintComponentTemplate = ComponentToArchetypeMap.FindRef(BlueprintCreatedComponent);
								if (BlueprintComponentTemplate &&
									ComponentData.OldArchetype == BlueprintComponentTemplate &&
									++FoundSerializedIndex == ComponentData.UCSComponentIndex)
								{
									bMatches = (BlueprintCreatedComponent == NewComponent);
									break;
								}
							}
						}

						if (bMatches)
						{
							OldToNewComponentMapping.Add(ComponentData.OldComponent, NewComponent);
							break;
						}
					}
				}
			}
			else
			{
				// Component added by the SCS. We can't rely on serialization order as this can change.
				// Instead look for matching names, and, if there's an outer component, look for a match there.
				TArray<UActorComponent*> MatchedComponents;
				NameToNewComponent.MultiFind(ComponentData.OldName, MatchedComponents);
				if (MatchedComponents.Num() > 0)
				{
					UActorComponent* OuterToMatch = ComponentData.OldOuter;
					if (OuterToMatch)
					{
						// The saved outer component is the previous component, hence we transform it to the new one through the OldToNewComponentMapping
						// before comparing with the new outer to match.
						// We can rely on this because the ComponentMapping list is ordered topologically, such that parents appear before children.
						if (UObject** NewOuterToMatch = OldToNewComponentMapping.Find(OuterToMatch))
						{
							OuterToMatch = Cast<UActorComponent>(*NewOuterToMatch);
						}
						else
						{
							OuterToMatch = nullptr;
						}
					}

					// Now look for a match within the set of possible matches
					for (UActorComponent* MatchedComponent : MatchedComponents)
					{
						if (!OuterToMatch || GetComponentAddedByConstructionScript(MatchedComponent) == OuterToMatch)
						{
							OldToNewComponentMapping.Add(ComponentData.OldComponent, MatchedComponent);
							break;
						}
					}
				}
			}
		}

		if (GEditor && (OldToNewComponentMapping.Num() > 0))
		{
			GEditor->NotifyToolsOfObjectReplacement(OldToNewComponentMapping);
		}

		if (bErrorFree)
		{
			CurrentTransactionAnnotation = nullptr;
		}
#else
		delete InstanceDataCache;
#endif

	}
}

bool AActor::ExecuteConstruction(const FTransform& Transform, const FRotationConversionCache* TransformRotationCache, const FComponentInstanceDataCache* InstanceDataCache, bool bIsDefaultTransform)
{
	check(!IsPendingKill());
	check(!HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed));

	// ensure that any existing native root component gets this new transform
	// we can skip this in the default case as the given transform will be the root component's transform
	if (RootComponent && !bIsDefaultTransform)
	{
		if (TransformRotationCache)
		{
			RootComponent->SetRelativeRotationCache(*TransformRotationCache);
		}
		RootComponent->SetWorldTransform(Transform);
	}

	// Generate the parent blueprint hierarchy for this actor, so we can run all the construction scripts sequentially
	TArray<const UBlueprintGeneratedClass*> ParentBPClassStack;
	const bool bErrorFree = UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(GetClass(), ParentBPClassStack);

	TArray<const UDynamicClass*> ParentDynamicClassStack;
	for (UClass* ClassIt = GetClass(); ClassIt; ClassIt = ClassIt->GetSuperClass())
	{
		if (UDynamicClass* DynamicClass = Cast<UDynamicClass>(ClassIt))
		{
			ParentDynamicClassStack.Add(DynamicClass);
		}
	}
	for (int32 i = ParentDynamicClassStack.Num() - 1; i >= 0; i--)
	{
		UBlueprintGeneratedClass::CreateComponentsForActor(ParentDynamicClassStack[i], this);
	}

	// If this actor has a blueprint lineage, go ahead and run the construction scripts from least derived to most
	if( (ParentBPClassStack.Num() > 0)  )
	{
		if (bErrorFree)
		{
			// Get all components owned by the given actor prior to SCS execution.
			// Note: GetComponents() internally does a NULL check, so we can assume here that all entries are valid.
			TInlineComponentArray<UActorComponent*> PreSCSComponents;
			GetComponents(PreSCSComponents);

			// Determine the set of native scene components that SCS nodes can attach to.
			TInlineComponentArray<USceneComponent*> NativeSceneComponents;
			for (UActorComponent* ActorComponent : PreSCSComponents)
			{
				if (USceneComponent* SceneComponent = Cast<USceneComponent>(ActorComponent))
				{
					// Exclude subcomponents of native components, as these could unintentionally be matched by name during SCS execution. Also exclude instance-only components.
					if (SceneComponent->CreationMethod == EComponentCreationMethod::Native && SceneComponent->GetOuter()->IsA<AActor>())
					{
						// If RootComponent is not set, the first unattached native scene component will be used as root. This matches what's done in FixupNativeActorComponents().
						// @TODO - consider removing this; keeping here as a fallback just in case it wasn't set prior to SCS execution, but in most cases now this should be valid. 
						if (RootComponent == nullptr && SceneComponent->GetAttachParent() == nullptr)
						{
							// Note: All native scene components should already have been registered at this point, so we don't need to register the component here.
							SetRootComponent(SceneComponent);
						}

						NativeSceneComponents.Add(SceneComponent);
					}
				}
			}

			// Prevent user from spawning actors in User Construction Script
			TGuardValue<bool> AutoRestoreISCS(GetWorld()->bIsRunningConstructionScript, true);
			for (int32 i = ParentBPClassStack.Num() - 1; i >= 0; i--)
			{
				const UBlueprintGeneratedClass* CurrentBPGClass = ParentBPClassStack[i];
				check(CurrentBPGClass);
				USimpleConstructionScript* SCS = CurrentBPGClass->SimpleConstructionScript;
				if (SCS)
				{
					SCS->CreateNameToSCSNodeMap();
					SCS->ExecuteScriptOnActor(this, NativeSceneComponents, Transform, TransformRotationCache, bIsDefaultTransform);
				}
				// Now that the construction scripts have been run, we can create timelines and hook them up
				UBlueprintGeneratedClass::CreateComponentsForActor(CurrentBPGClass, this);
			}

			// Ensure that we've called RegisterAllComponents(), in case it was deferred and the SCS could not be fully executed.
			if (HasDeferredComponentRegistration())
			{
				RegisterAllComponents();
			}

			// Once SCS execution has finished, we do a final pass to register any new components that may have been deferred or were otherwise left unregistered after SCS execution.
			TInlineComponentArray<UActorComponent*> PostSCSComponents;
			GetComponents(PostSCSComponents);
			for (UActorComponent* ActorComponent : PostSCSComponents)
			{
				// Limit registration to components that are known to have been created during SCS execution
				if (!ActorComponent->IsRegistered() && ActorComponent->bAutoRegister && !ActorComponent->IsPendingKill()
					&& (ActorComponent->CreationMethod == EComponentCreationMethod::SimpleConstructionScript || !PreSCSComponents.Contains(ActorComponent)))
				{
					USimpleConstructionScript::RegisterInstancedComponent(ActorComponent);
				}
			}

			// If we passed in cached data, we apply it now, so that the UserConstructionScript can use the updated values
			if (InstanceDataCache)
			{
				InstanceDataCache->ApplyToActor(this, ECacheApplyPhase::PostSimpleConstructionScript);
			}

#if WITH_EDITOR
			bool bDoUserConstructionScript;
			GConfig->GetBool(TEXT("Kismet"), TEXT("bTurnOffEditorConstructionScript"), bDoUserConstructionScript, GEngineIni);
			if (!GIsEditor || !bDoUserConstructionScript)
#endif
			{
				// Then run the user script, which is responsible for calling its own super, if desired
				ProcessUserConstructionScript();
			}

			// Since re-run construction scripts will never be run and we want to keep dynamic spawning fast, don't spend time
			// determining the UCS modified properties in game worlds
			if (!GetWorld()->IsGameWorld())
			{
				for (UActorComponent* Component : GetComponents())
				{
					if (Component)
					{
						Component->DetermineUCSModifiedProperties();
					}
				}
			}

			// Bind any delegates on components			
			UBlueprintGeneratedClass::BindDynamicDelegates(GetClass(), this); // We have a BP stack, we must have a UBlueprintGeneratedClass...

			// Apply any cached data procedural components
			// @TODO Don't re-apply to components we already applied to above
			if (InstanceDataCache)
			{
				InstanceDataCache->ApplyToActor(this, ECacheApplyPhase::PostUserConstructionScript);
			}

			// Remove name to SCS_Node cached map
			for (const UBlueprintGeneratedClass* CurrentBPGClass : ParentBPClassStack)
			{
				check(CurrentBPGClass);
				USimpleConstructionScript* SCS = CurrentBPGClass->SimpleConstructionScript;
				if (SCS)
				{
					SCS->RemoveNameToSCSNodeMap();
				}
			}
		}
		else
		{
			// Disaster recovery mode; create a dummy billboard component to retain the actor location
			// until the compile error can be fixed
			if (RootComponent == nullptr)
			{
				UBillboardComponent* BillboardComponent = NewObject<UBillboardComponent>(this);
				BillboardComponent->SetFlags(RF_Transactional);
				BillboardComponent->CreationMethod = EComponentCreationMethod::SimpleConstructionScript;
#if WITH_EDITOR
				BillboardComponent->Sprite = (UTexture2D*)(StaticLoadObject(UTexture2D::StaticClass(), nullptr, TEXT("/Engine/EditorResources/BadBlueprintSprite.BadBlueprintSprite")));
#endif
				BillboardComponent->SetRelativeTransform(Transform);

				SetRootComponent(BillboardComponent);
				FinishAndRegisterComponent(BillboardComponent);
			}

			// Ensure that we've called RegisterAllComponents(), in case it was deferred and the SCS could not be executed (due to error).
			if (HasDeferredComponentRegistration())
			{
				RegisterAllComponents();
			}
		}
	}
	else
	{
#if WITH_EDITOR
		bool bDoUserConstructionScript;
		GConfig->GetBool(TEXT("Kismet"), TEXT("bTurnOffEditorConstructionScript"), bDoUserConstructionScript, GEngineIni);
		if (!GIsEditor || !bDoUserConstructionScript)
#endif
		{
			// Then run the user script, which is responsible for calling its own super, if desired
			ProcessUserConstructionScript();
		}
		UBlueprintGeneratedClass::BindDynamicDelegates(GetClass(), this);
	}

	GetWorld()->UpdateCullDistanceVolumes(this);

	// Now run virtual notification
	OnConstruction(Transform);

	return bErrorFree;
}

void AActor::ProcessUserConstructionScript()
{
	// Set a flag that this actor is currently running UserConstructionScript.
	bRunningUserConstructionScript = true;
	UserConstructionScript();
	bRunningUserConstructionScript = false;

	// Validate component mobility after UCS execution
	for (UActorComponent* Component : GetComponents())
	{
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			// A parent component can't be more mobile than its children, so we check for that here and adjust as needed.
			if (SceneComponent != RootComponent && SceneComponent->GetAttachParent() != nullptr && SceneComponent->GetAttachParent()->Mobility > SceneComponent->Mobility)
			{
				if (SceneComponent->IsA<UStaticMeshComponent>())
				{
					// SMCs can't be stationary, so always set them (and any children) to be movable
					SceneComponent->SetMobility(EComponentMobility::Movable);
				}
				else
				{
					// Set the new component (and any children) to be at least as mobile as its parent
					SceneComponent->SetMobility(SceneComponent->GetAttachParent()->Mobility);
				}
			}
		}
	}
}

void AActor::FinishAndRegisterComponent(UActorComponent* Component)
{
	Component->RegisterComponent();
	BlueprintCreatedComponents.Add(Component);
}

UActorComponent* AActor::CreateComponentFromTemplate(UActorComponent* Template, const FString& InName)
{
	return CreateComponentFromTemplate(Template, FName(*InName));
}

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarLogBlueprintComponentInstanceCalls(
	TEXT("LogBlueprintComponentInstanceCalls"),
	0,
	TEXT("Log Blueprint Component instance calls; debugging."));
#endif

UActorComponent* AActor::CreateComponentFromTemplate(UActorComponent* Template, const FName InName)
{
	SCOPE_CYCLE_COUNTER(STAT_InstanceActorComponent);

	UActorComponent* NewActorComp = nullptr;
	if (Template != nullptr)
	{
#if !UE_BUILD_SHIPPING
		const double StartTime = FPlatformTime::Seconds();
#endif
		// Make sure, that the name of the instance is different than the name of the template. This ensures that archetypes will not be recycled as instances in the nativized case.
		const FName NewComponentName = (InName != NAME_None) ? InName : MakeUniqueObjectName(this, Template->GetClass(), Template->GetFName());
		ensure(NewComponentName != Template->GetFName());

		// Resolve any name conflicts.
		CheckComponentInstanceName(NewComponentName);

		// Note we aren't copying the the RF_ArchetypeObject flag. Also note the result is non-transactional by default.
		NewActorComp = (UActorComponent*)StaticDuplicateObject(Template, this, NewComponentName, RF_AllFlags & ~(RF_ArchetypeObject | RF_Transactional | RF_WasLoaded | RF_Public | RF_InheritableComponentTemplate));

		// Handle post-creation tasks.
		PostCreateBlueprintComponent(NewActorComp);

#if !UE_BUILD_SHIPPING
		if (CVarLogBlueprintComponentInstanceCalls.GetValueOnGameThread())
		{
			UE_LOG(LogBlueprint, Log, TEXT("%s: CreateComponentFromTemplate() - %s \'%s\' completed in %.02g ms"), *GetName(), *Template->GetClass()->GetName(), *NewComponentName.ToString(), (FPlatformTime::Seconds() - StartTime) * 1000.0);
		}
#endif
	}
	return NewActorComp;
}

UActorComponent* AActor::CreateComponentFromTemplateData(const FBlueprintCookedComponentInstancingData* TemplateData, const FName InName)
{
	SCOPE_CYCLE_COUNTER(STAT_InstanceActorComponent);

	// Component instance data loader implementation.
	class FBlueprintComponentInstanceDataLoader : public FObjectReader
	{
	public:
		FBlueprintComponentInstanceDataLoader(const TArray<uint8>& InSrcBytes, const FCustomPropertyListNode* InPropertyList)
			:FObjectReader(const_cast<TArray<uint8>&>(InSrcBytes))
		{
			ArCustomPropertyList = InPropertyList;
			ArUseCustomPropertyList = true;
			ArWantBinaryPropertySerialization = true;

			// Set this flag to emulate things that would happen in the SDO case when this flag is set (e.g. - not setting 'bHasBeenCreated').
			ArPortFlags |= PPF_Duplicate;
		}
	};

	UActorComponent* NewActorComp = nullptr;
	if (TemplateData != nullptr && TemplateData->ComponentTemplateClass != nullptr)	// some components (e.g. UTextRenderComponent) are not loaded on a server (or client). Handle that gracefully, but we ideally shouldn't even get here (see UEBP-175).
	{
#if !UE_BUILD_SHIPPING
		const double StartTime = FPlatformTime::Seconds();
#endif
		// Make sure, that the name of the instance is different than the name of the template. This ensures that archetypes will not be recycled as instances in the nativized case.
		const FName NewComponentName = (InName != NAME_None) ? InName : MakeUniqueObjectName(this, TemplateData->ComponentTemplateClass, TemplateData->ComponentTemplateName);
		ensure(NewComponentName != TemplateData->ComponentTemplateName);

		// Resolve any name conflicts.
		CheckComponentInstanceName(NewComponentName);

		// Note we aren't copying the the RF_ArchetypeObject flag. Also note the result is non-transactional by default.
		NewActorComp = NewObject<UActorComponent>(
			this,
			TemplateData->ComponentTemplateClass,
			NewComponentName,
			EObjectFlags(TemplateData->ComponentTemplateFlags) & ~(RF_ArchetypeObject | RF_Transactional | RF_WasLoaded | RF_Public | RF_InheritableComponentTemplate)
		);

		// Set these flags to match what SDO would otherwise do before serialization to enable post-duplication logic on the destination object.
		NewActorComp->SetFlags(RF_NeedPostLoad | RF_NeedPostLoadSubobjects);

		// Load cached data into the new instance.
		FBlueprintComponentInstanceDataLoader ComponentInstanceDataLoader(TemplateData->GetCachedPropertyDataForSerialization(), TemplateData->GetCachedPropertyListForSerialization());
		NewActorComp->Serialize(ComponentInstanceDataLoader);

		// Handle tasks that would normally occur post-duplication w/ SDO.
		NewActorComp->PostDuplicate(EDuplicateMode::Normal);
		{
			TGuardValue<bool> GuardIsRoutingPostLoad(FUObjectThreadContext::Get().IsRoutingPostLoad, true);
			NewActorComp->ConditionalPostLoad();
		}

		// Handle post-creation tasks.
		PostCreateBlueprintComponent(NewActorComp);

#if !UE_BUILD_SHIPPING
		if (CVarLogBlueprintComponentInstanceCalls.GetValueOnGameThread())
		{
			UE_LOG(LogBlueprint, Log, TEXT("%s: CreateComponentFromTemplateData() - %s \'%s\' completed in %.02g ms"), *GetName(), *TemplateData->ComponentTemplateClass->GetName(), *NewComponentName.ToString(), (FPlatformTime::Seconds() - StartTime) * 1000.0);
		}
#endif
	}
	return NewActorComp;
}

UActorComponent* AActor::AddComponent(FName TemplateName, bool bManualAttachment, const FTransform& RelativeTransform, const UObject* ComponentTemplateContext)
{
	if (GetWorld()->bIsTearingDown)
	{
		UE_LOG(LogActor, Warning, TEXT("AddComponent failed because we are in the process of tearing down the world"));
		return nullptr;
	}

	UActorComponent* Template = nullptr;
	FBlueprintCookedComponentInstancingData* TemplateData = nullptr;
	for (UClass* TemplateOwnerClass = (ComponentTemplateContext != nullptr) ? ComponentTemplateContext->GetClass() : GetClass()
		; TemplateOwnerClass && !Template && !TemplateData
		; TemplateOwnerClass = TemplateOwnerClass->GetSuperClass())
	{
		if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(TemplateOwnerClass))
		{
			// Use cooked instancing data if available (fast path).
			if (FPlatformProperties::RequiresCookedData())
			{
				TemplateData = BPGC->CookedComponentInstancingData.Find(TemplateName);
			}
			
			if (!TemplateData || !TemplateData->bIsValid)
			{
				Template = BPGC->FindComponentTemplateByName(TemplateName);
			}
		}
		else if (UDynamicClass* DynamicClass = Cast<UDynamicClass>(TemplateOwnerClass))
		{
			UObject** FoundTemplatePtr = DynamicClass->ComponentTemplates.FindByPredicate([=](UObject* Obj) -> bool
			{
				return Obj && Obj->IsA<UActorComponent>() && (Obj->GetFName() == TemplateName);
			});
			Template = (nullptr != FoundTemplatePtr) ? Cast<UActorComponent>(*FoundTemplatePtr) : nullptr;
		}
	}

	bool bIsSceneComponent = false;
	UActorComponent* NewActorComp = TemplateData ? CreateComponentFromTemplateData(TemplateData) : CreateComponentFromTemplate(Template);
	if(NewActorComp != nullptr)
	{
		// Call function to notify component it has been created
		NewActorComp->OnComponentCreated();
		
		// The user has the option of doing attachment manually where they have complete control or via the automatic rule
		// that the first component added becomes the root component, with subsequent components attached to the root.
		USceneComponent* NewSceneComp = Cast<USceneComponent>(NewActorComp);
		if(NewSceneComp != nullptr)
		{
			if (!bManualAttachment)
			{
				if (RootComponent == nullptr)
				{
					RootComponent = NewSceneComp;
				}
				else
				{
					NewSceneComp->SetupAttachment(RootComponent);
				}
			}

			NewSceneComp->SetRelativeTransform(RelativeTransform);

			bIsSceneComponent = true;
		}

		// Register component, which will create physics/rendering state, now component is in correct position
		if (NewActorComp->bAutoRegister)
		{
			NewActorComp->RegisterComponent();
		}

		UWorld* World = GetWorld();
		if (!bRunningUserConstructionScript && World && bIsSceneComponent)
		{
			UPrimitiveComponent* NewPrimitiveComponent = Cast<UPrimitiveComponent>(NewActorComp);
			if (NewPrimitiveComponent && ACullDistanceVolume::CanBeAffectedByVolumes(NewPrimitiveComponent))
			{
				World->UpdateCullDistanceVolumes(this, NewPrimitiveComponent);
			}
		}
	}

	return NewActorComp;
}

void AActor::CheckComponentInstanceName(const FName InName)
{
	// If there is a Component with this name already (almost certainly because it is an Instance component), we need to rename it out of the way
	if (!InName.IsNone())
	{
		UObject* ConflictingObject = FindObjectFast<UObject>(this, InName);
		if (ConflictingObject && ConflictingObject->IsA<UActorComponent>() && CastChecked<UActorComponent>(ConflictingObject)->CreationMethod == EComponentCreationMethod::Instance)
		{
			// Try and pick a good name
			FString ConflictingObjectName = ConflictingObject->GetName();
			int32 CharIndex = ConflictingObjectName.Len() - 1;
			while (FChar::IsDigit(ConflictingObjectName[CharIndex]))
			{
				--CharIndex;
			}
			int32 Counter = 0;
			if (CharIndex < ConflictingObjectName.Len() - 1)
			{
				Counter = FCString::Atoi(*ConflictingObjectName.RightChop(CharIndex + 1));
				ConflictingObjectName = ConflictingObjectName.Left(CharIndex + 1);
			}
			FString NewObjectName;
			do
			{
				NewObjectName = ConflictingObjectName + FString::FromInt(++Counter);

			} while (FindObjectFast<UObject>(this, *NewObjectName) != nullptr);

			ConflictingObject->Rename(*NewObjectName, this);
		}
	}
}

void AActor::PostCreateBlueprintComponent(UActorComponent* NewActorComp)
{
	if (NewActorComp)
	{
		NewActorComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;

		// Need to do this so component gets saved - Components array is not serialized
		BlueprintCreatedComponents.Add(NewActorComp);
	}
}


