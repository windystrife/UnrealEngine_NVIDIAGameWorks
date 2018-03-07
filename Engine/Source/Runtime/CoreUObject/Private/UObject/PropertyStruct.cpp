// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyHelper.h"
#include "UObject/LinkerPlaceholderBase.h"

static inline void PreloadInnerStructMembers(UStructProperty* StructProperty)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	uint32 PropagatedLoadFlags = 0;
	if (FLinkerLoad* Linker = StructProperty->GetLinker())
	{
		PropagatedLoadFlags |= (Linker->LoadFlags & LOAD_DeferDependencyLoads);
	}

	if (UScriptStruct* Struct = StructProperty->Struct)
	{
		if (FLinkerLoad* StructLinker = Struct->GetLinker())
		{
			TGuardValue<uint32> LoadFlagGuard(StructLinker->LoadFlags, StructLinker->LoadFlags | PropagatedLoadFlags);
			Struct->RecursivelyPreload();
		}
	}
#else // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	StructProperty->Struct->RecursivelyPreload();
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

/*-----------------------------------------------------------------------------
	UStructProperty.
-----------------------------------------------------------------------------*/

UStructProperty::UStructProperty(ECppProperty, int32 InOffset, uint64 InFlags, UScriptStruct* InStruct)
	: UProperty(FObjectInitializer::Get(), EC_CppProperty, InOffset, InStruct->GetCppStructOps() ? InStruct->GetCppStructOps()->GetComputedPropertyFlags() | InFlags : InFlags)
	, Struct(InStruct)
{
	ElementSize = Struct->PropertiesSize;
}

UStructProperty::UStructProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UScriptStruct* InStruct )
	:	UProperty( ObjectInitializer, EC_CppProperty, InOffset, InStruct->GetCppStructOps() ? InStruct->GetCppStructOps()->GetComputedPropertyFlags() | InFlags : InFlags )
	,	Struct( InStruct )
{
	ElementSize = Struct->PropertiesSize;
}

int32 UStructProperty::GetMinAlignment() const
{
	return Struct->GetMinAlignment();
}

void UStructProperty::LinkInternal(FArchive& Ar)
{
	// We potentially have to preload the property itself here, if we were the inner of an array property
	if(HasAnyFlags(RF_NeedLoad))
	{
		GetLinker()->Preload(this);
	}

	if (Struct)
	{
		// Preload is required here in order to load the value of Struct->PropertiesSize
		Ar.Preload(Struct);
	}
	else
	{
		UE_LOG(LogProperty, Error, TEXT("Struct type unknown for property '%s'; perhaps the USTRUCT() was renamed or deleted?"), *GetFullName());
		Struct = GetFallbackStruct();
	}
	PreloadInnerStructMembers(this);
	
	ElementSize = Align(Struct->PropertiesSize, Struct->GetMinAlignment());
	if(UScriptStruct::ICppStructOps* Ops = Struct->GetCppStructOps())
	{
		PropertyFlags |= Ops->GetComputedPropertyFlags();
	}
	else
	{
		// User Defined structs won't have UScriptStruct::ICppStructOps. Setting their flags here.
		PropertyFlags |= CPF_HasGetValueTypeHash;
	}
}

bool UStructProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	return Struct->CompareScriptStruct(A, B, PortFlags);
}

bool UStructProperty::UseBinaryOrNativeSerialization(const FArchive& Ar) const
{
	check(Struct);

	const bool bUseBinarySerialization = Struct->UseBinarySerialization(Ar);
	const bool bUseNativeSerialization = Struct->UseNativeSerialization();
	return bUseBinarySerialization || bUseNativeSerialization;
}

uint32 UStructProperty::GetValueTypeHashInternal(const void* Src) const
{
	check(Struct);
	return Struct->GetStructTypeHash(Src);
}

void UStructProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	check(Struct);

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	FScopedPlaceholderPropertyTracker ImportPropertyTracker(this);
#endif

	Struct->SerializeItem(Ar, Value, Defaults);
}

