// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/ComponentDelegateBinding.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

UComponentDelegateBinding::UComponentDelegateBinding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FMulticastScriptDelegate* UComponentDelegateBinding::FindComponentTargetDelegate(const UObject* InInstance, const FBlueprintComponentDelegateBinding& InBinding, const UObjectProperty* InObjectProperty)
{
	// Get the property that points to the component
	const UObjectProperty* ObjProp = (InObjectProperty != nullptr && InObjectProperty->GetFName() == InBinding.ComponentPropertyName) ? InObjectProperty : FindField<UObjectProperty>(InInstance->GetClass(), InBinding.ComponentPropertyName);
	if(ObjProp != nullptr)
	{
		// ..see if there is actually a component assigned
		UObject* Component = ObjProp->GetObjectPropertyValue_InContainer(InInstance);
		if(Component != nullptr)
		{
			// If there is, find and return the delegate property on it
			UMulticastDelegateProperty* DelegateProp = FindField<UMulticastDelegateProperty>(Component->GetClass(), InBinding.DelegatePropertyName);
			if(DelegateProp != nullptr)
			{
				return DelegateProp->GetPropertyValuePtr_InContainer(Component);
			}
		}
	}

	return nullptr;
}

void UComponentDelegateBinding::BindDynamicDelegates(UObject* InInstance) const
{
	for(int32 BindIdx=0; BindIdx<ComponentDelegateBindings.Num(); BindIdx++)
	{
		const FBlueprintComponentDelegateBinding& Binding = ComponentDelegateBindings[BindIdx];

		// Get the delegate property on the component we want to bind to
		FMulticastScriptDelegate* TargetDelegate = FindComponentTargetDelegate(InInstance, Binding);
		// Get the function we want to bind
		UFunction* FunctionToBind = FindField<UFunction>(InInstance->GetClass(), Binding.FunctionNameToBind);
		// If we have both of those..
		if(TargetDelegate != nullptr && FunctionToBind != nullptr)
		{
			// Bind function on the instance to this delegate
			FScriptDelegate Delegate;
			Delegate.BindUFunction(InInstance, Binding.FunctionNameToBind);
			TargetDelegate->AddUnique(Delegate);
		}
	}
}

void UComponentDelegateBinding::UnbindDynamicDelegates(UObject* InInstance) const
{
	for (int32 BindIdx = 0; BindIdx<ComponentDelegateBindings.Num(); BindIdx++)
	{
		const FBlueprintComponentDelegateBinding& Binding = ComponentDelegateBindings[BindIdx];

		// Get the delegate property on the component we want to unbind from
		FMulticastScriptDelegate* TargetDelegate = FindComponentTargetDelegate(InInstance, Binding);
		if (TargetDelegate != nullptr)
		{
			// Unbind function on the instance from this delegate
			TargetDelegate->Remove(InInstance, Binding.FunctionNameToBind);
		}
	}
}

void UComponentDelegateBinding::UnbindDynamicDelegatesForProperty(UObject* InInstance, const UObjectProperty* InObjectProperty) const
{
	for(int32 BindIdx=0; BindIdx<ComponentDelegateBindings.Num(); BindIdx++)
	{
		const FBlueprintComponentDelegateBinding& Binding = ComponentDelegateBindings[BindIdx];
		const UObjectProperty* ObjProp = FindField<UObjectProperty>(InInstance->GetClass(), Binding.ComponentPropertyName);
		if(ObjProp == InObjectProperty)
		{
			// Get the delegate property on the component we want to unbind from
			FMulticastScriptDelegate* TargetDelegate = FindComponentTargetDelegate(InInstance, Binding, ObjProp);
			if(TargetDelegate != nullptr)
			{
				// Unbind function on the instance from this delegate
				TargetDelegate->Remove(InInstance, Binding.FunctionNameToBind);
			}

			break;
		}
	}
}
