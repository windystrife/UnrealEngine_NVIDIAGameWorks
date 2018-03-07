// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ErrorException.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/TextProperty.h"
#include "UObject/EnumProperty.h"
#include "UnrealHeaderToolGlobals.h"
#include "ClassMaps.h"
#include "Templates/UniqueObj.h"
#include "UniquePtr.h"

class UEnum;
class UScriptStruct;
class UProperty;
class FUnrealSourceFile;
class UObject;
class UField;
class UMetaData;
class FHeaderParser;

extern class FCompilerMetadataManager GScriptHelper;

/*-----------------------------------------------------------------------------
	FPropertyBase.
-----------------------------------------------------------------------------*/

enum EFunctionExportFlags
{
	FUNCEXPORT_Final			=0x00000001,	// function declaration included "final" keyword.  Used to differentiate between functions that have FUNC_Final only because they're private
	//							=0x00000002,
	//							=0x00000004,
	FUNCEXPORT_RequiredAPI		=0x00000008,	// Function should be exported as a public API function
	FUNCEXPORT_Inline			=0x00000010,	// export as an inline static C++ function
	FUNCEXPORT_CppStatic		=0x00000020,	// Export as a real C++ static function, causing thunks to call via ClassName::FuncName instead of this->FuncName
	FUNCEXPORT_CustomThunk		=0x00000040,	// Export no thunk function; the user will manually define a custom one
	//							=0x00000080,
	//							=0x00000100,
};

enum EPropertyHeaderExportFlags
{
	PROPEXPORT_Public		=0x00000001,	// property should be exported as public
	PROPEXPORT_Private		=0x00000002,	// property should be exported as private
	PROPEXPORT_Protected	=0x00000004,	// property should be exported as protected
};

struct EPointerType
{
	enum Type
	{
		None,
		Native
	};
};

struct EArrayType
{
	enum Type
	{
		None,
		Static,
		Dynamic,
		Set
	};
};

struct ERefQualifier
{
	enum Type
	{
		None,
		ConstRef,
		NonConstRef
	};
};

enum class EIntType
{
	None,
	Sized,  // e.g. int32, int16
	Unsized // e.g. int, unsigned int
};

#ifndef CASE_TEXT
#define CASE_TEXT(txt) case txt: return TEXT(#txt)
#endif

/**
 * Basic information describing a type.
 */
class FPropertyBase : public TSharedFromThis<FPropertyBase>
{
public:
	// Variables.
	EPropertyType       Type;
	EArrayType::Type    ArrayType;
	uint64              PropertyFlags;
	uint64              ImpliedPropertyFlags;
	ERefQualifier::Type RefQualifier; // This is needed because of legacy stuff - FString mangles the flags for reasons that have become lost in time but we need this info for testing for invalid replicated function signatures.

	TSharedPtr<FPropertyBase> MapKeyProp;

	/**
	 * A mask of EPropertyHeaderExportFlags which are used for modifying how this property is exported to the native class header
	 */
	uint32 PropertyExportFlags;
	union
	{
		class UEnum* Enum;
		class UClass* PropertyClass;
		class UScriptStruct* Struct;
		class UFunction* Function;
#if PLATFORM_64BITS
		int64 StringSize;
#else
		int32 StringSize;
#endif
	};

	UClass* MetaClass;
	FName   DelegateName;
	UClass*	DelegateSignatureOwnerClass;
	FName   RepNotifyName;

	/** Raw string (not type-checked) used for specifying special text when exporting a property to the *Classes.h file */
	FString	ExportInfo;

	/** Map of key value pairs that will be added to the package's UMetaData for this property */
	TMap<FName, FString> MetaData;

	EPointerType::Type PointerType;
	EIntType IntType;

public:
	/** @name Constructors */
	//@{
	explicit FPropertyBase(EPropertyType InType)
	: Type                (InType)
	, ArrayType           (EArrayType::None)
	, PropertyFlags       (0)
	, ImpliedPropertyFlags(0)
	, RefQualifier        (ERefQualifier::None)
	, PropertyExportFlags (PROPEXPORT_Public)
	, StringSize          (0)
	, MetaClass           (nullptr)
	, DelegateName        (NAME_None)
	, DelegateSignatureOwnerClass(nullptr)
	, RepNotifyName       (NAME_None)
	, PointerType         (EPointerType::None)
	, IntType             (GetSizedIntTypeFromPropertyType(InType))
	{
	}

	explicit FPropertyBase(EPropertyType InType, EIntType InIntType)
	: Type                (InType)
	, ArrayType           (EArrayType::None)
	, PropertyFlags       (0)
	, ImpliedPropertyFlags(0)
	, RefQualifier        (ERefQualifier::None)
	, PropertyExportFlags (PROPEXPORT_Public)
	, StringSize          (0)
	, MetaClass           (nullptr)
	, DelegateName        (NAME_None)
	, DelegateSignatureOwnerClass(nullptr)
	, RepNotifyName       (NAME_None)
	, PointerType         (EPointerType::None)
	, IntType             (InIntType)
	{
	}

	explicit FPropertyBase(UEnum* InEnum, EPropertyType InType)
	: Type                (InType)
	, ArrayType           (EArrayType::None)
	, PropertyFlags       (0)
	, ImpliedPropertyFlags(0)
	, RefQualifier        (ERefQualifier::None)
	, PropertyExportFlags (PROPEXPORT_Public)
	, Enum                (InEnum)
	, MetaClass           (nullptr)
	, DelegateName        (NAME_None)
	, DelegateSignatureOwnerClass(nullptr)
	, RepNotifyName       (NAME_None)
	, PointerType         (EPointerType::None)
	, IntType             (GetSizedIntTypeFromPropertyType(InType))
	{
	}

	explicit FPropertyBase(UClass* InClass, bool bIsWeak = false, bool bWeakIsAuto = false, bool bIsLazy = false, bool bIsSoft = false)
	: Type                (CPT_ObjectReference)
	, ArrayType           (EArrayType::None)
	, PropertyFlags       (0)
	, ImpliedPropertyFlags(0)
	, RefQualifier        (ERefQualifier::None)
	, PropertyExportFlags (PROPEXPORT_Public)
	, PropertyClass       (InClass)
	, MetaClass           (nullptr)
	, DelegateName        (NAME_None)
	, DelegateSignatureOwnerClass(nullptr)
	, RepNotifyName       (NAME_None)
	, PointerType         (EPointerType::None)
	, IntType             (EIntType::None)
	{
		// if this is an interface class, we use the UInterfaceProperty class instead of UObjectProperty
		if ( InClass->HasAnyClassFlags(CLASS_Interface) )
		{
			Type = CPT_Interface;
		}
		if (bIsLazy)
		{
			Type = CPT_LazyObjectReference;
		}
		else if (bIsSoft)
		{
			Type = CPT_SoftObjectReference;
		}
		else if (bIsWeak)
		{
			Type = CPT_WeakObjectReference;
			if (bWeakIsAuto)
			{
				PropertyFlags |= CPF_AutoWeak;
			}
		}
	}

	explicit FPropertyBase(UScriptStruct* InStruct)
	: Type                (CPT_Struct)
	, ArrayType           (EArrayType::None)
	, PropertyFlags       (0)
	, ImpliedPropertyFlags(0)
	, RefQualifier        (ERefQualifier::None)
	, PropertyExportFlags (PROPEXPORT_Public)
	, Struct              (InStruct)
	, MetaClass           (NULL)
	, DelegateName        (NAME_None)
	, DelegateSignatureOwnerClass(nullptr)
	, RepNotifyName       (NAME_None)
	, PointerType         (EPointerType::None)
	, IntType             (EIntType::None)
	{
	}

