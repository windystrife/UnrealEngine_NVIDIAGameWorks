// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/SCS_Node.h"
#include "UObject/LinkerLoad.h"
#include "Engine/Blueprint.h"
#include "Misc/SecureHash.h"
#include "UObject/PropertyPortFlags.h"
#include "Engine/InheritableComponentHandler.h"

//////////////////////////////////////////////////////////////////////////
// USCS_Node

USCS_Node::USCS_Node(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsFalseRoot_DEPRECATED = false;
	bIsNative_DEPRECATED = false;

	bIsParentComponentNative = false;

#if WITH_EDITOR
	EditorComponentInstance = NULL;
#endif
}

UActorComponent* USCS_Node::GetActualComponentTemplate(UBlueprintGeneratedClass* ActualBPGC) const
{
	UActorComponent* OverridenComponentTemplate = nullptr;
	static const FBoolConfigValueHelper EnableInheritableComponents(TEXT("Kismet"), TEXT("bEnableInheritableComponents"), GEngineIni);
	if (EnableInheritableComponents)
	{
		const USimpleConstructionScript* SCS = GetSCS();
		if (SCS != ActualBPGC->SimpleConstructionScript)
		{
			const FComponentKey ComponentKey(this);
			
			do
			{
				UInheritableComponentHandler* InheritableComponentHandler = ActualBPGC->GetInheritableComponentHandler();
				if (InheritableComponentHandler)
				{
					OverridenComponentTemplate = InheritableComponentHandler->GetOverridenComponentTemplate(ComponentKey);
				}

				ActualBPGC = Cast<UBlueprintGeneratedClass>(ActualBPGC->GetSuperClass());

			} while (!OverridenComponentTemplate && ActualBPGC && SCS != ActualBPGC->SimpleConstructionScript);
		}
	}
	return OverridenComponentTemplate ? OverridenComponentTemplate : ComponentTemplate;
}

const FBlueprintCookedComponentInstancingData* USCS_Node::GetActualComponentTemplateData(UBlueprintGeneratedClass* ActualBPGC) const
{
	const FBlueprintCookedComponentInstancingData* OverridenComponentTemplateData = nullptr;
	static const FBoolConfigValueHelper EnableInheritableComponents(TEXT("Kismet"), TEXT("bEnableInheritableComponents"), GEngineIni);
	if (EnableInheritableComponents)
	{
		const USimpleConstructionScript* SCS = GetSCS();
		if (SCS != ActualBPGC->SimpleConstructionScript)
		{
			const FComponentKey ComponentKey(this);
			
			do
			{
				UInheritableComponentHandler* InheritableComponentHandler = ActualBPGC->GetInheritableComponentHandler();
				if (InheritableComponentHandler)
				{
					OverridenComponentTemplateData = InheritableComponentHandler->GetOverridenComponentTemplateData(ComponentKey);
				}

				ActualBPGC = Cast<UBlueprintGeneratedClass>(ActualBPGC->GetSuperClass());

			} while (!OverridenComponentTemplateData && ActualBPGC && SCS != ActualBPGC->SimpleConstructionScript);
		}
	}

	return OverridenComponentTemplateData ? OverridenComponentTemplateData : &CookedComponentInstancingData;
}

