// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintSetLibrary.generated.h"

UCLASS(Experimental)
class ENGINE_API UBlueprintSetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 
	 * Adds item to set. Output value indicates whether the item was successfully added, meaning an 
	 * output of False indicates the item was already in the Set.
	 *
	 * @param	TargetSet		The set to add item to
	 * @param	NewItem			The item to add to the set
	 * @return	True if NewItem was added to the set (False indicates an equivalent item was present)
	 * 
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Add", CompactNodeTitle = "ADD", SetParam = "TargetSet|NewItem", AutoCreateRefTerm = "NewItem"), Category="Utilities|Set")
	static void Set_Add(const TSet<int32>& TargetSet, const int32& NewItem);
	
	/**
	 * Adds all elements from an Array to a Set
	 *
	 * @param	TargetSet		The set to search for the item
	 * @param	NewItems		The items to add to the set
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Add Items", CompactNodeTitle = "ADD ITEMS", SetParam = "TargetSet|NewItems", AutoCreateRefTerm = "NewItems"), Category="Utilities|Set")
	static void Set_AddItems(const TSet<int32>& TargetSet, const TArray<int32>& NewItems);

	/**
	 * Remove item from set. Output value indicates if something was actually removed. False
	 * indicates no equivalent item was found.
	 *
	 * @param	TargetSet		The set to remove from
	 * @param	Item			The item to remove from the set
	 * @return	True if an item was removed (False indicates no equivalent item was present)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Remove", CompactNodeTitle = "REMOVE", SetParam = "TargetSet|Item", AutoCreateRefTerm = "Item"), Category="Utilities|Set")
	static bool Set_Remove(const TSet<int32>& TargetSet, const int32& Item);
	
	/**
	 * Removes all elements in an Array from a set.
	 *
	 * @param	TargetSet		The set to remove from
	 * @param	Items			The items to remove from the set
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Remove Items", CompactNodeTitle = "REMOVE ITEMS", SetParam = "TargetSet|Items", AutoCreateRefTerm = "Items"), Category="Utilities|Set")
	static void Set_RemoveItems(const TSet<int32>& TargetSet, const  TArray<int32>& Items);

	/**
	 * Outputs an Array containing copies of the entries of a Set.
	 *
	 * @param		A		Set
	 * @param		Result	Array
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "To Array", CompactNodeTitle = "TO ARRAY", SetParam = "A|Result"), Category = "Utilities|Set")
	static void Set_ToArray(const TSet<int32>& A, TArray<int32>& Result);

	/**
	 * Clear a set, removes all content.
	 *
	 * @param	TargetSet		The set to clear
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Clear", CompactNodeTitle = "CLEAR", Keywords = "empty", SetParam = "TargetSet"), Category="Utilities|Set")
	static void Set_Clear(const TSet<int32>& TargetSet);

	/**
	 * Get the number of items in a set.
	 *
	 * @param	TargetSet		The set to get the length of
	 * @return	The length of the set
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "Length", CompactNodeTitle = "LENGTH", SetParam = "TargetSet", Keywords = "num size count", BlueprintThreadSafe), Category="Utilities|Set")
	static int32 Set_Length(const TSet<int32>& TargetSet);

	/**
	 * Returns true if the set contains the given item.
	 *
	 * @param	TargetSet		The set to search for the item
	 * @param	ItemToFind		The item to look for
	 * @return	True if the item was found within the set
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "Contains Item", CompactNodeTitle = "CONTAINS", SetParam = "TargetSet|ItemToFind", AutoCreateRefTerm = "ItemToFind", BlueprintThreadSafe), Category="Utilities|Set")
	static bool Set_Contains(const TSet<int32>& TargetSet, const int32& ItemToFind);

	/**
	 * Assigns Result to the intersection of Set A and Set B. That is, Result will contain
	 * all elements that are in both Set A and Set B. To intersect with the empty set use
	 * Clear.
	 *
	 * @param		A		One set to intersect
	 * @param		B		Another set to intersect
	 * @param		Result	Set to store results in
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Intersection", CompactNodeTitle = "INTERSECTION", SetParam = "A|B|Result"), Category="Utilities|Set")
	static void Set_Intersection(const TSet<int32>& A, const TSet<int32>& B, TSet<int32>& Result );

	/**
	 * Assigns Result to the union of two sets, A and B. That is, Result will contain
	 * all elements that are in Set A and in addition all elements in Set B. Note that 
	 * a Set is a collection of unique elements, so duplicates will be eliminated.
	 *
	 * @param		A		One set to union
	 * @param		B		Another set to union
	 * @param		Result	Set to store results in
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "Union", CompactNodeTitle = "UNION", SetParam = "A|B|Result"), Category = "Utilities|Set")
	static void Set_Union(const TSet<int32>& A, const TSet<int32>& B, TSet<int32>& Result );

	/**
	 * Assigns Result to the relative difference of two sets, A and B. That is, Result will 
	 * contain  all elements that are in Set A but are not found in Set B. Note that the 
	 * difference between two sets  is not commutative. The Set whose elements you wish to 
	 * preserve should be the first (top) parameter. Also called the relative complement.
	 *
	 * @param		A		Starting set
	 * @param		B		Set of elements to remove from set A
	 * @param		Result	Set containing all elements in A that are not found in B
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "Difference", CompactNodeTitle = "DIFFERENCE", SetParam = "A|B|Result"), Category = "Utilities|Set")
	static void Set_Difference(const TSet<int32>& A, const TSet<int32>& B, TSet<int32>& Result );

	/** 
	 * Not exposed to users. Supports setting a set property on an object by name.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(BlueprintInternalUseOnly = "true", SetParam = "Value"))
	static void SetSetPropertyByName(UObject* Object, FName PropertyName, const TSet<int32>& Value);

	DECLARE_FUNCTION(execSet_Add)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddr = Stack.MostRecentPropertyAddress;
		USetProperty* SetProperty = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since ItemPtr isn't really an int, step the stack manually
		const UProperty* ElementProp = SetProperty->ElementProp;
		const int32 PropertySize = ElementProp->ElementSize * ElementProp->ArrayDim;
		void* StorageSpace = FMemory_Alloca(PropertySize);
		ElementProp->InitializeValue(StorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* ItemPtr = StorageSpace;

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericSet_Add(SetAddr, SetProperty, ItemPtr);
		P_NATIVE_END;

		ElementProp->DestroyValue(StorageSpace);
	}

	DECLARE_FUNCTION(execSet_AddItems)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddr = Stack.MostRecentPropertyAddress;
		USetProperty* SetProperty = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* TargetArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* TargetArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!TargetArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericSet_AddItems(SetAddr, SetProperty, TargetArrayAddr, TargetArrayProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execSet_Remove)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddr = Stack.MostRecentPropertyAddress;
		USetProperty* SetProperty = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since ItemPtr isn't really an int, step the stack manually
		const UProperty* ElementProp = SetProperty->ElementProp;
		const int32 PropertySize = ElementProp->ElementSize * ElementProp->ArrayDim;
		void* StorageSpace = FMemory_Alloca(PropertySize);
		ElementProp->InitializeValue(StorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* ItemPtr = StorageSpace;

		P_FINISH;

		P_NATIVE_BEGIN;
		*(bool*)RESULT_PARAM = GenericSet_Remove(SetAddr, SetProperty, ItemPtr);
		P_NATIVE_END;

		ElementProp->DestroyValue(StorageSpace);
	}

	DECLARE_FUNCTION(execSet_RemoveItems)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddr = Stack.MostRecentPropertyAddress;
		USetProperty* SetProperty = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* TargetArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* TargetArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!TargetArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericSet_RemoveItems(SetAddr, SetProperty, TargetArrayAddr, TargetArrayProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execSet_ToArray)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddr = Stack.MostRecentPropertyAddress;
		USetProperty* SetProperty = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* TargetArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* TargetArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!TargetArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericSet_ToArray(SetAddr, SetProperty, TargetArrayAddr, TargetArrayProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execSet_Clear)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddr = Stack.MostRecentPropertyAddress;
		USetProperty* SetProperty = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;

		// Perform the search
		P_NATIVE_BEGIN;
		GenericSet_Clear(SetAddr, SetProperty);
		P_NATIVE_END;

	}

	DECLARE_FUNCTION(execSet_Length)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddr = Stack.MostRecentPropertyAddress;
		USetProperty* SetProperty = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;

		// Perform the search
		P_NATIVE_BEGIN;
		*(int32*)RESULT_PARAM = GenericSet_Length(SetAddr, SetProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execSet_Contains)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddr = Stack.MostRecentPropertyAddress;
		USetProperty* SetProperty = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetProperty)
		{
			// @todo(dano): rename to 'container context failed'
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since ItemToFind isn't really an int, step the stack manually
		const UProperty* ElementProp = SetProperty->ElementProp;
		const int32 PropertySize = ElementProp->ElementSize * ElementProp->ArrayDim;
		void* StorageSpace = FMemory_Alloca(PropertySize);
		ElementProp->InitializeValue(StorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* ItemToFindPtr = StorageSpace;

		P_FINISH;

		P_NATIVE_BEGIN;
		*(bool*)RESULT_PARAM = GenericSet_Contains(SetAddr, SetProperty, ItemToFindPtr);
		P_NATIVE_END;

		ElementProp->DestroyValue(StorageSpace);
	}

	DECLARE_FUNCTION(execSet_Intersection)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddrA = Stack.MostRecentPropertyAddress;
		USetProperty* SetPropertyA = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetPropertyA)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddrB = Stack.MostRecentPropertyAddress;
		USetProperty* SetPropertyB = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetPropertyB)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddrResult = Stack.MostRecentPropertyAddress;
		USetProperty* SetPropertyResult = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetPropertyResult)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericSet_Intersect(SetAddrA, SetPropertyA, SetAddrB, SetPropertyB, SetAddrResult, SetPropertyResult);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execSet_Union)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddrA = Stack.MostRecentPropertyAddress;
		USetProperty* SetPropertyA = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetPropertyA)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddrB = Stack.MostRecentPropertyAddress;
		USetProperty* SetPropertyB = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetPropertyB)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddrResult = Stack.MostRecentPropertyAddress;
		USetProperty* SetPropertyResult = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetPropertyResult)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericSet_Union(SetAddrA, SetPropertyA, SetAddrB, SetPropertyB, SetAddrResult, SetPropertyResult);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execSet_Difference)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddrA = Stack.MostRecentPropertyAddress;
		USetProperty* SetPropertyA = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetPropertyA)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddrB = Stack.MostRecentPropertyAddress;
		USetProperty* SetPropertyB = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetPropertyB)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<USetProperty>(NULL);
		void* SetAddrResult = Stack.MostRecentPropertyAddress;
		USetProperty* SetPropertyResult = Cast<USetProperty>(Stack.MostRecentProperty);
		if (!SetPropertyResult)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericSet_Difference(SetAddrA, SetPropertyA, SetAddrB, SetPropertyB, SetAddrResult, SetPropertyResult);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execSetSetPropertyByName)
	{
		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, SetPropertyName);

		Stack.StepCompiledIn<USetProperty>(nullptr);
		void* SrcSetAddr = Stack.MostRecentPropertyAddress;

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericSet_SetSetPropertyByName(OwnerObject, SetPropertyName, SrcSetAddr);
		P_NATIVE_END;
	}

	static void GenericSet_Add(const void* TargetSet, const USetProperty* SetProperty, const void* ItemPtr);
	static void GenericSet_AddItems(const void* TargetSet, const USetProperty* SetProperty, const void* TargetArray, const UArrayProperty* ArrayProperty);
	static bool GenericSet_Remove(const void* TargetSet, const USetProperty* SetProperty, const void* ItemPtr);
	static void GenericSet_RemoveItems(const void* TargetSet, const USetProperty* SetProperty, const void* TargetArray, const UArrayProperty* ArrayProperty);
	static void GenericSet_ToArray(const void* TargetSet, const USetProperty* SetProperty, void* TargetArray, const UArrayProperty* ArrayProperty);
	static void GenericSet_Clear(const void* TargetSet, const USetProperty* SetProperty);
	static int32 GenericSet_Length(const void* TargetSet, const USetProperty* SetProperty);
	static bool GenericSet_Contains(const void* TargetSet, const USetProperty* SetProperty, const void* ItemToFind);
	static void GenericSet_Intersect(const void* SetA, const USetProperty* SetPropertyA, const void* SetB, const USetProperty* SetPropertyB, const void* SetResult, const USetProperty* SetPropertyResult);
	static void GenericSet_Union(const void* SetA, const USetProperty* SetPropertyA, const void* SetB, const USetProperty* SetPropertyB, const void* SetResult, const USetProperty* SetPropertyResult);
	static void GenericSet_Difference(const void* SetA, const USetProperty* SetPropertyA, const void* SetB, const USetProperty* SetPropertyB, const void* SetResult, const USetProperty* SetPropertyResult);
	static void GenericSet_SetSetPropertyByName(UObject* OwnerObject, FName SetPropertyName, const void* SrcSetAddr);
};