	explicit FPropertyBase(UProperty* Property)
	: PropertyExportFlags(PROPEXPORT_Public)
	, DelegateName       (NAME_None)
	, DelegateSignatureOwnerClass(nullptr)
	, RepNotifyName      (NAME_None)
	, IntType            (EIntType::None)
	{
		checkSlow(Property);

		EArrayType::Type ArrType = EArrayType::None;
		uint64  PropagateFlags   = 0;
		UClass* ClassOfProperty  = Property->GetClass();

		if( ClassOfProperty==UArrayProperty::StaticClass() )
		{
			ArrType = EArrayType::Dynamic;

			// if we're an array, save up Parm flags so we can propagate them.
			// below the array will be assigned the inner property flags. This allows propagation of Parm flags (out, optional..)
			PropagateFlags = Property->PropertyFlags & CPF_ParmFlags;
			Property = CastChecked<UArrayProperty>(Property)->Inner;
			ClassOfProperty = Property->GetClass();
		}

		if( ClassOfProperty==UByteProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Byte);
			Enum = Cast<UByteProperty>(Property)->Enum;
			IntType = EIntType::Sized;
		}
		else if( ClassOfProperty==UEnumProperty::StaticClass() )
		{
			UEnumProperty* EnumProp = Cast<UEnumProperty>(Property);
			UNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();

			*this = FPropertyBase(
				  UnderlyingProp->IsA<UInt8Property>()   ? CPT_Int8
				: UnderlyingProp->IsA<UInt16Property>()  ? CPT_Int16
				: UnderlyingProp->IsA<UIntProperty>()    ? CPT_Int
				: UnderlyingProp->IsA<UInt64Property>()  ? CPT_Int64
				: UnderlyingProp->IsA<UByteProperty>()   ? CPT_Byte
				: UnderlyingProp->IsA<UUInt16Property>() ? CPT_UInt16
				: UnderlyingProp->IsA<UUInt32Property>() ? CPT_UInt32
				: UnderlyingProp->IsA<UUInt64Property>() ? CPT_UInt64
				:                                          CPT_None
			);
			check(this->Type != CPT_None);
			Enum = EnumProp->Enum;
			IntType = EIntType::Sized;
		}
		else if( ClassOfProperty==UInt8Property::StaticClass() )
		{
			*this = FPropertyBase(CPT_Int8);
			IntType = EIntType::Sized;
		}
		else if( ClassOfProperty==UInt16Property::StaticClass() )
		{
			*this = FPropertyBase(CPT_Int16);
			IntType = EIntType::Sized;
		}
		else if( ClassOfProperty==UIntProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Int);
			IntType = EIntType::Sized;
		}
		else if( ClassOfProperty==UInt64Property::StaticClass() )
		{
			*this = FPropertyBase(CPT_Int64);
			IntType = EIntType::Sized;
		}
		else if( ClassOfProperty==UUInt16Property::StaticClass() )
		{
			*this = FPropertyBase(CPT_UInt16);
			IntType = EIntType::Sized;
		}
		else if( ClassOfProperty==UUInt32Property::StaticClass() )
		{
			*this = FPropertyBase(CPT_UInt32);
			IntType = EIntType::Sized;
		}
		else if( ClassOfProperty==UUInt64Property::StaticClass() )
		{
			*this = FPropertyBase(CPT_UInt64);
			IntType = EIntType::Sized;
		}
		else if( ClassOfProperty==UBoolProperty::StaticClass() )
		{
			UBoolProperty* BoolProperty =  Cast<UBoolProperty>(Property);
			if (BoolProperty->IsNativeBool())
			{
				*this = FPropertyBase(CPT_Bool);
			}
			else
			{
				switch( BoolProperty->ElementSize )
				{
				case sizeof(uint8):
					*this = FPropertyBase(CPT_Bool8);
					break;
				case sizeof(uint16):
					*this = FPropertyBase(CPT_Bool16);
					break;
				case sizeof(uint32):
					*this = FPropertyBase(CPT_Bool32);
					break;
				case sizeof(uint64):
					*this = FPropertyBase(CPT_Bool64);
					break;
				}
			}
		}
		else if( ClassOfProperty==UFloatProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Float);
		}
		else if( ClassOfProperty==UDoubleProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Double);
		}
		else if( ClassOfProperty==UClassProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_ObjectReference);
			PropertyClass = Cast<UClassProperty>(Property)->PropertyClass;
			MetaClass = Cast<UClassProperty>(Property)->MetaClass;
		}
		else if( ClassOfProperty==UObjectProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_ObjectReference);
			PropertyClass = Cast<UObjectProperty>(Property)->PropertyClass;
		}
		else if( ClassOfProperty==UWeakObjectProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_WeakObjectReference);
			PropertyClass = Cast<UWeakObjectProperty>(Property)->PropertyClass;
		}
		else if( ClassOfProperty==ULazyObjectProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_LazyObjectReference);
			PropertyClass = Cast<ULazyObjectProperty>(Property)->PropertyClass;
		}
		else if( ClassOfProperty==USoftClassProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_SoftObjectReference);
			PropertyClass = Cast<USoftClassProperty>(Property)->PropertyClass;
			MetaClass = Cast<USoftClassProperty>(Property)->MetaClass;
		}
		else if( ClassOfProperty==USoftObjectProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_SoftObjectReference);
			PropertyClass = Cast<USoftObjectProperty>(Property)->PropertyClass;
		}
		else if( ClassOfProperty==UNameProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Name);
		}
		else if( ClassOfProperty==UStrProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_String);
		}
		else if( ClassOfProperty==UTextProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Text);
		}
		else if( ClassOfProperty==UStructProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Struct);
			Struct = Cast<UStructProperty>(Property)->Struct;
		}
		else if( ClassOfProperty==UDelegateProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Delegate);
			Function = Cast<UDelegateProperty>(Property)->SignatureFunction;
		}
		else if( ClassOfProperty==UMulticastDelegateProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_MulticastDelegate);
			// @todo delegate: Any other setup for calling multi-cast delegates from script needed?
			Function = Cast<UMulticastDelegateProperty>(Property)->SignatureFunction;
		}
		else if ( ClassOfProperty==UInterfaceProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Interface);
			PropertyClass = Cast<UInterfaceProperty>(Property)->InterfaceClass;
		}
		else
		{
			UE_LOG(LogCompile, Fatal, TEXT("Unknown property type '%s'"), *Property->GetFullName() );
		}
		ArrayType            = ArrType;
		PropertyFlags        = Property->PropertyFlags | PropagateFlags;
		ImpliedPropertyFlags = 0;
		RefQualifier         = ERefQualifier::None;
		PointerType          = EPointerType::None;
	}
	//@}

	/** @name Functions */
	//@{

	/**
	 * Returns whether this token represents an object reference
	 */
	bool IsObject() const
	{
		return Type == CPT_ObjectReference || Type == CPT_Interface || Type == CPT_WeakObjectReference || Type == CPT_LazyObjectReference || Type == CPT_SoftObjectReference;
	}

	bool IsContainer() const
	{
		return (ArrayType != EArrayType::None || MapKeyProp.IsValid());
	}

	/**
	 * Determines whether this token's type is compatible with another token's type.
	 *
	 * @param	Other							the token to check against this one.
	 *											Given the following example expressions, VarA is Other and VarB is 'this':
	 *												VarA = VarB;
	 *
	 *												function func(type VarB) {}
	 *												func(VarA);
	 *
	 *												static operator==(type VarB_1, type VarB_2) {}
	 *												if ( VarA_1 == VarA_2 ) {}
	 *
	 * @param	bDisallowGeneralization			controls whether it should be considered a match if this token's type is a generalization
	 *											of the other token's type (or vice versa, when dealing with structs
	 * @param	bIgnoreImplementedInterfaces	controls whether two types can be considered a match if one type is an interface implemented
	 *											by the other type.
	 */
	bool MatchesType( const FPropertyBase& Other, bool bDisallowGeneralization, bool bIgnoreImplementedInterfaces=false ) const
	{
		check(Type!=CPT_None || !bDisallowGeneralization);

		bool bIsObjectType = IsObject();
		bool bOtherIsObjectType = Other.IsObject();
		bool bIsObjectComparison = bIsObjectType && bOtherIsObjectType;
		bool bReverseClassChainCheck = true;

		// If converting to an l-value, we require an exact match with an l-value.
		if( (PropertyFlags&CPF_OutParm) != 0 )
		{
			// if the other type is not an l-value, disallow
			if ( (Other.PropertyFlags&CPF_OutParm) == 0 )
			{
				return false;
			}

			// if the other type is const and we are not const, disallow
			if ( (Other.PropertyFlags&CPF_ConstParm) != 0 && (PropertyFlags&CPF_ConstParm) == 0 )
			{
				return false;
			}

			if ( Type == CPT_Struct )
			{
				// Allow derived structs to be passed by reference, unless this is a dynamic array of structs
				bDisallowGeneralization = bDisallowGeneralization || ArrayType == EArrayType::Dynamic || Other.ArrayType == EArrayType::Dynamic;
			}

			// if Type == CPT_ObjectReference, out object function parm; allow derived classes to be passed in
			// if Type == CPT_Interface, out interface function parm; allow derived classes to be passed in
			else if ( (PropertyFlags & CPF_ConstParm) == 0 || !IsObject() )
			{
				// all other variable types must match exactly when passed as the value to an 'out' parameter
				bDisallowGeneralization = true;
			}
			
			// both types are objects, but one is an interface and one is an object reference
			else if ( bIsObjectComparison && Type != Other.Type )
			{
				return false;
			}
		}
		else if ((Type == CPT_ObjectReference || Type == CPT_WeakObjectReference || Type == CPT_LazyObjectReference || Type == CPT_SoftObjectReference) && Other.Type != CPT_Interface && (PropertyFlags & CPF_ReturnParm))
		{
			bReverseClassChainCheck = false;
		}

		// Check everything.
		if( Type==CPT_None && (Other.Type==CPT_None || !bDisallowGeneralization) )
		{
			// If Other has no type, accept anything.
			return true;
		}
		else if( Type != Other.Type && !bIsObjectComparison )
		{
			// Mismatched base types.
			return false;
		}
		else if( ArrayType != Other.ArrayType )
		{
			// Mismatched array types.
			return false;
		}
		else if( Type==CPT_Byte )
		{
			// Make sure enums match, or we're generalizing.
			return Enum==Other.Enum || (Enum==NULL && !bDisallowGeneralization);
		}
		else if( bIsObjectType )
		{
			check(PropertyClass!=NULL);

			// Make sure object types match, or we're generalizing.
			if( bDisallowGeneralization )
			{
				// Exact match required.
				return PropertyClass==Other.PropertyClass && MetaClass==Other.MetaClass;
			}
			else if( Other.PropertyClass==NULL )
			{
				// Cannonical 'None' matches all object classes.
				return true;
			}
			else
			{
				// Generalization is ok (typical example of this check would look like: VarA = VarB;, where this is VarB and Other is VarA)
				if ( Other.PropertyClass->IsChildOf(PropertyClass) )
				{
					if ( !bIgnoreImplementedInterfaces || ((Type == CPT_Interface) == (Other.Type == CPT_Interface)) )
					{
						if ( !PropertyClass->IsChildOf(UClass::StaticClass()) || MetaClass == NULL || Other.MetaClass->IsChildOf(MetaClass) ||
							(bReverseClassChainCheck && (Other.MetaClass == NULL || MetaClass->IsChildOf(Other.MetaClass))) )
						{
							return true;
						}
					}
				}
				// check the opposite class chain for object types
				else if (bReverseClassChainCheck && Type != CPT_Interface && bIsObjectComparison && PropertyClass != NULL && PropertyClass->IsChildOf(Other.PropertyClass))
				{
					if (!Other.PropertyClass->IsChildOf(UClass::StaticClass()) || MetaClass == NULL || Other.MetaClass == NULL || MetaClass->IsChildOf(Other.MetaClass) || Other.MetaClass->IsChildOf(MetaClass))
					{
						return true;
					}
				}

				if ( PropertyClass->HasAnyClassFlags(CLASS_Interface) && !bIgnoreImplementedInterfaces )
				{
					if ( Other.PropertyClass->ImplementsInterface(PropertyClass) )
					{
						return true;
					}
				}

				return false;
			}
		}
		else if( Type==CPT_Struct )
		{
			check(Struct!=NULL);
			check(Other.Struct!=NULL);

			if ( Struct == Other.Struct )
			{
				// struct types match exactly 
				return true;
			}

			// returning false here prevents structs related through inheritance from being used interchangeably, such as passing a derived struct as the value for a parameter
			// that expects the base struct, or vice versa.  An easier example is assignment (e.g. Vector = Plane or Plane = Vector).
			// there are two cases to consider (let's use vector and plane for the example):
			// - Vector = Plane;
			//		in this expression, 'this' is the vector, and Other is the plane.  This is an unsafe conversion, as the destination property type is used to copy the r-value to the l-value
			//		so in this case, the VM would call CopyCompleteValue on the FPlane struct, which would copy 16 bytes into the l-value's buffer;  However, the l-value buffer will only be
			//		12 bytes because that is the size of FVector
			// - Plane = Vector;
			//		in this expression, 'this' is the plane, and Other is the vector.  This is a safe conversion, since only 12 bytes would be copied from the r-value into the l-value's buffer
			//		(which would be 16 bytes).  The problem with allowing this conversion is that what to do with the extra member (e.g. Plane.W); should it be left alone? should it be zeroed?
			//		difficult to say what the correct behavior should be, so let's just ignore inheritance for the sake of determining whether two structs are identical

			// Previously, the logic for determining whether this is a generalization of Other was reversed; this is very likely the culprit behind all current issues with 
			// using derived structs interchangeably with their base versions.  The inheritance check has been fixed; for now, allow struct generalization and see if we can find any further
			// issues with allowing conversion.  If so, then we disable all struct generalization by returning false here.
 			// return false;

			if ( bDisallowGeneralization )
			{
				return false;
			}

			// Generalization is ok if this is not a dynamic array
			if ( ArrayType != EArrayType::Dynamic && Other.ArrayType != EArrayType::Dynamic )
			{
				if ( !Other.Struct->IsChildOf(Struct) && Struct->IsChildOf(Other.Struct) )
				{
					return true;
				}
			}

			return false;
		}
		else
		{
			// General match.
			return true;
		}
	}

	FString Describe()
	{
		return FString::Printf(
			TEXT("Type:%s  Flags:%lli  ImpliedFlags:%lli  Enum:%s  PropertyClass:%s  Struct:%s  Function:%s  MetaClass:%s"),
			GetPropertyTypeText(Type), PropertyFlags, ImpliedPropertyFlags,
			Enum!=NULL?*Enum->GetName():TEXT(""),
			PropertyClass!=NULL?*PropertyClass->GetName():TEXT("NULL"),
			Struct!=NULL?*Struct->GetName():TEXT("NULL"),
			Function!=NULL?*Function->GetName():TEXT("NULL"),
			MetaClass!=NULL?*MetaClass->GetName():TEXT("NULL")
			);
	}
	//@}

	EIntType GetSizedIntTypeFromPropertyType(EPropertyType PropType)
	{
		switch (PropType)
		{
			case CPT_Byte:
			case CPT_UInt16:
			case CPT_UInt32:
			case CPT_UInt64:
			case CPT_Int8:
			case CPT_Int16:
			case CPT_Int:
			case CPT_Int64:
				return EIntType::Sized;

			default:
				return EIntType::None;
		}
	}

	static const TCHAR* GetPropertyTypeText( EPropertyType Type );

	friend struct FPropertyBaseArchiveProxy;
};

