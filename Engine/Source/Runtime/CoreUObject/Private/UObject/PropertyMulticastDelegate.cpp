// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyHelper.h"
#include "UObject/LinkerPlaceholderFunction.h"

/*-----------------------------------------------------------------------------
	UMulticastDelegateProperty.
-----------------------------------------------------------------------------*/

void UMulticastDelegateProperty::InstanceSubobjects(void* Data, void const* DefaultData, UObject* Owner, FObjectInstancingGraph* InstanceGraph )
{
	if (DefaultData)
	{
		for( int32 i=0; i<ArrayDim; i++ )
		{
			FMulticastScriptDelegate& DestDelegate = ((FMulticastScriptDelegate*)Data)[i];
			FMulticastScriptDelegate& DefaultDelegate = ((FMulticastScriptDelegate*)DefaultData)[i];

			// Fix up references to the class default object (if necessary)
			FMulticastScriptDelegate::FInvocationList::TIterator CurInvocation( DestDelegate.InvocationList );
			FMulticastScriptDelegate::FInvocationList::TIterator DefaultInvocation( DefaultDelegate.InvocationList );
			for(; CurInvocation && DefaultInvocation; ++CurInvocation, ++DefaultInvocation )
			{
				FScriptDelegate& DestDelegateInvocation = *CurInvocation;
				UObject* CurrentUObject = DestDelegateInvocation.GetUObject();

				if (CurrentUObject)
				{
					FScriptDelegate& DefaultDelegateInvocation = *DefaultInvocation;
					UObject *Template = DefaultDelegateInvocation.GetUObject();
					UObject* NewUObject = InstanceGraph->InstancePropertyValue(Template, CurrentUObject, Owner, HasAnyPropertyFlags(CPF_Transient), false, true);
					DestDelegateInvocation.BindUFunction(NewUObject, DestDelegateInvocation.GetFunctionName());
				}
			}
			// now finish up the ones for which there is no default
			for(; CurInvocation; ++CurInvocation )
			{
				FScriptDelegate& DestDelegateInvocation = *CurInvocation;
				UObject* CurrentUObject = DestDelegateInvocation.GetUObject();

				if (CurrentUObject)
				{
					UObject* NewUObject = InstanceGraph->InstancePropertyValue(NULL, CurrentUObject, Owner, HasAnyPropertyFlags(CPF_Transient), false, true);
					DestDelegateInvocation.BindUFunction(NewUObject, DestDelegateInvocation.GetFunctionName());
				}
			}
		}
	}
	else // no default data 
	{
		for( int32 i=0; i<ArrayDim; i++ )
		{
			FMulticastScriptDelegate& DestDelegate = ((FMulticastScriptDelegate*)Data)[i];

			for( FMulticastScriptDelegate::FInvocationList::TIterator CurInvocation( DestDelegate.InvocationList ); CurInvocation; ++CurInvocation )
			{
				FScriptDelegate& DestDelegateInvocation = *CurInvocation;
				UObject* CurrentUObject = DestDelegateInvocation.GetUObject();

				if (CurrentUObject)
				{
					UObject* NewUObject = InstanceGraph->InstancePropertyValue(NULL, CurrentUObject, Owner, HasAnyPropertyFlags(CPF_Transient), false, true);
					DestDelegateInvocation.BindUFunction(NewUObject, DestDelegateInvocation.GetFunctionName());
				}
			}
		}
	}
}

bool UMulticastDelegateProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	bool bResult = false;

	FMulticastScriptDelegate* DA = (FMulticastScriptDelegate*)A;
	FMulticastScriptDelegate* DB = (FMulticastScriptDelegate*)B;
	
	if( DB == NULL )
	{
		bResult = DA->InvocationList.Num() == 0;
	}
	else if ( DA->InvocationList.Num() == DB->InvocationList.Num() )
	{
		bResult = true;
		for( int32 CurInvocationIndex = 0; CurInvocationIndex < DA->InvocationList.Num(); ++CurInvocationIndex )
		{
			const FScriptDelegate& InvocationA = DA->InvocationList[ CurInvocationIndex ];
			const FScriptDelegate& InvocationB = DB->InvocationList[ CurInvocationIndex ];
			
			if( InvocationA.GetUObject() != InvocationB.GetUObject() ||
				!( (PortFlags&PPF_DeltaComparison) != 0 && ( InvocationA.GetUObject() == NULL || InvocationB.GetUObject() == NULL ) ) )
			{
				bResult = false;
				break;
			}
		}
	}

	return bResult;
}

void UMulticastDelegateProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	Ar << *GetPropertyValuePtr(Value);
}

bool UMulticastDelegateProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	// Do not allow replication of delegates, as there is no way to make this secure (it allows the execution of any function in any object, on the remote client/server)
	return 1;
}


FString UMulticastDelegateProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
#if HACK_HEADER_GENERATOR
	// We have this test because sometimes the delegate hasn't been set up by FixupDelegateProperties at the time
	// we need the type for an error message.  We deliberately format it so that it's unambiguously not CPP code, but is still human-readable.
	if (!SignatureFunction)
	{
		return FString(TEXT("{multicast delegate type}"));
	}
