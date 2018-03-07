// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_ComponentBoundEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/ComponentDelegateBinding.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_ComponentBoundEvent::UK2Node_ComponentBoundEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UK2Node_ComponentBoundEvent::Modify(bool bAlwaysMarkDirty)
{
	CachedNodeTitle.MarkDirty();

	return Super::Modify(bAlwaysMarkDirty);
}

bool UK2Node_ComponentBoundEvent::CanPasteHere(const UEdGraph* TargetGraph) const
{
	// By default, to be safe, we don't allow events to be pasted, except under special circumstances (see below)
	bool bDisallowPaste = !Super::CanPasteHere(TargetGraph);
	if (!bDisallowPaste)
	{
		if (const UK2Node_Event* PreExistingNode = FKismetEditorUtilities::FindBoundEventForComponent(FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph), DelegatePropertyName, ComponentPropertyName))
		{
			//UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because it is flagged as an internal event."), *GetFName().ToString());
			bDisallowPaste = true;
		}
	}
	return !bDisallowPaste;

}
FText UK2Node_ComponentBoundEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("DelegatePropertyName"), DelegatePropertyDisplayName.IsEmpty() ? FText::FromName(DelegatePropertyName) : DelegatePropertyDisplayName);
		Args.Add(TEXT("ComponentPropertyName"), FText::FromName(ComponentPropertyName));

		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("ComponentBoundEvent_Title", "{DelegatePropertyName} ({ComponentPropertyName})"), Args), this);
	}
	return CachedNodeTitle;
}

void UK2Node_ComponentBoundEvent::InitializeComponentBoundEventParams(UObjectProperty const* InComponentProperty, const UMulticastDelegateProperty* InDelegateProperty)
{
	if (InComponentProperty && InDelegateProperty)
	{
		ComponentPropertyName = InComponentProperty->GetFName();
		DelegatePropertyName = InDelegateProperty->GetFName();
		DelegatePropertyDisplayName = InDelegateProperty->GetDisplayNameText();
		DelegateOwnerClass = CastChecked<UClass>(InDelegateProperty->GetOuter())->GetAuthoritativeClass();

		EventReference.SetFromField<UFunction>(InDelegateProperty->SignatureFunction, /*bIsConsideredSelfContext =*/false);

		CustomFunctionName = FName(*FString::Printf(TEXT("BndEvt__%s_%s_%s"), *InComponentProperty->GetName(), *GetName(), *EventReference.GetMemberName().ToString()));
		bOverrideFunction = false;
		bInternalEvent = true;
		CachedNodeTitle.MarkDirty();
	}
}

UClass* UK2Node_ComponentBoundEvent::GetDynamicBindingClass() const
{
	return UComponentDelegateBinding::StaticClass();
}

void UK2Node_ComponentBoundEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UComponentDelegateBinding* ComponentBindingObject = CastChecked<UComponentDelegateBinding>(BindingObject);

	FBlueprintComponentDelegateBinding Binding;
	Binding.ComponentPropertyName = ComponentPropertyName;
	Binding.DelegatePropertyName = DelegatePropertyName;
	Binding.FunctionNameToBind = CustomFunctionName;

	CachedNodeTitle.MarkDirty();
	ComponentBindingObject->ComponentDelegateBindings.Add(Binding);
}

void UK2Node_ComponentBoundEvent::HandleVariableRenamed(UBlueprint* InBlueprint, UClass* InVariableClass, UEdGraph* InGraph, const FName& InOldVarName, const FName& InNewVarName)
{
	if (InOldVarName == ComponentPropertyName && InVariableClass->IsChildOf(InBlueprint->GeneratedClass))
	{
		Modify();
		ComponentPropertyName = InNewVarName;
	}
}

bool UK2Node_ComponentBoundEvent::IsUsedByAuthorityOnlyDelegate() const
{
	UMulticastDelegateProperty* TargetDelegateProp = GetTargetDelegateProperty();
	return (TargetDelegateProp && TargetDelegateProp->HasAnyPropertyFlags(CPF_BlueprintAuthorityOnly));
}

UMulticastDelegateProperty* UK2Node_ComponentBoundEvent::GetTargetDelegateProperty() const
{
	return FindField<UMulticastDelegateProperty>(DelegateOwnerClass, DelegatePropertyName);
}


FText UK2Node_ComponentBoundEvent::GetTooltipText() const
{
	UMulticastDelegateProperty* TargetDelegateProp = GetTargetDelegateProperty();
	if (TargetDelegateProp)
	{
		return TargetDelegateProp->GetToolTipText();
	}
	else
	{
		return FText::FromName(DelegatePropertyName);
	}
}

FString UK2Node_ComponentBoundEvent::GetDocumentationLink() const
{
	if (DelegateOwnerClass)
	{
		return FString::Printf(TEXT("Shared/GraphNodes/Blueprint/%s%s"), DelegateOwnerClass->GetPrefixCPP(), *EventReference.GetMemberName().ToString());
	}

	return FString();
}

FString UK2Node_ComponentBoundEvent::GetDocumentationExcerptName() const
{
	return DelegatePropertyName.ToString();
}

void UK2Node_ComponentBoundEvent::ReconstructNode()
{
	// We need to fixup our event reference as it may have changed or been redirected
	UMulticastDelegateProperty* TargetDelegateProp = GetTargetDelegateProperty();

	// If we couldn't find the target delegate, then try to find it in the property remap table
	if (!TargetDelegateProp)
	{
		UMulticastDelegateProperty* NewProperty = FMemberReference::FindRemappedField<UMulticastDelegateProperty>(DelegateOwnerClass, DelegatePropertyName);
		if (NewProperty)
		{
			// Found a remapped property, update the node
			TargetDelegateProp = NewProperty;
			DelegatePropertyName = NewProperty->GetFName();
			CachedNodeTitle.MarkDirty();
		}
	}

	if (TargetDelegateProp && TargetDelegateProp->SignatureFunction)
	{
		EventReference.SetFromField<UFunction>(TargetDelegateProp->SignatureFunction, false);
	}

	CachedNodeTitle.MarkDirty();

	Super::ReconstructNode();
}

void UK2Node_ComponentBoundEvent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Fix up legacy nodes that may not yet have a delegate pin
	if (Ar.IsLoading())
	{
		if(Ar.UE4Ver() < VER_UE4_K2NODE_EVENT_MEMBER_REFERENCE)
		{
			DelegateOwnerClass = EventSignatureClass_DEPRECATED;
		}

		// Recover from the period where DelegateOwnerClass was transient
		if (!DelegateOwnerClass && HasValidBlueprint())
		{
			// Search for a component property on the owning class, this should work in most cases
			UBlueprint* ParentBlueprint = GetBlueprint();
			UClass* ParentClass = ParentBlueprint ? ParentBlueprint->GeneratedClass : NULL;
			if (!ParentClass && ParentBlueprint)
			{
				// Try the skeleton class
				ParentClass = ParentBlueprint->SkeletonGeneratedClass;
			}

			UObjectProperty* ComponentProperty = ParentClass ? Cast<UObjectProperty>(ParentClass->FindPropertyByName(ComponentPropertyName)) : NULL;

			if (ParentClass && ComponentProperty)
			{
				UE_LOG(LogBlueprint, Warning, TEXT("Repaired invalid component bound event in node %s."), *GetPathName());
				DelegateOwnerClass = ComponentProperty->PropertyClass;
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
