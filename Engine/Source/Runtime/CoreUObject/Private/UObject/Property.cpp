// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Property.cpp: UProperty implementation
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Math/RandomStream.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "Templates/Casts.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyHelper.h"
#include "UObject/CoreRedirects.h"
#include "UObject/SoftObjectPath.h"
#include "Math/Box2D.h"

DEFINE_LOG_CATEGORY(LogProperty);

// List the core ones here as they have already been included (and can be used without CoreUObject!)
template<>
struct TStructOpsTypeTraits<FVector> : public TStructOpsTypeTraitsBase2<FVector>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithNetSerializer = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(Vector);

template<>
struct TStructOpsTypeTraits<FIntPoint> : public TStructOpsTypeTraitsBase2<FIntPoint>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(IntPoint);

template<>
struct TStructOpsTypeTraits<FIntVector> : public TStructOpsTypeTraitsBase2<FIntVector>
{
	enum
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(IntVector);

template<>
struct TStructOpsTypeTraits<FVector2D> : public TStructOpsTypeTraitsBase2<FVector2D>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithNetSerializer = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(Vector2D);

template<>
struct TStructOpsTypeTraits<FVector4> : public TStructOpsTypeTraitsBase2<FVector4>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(Vector4);

template<>
struct TStructOpsTypeTraits<FPlane> : public TStructOpsTypeTraitsBase2<FPlane>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithNetSerializer = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(Plane);

template<>
struct TStructOpsTypeTraits<FRotator> : public TStructOpsTypeTraitsBase2<FRotator>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithNetSerializer = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(Rotator);

template<>
struct TStructOpsTypeTraits<FBox> : public TStructOpsTypeTraitsBase2<FBox>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(Box);

template<>
struct TStructOpsTypeTraits<FBox2D> : public TStructOpsTypeTraitsBase2<FBox2D>
{
	enum
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
	};
};
IMPLEMENT_STRUCT(Box2D);

template<>
struct TStructOpsTypeTraits<FMatrix> : public TStructOpsTypeTraitsBase2<FMatrix>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(Matrix);

template<>
struct TStructOpsTypeTraits<FBoxSphereBounds> : public TStructOpsTypeTraitsBase2<FBoxSphereBounds>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
	};
};
IMPLEMENT_STRUCT(BoxSphereBounds);

template<>
struct TStructOpsTypeTraits<FOrientedBox> : public TStructOpsTypeTraitsBase2<FOrientedBox>
{
};
IMPLEMENT_STRUCT(OrientedBox);

template<>
struct TStructOpsTypeTraits<FLinearColor> : public TStructOpsTypeTraitsBase2<FLinearColor>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(LinearColor);

