// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyHelper.h"

/*-----------------------------------------------------------------------------
	ULazyObjectProperty.
-----------------------------------------------------------------------------*/

FString ULazyObjectProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	return FString::Printf( TEXT("TLazyObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName() );
}
FString ULazyObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("TLazyObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("LAZYOBJECT");
}

FName ULazyObjectProperty::GetID() const
{
	return NAME_LazyObjectProperty;
}

void ULazyObjectProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want lazy pointers to keep objects from being garbage collected

	if( !Ar.IsObjectReferenceCollector() || Ar.IsModifyingWeakAndStrongReferences() )
	{
		UObject* ObjectValue = GetObjectPropertyValue(Value);

		Ar << *(FLazyObjectPtr*)Value;

		if ((Ar.IsLoading() || Ar.IsModifyingWeakAndStrongReferences()) && ObjectValue != GetObjectPropertyValue(Value))
		{
			CheckValidObject(Value);
		}
	}
}

bool ULazyObjectProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	FLazyObjectPtr ObjectA = A ? *((FLazyObjectPtr*)A) : FLazyObjectPtr();
	FLazyObjectPtr ObjectB = B ? *((FLazyObjectPtr*)B) : FLazyObjectPtr();

	// Compare actual pointers. We don't do this during PIE because we want to be sure to serialize everything. An example is the LevelScriptActor being serialized against its CDO,
	// which contains actor references. We want to serialize those references so they are fixed up.
	const bool bDuplicatingForPIE = (PortFlags&PPF_DuplicateForPIE) != 0;
	bool bResult = !bDuplicatingForPIE ? (ObjectA == ObjectB) : false;
	// always serialize the cross level references, because they could be NULL
	// @todo: okay, this is pretty hacky overall - we should have a PortFlag or something
	// that is set during SavePackage. Other times, we don't want to immediately return false
	// (instead of just this ExportDefProps case)
	// instance testing
	if (!bResult && ObjectA.IsValid() && ObjectB.IsValid() && ObjectA->GetClass() == ObjectB->GetClass())
	{
		bool bPerformDeepComparison = (PortFlags&PPF_DeepComparison) != 0;
		if ((PortFlags&PPF_DeepCompareInstances) && !bPerformDeepComparison)
		{
			bPerformDeepComparison = ObjectA->IsTemplate() != ObjectB->IsTemplate();
		}

		if (!bResult && bPerformDeepComparison)
		{
			// In order for deep comparison to be match they both need to have the same name and that name needs to be included in the instancing table for the class
			if (ObjectA->GetFName() == ObjectB->GetFName() && ObjectA->GetClass()->GetDefaultSubobjectByName(ObjectA->GetFName()))
			{
				checkSlow(ObjectA->IsDefaultSubobject() && ObjectB->IsDefaultSubobject() && ObjectA->GetClass()->GetDefaultSubobjectByName(ObjectA->GetFName()) == ObjectB->GetClass()->GetDefaultSubobjectByName(ObjectB->GetFName())); // equivalent
				bResult = AreInstancedObjectsIdentical(ObjectA.Get(), ObjectB.Get(), PortFlags);
			}
		}
	}
	return bResult;
}

UObject* ULazyObjectProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress).Get();
}

void ULazyObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	SetPropertyValue(PropertyValueAddress, TCppType(Value));
}

bool ULazyObjectProperty::AllowCrossLevel() const
{
	return true;
}

uint32 ULazyObjectProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(GetPropertyValue(Src));
}

IMPLEMENT_CORE_INTRINSIC_CLASS(ULazyObjectProperty, UObjectPropertyBase,
	{
	}
);