UActorComponent* USCS_Node::ExecuteNodeOnActor(AActor* Actor, USceneComponent* ParentComponent, const FTransform* RootTransform, const FRotationConversionCache* RootRelativeRotationCache, bool bIsDefaultTransform)
{
	check(Actor != nullptr);
	check((ParentComponent != nullptr && !ParentComponent->IsPendingKill()) || (RootTransform != nullptr)); // must specify either a parent component or a world transform

	// Create a new component instance based on the template
	UActorComponent* NewActorComp = nullptr;
	UBlueprintGeneratedClass* ActualBPGC = Cast<UBlueprintGeneratedClass>(Actor->GetClass());
	const FBlueprintCookedComponentInstancingData* ActualComponentTemplateData = FPlatformProperties::RequiresCookedData() ? GetActualComponentTemplateData(ActualBPGC) : nullptr;
	if (ActualComponentTemplateData && ActualComponentTemplateData->bIsValid)
	{
		// Use cooked instancing data if valid (fast path).
		NewActorComp = Actor->CreateComponentFromTemplateData(ActualComponentTemplateData, InternalVariableName);
	}
	else if (UActorComponent* ActualComponentTemplate = GetActualComponentTemplate(ActualBPGC))
	{
		NewActorComp = Actor->CreateComponentFromTemplate(ActualComponentTemplate, InternalVariableName);
	}

	if(NewActorComp != nullptr)
	{
		NewActorComp->CreationMethod = EComponentCreationMethod::SimpleConstructionScript;

		// SCS created components are net addressable
		NewActorComp->SetNetAddressable();

		if (!NewActorComp->HasBeenCreated())
		{
			// Call function to notify component it has been created
			NewActorComp->OnComponentCreated();
		}

		if (NewActorComp->GetIsReplicated())
		{
			// Make sure this component is added to owning actor's replicated list.
			NewActorComp->SetIsReplicated(true);
		}

		// Special handling for scene components
		USceneComponent* NewSceneComp = Cast<USceneComponent>(NewActorComp);
		if (NewSceneComp != nullptr)
		{
			// If NULL is passed in, we are the root, so set transform and assign as RootComponent on Actor, similarly if the 
			// NewSceneComp is the ParentComponent then we are the root component. This happens when the root component is recycled
			// by StaticAllocateObject.
			if (ParentComponent == nullptr || (ParentComponent && ParentComponent->IsPendingKill()) || ParentComponent == NewSceneComp)
			{
				FTransform WorldTransform = *RootTransform;
				if(bIsDefaultTransform)
				{
					// Note: We use the scale vector from the component template when spawning (to match what happens with a native root)
					WorldTransform.SetScale3D(NewSceneComp->RelativeScale3D);
				}

				if (RootRelativeRotationCache)
				{	// Enforces using the same rotator as much as possible.
					NewSceneComp->SetRelativeRotationCache(*RootRelativeRotationCache);
				}

				NewSceneComp->SetWorldTransform(WorldTransform);
				Actor->SetRootComponent(NewSceneComp);

				// This will be true if we deferred the RegisterAllComponents() call at spawn time. In that case, we can call it now since we have set a scene root.
				if (Actor->HasDeferredComponentRegistration())
				{
					// Register the root component along with any components whose registration may have been deferred pending SCS execution in order to establish a root.
					Actor->RegisterAllComponents();
				}
			}
			// Otherwise, attach to parent component passed in
			else
			{
				NewSceneComp->SetupAttachment(ParentComponent, AttachToName);
			}

			// Register SCS scene components now (if necessary). Non-scene SCS component registration is deferred until after SCS execution, as there can be dependencies on the scene hierarchy.
			USimpleConstructionScript::RegisterInstancedComponent(NewSceneComp);
		}

		// If we want to save this to a property, do it here
		FName VarName = InternalVariableName;
		if (VarName != NAME_None)
		{
			UClass* ActorClass = Actor->GetClass();
			if (UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(ActorClass, VarName))
			{
				Prop->SetObjectPropertyValue_InContainer(Actor, NewActorComp);
			}
			else
			{
				UE_LOG(LogBlueprint, Log, TEXT("ExecuteNodeOnActor: Couldn't find property '%s' on '%s'"), *VarName.ToString(), *Actor->GetName());
#if WITH_EDITOR
				// If we're constructing editable components in the SCS editor, set the component instance corresponding to this node for editing purposes
				USimpleConstructionScript* SCS = GetSCS();
				if(SCS != nullptr && (SCS->IsConstructingEditorComponents() || SCS->GetComponentEditorActorInstance() == Actor))
				{
					EditorComponentInstance = NewSceneComp;
				}
#endif
			}
		}

		// Determine the parent component for our children (it's still our parent if we're a non-scene component)
		USceneComponent* ParentSceneComponentOfChildren = (NewSceneComp != nullptr) ? NewSceneComp : ParentComponent;

		// If we made a component, go ahead and process our children
		for (int32 NodeIdx = 0; NodeIdx < ChildNodes.Num(); NodeIdx++)
		{
			USCS_Node* Node = ChildNodes[NodeIdx];
			check(Node != nullptr);
			Node->ExecuteNodeOnActor(Actor, ParentSceneComponentOfChildren, nullptr, nullptr, false);
		}
	}

	return NewActorComp;
}

TArray<USCS_Node*> USCS_Node::GetAllNodes()
{
	TArray<USCS_Node*> AllNodes;

	//  first add ourself
	AllNodes.Add(this);

	// then add each child (including all their children)
	for(int32 ChildIdx=0; ChildIdx<ChildNodes.Num(); ChildIdx++)
	{
		USCS_Node* ChildNode = ChildNodes[ChildIdx];
		check(ChildNode != NULL);
		AllNodes.Append( ChildNode->GetAllNodes() );
	}

	return AllNodes;
}

