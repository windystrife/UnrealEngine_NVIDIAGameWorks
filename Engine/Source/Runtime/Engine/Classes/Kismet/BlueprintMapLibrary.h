// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintMapLibrary.generated.h"

UCLASS(Experimental)
class ENGINE_API UBlueprintMapLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 
	 * Adds a key and value to the map. If something already uses the provided key it will be overwritten with the new value.
	 * After calling Key is guaranteed to be associated with Value until a subsequent mutation of the Map.
	 *
	 * @param	TargetMap		The map to add the key and value to
	 * @param	Key				The key that will be used to look the value up
	 * @param	Value			The value to be retrieved later
	 * @return	True if a Value was added, or False if the Key was already present and has been overwritten
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Add", CompactNodeTitle = "ADD", MapParam = "TargetMap", MapKeyParam = "Key", MapValueParam = "Value", AutoCreateRefTerm = "Key, Value"), Category = "Utilities|Map")
	static void Map_Add(const TMap<int32, int32>& TargetMap, const int32& Key, const int32& Value);
	
	/** 
	 * Removes a key and its associated value from the map.
	 *
	 * @param	TargetMap		The map to remove the key and its associated value from
	 * @param	Key				The key that will be used to look the value up
	 * @return	True if an item was removed (False indicates nothing in the map uses the provided key)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Remove", CompactNodeTitle = "REMOVE", MapParam = "TargetMap", MapKeyParam = "Key",  AutoCreateRefTerm = "Key"), Category = "Utilities|Map")
	static bool Map_Remove(const TMap<int32, int32>& TargetMap, const int32& Key);
	
	/** 
	 * Finds the value associated with the provided Key
	 *
	 * @param	TargetMap		The map to perform the lookup on
	 * @param	Key				The key that will be used to look the value up
	 * @param	Value			The value associated with the key, default constructed if key was not found
	 * @return	True if an item was found (False indicates nothing in the map uses the provided key)
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "Find", CompactNodeTitle = "FIND", MapParam = "TargetMap", MapKeyParam = "Key", MapValueParam = "Value", AutoCreateRefTerm = "Key, Value", BlueprintThreadSafe), Category = "Utilities|Map")
	static bool Map_Find(const TMap<int32, int32>& TargetMap, const int32& Key, int32& Value);
	
	/** 
	 * Checks whether key is in a provided Map
	 *
	 * @param	TargetMap		The map to perform the lookup on
	 * @param	Key				The key that will be used to lookup
	 * @return	True if an item was found (False indicates nothing in the map uses the provided key)
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "Contains", CompactNodeTitle = "CONTAINS", MapParam = "TargetMap", MapKeyParam = "Key", AutoCreateRefTerm = "Key", BlueprintThreadSafe), Category = "Utilities|Map")
	static bool Map_Contains(const TMap<int32, int32>& TargetMap, const int32& Key);
	
	/** 
	 * Outputs an array of all keys present in the map
	 *
	 * @param	TargetMap		The map to get the list of keys from
	 * @param	Keys			All keys present in the map
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Keys", CompactNodeTitle = "KEYS", MapParam = "TargetMap", MapKeyParam = "Keys", AutoCreateRefTerm = "Keys"), Category = "Utilities|Map")
	static void Map_Keys(const TMap<int32, int32>& TargetMap, TArray<int32>& Keys);
	
	/** 
	 * Outputs an array of all values present in the map
	 *
	 * @param	TargetMap		The map to get the list of values from
	 * @param	Values			All values present in the map
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Values", CompactNodeTitle = "VALUES", MapParam = "TargetMap", MapValueParam = "Values", AutoCreateRefTerm = "Values"), Category = "Utilities|Map")
	static void Map_Values(const TMap<int32, int32>& TargetMap, TArray<int32>& Values);

	/** 
	 * Determines the number of entries in a provided Map
	 *
	 * @param	TargetMap		The map in question
	 * @return	The number of entries in the map
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "Length", CompactNodeTitle = "LENGTH", MapParam = "TargetMap", Keywords = "num size count", BlueprintThreadSafe), Category="Utilities|Map")
	static int32 Map_Length(const TMap<int32, int32>& TargetMap);
	
	/** 
	 * Clears a map of all entries, resetting it to empty
	 *
	 * @param	TargetMap		The map to clear
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "Clear", CompactNodeTitle = "CLEAR", MapParam = "TargetMap" ), Category = "Utilities|Map")
	static void Map_Clear(const TMap<int32, int32>& TargetMap);

	/** 
	* Not exposed to users. Supports setting a map property on an object by name.
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(BlueprintInternalUseOnly = "true", MapParam = "Value"))
	static void SetMapPropertyByName(UObject* Object, FName PropertyName, const TMap<int32, int32>& Value);


	DECLARE_FUNCTION(execMap_Add)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		UMapProperty* MapProperty = Cast<UMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since Key and Value aren't really an int, step the stack manually
		const UProperty* CurrKeyProp = MapProperty->KeyProp;
		const int32 KeyPropertySize = CurrKeyProp->ElementSize * CurrKeyProp->ArrayDim;
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		CurrKeyProp->InitializeValue(KeyStorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(KeyStorageSpace);
		
		const UProperty* CurrValueProp = MapProperty->ValueProp;
		const int32 ValuePropertySize = CurrValueProp->ElementSize * CurrValueProp->ArrayDim;
		void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
		CurrValueProp->InitializeValue(ValueStorageSpace);
		
		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(ValueStorageSpace);

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericMap_Add(MapAddr, MapProperty, KeyStorageSpace, ValueStorageSpace);
		P_NATIVE_END;
		
		CurrValueProp->DestroyValue(ValueStorageSpace);
		CurrKeyProp->DestroyValue(KeyStorageSpace);
	}

	DECLARE_FUNCTION(execMap_Remove)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		UMapProperty* MapProperty = Cast<UMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since Key and Value aren't really an int, step the stack manually
		const UProperty* CurrKeyProp = MapProperty->KeyProp;
		const int32 KeyPropertySize = CurrKeyProp->ElementSize * CurrKeyProp->ArrayDim;
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		CurrKeyProp->InitializeValue(KeyStorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(KeyStorageSpace);
		
		P_FINISH;
		P_NATIVE_BEGIN;
		*(bool*)RESULT_PARAM = GenericMap_Remove(MapAddr, MapProperty, KeyStorageSpace);
		P_NATIVE_END;

		CurrKeyProp->DestroyValue(KeyStorageSpace);
	}

	DECLARE_FUNCTION(execMap_Find)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		UMapProperty* MapProperty = Cast<UMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since Key and Value aren't really an int, step the stack manually
		const UProperty* CurrKeyProp = MapProperty->KeyProp;
		const int32 KeyPropertySize = CurrKeyProp->ElementSize * CurrKeyProp->ArrayDim;
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		CurrKeyProp->InitializeValue(KeyStorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(KeyStorageSpace);
		
		const UProperty* CurrValueProp = MapProperty->ValueProp;
		const int32 ValuePropertySize = CurrValueProp->ElementSize * CurrValueProp->ArrayDim;
		void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
		CurrValueProp->InitializeValue(ValueStorageSpace);
		
		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(ValueStorageSpace);
		void* ItemPtr = (Stack.MostRecentPropertyAddress != NULL && Stack.MostRecentProperty->GetClass() == CurrValueProp->GetClass()) ? Stack.MostRecentPropertyAddress : ValueStorageSpace;
		
		P_FINISH;
		P_NATIVE_BEGIN;
		*(bool*)RESULT_PARAM = GenericMap_Find(MapAddr, MapProperty, KeyStorageSpace, ItemPtr);
		P_NATIVE_END;

		CurrValueProp->DestroyValue(ValueStorageSpace);
		CurrKeyProp->DestroyValue(KeyStorageSpace);
	}
	
	DECLARE_FUNCTION(execMap_Keys)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		UMapProperty* MapProperty = Cast<UMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

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
		GenericMap_Keys(MapAddr, MapProperty, ArrayAddr, ArrayProperty);
		P_NATIVE_END

	}
	
	DECLARE_FUNCTION(execMap_Values)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		UMapProperty* MapProperty = Cast<UMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

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
		GenericMap_Values(MapAddr, MapProperty, ArrayAddr, ArrayProperty);
		P_NATIVE_END
	}

	DECLARE_FUNCTION(execMap_Contains)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		UMapProperty* MapProperty = Cast<UMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since Key and Value aren't really an int, step the stack manually
		const UProperty* CurrKeyProp = MapProperty->KeyProp;
		const int32 KeyPropertySize = CurrKeyProp->ElementSize * CurrKeyProp->ArrayDim;
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		CurrKeyProp->InitializeValue(KeyStorageSpace);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.StepCompiledIn<UProperty>(KeyStorageSpace);

		P_FINISH;
		P_NATIVE_BEGIN;
		*(bool*)RESULT_PARAM = GenericMap_Find(MapAddr, MapProperty, KeyStorageSpace, nullptr);
		P_NATIVE_END;

		CurrKeyProp->DestroyValue(KeyStorageSpace);
	}

	DECLARE_FUNCTION(execMap_Length)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		UMapProperty* MapProperty = Cast<UMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		
		P_FINISH;
		P_NATIVE_BEGIN;
		*(int32*)RESULT_PARAM = GenericMap_Length(MapAddr, MapProperty);
		P_NATIVE_END
	}

	DECLARE_FUNCTION(execMap_Clear)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		UMapProperty* MapProperty = Cast<UMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}
		
		P_FINISH;
		P_NATIVE_BEGIN;
		GenericMap_Clear(MapAddr, MapProperty);
		P_NATIVE_END
	}

	DECLARE_FUNCTION(execSetMapPropertyByName)
	{
		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, MapPropertyName);

		Stack.StepCompiledIn<UMapProperty>(nullptr);
		void* SrcMapAddr = Stack.MostRecentPropertyAddress;

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericMap_SetMapPropertyByName(OwnerObject, MapPropertyName, SrcMapAddr);
		P_NATIVE_END;
	}

	static void GenericMap_Add(const void* TargetMap, const UMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr);
	static bool GenericMap_Remove(const void* TargetMap, const UMapProperty* MapProperty, const void* KeyPtr);
	static bool GenericMap_Find(const void* TargetMap, const UMapProperty* MapProperty, const void* KeyPtr, void* ValuePtr);
	static void GenericMap_Keys(const void* MapAddr, const UMapProperty* MapProperty, const void* ArrayAddr, const UArrayProperty* ArrayProperty);
	static void GenericMap_Values(const void* MapAddr, const UMapProperty* MapProperty, const void* ArrayAddr, const UArrayProperty* ArrayProperty);
	static int32 GenericMap_Length(const void* TargetMap, const UMapProperty* MapProperty);
	static void GenericMap_Clear(const void* TargetMap, const UMapProperty* MapProperty);
	static void GenericMap_SetMapPropertyByName(UObject* OwnerObject, FName MapPropertyName, const void* SrcMapAddr);
};