//
// Token types.
//
enum ETokenType
{
	TOKEN_None				= 0x00,		// No token.
	TOKEN_Identifier		= 0x01,		// Alphanumeric identifier.
	TOKEN_Symbol			= 0x02,		// Symbol.
	TOKEN_Const				= 0x03,		// A constant.
	TOKEN_Max				= 0x0D
};

/*-----------------------------------------------------------------------------
	FToken.
-----------------------------------------------------------------------------*/
//
// Information about a token that was just parsed.
//
class FToken : public FPropertyBase
{
public:
	/** @name Variables */
	//@{
	/** Type of token. */
	ETokenType TokenType;
	/** Name of token. */
	FName TokenName;
	/** Starting position in script where this token came from. */
	int32 StartPos;
	/** Starting line in script. */
	int32 StartLine;
	/** Always valid. */
	TCHAR Identifier[NAME_SIZE];
	/** property that corresponds to this FToken - null if this Token doesn't correspond to a UProperty */
	UProperty* TokenProperty;

	union
	{
		// TOKEN_Const values.
		uint8 Byte;								// If CPT_Byte.
		int64 Int64;							// If CPT_Int64.
		int32 Int;								// If CPT_Int.
		bool NativeBool;						// if CPT_Bool
		float Float;							// If CPT_Float.
		double Double;							// If CPT_Double.
		uint8 NameBytes[sizeof(FName)];			// If CPT_Name.
		TCHAR String[MAX_STRING_CONST_SIZE];	// If CPT_String
	};
	//@}

