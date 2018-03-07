// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/Casts.h"
#include "UObject/UnrealType.h"
#include "UObject/LinkerPlaceholderClass.h"

/*-----------------------------------------------------------------------------
	USoftClassProperty.
-----------------------------------------------------------------------------*/

void USoftClassProperty::BeginDestroy()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(MetaClass))
	{
		PlaceholderClass->RemoveReferencingProperty(this);
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	Super::BeginDestroy();
}

FString USoftClassProperty::GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags) const
{
	check(MetaClass);
	return GetCPPTypeCustom(ExtendedTypeText, CPPExportFlags, 
		FString::Printf(TEXT("%s%s"), MetaClass->GetPrefixCPP(), *MetaClass->GetName()));
}
FString USoftClassProperty::GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName) const
{
	ensure(!InnerNativeTypeName.IsEmpty());
	return FString::Printf(TEXT("TSoftClassPtr<%s> "), *InnerNativeTypeName);
}
FString USoftClassProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("TSoftClassPtr<%s%s> "),MetaClass->GetPrefixCPP(),*MetaClass->GetName());
	return TEXT("SOFTOBJECT");
}

FString USoftClassProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("class %s%s;"), MetaClass->GetPrefixCPP(), *MetaClass->GetName());
}

void USoftClassProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << MetaClass;

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (Ar.IsLoading() || Ar.IsObjectReferenceCollector())
	{
		if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(MetaClass))
		{
			PlaceholderClass->AddReferencingProperty(this);
		}
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	if( !(MetaClass||HasAnyFlags(RF_ClassDefaultObject)) )
	{
		// If we failed to load the MetaClass and we're not a CDO, that means we relied on a class that has been removed or doesn't exist.
		// The most likely cause for this is either an incomplete recompile, or if content was migrated between games that had native class dependencies
		// that do not exist in this game.  We allow blueprint classes to continue, because compile on load will error out, and stub the class that was using it
		UClass* TestClass = dynamic_cast<UClass*>(GetOwnerStruct());
		if( TestClass && TestClass->HasAllClassFlags(CLASS_Native) && !TestClass->HasAllClassFlags(CLASS_NewerVersionExists) && (TestClass->GetOutermost() != GetTransientPackage()) )
		{
			checkf(false, TEXT("Class property tried to serialize a missing class.  Did you remove a native class and not fully recompile?"));
		}
	}
}

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
void USoftClassProperty::SetMetaClass(UClass* NewMetaClass)
{
	if (ULinkerPlaceholderClass* NewPlaceholderClass = Cast<ULinkerPlaceholderClass>(NewMetaClass))
	{
		NewPlaceholderClass->AddReferencingProperty(this);
	}

	if (ULinkerPlaceholderClass* OldPlaceholderClass = Cast<ULinkerPlaceholderClass>(MetaClass))
	{
		OldPlaceholderClass->RemoveReferencingProperty(this);
	}
	MetaClass = NewMetaClass;
}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

void USoftClassProperty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	USoftClassProperty* This = CastChecked<USoftClassProperty>(InThis);
	Collector.AddReferencedObject( This->MetaClass, This );
	Super::AddReferencedObjects( This, Collector );
}

bool USoftClassProperty::SameType(const UProperty* Other) const
{
	return Super::SameType(Other) && (MetaClass == ((USoftClassProperty*)Other)->MetaClass);
}

IMPLEMENT_CORE_INTRINSIC_CLASS(USoftClassProperty, USoftObjectProperty,
	{
		Class->EmitObjectReference(STRUCT_OFFSET(USoftClassProperty, MetaClass), TEXT("MetaClass"));
	}
);