#endif

	FString UnmangledFunctionName = SignatureFunction->GetName().LeftChop( FString( HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX ).Len() );
	const UClass* OwnerClass = SignatureFunction->GetOwnerClass();

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
		if ((0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_BlueprintCppBackend)) && OwnerClass && !OwnerClass->HasAnyClassFlags(CLASS_Native))
		{
			// The name must be valid, this removes spaces, ?, etc from the user's function name. It could
			// be slightly shorter because the postfix ("__pf") is not needed here because we further post-
			// pend to the string. Normally the postfix is needed to make sure we don't mangle to a valid
			// identifier and collide:
			UnmangledFunctionName = UnicodeToCPPIdentifier(UnmangledFunctionName, false, TEXT(""));
			// the name must be unique
			const FString OwnerName = UnicodeToCPPIdentifier(OwnerClass->GetName(), false, TEXT(""));
			const FString NewUnmangledFunctionName = FString::Printf(TEXT("%s__%s"), *UnmangledFunctionName, *OwnerName);
			UnmangledFunctionName = NewUnmangledFunctionName;
		}
		if (0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_CustomTypeName))
		{
			UnmangledFunctionName += TEXT("__MulticastDelegate");
		}
	}
	return FString(TEXT("F")) + UnmangledFunctionName;
}


void UMulticastDelegateProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += TEXT("{}");
		return;
	}

	const FMulticastScriptDelegate* MulticastDelegate = (const FMulticastScriptDelegate*)( PropertyValue );
	check( MulticastDelegate != NULL );

	// Start delegate array with open paren
	ValueStr += TEXT( "(" );

	bool bIsFirstFunction = true;
	for( FMulticastScriptDelegate::FInvocationList::TConstIterator CurInvocation( MulticastDelegate->InvocationList ); CurInvocation; ++CurInvocation )
	{
		if( CurInvocation->IsBound() )
		{
			if( !bIsFirstFunction )
			{
				ValueStr += TEXT( "," );
			}
			bIsFirstFunction = false;

			bool bDelegateHasValue = CurInvocation->GetFunctionName() != NAME_None;
			ValueStr += FString::Printf( TEXT("%s.%s"),
				CurInvocation->GetUObject() != NULL ? *CurInvocation->GetUObject()->GetName() : TEXT("(null)"),
				*CurInvocation->GetFunctionName().ToString() );
		}
	}

	// Close the array (NOTE: It could be empty, but that's fine.)
	ValueStr += TEXT( ")" );
}


const TCHAR* UMulticastDelegateProperty::ImportText_Internal( const TCHAR* Buffer, void* PropertyValue, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	// Multi-cast delegates always expect an opening parenthesis when using assignment syntax, so that
	// users don't accidentally blow away already-bound delegates in DefaultProperties.  This also helps
	// to differentiate between single-cast and multi-cast delegates
	if( *Buffer != TCHAR( '(' ) )
	{
		return NULL;
	}

	FMulticastScriptDelegate& MulticastDelegate = (*(FMulticastScriptDelegate*)PropertyValue);

	// Clear the existing delegate
	MulticastDelegate.Clear();

	// process opening parenthesis
	++Buffer;
	SkipWhitespace(Buffer);

	// Empty Multi-cast delegates is still valid.
	if (*Buffer == TCHAR(')'))
	{
		return Buffer;
	}

	do
	{
		// Parse the delegate
		FScriptDelegate ImportedDelegate;
		Buffer = DelegatePropertyTools::ImportDelegateFromText( ImportedDelegate, SignatureFunction, Buffer, Parent, ErrorText );
		if( Buffer == NULL )
		{
			return NULL;
		}

		// Add this delegate to our multicast delegate's invocation list
		MulticastDelegate.AddUnique( ImportedDelegate );

		SkipWhitespace(Buffer);
	}
	while( *Buffer == TCHAR(',') && Buffer++ );


	// We expect a closing paren
	if( *( Buffer++ ) != TCHAR(')') )
	{
		return NULL;
	}

	return MulticastDelegate.IsBound() ? Buffer : NULL;
}


const TCHAR* UMulticastDelegateProperty::ImportText_Add( const TCHAR* Buffer, void* PropertyValue, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
	{
		return NULL;
	}

	FMulticastScriptDelegate& MulticastDelegate = (*(FMulticastScriptDelegate*)PropertyValue);

	// Parse the delegate
	FScriptDelegate ImportedDelegate;
	Buffer = DelegatePropertyTools::ImportDelegateFromText( ImportedDelegate, SignatureFunction, Buffer, Parent, ErrorText );
	if( Buffer == NULL )
	{
		return NULL;
	}

	// Add this delegate to our multicast delegate's invocation list
	MulticastDelegate.Add( ImportedDelegate );

	SkipWhitespace(Buffer);

	return Buffer;
}


const TCHAR* UMulticastDelegateProperty::ImportText_Remove( const TCHAR* Buffer, void* PropertyValue, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
	{
		return NULL;
	}

	FMulticastScriptDelegate& MulticastDelegate = (*(FMulticastScriptDelegate*)PropertyValue);

	// Parse the delegate
	FScriptDelegate ImportedDelegate;
	Buffer = DelegatePropertyTools::ImportDelegateFromText( ImportedDelegate, SignatureFunction, Buffer, Parent, ErrorText );
	if( Buffer == NULL )
	{
		return NULL;
	}

	// Remove this delegate to our multicast delegate's invocation list
	MulticastDelegate.Remove( ImportedDelegate );

	SkipWhitespace(Buffer);

	return Buffer;
}


void UMulticastDelegateProperty::Serialize( FArchive& Ar )
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

void UMulticastDelegateProperty::BeginDestroy()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (auto PlaceholderFunc = Cast<ULinkerPlaceholderFunction>(SignatureFunction))
	{
		PlaceholderFunc->RemoveReferencingProperty(this);
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	Super::BeginDestroy();
}

bool UMulticastDelegateProperty::SameType(const UProperty* Other) const
{
	return Super::SameType(Other) && (SignatureFunction == ((UMulticastDelegateProperty*)Other)->SignatureFunction);
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UMulticastDelegateProperty, UProperty,
	{
		Class->EmitObjectReference(STRUCT_OFFSET(UMulticastDelegateProperty, SignatureFunction), TEXT("SignatureFunction"));
	}
);

