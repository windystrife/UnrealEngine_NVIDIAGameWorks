// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ScriptBlueprintGeneratedClass.h"
#include "ScriptPluginLog.h"
#if WITH_LUA
#include "LuaIntegration.h"
#endif

/////////////////////////////////////////////////////
// UScriptBlueprintGeneratedClass

UScriptBlueprintGeneratedClass::UScriptBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UScriptBlueprintGeneratedClass::AddUniqueNativeFunction(const FName& InName, Native InPointer)
{
	// Find the function in the class's native function lookup table.
	for (int32 FunctionIndex = 0; FunctionIndex < NativeFunctionLookupTable.Num(); ++FunctionIndex)
	{
		FNativeFunctionLookup& NativeFunctionLookup = NativeFunctionLookupTable[FunctionIndex];
		if (NativeFunctionLookup.Name == InName)
		{
			return;
		}
	}
	new(NativeFunctionLookupTable)FNativeFunctionLookup(InName, InPointer);
}

void UScriptBlueprintGeneratedClass::RemoveNativeFunction(const FName& InName)
{
	// Find the function in the class's native function lookup table.
	for (int32 FunctionIndex = 0; FunctionIndex < NativeFunctionLookupTable.Num(); ++FunctionIndex)
	{
		FNativeFunctionLookup& NativeFunctionLookup = NativeFunctionLookupTable[FunctionIndex];
		if (NativeFunctionLookup.Name == InName)
		{
			NativeFunctionLookupTable.RemoveAt(FunctionIndex);
			return;
		}
	}
}

void UScriptBlueprintGeneratedClass::PurgeClass(bool bRecompilingOnLoad)
{
	Super::PurgeClass(bRecompilingOnLoad);
	ScriptProperties.Empty();
}

void FScriptContextBase::PushScriptPropertyValues(UScriptBlueprintGeneratedClass* Class, const UObject* Obj)
{
	// @todo: optimize this
	for (auto Property : Class->ScriptProperties)
	{
		if (UFloatProperty* FloatProperty = Cast<UFloatProperty>(Property))
		{
			float Value = FloatProperty->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<float>(Obj));
			SetFloatProperty(Property->GetName(), Value);
		}
		else if (UIntProperty* IntProperty = Cast<UIntProperty>(Property))
		{
			int32 Value = IntProperty->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<int32>(Obj));
			SetIntProperty(Property->GetName(), Value);
		}
		else if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
		{
			bool Value = BoolProperty->GetPropertyValue(Property->ContainerPtrToValuePtr<void>(Obj));
			SetBoolProperty(Property->GetName(), Value);
		}
		else if (UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
		{
			UObject* Value = ObjectProperty->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(Obj));
			SetObjectProperty(Property->GetName(), Value);
		}
		else if (UStrProperty* StringProperty = Cast<UStrProperty>(Property))
		{
			FString Value = StringProperty->GetPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(Obj));
			SetStringProperty(Property->GetName(), Value);
		}
	}
}

void FScriptContextBase::FetchScriptPropertyValues(UScriptBlueprintGeneratedClass* Class, UObject* Obj)
{
	// @todo: optimize this
	for (auto Property : Class->ScriptProperties)
	{
		if (UFloatProperty* FloatProperty = Cast<UFloatProperty>(Property))
		{
			float Value = GetFloatProperty(Property->GetName());
			FloatProperty->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<float>(Obj), Value);
		}
		else if (UIntProperty* IntProperty = Cast<UIntProperty>(Property))
		{
			int32 Value = GetIntProperty(Property->GetName());
			IntProperty->SetPropertyValue(Property->ContainerPtrToValuePtr<int32>(Obj), Value);
		}
		else if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
		{
			bool Value = GetBoolProperty(Property->GetName());
			BoolProperty->SetPropertyValue(Property->ContainerPtrToValuePtr<float>(Obj), Value);
		}
		else if (UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
		{
			UObject* Value = GetObjectProperty(Property->GetName());
			ObjectProperty->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(Obj), Value);
		}
		else if (UStrProperty* StringProperty = Cast<UStrProperty>(Property))
		{
			FString Value = GetStringProperty(Property->GetName());
			StringProperty->SetPropertyValue(Property->ContainerPtrToValuePtr<FString>(Obj), Value);
		}
	}
}

FScriptContextBase* FScriptContextBase::CreateContext(const FString& SourceCode, UScriptBlueprintGeneratedClass* Class, UObject* Owner)
{
	FScriptContextBase* NewContext = NULL;
#if WITH_LUA
	NewContext = new FLuaContext();
#endif
	if (NewContext)
	{
		if (NewContext->Initialize(SourceCode, Owner))
		{
			// Push values set by CDO
			if (Class && Owner)
			{
				NewContext->PushScriptPropertyValues(Class, Owner);
			}
		}
		else
		{
			delete NewContext;
			NewContext = NULL;
		}
	}
	return NewContext;
}

void FScriptContextBase::GetSupportedScriptFileFormats(TArray<FString>& OutFormats)
{
#if WITH_LUA
	OutFormats.Add(TEXT("lua;Script"));
#endif
}