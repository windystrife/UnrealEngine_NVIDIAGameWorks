// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/BlueprintMapLibrary.h"

void UBlueprintMapLibrary::GenericMap_Add(const void* TargetMap, const UMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr)
{
	if (TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		MapHelper.AddPair(KeyPtr, ValuePtr);
	}
}

bool UBlueprintMapLibrary::GenericMap_Remove(const void* TargetMap, const UMapProperty* MapProperty, const void* KeyPtr)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		return MapHelper.RemovePair(KeyPtr);
	}
	return false;
}

bool UBlueprintMapLibrary::GenericMap_Find(const void* TargetMap, const UMapProperty* MapProperty, const void* KeyPtr, void* OutValuePtr)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		uint8* FoundValuePtr = MapHelper.FindValueFromHash(KeyPtr);
		if (OutValuePtr)
		{
			if (FoundValuePtr)
			{
				MapProperty->ValueProp->CopyCompleteValueFromScriptVM(OutValuePtr, FoundValuePtr);
			}
			else
			{
				MapProperty->ValueProp->InitializeValue(OutValuePtr);
			}
		}
		return FoundValuePtr != nullptr;
	}
	return false;
}

void UBlueprintMapLibrary::GenericMap_Keys(const void* TargetMap, const UMapProperty* MapProperty, const void* TargetArray, const UArrayProperty* ArrayProperty)
{
	if(TargetMap && TargetArray && ensure(MapProperty->KeyProp->GetID() == ArrayProperty->Inner->GetID()) )
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		FScriptArrayHelper ArrayHelper(ArrayProperty, TargetArray);
		ArrayHelper.EmptyValues();

		UProperty* InnerProp = ArrayProperty->Inner;

		int32 Size = MapHelper.Num();
		for( int32 I = 0; Size; ++I )
		{
			if(MapHelper.IsValidIndex(I))
			{
				int32 LastIndex = ArrayHelper.AddValue();
				InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(LastIndex), MapHelper.GetKeyPtr(I));
				--Size;
			}
		}
	}
}

void UBlueprintMapLibrary::GenericMap_Values(const void* TargetMap, const UMapProperty* MapProperty, const void* TargetArray, const UArrayProperty* ArrayProperty)
{	
	if(TargetMap && TargetArray && ensure(MapProperty->ValueProp->GetID() == ArrayProperty->Inner->GetID()))
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		FScriptArrayHelper ArrayHelper(ArrayProperty, TargetArray);
		ArrayHelper.EmptyValues();

		UProperty* InnerProp = ArrayProperty->Inner;
		
		int32 Size = MapHelper.Num();
		for( int32 I = 0; Size; ++I )
		{
			if(MapHelper.IsValidIndex(I))
			{
				int32 LastIndex = ArrayHelper.AddValue();
				InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(LastIndex), MapHelper.GetValuePtr(I));
				--Size;
			}
		}
	}
}

int32 UBlueprintMapLibrary::GenericMap_Length(const void* TargetMap, const UMapProperty* MapProperty)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		return MapHelper.Num();
	}
	return 0;
}

void UBlueprintMapLibrary::GenericMap_Clear(const void* TargetMap, const UMapProperty* MapProperty)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		MapHelper.EmptyValues();
	}
}

void UBlueprintMapLibrary::GenericMap_SetMapPropertyByName(UObject* OwnerObject, FName MapPropertyName, const void* SrcMapAddr)
{
	if (OwnerObject)
	{
		if (UMapProperty* MapProp = FindField<UMapProperty>(OwnerObject->GetClass(), MapPropertyName))
		{
			void* Dest = MapProp->ContainerPtrToValuePtr<void>(OwnerObject);
			MapProp->CopyValuesInternal(Dest, SrcMapAddr, 1);
		}
	}
}
