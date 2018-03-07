// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintBoundEventNodeSpawner.h"
#include "GameFramework/Actor.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_ComponentBoundEvent.h"
#include "EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorCategoryUtils.h"
#include "BlueprintNodeSpawnerUtils.h"

#define LOCTEXT_NAMESPACE "BlueprintBoundEventNodeSpawner"

/*******************************************************************************
 * Static UBlueprintBoundEventNodeSpawner Helpers
 ******************************************************************************/

namespace BlueprintBoundEventNodeSpawnerImpl
{
	static FText GetDefaultMenuName(UMulticastDelegateProperty const* Delegate);
	static FText GetDefaultMenuCategory(UMulticastDelegateProperty const* Delegate);
}

//------------------------------------------------------------------------------
static FText BlueprintBoundEventNodeSpawnerImpl::GetDefaultMenuName(UMulticastDelegateProperty const* Delegate)
{
	bool const bShowFriendlyNames = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames;
	FText const DelegateName = bShowFriendlyNames ? FText::FromString(UEditorEngine::GetFriendlyName(Delegate)) : FText::FromName(Delegate->GetFName());

	return FText::Format(LOCTEXT("ComponentEventName", "Add {0}"), DelegateName);
}

//------------------------------------------------------------------------------
static FText BlueprintBoundEventNodeSpawnerImpl::GetDefaultMenuCategory(UMulticastDelegateProperty const* Delegate)
{
	FText DelegateCategory = FText::FromString(FObjectEditorUtils::GetCategory(Delegate));
	if (DelegateCategory.IsEmpty())
	{
		DelegateCategory = FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Delegates);
	}
	return DelegateCategory;
}

/*******************************************************************************
 * UBlueprintBoundEventNodeSpawner
 ******************************************************************************/

//------------------------------------------------------------------------------
UBlueprintBoundEventNodeSpawner* UBlueprintBoundEventNodeSpawner::Create(TSubclassOf<UK2Node_Event> NodeClass, UMulticastDelegateProperty* EventDelegate, UObject* Outer/* = nullptr*/)
{
	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	UBlueprintBoundEventNodeSpawner* NodeSpawner = NewObject<UBlueprintBoundEventNodeSpawner>(Outer);
	NodeSpawner->NodeClass     = NodeClass;
	NodeSpawner->EventDelegate = EventDelegate;

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	MenuSignature.MenuName = BlueprintBoundEventNodeSpawnerImpl::GetDefaultMenuName(EventDelegate);
	MenuSignature.Category = BlueprintBoundEventNodeSpawnerImpl::GetDefaultMenuCategory(EventDelegate);
	//MenuSignature.Tooltip,  will be pulled from the node template
	//MenuSignature.Keywords, will be pulled from the node template
	MenuSignature.Icon = FSlateIcon("EditorStyle", "GraphEditor.Event_16x");

	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintBoundEventNodeSpawner::UBlueprintBoundEventNodeSpawner(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, EventDelegate(nullptr)
{
}

//------------------------------------------------------------------------------
FBlueprintNodeSignature UBlueprintBoundEventNodeSpawner::GetSpawnerSignature() const
{
	// explicit actions for binding (like this) cannot be reconstructed form a 
	// signature (since this spawner does not own whatever it will be binding 
	// to), therefore we return an empty (invalid) signature
	return FBlueprintNodeSignature();
}

//------------------------------------------------------------------------------
UEdGraphNode* UBlueprintBoundEventNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	UK2Node_Event* EventNode = nullptr;
	if (Bindings.Num() > 0)
	{
		EventNode = CastChecked<UK2Node_Event>(Super::Invoke(ParentGraph, Bindings, Location));
	}
	return EventNode;
}

//------------------------------------------------------------------------------
UK2Node_Event const* UBlueprintBoundEventNodeSpawner::FindPreExistingEvent(UBlueprint* Blueprint, FBindingSet const& Bindings) const
{
	UK2Node_Event const* PreExistingEvent = nullptr;

	UObject const* BoundObject = nullptr;
	if (Bindings.Num() > 0)
	{
		BoundObject = Bindings.CreateConstIterator()->Get();
	}

	if (BoundObject != nullptr)
	{
		if (NodeClass->IsChildOf<UK2Node_ComponentBoundEvent>())
		{
			PreExistingEvent = FKismetEditorUtilities::FindBoundEventForComponent(Blueprint, EventDelegate->GetFName(), BoundObject->GetFName());
		}
		else if (NodeClass->IsChildOf<UK2Node_ActorBoundEvent>())
		{
			PreExistingEvent = FKismetEditorUtilities::FindBoundEventForActor(CastChecked<AActor>(BoundObject), EventDelegate->GetFName());
		}
	}
	return PreExistingEvent;
}

//------------------------------------------------------------------------------
bool UBlueprintBoundEventNodeSpawner::IsBindingCompatible(UObject const* BindingCandidate) const
{
	bool bMatchesNodeType = false;
	if (NodeClass->IsChildOf<UK2Node_ComponentBoundEvent>())
	{
		UObjectProperty const* BindingProperty = Cast<UObjectProperty>(BindingCandidate);
		bMatchesNodeType = (BindingProperty != nullptr);
	}
	else if (NodeClass->IsChildOf<UK2Node_ActorBoundEvent>())
	{
		bMatchesNodeType = BindingCandidate->IsA<AActor>();
	}

	const UMulticastDelegateProperty* Delegate = GetEventDelegate();

	if ( !ensureMsgf(!FBlueprintNodeSpawnerUtils::IsStaleFieldAction(this), 
			TEXT("Invalid BlueprintBoundEventNodeSpawner (for %s). Was the action database properly updated when this class was compiled?"), 
			*Delegate->GetOwnerClass()->GetName()))
	{
		return false;
	}

	UClass* DelegateOwner = Delegate->GetOwnerClass()->GetAuthoritativeClass();
	UClass* BindingClass  = FBlueprintNodeSpawnerUtils::GetBindingClass(BindingCandidate)->GetAuthoritativeClass();

	return bMatchesNodeType && BindingClass->IsChildOf(DelegateOwner) && !FObjectEditorUtils::IsVariableCategoryHiddenFromClass(Delegate, BindingClass);
}

//------------------------------------------------------------------------------
bool UBlueprintBoundEventNodeSpawner::BindToNode(UEdGraphNode* Node, UObject* Binding) const
{
	bool bWasBound = false;
	if (UK2Node_ComponentBoundEvent* ComponentEventNode = Cast<UK2Node_ComponentBoundEvent>(Node))
	{
		if (UObjectProperty const* BoundProperty = Cast<UObjectProperty>(Binding))
		{
			ComponentEventNode->InitializeComponentBoundEventParams(BoundProperty, EventDelegate);
			bWasBound = true;
			Node->ReconstructNode();
		}
	}
	else if (UK2Node_ActorBoundEvent* ActorEventNode = CastChecked<UK2Node_ActorBoundEvent>(Node))
	{
		if (AActor* BoundActor = Cast<AActor>(Binding))
		{
			ActorEventNode->InitializeActorBoundEventParams(BoundActor, EventDelegate);
			bWasBound = true;
			Node->ReconstructNode();
		}
	}
	return bWasBound;
}

//------------------------------------------------------------------------------
UMulticastDelegateProperty const* UBlueprintBoundEventNodeSpawner::GetEventDelegate() const
{
	return EventDelegate;
}

#undef LOCTEXT_NAMESPACE