void USCS_Node::AddChildNode(USCS_Node* InNode, bool bAddToAllNodes)
{
	if (InNode != NULL && !ChildNodes.Contains(InNode))
	{
		Modify();

		ChildNodes.Add(InNode);
		if (bAddToAllNodes)
		{
			FSCSAllNodesHelper::Add(GetSCS(), InNode);
		}
	}
}

void USCS_Node::RemoveChildNodeAt(int32 ChildIndex, bool bRemoveFromAllNodes)
{
	if (ChildIndex >= 0 && ChildIndex < ChildNodes.Num())
	{
		Modify();

		USCS_Node* ChildNode = ChildNodes[ChildIndex];
		ChildNodes.RemoveAt(ChildIndex);
		if (bRemoveFromAllNodes)
		{
			FSCSAllNodesHelper::Remove(GetSCS(), ChildNode);
		}
	}
}

void USCS_Node::RemoveChildNode(USCS_Node* InNode, bool bRemoveFromAllNodes)
{
	Modify();
	if (ChildNodes.Remove(InNode) != INDEX_NONE && bRemoveFromAllNodes)
	{
		FSCSAllNodesHelper::Remove(GetSCS(), InNode);
	}
}

void USCS_Node::MoveChildNodes(USCS_Node* SourceNode, int32 InsertLocation)
{
	if (SourceNode)
	{
		Modify();
		SourceNode->Modify();

		USimpleConstructionScript* SourceSCS = SourceNode->GetSCS();
		USimpleConstructionScript* MySCS = GetSCS();
		if (SourceSCS != MySCS)
		{
			for (USCS_Node* SCSNode : SourceNode->ChildNodes)
			{
				FSCSAllNodesHelper::Remove(SourceSCS, SCSNode);
				FSCSAllNodesHelper::Add(MySCS, SCSNode);
			}
		}
		if (InsertLocation == INDEX_NONE)
		{
			ChildNodes.Append(SourceNode->ChildNodes);
		}
		else
		{
			ChildNodes.Insert(SourceNode->ChildNodes, InsertLocation);
		}
		SourceNode->ChildNodes.Empty();
	}
}

TArray<const USCS_Node*> USCS_Node::GetAllNodes() const
{
	TArray<const USCS_Node*> AllNodes;

	//  first add ourself
	AllNodes.Add(this);

	// then add each child (including all their children)
	for(int32 ChildIdx=0; ChildIdx<ChildNodes.Num(); ChildIdx++)
	{
		const USCS_Node* ChildNode = ChildNodes[ChildIdx];
		check(ChildNode != NULL);
		AllNodes.Append( ChildNode->GetAllNodes() );
	}

	return AllNodes;
}

bool USCS_Node::IsChildOf(USCS_Node* TestParent)
{
	TArray<USCS_Node*> AllNodes;
	if(TestParent != NULL)
	{
		AllNodes = TestParent->GetAllNodes();
	}
	return AllNodes.Contains(this);
}

void USCS_Node::PreloadChain()
{
	if( HasAnyFlags(RF_NeedLoad) )
	{
		GetLinker()->Preload(this);
	}

	if (ComponentTemplate && ComponentTemplate->HasAnyFlags(RF_NeedLoad))
	{
		ComponentTemplate->GetLinker()->Preload(ComponentTemplate);
	}

	for( TArray<USCS_Node*>::TIterator ChildIt(ChildNodes); ChildIt; ++ChildIt )
	{
		USCS_Node* CurrentChild = *ChildIt;
		if( CurrentChild )
		{
			CurrentChild->PreloadChain();
		}
	}
}

bool USCS_Node::IsRootNode() const
{
	USimpleConstructionScript* SCS = GetSCS();
	return(SCS->GetRootNodes().Contains(const_cast<USCS_Node*>(this)));
}

