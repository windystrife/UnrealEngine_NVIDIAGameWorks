// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KismetArrayLibrary.generated.h"

class AActor;

UCLASS()
class ENGINE_API UKismetArrayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	/** 
	 *Add item to array
	 *
	 *@param	TargetArray		The array to add item to
	 *@param	NewItem			The item to add to the array
	 *@return	The index of the newly added item
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Add", CompactNodeTitle = "ADD", ArrayParm = "TargetArray", ArrayTypeDependentParams = "NewItem", AutoCreateRefTerm = "NewItem"), Category="Utilities|Array")
	static int32 Array_Add(const TArray<int32>& TargetArray, const int32& NewItem);

	/**
	*Add item to array (unique)
	*
	*@param		TargetArray		The array to add item to
	*@param		NewItem			The item to add to the array
	*@return	The index of the newly added item, or INDEX_NONE if the item is already present in the array
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "Add Unique", CompactNodeTitle = "ADDUNIQUE", ArrayParm = "TargetArray", ArrayTypeDependentParams = "NewItem", AutoCreateRefTerm = "NewItem"), Category = "Utilities|Array")
	static int32 Array_AddUnique(const TArray<int32>& TargetArray, const int32& NewItem);

	/** 
	 * Shuffle (randomize) the elements of an array
	 *
	 *@param	TargetArray		The array to shuffle
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Shuffle", CompactNodeTitle = "SHUFFLE", ArrayParm = "TargetArray"), Category="Utilities|Array")
	static void Array_Shuffle(const TArray<int32>& TargetArray);

	/** 
	 *Append an array to another array
	 *
	 *@param	TargetArray		The array to add the source array to
	 *@param	SourceArray		The array to add to the target array
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Append Array", CompactNodeTitle = "APPEND", ArrayParm = "TargetArray,SourceArray", ArrayTypeDependentParams = "SourceArray"), Category="Utilities|Array")
	static void Array_Append(const TArray<int32>& TargetArray, const TArray<int32>& SourceArray);

	/* 
	 *Insert item at the given index into the array.
	 *	
	 *@param	TargetArray		The array to insert into
	 *@param	NewItem			The item to insert into the array
	 *@param	Index			The index at which to insert the item into the array
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Insert", CompactNodeTitle = "INSERT", ArrayParm = "TargetArray", ArrayTypeDependentParams = "NewItem", AutoCreateRefTerm = "NewItem"), Category="Utilities|Array")
	static void Array_Insert(const TArray<int32>& TargetArray, const int32& NewItem, int32 Index);


	/* 
	 *Remove item at the given index from the array.
	 *
	 *@param	TargetArray		The array to remove from
	 *@param	IndexToRemove	The index into the array to remove from
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Remove Index", CompactNodeTitle = "REMOVE INDEX", ArrayParm = "TargetArray"), Category="Utilities|Array")
	static void Array_Remove(const TArray<int32>& TargetArray, int32 IndexToRemove);

	/* 
	 *Remove all instances of item from array.
	 *
	 *@param	TargetArray		The array to remove from
	 *@param	Item			The item to remove from the array
	 *@return	True if one or more items were removed
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Remove Item", CompactNodeTitle = "REMOVE", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", AutoCreateRefTerm = "Item"), Category="Utilities|Array")
	static bool Array_RemoveItem(const TArray<int32>& TargetArray, const int32 &Item);

	/* 
	 *Clear an array, removes all content
	 *
	 *@param	TargetArray		The array to clear
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Clear", CompactNodeTitle = "CLEAR", Keywords = "empty", ArrayParm = "TargetArray"), Category="Utilities|Array")
	static void Array_Clear(const TArray<int32>& TargetArray);

	/* 
	 *Resize Array to specified size. 
	 *	
	 *@param	TargetArray		The array to resize
	 *@param	Size			The new size of the array
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Resize", CompactNodeTitle = "RESIZE", ArrayParm = "TargetArray"), Category="Utilities|Array")
	static void Array_Resize(const TArray<int32>& TargetArray, int32 Size);

	/* 
	 *Get the number of items in an array
	 *
	 *@param	TargetArray		The array to get the length of
	 *@return	The length of the array
	*/
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "Length", CompactNodeTitle = "LENGTH", ArrayParm = "TargetArray", Keywords = "num size count", BlueprintThreadSafe), Category="Utilities|Array")
	static int32 Array_Length(const TArray<int32>& TargetArray);


	/* 
	 *Get the last valid index into an array
	 *	
	 *@param	TargetArray		The array to perform the operation on
	 *@return	The last valid index of the array
	*/
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "Last Index", CompactNodeTitle = "LAST INDEX", ArrayParm = "TargetArray", BlueprintThreadSafe), Category="Utilities|Array")
	static int32 Array_LastIndex(const TArray<int32>& TargetArray);

	/*
	 *Given an array and an index, returns a copy of the item found at that index
	 *
	 *@param	TargetArray		The array to get an item from
	 *@param	Index			The index in the array to get an item from
	 *@return	A copy of the item stored at the index
	*/
	UFUNCTION(BlueprintPure, CustomThunk, meta=(BlueprintInternalUseOnly = "true", DisplayName = "Get", CompactNodeTitle = "GET", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category="Utilities|Array")
	static void Array_Get(const TArray<int32>& TargetArray, int32 Index, int32& Item);

	/* 
	 *Given an array and an index, assigns the item to that array element
	 *
	 *@param	TargetArray		The array to perform the operation on
	 *@param	Index			The index to assign the item to
	 *@param	Item			The item to assign to the index of the array
	 *@param	bSizeToFit		If true, the array will expand if Index is greater than the current size of the array
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Set Array Elem", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", AutoCreateRefTerm = "Item"), Category="Utilities|Array")
	static void Array_Set(const TArray<int32>& TargetArray, int32 Index, const int32& Item, bool bSizeToFit);

	/*
	 *Swaps the elements at the specified positions in the specified array
	 *If the specified positions are equal, invoking this method leaves the array unchanged
	 *
	 *@param	TargetArray		The array to perform the operation on
	 *@param    FirstIndex      The index of one element to be swapped
	 *@param    SecondIndex     The index of the other element to be swapped
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Swap Array Elements", CompactNodeTitle = "SWAP", ArrayParm = "TargetArray"), Category="Utilities|Array")
	static void Array_Swap(const TArray<int32>& TargetArray, int32 FirstIndex, int32 SecondIndex);

	/*  
	 *Finds the index of the first instance of the item within the array
	 *	
	 *@param	TargetArray		The array to search for the item
	 *@param	ItemToFind		The item to look for
	 *@return	The index the item was found at, or -1 if not found
	*/
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "Find Item", CompactNodeTitle = "FIND", ArrayParm = "TargetArray", ArrayTypeDependentParams = "ItemToFind", AutoCreateRefTerm = "ItemToFind", BlueprintThreadSafe), Category="Utilities|Array")
	static int32 Array_Find(const TArray<int32>& TargetArray, const int32& ItemToFind);

	/*  
	 *Returns true if the array contains the given item
	 *
	 *@param	TargetArray		The array to search for the item
	 *@param	ItemToFind		The item to look for
	 *@return	True if the item was found within the array
	*/
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "Contains Item", CompactNodeTitle = "CONTAINS", ArrayParm = "TargetArray", ArrayTypeDependentParams = "ItemToFind", AutoCreateRefTerm = "ItemToFind", BlueprintThreadSafe), Category="Utilities|Array")
	static bool Array_Contains(const TArray<int32>& TargetArray, const int32& ItemToFind);

	/*  
	 *Filter an array based on a Class derived from Actor.  
	 *	
	 *@param	TargetArray		The array to filter from
	 *@param	FilterClass		The Actor sub-class type that acts as the filter, only objects derived from it will be returned.
	 *@return	An array containing only those objects which are derived from the class specified.
	*/
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Filter Array"), Category="Utilities|Array")
	static void FilterArray(const TArray<AActor*>& TargetArray, TSubclassOf<class AActor> FilterClass, TArray<AActor*>& FilteredArray);

	/** 
	 * Not exposed to users. Supports setting an array property on an object by name.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(BlueprintInternalUseOnly = "true", ArrayParm = "Value", ArrayTypeDependentParams="Value"))
	static void SetArrayPropertyByName(UObject* Object, FName PropertyName, const TArray<int32>& Value);

	/*
	 *Tests if IndexToTest is valid, i.e. greater than or equal to zero, and less than the number of elements in TargetArray.
	 *
	 *@param	TargetArray		Array to use for the IsValidIndex test
	 *@param	IndexToTest		The Index, that we want to test for being valid
	 *@return	True if the Index is Valid, i.e. greater than or equal to zero, and less than the number of elements in TargetArray.
	*/
	UFUNCTION(BlueprintPure, CustomThunk, meta = (DisplayName = "Is Valid Index", CompactNodeTitle = "IS VALID INDEX", ArrayParm = "TargetArray", BlueprintThreadSafe), Category = "Utilities|Array")
	static bool Array_IsValidIndex(const TArray<int32>& TargetArray, int32 IndexToTest);

	// Native functions that will be called by the below custom thunk layers, which read off the property address, and call the appropriate native handler
	static int32 GenericArray_Add(void* TargetArray, const UArrayProperty* ArrayProp, const void* NewItem);
	static int32 GenericArray_AddUnique(void* TargetArray, const UArrayProperty* ArrayProp, const void* NewItem);
	static void GenericArray_Shuffle(void* TargetArray, const UArrayProperty* ArrayProp);
	static void GenericArray_Append(void* TargetArray, const UArrayProperty* TargetArrayProp, void* SourceArray, const UArrayProperty* SourceArrayProperty);
	static void GenericArray_Insert(void* TargetArray, const UArrayProperty* ArrayProp, const void* NewItem, int32 Index);
	static void GenericArray_Remove(void* TargetArray, const UArrayProperty* ArrayProp, int32 IndexToRemove);
	static bool GenericArray_RemoveItem(void* TargetArray, const UArrayProperty* ArrayProp, const void* Item);
	static void GenericArray_Clear(void* TargetArray, const UArrayProperty* ArrayProp);
	static void GenericArray_Resize(void* TargetArray, const UArrayProperty* ArrayProp, int32 Size);
	static int32 GenericArray_Length(const void* TargetArray, const UArrayProperty* ArrayProp);
	static int32 GenericArray_LastIndex(const void* TargetArray, const UArrayProperty* ArrayProp);
	static void GenericArray_Get(void* TargetArray, const UArrayProperty* ArrayProp, int32 Index, void* Item);
	static void GenericArray_Set(void* TargetArray, const UArrayProperty* ArrayProp, int32 Index, const void* NewItem, bool bSizeToFit);
	static void GenericArray_Swap(const void* TargetArray, const UArrayProperty* ArrayProp, int32 First, int32 Second);
	static int32 GenericArray_Find(const void* TargetArray, const UArrayProperty* ArrayProperty, const void* ItemToFind);
	static void GenericArray_SetArrayPropertyByName(UObject* OwnerObject, FName ArrayPropertyName, const void* SrcArrayAddr);
	static bool GenericArray_IsValidIndex(const void* TargetArray, const UArrayProperty* ArrayProp, int32 IndexToTest);
	