bool UStructProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	//------------------------------------------------
	//	Custom NetSerialization
	//------------------------------------------------
	if (Struct->StructFlags & STRUCT_NetSerializeNative)
	{
		UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
		check(CppStructOps); // else should not have STRUCT_NetSerializeNative
		bool bSuccess = true;
		bool bMapped = CppStructOps->NetSerialize(Ar, Map, bSuccess, Data);
		if (!bSuccess)
		{
			UE_LOG(LogProperty, Warning, TEXT("Native NetSerialize %s (%s) failed."), *GetFullName(), *Struct->GetFullName() );
		}
		return bMapped;
	}

	UE_LOG( LogProperty, Fatal, TEXT( "Deprecated code path" ) );

	return 1;
}

void UStructProperty::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	OutDeps.Add(Struct);
}

void UStructProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	static UScriptStruct* FallbackStruct = GetFallbackStruct();
	
	if (Ar.IsPersistent() && Ar.GetLinker() && Ar.IsLoading() && !Struct)
	{
		// It's necessary to solve circular dependency problems, when serializing the Struct causes linking of the Property.
		Struct = FallbackStruct;
	}

	Ar << Struct;
#if WITH_EDITOR
	if (Ar.IsPersistent() && Ar.GetLinker())
	{
		if (!Struct && Ar.IsLoading())
		{
			UE_LOG(LogProperty, Error, TEXT("UStructProperty::Serialize Loading: Property '%s'. Unknown structure."), *GetFullName());
			Struct = FallbackStruct;
		}
		else if ((FallbackStruct == Struct) && Ar.IsSaving())
		{
			UE_LOG(LogProperty, Error, TEXT("UStructProperty::Serialize Saving: Property '%s'. FallbackStruct structure."), *GetFullName());
		}
	}
#endif // WITH_EDITOR
	if (Struct)
	{
		PreloadInnerStructMembers(this);
	}
	else
	{
		ensure(true);
	}
}
void UStructProperty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UStructProperty* This = CastChecked<UStructProperty>(InThis);
	Collector.AddReferencedObject( This->Struct, This );
	Super::AddReferencedObjects( This, Collector );
}

#if HACK_HEADER_GENERATOR

bool UStructProperty::HasNoOpConstructor() const
{
	Struct->PrepareCppStructOps();
	UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
	if (CppStructOps && CppStructOps->HasNoopConstructor())
	{
		return true;
	}
	return false;
}

#endif

FString UStructProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	return Struct->GetStructCPPName();
}

FString UStructProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("struct F%s;"), *Struct->GetName());
}

FString UStructProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = GetCPPType(NULL, CPPF_None);
	return TEXT("STRUCT");
}

void UStructProperty::ExportTextItem_Static(UScriptStruct* InStruct, FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope)
{
	// For backward compatibility skip the native export 
	InStruct->ExportText(ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, ExportRootScope, false);
}

void UStructProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	Struct->ExportText(ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, ExportRootScope, true);
}

const TCHAR* UStructProperty::ImportText_Internal(const TCHAR* InBuffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText) const
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	FScopedPlaceholderPropertyTracker ImportPropertyTracker(this);
#endif 
	return Struct->ImportText(InBuffer, Data, Parent, PortFlags, ErrorText, GetName(), true);
}

const TCHAR* UStructProperty::ImportText_Static(UScriptStruct* InStruct, const FString& Name, const TCHAR* InBuffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	return InStruct->ImportText(InBuffer, Data, Parent, PortFlags, ErrorText, Name, true);
}

void UStructProperty::CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const
{
	Struct->CopyScriptStruct(Dest, Src, Count);
}

void UStructProperty::InitializeValueInternal( void* InDest ) const
{
	Struct->InitializeStruct(InDest, ArrayDim);
}

void UStructProperty::ClearValueInternal( void* Data ) const
{
	Struct->ClearScriptStruct(Data, 1); // clear only does one value
}