void USCS_Node::RenameComponentTemplate(UActorComponent* ComponentTemplate, const FName& NewName)
{
	if (ComponentTemplate != nullptr && ComponentTemplate->HasAllFlags(RF_ArchetypeObject))
	{
		// Gather all instances of the template (archetype)
		TArray<UObject*> ArchetypeInstances;
		ComponentTemplate->GetArchetypeInstances(ArchetypeInstances);

		// Rename the component template (archetype) - note that this can be called during compile-on-load, so we include the flag not to reset the BPGC's package loader.
		const FString NewComponentName = NewName.ToString();
		ComponentTemplate->Rename(*(NewComponentName + USimpleConstructionScript::ComponentTemplateNameSuffix), nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);

		// Rename all component instances to match the updated variable name
		for (UObject* ArchetypeInstance : ArchetypeInstances)
		{
			// Recursively handle inherited component template overrides. In the SCS case, this is because we must also handle these before the SCS key's variable name is changed.
			if (ArchetypeInstance->HasAllFlags(RF_ArchetypeObject | RF_InheritableComponentTemplate))
			{
				RenameComponentTemplate(CastChecked<UActorComponent>(ArchetypeInstance), NewName);
			}
			else
			{
				// If this is an instanced component (i.e. owned by an Actor), ensure that we have no conflict with another instanced component belonging to the same Actor instance.
				if (AActor* Actor = Cast<AActor>(ArchetypeInstance->GetOuter()))
				{
					Actor->CheckComponentInstanceName(NewName);
				}

				ArchetypeInstance->Rename(*NewComponentName, nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
			}
		}
	}
}

void USCS_Node::SetVariableName(const FName& NewName, bool bRenameTemplate)
{
	// We need to ensure that component object names stay in sync with the variable name; this is done for 2 reasons:
	//	1) This ensures existing instances can successfully route back to the archetype (template) object through the variable name.
	//	2) This prevents new SCS nodes for the same component type from recycling an existing template with the original (base) name.
	if (bRenameTemplate && ComponentTemplate != nullptr)
	{
		// This must be called BEFORE we change the internal variable name; otherwise it will fail to find any instances of the archetype!
		RenameComponentTemplate(ComponentTemplate, NewName);
	}

	InternalVariableName = NewName;

	// >>> Backwards Compatibility: Support existing projects/tools that might be reading the variable name directly. This can be removed in a future release.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	VariableName = InternalVariableName;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	// <<< End Backwards Compatibility
}

void USCS_Node::NameWasModified()
{
	OnNameChangedExternal.ExecuteIfBound(InternalVariableName);
}

void USCS_Node::SetOnNameChanged( const FSCSNodeNameChanged& OnChange )
{
	OnNameChangedExternal = OnChange;
}

int32 USCS_Node::FindMetaDataEntryIndexForKey(const FName& Key)
{
	for(int32 i=0; i<MetaDataArray.Num(); i++)
	{
		if(MetaDataArray[i].DataKey == Key)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FString USCS_Node::GetMetaData(const FName& Key)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	check(EntryIndex != INDEX_NONE);
	return MetaDataArray[EntryIndex].DataValue;
}

void USCS_Node::SetMetaData(const FName& Key, const FString& Value)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	if(EntryIndex != INDEX_NONE)
	{
		MetaDataArray[EntryIndex].DataValue = Value;
	}
	else
	{
		MetaDataArray.Add( FBPVariableMetaDataEntry(Key, Value) );
	}
}

void USCS_Node::RemoveMetaData(const FName& Key)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	if(EntryIndex != INDEX_NONE)
	{
		MetaDataArray.RemoveAt(EntryIndex);
	}
}

void USCS_Node::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		if (Ar.IsPersistent() && !Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE))
		{
			// >>> Backwards Compatibility: Support existing projects/tools that might be reading the variable name directly. This can be removed in a future release.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			VariableName = InternalVariableName;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			// <<< End Backwards Compatibility

			// Fix up the component class property, if it has not already been set.
			// Note: This is done here, instead of in PostLoad(), because it needs to be set before Blueprint class compilation.
			if (ComponentClass == nullptr && ComponentTemplate != nullptr)
			{
				ComponentClass = ComponentTemplate->GetClass();
			}
		}
	}
}

void USCS_Node::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	ValidateGuid();
#endif

	// If valid, load cooked component instancing data.
	if (ComponentTemplate && CookedComponentInstancingData.bIsValid)
	{
		CookedComponentInstancingData.LoadCachedPropertyDataForSerialization(ComponentTemplate);
	}
}


#if WITH_EDITOR
void USCS_Node::SetParent(USCS_Node* InParentNode)
{
	ensure(InParentNode);
	USimpleConstructionScript* ParentSCS = InParentNode ? InParentNode->GetSCS() : nullptr;
	ensure(ParentSCS);
	UBlueprint* ParentBlueprint = ParentSCS ? ParentSCS->GetBlueprint() : nullptr;
	ensure(ParentBlueprint);
	UClass* ParentBlueprintGeneratedClass = ParentBlueprint ? ParentBlueprint->GeneratedClass : nullptr;

	if (ParentBlueprintGeneratedClass && InParentNode)
	{
		const FName NewParentComponentOrVariableName = InParentNode->GetVariableName();
		const FName NewParentComponentOwnerClassName = ParentBlueprintGeneratedClass->GetFName();

		// Only modify if it differs from current
		if (bIsParentComponentNative
			|| ParentComponentOrVariableName != NewParentComponentOrVariableName
			|| ParentComponentOwnerClassName != NewParentComponentOwnerClassName)
		{
			Modify();

			bIsParentComponentNative = false;
			ParentComponentOrVariableName = NewParentComponentOrVariableName;
			ParentComponentOwnerClassName = NewParentComponentOwnerClassName;
		}
	}
}