	/**
	 * Copies the properties from this token into another.
	 *
	 * @param	Other	the token to copy this token's properties to.
	 */
	void Clone( const FToken& Other );

	FString GetConstantValue() const
	{
		if (TokenType == TOKEN_Const)
		{
			switch (Type)
			{
				case CPT_Byte:
					return FString::Printf(TEXT("%u"), Byte);
				case CPT_Int64:
					return FString::Printf(TEXT("%ld"), Int64);
				case CPT_Int:
					return FString::Printf(TEXT("%i"), Int);
				case CPT_Bool:
					// Don't use GTrue/GFalse here because they can be localized
					return FString::Printf(TEXT("%s"), NativeBool ? *(FName::GetEntry(NAME_TRUE)->GetPlainNameString()) : *(FName::GetEntry(NAME_FALSE)->GetPlainNameString()));
				case CPT_Float:
					return FString::Printf(TEXT("%f"), Float);
				case CPT_Double:
					return FString::Printf(TEXT("%f"), Double);
				case CPT_Name:
					return FString::Printf(TEXT("%s"), *(*(FName*)NameBytes).ToString());
				case CPT_String:
					return String;

				// unsupported (parsing never produces a constant token of these types)
				default:
					return TEXT("InvalidTypeForAToken");
			}
		}
		else
		{
			return TEXT("NotConstant");
		}
	}

	// Constructors.
	FToken()
		: FPropertyBase(CPT_None)
		, TokenProperty(NULL)
	{
		InitToken(CPT_None);
	}

	// copy constructors
	explicit FToken(EPropertyType InType)
		: FPropertyBase(InType)
		, TokenProperty(NULL)
	{
		InitToken(InType);
	}

	// copy constructors
	FToken(const FPropertyBase& InType)
		: FPropertyBase(CPT_None)
		, TokenProperty(NULL)
	{
		InitToken( CPT_None );
		(FPropertyBase&)*this = InType;
	}

	// Inlines.
	void InitToken( EPropertyType InType )
	{
		(FPropertyBase&)*this = FPropertyBase(InType);
		TokenType		= TOKEN_None;
		TokenName		= NAME_None;
		StartPos		= 0;
		StartLine		= 0;
		*Identifier		= 0;
		FMemory::Memzero(String);
	}
	bool Matches( const TCHAR* Str, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase ) const
	{
		return (TokenType==TOKEN_Identifier || TokenType==TOKEN_Symbol) && ((SearchCase == ESearchCase::CaseSensitive) ? !FCString::Strcmp(Identifier, Str) : !FCString::Stricmp(Identifier, Str));
	}
	bool Matches( const FName& Name ) const
	{
		return TokenType==TOKEN_Identifier && TokenName==Name;
	}
	bool StartsWith( const TCHAR* Str, bool bCaseSensitive = false ) const
	{
		const int32 StrLength = FCString::Strlen(Str);
		return (TokenType==TOKEN_Identifier || TokenType==TOKEN_Symbol) && (bCaseSensitive ? (!FCString::Strncmp(Identifier, Str, StrLength)) : (!FCString::Strnicmp(Identifier, Str, StrLength)));
	}
	bool IsBool() const
	{
		return Type == CPT_Bool || Type == CPT_Bool8 || Type == CPT_Bool16 || Type == CPT_Bool32 || Type == CPT_Bool64;
	}

	// Setters.
	
	void SetIdentifier( const TCHAR* InString)
	{
		InitToken(CPT_None);
		TokenType = TOKEN_Identifier;
		FCString::Strncpy(Identifier, InString, NAME_SIZE);
		TokenName = FName(Identifier, FNAME_Find);
	}