template<>
struct TStructOpsTypeTraits<FColor> : public TStructOpsTypeTraitsBase2<FColor>
{
	enum 
	{
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(Color);


template<>
struct TStructOpsTypeTraits<FQuat> : public TStructOpsTypeTraitsBase2<FQuat>
{
	enum 
	{
		//quat is somewhat special in that it initialized w to one
		WithNoInitConstructor = true,
		WithNetSerializer = true,
	};
};
IMPLEMENT_STRUCT(Quat);

template<>
struct TStructOpsTypeTraits<FTwoVectors> : public TStructOpsTypeTraitsBase2<FTwoVectors>
{
	enum 
	{
		WithZeroConstructor = true,
		WithSerializer = true,
		WithNoDestructor = true,
	};
};
IMPLEMENT_STRUCT(TwoVectors);

template<>
struct TStructOpsTypeTraits<FGuid> : public TStructOpsTypeTraitsBase2<FGuid>
{
	enum 
	{
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithZeroConstructor = true,
		WithSerializer = true,
	};
};
IMPLEMENT_STRUCT(Guid);

template<>
struct TStructOpsTypeTraits<FTransform> : public TStructOpsTypeTraitsBase2<FTransform>
{
};
IMPLEMENT_STRUCT(Transform);

template<>
struct TStructOpsTypeTraits<FRandomStream> : public TStructOpsTypeTraitsBase2<FRandomStream>
{
	enum 
	{
		WithExportTextItem = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
	};
};
IMPLEMENT_STRUCT(RandomStream);

template<>
struct TStructOpsTypeTraits<FDateTime> : public TStructOpsTypeTraitsBase2<FDateTime>
{
	enum 
	{
		WithCopy = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializer = true,
		WithZeroConstructor = true,
		WithIdenticalViaEquality = true,
	};
};
IMPLEMENT_STRUCT(DateTime);

template<>
struct TStructOpsTypeTraits<FTimespan> : public TStructOpsTypeTraitsBase2<FTimespan>
{
	enum 
	{
		WithCopy = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializer = true,
		WithZeroConstructor = true,
		WithIdenticalViaEquality = true,
	};
};
IMPLEMENT_STRUCT(Timespan);

template<>
struct TStructOpsTypeTraits<FSoftObjectPath> : public TStructOpsTypeTraitsBase2<FSoftObjectPath>
{
	enum
	{
		WithZeroConstructor = true,
		WithSerializer = true,
		WithCopy = true,
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializeFromMismatchedTag = true,
	};
};
IMPLEMENT_STRUCT(SoftObjectPath);

template<>
struct TStructOpsTypeTraits<FSoftClassPath> : public TStructOpsTypeTraitsBase2<FSoftClassPath>
{
	enum
	{
		WithZeroConstructor = true,
		WithSerializer = true,
		WithCopy = true,
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializeFromMismatchedTag = true,
	};
};
IMPLEMENT_STRUCT(SoftClassPath);

template<>
struct TStructOpsTypeTraits<FPrimaryAssetType> : public TStructOpsTypeTraitsBase2<FPrimaryAssetType>
{
	enum
	{
		WithZeroConstructor = true,
		WithCopy = true,
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializeFromMismatchedTag = true,
	};
};
IMPLEMENT_STRUCT(PrimaryAssetType);

template<>
struct TStructOpsTypeTraits<FPrimaryAssetId> : public TStructOpsTypeTraitsBase2<FPrimaryAssetId>
{
	enum
	{
		WithZeroConstructor = true,
		WithCopy = true,
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializeFromMismatchedTag = true,
	};
};
IMPLEMENT_STRUCT(PrimaryAssetId);
/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/

//
// Parse a token.
//
const TCHAR* UPropertyHelpers::ReadToken( const TCHAR* Buffer, FString& String, bool DottedNames )
{
	if( *Buffer == TCHAR('"') )
	{
		int32 NumCharsRead = 0;
		if (!FParse::QuotedString(Buffer, String, &NumCharsRead))
		{
			UE_LOG(LogProperty, Warning, TEXT("ReadToken: Bad quoted string") );
			return nullptr;
		}
		Buffer += NumCharsRead;
	}
	else if( FChar::IsAlnum( *Buffer ) || (DottedNames && (*Buffer==TCHAR('/'))) || (*Buffer > 255) )
	{
		// Get identifier.
		while ((FChar::IsAlnum(*Buffer) || (*Buffer > 255) || *Buffer == TCHAR('_') || *Buffer == TCHAR('-') || *Buffer == TCHAR('+') || (DottedNames && (*Buffer == TCHAR('.') || *Buffer == TCHAR('/') || *Buffer == SUBOBJECT_DELIMITER_CHAR))))
		{
			String += *Buffer++;
		}
	}
	else
	{
		// Get just one.
		String += *Buffer;
	}
	return Buffer;
}


/*-----------------------------------------------------------------------------
	UProperty implementation.
-----------------------------------------------------------------------------*/

//
// Constructors.
//
UProperty::UProperty(const FObjectInitializer& ObjectInitializer)
: UField(ObjectInitializer)	
, ArrayDim(1)
{
}

UProperty::UProperty(ECppProperty, int32 InOffset, uint64 InFlags)
	: UField(FObjectInitializer::Get())
	, ArrayDim(1)
	, PropertyFlags(InFlags)
	, Offset_Internal(InOffset)
{
	Init();
}

UProperty::UProperty(const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
: UField(ObjectInitializer)	
, ArrayDim(1)
, PropertyFlags(InFlags)
, Offset_Internal(InOffset)
{
	Init();
}

void UProperty::Init()
{
#if !WITH_EDITORONLY_DATA
	//@todo.COOKER/PACKAGER: Until we have a cooker/packager step, this can fire when WITH_EDITORONLY_DATA is not defined!
	//	checkSlow(!HasAnyPropertyFlags(CPF_EditorOnly));
#endif // WITH_EDITORONLY_DATA
	checkSlow(GetOuterUField()->HasAllFlags(RF_Transient));
	checkSlow(HasAllFlags(RF_Transient));

	GetOuterUField()->AddCppProperty(this);
}

//
// Serializer.
//
void UProperty::Serialize( FArchive& Ar )
{
	// Make sure that we aren't saving a property to a package that shouldn't be serialised.
#if WITH_EDITORONLY_DATA
	check( !Ar.IsFilterEditorOnly() || !IsEditorOnlyProperty() );
#endif // WITH_EDITORONLY_DATA

	Super::Serialize( Ar );

	uint64 SaveFlags(PropertyFlags & ~CPF_ComputedFlags);
	// Archive the basic info.
	Ar << ArrayDim << SaveFlags;
	if (Ar.IsLoading())
	{
		PropertyFlags = (SaveFlags & ~CPF_ComputedFlags) | (PropertyFlags & CPF_ComputedFlags);
	}
	
	if (FPlatformProperties::HasEditorOnlyData() == false)
	{
		// Make sure that we aren't saving a property to a package that shouldn't be serialised.
		check( !IsEditorOnlyProperty() );
	}

	Ar << RepNotifyFunc;

	if( Ar.IsLoading() )
	{
		Offset_Internal = 0;
		DestructorLinkNext = NULL;
	}
}

void UProperty::CopySingleValueToScriptVM( void* Dest, void const* Src ) const
{
	CopySingleValue(Dest, Src);
}

void UProperty::CopyCompleteValueToScriptVM( void* Dest, void const* Src ) const
{
	CopyCompleteValue(Dest, Src);
}

void UProperty::CopySingleValueFromScriptVM( void* Dest, void const* Src ) const
{
	CopySingleValue(Dest, Src);
}

void UProperty::CopyCompleteValueFromScriptVM( void* Dest, void const* Src ) const
{
	CopyCompleteValue(Dest, Src);
}

void UProperty::ClearValueInternal( void* Data ) const
{
	checkf(0, TEXT("%s failed to handle ClearValueInternal, but it was not CPF_NoDestructor | CPF_ZeroConstructor"), *GetFullName());
}
void UProperty::DestroyValueInternal( void* Dest ) const
{
	checkf(0, TEXT("%s failed to handle DestroyValueInternal, but it was not CPF_NoDestructor"), *GetFullName());
}
void UProperty::InitializeValueInternal( void* Dest ) const
{
	checkf(0, TEXT("%s failed to handle InitializeValueInternal, but it was not CPF_ZeroConstructor"), *GetFullName());
}

/**
 * Verify that modifying this property's value via ImportText is allowed.
 * 
 * @param	PortFlags	the flags specified in the call to ImportText
 *
 * @return	true if ImportText should be allowed
 */
bool UProperty::ValidateImportFlags( uint32 PortFlags, FOutputDevice* ErrorHandler ) const
{
	// PPF_RestrictImportTypes is set when importing defaultproperties; it indicates that
	// we should not allow config/localized properties to be imported here
	if ((PortFlags & PPF_RestrictImportTypes) && (PropertyFlags & CPF_Config))
	{
		FString ErrorMsg = FString::Printf(TEXT("Import failed for '%s': property is config (Check to see if the property is listed in the DefaultProperties.  It should only be listed in the specific .ini file)"), *GetName());

		if (ErrorHandler)
		{
			ErrorHandler->Logf(TEXT("%s"), *ErrorMsg);
		}
		else
		{
			UE_LOG(LogProperty, Warning, TEXT("%s"), *ErrorMsg);
		}

		return false;
	}

	return true;
}

FString UProperty::GetNameCPP() const
{
	return HasAnyPropertyFlags(CPF_Deprecated) ? GetName() + TEXT("_DEPRECATED") : GetName();
}

FString UProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = TEXT("U");
	ExtendedTypeText += GetClass()->GetName();
	return TEXT("PROPERTY");
}

bool UProperty::PassCPPArgsByRef() const
{
	return false;
}


void UProperty::ExportCppDeclaration(FOutputDevice& Out, EExportedDeclaration::Type DeclarationType, const TCHAR* ArrayDimOverride, uint32 AdditionalExportCPPFlags
	, bool bSkipParameterName, const FString* ActualCppType, const FString* ActualExtendedType, const FString* ActualParameterName) const
{
	const bool bIsParameter = (DeclarationType == EExportedDeclaration::Parameter) || (DeclarationType == EExportedDeclaration::MacroParameter);
	const bool bIsInterfaceProp = dynamic_cast<const UInterfaceProperty*>(this) != nullptr;

	// export the property type text (e.g. FString; int32; TArray, etc.)
	FString ExtendedTypeText;
	const uint32 ExportCPPFlags = AdditionalExportCPPFlags | (bIsParameter ? CPPF_ArgumentOrReturnValue : 0);
	FString TypeText;
	if (ActualCppType)
	{
		TypeText = *ActualCppType;
	}
	else
	{
		TypeText = GetCPPType(&ExtendedTypeText, ExportCPPFlags);
	}

	if (ActualExtendedType)
	{
		ExtendedTypeText = *ActualExtendedType;
	}

	const bool bCanHaveRef = 0 == (AdditionalExportCPPFlags & CPPF_NoRef);
	const bool bCanHaveConst = 0 == (AdditionalExportCPPFlags & CPPF_NoConst);
	if (!dynamic_cast<const UBoolProperty*>(this) && bCanHaveConst) // can't have const bitfields because then we cannot determine their offset and mask from the compiler
	{
		const UObjectProperty* ObjectProp = dynamic_cast<const UObjectProperty*>(this);

		// export 'const' for parameters
		const bool bIsConstParam   = bIsParameter && (HasAnyPropertyFlags(CPF_ConstParm) || (bIsInterfaceProp && !HasAllPropertyFlags(CPF_OutParm)));
		const bool bIsOnConstClass = ObjectProp && ObjectProp->PropertyClass && ObjectProp->PropertyClass->HasAnyClassFlags(CLASS_Const);
		const bool bShouldHaveRef = bCanHaveRef && HasAnyPropertyFlags(CPF_OutParm | CPF_ReferenceParm);

		const bool bConstAtTheBeginning = bIsOnConstClass || (bIsConstParam && !bShouldHaveRef);
		if (bConstAtTheBeginning)
		{
			TypeText = FString::Printf(TEXT("const %s"), *TypeText);
		}

		const UClass* const MyPotentialConstClass = (DeclarationType == EExportedDeclaration::Member) ? dynamic_cast<UClass*>(GetOuter()) : nullptr;
		const bool bFromConstClass = MyPotentialConstClass && MyPotentialConstClass->HasAnyClassFlags(CLASS_Const);
		const bool bConstAtTheEnd = bFromConstClass || (bIsConstParam && bShouldHaveRef);
		if (bConstAtTheEnd)
		{
			ExtendedTypeText += TEXT(" const");
		}
	}

	FString NameCpp;
	if (!bSkipParameterName)
	{
		ensure((0 == (AdditionalExportCPPFlags & CPPF_BlueprintCppBackend)) || ActualParameterName);
		NameCpp = ActualParameterName ? *ActualParameterName : GetNameCPP();
	}
	if (DeclarationType == EExportedDeclaration::MacroParameter)
	{
		NameCpp = FString(TEXT(", ")) + NameCpp;
	}

	TCHAR ArrayStr[MAX_SPRINTF]=TEXT("");
	const bool bExportStaticArray = 0 == (CPPF_NoStaticArray & AdditionalExportCPPFlags);
	if ((ArrayDim != 1) && bExportStaticArray)
	{
		if (ArrayDimOverride)
		{
			FCString::Sprintf( ArrayStr, TEXT("[%s]"), ArrayDimOverride );
		}
		else
		{
			FCString::Sprintf( ArrayStr, TEXT("[%i]"), ArrayDim );
		}
	}

	if(auto BoolProperty = dynamic_cast<const UBoolProperty*>(this) )
	{
		// if this is a member variable, export it as a bitfield
		if( ArrayDim==1 && DeclarationType == EExportedDeclaration::Member )
		{
			bool bCanUseBitfield = !BoolProperty->IsNativeBool();
			// export as a uint32 member....bad to hardcode, but this is a special case that won't be used anywhere else
			Out.Logf(TEXT("%s%s %s%s%s"), *TypeText, *ExtendedTypeText, *NameCpp, ArrayStr, bCanUseBitfield ? TEXT(":1") : TEXT(""));
		}

		//@todo we currently can't have out bools.. so this isn't really necessary, but eventually out bools may be supported, so leave here for now
		else if( bIsParameter && HasAnyPropertyFlags(CPF_OutParm) )
		{
			// export as a reference
			Out.Logf(TEXT("%s%s%s %s%s"), *TypeText, *ExtendedTypeText
				, bCanHaveRef ? TEXT("&") : TEXT("")
				, *NameCpp, ArrayStr);
		}

		else
		{
			Out.Logf(TEXT("%s%s %s%s"), *TypeText, *ExtendedTypeText, *NameCpp, ArrayStr);
		}
	}
	else 
	{
		if ( bIsParameter )
		{
			if ( ArrayDim > 1 )
			{
				// export as a pointer
				//Out.Logf( TEXT("%s%s* %s"), *TypeText, *ExtendedTypeText, *GetNameCPP() );
				// don't export as a pointer
				Out.Logf(TEXT("%s%s %s%s"), *TypeText, *ExtendedTypeText, *NameCpp, ArrayStr);
			}
			else
			{
				if ( PassCPPArgsByRef() )
				{
					// export as a reference (const ref if it isn't an out parameter)
					Out.Logf(TEXT("%s%s%s%s %s"),
						(bCanHaveConst && !HasAnyPropertyFlags(CPF_OutParm | CPF_ConstParm)) ? TEXT("const ") : TEXT(""),
						*TypeText, *ExtendedTypeText,
						bCanHaveRef ? TEXT("&") : TEXT(""),
						*NameCpp);
				}
				else
				{
					// export as a pointer if this is an optional out parm, reference if it's just an out parm, standard otherwise...
					TCHAR ModifierString[2]={0,0};
					if (bCanHaveRef && (HasAnyPropertyFlags(CPF_OutParm | CPF_ReferenceParm) || bIsInterfaceProp))
					{
						ModifierString[0] = TEXT('&');
					}
					Out.Logf(TEXT("%s%s%s %s%s"), *TypeText, *ExtendedTypeText, ModifierString, *NameCpp, ArrayStr);
				}
			}
		}
		else
		{
			Out.Logf(TEXT("%s%s %s%s"), *TypeText, *ExtendedTypeText, *NameCpp, ArrayStr);
		}
	}
}

bool UProperty::ExportText_Direct
	(
	FString&	ValueStr,
	const void*	Data,
	const void*	Delta,
	UObject*	Parent,
	int32			PortFlags,
	UObject*	ExportRootScope
	) const
{
	if( Data==Delta || !Identical(Data,Delta,PortFlags) )
	{
		ExportTextItem
			(
			ValueStr,
			(uint8*)Data,
			(uint8*)Delta,
			Parent,
			PortFlags,
			ExportRootScope
			);
		return true;
	}

	return false;
}

bool UProperty::ShouldSerializeValue( FArchive& Ar ) const
{
	if (Ar.ShouldSkipProperty(this))
	{
		return false;
	}

	if (!(PropertyFlags & CPF_SaveGame) && Ar.IsSaveGame())
	{
		return false;
	}

	const uint64 SkipFlags = CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient | CPF_NonTransactional | CPF_Deprecated | CPF_DevelopmentAssets | CPF_SkipSerialization;
	if (!(PropertyFlags & SkipFlags))
	{
		return true;
	}

	bool Skip =
			((PropertyFlags & CPF_Transient) && Ar.IsPersistent() && !Ar.IsSerializingDefaults())
		||	((PropertyFlags & CPF_DuplicateTransient) && (Ar.GetPortFlags() & PPF_Duplicate))
		||	((PropertyFlags & CPF_NonPIEDuplicateTransient) && !(Ar.GetPortFlags() & PPF_DuplicateForPIE) && (Ar.GetPortFlags() & PPF_Duplicate))
		||	((PropertyFlags & CPF_NonTransactional) && Ar.IsTransacting())
		||	((PropertyFlags & CPF_Deprecated) && !Ar.HasAllPortFlags(PPF_UseDeprecatedProperties) && (Ar.IsSaving() || Ar.IsTransacting() || Ar.WantBinaryPropertySerialization()))
		||  ((PropertyFlags & CPF_SkipSerialization) && (Ar.WantBinaryPropertySerialization() || !Ar.HasAllPortFlags(PPF_ForceTaggedSerialization)))
		||  (IsEditorOnlyProperty() && Ar.IsFilterEditorOnly());

	return !Skip;
}


//
// Net serialization.
//
bool UProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	SerializeItem( Ar, Data, NULL );
	return 1;
}

//
// Return whether the property should be exported.
//
bool UProperty::ShouldPort( uint32 PortFlags/*=0*/ ) const
{
	// if no size, don't export
	if (GetSize() <= 0)
		return false;

	// if we're parsing default properties or the user indicated that transient properties should be included
	if (HasAnyPropertyFlags(CPF_Transient) && !(PortFlags & (PPF_ParsingDefaultProperties | PPF_IncludeTransient)))
		return false;

	// if we're copying, treat DuplicateTransient as transient
	if ((PortFlags & PPF_Copy) && HasAnyPropertyFlags(CPF_DuplicateTransient | CPF_TextExportTransient) && !(PortFlags & (PPF_ParsingDefaultProperties | PPF_IncludeTransient)))
		return false;

	// if we're not copying for PIE and NonPIETransient is set, don't export
	if (!(PortFlags & PPF_DuplicateForPIE) && HasAnyPropertyFlags(CPF_NonPIEDuplicateTransient))
		return false;

	// if we're only supposed to export components and this isn't a component property, don't export
	if ((PortFlags & PPF_SubobjectsOnly) && !ContainsInstancedObjectProperty())
		return false;

	// hide non-Edit properties when we're exporting for the property window
	if ((PortFlags & PPF_PropertyWindow) && !(PropertyFlags & CPF_Edit))
		return false;

	return true;
}

//
// Return type id for encoding properties in .u files.
//
FName UProperty::GetID() const
{
	return GetClass()->GetFName();
}

void UProperty::InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph )
{
}