private:
	static void GenericArray_HandleBool(const UProperty* Property, void* ItemPtr);

public:
	// Helper function to get the last valid index of the array for error reporting, or 0 if the array is empty
	static int32 GetLastIndex(const FScriptArrayHelper& ArrayHelper)
	{
		const int32 ArraySize = ArrayHelper.Num();
		return (ArraySize > 0) ? ArraySize-1 : 0;
	}

	DECLARE_FUNCTION(execArray_Add)
	{
		Stack.MostRecentProperty = nullptr;
 		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
 
 		// Since NewItem isn't really an int, step the stack manually
 		const UProperty* InnerProp = ArrayProperty->Inner;
 		const int32 PropertySize = InnerProp->ElementSize * InnerProp->ArrayDim;
 		void* StorageSpace = FMemory_Alloca(PropertySize);
 		InnerProp->InitializeValue(StorageSpace);
 
 		Stack.MostRecentPropertyAddress = NULL;
 		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* NewItemPtr = (Stack.MostRecentPropertyAddress != NULL && Stack.MostRecentProperty->GetClass() == InnerProp->GetClass()) ? Stack.MostRecentPropertyAddress : StorageSpace;
 
 		P_FINISH;
		P_NATIVE_BEGIN;
		*(int32*)RESULT_PARAM = GenericArray_Add(ArrayAddr, ArrayProperty, NewItemPtr);
		P_NATIVE_END;
		InnerProp->DestroyValue(StorageSpace);
	}

	DECLARE_FUNCTION(execArray_AddUnique)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since NewItem isn't really an int, step the stack manually
		const UProperty* InnerProp = ArrayProperty->Inner;
		const int32 PropertySize = InnerProp->ElementSize * InnerProp->ArrayDim;
		void* StorageSpace = FMemory_Alloca(PropertySize);
		InnerProp->InitializeValue(StorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* NewItemPtr = (Stack.MostRecentPropertyAddress != NULL && Stack.MostRecentProperty->GetClass() == InnerProp->GetClass()) ? Stack.MostRecentPropertyAddress : StorageSpace;

		P_FINISH;
		P_NATIVE_BEGIN;
		*(int32*)RESULT_PARAM = GenericArray_AddUnique(ArrayAddr, ArrayProperty, NewItemPtr);
		P_NATIVE_END;
		InnerProp->DestroyValue(StorageSpace);
	}

	DECLARE_FUNCTION(execArray_Shuffle)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_Shuffle(ArrayAddr, ArrayProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execArray_Append)
	{
		// Retrieve the target array
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* TargetArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* TargetArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!TargetArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		// Retrieve the source array
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* SourceArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* SourceArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!SourceArrayProperty )
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_Append(TargetArrayAddr, TargetArrayProperty, SourceArrayAddr, SourceArrayProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execArray_Insert)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since NewItem isn't really an int, step the stack manually
		const UProperty* InnerProp = ArrayProperty->Inner;
		const int32 PropertySize = InnerProp->ElementSize * InnerProp->ArrayDim;
		void* StorageSpace = FMemory_Alloca(PropertySize);
		InnerProp->InitializeValue(StorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* NewItemPtr = (Stack.MostRecentPropertyAddress != NULL && Stack.MostRecentProperty->GetClass() == InnerProp->GetClass()) ? Stack.MostRecentPropertyAddress : StorageSpace;

		P_GET_PROPERTY(UIntProperty, Index);
		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_Insert(ArrayAddr, ArrayProperty, NewItemPtr, Index);
		P_NATIVE_END;
		InnerProp->DestroyValue(StorageSpace);
	}

	DECLARE_FUNCTION(execArray_Remove)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_GET_PROPERTY(UIntProperty, Index);
		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_Remove(ArrayAddr, ArrayProperty, Index);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execArray_RemoveItem)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		// Since Item isn't really an int, step the stack manually
		const UProperty* InnerProp = ArrayProperty->Inner;
		const int32 PropertySize = InnerProp->ElementSize * InnerProp->ArrayDim;
		void* StorageSpace = FMemory_Alloca(PropertySize);
		InnerProp->InitializeValue(StorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* ItemPtr = StorageSpace;

		P_FINISH;

		// Bools need to be processed internally by the property so that C++ bool value is properly set.
		GenericArray_HandleBool(InnerProp, ItemPtr);
		P_NATIVE_BEGIN;
		*(bool*)RESULT_PARAM = GenericArray_RemoveItem(ArrayAddr, ArrayProperty, ItemPtr);
		P_NATIVE_END;

		InnerProp->DestroyValue(StorageSpace);
	}
	
	DECLARE_FUNCTION(execArray_Clear)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_Clear(ArrayAddr, ArrayProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execArray_Resize)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		P_GET_PROPERTY(UIntProperty, Size);
		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_Resize(ArrayAddr, ArrayProperty, Size);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execArray_Length)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		P_FINISH;
		P_NATIVE_BEGIN;
		*(int32*)RESULT_PARAM = GenericArray_Length(ArrayAddr, ArrayProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execArray_LastIndex)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		P_FINISH;
		P_NATIVE_BEGIN;
		*(int32*)RESULT_PARAM = GenericArray_LastIndex(ArrayAddr, ArrayProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execArray_Get)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		P_GET_PROPERTY(UIntProperty, Index);

		// Since Item isn't really an int, step the stack manually
		const UProperty* InnerProp = ArrayProperty->Inner;
		const int32 PropertySize = InnerProp->ElementSize * InnerProp->ArrayDim;
		void* StorageSpace = FMemory_Alloca(PropertySize);
		InnerProp->InitializeValue(StorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* ItemPtr = (Stack.MostRecentPropertyAddress != NULL && Stack.MostRecentProperty->GetClass() == InnerProp->GetClass()) ? Stack.MostRecentPropertyAddress : StorageSpace;

		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_Get(ArrayAddr, ArrayProperty, Index, ItemPtr);
		P_NATIVE_END;
		InnerProp->DestroyValue(StorageSpace);
	}

	DECLARE_FUNCTION(execArray_Set)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		P_GET_PROPERTY(UIntProperty, Index);

		// Since NewItem isn't really an int, step the stack manually
		const UProperty* InnerProp = ArrayProperty->Inner;
		const int32 PropertySize = InnerProp->ElementSize * InnerProp->ArrayDim;
		void* StorageSpace = FMemory_Alloca(PropertySize);
		InnerProp->InitializeValue(StorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* NewItemPtr = (Stack.MostRecentPropertyAddress != NULL && Stack.MostRecentProperty->GetClass() == InnerProp->GetClass()) ? Stack.MostRecentPropertyAddress : StorageSpace;

		P_GET_UBOOL(bSizeToFit);

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericArray_Set(ArrayAddr, ArrayProperty, Index, NewItemPtr, bSizeToFit);
		P_NATIVE_END;
		InnerProp->DestroyValue(StorageSpace);
	}


	DECLARE_FUNCTION(execArray_Swap)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(nullptr);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_GET_PROPERTY(UIntProperty, First);
		P_GET_PROPERTY(UIntProperty, Second);

		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_Swap(ArrayAddr, ArrayProperty, First, Second);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execArray_Find)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		// Since ItemToFind isn't really an int, step the stack manually
		const UProperty* InnerProp = ArrayProperty->Inner;
		const int32 PropertySize = InnerProp->ElementSize * InnerProp->ArrayDim;
		void* StorageSpace = FMemory_Alloca(PropertySize);
		InnerProp->InitializeValue(StorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* ItemToFindPtr = StorageSpace;

		P_FINISH;

		// Bools need to be processed internally by the property so that C++ bool value is properly set.
		GenericArray_HandleBool(InnerProp, ItemToFindPtr);

		P_NATIVE_BEGIN;
		// Perform the search
		*(int32*)RESULT_PARAM = GenericArray_Find(ArrayAddr, ArrayProperty, ItemToFindPtr);
		P_NATIVE_END;

		InnerProp->DestroyValue(StorageSpace);
	}
	
	DECLARE_FUNCTION(execArray_Contains)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		// Since ItemToFind isn't really an int, step the stack manually
		const UProperty* InnerProp = ArrayProperty->Inner;
		const int32 PropertySize = InnerProp->ElementSize * InnerProp->ArrayDim;
		void* StorageSpace = FMemory_Alloca(PropertySize);
		InnerProp->InitializeValue(StorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(StorageSpace);
		void* ItemToFindPtr = StorageSpace;

		P_FINISH;

		// Bools need to be processed internally by the property so that C++ bool value is properly set.
		GenericArray_HandleBool(InnerProp, ItemToFindPtr);

		// Perform the search
		P_NATIVE_BEGIN;
		*(bool*)RESULT_PARAM = GenericArray_Find(ArrayAddr, ArrayProperty, ItemToFindPtr) >= 0;
		P_NATIVE_END;

		InnerProp->DestroyValue(StorageSpace);
	}

	DECLARE_FUNCTION(execSetArrayPropertyByName)
	{
		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, ArrayPropertyName);

		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* SrcArrayAddr = Stack.MostRecentPropertyAddress;

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericArray_SetArrayPropertyByName(OwnerObject, ArrayPropertyName, SrcArrayAddr);
		P_NATIVE_END;
	}
	
	DECLARE_FUNCTION(execArray_IsValidIndex)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_GET_PROPERTY(UIntProperty, IndexToTest);

		P_FINISH;

		P_NATIVE_BEGIN;
		*(bool*)RESULT_PARAM = GenericArray_IsValidIndex(ArrayAddr, ArrayProperty, IndexToTest);
		P_NATIVE_END;
	}
};