	void SetConstInt64( int64 InInt64 )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Int64);
		Int64			= InInt64;
		TokenType		= TOKEN_Const;
	}
	void SetConstInt( int32 InInt )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Int);
		Int				= InInt;
		TokenType		= TOKEN_Const;
	}
	void SetConstBool( bool InBool )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Bool);
		NativeBool 		= InBool;
		TokenType		= TOKEN_Const;
	}
	void SetConstFloat( float InFloat )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Float);
		Float			= InFloat;
		TokenType		= TOKEN_Const;
	}
	void SetConstDouble( double InDouble )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Double);
		Double			= InDouble;
		TokenType		= TOKEN_Const;
	}
	void SetConstName( FName InName )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Name);
		*(FName *)NameBytes = InName;
		TokenType		= TOKEN_Const;
	}
	void SetConstString( const TCHAR* InString, int32 MaxLength=MAX_STRING_CONST_SIZE )
	{
		check(MaxLength>0);
		(FPropertyBase&)*this = FPropertyBase(CPT_String);
		if( InString != String )
		{
			FCString::Strncpy( String, InString, MaxLength );
		}
		TokenType = TOKEN_Const;
	}
	void SetConstChar(TCHAR InChar)
	{
		//@TODO: Treating this like a string for now, nothing consumes it
		(FPropertyBase&)*this = FPropertyBase(CPT_String);
		String[0] = InChar;
		String[1] = 0;
		TokenType = TOKEN_Const;
	}
	//!!struct constants

	// Getters.
	bool GetConstInt( int32& I ) const
	{
		if( TokenType==TOKEN_Const && Type==CPT_Int64 )
		{
			I = Int64;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Int )
		{
			I = Int;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Byte )
		{
			I = Byte;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Float && Float==FMath::TruncToInt(Float))
		{
			I = (int32) Float;
			return 1;
		}
		else if (TokenType == TOKEN_Const && Type == CPT_Double && Float == FMath::TruncToInt(Double))
		{
			I = (int32) Double;
			return 1;
		}
		else return 0;
	}

	bool GetConstInt64( int64& I ) const
	{
		if( TokenType==TOKEN_Const && Type==CPT_Int64 )
		{
			I = Int64;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Int )
		{
			I = Int;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Byte )
		{
			I = Byte;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Float && Float==FMath::TruncToInt(Float))
		{
			I = (int32) Float;
			return 1;
		}
		else if (TokenType == TOKEN_Const && Type == CPT_Double && Float == FMath::TruncToInt(Double))
		{
			I = (int32) Double;
			return 1;
		}
		else return 0;
	}

	FString Describe()
	{
		return FString::Printf(
			TEXT("Property:%s  Type:%s  TokenName:%s  ConstValue:%s  Struct:%s  Flags:%lli  Implied:%lli"),
			TokenProperty!=NULL?*TokenProperty->GetName():TEXT("NULL"),
			GetPropertyTypeText(Type), *TokenName.ToString(), *GetConstantValue(),
			Struct!=NULL?*Struct->GetName():TEXT("NULL"),
			PropertyFlags,
			ImpliedPropertyFlags
			);
	}

	friend struct FTokenArchiveProxy;
};

/**
 * A group of FTokens.  Used for keeping track of reference chains tokens
 * e.g. SomeObject.default.Foo.DoSomething()
 */
class FTokenChain : public TArray<FToken>
{
public:
	FToken& operator+=( const FToken& NewToken )
	{
		FToken& Token = (*this)[AddZeroed()] = NewToken;
		return Token;
	}
};

/**
 * Information about a function being compiled.
 */
struct FFuncInfo
{
	/** @name Variables */
	//@{
	/** Name of the function or operator. */
	FToken		Function;
	/** Function flags. */
	EFunctionFlags	FunctionFlags;
	/** Function flags which are only required for exporting */
	uint32		FunctionExportFlags;
	/** Number of parameters expected for operator. */
	int32		ExpectParms;
	/** Pointer to the UFunction corresponding to this FFuncInfo */
	UFunction*	FunctionReference;
	/** Name of the wrapper function that marshalls the arguments and does the indirect call **/
	FString		MarshallAndCallName;
	/** Name of the actual implementation **/
	FString		CppImplName;
	/** Name of the actual validation implementation **/
	FString		CppValidationImplName;
	/** Name for callback-style names **/
	FString		UnMarshallAndCallName;
	/** Endpoint name */
	FString		EndpointName;
	/** Identifier for an RPC call to a platform service */
	int16		RPCId;
	/** Identifier for an RPC call expecting a response */
	int16		RPCResponseId;
	/** Whether this function represents a sealed event */
	bool		bSealedEvent;
	/** Delegate macro line in header. */
	int32		MacroLine;
	/** Position in file where this function was declared. Points to first char of function name. */
	int32 InputPos;
	/** TRUE if the function is being forced to be considered as impure by the user */
	bool bForceBlueprintImpure;

	//@}

	/** Constructor. */
	FFuncInfo()
		: Function()
		, FunctionFlags(FUNC_None)
		, FunctionExportFlags(0)
		, ExpectParms(0)
		, FunctionReference(NULL)
		, CppImplName(TEXT(""))
		, CppValidationImplName(TEXT(""))
		, RPCId(0)
		, RPCResponseId(0)
		, bSealedEvent(false)
		, MacroLine(-1)
		, InputPos(-1)
		, bForceBlueprintImpure(false)
	{}

	FFuncInfo(const FFuncInfo& Other)
		: FunctionFlags(Other.FunctionFlags)
		, FunctionExportFlags(Other.FunctionExportFlags)
		, ExpectParms(Other.ExpectParms)
		, FunctionReference(Other.FunctionReference)
		, CppImplName(Other.CppImplName)
		, CppValidationImplName(Other.CppValidationImplName)
		, RPCId(Other.RPCId)
		, RPCResponseId(Other.RPCResponseId)
		, MacroLine(Other.MacroLine)
		, InputPos(Other.InputPos)
		, bForceBlueprintImpure(Other.bForceBlueprintImpure)
	{
		Function.Clone(Other.Function);
		if (FunctionReference)
		{
			SetFunctionNames();
		}
	}

	/** Set the internal function names based on flags **/
	void SetFunctionNames()
	{
		FString FunctionName = FunctionReference->GetName();
		if( FunctionReference->HasAnyFunctionFlags( FUNC_Delegate ) )
		{
			FunctionName = FunctionName.LeftChop( FString( TEXT( "__DelegateSignature" ) ).Len() );
		}
		UnMarshallAndCallName = FString(TEXT("exec")) + FunctionName;
		
		if (FunctionReference->HasAnyFunctionFlags(FUNC_BlueprintEvent))
		{
			MarshallAndCallName = FunctionName;
		}
		else
		{
			MarshallAndCallName = FString(TEXT("event")) + FunctionName;
		}

		if (FunctionReference->HasAllFunctionFlags(FUNC_Native | FUNC_Net))
		{
			MarshallAndCallName = FunctionName;
			if (FunctionReference->HasAllFunctionFlags(FUNC_NetResponse))
			{
				// Response function implemented by programmer and called directly from thunk
				CppImplName = FunctionReference->GetName();
			}
			else
			{
				if (CppImplName.IsEmpty())
				{
					CppImplName = FunctionReference->GetName() + TEXT("_Implementation");
				}
				else if (CppImplName == FunctionName)
				{
					FError::Throwf(TEXT("Native implementation function must be different than original function name."));
				}

				if (CppValidationImplName.IsEmpty() && FunctionReference->HasAllFunctionFlags(FUNC_NetValidate))
				{
					CppValidationImplName = FunctionReference->GetName() + TEXT("_Validate");
				}
				else if(CppValidationImplName == FunctionName)
				{
					FError::Throwf(TEXT("Validation function must be different than original function name."));
				}
			}
		}

		if (FunctionReference->HasAllFunctionFlags(FUNC_Delegate))
		{
			MarshallAndCallName = FString(TEXT("delegate")) + FunctionName;
		}

		if (FunctionReference->HasAllFunctionFlags(FUNC_BlueprintEvent | FUNC_Native))
		{
			MarshallAndCallName = FunctionName;
			CppImplName = FunctionReference->GetName() + TEXT("_Implementation");
		}

		if (CppImplName.IsEmpty())
		{
			CppImplName = FunctionName;
		}
	}
};

/**
 * Stores "compiler" data about an FToken.  "Compiler" data is data that is associated with a
 * specific property, function or class that is only needed during script compile.
 * This class is designed to make adding new compiler data very simple.
 *
 * - stores the raw evaluated bytecode associated with an FToken
 */
