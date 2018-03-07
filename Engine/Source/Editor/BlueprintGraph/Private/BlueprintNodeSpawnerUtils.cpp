// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintNodeSpawnerUtils.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
#include "BlueprintEventNodeSpawner.h"
#include "BlueprintBoundEventNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.h"

//------------------------------------------------------------------------------
UField const* FBlueprintNodeSpawnerUtils::GetAssociatedField(UBlueprintNodeSpawner const* BlueprintAction)
{
	UField const* ClassField = nullptr;

	if (UFunction const* Function = GetAssociatedFunction(BlueprintAction))
	{
		ClassField = Function;
	}
	else if (UProperty const* Property = GetAssociatedProperty(BlueprintAction))
	{
		ClassField = Property;
	}
	// @TODO: have to fix up some of the filter cases to ignore structs/enums
// 	else if (UBlueprintFieldNodeSpawner const* FieldNodeSpawner = Cast<UBlueprintFieldNodeSpawner>(BlueprintAction))
// 	{
// 		ClassField = FieldNodeSpawner->GetField();
// 	}
	return ClassField;
}

//------------------------------------------------------------------------------
UFunction const* FBlueprintNodeSpawnerUtils::GetAssociatedFunction(UBlueprintNodeSpawner const* BlueprintAction)
{
	UFunction const* Function = nullptr;

	if (UBlueprintFunctionNodeSpawner const* FuncNodeSpawner = Cast<UBlueprintFunctionNodeSpawner>(BlueprintAction))
	{
		Function = FuncNodeSpawner->GetFunction();
	}
	else if (UBlueprintEventNodeSpawner const* EventSpawner = Cast<UBlueprintEventNodeSpawner>(BlueprintAction))
	{
		Function = EventSpawner->GetEventFunction();
	}

	return Function;
}

//------------------------------------------------------------------------------
UProperty const* FBlueprintNodeSpawnerUtils::GetAssociatedProperty(UBlueprintNodeSpawner const* BlueprintAction)
{
	UProperty const* Property = nullptr;

	if (UBlueprintDelegateNodeSpawner const* PropertySpawner = Cast<UBlueprintDelegateNodeSpawner>(BlueprintAction))
	{
		Property = PropertySpawner->GetDelegateProperty();
	}
	else if (UBlueprintVariableNodeSpawner const* VarSpawner = Cast<UBlueprintVariableNodeSpawner>(BlueprintAction))
	{
		Property = VarSpawner->GetVarProperty();
	}
	else if (UBlueprintBoundEventNodeSpawner const* BoundSpawner = Cast<UBlueprintBoundEventNodeSpawner>(BlueprintAction))
	{
		Property = BoundSpawner->GetEventDelegate();
	}
	return Property;
}

//------------------------------------------------------------------------------
UClass* FBlueprintNodeSpawnerUtils::GetBindingClass(const UObject* Binding)
{
	UClass* BindingClass = Binding->GetClass();
	if (const UObjectProperty* ObjProperty = Cast<UObjectProperty>(Binding))
	{
		BindingClass = ObjProperty->PropertyClass;
	}
	return BindingClass;
}

//------------------------------------------------------------------------------
bool FBlueprintNodeSpawnerUtils::IsStaleFieldAction(UBlueprintNodeSpawner const* BlueprintAction)
{
	bool bHasStaleAssociatedField= false;

	const UField* AssociatedField = GetAssociatedField(BlueprintAction);
	if (AssociatedField != nullptr)
	{
		UClass* ClassOwner = AssociatedField->GetOwnerClass();
		if (ClassOwner != nullptr)
		{
			// check to see if this field belongs to a TRASH or REINST class,
			// maybe to a class that was thrown out because of a hot-reload?
			bHasStaleAssociatedField = ClassOwner->HasAnyClassFlags(CLASS_NewerVersionExists) || (ClassOwner->GetOutermost() == GetTransientPackage());
		}
	}

	return bHasStaleAssociatedField;
}