int32 UProperty::GetMinAlignment() const
{
	return 1;
}


//
// Link property loaded from file.
//
void UProperty::LinkInternal(FArchive& Ar)
{
	check(0); // Link shouldn't call super...and we should never link an abstract property, like this base class
}

bool UProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty)
{
	return false;
}


int32 UProperty::SetupOffset()
{
	Offset_Internal = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
	return Offset_Internal + GetSize();
}

void UProperty::SetOffset_Internal(int32 NewOffset)
{
	Offset_Internal = NewOffset;
}

bool UProperty::SameType(const UProperty* Other) const
{
	return Other && (this->GetClass() == Other->GetClass());
}

/**
 * Attempts to read an array index (xxx) sequence.  Handles const/enum replacements, etc.
 * @param	ObjectStruct	the scope of the object/struct containing the property we're currently importing
 * @param	Str				[out] pointer to the the buffer containing the property value to import
 * @param	Warn			the output device to send errors/warnings to
 * @return	the array index for this defaultproperties line.  INDEX_NONE if this line doesn't contains an array specifier, or 0 if there was an error parsing the specifier.
 */
static const int32 ReadArrayIndex(UStruct* ObjectStruct, const TCHAR*& Str, FOutputDevice* Warn)
{
	const TCHAR* Start = Str;
	int32 Index = INDEX_NONE;
	SkipWhitespace(Str);

	if (*Str == '(' || *Str == '[')
	{
		Str++;
		FString IndexText(TEXT(""));
		while ( *Str && *Str != ')' && *Str != ']' )
		{
			if ( *Str == TCHAR('=') )
			{
				// we've encountered an equals sign before the closing bracket
				Warn->Logf(ELogVerbosity::Warning, TEXT("Missing ')' in default properties subscript: %s"), Start);
				return 0;
			}

			IndexText += *Str++;
		}

		if ( *Str++ )
		{
			if (IndexText.Len() > 0 )
			{
				if (FChar::IsAlpha(IndexText[0]))
				{
					FName IndexTokenName = FName(*IndexText, FNAME_Find);
					if (IndexTokenName != NAME_None)
					{
						// Search for the enum in question.
						Index = UEnum::LookupEnumName(IndexTokenName);
						if (Index == INDEX_NONE)
						{
							Index = 0;
							Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid subscript in default properties: %s"), Start);
						}
					}
					else
					{
						Index = 0;

						// unknown or invalid identifier specified for array subscript
						Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid subscript in default properties: %s"), Start);
					}
				}
				else if (FChar::IsDigit(IndexText[0]))
				{
					Index = FCString::Atoi(*IndexText);
				}
				else
				{
					// unknown or invalid identifier specified for array subscript
					Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid subscript in default properties: %s"), Start);
				}
			}
			else
			{
				Index = 0;

				// nothing was specified between the opening and closing parenthesis
				Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid subscript in default properties: %s"), Start);
			}
		}
		else
		{
			Index = 0;
			Warn->Logf(ELogVerbosity::Warning, TEXT("Missing ')' in default properties subscript: %s"), Start );
		}
	}
	return Index;
}