struct FTokenData
{
	/** The token tracked by this FTokenData. */
	FToken Token;

	/** @name Constructors */
	//@{
	/**
	 * Defalt constructor
	 */
	FTokenData()
	{}

	/**
	 * Copy constructor
	 */
	FTokenData(const FToken& inToken)
	 : Token(inToken)
	{}
	//@}
};

/**
 * Class for storing data about a list of properties.  Though FToken contains a reference to its
 * associated UProperty, it's faster lookup to use the UProperty as the key in a TMap.
 */
class FPropertyData : public TMap< UProperty*, TSharedPtr<FTokenData> >
{
	typedef TMap<UProperty*, TSharedPtr<FTokenData> >	Super;

public:
	/**
	 * Returns the value associated with a specified key.
	 * @param	Key - The key to search for.
	 * @return	A pointer to the value associated with the specified key, or NULL if the key isn't contained in this map.  The pointer
	 *			is only valid until the next change to any key in the map.
	 */
	FTokenData* Find(UProperty* Key)
	{
		FTokenData* Result = NULL;

		TSharedPtr<FTokenData>* pResult = Super::Find(Key);
		if ( pResult != NULL )
		{
			Result = pResult->Get();
		}
		return Result;
	}
	const FTokenData* Find(UProperty* Key) const
	{
		const FTokenData* Result = NULL;

		const TSharedPtr<FTokenData>* pResult = Super::Find(Key);
		if ( pResult != NULL )
		{
			Result = pResult->Get();
		}
		return Result;
	}

	/**
	 * Sets the value associated with a key.  If the key already exists in the map, uses the same
	 * value pointer and reinitalized the FTokenData with the input value.
	 *
	 * @param	InKey	the property to get a token wrapper for
	 * @param	InValue	the token wrapper for the specified key
	 *
	 * @return	a pointer to token data created associated with the property
	 */
	FTokenData* Set(UProperty* InKey, const FTokenData& InValue, FUnrealSourceFile* UnrealSourceFile);

	/**
	 * (debug) Dumps the values of this FPropertyData to the log file
	 * 
	 * @param	Indent	number of spaces to insert at the beginning of each line
	 */	
	void Dump( int32 Indent )
	{
		for (auto& Kvp : *this)
		{
			TSharedPtr<FTokenData>& PointerVal = Kvp.Value;
			FToken& Token = PointerVal->Token;
			if ( Token.Type != CPT_None )
			{
				UE_LOG(LogCompile, Log, TEXT("%s%s"), FCString::Spc(Indent), *Token.Describe());
			}
		}
	}
};

/**
 * Class for storing additional data about compiled structs and struct properties
 */
class FStructData
{
public:
	/** info about the struct itself */
	FToken			StructData;

private:
	/** info for the properties contained in this struct */
	FPropertyData	StructPropertyData;

public:
	/**
	 * Adds a new struct property token
	 * 
	 * @param	PropertyToken	token that should be added to the list
	 */
	void AddStructProperty(const FTokenData& PropertyToken, FUnrealSourceFile* UnrealSourceFile)
	{
		check(PropertyToken.Token.TokenProperty);
		StructPropertyData.Set(PropertyToken.Token.TokenProperty, PropertyToken, UnrealSourceFile);
	}

	FPropertyData& GetStructPropertyData()
	{
		return StructPropertyData;
	}
	const FPropertyData& GetStructPropertyData() const
	{
		return StructPropertyData;
	}

	/**
	* (debug) Dumps the values of this FStructData to the log file
	* 
	* @param	Indent	number of spaces to insert at the beginning of each line
	*/	
	void Dump( int32 Indent )
	{
		UE_LOG(LogCompile, Log, TEXT("%s%s"), FCString::Spc(Indent), *StructData.Describe());

		UE_LOG(LogCompile, Log, TEXT("%sproperties:"), FCString::Spc(Indent));
		StructPropertyData.Dump(Indent + 4);
	}

	/** Constructor */
	FStructData( const FToken& StructToken ) : StructData(StructToken) {}

	friend struct FStructDataArchiveProxy;
};

/**
 * Class for storing additional data about compiled function properties.
 */
class FFunctionData
{
	/** info about the function associated with this FFunctionData */
	FFuncInfo		FunctionData;

	/** return value for this function */
	FTokenData		ReturnTypeData;

	/** function parameter data */
	FPropertyData	ParameterData;

	/**
	 * Adds a new parameter token
	 * 
	 * @param	PropertyToken	token that should be added to the list
	 */
	void AddParameter(const FToken& PropertyToken, FUnrealSourceFile* UnrealSourceFile)
	{
		check(PropertyToken.TokenProperty);
		ParameterData.Set(PropertyToken.TokenProperty, PropertyToken, UnrealSourceFile);
	}

	/**
	 * Sets the value of the return token for this function
	 * 
	 * @param	PropertyToken	token that should be added
	 */
	void SetReturnData( const FToken& PropertyToken )
	{
		check(PropertyToken.TokenProperty);
		ReturnTypeData.Token = PropertyToken;
	}

public:
	/** Constructors */
	FFunctionData() {}
	FFunctionData( const FFunctionData& Other )
	{
		(*this) = Other;
	}
	FFunctionData( const FFuncInfo& inFunctionData )
	: FunctionData(inFunctionData)
	{}

	/** Copy operator */
	FFunctionData& operator=( const FFunctionData& Other )
	{
		FunctionData = Other.FunctionData;
		ParameterData = Other.ParameterData;
		ReturnTypeData.Token.Clone(Other.ReturnTypeData.Token);
		return *this;
	}
	
	/** @name getters */
	//@{
	const	FFuncInfo&		GetFunctionData()	const	{	return FunctionData;	}
	const	FToken&			GetReturnData()		const	{	return ReturnTypeData.Token;	}
	const	FPropertyData&	GetParameterData()	const	{	return ParameterData;	}
	FPropertyData&			GetParameterData()			{	return ParameterData;	}
	FTokenData* GetReturnTokenData() { return &ReturnTypeData; }
	//@}

	void UpdateFunctionData(FFuncInfo& UpdatedFuncData)
	{
		//@TODO: UCREMOVAL: Some more thorough evaluation should be done here
		FunctionData.FunctionFlags |= UpdatedFuncData.FunctionFlags;
		FunctionData.FunctionExportFlags |= UpdatedFuncData.FunctionExportFlags;
	}

	/**
	 * Adds a new function property to be tracked.  Determines whether the property is a
	 * function parameter, local property, or return value, and adds it to the appropriate
	 * list
	 * 
	 * @param	PropertyToken	the property to add
	 */
	void AddProperty(const FToken& PropertyToken, FUnrealSourceFile* UnrealSourceFile)
	{
		const UProperty* Prop = PropertyToken.TokenProperty;
		check(Prop);
		check( (Prop->PropertyFlags&CPF_Parm) != 0 );

		if ( (Prop->PropertyFlags&CPF_ReturnParm) != 0 )
		{
			SetReturnData(PropertyToken);
		}
		else
		{
			AddParameter(PropertyToken, UnrealSourceFile);
		}
	}

