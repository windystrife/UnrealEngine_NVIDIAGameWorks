// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyHelper.h"
#include "UObject/LinkerPlaceholderFunction.h"

/*-----------------------------------------------------------------------------
	UDelegateProperty.
-----------------------------------------------------------------------------*/

void UDelegateProperty::InstanceSubobjects(void* Data, void const* DefaultData, UObject* Owner, FObjectInstancingGraph* InstanceGraph )
{
	for( int32 i=0; i<ArrayDim; i++ )
	{
		FScriptDelegate& DestDelegate = ((FScriptDelegate*)Data)[i];
		UObject* CurrentUObject = DestDelegate.GetUObject();

		if (CurrentUObject)
		{
			UObject *Template = NULL;

			if (DefaultData)
			{
				FScriptDelegate& DefaultDelegate = ((FScriptDelegate*)DefaultData)[i];
				Template = DefaultDelegate.GetUObject();
			}

			UObject* NewUObject = InstanceGraph->InstancePropertyValue(Template, CurrentUObject, Owner, HasAnyPropertyFlags(CPF_Transient), false, true);
			DestDelegate.BindUFunction(NewUObject, DestDelegate.GetFunctionName());
		}
	}
}

bool UDelegateProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	bool bResult = false;

	FScriptDelegate* DA = (FScriptDelegate*)A;
	FScriptDelegate* DB = (FScriptDelegate*)B;
	
	if( DB == NULL )
	{
		bResult = DA->GetFunctionName() == NAME_None;
	}
	else if ( DA->GetFunctionName() == DB->GetFunctionName() )
	{
		if ( DA->GetUObject() == DB->GetUObject() )
		{
			bResult = true;
		}
		else if	((DA->GetUObject() == NULL || DB->GetUObject() == NULL)
			&&	(PortFlags&PPF_DeltaComparison) != 0)
		{
			bResult = true;
		}
	}

	return bResult;
}


void UDelegateProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	Ar << *GetPropertyValuePtr(Value);
}


bool UDelegateProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	// Do not allow replication of delegates, as there is no way to make this secure (it allows the execution of any function in any object, on the remote client/server)
	return 1;
}


FString UDelegateProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	FString UnmangledFunctionName = SignatureFunction->GetName().LeftChop( FString( HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX ).Len() );
	const bool bBlueprintCppBackend = (0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_BlueprintCppBackend));
	const bool bNative = SignatureFunction->IsNative();
	if (bBlueprintCppBackend && bNative)
	{
		UStruct* StructOwner = Cast<UStruct>(SignatureFunction->GetOuter());
		if (StructOwner)
		{
			return FString::Printf(TEXT("%s%s::F%s"), StructOwner->GetPrefixCPP(), *StructOwner->GetName(), *UnmangledFunctionName);
		}
	}
	else
	{
		const bool NonNativeClassOwner = (SignatureFunction->GetOwnerClass() && !SignatureFunction->GetOwnerClass()->HasAnyClassFlags(CLASS_Native));
		if (bBlueprintCppBackend && NonNativeClassOwner)
		{
			// The name must be valid, this removes spaces, ?, etc from the user's function name. It could
			// be slightly shorter because the postfix ("__pf") is not needed here because we further post-
			// pend to the string. Normally the postfix is needed to make sure we don't mangle to a valid
			// identifier and collide:
			UnmangledFunctionName = UnicodeToCPPIdentifier(UnmangledFunctionName, false, TEXT(""));
			// the name must be unique
			const FString OwnerName = UnicodeToCPPIdentifier(SignatureFunction->GetOwnerClass()->GetName(), false, TEXT(""));
			const FString NewUnmangledFunctionName = FString::Printf(TEXT("%s__%s"), *UnmangledFunctionName, *OwnerName);
			UnmangledFunctionName = NewUnmangledFunctionName;
		}
		if (0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_CustomTypeName))
		{
			UnmangledFunctionName += TEXT("__SinglecastDelegate");
		}
	}
	return FString(TEXT("F")) + UnmangledFunctionName;
}

FString UDelegateProperty::GetCPPTypeForwardDeclaration() const
{
	return FString();
}

void UDelegateProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += TEXT("{}");
		return;
	}

	FScriptDelegate* ScriptDelegate = (FScriptDelegate*)PropertyValue;
	check(ScriptDelegate != NULL);
	bool bDelegateHasValue = ScriptDelegate->GetFunctionName() != NAME_None;
	ValueStr += FString::Printf( TEXT("%s.%s"),
		ScriptDelegate->GetUObject() != NULL ? *ScriptDelegate->GetUObject()->GetName() : TEXT("(null)"),
		*ScriptDelegate->GetFunctionName().ToString() );
}


const TCHAR* UDelegateProperty::ImportText_Internal( const TCHAR* Buffer, void* PropertyValue, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	return DelegatePropertyTools::ImportDelegateFromText( *(FScriptDelegate*)PropertyValue, SignatureFunction, Buffer, Parent, ErrorText );
}

void UDelegateProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << SignatureFunction;

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (Ar.IsLoading() || Ar.IsObjectReferenceCollector())
	{
		if (auto PlaceholderFunc = Cast<ULinkerPlaceholderFunction>(SignatureFunction))
		{
			PlaceholderFunc->AddReferencingProperty(this);
		}
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

bool UDelegateProperty::SameType(const UProperty* Other) const
{
	return Super::SameType(Other) && (SignatureFunction == ((UDelegateProperty*)Other)->SignatureFunction);
}

void UDelegateProperty::BeginDestroy()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (auto PlaceholderFunc = Cast<ULinkerPlaceholderFunction>(SignatureFunction))
	{
		PlaceholderFunc->RemoveReferencingProperty(this);
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	Super::BeginDestroy();
}


IMPLEMENT_CORE_INTRINSIC_CLASS(UDelegateProperty, UProperty,
	{
		Class->EmitObjectReference(STRUCT_OFFSET(UDelegateProperty, SignatureFunction), TEXT("SignatureFunction"));
	}
);