/** 
 * Do not attempt to import this property if there is no value for it - i.e. (Prop1=,Prop2=)
 * This normally only happens for empty strings or empty dynamic arrays, and the alternative
 * is for strings and dynamic arrays to always export blank delimiters, such as Array=() or String="", 
 * but this tends to cause problems with inherited property values being overwritten, especially in the localization 
 * import/export code

 * The safest way is to interpret blank delimiters as an indication that the current value should be overwritten with an empty
 * value, while the lack of any value or delimiter as an indication to not import this property, thereby preventing any current
 * values from being overwritten if this is not the intent.

 * Thus, arrays and strings will only export empty delimiters when overriding an inherited property's value with an 
 * empty value.
 */
static bool IsPropertyValueSpecified( const TCHAR* Buffer )
{
	return Buffer && *Buffer && *Buffer != TCHAR(',') && *Buffer != TCHAR(')');
}

const TCHAR* UProperty::ImportSingleProperty( const TCHAR* Str, void* DestData, UStruct* ObjectStruct, UObject* SubobjectOuter, int32 PortFlags,
											FOutputDevice* Warn, TArray<FDefinedProperty>& DefinedProperties )
{
	check(ObjectStruct);

	while (*Str == ' ' || *Str == 9)
	{
		Str++;
	}
			
	const TCHAR* Start = Str;
			
	while (*Str && *Str != '=' && *Str != '(' && *Str != '[' && *Str != '.')
	{
		Str++;
	}

	if (*Str)
	{
		TCHAR* Token = new TCHAR[Str - Start + 1];
		FCString::Strncpy(Token, Start, Str - Start + 1);

		// strip trailing whitespace on token
		int32 l = FCString::Strlen(Token);
		while (l > 0 && (Token[l - 1] == ' ' || Token[l - 1] == 9))
		{
			Token[l - 1] = 0;
			--l;
		}

		const FName PropertyName(Token);
		UProperty* Property = FindField<UProperty>(ObjectStruct, PropertyName);

		if (Property == nullptr)
		{
			// Check for redirects
			FName NewPropertyName = FindRedirectedPropertyName(ObjectStruct, PropertyName);

			if (NewPropertyName != NAME_None)
			{
				Property = FindField<UProperty>(ObjectStruct, NewPropertyName);
			}
#if WITH_EDITOR
			if (!Property)
			{
				Property = ObjectStruct->CustomFindProperty(PropertyName);
			}
#endif	// WITH_EDITOR
		}		

		delete[] Token;
		Token = NULL;

		if (Property == NULL)
		{
			UE_SUPPRESS(LogExec, Verbose, Warn->Logf(TEXT("Unknown property in %s: %s "), *ObjectStruct->GetName(), Start));
			return Str;
		}

		if (!Property->ShouldPort(PortFlags))
		{
			UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Cannot perform text import on property '%s' here: %s"), *Property->GetName(), Start));
			return Str;
		}

		// Parse an array operation, if present.
		enum EArrayOp
		{
			ADO_None,
			ADO_Add,
			ADO_Remove,
			ADO_RemoveIndex,
			ADO_Empty,
		};

		EArrayOp ArrayOp = ADO_None;
		if(*Str == '.')
		{
			Str++;
			if(FParse::Command(&Str,TEXT("Empty")))
			{
				ArrayOp = ADO_Empty;
			}
			else if(FParse::Command(&Str,TEXT("Add")))
			{
				ArrayOp = ADO_Add;
			}
			else if(FParse::Command(&Str,TEXT("Remove")))
			{
				ArrayOp = ADO_Remove;
			}
			else if (FParse::Command(&Str,TEXT("RemoveIndex")))
			{
				ArrayOp = ADO_RemoveIndex;
			}
		}

		UArrayProperty* const ArrayProperty = ExactCast<UArrayProperty>(Property);
		UMulticastDelegateProperty* const MulticastDelegateProperty = ExactCast<UMulticastDelegateProperty>(Property);
		if( MulticastDelegateProperty != NULL && ArrayOp != ADO_None )
		{
			// Allow Add(), Remove() and Empty() on multi-cast delegates
			if( ArrayOp == ADO_Add || ArrayOp == ADO_Remove || ArrayOp == ADO_Empty )
			{
				SkipWhitespace(Str);
				if(*Str++ != '(')
				{
					UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing '(' in default properties multi-cast delegate operation: %s"), Start));
					return Str;
				}
				SkipWhitespace(Str);

				if( ArrayOp == ADO_Empty )
				{
					// Clear out the delegate
					MulticastDelegateProperty->GetPropertyValuePtr_InContainer(DestData)->Clear();
				}
				else
				{
					FStringOutputDevice ImportError;

					const TCHAR* Result = NULL;
					if( ArrayOp == ADO_Add )
					{
						// Add a function to a multi-cast delegate
						Result = MulticastDelegateProperty->ImportText_Add(Str, Property->ContainerPtrToValuePtr<void>(DestData), PortFlags, SubobjectOuter, &ImportError);
					}
					else if( ArrayOp == ADO_Remove )
					{
						// Remove a function from a multi-cast delegate
						Result = MulticastDelegateProperty->ImportText_Remove(Str, Property->ContainerPtrToValuePtr<void>(DestData), PortFlags, SubobjectOuter, &ImportError);
					}
				
					// Spit any error we had while importing property
					if (ImportError.Len() > 0)
					{
						TArray<FString> ImportErrors;
						ImportError.ParseIntoArray(ImportErrors, LINE_TERMINATOR, true);

						for ( int32 ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
						{
							Warn->Logf(ELogVerbosity::Warning, TEXT("%s"), *ImportErrors[ErrorIndex]);
						}
					}
					else if (Result == NULL || Result == Str)
					{
						Warn->Logf(ELogVerbosity::Warning, TEXT("Unable to parse parameter value '%s' in defaultproperties multi-cast delegate operation: %s"), Str, Start);
					}
					// in the failure case, don't return NULL so the caller can potentially skip less and get values further in the string
					if (Result != NULL)
					{
						Str = Result;
					}

				}
			}
			else
			{
				UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Unsupported operation on multi-cast delegate variable: %s"), Start));
				return Str;
			}
			SkipWhitespace(Str);
			if (*Str != ')')
			{
				UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing ')' in default properties multi-cast delegate operation: %s"), Start));
				return Str;
			}
			Str++;
		}
		else if (ArrayOp != ADO_None)
		{
			if (ArrayProperty == NULL)
			{
				UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Array operation performed on non-array variable: %s"), Start));
				return Str;
			}

			FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, DestData);
			if (ArrayOp == ADO_Empty)
			{
				ArrayHelper.EmptyValues();
				SkipWhitespace(Str);
				if (*Str++ != '(')
				{
					UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing '(' in default properties array operation: %s"), Start));
					return Str;
				}
			}
			else if (ArrayOp == ADO_Add || ArrayOp == ADO_Remove)
			{
				SkipWhitespace(Str);
				if(*Str++ != '(')
				{
					UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing '(' in default properties array operation: %s"), Start));
					return Str;
				}
				SkipWhitespace(Str);

				if(ArrayOp == ADO_Add)
				{
					int32	Index = ArrayHelper.AddValue();

					const TCHAR* Result = ArrayProperty->Inner->ImportText(Str, ArrayHelper.GetRawPtr(Index), PortFlags, SubobjectOuter, Warn);
					if ( Result == NULL || Result == Str )
					{
						Warn->Logf(ELogVerbosity::Warning, TEXT("Unable to parse parameter value '%s' in defaultproperties array operation: %s"), Str, Start);
						return Str;
					}
					else
					{
						Str = Result;
					}
				}
				else if(ArrayOp == ADO_Remove)
				{
					int32 Size = ArrayProperty->Inner->ElementSize;

					uint8* Temp = (uint8*)FMemory_Alloca(Size);
					ArrayProperty->Inner->InitializeValue(Temp);
							
					// export the value specified to a temporary buffer
					const TCHAR* Result = ArrayProperty->Inner->ImportText(Str, Temp, PortFlags, SubobjectOuter, Warn);
					if ( Result == NULL || Result == Str )
					{
						Warn->Logf(ELogVerbosity::Error, TEXT("Unable to parse parameter value '%s' in defaultproperties array operation: %s"), Str, Start);
						ArrayProperty->Inner->DestroyValue(Temp);
						return Str;
					}
					else
					{
						// find the array member corresponding to this value
						bool bFound = false;
						for(uint32 Index = 0;Index < (uint32)ArrayHelper.Num();Index++)
						{
							const void* ElementDestData = ArrayHelper.GetRawPtr(Index);
							if(ArrayProperty->Inner->Identical(Temp,ElementDestData))
							{
								ArrayHelper.RemoveValues(Index--);
								bFound = true;
							}
						}
						if (!bFound)
						{
							Warn->Logf(ELogVerbosity::Warning, TEXT("%s.Remove(): Value not found in array"), *ArrayProperty->GetName());
						}
						ArrayProperty->Inner->DestroyValue(Temp);
						Str = Result;
					}
				}
			}
			else if (ArrayOp == ADO_RemoveIndex)
			{
				SkipWhitespace(Str);
				if(*Str++ != '(')
				{
					UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing '(' in default properties array operation:: %s"), Start ));
					return Str;
				}
				SkipWhitespace(Str);

				FString strIdx;
				while (*Str != ')')
				{		
					if (*Str == 0)
					{
						UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing ')' in default properties array operation: %s"), Start));
						return Str;
					}
					strIdx += *Str;
					Str++;
				}
				int32 removeIdx = FCString::Atoi(*strIdx);

				ArrayHelper.RemoveValues(removeIdx);
			}
			SkipWhitespace(Str);
			if (*Str != ')')
			{
				UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing ')' in default properties array operation: %s"), Start));
				return Str;
			}
			Str++;
		}
		else
		{
			// try to read an array index
			int32 Index = ReadArrayIndex(ObjectStruct, Str, Warn);

			// check for out of bounds on static arrays
			if (ArrayProperty == NULL && Index >= Property->ArrayDim)
			{
				Warn->Logf(ELogVerbosity::Warning, TEXT("Out of bound array default property (%i/%i): %s"), Index, Property->ArrayDim, Start);
				return Str;
			}

			// check to see if this property has already imported data
			FDefinedProperty D;
			D.Property = Property;
			D.Index = Index;
			if (DefinedProperties.Find(D) != INDEX_NONE)
			{
				Warn->Logf(ELogVerbosity::Warning, TEXT("redundant data: %s"), Start);
				return Str;
			}
			DefinedProperties.Add(D);

			// strip whitespace before =
			SkipWhitespace(Str);
			if (*Str++ != '=')
			{
				Warn->Logf(ELogVerbosity::Warning, TEXT("Missing '=' in default properties assignment: %s"), Start );
				return Str;
			}
			// strip whitespace after =
			SkipWhitespace(Str);

			if (!IsPropertyValueSpecified(Str) && ArrayProperty == nullptr)
			{
				// if we're not importing default properties for classes (i.e. we're pasting something in the editor or something)
				// and there is no property value for this element, skip it, as that means that the value of this element matches
				// the intrinsic null value of the property type and we want to skip importing it
				return Str;
			}

			// disallow importing of an object's name from here
			// not done above with ShouldPort() check because this is intentionally exported so we don't want it to cause errors on import
			if (Property->GetFName() != NAME_Name || Property->GetOuter()->GetFName() != NAME_Object)
			{
				if (Index > -1 && ArrayProperty != NULL) //set single dynamic array element
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, DestData);

					ArrayHelper.ExpandForIndex(Index);

					FStringOutputDevice ImportError;
					const TCHAR* Result = ArrayProperty->Inner->ImportText(Str, ArrayHelper.GetRawPtr(Index), PortFlags, SubobjectOuter, &ImportError);
					// Spit any error we had while importing property
					if (ImportError.Len() > 0)
					{
						TArray<FString> ImportErrors;
						ImportError.ParseIntoArray(ImportErrors,LINE_TERMINATOR,true);

						for ( int32 ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
						{
							Warn->Logf(ELogVerbosity::Warning, TEXT("%s"), *ImportErrors[ErrorIndex]);
						}
					}
					else if (Result == Str)
					{
						Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid property value in defaults: %s"), Start);
					}
					// in the failure case, don't return NULL so the caller can potentially skip less and get values further in the string
					if (Result != NULL)
					{
						Str = Result;
					}
				}
				else
				{
					if (Index == INDEX_NONE)
					{
						Index = 0;
					}

					FStringOutputDevice ImportError;

					const TCHAR* Result = Property->ImportText(Str, Property->ContainerPtrToValuePtr<void>(DestData, Index), PortFlags, SubobjectOuter, &ImportError);
					
					// Spit any error we had while importing property
					if (ImportError.Len() > 0)
					{
						TArray<FString> ImportErrors;
						ImportError.ParseIntoArray(ImportErrors, LINE_TERMINATOR, true);

						for ( int32 ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
						{
							Warn->Logf(ELogVerbosity::Warning, TEXT("%s"), *ImportErrors[ErrorIndex]);
						}
					}
					else if ((Result == NULL && ArrayProperty == nullptr) || Result == Str)
					{
						UE_SUPPRESS(LogExec, Verbose, Warn->Logf(TEXT("Unknown property in %s: %s "), *ObjectStruct->GetName(), Start));
					}
					// in the failure case, don't return NULL so the caller can potentially skip less and get values further in the string
					if (Result != NULL)
					{
						Str = Result;
					}
				}
			}
		}
	}
	return Str;
}