	/**
	 * (debug) Dumps the values of this FFunctionData to the log file
	 * 
	 * @param	Indent	number of spaces to insert at the beginning of each line
	 */	
	void Dump( int32 Indent )
	{
		UE_LOG(LogCompile, Log, TEXT("%sparameters:"), FCString::Spc(Indent));
		ParameterData.Dump(Indent + 4);

		UE_LOG(LogCompile, Log, TEXT("%sreturn prop:"), FCString::Spc(Indent));
		if ( ReturnTypeData.Token.Type != CPT_None )
		{
			UE_LOG(LogCompile, Log, TEXT("%s%s"), FCString::Spc(Indent + 4), *ReturnTypeData.Token.Describe());
		}
	}

	/**
	 * Sets the specified function export flags
	 */
	void SetFunctionExportFlag( uint32 NewFlags )
	{
		FunctionData.FunctionExportFlags |= NewFlags;
	}

	/**
	 * Clears the specified function export flags
	 */
	void ClearFunctionExportFlags( uint32 ClearFlags )
	{
		FunctionData.FunctionExportFlags &= ~ClearFlags;
	}

	/**
	 * Finds function data for given function object.
	 */
	static FFunctionData* FindForFunction(UFunction* Function);

	/**
	 * Adds function data object for given function object.
	 */
	static FFunctionData* Add(UFunction* Function);

	/**
	 * Adds function data object for given function object.
	 */
	static FFunctionData* Add(const FFuncInfo& FunctionInfo);

	/**
	 * Tries to find function data for given function object.
	 */
	static bool TryFindForFunction(UFunction* Function, FFunctionData*& OutData);

private:
	static TMap<UFunction*, TUniqueObj<FFunctionData> > FunctionDataMap;
};

/**
 * Tracks information about a multiple inheritance parent declaration for native script classes.
 */
struct FMultipleInheritanceBaseClass
{
	/**
	 * The name to use for the base class when exporting the script class to header file.
	 */
	FString ClassName;

	/**
	 * For multiple inheritance parents declared using 'Implements', corresponds to the UClass for the interface.  For multiple inheritance parents declared
	 * using 'Inherits', this value will be NULL.
	 */
	UClass* InterfaceClass;

	/**
	 * Constructors
	 */
	FMultipleInheritanceBaseClass(const FString& BaseClassName)
	: ClassName(BaseClassName), InterfaceClass(NULL)
	{}

	FMultipleInheritanceBaseClass(UClass* ImplementedInterfaceClass)
	: InterfaceClass(ImplementedInterfaceClass)
	{
		ClassName = FString::Printf(TEXT("I%s"), *ImplementedInterfaceClass->GetName());
	}
};

/**
 * Class for storing compiler metadata about a class's properties.
 */
class FClassMetaData
{
	/** member properties for this class */
	FPropertyData											GlobalPropertyData;

	/** base classes to multiply inherit from (other than the main base class */
	TArray<FMultipleInheritanceBaseClass*>					MultipleInheritanceParents;

	/** whether this class declares delegate functions or properties */
	bool													bContainsDelegates;

	/** The line of UCLASS/UINTERFACE macro in this class. */
	int32 PrologLine;

	/** The line of GENERATED_BODY/GENERATED_UCLASS_BODY macro in this class. */
	int32 GeneratedBodyLine;

	/** Same as above, but for interface class associated with this class. */
	int32 InterfaceGeneratedBodyLine;

public:
	/** Default constructor */
	FClassMetaData()
		: bContainsDelegates(false)
		, PrologLine(-1)
		, GeneratedBodyLine(-1)
		, InterfaceGeneratedBodyLine(-1)
		, bConstructorDeclared(false)
		, bDefaultConstructorDeclared(false)
		, bObjectInitializerConstructorDeclared(false)
		, bCustomVTableHelperConstructorDeclared(false)
		, GeneratedBodyMacroAccessSpecifier(ACCESS_NotAnAccessSpecifier)
	{
	}

	/**
	 * Gets prolog line number for this class.
	 */
	int32 GetPrologLine() const
	{
		check(PrologLine > 0);
		return PrologLine;
	}

	/**
	 * Gets generated body line number for this class.
	 */
	int32 GetGeneratedBodyLine() const
	{
		check(GeneratedBodyLine > 0);
		return GeneratedBodyLine;
	}

	/**
	 * Gets interface generated body line number for this class.
	 */
	int32 GetInterfaceGeneratedBodyLine() const
	{
		check(InterfaceGeneratedBodyLine > 0);
		return InterfaceGeneratedBodyLine;
	}

	/**
	 * Sets prolog line number for this class.
	 */
	void SetPrologLine(int32 Line)
	{
		check(Line > 0);
		PrologLine = Line;
	}

	/**
	 * Sets generated body line number for this class.
	 */
	void SetGeneratedBodyLine(int32 Line)
	{
		check(Line > 0);
		GeneratedBodyLine = Line;
	}

	/**
	 * Sets interface generated body line number for this class.
	 */
	void SetInterfaceGeneratedBodyLine(int32 Line)
	{
		check(Line > 0);
		InterfaceGeneratedBodyLine = Line;
	}

	/**
	 * Sets contains delegates flag for this class.
	 */
	void MarkContainsDelegate()
	{
		bContainsDelegates = true;
	}