void UStructProperty::DestroyValueInternal( void* Dest ) const
{
	Struct->DestroyStruct(Dest, ArrayDim);
}

/**
 * Creates new copies of components
 * 
 * @param	Data				pointer to the address of the instanced object referenced by this UComponentProperty
 * @param	DefaultData			pointer to the address of the default value of the instanced object referenced by this UComponentProperty
 * @param	Owner				the object that contains this property's data
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void UStructProperty::InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, FObjectInstancingGraph* InstanceGraph )
{
	for (int32 Index = 0; Index < ArrayDim; Index++)
	{
		Struct->InstanceSubobjectTemplates( (uint8*)Data + ElementSize * Index, DefaultData ? (uint8*)DefaultData + ElementSize * Index : NULL, Struct, Owner, InstanceGraph );
	}
}

bool UStructProperty::SameType(const UProperty* Other) const
{
	return Super::SameType(Other) && (Struct == ((UStructProperty*)Other)->Struct);
}

bool UStructProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty)
{
	bOutAdvanceProperty = false;

	auto CanSerializeFromStructWithDifferentName = [](const FArchive& InAr, const FPropertyTag& PropertyTag, const UStructProperty* StructProperty)
	{
		if (InAr.UE4Ver() < VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG)
		{
			// Old Implementation
			return StructProperty && !StructProperty->UseBinaryOrNativeSerialization(InAr);
		}
		return PropertyTag.StructGuid.IsValid() && StructProperty && StructProperty->Struct && (PropertyTag.StructGuid == StructProperty->Struct->GetCustomGuid());
	};

	if (Struct)
	{
		if ((Struct->StructFlags & STRUCT_SerializeFromMismatchedTag) && (Tag.Type != NAME_StructProperty || (Tag.StructName != Struct->GetFName())))
		{
			UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
			check(CppStructOps && CppStructOps->HasSerializeFromMismatchedTag()); // else should not have STRUCT_SerializeFromMismatchedTag
			void* DestAddress = ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex);
			if (CppStructOps->SerializeFromMismatchedTag(Tag, Ar, DestAddress))
			{
				bOutAdvanceProperty = true;
			}			
			else
			{
				UE_LOG(LogClass, Warning, TEXT("SerializeFromMismatchedTag failed: Type mismatch in %s of %s - Previous (%s) Current(StructProperty) for package:  %s"), *Tag.Name.ToString(), *GetName(), *Tag.Type.ToString(), *Ar.GetArchiveName());
			}
			return true;
		}
		else if (Tag.Type == NAME_StructProperty && Tag.StructName != Struct->GetFName() && !CanSerializeFromStructWithDifferentName(Ar, Tag, this))
		{
			//handle Vector -> Vector4 upgrades here because using the SerializeFromMismatchedTag system would cause a dependency from Core -> CoreUObject
			if (Tag.StructName == NAME_Vector && Struct->GetFName() == NAME_Vector4)
			{
				void* DestAddress = ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex);
				FVector OldValue;
				Ar << OldValue;

				//only set X/Y/Z.  The W should already have been set to the property specific default and we don't want to trash it by forcing 0 or 1.
				FVector4* DestValue = (FVector4*)DestAddress;				
				DestValue->X = OldValue.X;
				DestValue->Y = OldValue.Y;
				DestValue->Z = OldValue.Z;
			}
			else
			{
				UE_LOG(LogClass, Warning, TEXT("Property %s of %s has a struct type mismatch (tag %s != prop %s) in package:  %s. If that struct got renamed, add an entry to ActiveStructRedirects."),
					*Tag.Name.ToString(), *GetName(), *Tag.StructName.ToString(), *Struct->GetName(), *Ar.GetArchiveName());
			}

			return true;
		}
	}
	return false;
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UStructProperty, UProperty,
	{
		Class->EmitObjectReference(STRUCT_OFFSET(UStructProperty, Struct), TEXT("Struct"));
	}
);