FName UProperty::FindRedirectedPropertyName(UStruct* ObjectStruct, FName OldName)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UProperty::FindRedirectedPropertyName"), STAT_LinkerLoad_FindRedirectedPropertyName, STATGROUP_LoadTimeVerbose);

	// ObjectStruct may be a nested struct, so extract path
	UPackage* StructPackage = ObjectStruct->GetOutermost();
	FString OuterPath = ObjectStruct->GetPathName(StructPackage);

	FCoreRedirectObjectName OldRedirectName(OldName, FName(*OuterPath), StructPackage->GetFName());
	FCoreRedirectObjectName NewRedirectName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Property, OldRedirectName);

	if (NewRedirectName != OldRedirectName)
	{
		return NewRedirectName.ObjectName;
	}

	return NAME_None;
}

/**
 * Returns the hash value for an element of this property.
 */
uint32 UProperty::GetValueTypeHash(const void* Src) const
{
	check(PropertyFlags & CPF_HasGetValueTypeHash); // make sure the type is hashable
	check(Src);
	return GetValueTypeHashInternal(Src);
}

void UProperty::CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const
{
	check(0); // if you are not memcpyable, then you need to deal with the virtual call
}

uint32 UProperty::GetValueTypeHashInternal(const void* Src) const
{
	check(false); // you need to deal with the virtual call
	return 0;
}


IMPLEMENT_CORE_INTRINSIC_CLASS(UProperty, UField,
	{
	}
);

void UFloatProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	Super::ExportTextItem(ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, ExportRootScope);

	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += TEXT("f");
	}
}


UProperty* UStruct::FindPropertyByName(FName InName) const
{
	for (UProperty* Property = PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
	{
		if (Property->GetFName() == InName)
		{
			return Property;
		}
	}

	return NULL;
}