void USCS_Node::SetParent(USceneComponent* InParentComponent)
{
	check(InParentComponent != NULL);

	const FName NewParentComponentOrVariableName = InParentComponent->GetFName();
	const FName NewParentComponentOwnerClassName = NAME_None;

	// Only modify if it differs from current
	if(!bIsParentComponentNative
		|| ParentComponentOrVariableName != NewParentComponentOrVariableName
		|| ParentComponentOwnerClassName != NewParentComponentOwnerClassName)
	{
		Modify();

		bIsParentComponentNative = true;
		ParentComponentOrVariableName = NewParentComponentOrVariableName;
		ParentComponentOwnerClassName = NewParentComponentOwnerClassName;
	}
}

USceneComponent* USCS_Node::GetParentComponentTemplate(UBlueprint* InBlueprint) const
{
	USceneComponent* ParentComponentTemplate = nullptr;
	if(ParentComponentOrVariableName != NAME_None)
	{
		check(InBlueprint != nullptr && InBlueprint->GeneratedClass != nullptr);

		// If the parent component template is found in the 'Components' array of the CDO (i.e. native)
		if(bIsParentComponentNative)
		{
			// Access the Blueprint CDO
			AActor* CDO = InBlueprint->GeneratedClass->GetDefaultObject<AActor>();
			if(CDO != nullptr)
			{
				// Find the component template in the CDO that matches the specified name
				TInlineComponentArray<USceneComponent*> Components;
				CDO->GetComponents(Components);

				for (USceneComponent* CompTemplate : Components)
				{
					if(CompTemplate->GetFName() == ParentComponentOrVariableName)
					{
						// Found a match; this is our parent, we're done
						ParentComponentTemplate = CompTemplate;
						break;
					}
				}
			}
		}
		// Otherwise the parent component template is found in a parent Blueprint's SCS tree (i.e. non-native)
		else
		{
			// Get the Blueprint hierarchy
			TArray<UBlueprint*> ParentBPStack;
			UBlueprint::GetBlueprintHierarchyFromClass(InBlueprint->GeneratedClass, ParentBPStack);

			// Find the parent Blueprint in the hierarchy
			for(int32 StackIndex = ParentBPStack.Num() - 1; StackIndex > 0 && !ParentComponentTemplate; --StackIndex)
			{
				UBlueprint* ParentBlueprint = ParentBPStack[StackIndex];
				if(ParentBlueprint != nullptr
					&& ParentBlueprint->SimpleConstructionScript != nullptr
					&& ParentBlueprint->GeneratedClass->GetFName() == ParentComponentOwnerClassName)
				{
					// Find the SCS node with a variable name that matches the specified name
					TArray<USCS_Node*> ParentSCSNodes = ParentBlueprint->SimpleConstructionScript->GetAllNodes();
					for(int32 ParentNodeIndex = 0; ParentNodeIndex < ParentSCSNodes.Num(); ++ParentNodeIndex)
					{
						USceneComponent* CompTemplate = Cast<USceneComponent>(ParentSCSNodes[ParentNodeIndex]->ComponentTemplate);
						if(CompTemplate != nullptr && ParentSCSNodes[ParentNodeIndex]->GetVariableName() == ParentComponentOrVariableName)
						{
							// Found a match; this is our parent, we're done
							ParentComponentTemplate = Cast<USceneComponent>(ParentSCSNodes[ParentNodeIndex]->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(InBlueprint->GeneratedClass)));
							break;
						}
					}
				}
			}
		}
	}

	return ParentComponentTemplate;
}

void USCS_Node::ValidateGuid()
{
	// Backward compatibility:
	// The guid for the node should be always the same (event when it's not saved). 
	// The guid is created in an deterministic way using persistent name.
	if (!VariableGuid.IsValid() && (InternalVariableName != NAME_None))
	{
		const FString HashString = InternalVariableName.ToString();
		ensure(HashString.Len());

		const uint32 BufferLength = HashString.Len() * sizeof(HashString[0]);
		uint32 HashBuffer[5];
		FSHA1::HashBuffer(*HashString, BufferLength, reinterpret_cast<uint8*>(HashBuffer));
		VariableGuid = FGuid(HashBuffer[1], HashBuffer[2], HashBuffer[3], HashBuffer[4]);
	}
}

#endif