	/**
	 * Adds a new property to be tracked.  Determines the correct list for the property based on
	 * its owner (function, struct, etc).
	 * 
	 * @param	PropertyToken	the property to add
	 */
	void AddProperty(const FToken& PropertyToken, FUnrealSourceFile* UnrealSourceFile)
	{
		UProperty* Prop = PropertyToken.TokenProperty;
		check(Prop);

		UObject* Outer = Prop->GetOuter();
		UStruct* OuterClass = Cast<UStruct>(Outer);
		if ( OuterClass != NULL )
		{
			// global property
			GlobalPropertyData.Set(Prop, PropertyToken, UnrealSourceFile);
		}
		else
		{
			checkNoEntry();
			UFunction* OuterFunction = Cast<UFunction>(Outer);
			if ( OuterFunction != NULL )
			{
				// function parameter, return, or local property
				FFunctionData::FindForFunction(OuterFunction)->AddProperty(PropertyToken, UnrealSourceFile);
			}
		}

		// update the optimization flags
		if ( !bContainsDelegates )
		{
			if( Prop->IsA( UDelegateProperty::StaticClass() ) || Prop->IsA( UMulticastDelegateProperty::StaticClass() ) )
			{
				bContainsDelegates = true;
			}
			else
			{
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop);
				if ( ArrayProp != NULL )
				{
					if( ArrayProp->Inner->IsA( UDelegateProperty::StaticClass() ) || ArrayProp->Inner->IsA( UMulticastDelegateProperty::StaticClass() ) )
					{
						bContainsDelegates = true;
					}
				}
			}
		}
	}

	/**
	 * Adds new editor-only metadata (key/value pairs) to the class or struct that
	 * owns this property or function.
	 * 
	 * @param	Field		the property or function to add to
	 * @param	InMetaData	the metadata to add
	 */
	static void AddMetaData(UField* Field, const TMap<FName, FString>& InMetaData)
	{
		// only add if we have some!
		if (InMetaData.Num())
		{
			check(Field);

			// get (or create) a metadata object for this package
			UMetaData* MetaData = Field->GetOutermost()->GetMetaData();

			// set the metadata for this field
			MetaData->SetObjectValues(Field, InMetaData);
		}
	}


	/**
	 * Finds the metadata for the function specified
	 * 
	 * @param	Func	the function to search for
	 *
	 * @return	pointer to the metadata for the function specified, or NULL
	 *			if the function doesn't exist in the list (for example, if it
	 *			is declared in a package that is already compiled and has had its
	 *			source stripped)
	 */
	FFunctionData* FindFunctionData( UFunction* Func );

	/**
	 * Finds the metadata for the property specified
	 * 
	 * @param	Prop	the property to search for
	 *
	 * @return	pointer to the metadata for the property specified, or NULL
	 *			if the property doesn't exist in the list (for example, if it
	 *			is declared in a package that is already compiled and has had its
	 *			source stripped)
	 */
	FTokenData* FindTokenData( UProperty* Prop );

	/**
	 * (debug) Dumps the values of this FFunctionData to the log file
	 * 
	 * @param	Indent	number of spaces to insert at the beginning of each line
	 */	
	void Dump( int32 Indent );

	/**
	 * Add a string to the list of inheritance parents for this class.
	 *
	 * @param Inparent The C++ class name to add to the multiple inheritance list
	 * @param UnrealSourceFile Currently parsed source file.
	 */
	void AddInheritanceParent(const FString& InParent, FUnrealSourceFile* UnrealSourceFile);

	/**
	 * Add a string to the list of inheritance parents for this class.
	 *
	 * @param Inparent	The C++ class name to add to the multiple inheritance list
	 * @param UnrealSourceFile Currently parsed source file.
	 */
	void AddInheritanceParent(UClass* ImplementedInterfaceClass, FUnrealSourceFile* UnrealSourceFile);

	/**
	 * Return the list of inheritance parents
	 */
	const TArray<FMultipleInheritanceBaseClass*>& GetInheritanceParents() const
	{
		return MultipleInheritanceParents;
	}

	/**
	 * Returns whether this class contains any delegate properties which need to be fixed up.
	 */
	bool ContainsDelegates() const
	{
		return bContainsDelegates;
	}

	/**
	 * Shrink TMaps to avoid slack in Pairs array.
	 */
	void Shrink()
	{
		GlobalPropertyData.Shrink();
		MultipleInheritanceParents.Shrink();
	}

	// Is constructor declared?
	bool bConstructorDeclared;

	// Is default constructor declared?
	bool bDefaultConstructorDeclared;

	// Is ObjectInitializer constructor (i.e. a constructor with only one parameter of type FObjectInitializer) declared?
	bool bObjectInitializerConstructorDeclared;

	// Is custom VTable helper constructor declared?
	bool bCustomVTableHelperConstructorDeclared;

	// GENERATED_BODY access specifier to preserve.
	EAccessSpecifier GeneratedBodyMacroAccessSpecifier;

	friend struct FClassMetaDataArchiveProxy;
};

/**
 * Class for storing and linking data about properties and functions that is only required by the compiler.
 * The type of data tracked by this class is data that would otherwise only be accessible by adding a 
 * member property to UFunction/UProperty.  
 */
class FCompilerMetadataManager : protected TMap<UStruct*, TUniquePtr<FClassMetaData> >
{
public:
	/**
	 * Adds a new class to be tracked
	 * 
	 * @param	Struct	the UStruct to add
	 *
	 * @return	a pointer to the newly added metadata for the class specified
	 */
	FClassMetaData* AddClassData(UStruct* Struct, FUnrealSourceFile* UnrealSourceFile);

	/**
	 * Find the metadata associated with the class specified
	 * 
	 * @param	Struct	the UStruct to add
	 *
	 * @return	a pointer to the newly added metadata for the class specified
	 */
	FClassMetaData* FindClassData(UStruct* Struct)
	{
		FClassMetaData* Result = nullptr;

		TUniquePtr<FClassMetaData>* pClassData = Find(Struct);
		if (pClassData)
		{
			Result = pClassData->Get();
		}

		return Result;
	}

	/**
	 * Shrink TMaps to avoid slack in Pairs array.
	 */
	void Shrink()
	{
		TMap<UStruct*, TUniquePtr<FClassMetaData> >::Shrink();
		for (TMap<UStruct*, TUniquePtr<FClassMetaData> >::TIterator It(*this); It; ++It)
		{
			FClassMetaData* MetaData = It->Value.Get();
			MetaData->Shrink();
		}
	}

	friend struct FCompilerMetadataManagerArchiveProxy;
};

/*-----------------------------------------------------------------------------
	Retry points.
-----------------------------------------------------------------------------*/

/**
 * A point in the header parsing state that can be set and returned to
 * using InitScriptLocation() and ReturnToLocation().  This is used in cases such as testing
 * to see which overridden operator should be used, where code must be compiled
 * and then "undone" if it was found not to match.
 * <p>
 * Retries are not allowed to cross command boundaries (and thus nesting 
 * boundaries).  Retries can occur across a single command or expressions and
 * subexpressions within a command.
 */
struct FScriptLocation
{
	static class FHeaderParser* Compiler;

	/** the text buffer for the class associated with this retry point */
	const TCHAR* Input;

	/** the position into the Input buffer where this retry point is located */
	int32 InputPos;

	/** the LineNumber of the compiler when this retry point was created */
	int32 InputLine;

	/** Constructor */
	FScriptLocation();
};

/////////////////////////////////////////////////////
// FNameLookupCPP

/**
 * Helper class used to cache UClass* -> TCHAR* name lookup for finding the named used for C++ declaration.
 */
struct FNameLookupCPP
{
	/**
	 * Destructor, cleaning up allocated memory.
	 */
	~FNameLookupCPP()
	{
		for (TMap<UStruct*,TCHAR*>::TIterator It(StructNameMap); It; ++It)
		{
			TCHAR* Name = It.Value();
			delete [] Name;
		}
	
		for (TArray<TCHAR*>::TIterator It(InterfaceAllocations); It; ++It)
		{
			TCHAR* Name = *It;
			delete [] Name;
		}
	}

	/**
	 * Returns the name used for declaring the passed in struct in C++
	 *
	 * @param	Struct	UStruct to obtain C++ name for
	 * @return	Name used for C++ declaration
	 */
	const TCHAR* GetNameCPP( UStruct* Struct, bool bForceInterface = false );

	void SetCurrentSourceFile(FUnrealSourceFile* InUnrealSourceFile)
	{
		UnrealSourceFile = InUnrealSourceFile;
	}

private:
	/** Map of UStruct pointers to C++ names */
	TMap<UStruct*,TCHAR*> StructNameMap;
	TArray<TCHAR*> InterfaceAllocations;

	FUnrealSourceFile* UnrealSourceFile;
};

extern FNameLookupCPP NameLookupCPP;


/////////////////////////////////////////////////////
// FAdvancedDisplayParameterHandler - used by FHeaderParser::ParseParameterList, to check if a property if a function parameter has 'AdvancedDisplay' flag

/** 
 *	AdvancedDisplay can be used in two ways:
 *	1. 'AdvancedDisplay = "3"' - the number tells how many parameters (from beginning) should NOT BE marked
 *	2. 'AdvancedDisplay = "AttachPointName, Location, LocationType"' - list the parameters, that should BE marked
 */
class FAdvancedDisplayParameterHandler
{
	TArray<FString> ParametersNames;

	int32 NumberLeaveUnmarked;
	int32 AlreadyLeft;

	bool bUseNumber;
public:
	FAdvancedDisplayParameterHandler(const TMap<FName, FString>* MetaData);

	/** 
	 * return if given parameter should be marked as Advance View, 
	 * the function should be called only once for any parameter
	 */
	bool ShouldMarkParameter(const FString& ParameterName);

	/** return if more parameters can be marked */
	bool CanMarkMore() const;
};
