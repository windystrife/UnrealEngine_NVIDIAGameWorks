// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Blueprint/BlueprintSupport.h"
#include "UObject/LinkerPlaceholderBase.h"
#include "UObject/LinkerPlaceholderExportObject.h"
#include "UObject/LinkerPlaceholderClass.h"

/*-----------------------------------------------------------------------------
	UObjectProperty.
-----------------------------------------------------------------------------*/

FString UObjectProperty::GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName)  const
{
	ensure(!InnerNativeTypeName.IsEmpty());
	return FString::Printf(TEXT("%s*"), *InnerNativeTypeName);
}

FString UObjectProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("class %s%s;"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}

FString UObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("%s%s"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("OBJECT");
}

bool UObjectProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty)
{
	static FName NAME_AssetObjectProperty = "AssetObjectProperty"; // old name of soft object property

	if (Tag.Type == NAME_SoftObjectProperty || Tag.Type == NAME_AssetObjectProperty)
	{
		// This property used to be a TSoftObjectPtr<Foo> but is now a raw UObjectProperty Foo*, we can convert without loss of data
		FSoftObjectPtr PreviousValue;
		Ar << PreviousValue;

		// now copy the value into the object's address space
		UObject* PreviousValueObj = PreviousValue.LoadSynchronous();
		SetPropertyValue_InContainer(Data, PreviousValueObj, Tag.ArrayIndex);

		// Validate the type is proper
		CheckValidObject(GetPropertyValuePtr_InContainer(Data, Tag.ArrayIndex));

		return true;
	}

	return false;
}

void UObjectProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	if (Ar.IsObjectReferenceCollector())
	{
		// Serialize in place
		UObject** ObjectPtr = GetPropertyValuePtr(Value);
		Ar << (*ObjectPtr);
	}
	else
	{
		UObject* ObjectValue = GetObjectPropertyValue(Value);
		Ar << ObjectValue;

		UObject* CurrentValue = GetObjectPropertyValue(Value);
		if (ObjectValue != CurrentValue)
		{
			SetObjectPropertyValue(Value, ObjectValue);

	#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			if (ULinkerPlaceholderExportObject* PlaceholderVal = Cast<ULinkerPlaceholderExportObject>(ObjectValue))
			{
				PlaceholderVal->AddReferencingPropertyValue(this, Value);
			}
			else if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(ObjectValue))
			{
				PlaceholderClass->AddReferencingPropertyValue(this, Value);
			}
			// NOTE: we don't remove this from CurrentValue if it is a 
			//       ULinkerPlaceholderExportObject; this is because this property 
			//       could be an array inner, and another member of that array (also 
			//       referenced through this property)... if this becomes a problem,
			//       then we could inc/decrement a ref count per referencing property 
			//
			// @TODO: if this becomes problematic (because ObjectValue doesn't match 
			//        this property's PropertyClass), then we could spawn another
			//        placeholder object (of PropertyClass's type), or use null; but
			//        we'd have to modify ULinkerPlaceholderExportObject::ReplaceReferencingObjectValues()
			//        to accommodate this (as it depends on finding itself as the set value)
	#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

			CheckValidObject(Value);
		}
	}
}

const TCHAR* UObjectProperty::ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	const TCHAR* Result = TUObjectPropertyBase<UObject*>::ImportText_Internal(Buffer, Data, PortFlags, OwnerObject, ErrorText);
	if (Result)
	{
		CheckValidObject(Data);

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
		UObject* ObjectValue = GetObjectPropertyValue(Data);

		if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(ObjectValue))
		{
			// we use this tracker mechanism to help record the instance that is
			// referencing the placeholder (so we can replace it later on fixup)
			FScopedPlaceholderContainerTracker ImportingObjTracker(OwnerObject);

			PlaceholderClass->AddReferencingPropertyValue(this, Data);
		}
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		else
		{
			// as far as we know, ULinkerPlaceholderClass is the only type we have to handle through ImportText()
			check(!FBlueprintSupport::IsDeferredDependencyPlaceholder(ObjectValue));
		}
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	}
	return Result;
}

uint32 UObjectProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(GetPropertyValue(Src));
}

UObject* UObjectProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress);
}

void UObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	SetPropertyValue(PropertyValueAddress, Value);
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectProperty, UObjectPropertyBase,
	{
	}
);

