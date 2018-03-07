// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealType.h: Unreal engine base type definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/CoreNetTypes.h"
#include "UObject/ScriptInterface.h"
#include "Templates/Casts.h"
#include "Templates/IsFloatingPoint.h"
#include "Templates/IsIntegral.h"
#include "Templates/IsSigned.h"
#include "Templates/Greater.h"
#include "Containers/List.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/PropertyTag.h"
#include "Serialization/SerializedPropertyScope.h"

COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogType, Log, All);

/*-----------------------------------------------------------------------------
	UProperty.
-----------------------------------------------------------------------------*/

enum EPropertyExportCPPFlags
{
	/** Indicates that there are no special C++ export flags */
	CPPF_None						=	0x00000000,
	/** Indicates that we are exporting this property's CPP text for an optional parameter value */
	CPPF_OptionalValue				=	0x00000001,
	/** Indicates that we are exporting this property's CPP text for an argument or return value */
	CPPF_ArgumentOrReturnValue		=	0x00000002,
	/** Indicates thet we are exporting this property's CPP text for C++ definition of a function. */
	CPPF_Implementation				=	0x00000004,
	/** Indicates thet we are exporting this property's CPP text with an custom type name */
	CPPF_CustomTypeName				=	0x00000008,
	/** No 'const' keyword */
	CPPF_NoConst					=	0x00000010,
	/** No reference '&' sign */
	CPPF_NoRef						=	0x00000020,
	/** No static array [%d] */
	CPPF_NoStaticArray				=	0x00000040,
	/** Blueprint compiler generated C++ code */
	CPPF_BlueprintCppBackend		=	0x00000080,
};

namespace EExportedDeclaration
{
	enum Type
	{
		Local,
		Member,
		Parameter,
		/** Type and mane are separated by comma */
		MacroParameter, 
	};
}

//
// An UnrealScript variable.
//
class COREUOBJECT_API UProperty : public UField
{
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UProperty, UField, CLASS_Abstract, TEXT("/Script/CoreUObject"), CASTCLASS_UProperty, NO_API)
	DECLARE_WITHIN(UField)

	// Persistent variables.
	int32		ArrayDim;
	int32		ElementSize;
	uint64		PropertyFlags;
	uint16		RepIndex;

private:
	TEnumAsByte<ELifetimeCondition> BlueprintReplicationCondition;

	// In memory variables (generated during Link()).
	int32		Offset_Internal;

public:
	FName		RepNotifyFunc;

	/** In memory only: Linked list of properties from most-derived to base **/
	UProperty*	PropertyLinkNext;
	/** In memory only: Linked list of object reference properties from most-derived to base **/
	UProperty*  NextRef;
	/** In memory only: Linked list of properties requiring destruction. Note this does not include things that will be destroyed byt he native destructor **/
	UProperty*	DestructorLinkNext;
	/** In memory only: Linked list of properties requiring post constructor initialization.**/
	UProperty*	PostConstructLinkNext;

public:
	// Constructors.
	UProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UProperty(ECppProperty, int32 InOffset, uint64 InFlags);
	UProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags );

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	// End of UObject interface

	/** parses and imports a text definition of a single property's value (if array, may be an individual element)
	 * also includes parsing of special operations for array properties (Add/Remove/RemoveIndex/Empty)
	 * @param Str	the string to parse
	 * @param DestData	base location the parsed property should place its data (DestData + ParsedProperty->Offset)
	 * @param ObjectStruct	the struct containing the valid fields
	 * @param SubobjectOuter	owner of DestData and any subobjects within it
	 * @param PortFlags	property import flags
	 * @param Warn	output device for any error messages
	 * @param DefinedProperties (out)	list of properties/indices that have been parsed by previous calls, so duplicate definitions cause an error
	 * @return pointer to remaining text in the stream (even on failure, but on failure it may not be advanced past the entire key/value pair)
	 */
	static const TCHAR* ImportSingleProperty( const TCHAR* Str, void* DestData, class UStruct* ObjectStruct, UObject* SubobjectOuter, int32 PortFlags,
											FOutputDevice* Warn, TArray<struct FDefinedProperty>& DefinedProperties );

	/** Gets a redirected property name, will return NAME_None if no redirection was found */
	static FName FindRedirectedPropertyName(UStruct* ObjectStruct, FName OldName);

	// UHT interface
	void ExportCppDeclaration(FOutputDevice& Out, EExportedDeclaration::Type DeclarationType, const TCHAR* ArrayDimOverride = NULL, uint32 AdditionalExportCPPFlags = 0
		, bool bSkipParameterName = false, const FString* ActualCppType = nullptr, const FString* ActualExtendedType = nullptr, const FString* ActualParameterName = nullptr) const;
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual bool PassCPPArgsByRef() const;

	/**
	 * Returns the C++ name of the property, including the _DEPRECATED suffix if the 
	 * property is deprecated.
	 *
	 * @return C++ name of property
	 */
	FString GetNameCPP() const;

	/**
	 * Returns the text to use for exporting this property to header file.
	 *
	 * @param	ExtendedTypeText	for property types which use templates, will be filled in with the type
	 * @param	CPPExportFlags		flags for modifying the behavior of the export
	 */
	virtual FString GetCPPType( FString* ExtendedTypeText=NULL, uint32 CPPExportFlags=0 ) const PURE_VIRTUAL(UProperty::GetCPPType,return TEXT(""););

	virtual FString GetCPPTypeForwardDeclaration() const PURE_VIRTUAL(UProperty::GetCPPTypeForwardDeclaration, return TEXT(""););
	// End of UHT interface

private:
	/** Set the alignment offset for this property 
	 * @return the size of the structure including this newly added property
	*/
	int32 SetupOffset();

protected:
	friend class UMapProperty;

	/** Set the alignment offset for this property - added for UMapProperty */
	void SetOffset_Internal(int32 NewOffset);

	/**
	 * Initializes internal state.
	 */
	void Init();

public:
	/** Return offset of property from container base. */
	FORCEINLINE int32 GetOffset_ForDebug() const
	{
		return Offset_Internal;
	}
	/** Return offset of property from container base. */
	FORCEINLINE int32 GetOffset_ForUFunction() const
	{
		return Offset_Internal;
	}
	/** Return offset of property from container base. */
	FORCEINLINE int32 GetOffset_ForGC() const
	{
		return Offset_Internal;
	}
	/** Return offset of property from container base. */
	FORCEINLINE int32 GetOffset_ForInternal() const
	{
		return Offset_Internal;
	}
	/** Return offset of property from container base. */
	FORCEINLINE int32 GetOffset_ReplaceWith_ContainerPtrToValuePtr() const
	{
		return Offset_Internal;
	}

	void LinkWithoutChangingOffset(FArchive& Ar)
	{
		LinkInternal(Ar);
	}

	int32 Link(FArchive& Ar)
	{
		LinkInternal(Ar);
		return SetupOffset();
	}
protected:
	virtual void LinkInternal(FArchive& Ar);
public:

	/**
	* Allows a property to implement backwards compatibility handling for tagged properties
	* 
	* @param	Tag			property tag of the loading data
	* @param	Ar			the archive the data is being loaded from
	* @param	Data		
	* @param	DefaultsStruct 
	* @param	bOutAdvanceProperty whether the property should be advanced and continue to next property or not
	*
	* @return	true if the function has handled the tag. Returning true but assigning bOutAdvanceProperty false 
	*			indicates to calling code that it needs to manually advance the property. bOutAdvanceProperty true
	*			indicates that ConvertFromType itself has handled this property tag.
	*/
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty);

	/**
	 * Determines whether the property values are identical.
	 * 
	 * @param	A			property data to be compared, already offset
	 * @param	B			property data to be compared, already offset
	 * @param	PortFlags	allows caller more control over how the property values are compared
	 *
	 * @return	true if the property values are identical
	 */
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags=0 ) const PURE_VIRTUAL(UProperty::Identical,return false;);

	/**
	 * Determines whether the property values are identical.
	 * 
	 * @param	A			property container of data to be compared, NOT offset
	 * @param	B			property container of data to be compared, NOT offset
	 * @param	PortFlags	allows caller more control over how the property values are compared
	 *
	 * @return	true if the property values are identical
	 */
	bool Identical_InContainer( const void* A, const void* B, int32 ArrayIndex = 0, uint32 PortFlags=0 ) const
	{
		return Identical( ContainerPtrToValuePtr<void>(A, ArrayIndex), B ? ContainerPtrToValuePtr<void>(B, ArrayIndex) : NULL, PortFlags );
	}

	/**
	 * Serializes the property with the struct's data residing in Data.
	 *
	 * @param	Ar				the archive to use for serialization
	 * @param	Data			pointer to the location of the beginning of the struct's property data
	 * @param	ArrayIdx		if not -1 (default), only this array slot will be serialized
	 */
	void SerializeBinProperty( FArchive& Ar, void* Data, int32 ArrayIdx = -1 )
	{
		if( ShouldSerializeValue(Ar) )
		{
			FSerializedPropertyScope SerializedProperty(Ar, this);
			const int32 LoopMin = ArrayIdx < 0 ? 0 : ArrayIdx;
			const int32 LoopMax = ArrayIdx < 0 ? ArrayDim : ArrayIdx + 1;
			for (int32 Idx = LoopMin; Idx < LoopMax; Idx++)
			{
				// Keep setting the property in case something inside of SerializeItem changes it
				Ar.SetSerializedProperty(this);
				SerializeItem( Ar, ContainerPtrToValuePtr<void>(Data, Idx) );
			}
		}
	}
	/**
	 * Serializes the property with the struct's data residing in Data, unless it matches the default
	 *
	 * @param	Ar				the archive to use for serialization
	 * @param	Data			pointer to the location of the beginning of the struct's property data
	 * @param	DefaultData		pointer to the location of the beginning of the data that should be compared against
	 * @param	DefaultStruct	struct corresponding to the block of memory located at DefaultData 
	 */
	void SerializeNonMatchingBinProperty( FArchive& Ar, void* Data, void const* DefaultData, UStruct* DefaultStruct)
	{
		if( ShouldSerializeValue(Ar) )
		{
			for (int32 Idx = 0; Idx < ArrayDim; Idx++)
			{
				void *Target = ContainerPtrToValuePtr<void>(Data, Idx);
				void const* Default = ContainerPtrToValuePtrForDefaults<void>(DefaultStruct, DefaultData, Idx);
				if ( !Identical(Target, Default, Ar.GetPortFlags()) )
				{
					FSerializedPropertyScope SerializedProperty(Ar, this);
					SerializeItem( Ar, Target, Default );
				}
			}
		}
	}

	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults=NULL ) const PURE_VIRTUAL(UProperty::SerializeItem,);
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope = NULL ) const PURE_VIRTUAL(UProperty::ExportTextItem,);
	const TCHAR* ImportText( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText = (FOutputDevice*)GWarn ) const
	{
		if ( !ValidateImportFlags(PortFlags,ErrorText) || Buffer == NULL )
		{
			return NULL;
		}
		return ImportText_Internal( Buffer, Data, PortFlags, OwnerObject, ErrorText );
	}
protected:
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const PURE_VIRTUAL(UProperty::ImportText,return NULL;);
public:
	
	bool ExportText_Direct( FString& ValueStr, const void* Data, const void* Delta, UObject* Parent, int32 PortFlags, UObject* ExportRootScope = NULL ) const;
	FORCEINLINE bool ExportText_InContainer( int32 Index, FString& ValueStr, const void* Data, const void* Delta, UObject* Parent, int32 PortFlags, UObject* ExportRootScope = NULL ) const
	{
		return ExportText_Direct(ValueStr, ContainerPtrToValuePtr<void>(Data, Index), ContainerPtrToValuePtrForDefaults<void>(NULL, Delta, Index), Parent, PortFlags, ExportRootScope);
	}

private:

	FORCEINLINE void* ContainerVoidPtrToValuePtrInternal(void* ContainerPtr, int32 ArrayIndex) const
	{
		check(ArrayIndex < ArrayDim);
		check(ContainerPtr);

		if (0)
		{
			// in the future, these checks will be tested if the property is NOT relative to a UClass
			check(!Cast<UClass>(GetOuter())); // Check we are _not_ calling this on a direct child property of a UClass, you should pass in a UObject* in that case
		}

		return (uint8*)ContainerPtr + Offset_Internal + ElementSize * ArrayIndex;
	}

	FORCEINLINE void* ContainerUObjectPtrToValuePtrInternal(UObject* ContainerPtr, int32 ArrayIndex) const
	{
		check(ArrayIndex < ArrayDim);
		check(ContainerPtr);

		// in the future, these checks will be tested if the property is supposed be from a UClass
		// need something for networking, since those are NOT live uobjects, just memory blocks
		check(((UObject*)ContainerPtr)->IsValidLowLevel()); // Check its a valid UObject that was passed in
		check(((UObject*)ContainerPtr)->GetClass() != NULL);
		check(GetOuter()->IsA(UClass::StaticClass())); // Check that the outer of this property is a UClass (not another property)

		// Check that the object we are accessing is of the class that contains this property
		checkf(((UObject*)ContainerPtr)->IsA((UClass*)GetOuter()), TEXT("'%s' is of class '%s' however property '%s' belongs to class '%s'")
			, *((UObject*)ContainerPtr)->GetName()
			, *((UObject*)ContainerPtr)->GetClass()->GetName()
			, *GetName()
			, *((UClass*)GetOuter())->GetName());

		if (0)
		{
			// in the future, these checks will be tested if the property is NOT relative to a UClass
			check(!GetOuter()->IsA(UClass::StaticClass())); // Check we are _not_ calling this on a direct child property of a UClass, you should pass in a UObject* in that case
		}

		return (uint8*)ContainerPtr + Offset_Internal + ElementSize * ArrayIndex;
	}

public:

	/** 
	 *	Get the pointer to property value in a supplied 'container'. 
	 *	You can _only_ call this function on a UObject* or a uint8*. If the property you want is a 'top level' UObject property, you _must_
	 *	call the function passing in a UObject* and not a uint8*. There are checks inside the function to vertify this.
	 *	@param	ContainerPtr			UObject* or uint8* to container of property value
	 *	@param	ArrayIndex				In array case, index of array element we want
	 */
	template<typename ValueType>
	FORCEINLINE ValueType* ContainerPtrToValuePtr(UObject* ContainerPtr, int32 ArrayIndex = 0) const
	{
		return (ValueType*)ContainerUObjectPtrToValuePtrInternal(ContainerPtr, ArrayIndex);
	}
	template<typename ValueType>
	FORCEINLINE ValueType* ContainerPtrToValuePtr(void* ContainerPtr, int32 ArrayIndex = 0) const
	{
		return (ValueType*)ContainerVoidPtrToValuePtrInternal(ContainerPtr, ArrayIndex);
	}
	template<typename ValueType>
	FORCEINLINE ValueType const* ContainerPtrToValuePtr(UObject const* ContainerPtr, int32 ArrayIndex = 0) const
	{
		return ContainerPtrToValuePtr<ValueType>((UObject*)ContainerPtr, ArrayIndex);
	}
	template<typename ValueType>
	FORCEINLINE ValueType const* ContainerPtrToValuePtr(void const* ContainerPtr, int32 ArrayIndex = 0) const
	{
		return ContainerPtrToValuePtr<ValueType>((void*)ContainerPtr, ArrayIndex);
	}

	// Default variants, these accept and return NULL, and also check the property against the size of the container. 
	// If we copying from a baseclass (like for a CDO), then this will give NULL for a property that doesn't belong to the baseclass
	template<typename ValueType>
	FORCEINLINE ValueType* ContainerPtrToValuePtrForDefaults(UStruct* ContainerClass, UObject* ContainerPtr, int32 ArrayIndex = 0) const
	{
		if (ContainerPtr && IsInContainer(ContainerClass))
		{
			return ContainerPtrToValuePtr<ValueType>(ContainerPtr, ArrayIndex);
		}
		return NULL;
	}
	template<typename ValueType>
	FORCEINLINE ValueType* ContainerPtrToValuePtrForDefaults(UStruct* ContainerClass, void* ContainerPtr, int32 ArrayIndex = 0) const
	{
		if (ContainerPtr && IsInContainer(ContainerClass))
		{
			return ContainerPtrToValuePtr<ValueType>(ContainerPtr, ArrayIndex);
		}
		return NULL;
	}
	template<typename ValueType>
	FORCEINLINE ValueType const* ContainerPtrToValuePtrForDefaults(UStruct* ContainerClass, UObject const* ContainerPtr, int32 ArrayIndex = 0) const
	{
		if (ContainerPtr && IsInContainer(ContainerClass))
		{
			return ContainerPtrToValuePtr<ValueType>(ContainerPtr, ArrayIndex);
		}
		return NULL;
	}
	template<typename ValueType>
	FORCEINLINE ValueType const* ContainerPtrToValuePtrForDefaults(UStruct* ContainerClass, void const* ContainerPtr, int32 ArrayIndex = 0) const
	{
		if (ContainerPtr && IsInContainer(ContainerClass))
		{
			return ContainerPtrToValuePtr<ValueType>(ContainerPtr, ArrayIndex);
		}
		return NULL;
	}
	/** See if the offset of this property is below the supplied container size */
	FORCEINLINE bool IsInContainer(int32 ContainerSize) const
	{
		return Offset_Internal + GetSize() <= ContainerSize;
	}
	/** See if the offset of this property is below the supplied container size */
	FORCEINLINE bool IsInContainer(UStruct* ContainerClass) const
	{
		return Offset_Internal + GetSize() <= (ContainerClass ? ContainerClass->GetPropertiesSize() : MAX_int32);
	}

	/**
	 * Copy the value for a single element of this property.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 * @param	InstancingParams	contains information about instancing (if any) to perform
	 */
	FORCEINLINE void CopySingleValue( void* Dest, void const* Src ) const
	{
		if(Dest != Src)
		{
			if (PropertyFlags & CPF_IsPlainOldData)
			{
				FMemory::Memcpy( Dest, Src, ElementSize );
			}
			else
			{
				CopyValuesInternal(Dest, Src, 1);
			}
		}
	}

	/**
	 * Returns the hash value for an element of this property.
	 */
	uint32 GetValueTypeHash(const void* Src) const;

protected:
	virtual void CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const;
	virtual uint32 GetValueTypeHashInternal(const void* Src) const;

public:
	/**
	 * Copy the value for all elements of this property.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 * @param	InstancingParams	contains information about instancing (if any) to perform
	 */
	FORCEINLINE void CopyCompleteValue( void* Dest, void const* Src ) const
	{
		if(Dest != Src)
		{
			if (PropertyFlags & CPF_IsPlainOldData)
			{
				FMemory::Memcpy( Dest, Src, ElementSize * ArrayDim );
			}
			else
			{
				CopyValuesInternal(Dest, Src, ArrayDim);
			}
		}
	}
	FORCEINLINE void CopyCompleteValue_InContainer( void* Dest, void const* Src ) const
	{
		return CopyCompleteValue(ContainerPtrToValuePtr<void>(Dest), ContainerPtrToValuePtr<void>(Src));
	}

	/**
	 * Copy the value for a single element of this property. To the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopySingleValueToScriptVM( void* Dest, void const* Src ) const;

	/**
	 * Copy the value for all elements of this property. To the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopyCompleteValueToScriptVM( void* Dest, void const* Src ) const;

	/**
	 * Copy the value for a single element of this property. From the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopySingleValueFromScriptVM( void* Dest, void const* Src ) const;

	/**
	 * Copy the value for all elements of this property. From the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopyCompleteValueFromScriptVM( void* Dest, void const* Src ) const;

	/**
	 * Zeros the value for this property. The existing data is assumed valid (so for example this calls FString::Empty)
	 * This only does one item and not the entire fixed size array.
	 *
	 * @param	Data		the address of the value for this property that should be cleared.
	 */
	FORCEINLINE void ClearValue( void* Data ) const
	{
		if (HasAllPropertyFlags(CPF_NoDestructor | CPF_ZeroConstructor))
		{
			FMemory::Memzero( Data, ElementSize );
		}
		else
		{
			ClearValueInternal((uint8*)Data);
		}
	}
	/**
	 * Zeros the value for this property. The existing data is assumed valid (so for example this calls FString::Empty)
	 * This only does one item and not the entire fixed size array.
	 *
	 * @param	Data		the address of the container of the value for this property that should be cleared.
	 */
	FORCEINLINE void ClearValue_InContainer( void* Data, int32 ArrayIndex = 0 ) const
	{
		if (HasAllPropertyFlags(CPF_NoDestructor | CPF_ZeroConstructor))
		{
			FMemory::Memzero( ContainerPtrToValuePtr<void>(Data, ArrayIndex), ElementSize );
		}
		else
		{
			ClearValueInternal(ContainerPtrToValuePtr<uint8>(Data, ArrayIndex));
		}
	}
protected:
	virtual void ClearValueInternal( void* Data ) const;
public:
	/**
	 * Destroys the value for this property. The existing data is assumed valid (so for example this calls FString::Empty)
	 * This does the entire fixed size array.
	 *
	 * @param	Dest		the address of the value for this property that should be destroyed.
	 */
	FORCEINLINE void DestroyValue( void* Dest ) const
	{
		if (!(PropertyFlags & CPF_NoDestructor))
		{
			DestroyValueInternal(Dest);
		}
	}
	/**
	 * Destroys the value for this property. The existing data is assumed valid (so for example this calls FString::Empty)
	 * This does the entire fixed size array.
	 *
	 * @param	Dest		the address of the container containing the value that should be destroyed.
	 */
	FORCEINLINE void DestroyValue_InContainer( void* Dest ) const
	{
		if (!(PropertyFlags & CPF_NoDestructor))
		{
			DestroyValueInternal(ContainerPtrToValuePtr<void>(Dest));
		}
	}
protected:
	virtual void DestroyValueInternal( void* Dest ) const;
public:

	/**
	 * Zeros, copies from the default, or calls the constructor for on the value for this property. 
	 * The existing data is assumed invalid (so for example this might indirectly call FString::FString,
	 * This will do the entire fixed size array.
	 *
	 * @param	Dest		the address of the value for this property that should be cleared.
	 */
	FORCEINLINE void InitializeValue( void* Dest ) const
	{
		if (PropertyFlags & CPF_ZeroConstructor)
		{
			FMemory::Memzero(Dest,ElementSize * ArrayDim);
		}
		else
		{
			InitializeValueInternal(Dest);
		}
	}
	/**
	 * Zeros, copies from the default, or calls the constructor for on the value for this property. 
	 * The existing data is assumed invalid (so for example this might indirectly call FString::FString,
	 * This will do the entire fixed size array.
	 *
	 * @param	Dest		the address of the container of value for this property that should be cleared.
	 */
	FORCEINLINE void InitializeValue_InContainer( void* Dest ) const
	{
		if (PropertyFlags & CPF_ZeroConstructor)
		{
			FMemory::Memzero(ContainerPtrToValuePtr<void>(Dest),ElementSize * ArrayDim);
		}
		else
		{
			InitializeValueInternal(ContainerPtrToValuePtr<void>(Dest));
		}
	}
protected:
	virtual void InitializeValueInternal( void* Dest ) const;
public:

	/**
	 * Verify that modifying this property's value via ImportText is allowed.
	 * 
	 * @param	PortFlags	the flags specified in the call to ImportText
	 * @param	ErrorText	[out] set to the error message that should be displayed if returns false
	 *
	 * @return	true if ImportText should be allowed
	 */
	bool ValidateImportFlags( uint32 PortFlags, FOutputDevice* ErrorText = NULL ) const;
	bool ShouldPort( uint32 PortFlags=0 ) const;
	virtual FName GetID() const;

	/**
	 * Creates new copies of components
	 * 
	 * @param	Data				pointer to the address of the instanced object referenced by this UComponentProperty
	 * @param	DefaultData			pointer to the address of the default value of the instanced object referenced by this UComponentProperty
	 * @param	Owner				the object that contains this property's data
	 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
	 */
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph );

	virtual int32 GetMinAlignment() const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference.
	 * @param	EncounteredStructProps		used to check for recursion in arrays
	 *
	 * @return true if property (or sub- properties) contain a UObject reference, false otherwise
	 */
	virtual bool ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * weak UObject reference.
	 *
	 * @return true if property (or sub- properties) contain a weak UObject reference, false otherwise
	 */
	virtual bool ContainsWeakObjectReference() const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
	 *
	 * @return true if property (or sub- properties) contain a UObjectProperty that is marked CPF_NeedCtorLink, false otherwise
	 */
	FORCEINLINE bool ContainsInstancedObjectProperty() const
	{
		return (PropertyFlags&(CPF_ContainsInstancedReference | CPF_InstancedReference)) != 0;
	}

	/**
	 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
	 * to the passed in BaseOffset which is used by e.g. arrays of structs.
	 */
	virtual void EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps);

	FORCEINLINE int32 GetSize() const
	{
		return ArrayDim * ElementSize;
	}
	bool ShouldSerializeValue( FArchive& Ar ) const;

	/**
	 * Determines whether this property value is eligible for copying when duplicating an object
	 * 
	 * @return	true if this property value should be copied into the duplicate object
	 */
	bool ShouldDuplicateValue() const
	{
		return ShouldPort() && GetOwnerClass() != UObject::StaticClass();
	}

	/**
	 * Returns the first UProperty in this property's Outer chain that does not have a UProperty for an Outer
	 */
	UProperty* GetOwnerProperty()
	{
		UProperty* Result=this;
		for (UProperty* PropBase = dynamic_cast<UProperty*>(GetOuter()); PropBase; PropBase = dynamic_cast<UProperty*>(PropBase->GetOuter()))
		{
			Result = PropBase;
		}
		return Result;
	}

	const UProperty* GetOwnerProperty() const
	{
		const UProperty* Result = this;
		for (UProperty* PropBase = dynamic_cast<UProperty*>(GetOuter()); PropBase; PropBase = dynamic_cast<UProperty*>(PropBase->GetOuter()))
		{
			Result = PropBase;
		}
		return Result;
	}

	/**
	 * Returns this property's propertyflags
	 */
	FORCEINLINE uint64 GetPropertyFlags() const
	{
		return PropertyFlags;
	}
	FORCEINLINE void SetPropertyFlags( uint64 NewFlags )
	{
		PropertyFlags |= NewFlags;
	}
	FORCEINLINE void ClearPropertyFlags( uint64 NewFlags )
	{
		PropertyFlags &= ~NewFlags;
	}
	/**
	 * Used to safely check whether any of the passed in flags are set. This is required
	 * as PropertyFlags currently is a 64 bit data type and bool is a 32 bit data type so
	 * simply using PropertyFlags&CPF_MyFlagBiggerThanMaxInt won't work correctly when
	 * assigned directly to an bool.
	 *
	 * @param FlagsToCheck	Object flags to check for.
	 *
	 * @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	 */
	FORCEINLINE bool HasAnyPropertyFlags( uint64 FlagsToCheck ) const
	{
		return (PropertyFlags & FlagsToCheck) != 0 || FlagsToCheck == CPF_AllFlags;
	}
	/**
	 * Used to safely check whether all of the passed in flags are set. This is required
	 * as PropertyFlags currently is a 64 bit data type and bool is a 32 bit data type so
	 * simply using PropertyFlags&CPF_MyFlagBiggerThanMaxInt won't work correctly when
	 * assigned directly to an bool.
	 *
	 * @param FlagsToCheck	Object flags to check for
	 *
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	FORCEINLINE bool HasAllPropertyFlags( uint64 FlagsToCheck ) const
	{
		return ((PropertyFlags & FlagsToCheck) == FlagsToCheck);
	}

	/**
	 * Returns the replication owner, which is the property itself, or NULL if this isn't important for replication.
	 * It is relevant if the property is a net relevant and not being run in the editor
	 */
	FORCEINLINE UProperty* GetRepOwner()
	{
		return (!GIsEditor && ((PropertyFlags & CPF_Net) != 0)) ? this : NULL;
	}

	/**
	 * Editor-only properties are those that only are used with the editor is present or cannot be removed from serialisation.
	 * Editor-only properties include: EditorOnly properties
	 * Properties that cannot be removed from serialisation are:
	 *		Boolean properties (may affect GCC_BITFIELD_MAGIC computation)
	 *		Native properties (native serialisation)
	 */
	FORCEINLINE bool IsEditorOnlyProperty() const
	{
		return (PropertyFlags & CPF_DevelopmentAssets) != 0;
	}

	/** returns true, if Other is property of exactly the same type */
	virtual bool SameType(const UProperty* Other) const;

	ELifetimeCondition GetBlueprintReplicationCondition() const { return BlueprintReplicationCondition; }
	void SetBlueprintReplicationCondition(ELifetimeCondition InBlueprintReplicationCondition) { BlueprintReplicationCondition = InBlueprintReplicationCondition; }

#if HACK_HEADER_GENERATOR
	// Required by UHT makefiles for internal data serialization.
	friend struct FPropertyArchiveProxy;
#endif // HACK_HEADER_GENERATOR
};


class COREUOBJECT_API UPropertyHelpers
{
public:

	static const TCHAR* ReadToken( const TCHAR* Buffer, FString& String, bool DottedNames = 0 );

};


/** reference to a property and optional array index used in property text import to detect duplicate references */
struct COREUOBJECT_API FDefinedProperty
{
    UProperty* Property;
    int32 Index;
    bool operator== (const FDefinedProperty& Other) const
    {
        return (Property == Other.Property && Index == Other.Index);
    }
};

/**
 * Creates a temporary object that represents the default constructed value of a UProperty
 */
class COREUOBJECT_API FDefaultConstructedPropertyElement
{
public:
	explicit FDefaultConstructedPropertyElement(UProperty* InProp)
		: Prop(InProp)
		, Obj(FMemory::Malloc(InProp->GetSize(), InProp->GetMinAlignment()))
	{
		InProp->InitializeValue(Obj);
	}

	~FDefaultConstructedPropertyElement()
	{
		Prop->DestroyValue(Obj);
		FMemory::Free(Obj);
	}

	void* GetObjAddress() const
	{
		return Obj;
	}

private:
	// Non-copyable
	FDefaultConstructedPropertyElement(const FDefaultConstructedPropertyElement&) = delete;
	FDefaultConstructedPropertyElement& operator=(const FDefaultConstructedPropertyElement&) = delete;

private:
	UProperty* Prop;
	void* Obj;
};


/*-----------------------------------------------------------------------------
	TProperty.
-----------------------------------------------------------------------------*/


template<typename InTCppType>
class TPropertyTypeFundamentals
{
public:
	/** Type of the CPP property **/
	typedef InTCppType TCppType;
	enum
	{
		CPPSize = sizeof(TCppType),
		CPPAlignment = alignof(TCppType)
	};

	static FORCEINLINE TCHAR const* GetTypeName()
	{
		return TNameOf<TCppType>::GetName();
	}

	/** Convert the address of a value of the property to the proper type */
	static FORCEINLINE TCppType const* GetPropertyValuePtr(void const* A)
	{
		return (TCppType const*)A;
	}
	/** Convert the address of a value of the property to the proper type */
	static FORCEINLINE TCppType* GetPropertyValuePtr(void* A)
	{
		return (TCppType*)A;
	}
	/** Get the value of the property from an address */
	static FORCEINLINE TCppType const& GetPropertyValue(void const* A)
	{
		return *GetPropertyValuePtr(A);
	}
	/** Get the default value of the cpp type, just the default constructor, which works even for things like in32 */
	static FORCEINLINE TCppType GetDefaultPropertyValue()
	{
		return TCppType();
	}
	/** Get the value of the property from an address, unless it is NULL, then return the default value */
	static FORCEINLINE TCppType GetOptionalPropertyValue(void const* B)
	{
		return B ? GetPropertyValue(B) : GetDefaultPropertyValue();
	}
	/** Set the value of a property at an address */
	static FORCEINLINE void SetPropertyValue(void* A, TCppType const& Value)
	{
		*GetPropertyValuePtr(A) = Value;
	}
	/** Initialize the value of a property at an address, this assumes over uninitialized memory */
	static FORCEINLINE TCppType* InitializePropertyValue(void* A)
	{
		return new (A) TCppType();
	}
	/** Destroy the value of a property at an address */
	static FORCEINLINE void DestroyPropertyValue(void* A)
	{
		GetPropertyValuePtr(A)->~TCppType();
	}

protected:
	/** Get the property flags corresponding to this C++ type, from the C++ type traits system */
	static FORCEINLINE uint64 GetComputedFlagsPropertyFlags()
	{
		return 
			(TIsPODType<TCppType>::Value ? CPF_IsPlainOldData : 0) 
			| (TIsTriviallyDestructible<TCppType>::Value ? CPF_NoDestructor : 0) 
			| (TIsZeroConstructType<TCppType>::Value ? CPF_ZeroConstructor : 0)
			| (THasGetTypeHash<TCppType>::Value ? CPF_HasGetValueTypeHash : 0);

	}
};


template<typename InTCppType, class TInPropertyBaseClass>
class TProperty : public TInPropertyBaseClass, public TPropertyTypeFundamentals<InTCppType>
{
public:

	typedef InTCppType TCppType;
	typedef TInPropertyBaseClass Super;
	typedef TPropertyTypeFundamentals<InTCppType> TTypeFundamentals;

	TProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
	{
		SetElementSize();
	}
	TProperty(ECppProperty, int32 InOffset, uint64 InFlags)
		: Super(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags | TTypeFundamentals::GetComputedFlagsPropertyFlags())
	{
		SetElementSize();
	}
	TProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
		:	Super( ObjectInitializer, EC_CppProperty, InOffset, InFlags | TTypeFundamentals::GetComputedFlagsPropertyFlags())
	{
		SetElementSize();
	}

	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	TProperty(FVTableHelper& Helper) : Super(Helper) {};

	// UHT interface
	virtual FString GetCPPType( FString* ExtendedTypeText=NULL, uint32 CPPExportFlags=0 ) const override
	{
		return FString(TTypeFundamentals::GetTypeName());
	}
	virtual bool PassCPPArgsByRef() const override
	{
		// non-pod data is passed by reference
		return !TIsPODType<TCppType>::Value;
	}
	// End of UHT interface

	// UProperty interface.
	virtual int32 GetMinAlignment() const override
	{
		return TTypeFundamentals::CPPAlignment;
	}
	virtual void LinkInternal(FArchive& Ar) override
	{
		SetElementSize();
		this->PropertyFlags |= TTypeFundamentals::GetComputedFlagsPropertyFlags();

	}
	virtual void CopyValuesInternal( void* Dest, void const* Src, int32 Count ) const override
	{
		for (int32 Index = 0; Index < Count; Index++)
		{
			TTypeFundamentals::GetPropertyValuePtr(Dest)[Index] = TTypeFundamentals::GetPropertyValuePtr(Src)[Index];
		}
	}
	virtual void ClearValueInternal( void* Data ) const override
	{
		TTypeFundamentals::SetPropertyValue(Data, TTypeFundamentals::GetDefaultPropertyValue());
	}
	virtual void InitializeValueInternal( void* Dest ) const override
	{
		for (int32 i = 0; i < this->ArrayDim; ++i)
		{
			TTypeFundamentals::InitializePropertyValue((uint8*)Dest + i * this->ElementSize);
		}
	}
	virtual void DestroyValueInternal( void* Dest ) const override
	{
		for (int32 i = 0; i < this->ArrayDim; ++i)
		{
			TTypeFundamentals::DestroyPropertyValue((uint8*)Dest + i * this->ElementSize);
		}
	}

	/** Convert the address of a container to the address of the property value, in the proper type */
	FORCEINLINE TCppType const* GetPropertyValuePtr_InContainer(void const* A, int32 ArrayIndex = 0) const
	{
		return TTypeFundamentals::GetPropertyValuePtr(Super::template ContainerPtrToValuePtr<void>(A, ArrayIndex));
	}
	/** Convert the address of a container to the address of the property value, in the proper type */
	FORCEINLINE TCppType* GetPropertyValuePtr_InContainer(void* A, int32 ArrayIndex = 0) const
	{
		return TTypeFundamentals::GetPropertyValuePtr(Super::template ContainerPtrToValuePtr<void>(A, ArrayIndex));
	}
	/** Get the value of the property from a container address */
	FORCEINLINE TCppType const& GetPropertyValue_InContainer(void const* A, int32 ArrayIndex = 0) const
	{
		return *GetPropertyValuePtr_InContainer(A, ArrayIndex);
	}
	/** Get the value of the property from a container address, unless it is NULL, then return the default value */
	FORCEINLINE TCppType GetOptionalPropertyValue_InContainer(void const* B, int32 ArrayIndex = 0) const
	{
		return B ? GetPropertyValue_InContainer(B, ArrayIndex) : TTypeFundamentals::GetDefaultPropertyValue();
	}
	/** Set the value of a property in a container */
	FORCEINLINE void SetPropertyValue_InContainer(void* A, TCppType const& Value, int32 ArrayIndex = 0) const
	{
		*GetPropertyValuePtr_InContainer(A, ArrayIndex) = Value;
	}

protected:
	FORCEINLINE void SetElementSize()
	{
		this->ElementSize = TTypeFundamentals::CPPSize;
	}
	// End of UProperty interface
};

template<typename InTCppType, class TInPropertyBaseClass>
class COREUOBJECT_API TProperty_WithEqualityAndSerializer : public TProperty<InTCppType, TInPropertyBaseClass>
{

public:
	typedef TProperty<InTCppType, TInPropertyBaseClass> Super;
	typedef InTCppType TCppType;
	typedef typename Super::TTypeFundamentals TTypeFundamentals;

	TProperty_WithEqualityAndSerializer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
	{
	}
	TProperty_WithEqualityAndSerializer(ECppProperty, int32 InOffset, uint64 InFlags)
		: Super(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}
	TProperty_WithEqualityAndSerializer( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
		:	Super( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	{
	}

	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	TProperty_WithEqualityAndSerializer(FVTableHelper& Helper) : Super(Helper) {};

	// UProperty interface.
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags=0 ) const override
	{
		return TTypeFundamentals::GetPropertyValue(A) == TTypeFundamentals::GetOptionalPropertyValue(B);
	}
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override
	{
		Ar << *TTypeFundamentals::GetPropertyValuePtr(Value);
	}
	// End of UProperty interface

};

class COREUOBJECT_API UNumericProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UNumericProperty, UProperty, CLASS_Abstract, TEXT("/Script/CoreUObject"), CASTCLASS_UNumericProperty)

	UNumericProperty(ECppProperty, int32 InOffset, uint64 InFlags)
		: UProperty(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{}

	UNumericProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
		:	UProperty( ObjectInitializer, EC_CppProperty, InOffset, InFlags )
	{}

	// UProperty interface.
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const override;

	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	// End of UProperty interface

	// UNumericProperty interface.

	/** Return true if this property is for a floating point number **/
	virtual bool IsFloatingPoint() const;

	/** Return true if this property is for a integral or enum type **/
	virtual bool IsInteger() const;

	template <typename T>
	bool CanHoldValue(T Value) const
	{
		if (!TIsFloatingPoint<T>::Value)
		{
			return CanHoldDoubleValueInternal(Value);
		}
		else if (TIsSigned<T>::Value)
		{
			return CanHoldSignedValueInternal(Value);
		}
		else
		{
			return CanHoldUnsignedValueInternal(Value);
		}
	}

	/** Return true if this property is a UByteProperty with a non-null Enum **/
	FORCEINLINE bool IsEnum() const
	{
		return !!GetIntPropertyEnum();
	}

	/** Return the UEnum if this property is a UByteProperty with a non-null Enum **/
	virtual UEnum* GetIntPropertyEnum() const;

	/** 
	 * Set the value of an unsigned integral property type
	 * @param Data - pointer to property data to set
	 * @param Value - Value to set data to
	**/
	virtual void SetIntPropertyValue(void* Data, uint64 Value) const;

	/** 
	 * Set the value of a signed integral property type
	 * @param Data - pointer to property data to set
	 * @param Value - Value to set data to
	**/
	virtual void SetIntPropertyValue(void* Data, int64 Value) const;

	/** 
	 * Set the value of a floating point property type
	 * @param Data - pointer to property data to set
	 * @param Value - Value to set data to
	**/
	virtual void SetFloatingPointPropertyValue(void* Data, double Value) const;

	/** 
	 * Set the value of any numeric type from a string point
	 * @param Data - pointer to property data to set
	 * @param Value - Value (as a string) to set 
	 * CAUTION: This routine does not do enum name conversion
	**/
	virtual void SetNumericPropertyValueFromString(void* Data, TCHAR const* Value) const;

	/** 
	 * Gets the value of a signed integral property type
	 * @param Data - pointer to property data to get
	 * @return Data as a signed int
	**/
	virtual int64 GetSignedIntPropertyValue(void const* Data) const;

	/** 
	 * Gets the value of an unsigned integral property type
	 * @param Data - pointer to property data to get
	 * @return Data as an unsigned int
	**/
	virtual uint64 GetUnsignedIntPropertyValue(void const* Data) const;

	/** 
	 * Gets the value of an floating point property type
	 * @param Data - pointer to property data to get
	 * @return Data as a double
	**/
	virtual double GetFloatingPointPropertyValue(void const* Data) const;

	/** 
	 * Get the value of any numeric type and return it as a string
	 * @param Data - pointer to property data to get
	 * @return Data as a string
	 * CAUTION: This routine does not do enum name conversion
	**/
	virtual FString GetNumericPropertyValueToString(void const* Data) const;
	// End of UNumericProperty interface

	static int64 ReadEnumAsInt64(FArchive& Ar, UStruct* DefaultsStruct, const FPropertyTag& Tag);

private:
	virtual bool CanHoldDoubleValueInternal  (double Value) const PURE_VIRTUAL(UNumericProperty::CanHoldDoubleValueInternal,   return false;);
	virtual bool CanHoldSignedValueInternal  (int64  Value) const PURE_VIRTUAL(UNumericProperty::CanHoldSignedValueInternal,   return false;);
	virtual bool CanHoldUnsignedValueInternal(uint64 Value) const PURE_VIRTUAL(UNumericProperty::CanHoldUnsignedValueInternal, return false;);
};

template<typename InTCppType>
class COREUOBJECT_API TProperty_Numeric : public TProperty_WithEqualityAndSerializer<InTCppType, UNumericProperty>
{

public:
	typedef TProperty_WithEqualityAndSerializer<InTCppType, UNumericProperty> Super;
	typedef InTCppType TCppType;
	typedef typename Super::TTypeFundamentals TTypeFundamentals;
	TProperty_Numeric(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
	{
	}

	TProperty_Numeric(ECppProperty, int32 InOffset, uint64 InFlags)
		: Super(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	TProperty_Numeric( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
		:	Super( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	{
	}

	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	TProperty_Numeric(FVTableHelper& Helper) : Super(Helper) {};

	virtual FString GetCPPTypeForwardDeclaration() const override
	{
		return FString();
	}

	// UProperty interface
	virtual uint32 GetValueTypeHashInternal(const void* Src) const override
	{
		return GetTypeHash(*(const InTCppType*)Src);
	}

protected:
	template <typename OldNumericType>
	void ConvertFromArithmeticValue(FArchive& Ar, void* Obj, const FPropertyTag& Tag)
	{
		OldNumericType OldValue;
		Ar << OldValue;
		TCppType NewValue = (TCppType)OldValue;
		this->SetPropertyValue_InContainer(Obj, NewValue, Tag.ArrayIndex);

		UE_CLOG(
			((TIsSigned<OldNumericType>::Value || TIsFloatingPoint<OldNumericType>::Value) && (!TIsSigned<TCppType>::Value && !TIsFloatingPoint<TCppType>::Value) && OldValue < 0) || ((OldNumericType)NewValue != OldValue),
			LogClass,
			Warning,
			TEXT("Potential data loss during conversion of integer property %s of %s - was (%s) now (%s) - for package: %s"),
			*this->GetName(),
			*Ar.GetArchiveName(),
			*Lex::ToString(OldValue),
			*Lex::ToString(NewValue),
			*Ar.GetArchiveName()
			);
	}

public:
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override
	{
		// Assume we're going to convert it successfully
		bOutAdvanceProperty = true;

		if (Tag.Type == NAME_Int8Property)
		{
			ConvertFromArithmeticValue<int8>(Ar, Data, Tag);
		}
		else if (Tag.Type == NAME_Int16Property)
		{
			ConvertFromArithmeticValue<int16>(Ar, Data, Tag);
		}
		else if (Tag.Type == NAME_IntProperty)
		{
			ConvertFromArithmeticValue<int32>(Ar, Data, Tag);
		}
		else if (Tag.Type == NAME_Int64Property)
		{
			ConvertFromArithmeticValue<int64>(Ar, Data, Tag);
		}
		else if (Tag.Type == NAME_ByteProperty)
		{
			if (Tag.EnumName != NAME_None)
			{
				int64 PreviousValue = this->ReadEnumAsInt64(Ar, DefaultsStruct, Tag);
				this->SetPropertyValue_InContainer(Data, PreviousValue, Tag.ArrayIndex);
			}
			else
			{
				ConvertFromArithmeticValue<int8>(Ar, Data, Tag);
			}
		}
		else if (Tag.Type == NAME_EnumProperty)
		{
			int64 PreviousValue = this->ReadEnumAsInt64(Ar, DefaultsStruct, Tag);
			this->SetPropertyValue_InContainer(Data, (TCppType)PreviousValue, Tag.ArrayIndex);
		}
		else if (Tag.Type == NAME_UInt16Property)
		{
			ConvertFromArithmeticValue<uint16>(Ar, Data, Tag);
		}
		else if (Tag.Type == NAME_UInt32Property)
		{
			ConvertFromArithmeticValue<uint32>(Ar, Data, Tag);
		}
		else if (Tag.Type == NAME_UInt64Property)
		{
			ConvertFromArithmeticValue<uint64>(Ar, Data, Tag);
		}
		else if (Tag.Type == NAME_FloatProperty)
		{
			ConvertFromArithmeticValue<float>(Ar, Data, Tag);
		}
		else if (Tag.Type == NAME_DoubleProperty)
		{
			ConvertFromArithmeticValue<double>(Ar, Data, Tag);
		}
		else
		{
			// We didn't convert it
			bOutAdvanceProperty = false;
		}

		return bOutAdvanceProperty;
	}
	// End of UProperty interface

	// UNumericProperty interface.
	virtual bool IsFloatingPoint() const override
	{
		return TIsFloatingPoint<TCppType>::Value;
	}
	virtual bool IsInteger() const override
	{
		return TIsIntegral<TCppType>::Value;
	}
	virtual void SetIntPropertyValue(void* Data, uint64 Value) const override
	{
		check(TIsIntegral<TCppType>::Value);
		TTypeFundamentals::SetPropertyValue(Data, Value);
	}
	virtual void SetIntPropertyValue(void* Data, int64 Value) const override
	{
		check(TIsIntegral<TCppType>::Value);
		TTypeFundamentals::SetPropertyValue(Data, Value);
	}
	virtual void SetFloatingPointPropertyValue(void* Data, double Value) const override
	{
		check(TIsFloatingPoint<TCppType>::Value);
		TTypeFundamentals::SetPropertyValue(Data, Value);
	}
	virtual void SetNumericPropertyValueFromString(void* Data, TCHAR const* Value) const override
	{
		Lex::FromString(*TTypeFundamentals::GetPropertyValuePtr(Data), Value);
	}
	virtual FString GetNumericPropertyValueToString(void const* Data) const override
	{
		return Lex::ToString(TTypeFundamentals::GetPropertyValue(Data));
	}
	virtual int64 GetSignedIntPropertyValue(void const* Data) const override
	{
		check(TIsIntegral<TCppType>::Value);
		return TTypeFundamentals::GetPropertyValue(Data);
	}
	virtual uint64 GetUnsignedIntPropertyValue(void const* Data) const override
	{
		check(TIsIntegral<TCppType>::Value);
		return TTypeFundamentals::GetPropertyValue(Data);
	}
	virtual double GetFloatingPointPropertyValue(void const* Data) const override
	{
		check(TIsFloatingPoint<TCppType>::Value);
		return TTypeFundamentals::GetPropertyValue(Data);
	}
	// End of UNumericProperty interface

private:
	virtual bool CanHoldDoubleValueInternal(double Value) const
	{
		return (double)(InTCppType)Value == Value;
	}

	virtual bool CanHoldSignedValueInternal(int64 Value) const
	{
		return (int64)(InTCppType)Value == Value;
	}

	virtual bool CanHoldUnsignedValueInternal(uint64 Value) const
	{
		return (uint64)(InTCppType)Value == Value;
	}
};

/*-----------------------------------------------------------------------------
	UByteProperty.
-----------------------------------------------------------------------------*/

//
// Describes an unsigned byte value or 255-value enumeration variable.
//
class COREUOBJECT_API UByteProperty : public TProperty_Numeric<uint8>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UByteProperty, TProperty_Numeric<uint8>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UByteProperty)

	// Variables.
	UEnum* Enum;

	UByteProperty(ECppProperty, int32 InOffset, uint64 InFlags, UEnum* InEnum = nullptr)
		: TProperty_Numeric(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
		, Enum(InEnum)
	{
	}

	UByteProperty(const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UEnum* InEnum = nullptr)
	:	TProperty_Numeric( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	,	Enum( InEnum )
	{
	}

	// UObject interface.
	virtual void Serialize( FArchive& Ar ) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;

	// End of UObject interface

	// UHT interface
	virtual FString GetCPPType( FString* ExtendedTypeText=NULL, uint32 CPPExportFlags=0 ) const override;
	// End of UHT interface

	// UProperty interface.
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const override;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	// End of UProperty interface

	// UNumericProperty interface.
	virtual UEnum* GetIntPropertyEnum() const override;
	// End of UNumericProperty interface
};

/*-----------------------------------------------------------------------------
	UInt8Property.
-----------------------------------------------------------------------------*/

//
// Describes a 8-bit signed integer variable.
//
class COREUOBJECT_API UInt8Property : public TProperty_Numeric<int8>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UInt8Property, TProperty_Numeric<int8>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UInt8Property)

	UInt8Property(ECppProperty, int32 InOffset, uint64 InFlags)
		: TProperty_Numeric(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UInt8Property( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
		:	TProperty_Numeric( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	{
	}
};

/*-----------------------------------------------------------------------------
	UInt16Property.
-----------------------------------------------------------------------------*/

//
// Describes a 16-bit signed integer variable.
//
class COREUOBJECT_API UInt16Property : public TProperty_Numeric<int16>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UInt16Property, TProperty_Numeric<int16>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UInt16Property)

	UInt16Property(ECppProperty, int32 InOffset, uint64 InFlags)
		: TProperty_Numeric(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UInt16Property( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	{
	}
};


/*-----------------------------------------------------------------------------
	UIntProperty.
-----------------------------------------------------------------------------*/

//
// Describes a 32-bit signed integer variable.
//
class COREUOBJECT_API UIntProperty : public TProperty_Numeric<int32>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UIntProperty, TProperty_Numeric<int32>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UIntProperty)

	UIntProperty(ECppProperty, int32 InOffset, uint64 InFlags)
		: TProperty_Numeric(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UIntProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	{
	}
};

/*-----------------------------------------------------------------------------
	UInt64Property.
-----------------------------------------------------------------------------*/

//
// Describes a 64-bit signed integer variable.
//
class COREUOBJECT_API UInt64Property : public TProperty_Numeric<int64>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UInt64Property, TProperty_Numeric<int64>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UInt64Property)

	UInt64Property(ECppProperty, int32 InOffset, uint64 InFlags)
		: TProperty_Numeric(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UInt64Property( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( ObjectInitializer, EC_CppProperty, InOffset, InFlags )
	{
	}
};

/*-----------------------------------------------------------------------------
	UUInt16Property.
-----------------------------------------------------------------------------*/

//
// Describes a 16-bit unsigned integer variable.
//
class COREUOBJECT_API UUInt16Property : public TProperty_Numeric<uint16>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UUInt16Property, TProperty_Numeric<uint16>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UUInt16Property)

	UUInt16Property(ECppProperty, int32 InOffset, uint64 InFlags)
		: TProperty_Numeric(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UUInt16Property( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( ObjectInitializer, EC_CppProperty, InOffset, InFlags )
	{
	}
};

/*-----------------------------------------------------------------------------
	UUInt32Property.
-----------------------------------------------------------------------------*/

//
// Describes a 32-bit unsigned integer variable.
//
class COREUOBJECT_API UUInt32Property : public TProperty_Numeric<uint32>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UUInt32Property, TProperty_Numeric<uint32>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UUInt32Property)

	UUInt32Property( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( ObjectInitializer, EC_CppProperty, InOffset, InFlags )
	{
	}
};

/*-----------------------------------------------------------------------------
	UUInt64Property.
-----------------------------------------------------------------------------*/

//
// Describes a 64-bit unsigned integer variable.
//
class COREUOBJECT_API UUInt64Property : public TProperty_Numeric<uint64>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UUInt64Property, TProperty_Numeric<uint64>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UUInt64Property)

	UUInt64Property(ECppProperty, int32 InOffset, uint64 InFlags)
		: TProperty_Numeric(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UUInt64Property( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( ObjectInitializer, EC_CppProperty, InOffset, InFlags )
	{
	}
};


/*-----------------------------------------------------------------------------
	Aliases for implicitly-sized integer properties.
-----------------------------------------------------------------------------*/

namespace UE4Types_Private
{
	template <typename IntType> struct TIntegerPropertyMapping;

	template <> struct TIntegerPropertyMapping<int8>   { typedef UInt8Property   Type; };
	template <> struct TIntegerPropertyMapping<int16>  { typedef UInt16Property  Type; };
	template <> struct TIntegerPropertyMapping<int32>  { typedef UIntProperty    Type; };
	template <> struct TIntegerPropertyMapping<int64>  { typedef UInt64Property  Type; };
	template <> struct TIntegerPropertyMapping<uint8>  { typedef UByteProperty   Type; };
	template <> struct TIntegerPropertyMapping<uint16> { typedef UUInt16Property Type; };
	template <> struct TIntegerPropertyMapping<uint32> { typedef UUInt32Property Type; };
	template <> struct TIntegerPropertyMapping<uint64> { typedef UUInt64Property Type; };
}

typedef UE4Types_Private::TIntegerPropertyMapping<signed int>::Type UUnsizedIntProperty;
typedef UE4Types_Private::TIntegerPropertyMapping<unsigned int>::Type UUnsizedUIntProperty;


/*-----------------------------------------------------------------------------
	UFloatProperty.
-----------------------------------------------------------------------------*/

//
// Describes an IEEE 32-bit floating point variable.
//
class COREUOBJECT_API UFloatProperty : public TProperty_Numeric<float>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UFloatProperty, TProperty_Numeric<float>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UFloatProperty)

	UFloatProperty(ECppProperty, int32 InOffset, uint64 InFlags)
		: TProperty_Numeric(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UFloatProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
		:	TProperty_Numeric( ObjectInitializer, EC_CppProperty, InOffset, InFlags )
	{
	}

	// UProperty interface
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const override;
	// End of UProperty interface
};

/*-----------------------------------------------------------------------------
	UDoubleProperty.
-----------------------------------------------------------------------------*/

//
// Describes an IEEE 64-bit floating point variable.
//
class COREUOBJECT_API UDoubleProperty : public TProperty_Numeric<double>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UDoubleProperty, TProperty_Numeric<double>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UDoubleProperty)

	UDoubleProperty(ECppProperty, int32 InOffset, uint64 InFlags)
		: TProperty_Numeric(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UDoubleProperty(const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags)
		: TProperty_Numeric(ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	{
	}
};



/*-----------------------------------------------------------------------------
	UBoolProperty.
-----------------------------------------------------------------------------*/

//
// Describes a single bit flag variable residing in a 32-bit unsigned double word.
//
class COREUOBJECT_API UBoolProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UBoolProperty, UProperty, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UBoolProperty, NO_API)

	// Variables.
private:

	/** Size of the bitfield/bool property. Equal to ElementSize but used to check if the property has been properly initialized (0-8, where 0 means uninitialized). */
	uint8 FieldSize;
	/** Offset from the memeber variable to the byte of the property (0-7). */
	uint8 ByteOffset;
	/** Mask of the byte byte with the property value. */
	uint8 ByteMask;
	/** Mask of the field with the property value. Either equal to ByteMask or 255 in case of 'bool' type. */
	uint8 FieldMask;

public:

	UBoolProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	/**
	 * Constructor.
	 *
	 * @param ECppProperty Unused.
	 * @param InOffset Offset of the property.
	 * @param InCategory Category of the property.
	 * @param InFlags Property flags.
	 * @param InBitMask Bitmask of the bitfield this property represents.
	 * @param InElementSize Sizeof of the boolean type this property represents.
	 * @param bIsNativeBool true if this property represents C++ bool type.
	 */
	UBoolProperty(ECppProperty, int32 InOffset, uint64 InFlags, uint32 InBitMask, uint32 InElementSize, bool bIsNativeBool);

	/**
	 * Constructor.
	 *
	 * @param ObjectInitializer Properties.
	 * @param ECppProperty Unused.
	 * @param InOffset Offset of the property.
	 * @param InCategory Category of the property.
	 * @param InFlags Property flags.
	 * @param InBitMask Bitmask of the bitfield this property represents.
	 * @param InElementSize Sizeof of the boolean type this property represents.
	 * @param bIsNativeBool true if this property represents C++ bool type.
	 */
	UBoolProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, uint32 InBitMask, uint32 InElementSize, bool bIsNativeBool );

	// UObject interface.
	virtual void Serialize( FArchive& Ar ) override;
	// End of UObject interface

	// UHT interface
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const override;
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	// End of UHT interface

	// UProperty interface.
	virtual void LinkInternal(FArchive& Ar) override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const override;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const override;
	virtual void CopyValuesInternal( void* Dest, void const* Src, int32 Count ) const override;
	virtual void ClearValueInternal( void* Data ) const override;
	virtual void InitializeValueInternal( void* Dest ) const override;
	virtual int32 GetMinAlignment() const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	// End of UProperty interface

	// Emulate the CPP type API, see TPropertyTypeFundamentals
	// this is incomplete as some operations make no sense for bitfields, for example they don't have a usable address
	typedef bool TCppType;
	FORCEINLINE bool GetPropertyValue(void const* A) const
	{
		check(FieldSize != 0);
		uint8* ByteValue = (uint8*)A + ByteOffset;
		return !!(*ByteValue & FieldMask);
	}
	FORCEINLINE bool GetPropertyValue_InContainer(void const* A, int32 ArrayIndex = 0) const
	{
		return GetPropertyValue(ContainerPtrToValuePtr<void>(A, ArrayIndex));
	}
	static FORCEINLINE bool GetDefaultPropertyValue()
	{
		return false;
	}
	FORCEINLINE bool GetOptionalPropertyValue(void const* B) const
	{
		return B ? GetPropertyValue(B) : GetDefaultPropertyValue();
	}
	FORCEINLINE bool GetOptionalPropertyValue_InContainer(void const* B, int32 ArrayIndex = 0) const
	{
		return B ? GetPropertyValue_InContainer(B, ArrayIndex) : GetDefaultPropertyValue();
	}
	FORCEINLINE void SetPropertyValue(void* A, bool Value) const
	{
		check(FieldSize != 0);
		uint8* ByteValue = (uint8*)A + ByteOffset;
		*ByteValue = ((*ByteValue) & ~FieldMask) | (Value ? ByteMask : 0);
	}
	FORCEINLINE void SetPropertyValue_InContainer(void* A, bool Value, int32 ArrayIndex = 0) const
	{
		SetPropertyValue(ContainerPtrToValuePtr<void>(A, ArrayIndex), Value);
	}
	// End of the CPP type API

	/** 
	 * Sets the bitfield/bool type and size. 
	 * This function must be called before UBoolProperty can be used.
	 *
	 * @param InSize size of the bitfield/bool type.
	 * @param bIsNativeBool true if this property represents C++ bool type.
	 */
	void SetBoolSize( const uint32 InSize, const bool bIsNativeBool = false, const uint32 InBitMask = 0 );

	/**
	 * If the return value is true this UBoolProperty represents C++ bool type.
	 */
	FORCEINLINE bool IsNativeBool() const
	{
		return FieldMask == 0xff;
	}

	uint32 GetValueTypeHashInternal(const void* Src) const override;

#if HACK_HEADER_GENERATOR
	// Required by UHT makefiles for internal data serialization.
	friend struct FBoolPropertyArchiveProxy;
#endif // HACK_HEADER_GENERATOR
};

/*-----------------------------------------------------------------------------
	UObjectPropertyBase.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another object which may be nil.
//
class COREUOBJECT_API UObjectPropertyBase : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UObjectPropertyBase, UProperty, CLASS_Abstract, TEXT("/Script/CoreUObject"), CASTCLASS_UObjectPropertyBase)

	// Variables.
	class UClass* PropertyClass;

	UObjectPropertyBase(ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass = NULL)
		: UProperty(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
		, PropertyClass(InClass)
	{}

	UObjectPropertyBase( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass=NULL )
	:	UProperty( ObjectInitializer, EC_CppProperty, InOffset, InFlags )
	,	PropertyClass( InClass )
	{}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void BeginDestroy() override;
	// End of UObject interface

	// UProperty interface
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const override;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual FName GetID() const override;
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) override;
	virtual bool SameType(const UProperty* Other) const override;
	/**
	 * Copy the value for a single element of this property. To the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopySingleValueToScriptVM( void* Dest, void const* Src ) const override;

	/**
	 * Copy the value for all elements of this property. To the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopyCompleteValueToScriptVM( void* Dest, void const* Src ) const override;

	/**
	 * Copy the value for a single element of this property. From the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopySingleValueFromScriptVM( void* Dest, void const* Src ) const override;

	/**
	 * Copy the value for all elements of this property. From the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopyCompleteValueFromScriptVM( void* Dest, void const* Src ) const override;
	// End of UProperty interface

	// UObjectPropertyBase interface
public:

	virtual FString GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName) const PURE_VIRTUAL(UObjectPropertyBase::GetCPPTypeCustom, return TEXT(""););

	/**
	 * Parses a text buffer into an object reference.
	 *
	 * @param	Property			the property that the value is being importing to
	 * @param	OwnerObject			the object that is importing the value; used for determining search scope.
	 * @param	RequiredMetaClass	the meta-class for the object to find; if the object that is resolved is not of this class type, the result is NULL.
	 * @param	PortFlags			bitmask of EPropertyPortFlags that can modify the behavior of the search
	 * @param	Buffer				the text to parse; should point to a textual representation of an object reference.  Can be just the object name (either fully 
	 *								fully qualified or not), or can be formatted as a const object reference (i.e. SomeClass'SomePackage.TheObject')
	 *								When the function returns, Buffer will be pointing to the first character after the object value text in the input stream.
	 * @param	ResolvedValue		receives the object that is resolved from the input text.
	 *
	 * @return	true if the text is successfully resolved into a valid object reference of the correct type, false otherwise.
	 */
	static bool ParseObjectPropertyValue( const UProperty* Property, UObject* OwnerObject, UClass* RequiredMetaClass, uint32 PortFlags, const TCHAR*& Buffer, UObject*& out_ResolvedValue );
	static UObject* FindImportedObject( const UProperty* Property, UObject* OwnerObject, UClass* ObjectClass, UClass* RequiredMetaClass, const TCHAR* Text, uint32 PortFlags = 0);
	
	// Returns the qualified export path for a given object, parent, and export root scope
	static FString GetExportPath(const UObject* Object, const UObject* Parent, const UObject* ExportRootScope, const uint32 PortFlags);

	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const;
	FORCEINLINE UObject* GetObjectPropertyValue_InContainer(const void* PropertyValueAddress, int32 ArrayIndex = 0) const
	{
		return GetObjectPropertyValue(ContainerPtrToValuePtr<void>(PropertyValueAddress, ArrayIndex));
	}
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const;
	FORCEINLINE void SetObjectPropertyValue_InContainer(void* PropertyValueAddress, UObject* Value, int32 ArrayIndex = 0) const
	{
		SetObjectPropertyValue(ContainerPtrToValuePtr<void>(PropertyValueAddress, ArrayIndex), Value);
	}

	/**
	 * Setter function for this property's PropertyClass member. Favor this 
	 * function whilst loading (since, to handle circular dependencies, we defer 
	 * some class loads and use a placeholder class instead). It properly 
	 * handles deferred loading placeholder classes (so they can properly be 
	 * replaced later).
	 *  
	 * @param  NewPropertyClass    The PropertyClass you want this property set with.
	 */
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	void SetPropertyClass(UClass* NewPropertyClass);
#else
	FORCEINLINE void SetPropertyClass(UClass* NewPropertyClass) { PropertyClass = NewPropertyClass; }
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

protected:
	virtual bool AllowCrossLevel() const;

	virtual void CheckValidObject(void* Value) const;
	// End of UObjectPropertyBase interface
};

template<typename InTCppType>
class COREUOBJECT_API TUObjectPropertyBase : public TProperty<InTCppType, UObjectPropertyBase>
{
public:
	typedef TProperty<InTCppType, UObjectPropertyBase> Super;
	typedef InTCppType TCppType;
	typedef typename Super::TTypeFundamentals TTypeFundamentals;

	TUObjectPropertyBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
	{
	}

	TUObjectPropertyBase(ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass)
		: Super(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
		this->PropertyClass = InClass;
	}

	TUObjectPropertyBase( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass )
		:	Super( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	{
		this->PropertyClass = InClass;
	}

	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	TUObjectPropertyBase(FVTableHelper& Helper) : Super(Helper) {};

	// UProperty interface.
	virtual bool ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const override
	{
		return !TIsWeakPointerType<InTCppType>::Value;
	}
	virtual bool ContainsWeakObjectReference() const override
	{
		return TIsWeakPointerType<InTCppType>::Value;
	}
	// End of UProperty interface

	// TProperty::GetCPPType should not be used here
	virtual FString GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags) const override
	{
		check(UObjectPropertyBase::PropertyClass);
		return this->GetCPPTypeCustom(ExtendedTypeText, CPPExportFlags, 
			FString::Printf(TEXT("%s%s"), UObjectPropertyBase::PropertyClass->GetPrefixCPP(), *UObjectPropertyBase::PropertyClass->GetName()));
	}
};


//
// Describes a reference variable to another object which may be nil.
//
class COREUOBJECT_API UObjectProperty : public TUObjectPropertyBase<UObject*>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UObjectProperty, TUObjectPropertyBase<UObject*>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UObjectProperty)

	UObjectProperty(ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass)
		: TUObjectPropertyBase(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags, InClass)
	{
	}

	UObjectProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass )
		: TUObjectPropertyBase(ObjectInitializer, EC_CppProperty, InOffset, InFlags, InClass)
	{
	}

	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	// End of UHT interface

	// UProperty interface
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	virtual void EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps) override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;

private:
	virtual uint32 GetValueTypeHashInternal(const void* Src) const override;
public:
	// End of UProperty interface

	// UObjectPropertyBase interface
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const override;
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const override;
	virtual FString GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName)  const override;
	// End of UObjectPropertyBase interface
};

//
// Describes a reference variable to another object which may be nil, and may turn nil at any point
//
class COREUOBJECT_API UWeakObjectProperty : public TUObjectPropertyBase<FWeakObjectPtr>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UWeakObjectProperty, TUObjectPropertyBase<FWeakObjectPtr>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UWeakObjectProperty)

	UWeakObjectProperty(ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass)
		: TUObjectPropertyBase(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags, InClass)
	{
	}

	UWeakObjectProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass )
	:	TUObjectPropertyBase( ObjectInitializer, EC_CppProperty, InOffset, InFlags, InClass )
	{
	}
	
	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  override;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	// End of UHT interface

	// UProperty interface
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	// End of UProperty interface

	// UObjectProperty interface
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const override;
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const override;
	// End of UObjectProperty interface
};

//
// Describes a reference variable to another object which may be nil, and will become valid or invalid at any point
//
class COREUOBJECT_API ULazyObjectProperty : public TUObjectPropertyBase<FLazyObjectPtr>
{
	DECLARE_CASTED_CLASS_INTRINSIC(ULazyObjectProperty, TUObjectPropertyBase<FLazyObjectPtr>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_ULazyObjectProperty)

	ULazyObjectProperty(ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass)
		: TUObjectPropertyBase(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags, InClass)
	{
	}

	ULazyObjectProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass )
		: TUObjectPropertyBase(ObjectInitializer, EC_CppProperty, InOffset, InFlags, InClass)
	{
	}

	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  override;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const override;
	// End of UHT interface

	// UProperty interface
	virtual FName GetID() const override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	// End of UProperty interface

	// UObjectProperty interface
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const override;
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const override;
	virtual bool AllowCrossLevel() const override;
private:
	virtual uint32 GetValueTypeHashInternal(const void* Src) const override;
public:
	// End of UObjectProperty interface
};

//
// Describes a reference variable to another object which may be nil, and will become valid or invalid at any point
//
class COREUOBJECT_API USoftObjectProperty : public TUObjectPropertyBase<FSoftObjectPtr>
{
	DECLARE_CASTED_CLASS_INTRINSIC(USoftObjectProperty, TUObjectPropertyBase<FSoftObjectPtr>, 0, TEXT("/Script/CoreUObject"), CASTCLASS_USoftObjectProperty)

	USoftObjectProperty(ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass)
		: TUObjectPropertyBase(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags, InClass)
	{}

	USoftObjectProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass )
		: TUObjectPropertyBase(ObjectInitializer, EC_CppProperty, InOffset, InFlags, InClass)
	{}

	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	// End of UHT interface

	// UProperty interface
	virtual FName GetID() const override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	// End of UProperty interface

	// UObjectProperty interface
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const override;
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const override;
	virtual bool AllowCrossLevel() const override;
	virtual FString GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName)  const override;

private:
	virtual uint32 GetValueTypeHashInternal(const void* Src) const override;
public:

	// ScriptVM should store Asset as FSoftObjectPtr not as UObject.

	virtual void CopySingleValueToScriptVM(void* Dest, void const* Src) const override;
	virtual void CopyCompleteValueToScriptVM(void* Dest, void const* Src) const override;
	virtual void CopySingleValueFromScriptVM(void* Dest, void const* Src) const override;
	virtual void CopyCompleteValueFromScriptVM(void* Dest, void const* Src) const override;
	// End of UObjectProperty interface
};

/*-----------------------------------------------------------------------------
	UClassProperty.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another object which may be nil.
//
class COREUOBJECT_API UClassProperty : public UObjectProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UClassProperty, UObjectProperty, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UClassProperty)

	// Variables.
	class UClass* MetaClass;
public:
	UClassProperty(ECppProperty, int32 InOffset, uint64 InFlags, UClass* InMetaClass, UClass* InClassType)
		: UObjectProperty(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags, InClassType ? InClassType : UClass::StaticClass())
		, MetaClass(InMetaClass)
	{
	}

	UClassProperty(const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InMetaClass, UClass* InClassType)
		: UObjectProperty(ObjectInitializer, EC_CppProperty, InOffset, InFlags, InClassType ? InClassType : UClass::StaticClass())
	,	MetaClass( InMetaClass )
	{
	}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void BeginDestroy() override;
	// End of UObject interface

	// UHT interface
	virtual FString GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags)  const override;
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	// End of UHT interface

	// UProperty interface
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual bool SameType(const UProperty* Other) const override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	// End of UProperty interface

	virtual FString GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName)  const override;

	/**
	 * Setter function for this property's MetaClass member. Favor this function 
	 * whilst loading (since, to handle circular dependencies, we defer some 
	 * class loads and use a placeholder class instead). It properly handles 
	 * deferred loading placeholder classes (so they can properly be replaced 
	 * later).
	 * 
	 * @param  NewMetaClass    The MetaClass you want this property set with.
	 */
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	void SetMetaClass(UClass* NewMetaClass);
#else
	FORCEINLINE void SetMetaClass(UClass* NewMetaClass) { MetaClass = NewMetaClass; }
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
};

/*-----------------------------------------------------------------------------
	USoftClassProperty.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another class which may be nil, and will become valid or invalid at any point
//
class COREUOBJECT_API USoftClassProperty : public USoftObjectProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(USoftClassProperty, USoftObjectProperty, 0, TEXT("/Script/CoreUObject"), CASTCLASS_USoftClassProperty)

	// Variables.
	class UClass* MetaClass;
public:
	USoftClassProperty(ECppProperty, int32 InOffset, uint64 InFlags, UClass* InMetaClass)
		: Super(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags, UClass::StaticClass())
		, MetaClass(InMetaClass)
	{}

	USoftClassProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InMetaClass )
		:	Super(ObjectInitializer, EC_CppProperty, InOffset, InFlags, UClass::StaticClass() )
		,	MetaClass( InMetaClass )
	{}

	// UHT interface
	virtual FString GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags) const override;
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	// End of UHT interface

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void BeginDestroy() override;
	// End of UObject interface

	// UProperty interface
	virtual bool SameType(const UProperty* Other) const override;
	// End of UProperty interface

	virtual FString GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName)  const override;

	/**
	 * Setter function for this property's MetaClass member. Favor this function 
	 * whilst loading (since, to handle circular dependencies, we defer some 
	 * class loads and use a placeholder class instead). It properly handles 
	 * deferred loading placeholder classes (so they can properly be replaced 
	 * later).
	 * 
	 * @param  NewMetaClass    The MetaClass you want this property set with.
	 */
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	void SetMetaClass(UClass* NewMetaClass);
#else
	FORCEINLINE void SetMetaClass(UClass* NewMetaClass) { MetaClass = NewMetaClass; }
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
};

/*-----------------------------------------------------------------------------
	UInterfaceProperty.
-----------------------------------------------------------------------------*/

/**
 * This variable type provides safe access to a native interface pointer.  The data class for this variable is FScriptInterface, and is exported to auto-generated
 * script header files as a TScriptInterface.
 */

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FScriptInterface, UProperty> UInterfaceProperty_Super;

class COREUOBJECT_API UInterfaceProperty : public UInterfaceProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UInterfaceProperty, UInterfaceProperty_Super, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UInterfaceProperty)

	/** The native interface class that this interface property refers to */
	class	UClass*		InterfaceClass;
public:
	typedef UInterfaceProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UInterfaceProperty(ECppProperty, int32 InOffset, uint64 InFlags, UClass* InInterfaceClass)
		: UInterfaceProperty_Super(FObjectInitializer::Get(), EC_CppProperty, InOffset, (InFlags & ~CPF_InterfaceClearMask))
		, InterfaceClass(InInterfaceClass)
	{
	}

	UInterfaceProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InInterfaceClass )
		:	UInterfaceProperty_Super( ObjectInitializer, EC_CppProperty, InOffset, (InFlags & ~CPF_InterfaceClearMask) )
		,	InterfaceClass( InInterfaceClass )
	{
	}

	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  override;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	// End of UHT interface

	// UProperty interface
	virtual void LinkInternal(FArchive& Ar) override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const override;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual bool ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const override;
	virtual bool SameType(const UProperty* Other) const override;
	// End of UProperty interface

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	virtual void EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps) override;
	virtual void BeginDestroy() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End of UObject interface

	/**
	 * Setter function for this property's InterfaceClass member. Favor this 
	 * function whilst loading (since, to handle circular dependencies, we defer 
	 * some class loads and use a placeholder class instead). It properly 
	 * handles deferred loading placeholder classes (so they can properly be 
	 * replaced later).
	 *  
	 * @param  NewInterfaceClass    The InterfaceClass you want this property set with.
	 */
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	void SetInterfaceClass(UClass* NewInterfaceClass);
#else
	FORCEINLINE void SetInterfaceClass(UClass* NewInterfaceClass) { InterfaceClass = NewInterfaceClass; }
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
};

/*-----------------------------------------------------------------------------
	UNameProperty.
-----------------------------------------------------------------------------*/

//
// Describes a name variable pointing into the global name table.
//

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty_WithEqualityAndSerializer<FName, UProperty> UNameProperty_Super;

class COREUOBJECT_API UNameProperty : public UNameProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UNameProperty, UNameProperty_Super, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UNameProperty)
public:
	typedef UNameProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UNameProperty(ECppProperty, int32 InOffset, uint64 InFlags)
		: UNameProperty_Super(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UNameProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
	:	UNameProperty_Super( ObjectInitializer, EC_CppProperty, InOffset, InFlags )
	{
	}

	// UProperty interface
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	uint32 GetValueTypeHashInternal(const void* Src) const override;
	// End of UProperty interface
};

/*-----------------------------------------------------------------------------
	UStrProperty.
-----------------------------------------------------------------------------*/

//
// Describes a dynamic string variable.
//

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty_WithEqualityAndSerializer<FString, UProperty> UStrProperty_Super;

class COREUOBJECT_API UStrProperty : public UStrProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UStrProperty, UStrProperty_Super, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UStrProperty)
public:
	typedef UStrProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UStrProperty(ECppProperty, int32 InOffset, uint64 InFlags)
		: UStrProperty_Super(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UStrProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
	:	UStrProperty_Super( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	{
	}

	// UProperty interface
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	uint32 GetValueTypeHashInternal(const void* Src) const override;
	// End of UProperty interface

	// Necessary to fix Compiler Error C2026
	static FString ExportCppHardcodedText(const FString& InSource, const FString& Indent);
};

/*-----------------------------------------------------------------------------
	UArrayProperty.
-----------------------------------------------------------------------------*/

//
// Describes a dynamic array.
//

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FScriptArray, UProperty> UArrayProperty_Super;

class COREUOBJECT_API UArrayProperty : public UArrayProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UArrayProperty, UArrayProperty_Super, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UArrayProperty)

	// Variables.
	UProperty* Inner;

public:
	typedef UArrayProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UArrayProperty(ECppProperty, int32 InOffset, uint64 InFlags)
		: UArrayProperty_Super(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UArrayProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
	:	UArrayProperty_Super( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	{
	}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	// End of UObject interface

	// UField interface
	virtual void AddCppProperty( UProperty* Property ) override;
	// End of UField interface

	// UProperty interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  override;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	virtual void LinkInternal(FArchive& Ar) override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const override;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual void CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const override;
	virtual void ClearValueInternal( void* Data ) const override;
	virtual void DestroyValueInternal( void* Dest ) const override;
	virtual bool PassCPPArgsByRef() const override;
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) override;
	virtual bool ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const override;
	virtual bool ContainsWeakObjectReference() const override;
	virtual void EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps) override;
	virtual bool SameType(const UProperty* Other) const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	// End of UProperty interface

	FString GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerTypeText, const FString& InInnerExtendedTypeText) const;
};

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FScriptMap, UProperty> UMapProperty_Super;

class COREUOBJECT_API UMapProperty : public UMapProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UMapProperty, UMapProperty_Super, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UMapProperty)

	// Properties representing the key type and value type of the contained pairs
	UProperty*       KeyProp;
	UProperty*       ValueProp;
	FScriptMapLayout MapLayout;

public:
	typedef UMapProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UMapProperty(const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags);

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	// End of UObject interface

	// UField interface
	virtual void AddCppProperty(UProperty* Property) override;
	// End of UField interface

	// UProperty interface
	virtual FString GetCPPMacroType(FString& ExtendedTypeText) const  override;
	virtual FString GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags) const override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	virtual void LinkInternal(FArchive& Ar) override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags) const override;
	virtual void SerializeItem(FArchive& Ar, void* Value, void const* Defaults) const override;
	virtual bool NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const override;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const override;
	virtual void ClearValueInternal(void* Data) const override;
	virtual void DestroyValueInternal(void* Dest) const override;
	virtual bool PassCPPArgsByRef() const override;
	virtual void InstanceSubobjects(void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph) override;
	virtual bool ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const override;
	virtual bool ContainsWeakObjectReference() const override;
	virtual void EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps) override;
	virtual bool SameType(const UProperty* Other) const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	// End of UProperty interface
};

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FScriptSet, UProperty> USetProperty_Super;

class COREUOBJECT_API USetProperty : public USetProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(USetProperty, USetProperty_Super, 0, TEXT("/Script/CoreUObject"), CASTCLASS_USetProperty)

	// Properties representing the key type and value type of the contained pairs
	UProperty*       ElementProp;
	FScriptSetLayout SetLayout;

public:
	typedef USetProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	USetProperty(const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags);

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	// End of UObject interface

	// UField interface
	virtual void AddCppProperty(UProperty* Property) override;
	// End of UField interface

	// UProperty interface
	virtual FString GetCPPMacroType(FString& ExtendedTypeText) const  override;
	virtual FString GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags) const override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	virtual void LinkInternal(FArchive& Ar) override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags) const override;
	virtual void SerializeItem(FArchive& Ar, void* Value, void const* Defaults) const override;
	virtual bool NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const override;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const override;
	virtual void ClearValueInternal(void* Data) const override;
	virtual void DestroyValueInternal(void* Dest) const override;
	virtual bool PassCPPArgsByRef() const override;
	virtual void InstanceSubobjects(void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph) override;
	virtual bool ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const override;
	virtual bool ContainsWeakObjectReference() const override;
	virtual void EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps) override;
	virtual bool SameType(const UProperty* Other) const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	// End of UProperty interface
};

/**
 * FScriptArrayHelper: Pseudo dynamic array. Used to work with array properties in a sensible way.
 **/
class FScriptArrayHelper
{
public:
	/**
	 *	Constructor, brings together a property and an instance of the property located in memory
	 *	@param	InProperty: the property associated with this memory
	 *	@param	InArray: pointer to raw memory that corresponds to this array. This can be NULL, and sometimes is, but in that case almost all operations will crash.
	**/
	FORCEINLINE FScriptArrayHelper(const UArrayProperty* InProperty, const void *InArray)
		: InnerProperty(InProperty->Inner)
		, Array((FScriptArray*)InArray)  //@todo, we are casting away the const here
		, ElementSize(InnerProperty->ElementSize)
	{
		check(ElementSize > 0);
		check(InnerProperty);
	}

	/**
	 *	Index range check
	 *	@param	Index: Index to check
	 *	@return true if accessing this element is legal.
	**/
	FORCEINLINE bool IsValidIndex( int32 Index ) const
	{
		return Index >= 0 && Index < Num();
	}
	/**
	 *	Return the number of elements in the array.
	 *	@return	The number of elements in the array.
	**/
	FORCEINLINE int32 Num() const
	{
		checkSlow(Array->Num() >= 0); 
		return Array->Num();
	}
	/**
	 *	Static version of Num() used when you don't need to bother to construct a FScriptArrayHelper. Returns the number of elements in the array.
	 *	@param	Target: pointer to the raw memory associated with a FScriptArray
	 *	@return The number of elements in the array.
	**/
	static FORCEINLINE int32 Num(const void *Target)
	{
		checkSlow(((const FScriptArray*)Target)->Num() >= 0); 
		return ((const FScriptArray*)Target)->Num();
	}
	/**
	 *	Returns a uint8 pointer to an element in the array
	 *	@param	Index: index of the item to return a pointer to.
	 *	@return	Pointer to this element, or NULL if the array is empty
	**/
	FORCEINLINE uint8* GetRawPtr(int32 Index = 0)
	{
		if (!Num())
		{
			checkSlow(!Index);
			return NULL;
		}
		checkSlow(IsValidIndex(Index)); 
		return (uint8*)Array->GetData() + Index * ElementSize;
	}
	/**
	*	Empty the array, then add blank, constructed values to a given size.
	*	@param	Count: the number of items the array will have on completion.
	**/
	void EmptyAndAddValues(int32 Count)
	{ 
		check(Count>=0);
		checkSlow(Num() >= 0); 
		EmptyValues(Count);
		if (Count)
		{
			AddValues(Count);
		}
	}
	/**
	*	Empty the array, then add uninitialized values to a given size.
	*	@param	Count: the number of items the array will have on completion.
	**/
	void EmptyAndAddUninitializedValues(int32 Count)
	{ 
		check(Count>=0);
		checkSlow(Num() >= 0); 
		EmptyValues(Count);
		if (Count)
		{
			AddUninitializedValues(Count);
		}
	}
	/**
	*	Expand the array, if needed, so that the given index is valid
	*	@param	Index: index for the item that we want to ensure is valid
	*	@return true if expansion was necessary
	*	NOTE: This is not a count, it is an INDEX, so the final count will be at least Index+1 this matches the usage.
	**/
	bool ExpandForIndex(int32 Index)
	{ 
		check(Index>=0);
		checkSlow(Num() >= 0); 
		if (Index >= Num())
		{
			AddValues(Index - Num() + 1);
			return true;
		}
		return false;
	}
	/**
	*	Add or remove elements to set the array to a given size.
	*	@param	Count: the number of items the array will have on completion.
	**/
	void Resize(int32 Count)
	{ 
		check(Count>=0);
		int32 OldNum = Num();
		if (Count > OldNum)
		{
			AddValues(Count - OldNum);
		}
		else if (Count < OldNum)
		{
			RemoveValues(Count, OldNum - Count);
		}
	}
	/**
	*	Add blank, constructed values to the end of the array.
	*	@param	Count: the number of items to insert.
	*	@return	the index of the first newly added item.
	**/
	int32 AddValues(int32 Count)
	{ 
		const int32 OldNum = AddUninitializedValues(Count);		
		ConstructItems(OldNum, Count);
		return OldNum;
	}
	/**
	*	Add a blank, constructed values to the end of the array.
	*	@return	the index of the newly added item.
	**/
	FORCEINLINE int32 AddValue()
	{ 
		return AddValues(1);
	}
	/**
	*	Add uninitialized values to the end of the array.
	*	@param	Count: the number of items to insert.
	*	@return	the index of the first newly added item.
	**/
	int32 AddUninitializedValues(int32 Count)
	{
		check(Count>0);
		checkSlow(Num() >= 0);
		const int32 OldNum = Array->Add(Count, ElementSize);
		return OldNum;
	}
	/**
	*	Add an uninitialized value to the end of the array.
	*	@return	the index of the newly added item.
	**/
	FORCEINLINE int32 AddUninitializedValue()
	{
		return AddUninitializedValues(1);
	}
	/**
	 *	Insert blank, constructed values into the array.
	 *	@param	Index: index of the first inserted item after completion
	 *	@param	Count: the number of items to insert.
	**/
	void InsertValues( int32 Index, int32 Count = 1)
	{
		check(Count>0);
		check(Index>=0 && Index <= Num());
		Array->Insert(Index, Count, ElementSize);
		ConstructItems(Index, Count);
	}
	/**
	 *	Remove all values from the array, calling destructors, etc as appropriate.
	 *	@param Slack: used to presize the array for a subsequent add, to avoid reallocation.
	**/
	void EmptyValues(int32 Slack = 0)
	{
		checkSlow(Slack>=0);
		const int32 OldNum = Num();
		if (OldNum)
		{
			DestructItems(0, OldNum);
		}
		if (OldNum || Slack)
		{
			Array->Empty(Slack, ElementSize);
		}
	}
	/**
	 *	Remove values from the array, calling destructors, etc as appropriate.
	 *	@param Index: first item to remove.
	 *	@param Count: number of items to remove.
	**/
	void RemoveValues(int32 Index, int32 Count = 1)
	{
		check(Count>0);
		check(Index>=0 && Index + Count <= Num());
		DestructItems(Index, Count);
		Array->Remove(Index, Count, ElementSize);
	}

	/**
	*	Clear values in the array. The meaning of clear is defined by the property system.
	*	@param Index: first item to clear.
	*	@param Count: number of items to clear.
	**/
	void ClearValues(int32 Index, int32 Count = 1)
	{
		check(Count>0);
		check(Index>=0);
		ClearItems(Index, Count);
	}

	/**
	 *	Swap two elements in the array, does not call constructors and destructors
	 *	@param A index of one item to swap.
	 *	@param B index of the other item to swap.
	**/
	void SwapValues(int32 A, int32 B)
	{
		Array->SwapMemory(A, B, ElementSize);
	}
	/**
	 *	Used by memory counting archives to accumlate the size of this array.
	 *	@param Ar archive to accumulate sizes
	**/
	void CountBytes( FArchive& Ar  )
	{
		Array->CountBytes(Ar, ElementSize);
	}	

	static FScriptArrayHelper CreateHelperFormInnerProperty(const UProperty* InInnerProperty, const void *InArray)
	{
		check(InInnerProperty);
		FScriptArrayHelper ScriptArrayHelper;
		ScriptArrayHelper.InnerProperty = InInnerProperty;
		ScriptArrayHelper.Array = (FScriptArray*)InArray;
		ScriptArrayHelper.ElementSize = InInnerProperty->ElementSize;
		return ScriptArrayHelper;
	}

private:

	FScriptArrayHelper()
		: InnerProperty(nullptr)
		, Array(nullptr)
		, ElementSize(0)
	{}

	/**
	 *	Internal function to call into the property system to construct / initialize elements.
	 *	@param Index: first item to .
	 *	@param Count: number of items to .
	**/
	void ConstructItems(int32 Index, int32 Count)
	{
		checkSlow(Count > 0);
		checkSlow(Index >= 0); 
		checkSlow(Index <= Num());
		checkSlow(Index + Count <= Num());
		uint8 *Dest = GetRawPtr(Index);
		if (InnerProperty->PropertyFlags & CPF_ZeroConstructor)
		{
			FMemory::Memzero(Dest, Count * ElementSize);
		}
		else
		{
			for (int32 LoopIndex = 0 ; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
			{
				InnerProperty->InitializeValue(Dest);
			}
		}
	}
	/**
	 *	Internal function to call into the property system to destruct elements.
	 *	@param Index: first item to .
	 *	@param Count: number of items to .
	**/
	void DestructItems(int32 Index, int32 Count)
	{
		if (!(InnerProperty->PropertyFlags & (CPF_IsPlainOldData | CPF_NoDestructor)))
		{
			checkSlow(Count > 0);
			checkSlow(Index >= 0); 
			checkSlow(Index < Num());
			checkSlow(Index + Count <= Num());
			uint8 *Dest = GetRawPtr(Index);
			for (int32 LoopIndex = 0 ; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
			{
				InnerProperty->DestroyValue(Dest);
			}
		}
	}
	/**
	 *	Internal function to call into the property system to clear elements.
	 *	@param Index: first item to .
	 *	@param Count: number of items to .
	**/
	void ClearItems(int32 Index, int32 Count)
	{
		checkSlow(Count > 0);
		checkSlow(Index >= 0); 
		checkSlow(Index < Num());
		checkSlow(Index + Count <= Num());
		uint8 *Dest = GetRawPtr(Index);
		if ((InnerProperty->PropertyFlags & (CPF_ZeroConstructor | CPF_NoDestructor)) == (CPF_ZeroConstructor | CPF_NoDestructor))
		{
			FMemory::Memzero(Dest, Count * ElementSize);
		}
		else
		{
			for (int32 LoopIndex = 0; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
			{
				InnerProperty->ClearValue(Dest);
			}
		}
	}

	const UProperty* InnerProperty;
	FScriptArray* Array;
	int32 ElementSize;
};

class FScriptArrayHelper_InContainer : public FScriptArrayHelper
{
public:
	FORCEINLINE FScriptArrayHelper_InContainer(const UArrayProperty* InProperty, const void* InArray, int32 FixedArrayIndex=0)
		:FScriptArrayHelper(InProperty, InProperty->ContainerPtrToValuePtr<void>(InArray, FixedArrayIndex))
	{
	}
};


/**
 * FScriptMapHelper: Pseudo dynamic map. Used to work with map properties in a sensible way.
 */
class FScriptMapHelper
{
	friend class UMapProperty;

public:
	/**
	 * Constructor, brings together a property and an instance of the property located in memory
	 *
	 * @param  InProperty  The property associated with this memory
	 * @param  InMap       Pointer to raw memory that corresponds to this map. This can be NULL, and sometimes is, but in that case almost all operations will crash.
	 */
	FORCEINLINE FScriptMapHelper(const UMapProperty* InProperty, const void* InMap)
		: KeyProp         (InProperty->KeyProp)
		, ValueProp       (InProperty->ValueProp)
		, Map             ((FScriptMap*)InMap)  //@todo, we are casting away the const here
		, MapLayout(InProperty->MapLayout)
	{
		check(KeyProp && ValueProp);
	}

	/**
	 * Index range check
	 *
	 * @param  Index  Index to check
	 *
	 * @return true if accessing this element is legal.
	 */
	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Map->IsValidIndex(Index);
	}

	/**
	 * Returns the number of elements in the map.
	 *
	 * @return The number of elements in the map.
	 */
	FORCEINLINE int32 Num() const
	{
		int32 Result = Map->Num();
		checkSlow(Result >= 0); 
		return Result;
	}

	/**
	 * Returns the (non-inclusive) maximum index of elements in the map.
	 *
	 * @return The (non-inclusive) maximum index of elements in the map.
	 */
	FORCEINLINE int32 GetMaxIndex() const
	{
		int32 Result = Map->GetMaxIndex();
		checkSlow(Result >= Num());
		return Result;
	}

	/**
	 * Static version of Num() used when you don't need to bother to construct a FScriptMapHelper. Returns the number of elements in the map.
	 *
	 * @param  Target  Pointer to the raw memory associated with a FScriptMap
	 *
	 * @return The number of elements in the map.
	 */
	static FORCEINLINE int32 Num(const void* Target)
	{
		int32 Result = ((const FScriptMap*)Target)->Num();
		checkSlow(Result >= 0); 
		return Result;
	}

	/**
	 * Returns a uint8 pointer to the pair in the map
	 *
	 * @param  Index  index of the item to return a pointer to.
	 *
	 * @return Pointer to the pair, or nullptr if the map is empty.
	 */
	FORCEINLINE uint8* GetPairPtr(int32 Index)
	{
		if (Num() == 0)
		{
			checkSlow(!Index);
			return nullptr;
		}

		checkSlow(IsValidIndex(Index));
		return (uint8*)Map->GetData(Index, MapLayout);
	}

	/**
	 * Returns a uint8 pointer to the Key (first element) in the map. Currently 
	 * identical to GetPairPtr, but provides clarity of purpose and avoids exposing
	 * implementation details of TMap.
	 *
	 * @param  Index  index of the item to return a pointer to.
	 *
	 * @return Pointer to the key, or nullptr if the map is empty.
	 */
	FORCEINLINE uint8* GetKeyPtr(int32 Index)
	{
		if (Num() == 0)
		{
			checkSlow(!Index);
			return nullptr;
		}
		
		checkSlow(IsValidIndex(Index));
		return (uint8*)Map->GetData(Index, MapLayout) + MapLayout.KeyOffset;
	}

	/**
	 * Returns a uint8 pointer to the Value (second element) in the map.
	 *
	 * @param  Index  index of the item to return a pointer to.
	 *
	 * @return Pointer to the value, or nullptr if the map is empty.
	 */
	FORCEINLINE uint8* GetValuePtr(int32 Index)
	{
		if (Num() == 0)
		{
			checkSlow(!Index);
			return nullptr;
		}
		
		checkSlow(IsValidIndex(Index));
		return (uint8*)Map->GetData(Index, MapLayout) + MapLayout.ValueOffset;
	}

	/**
	 * Returns a uint8 pointer to the pair in the map.
	 *
	 * @param  Index  index of the item to return a pointer to.
	 *
	 * @return Pointer to the pair, or nullptr if the map is empty.
	 */
	FORCEINLINE const uint8* GetPairPtr(int32 Index) const
	{
		return const_cast<FScriptMapHelper*>(this)->GetPairPtr(Index);
	}

	/**
	 * Add an uninitialized value to the end of the map.
	 *
	 * @return  The index of the added element.
	 */
	FORCEINLINE int32 AddUninitializedValue()
	{
		checkSlow(Num() >= 0);

		return Map->AddUninitialized(MapLayout);
	}

	/**
	 *	Remove all values from the map, calling destructors, etc as appropriate.
	 *	@param Slack: used to presize the set for a subsequent add, to avoid reallocation.
	**/
	void EmptyValues(int32 Slack = 0)
	{
		checkSlow(Slack >= 0);

		int32 OldNum = Num();
		if (OldNum)
		{
			DestructItems(0, OldNum);
		}
		if (OldNum || Slack)
		{
			Map->Empty(Slack, MapLayout);
		}
	}

	/**
	 * Adds a blank, constructed value to a given size.
	 * Note that this will create an invalid map because all the keys will be default constructed, and the map needs rehashing.
	 *
	 * @return  The index of the first element added.
	 **/
	int32 AddDefaultValue_Invalid_NeedsRehash()
	{
		checkSlow(Num() >= 0);

		int32 Result = AddUninitializedValue();
		ConstructItem(Result);

		return Result;
	}

	/**
	 * Returns the property representing the key of the map pair.
	 *
	 * @return The property representing the key of the map pair.
	 */
	UProperty* GetKeyProperty() const
	{
		return KeyProp;
	}

	/**
	 * Returns the property representing the value of the map pair.
	 *
	 * @return The property representing the value of the map pair.
	 */
	UProperty* GetValueProperty() const
	{
		return ValueProp;
	}

	/**
	 * Removes an element at the specified index, destroying it.
	 *
	 * @param  Index  The index of the element to remove.
	 */
	void RemoveAt(int32 Index, int32 Count = 1)
	{
		check(IsValidIndex(Index));

		DestructItems(Index, Count);
		for (; Count; ++Index)
		{
			if (IsValidIndex(Index))
			{
				Map->RemoveAt(Index, MapLayout);
				--Count;
			}
		}
	}

	/**
	 * Rehashes the keys in the map.
	 * This function must be called to create a valid map.
	 */
	COREUOBJECT_API void Rehash();

	/**
	 * Finds the index of an element in a map which matches the key in another pair.
	 *
	 * @param  PairWithKeyToFind  The address of a map pair which contains the key to search for.
	 * @param  IndexHint          The index to start searching from.
	 *
	 * @return The index of an element found in MapHelper, or -1 if none was found.
	 */
	int32 FindMapIndexWithKey(const void* PairWithKeyToFind, int32 IndexHint = 0) const
	{
		int32 MapMax = GetMaxIndex();
		if (MapMax == 0)
		{
			return INDEX_NONE;
		}

		check(IndexHint >= 0 && IndexHint < MapMax);

		UProperty* LocalKeyProp = this->KeyProp; // prevent aliasing in loop below

		int32 Index = IndexHint;
		for (;;)
		{
			if (IsValidIndex(Index))
			{
				const void* PairToSearch = GetPairPtrWithoutCheck(Index);
				if (LocalKeyProp->Identical(PairWithKeyToFind, PairToSearch))
				{
					return Index;
				}
			}

			++Index;
			if (Index == MapMax)
			{
				Index = 0;
			}

			if (Index == IndexHint)
			{
				return INDEX_NONE;
			}
		}
	}

	/**
	 * Finds the pair in a map which matches the key in another pair.
	 *
	 * @param  PairWithKeyToFind  The address of a map pair which contains the key to search for.
	 * @param  IndexHint          The index to start searching from.
	 *
	 * @return A pointer to the found pair, or nullptr if none was found.
	 */
	FORCEINLINE uint8* FindMapPairPtrWithKey(const void* PairWithKeyToFind, int32 IndexHint = 0)
	{
		int32 Index = FindMapIndexWithKey(PairWithKeyToFind, IndexHint);
		uint8* Result = (Index >= 0) ? GetPairPtr(Index) : nullptr;
		return Result;
	}

	/** Finds the associated value from hash, rather than linearly searching */
	uint8* FindValueFromHash(const void* KeyPtr)
	{
		UProperty* LocalKeyPropForCapture = KeyProp;
		return Map->FindValue(
			KeyPtr, 
			MapLayout,
			[LocalKeyPropForCapture](const void* ElementKey) { return LocalKeyPropForCapture->GetValueTypeHash(ElementKey); },
			[LocalKeyPropForCapture](const void* A, const void* B) { return LocalKeyPropForCapture->Identical(A, B); }
			);
	}

	/** Adds the (key, value) pair to the map, returning true if the element was added, or false if the element was already present and has been overwritten */
	void AddPair(const void* KeyPtr, const void* ValuePtr)
	{
		UProperty* LocalKeyPropForCapture = KeyProp;
		UProperty* LocalValuePropForCapture = ValueProp;
		FScriptMapLayout& LocalMapLayoutForCapture = MapLayout;
		Map->Add(
			KeyPtr,
			ValuePtr,
			MapLayout,
			[LocalKeyPropForCapture](const void* ElementKey) { return LocalKeyPropForCapture->GetValueTypeHash(ElementKey); },
			[LocalKeyPropForCapture](const void* A, const void* B) { return LocalKeyPropForCapture->Identical(A, B); },
			[LocalKeyPropForCapture, KeyPtr, LocalMapLayoutForCapture](void* NewElementKey)
			{
				if (LocalKeyPropForCapture->PropertyFlags & CPF_ZeroConstructor)
				{
					FMemory::Memzero(NewElementKey, LocalKeyPropForCapture->GetSize());
				}
				else
				{
					LocalKeyPropForCapture->InitializeValue(NewElementKey);
				}

				LocalKeyPropForCapture->CopySingleValueToScriptVM(NewElementKey, KeyPtr);
			},
			[LocalValuePropForCapture, ValuePtr, LocalMapLayoutForCapture](void* NewElementValue)
			{
				if (LocalValuePropForCapture->PropertyFlags & CPF_ZeroConstructor)
				{
					FMemory::Memzero(NewElementValue, LocalValuePropForCapture->GetSize());
				}
				else
				{
					LocalValuePropForCapture->InitializeValue(NewElementValue);
				}

				LocalValuePropForCapture->CopySingleValueToScriptVM(NewElementValue, ValuePtr);
			},
			[LocalValuePropForCapture, ValuePtr](void* ExistingElementValue)
			{
				LocalValuePropForCapture->CopySingleValueToScriptVM(ExistingElementValue, ValuePtr);
			},
			[LocalKeyPropForCapture](void* ElementKey)
			{
				if (!(LocalKeyPropForCapture->PropertyFlags & (CPF_IsPlainOldData | CPF_NoDestructor)))
				{
					LocalKeyPropForCapture->DestroyValue(ElementKey);
				}
			},
			[LocalValuePropForCapture](void* ElementValue)
			{
				if (!(LocalValuePropForCapture->PropertyFlags & (CPF_IsPlainOldData | CPF_NoDestructor)))
				{
					LocalValuePropForCapture->DestroyValue(ElementValue);
				}
			}
		);
	}

	/** Removes the key and its associated value from the map */
	bool RemovePair(const void* KeyPtr)
	{
		UProperty* LocalKeyPropForCapture = KeyProp;
		if(uint8* Entry =  Map->FindValue(
			KeyPtr, 
			MapLayout,
			[LocalKeyPropForCapture](const void* ElementKey) { return LocalKeyPropForCapture->GetValueTypeHash(ElementKey); },
			[LocalKeyPropForCapture](const void* A, const void* B) { return LocalKeyPropForCapture->Identical(A, B); }
			))
		{
			int32 Idx = (Entry - (uint8*)Map->GetData(0, MapLayout)) / MapLayout.SetLayout.Size;
			RemoveAt(Idx);
			return true;
		}
		else
		{
			return false;
		}
	}

	/**
	 * Checks if a key in the map matches the specified key
	 *
	 * @param	InBaseAddress	The base address of the map
	 * @param	InKeyValue		The key to find within the map
	 *
	 * @return	True if the key is found, false otherwise
	 */
	bool HasKey(const void* InBaseAddress, const FString& InKeyValue) const
	{
		for (int32 Index = 0, ItemsLeft = Num(); ItemsLeft > 0; ++Index)
		{
			if (IsValidIndex(Index))
			{
				--ItemsLeft;

				const uint8* PairPtr = GetPairPtr(Index);
				const uint8* KeyPtr = KeyProp->ContainerPtrToValuePtr<const uint8>(PairPtr);

				FString KeyValue;
				if (KeyPtr != InBaseAddress && KeyProp->ExportText_Direct(KeyValue, KeyPtr, KeyPtr, nullptr, 0))
				{
					if ((Cast<UObjectProperty>(KeyProp) != nullptr && KeyValue.Contains(InKeyValue)) || InKeyValue == KeyValue)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	static FScriptMapHelper CreateHelperFormInnerProperties(UProperty* InKeyProperty, UProperty* InValProperty, const void *InMap)
	{
		check(InKeyProperty && InValProperty);

		FScriptMapHelper ScriptMapHelper;
		ScriptMapHelper.KeyProp = InKeyProperty;
		ScriptMapHelper.ValueProp = InValProperty;
		ScriptMapHelper.Map = (FScriptMap*)InMap;
		ScriptMapHelper.MapLayout = FScriptMap::GetScriptLayout(InKeyProperty->GetSize(), InKeyProperty->GetMinAlignment(), InValProperty->GetSize(), InValProperty->GetMinAlignment());

		return ScriptMapHelper;
	}

private:
	FScriptMapHelper()
		: KeyProp(nullptr)
		, ValueProp(nullptr)
		, Map(nullptr)
		, MapLayout(FScriptMap::GetScriptLayout(0, 1, 0, 1))
	{}

	/**
	 * Internal function to call into the property system to construct / initialize elements.
	 *
	 * @param  Index  First item to construct.
	 * @param  Count  Number of items to construct.
	 */
	void ConstructItem(int32 Index)
	{
		check(IsValidIndex(Index));

		bool bZeroKey   = !!(KeyProp  ->PropertyFlags & CPF_ZeroConstructor);
		bool bZeroValue = !!(ValueProp->PropertyFlags & CPF_ZeroConstructor);

		uint8* Dest = GetPairPtrWithoutCheck(Index);

		if (bZeroKey || bZeroValue)
		{
			// If any nested property needs zeroing, just pre-zero the whole space
			FMemory::Memzero(Dest, MapLayout.SetLayout.Size);
		}

		if (!bZeroKey)
		{
			KeyProp->InitializeValue_InContainer(Dest);
		}

		if (!bZeroValue)
		{
			ValueProp->InitializeValue_InContainer(Dest);
		}
	}

	/**
	 * Internal function to call into the property system to destruct elements.
	 */
	void DestructItems(int32 Index, int32 Count)
	{
		check(Index >= 0);
		check(Count >= 0);

		if (Count == 0)
		{
			return;
		}

		bool bDestroyKeys   = !(KeyProp  ->PropertyFlags & (CPF_IsPlainOldData | CPF_NoDestructor));
		bool bDestroyValues = !(ValueProp->PropertyFlags & (CPF_IsPlainOldData | CPF_NoDestructor));

		if (bDestroyKeys || bDestroyValues)
		{
			uint32 Stride  = MapLayout.SetLayout.Size;
			uint8* PairPtr = GetPairPtrWithoutCheck(Index);
			if (bDestroyKeys)
			{
				if (bDestroyValues)
				{
					for (; Count; ++Index)
					{
						if (IsValidIndex(Index))
						{
							KeyProp  ->DestroyValue_InContainer(PairPtr);
							ValueProp->DestroyValue_InContainer(PairPtr);
							--Count;
						}
						PairPtr += Stride;
					}
				}
				else
				{
					for (; Count; ++Index)
					{
						if (IsValidIndex(Index))
						{
							KeyProp->DestroyValue_InContainer(PairPtr);
							--Count;
						}
						PairPtr += Stride;
					}
				}
			}
			else
			{
				for (; Count; ++Index)
				{
					if (IsValidIndex(Index))
					{
						ValueProp->DestroyValue_InContainer(PairPtr);
						--Count;
					}
					PairPtr += Stride;
				}
			}
		}
	}

	/**
	 * Returns a uint8 pointer to the pair in the array without checking the index.
	 *
	 * @param  Index  index of the item to return a pointer to.
	 *
	 * @return Pointer to the pair, or nullptr if the map is empty.
	 */
	FORCEINLINE uint8* GetPairPtrWithoutCheck(int32 Index)
	{
		return (uint8*)Map->GetData(Index, MapLayout);
	}

	/**
	 * Returns a uint8 pointer to the pair in the array without checking the index.
	 *
	 * @param  Index  index of the item to return a pointer to.
	 *
	 * @return Pointer to the pair, or nullptr if the map is empty.
	 */
	FORCEINLINE const uint8* GetPairPtrWithoutCheck(int32 Index) const
	{
		return const_cast<FScriptMapHelper*>(this)->GetPairPtrWithoutCheck(Index);
	}

public:
	UProperty*       KeyProp;
	UProperty*       ValueProp;
	FScriptMap*      Map;
	FScriptMapLayout MapLayout;
};

class FScriptMapHelper_InContainer : public FScriptMapHelper
{
public:
	FORCEINLINE FScriptMapHelper_InContainer(const UMapProperty* InProperty, const void* InArray, int32 FixedArrayIndex=0)
		:FScriptMapHelper(InProperty, InProperty->ContainerPtrToValuePtr<void>(InArray, FixedArrayIndex))
	{
	}
};

/**
* FScriptSetHelper: Pseudo dynamic Set. Used to work with Set properties in a sensible way.
*/
class FScriptSetHelper
{
	friend class USetProperty;

public:
	/**
	* Constructor, brings together a property and an instance of the property located in memory
	*
	* @param  InProperty  The property associated with this memory
	* @param  InSet       Pointer to raw memory that corresponds to this Set. This can be NULL, and sometimes is, but in that case almost all operations will crash.
	*/
	FORCEINLINE FScriptSetHelper(const USetProperty* InProperty, const void* InSet)
		: ElementProp(InProperty->ElementProp)
		, Set((FScriptSet*)InSet)  //@todo, we are casting away the const here
		, SetLayout(InProperty->SetLayout)
	{
		check(ElementProp);
	}

	/**
	* Index range check
	*
	* @param  Index  Index to check
	*
	* @return true if accessing this element is legal.
	*/
	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Set->IsValidIndex(Index);
	}

	/**
	* Returns the number of elements in the set.
	*
	* @return The number of elements in the set.
	*/
	FORCEINLINE int32 Num() const
	{
		const int32 Result = Set->Num();
		checkSlow(Result >= 0); 
		return Result;
	}

	/**
	* Returns the (non-inclusive) maximum index of elements in the set.
	*
	* @return The (non-inclusive) maximum index of elements in the set.
	*/
	FORCEINLINE int32 GetMaxIndex() const
	{
		const int32 Result = Set->GetMaxIndex();
		checkSlow(Result >= Num());
		return Result;
	}

	/**
	* Static version of Num() used when you don't need to bother to construct a FScriptSetHelper. Returns the number of elements in the set.
	*
	* @param  Target  Pointer to the raw memory associated with a FScriptSet
	*
	* @return The number of elements in the set.
	*/
	static FORCEINLINE int32 Num(const void* Target)
	{
		const int32 Result = ((const FScriptSet*)Target)->Num();
		checkSlow(Result >= 0); 
		return Result;
	}

	/**
	* Returns a uint8 pointer to the element in the set.
	*
	* @param  Index  index of the item to return a pointer to.
	*
	* @return Pointer to the element, or nullptr if the set is empty.
	*/
	FORCEINLINE uint8* GetElementPtr(int32 Index)
	{
		if (Num() == 0)
		{
			checkSlow(!Index);
			return nullptr;
		}

		checkSlow(IsValidIndex(Index));
		return (uint8*)Set->GetData(Index, SetLayout);
	}

	/**
	* Returns a uint8 pointer to the element in the set.
	*
	* @param  Index  index of the item to return a pointer to.
	*
	* @return Pointer to the element, or nullptr if the set is empty.
	*/
	FORCEINLINE const uint8* GetElementPtr(int32 Index) const
	{
		return const_cast<FScriptSetHelper*>(this)->GetElementPtr(Index);
	}

	/**
	* Add an uninitialized value to the end of the set.
	*
	* @return  The index of the added element.
	*/
	FORCEINLINE int32 AddUninitializedValue()
	{
		checkSlow(Num() >= 0);

		return Set->AddUninitialized(SetLayout);
	}

	/**
	*	Remove all values from the set, calling destructors, etc as appropriate.
	*	@param Slack: used to presize the set for a subsequent add, to avoid reallocation.
	**/
	void EmptyElements(int32 Slack = 0)
	{
		checkSlow(Slack >= 0);

		int32 OldNum = Num();
		if (OldNum)
		{
			DestructItems(0, OldNum);
		}
		if (OldNum || Slack)
		{
			Set->Empty(Slack, SetLayout);
		}
	}

	/**
	* Adds a blank, constructed value to a given size.
	* Note that this will create an invalid Set because all the keys will be default constructed, and the set needs rehashing.
	*
	* @return  The index of the first element added.
	**/
	int32 AddDefaultValue_Invalid_NeedsRehash()
	{
		checkSlow(Num() >= 0);

		int32 Result = AddUninitializedValue();
		ConstructItem(Result);

		return Result;
	}

	/**
	* Returns the property representing the element of the set
	*/
	UProperty* GetElementProperty() const
	{
		return ElementProp;
	}

	/**
	* Removes an element at the specified index, destroying it.
	*
	* @param  Index  The index of the element to remove.
	*/
	void RemoveAt(int32 Index, int32 Count = 1)
	{
		check(IsValidIndex(Index));

		DestructItems(Index, Count);
		for (; Count; ++Index)
		{
			if (IsValidIndex(Index))
			{
				Set->RemoveAt(Index, SetLayout);
				--Count;
			}
		}
	}

	/**
	* Rehashes the keys in the set.
	* This function must be called to create a valid set.
	*/
	COREUOBJECT_API void Rehash();

	/**
	* Finds the index of an element in a set
	*
	* @param  ElementToFind		The address of an element to search for.
	* @param  IndexHint         The index to start searching from.
	*
	* @return The index of an element found in SetHelper, or -1 if none was found.
	*/
	int32 FindElementIndex(const void* ElementToFind, int32 IndexHint = 0) const
	{
		const int32 SetMax = GetMaxIndex();
		if (SetMax == 0)
		{
			return INDEX_NONE;
		}

		check(IndexHint >= 0 && IndexHint < SetMax);

		UProperty* LocalKeyProp = this->ElementProp; // prevent aliasing in loop below

		int32 Index = IndexHint;
		for (;;)
		{
			if (IsValidIndex(Index))
			{
				const void* ElementToCheck = GetElementPtrWithoutCheck(Index);
				if (LocalKeyProp->Identical(ElementToFind, ElementToCheck))
				{
					return Index;
				}
			}

			++Index;
			if (Index == SetMax)
			{
				Index = 0;
			}

			if (Index == IndexHint)
			{
				return INDEX_NONE;
			}
		}
	}

	/**
	* Finds the pair in a map which matches the key in another pair.
	*
	* @param  PairWithKeyToFind  The address of a map pair which contains the key to search for.
	* @param  IndexHint          The index to start searching from.
	*
	* @return A pointer to the found pair, or nullptr if none was found.
	*/
	FORCEINLINE uint8* FindElementPtr(const void* ElementToFind, int32 IndexHint = 0)
	{
		const int32 Index = FindElementIndex(ElementToFind, IndexHint);
		uint8* Result = (Index >= 0 ? GetElementPtr(Index) : nullptr);
		return Result;
	}

	/** Finds element index from hash, rather than linearly searching */
	FORCEINLINE int32 FindElementIndexFromHash(const void* ElementToFind) const
	{
		UProperty* LocalElementPropForCapture = ElementProp;
		return Set->FindIndex(
			ElementToFind,
			SetLayout,
			[LocalElementPropForCapture](const void* Element) { return LocalElementPropForCapture->GetValueTypeHash(Element); },
			[LocalElementPropForCapture](const void* A, const void* B) { return LocalElementPropForCapture->Identical(A, B); }
		);
	}

	/** Adds the element to the set, returning true if the element was added, or false if the element was already present */
	void AddElement(const void* ElementToAdd)
	{
		UProperty* LocalElementPropForCapture = ElementProp;
		FScriptSetLayout& LocalSetLayoutForCapture = SetLayout;
		Set->Add(
			ElementToAdd,
			SetLayout,
			[LocalElementPropForCapture](const void* Element) { return LocalElementPropForCapture->GetValueTypeHash(Element); },
			[LocalElementPropForCapture](const void* A, const void* B) { return LocalElementPropForCapture->Identical(A, B); },
			[LocalElementPropForCapture, ElementToAdd, LocalSetLayoutForCapture](void* NewElement)
			{
				if (LocalElementPropForCapture->PropertyFlags & CPF_ZeroConstructor)
				{
					FMemory::Memzero(NewElement, LocalElementPropForCapture->GetSize());
				}
				else
				{
					LocalElementPropForCapture->InitializeValue(NewElement);
				}

				LocalElementPropForCapture->CopySingleValueToScriptVM(NewElement, ElementToAdd);
			},
			[LocalElementPropForCapture](void* Element)
			{
				if (!(LocalElementPropForCapture->PropertyFlags & (CPF_IsPlainOldData | CPF_NoDestructor)))
				{
					LocalElementPropForCapture->DestroyValue(Element);
				}
			}
		);
	}

	/** Removes the element from the set */
	bool RemoveElement(const void* ElementToRemove)
	{
		UProperty* LocalElementPropForCapture = ElementProp;
		int32 FoundIndex = Set->FindIndex(
			ElementToRemove,
			SetLayout,
			[LocalElementPropForCapture](const void* Element) { return LocalElementPropForCapture->GetValueTypeHash(Element); },
			[LocalElementPropForCapture](const void* A, const void* B) { return LocalElementPropForCapture->Identical(A, B); }
		);
		if (FoundIndex != INDEX_NONE)
		{
			RemoveAt(FoundIndex);
			return true;
		}
		else
		{
			return false;
		}
	}

	/**
	 * Checks if an element has already been added to the set
	 *
	 * @param	InBaseAddress	The base address of the set
	 * @param	InElementValue	The element value to check for
	 *
	 * @return	True if the element is found in the set, false otherwise
	 */
	bool HasElement(void* InBaseAddress, const FString& InElementValue) const
	{
		for (int32 Index = 0, ItemsLeft = Num(); ItemsLeft > 0; ++Index)
		{
			if (IsValidIndex(Index))
			{
				--ItemsLeft;

				const uint8* Element = GetElementPtr(Index);

				FString ElementValue;
				if (Element != InBaseAddress && ElementProp->ExportText_Direct(ElementValue, Element, Element, nullptr, 0))
				{
					if ((Cast<UObjectProperty>(ElementProp) != nullptr && ElementValue.Contains(InElementValue)) || ElementValue == InElementValue)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	static FScriptSetHelper CreateHelperFormElementProperty(UProperty* InElementProperty, const void *InSet)
	{
		check(InElementProperty);

		FScriptSetHelper ScriptSetHelper;
		ScriptSetHelper.ElementProp = InElementProperty;
		ScriptSetHelper.Set = (FScriptSet*)InSet;

		const int32 ElementPropSize = InElementProperty->GetSize();
		const int32 ElementPropAlignment = InElementProperty->GetMinAlignment();
		ScriptSetHelper.SetLayout = FScriptSet::GetScriptLayout(ElementPropSize, ElementPropAlignment);

		return ScriptSetHelper;
	}

private: 
	FScriptSetHelper()
		: ElementProp(nullptr)
		, Set(nullptr)
		, SetLayout(FScriptSet::GetScriptLayout(0, 1))
	{}

	/**
	* Internal function to call into the property system to construct / initialize elements.
	*
	* @param  Index  First item to construct.
	* @param  Count  Number of items to construct.
	*/
	void ConstructItem(int32 Index)
	{
		check(IsValidIndex(Index));

		bool bZeroElement = !!(ElementProp->PropertyFlags & CPF_ZeroConstructor);
		uint8* Dest = GetElementPtrWithoutCheck(Index);

		if (bZeroElement)
		{
			// If any nested property needs zeroing, just pre-zero the whole space
			FMemory::Memzero(Dest, SetLayout.Size);
		}

		if (!bZeroElement)
		{
			ElementProp->InitializeValue_InContainer(Dest);
		}
	}

	/**
	* Internal function to call into the property system to destruct elements.
	*/
	void DestructItems(int32 Index, int32 Count)
	{
		check(Index >= 0);
		check(Count >= 0);

		if (Count == 0)
		{
			return;
		}

		bool bDestroyElements = !(ElementProp->PropertyFlags & (CPF_IsPlainOldData | CPF_NoDestructor));

		if (bDestroyElements)
		{
			uint32 Stride = SetLayout.Size;
			uint8* ElementPtr = GetElementPtrWithoutCheck(Index);

			for (; Count; ++Index)
			{
				if (IsValidIndex(Index))
				{
					ElementProp->DestroyValue_InContainer(ElementPtr);
					--Count;
				}
				ElementPtr += Stride;
			}
		}
	}

	/**
	* Returns a uint8 pointer to the element in the array without checking the index.
	*
	* @param  Index  index of the item to return a pointer to.
	*
	* @return Pointer to the element, or nullptr if the array is empty.
	*/
	FORCEINLINE uint8* GetElementPtrWithoutCheck(int32 Index)
	{
		return (uint8*)Set->GetData(Index, SetLayout);
	}

	/**
	* Returns a uint8 pointer to the element in the array without checking the index.
	*
	* @param  Index  index of the item to return a pointer to.
	*
	* @return Pointer to the pair, or nullptr if the array is empty.
	*/
	FORCEINLINE const uint8* GetElementPtrWithoutCheck(int32 Index) const
	{
		return const_cast<FScriptSetHelper*>(this)->GetElementPtrWithoutCheck(Index);
	}

public:
	UProperty*       ElementProp;
	FScriptSet*      Set;
	FScriptSetLayout SetLayout;
};

class FScriptSetHelper_InContainer : public FScriptSetHelper
{
public:
	FORCEINLINE FScriptSetHelper_InContainer(const USetProperty* InProperty, const void* InArray, int32 FixedArrayIndex=0)
		:FScriptSetHelper(InProperty, InProperty->ContainerPtrToValuePtr<void>(InArray, FixedArrayIndex))
	{
	}
};

/*-----------------------------------------------------------------------------
	UStructProperty.
-----------------------------------------------------------------------------*/

//
// Describes a structure variable embedded in (as opposed to referenced by) 
// an object.
//
class COREUOBJECT_API UStructProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UStructProperty, UProperty, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UStructProperty)

	// Variables.
	class UScriptStruct* Struct;
public:
	UStructProperty(ECppProperty, int32 InOffset, uint64 InFlags, UScriptStruct* InStruct);
	UStructProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UScriptStruct* InStruct );

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	// End of UObject interface

	// UProperty interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  override;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	virtual void LinkInternal(FArchive& Ar) override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const override;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual void CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const override;
	virtual void ClearValueInternal( void* Data ) const override;
	virtual void DestroyValueInternal( void* Dest ) const override;
	virtual void InitializeValueInternal( void* Dest ) const override;
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) override;
	virtual int32 GetMinAlignment() const override;
	virtual bool ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const override;
	virtual bool ContainsWeakObjectReference() const override;
	virtual void EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps) override;
	virtual bool SameType(const UProperty* Other) const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	// End of UProperty interface

	DEPRECATED(4.14, "Use UScriptStruct::ImportText instead")
	static const TCHAR* ImportText_Static(UScriptStruct* InStruct, const FString& InName, const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText);
	
	DEPRECATED(4.14, "Use UScriptStruct::ExportText instead")
	static void ExportTextItem_Static(class UScriptStruct* InStruct, FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope);

	bool UseBinaryOrNativeSerialization(const FArchive& Ar) const;

private:
	virtual uint32 GetValueTypeHashInternal(const void* Src) const;

public:

#if HACK_HEADER_GENERATOR
	/**
	 * Some native structs, like FIntPoint, FIntRect, FVector2D, FVector, FPlane, FRotator, FCylinder have a default constructor that does nothing and require EForceInit
	 * Since it is name-based, this is not a fast routine intended to be used for header generation only
	 * 
	 * @return	true if this struct requires the EForceInit constructor to initialize
	 */
	bool HasNoOpConstructor() const;
#endif
};

/*-----------------------------------------------------------------------------
	UDelegateProperty.
-----------------------------------------------------------------------------*/

/**
 * Describes a pointer to a function bound to an Object.
 */
// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FScriptDelegate, UProperty> UDelegateProperty_Super;

class COREUOBJECT_API UDelegateProperty : public UDelegateProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UDelegateProperty, UDelegateProperty_Super, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UDelegateProperty)

	/** Points to the source delegate function (the function declared with the delegate keyword) used in the declaration of this delegate property. */
	UFunction* SignatureFunction;
public:

	typedef UDelegateProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UDelegateProperty(ECppProperty, int32 InOffset, uint64 InFlags, UFunction* InSignatureFunction = NULL)
		: UDelegateProperty_Super(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
		, SignatureFunction(InSignatureFunction)
	{
	}

	UDelegateProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UFunction* InSignatureFunction = NULL )
		: UDelegateProperty_Super( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
		, SignatureFunction(InSignatureFunction)
	{
	}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	virtual void BeginDestroy() override;
	// End of UObject interface

	// UProperty interface
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const override;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual bool ContainsWeakObjectReference() const override;
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) override;
	virtual bool SameType(const UProperty* Other) const override;
	// End of UProperty interface
};


/*-----------------------------------------------------------------------------
	UMulticastDelegateProperty.
-----------------------------------------------------------------------------*/

/**
 * Describes a pointer to a function bound to an Object.
 */
// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FMulticastScriptDelegate, UProperty> UMulticastDelegateProperty_Super;

class COREUOBJECT_API UMulticastDelegateProperty : public UMulticastDelegateProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UMulticastDelegateProperty, UMulticastDelegateProperty_Super, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UMulticastDelegateProperty)

	/** Points to the source delegate function (the function declared with the delegate keyword) used in the declaration of this delegate property. */
	UFunction* SignatureFunction;
public:

	typedef UMulticastDelegateProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UMulticastDelegateProperty(ECppProperty, int32 InOffset, uint64 InFlags, UFunction* InSignatureFunction = NULL)
		: TProperty(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
		, SignatureFunction(InSignatureFunction)
	{
	}

	UMulticastDelegateProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UFunction* InSignatureFunction = NULL )
		: TProperty( ObjectInitializer, EC_CppProperty, InOffset, InFlags )
		, SignatureFunction(InSignatureFunction)
	{
	}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	virtual void BeginDestroy() override;
	// End of UObject interface

	// UProperty interface
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const override;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual bool ContainsWeakObjectReference() const override;
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) override;
	virtual bool SameType(const UProperty* Other) const override;
	// End of UProperty interface

protected:
	friend class UProperty;

	const TCHAR* ImportText_Add( const TCHAR* Buffer, void* PropertyValue, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const;
	const TCHAR* ImportText_Remove( const TCHAR* Buffer, void* PropertyValue, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const;
};


/** Describes a single node in a custom property list. */
struct COREUOBJECT_API FCustomPropertyListNode
{
	/** The property that's being referenced at this node. */
	UProperty* Property;

	/** Used to identify which array index is specifically being referenced if this is an array property. Defaults to 0. */
	int32 ArrayIndex;

	/** If this node represents a struct property, this may contain a "sub" property list for the struct itself. */
	struct FCustomPropertyListNode* SubPropertyList;

	/** Points to the next node in the list. */
	struct FCustomPropertyListNode* PropertyListNext;

	/** Default constructor. */
	FCustomPropertyListNode(UProperty* InProperty = nullptr, int32 InArrayIndex = 0)
		:Property(InProperty)
		, ArrayIndex(InArrayIndex)
		, SubPropertyList(nullptr)
		, PropertyListNext(nullptr)
	{
	}

	/** Convenience method to return the next property in the list and advance the given ptr. */
	FORCEINLINE static UProperty* GetNextPropertyAndAdvance(const FCustomPropertyListNode*& Node)
	{
		if (Node)
		{
			Node = Node->PropertyListNext;
		}

		return Node ? Node->Property : nullptr;
	}
};


/**
 * This class represents the chain of member properties leading to an internal struct property.  It is used
 * for tracking which member property corresponds to the UScriptStruct that owns a particular property.
 */
class COREUOBJECT_API FEditPropertyChain : public TDoubleLinkedList<UProperty*>
{

public:
	/** Constructors */
	FEditPropertyChain() : ActivePropertyNode(NULL), ActiveMemberPropertyNode(NULL) {}

	/**
	 * Sets the ActivePropertyNode to the node associated with the property specified.
	 *
	 * @param	NewActiveProperty	the UProperty that is currently being evaluated by Pre/PostEditChange
	 *
	 * @return	true if the ActivePropertyNode was successfully changed to the node associated with the property
	 *			specified.  false if there was no node corresponding to that property.
	 */
	bool SetActivePropertyNode( UProperty* NewActiveProperty );

	/**
	 * Sets the ActiveMemberPropertyNode to the node associated with the property specified.
	 *
	 * @param	NewActiveMemberProperty		the member UProperty which contains the property currently being evaluated
	 *										by Pre/PostEditChange
	 *
	 * @return	true if the ActiveMemberPropertyNode was successfully changed to the node associated with the
	 *			property specified.  false if there was no node corresponding to that property.
	 */
	bool SetActiveMemberPropertyNode( UProperty* NewActiveMemberProperty );

	/**
	 * Returns the node corresponding to the currently active property.
	 */
	TDoubleLinkedListNode* GetActiveNode() const;

	/**
	 * Returns the node corresponding to the currently active property, or if the currently active property
	 * is not a member variable (i.e. inside of a struct/array), the node corresponding to the member variable
	 * which contains the currently active property.
	 */
	TDoubleLinkedListNode* GetActiveMemberNode() const;

protected:
	/**
	 * In a hierarchy of properties being edited, corresponds to the property that is currently
	 * being processed by Pre/PostEditChange
	 */
	TDoubleLinkedListNode* ActivePropertyNode;

	/**
	 * In a hierarchy of properties being edited, corresponds to the class member property which
	 * contains the property that is currently being processed by Pre/PostEditChange.  This will
	 * only be different from the ActivePropertyNode if the active property is contained within a struct,
	 * dynamic array, or static array.
	 */
	TDoubleLinkedListNode* ActiveMemberPropertyNode;


	/** TDoubleLinkedList interface */
	/**
	 * Updates the size reported by Num().  Child classes can use this function to conveniently
	 * hook into list additions/removals.
	 *
	 * This version ensures that the ActivePropertyNode and ActiveMemberPropertyNode point to a valid nodes or NULL if this list is empty.
	 *
	 * @param	NewListSize		the new size for this list
	 */
	virtual void SetListSize( int32 NewListSize );
};


//-----------------------------------------------------------------------------
//EPropertyNodeFlags - Flags used internally by property editors
//-----------------------------------------------------------------------------
namespace EPropertyChangeType
{
	typedef uint32 Type;

	//default value.  Add new enums to add new functionality.
	const Type Unspecified = 1 << 0;
	//Array Add
	const Type ArrayAdd = 1 << 1;
	//Array Remove
	const Type ArrayRemove = 1 << 2;
	//Array Clear
	const Type ArrayClear = 1 << 3;
	//Value Set
	const Type ValueSet = 1 << 4;
	//Duplicate
	const Type Duplicate = 1 << 5;
	//Interactive, e.g. dragging a slider. Will be followed by a ValueSet when finished.
	const Type Interactive = 1 << 6;
	//Redirected.  Used when property references are updated due to content hot-reloading, or an asset being replaced during asset deletion (aka, asset consolidation).
	const Type Redirected = 1 << 7;
};

/**
 * Structure for passing pre and post edit change events
 */
struct FPropertyChangedEvent
{
	//default constructor
	FPropertyChangedEvent(UProperty* InProperty)
		: Property(InProperty)
		, MemberProperty(InProperty)
		, ChangeType(EPropertyChangeType::Unspecified)
		, ObjectIteratorIndex(INDEX_NONE)
		, ArrayIndicesPerObject(nullptr)
		, InstancesChangedResultPerArchetype(nullptr)
		, TopLevelObjects(nullptr)
	{
	}

	FPropertyChangedEvent(UProperty* InProperty, EPropertyChangeType::Type InChangeType, const TArray<const UObject*>* InTopLevelObjects = nullptr )
		: Property(InProperty)
		, MemberProperty(InProperty)
		, ChangeType(InChangeType)
		, ObjectIteratorIndex(INDEX_NONE)
		, ArrayIndicesPerObject(nullptr)
		, InstancesChangedResultPerArchetype(nullptr)
		, TopLevelObjects(InTopLevelObjects)
	{
	}

	void SetActiveMemberProperty( UProperty* InActiveMemberProperty )
	{
		MemberProperty = InActiveMemberProperty;
	}

	/**
	 * Saves off map of array indices per object being set.
	 */
	void SetArrayIndexPerObject(const TArray< TMap<FString,int32> >& InArrayIndices)
	{
		ArrayIndicesPerObject = &InArrayIndices; 
	}

	/**
	 * Saves off map of instance changed result per archetype being set.
	 */
	void SetInstancesChangedResultPerArchetype(const TArray< TMap<UObject*, bool> >& InInstancesChangedResultPerArchetype)
	{
		InstancesChangedResultPerArchetype = &InInstancesChangedResultPerArchetype;
	}

	/**
	 * Gets the Array Index of the "current object" based on a particular name
	 * InName - Name of the property to find the array index for
	 */
	int32 GetArrayIndex(const FString& InName)
	{
		//default to unknown index
		int32 Retval = -1;
		if (ArrayIndicesPerObject && ArrayIndicesPerObject->IsValidIndex(ObjectIteratorIndex))
		{
			const int32* ValuePtr = (*ArrayIndicesPerObject)[ObjectIteratorIndex].Find(InName);
			if (ValuePtr)
			{
				Retval = *ValuePtr;
			}
		}
		return Retval;
	}

	/**
	 * Gets the instance changed status of the "current object" based on the instance provided
	 * InInstance - The instance we want to know the status.
	 */
	bool HasArchetypeInstanceChanged(UObject* InInstance) const
	{
		bool Retval = true;

		if (InstancesChangedResultPerArchetype && InstancesChangedResultPerArchetype->IsValidIndex(ObjectIteratorIndex))
		{
			const bool* ValuePtr = (*InstancesChangedResultPerArchetype)[ObjectIteratorIndex].Find(InInstance);
			if (ValuePtr)
			{
				Retval = *ValuePtr;
			}
		}

		return Retval;
	}

	/**
	 * @return The number of objects being edited during this change event
	 */
	int32 GetNumObjectsBeingEdited() const { return TopLevelObjects ? TopLevelObjects->Num() : 0; }

	/**
	 * Gets an object being edited by this change event.  Multiple objects could be edited at once
	 *
	 * @param Index	The index of the object being edited. Assumes index is valid.  Call GetNumObjectsBeingEdited first to check if there are valid objects
	 * @return The object being edited or nullptr if no object was found
	 */
	const UObject* GetObjectBeingEdited(int32 Index) const { return TopLevelObjects ? (*TopLevelObjects)[Index] : nullptr; }

	/**
	 * Simple utility to get the name of the property and takes care of the possible null property.
	 */
	FName GetPropertyName() const
	{
		return (Property != nullptr) ? Property->GetFName() : NAME_None;
	}

	/**
	 * The actual property that changed
	 */
	UProperty* Property;

	/**
	 * The member property of the object that PostEditChange is being called on.  
	 * For example if the property that changed is inside a struct on the object, this property is the struct property
	 */
	UProperty* MemberProperty;

	// The kind of change event that occurred
	EPropertyChangeType::Type ChangeType;

	// Used by the param system to say which object is receiving the event in the case of multi-select
	int32 ObjectIteratorIndex;
private:
	//In the property window, multiple objects can be selected at once.  In the case of adding/inserting to an array, each object COULD have different indices for the new entries in the array
	const TArray< TMap<FString,int32> >* ArrayIndicesPerObject;
	
	//In the property window, multiple objects can be selected at once. In this case we want to know if an instance was updated for this operation (used in array/set/map context)
	const TArray< TMap<UObject*, bool> >* InstancesChangedResultPerArchetype;

	/** List of top level objects being changed */
	const TArray<const UObject*>* TopLevelObjects;
};

/**
 * Structure for passing pre and post edit change events
 */
struct FPropertyChangedChainEvent : public FPropertyChangedEvent
{
	FPropertyChangedChainEvent(FEditPropertyChain& InPropertyChain, FPropertyChangedEvent& SrcChangeEvent) :
		FPropertyChangedEvent(SrcChangeEvent),
		PropertyChain(InPropertyChain)
	{
	}
	FEditPropertyChain& PropertyChain;
};

/*-----------------------------------------------------------------------------
TFieldIterator.
-----------------------------------------------------------------------------*/

/** TFieldIterator construction flags */
namespace EFieldIteratorFlags
{
	enum SuperClassFlags
	{
		ExcludeSuper = 0,	// Exclude super class
		IncludeSuper		// Include super class
	};

	enum DeprecatedPropertyFlags
	{
		ExcludeDeprecated = 0,	// Exclude deprecated properties
		IncludeDeprecated		// Include deprecated properties
	};

	enum InterfaceClassFlags
	{
		ExcludeInterfaces = 0,	// Exclude interfaces
		IncludeInterfaces		// Include interfaces
	};
}

//
// For iterating through a linked list of fields.
//
template <class T>
class TFieldIterator
{
private:
	/** The object being searched for the specified field */
	const UStruct* Struct;
	/** The current location in the list of fields being iterated */
	UField* Field;
	/** The index of the current interface being iterated */
	int32 InterfaceIndex;
	/** Whether to include the super class or not */
	const bool bIncludeSuper;
	/** Whether to include deprecated fields or not */
	const bool bIncludeDeprecated;
	/** Whether to include interface fields or not */
	const bool bIncludeInterface;

public:
	TFieldIterator(const UStruct*                               InStruct,
	               EFieldIteratorFlags::SuperClassFlags         InSuperClassFlags      = EFieldIteratorFlags::IncludeSuper,
	               EFieldIteratorFlags::DeprecatedPropertyFlags InDeprecatedFieldFlags = EFieldIteratorFlags::IncludeDeprecated,
	               EFieldIteratorFlags::InterfaceClassFlags     InInterfaceFieldFlags  = EFieldIteratorFlags::ExcludeInterfaces)
		: Struct            ( InStruct )
		, Field             ( InStruct ? InStruct->Children : NULL )
		, InterfaceIndex    ( -1 )
		, bIncludeSuper     ( InSuperClassFlags      == EFieldIteratorFlags::IncludeSuper )
		, bIncludeDeprecated( InDeprecatedFieldFlags == EFieldIteratorFlags::IncludeDeprecated )
		, bIncludeInterface ( InInterfaceFieldFlags  == EFieldIteratorFlags::IncludeInterfaces && InStruct && InStruct->IsA(UClass::StaticClass()) )
	{
		IterateToNext();
	}

	/** conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE explicit operator bool() const
	{ 
		return Field != NULL; 
	}
	/** inverse of the "bool" operator */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

	inline friend bool operator==(const TFieldIterator<T>& Lhs, const TFieldIterator<T>& Rhs) { return Lhs.Field == Rhs.Field; }
	inline friend bool operator!=(const TFieldIterator<T>& Lhs, const TFieldIterator<T>& Rhs) { return Lhs.Field != Rhs.Field; }

	inline void operator++()
	{
		checkSlow(Field);
		Field = Field->Next;
		IterateToNext();
	}
	inline T* operator*()
	{
		checkSlow(Field);
		return (T*)Field;
	}
	inline T* operator->()
	{
		checkSlow(Field);
		return (T*)Field;
	}
	inline const UStruct* GetStruct()
	{
		return Struct;
	}
protected:
	inline void IterateToNext()
	{
		      UField*  CurrentField  = Field;
		const UStruct* CurrentStruct = Struct;

		while (CurrentStruct)
		{
			while (CurrentField)
			{
				UClass* FieldClass = CurrentField->GetClass();

				if (FieldClass->HasAllCastFlags(T::StaticClassCastFlags()) &&
					(
						   bIncludeDeprecated
						|| !FieldClass->HasAllCastFlags(CASTCLASS_UProperty)
						|| !((UProperty*)CurrentField)->HasAllPropertyFlags(CPF_Deprecated)
					)
				)
				{
					Struct = CurrentStruct;
					Field  = CurrentField;
					return;
				}

				CurrentField = CurrentField->Next;
			}

			if (bIncludeInterface)
			{
				// We shouldn't be able to get here for non-classes
				UClass* CurrentClass = (UClass*)CurrentStruct;
				++InterfaceIndex;
				if (InterfaceIndex < CurrentClass->Interfaces.Num())
				{
					FImplementedInterface& Interface = CurrentClass->Interfaces[InterfaceIndex];
					CurrentField = Interface.Class ? Interface.Class->Children : nullptr;
					continue;
				}
			}

			if (bIncludeSuper)
			{
				CurrentStruct = CurrentStruct->GetInheritanceSuper();
				if (CurrentStruct)
				{
					CurrentField   = CurrentStruct->Children;
					InterfaceIndex = -1;
					continue;
				}
			}

			break;
		}

		Struct = CurrentStruct;
		Field  = CurrentField;
	}
};

template <typename T>
struct TFieldRange
{
	TFieldRange(const UStruct*                               InStruct,
	            EFieldIteratorFlags::SuperClassFlags         InSuperClassFlags      = EFieldIteratorFlags::IncludeSuper,
	            EFieldIteratorFlags::DeprecatedPropertyFlags InDeprecatedFieldFlags = EFieldIteratorFlags::IncludeDeprecated,
	            EFieldIteratorFlags::InterfaceClassFlags     InInterfaceFieldFlags  = EFieldIteratorFlags::ExcludeInterfaces)
		: Begin(InStruct, InSuperClassFlags, InDeprecatedFieldFlags, InInterfaceFieldFlags)
	{
	}

	friend TFieldIterator<T> begin(const TFieldRange& Range) { return Range.Begin; }
	friend TFieldIterator<T> end  (const TFieldRange& Range) { return TFieldIterator<T>(NULL); }

	TFieldIterator<T> Begin;
};

/*-----------------------------------------------------------------------------
	Field templates.
-----------------------------------------------------------------------------*/

//
// Find a typed field in a struct.
//
template <class T> T* FindField( const UStruct* Owner, FName FieldName )
{
	// We know that a "none" field won't exist in this Struct
	if( FieldName.IsNone() )
	{
		return nullptr;
	}

	// Search by comparing FNames (INTs), not strings
	for( TFieldIterator<T>It( Owner ); It; ++It )
	{
		if( It->GetFName() == FieldName )
		{
			return *It;
		}
	}

	// If we didn't find it, return no field
	return nullptr;
}

template <class T> T* FindField( const UStruct* Owner, const TCHAR* FieldName )
{
	// lookup the string name in the Name hash
	FName Name(FieldName, FNAME_Find);
	return FindField<T>(Owner, Name);
}

/**
 * Search for the named field within the specified scope, including any Outer classes; assert on failure.
 *
 * @param	Scope		the scope to search for the field in
 * @param	FieldName	the name of the field to search for.
 */
template<typename T>
T* FindFieldChecked( const UStruct* Scope, FName FieldName )
{
	if ( FieldName != NAME_None && Scope != NULL )
	{
		const UStruct* InitialScope = Scope;
		for ( ; Scope != NULL; Scope = dynamic_cast<const UStruct*>(Scope->GetOuter()) )
		{
			for ( TFieldIterator<T> It(Scope); It; ++It )
			{
				if ( It->GetFName() == FieldName )
				{
					return *It;
				}
			}
		}
	
		UE_LOG( LogType, Fatal, TEXT("Failed to find %s %s in %s"), *T::StaticClass()->GetName(), *FieldName.ToString(), *InitialScope->GetFullName() );
	}

	return NULL;
}

/*-----------------------------------------------------------------------------*
 * PropertyValueIterator
 *-----------------------------------------------------------------------------*/

/** FPropertyValueIterator construction flags */
enum class EPropertyValueIteratorFlags : uint8
{
	NoRecursion = 0,	// Don't recurse at all, only do top level properties
	FullRecursion = 1,	// Recurse into containers and structs
};

/** For recursively iterating over a UStruct to find nested UProperty pointers and values */
class FPropertyValueIterator
{
public:
	typedef TPair<const UProperty*, const void*> BasePairType;

	/** 
	 * Construct an iterator using a struct and struct value
	 *
	 * @param InPropertyClass	The UClass of the UProperty type you are looking for
	 * @param InStruct			The UClass or UScriptStruct containing properties to search for
	 * @param InStructValue		Address in memory of struct to search for property values
	 * @param InRecursionFlags	Rather to recurse into container and struct properties
	 * @param InDeprecatedPropertyFlags	Rather to iterate over deprecated properties
	 */
	FPropertyValueIterator(const UClass* InPropertyClass, const UStruct* InStruct, const void* InStructValue,
		EPropertyValueIteratorFlags						InRecursionFlags = EPropertyValueIteratorFlags::FullRecursion,
		EFieldIteratorFlags::DeprecatedPropertyFlags	InDeprecatedPropertyFlags = EFieldIteratorFlags::IncludeDeprecated)
		: PropertyClass(InPropertyClass)
		, RecursionFlags(InRecursionFlags)
		, DeprecatedPropertyFlags(InDeprecatedPropertyFlags)
		, bSkipRecursionOnce(false)
	{
		PropertyIteratorStack.Emplace(InStruct, InStructValue, InDeprecatedPropertyFlags);
		IterateToNext();
	}

	/** Invalid iterator, start with empty stack */
	FPropertyValueIterator()
		: PropertyClass(nullptr)
		, RecursionFlags(EPropertyValueIteratorFlags::FullRecursion)
		, DeprecatedPropertyFlags(EFieldIteratorFlags::IncludeDeprecated)
		, bSkipRecursionOnce(false)
	{
	}

	/** Conversion to "bool" returning true if the iterator is valid */
	FORCEINLINE explicit operator bool() const
	{
		// If nothing left in the stack, iteration is complete
		if (PropertyIteratorStack.Num() > 0)
		{
			return true;
		}
		return false;
	}

	FORCEINLINE friend bool operator==(const FPropertyValueIterator& Lhs, const FPropertyValueIterator& Rhs) 
	{
		return Lhs.PropertyIteratorStack == Rhs.PropertyIteratorStack;
	}
	
	FORCEINLINE friend bool operator!=(const FPropertyValueIterator& Lhs, const FPropertyValueIterator& Rhs)
	{
		return !(Lhs.PropertyIteratorStack == Rhs.PropertyIteratorStack);
	}

	/** Returns a TPair containing Property/Value currently being iterated */
	FORCEINLINE const BasePairType& operator*() const
	{
		const FPropertyValueStackEntry& Entry = PropertyIteratorStack.Last();
		return Entry.GetPropertyValue();
	}

	FORCEINLINE const BasePairType* operator->() const
	{
		const FPropertyValueStackEntry& Entry = PropertyIteratorStack.Last();
		return &Entry.GetPropertyValue();
	}

	/** Returns Property currently being iterated */
	FORCEINLINE const UProperty* Key() const 
	{
		return (*this)->Key; 
	}
	
	/** Returns memory address currently being iterated */
	FORCEINLINE const void* Value() const
	{
		return (*this)->Value;
	}

	/** Increments iterator */
	FORCEINLINE void operator++()
	{
		IterateToNext();
	}

	/** Call when iterating a recursive property such as Array or Struct to stop it from iterating into that property */
	FORCEINLINE void SkipRecursiveProperty()
	{
		bSkipRecursionOnce = true;
	}

	/** 
	 * Returns the full stack of properties for the property currently being iterated. This includes struct and container properties
	 *
	 * @param PropertyChain	Filled in with ordered list of Properties, with currently active property first and top parent last
	 */
	COREUOBJECT_API void GetPropertyChain(TArray<const UProperty*>& PropertyChain) const;

private:
	struct FPropertyValueStackEntry
	{
		/** Field iterator within a UStruct */
		TFieldIterator<const UProperty> FieldIterator;

		/** Address of owning UStruct */
		const void* StructValue;
		
		/** List of current root property+value pairs for the current top level UProperty */
		TArray<BasePairType> ValueArray;

		/** Current position inside ValueArray */
		int32 ValueIndex;

		FPropertyValueStackEntry(const UStruct* InStruct, const void* InValue, EFieldIteratorFlags::DeprecatedPropertyFlags InDeprecatedPropertyFlags)
			: FieldIterator(InStruct, EFieldIteratorFlags::IncludeSuper, InDeprecatedPropertyFlags, EFieldIteratorFlags::ExcludeInterfaces)
			, StructValue(InValue)
			, ValueIndex(0)
		{}

		FORCEINLINE friend bool operator==(const FPropertyValueStackEntry& Lhs, const FPropertyValueStackEntry& Rhs)
		{
			return Lhs.ValueIndex == Rhs.ValueIndex && Lhs.FieldIterator == Rhs.FieldIterator && Lhs.StructValue == Rhs.StructValue;
		}

		FORCEINLINE const BasePairType& GetPropertyValue() const
		{
			// Index has to be valid to get this far
			return ValueArray[ValueIndex];
		}
	};

	/** Internal stack, one per UStruct */
	TArray<FPropertyValueStackEntry> PropertyIteratorStack;

	/** Property type that is explicitly checked for */
	const UClass* PropertyClass;

	/** Whether to recurse into containers and StructProperties */
	const EPropertyValueIteratorFlags RecursionFlags;

	/** Inherits to child field iterator */
	const EFieldIteratorFlags::DeprecatedPropertyFlags DeprecatedPropertyFlags;

	/** If true, next iteration will skip recursing into containers/structs */
	bool bSkipRecursionOnce;

	/** Goes to the next Property/value pair. Returns true if next value is valid */
	bool NextValue(EPropertyValueIteratorFlags RecursionFlags);

	/** Iterates to next property being checked for or until reaching the end of the structure */
	COREUOBJECT_API void IterateToNext();
};

/** Templated version, will verify the property type is correct and will skip any properties that are not */
template <class T>
class TPropertyValueIterator : public FPropertyValueIterator
{
public:
	typedef TPair<T*, const void*> PairType;
	
	/** 
	 * Construct an iterator using a struct and struct value
	 *
	 * @param InStruct			The UClass or UScriptStruct containing properties to search for
	 * @param InStructValue		Address in memory of struct to search for property values
	 * @param InRecursionFlags	Rather to recurse into container and struct properties
	 * @param InDeprecatedPropertyFlags	Rather to iterate over deprecated properties
	 */
	TPropertyValueIterator(const UStruct* InStruct, const void* InStructValue,
		EPropertyValueIteratorFlags						InRecursionFlags = EPropertyValueIteratorFlags::FullRecursion,
		EFieldIteratorFlags::DeprecatedPropertyFlags	InDeprecatedPropertyFlags = EFieldIteratorFlags::IncludeDeprecated)
		: FPropertyValueIterator(T::StaticClass(), InStruct, InStructValue, InRecursionFlags, InDeprecatedPropertyFlags)
	{
	}

	/** Invalid iterator, start with empty stack */
	TPropertyValueIterator() 
		: FPropertyValueIterator()
	{
	}

	/** Returns a TPair containing Property/Value currently being iterated */
	FORCEINLINE const PairType& operator*() const
	{
		return (const PairType&)FPropertyValueIterator::operator*();
	}

	FORCEINLINE const PairType* operator->() const
	{
		return (const PairType*)FPropertyValueIterator::operator->();
	}

	/** Returns Property currently being iterated */
	FORCEINLINE T* Key() const
	{
		return (*this)->Key;
	}
};

/** Templated range to allow ranged-for syntax */
template <class T>
struct TPropertyValueRange
{
	/** 
	 * Construct a range using a struct and struct value
	 *
	 * @param InStruct			The UClass or UScriptStruct containing properties to search for
	 * @param InStructValue		Address in memory of struct to search for property values
	 * @param InRecursionFlags	Rather to recurse into container and struct properties
	 * @param InDeprecatedPropertyFlags	Rather to iterate over deprecated properties
	 */
	TPropertyValueRange(const UStruct* InStruct, const void* InStructValue,
		EPropertyValueIteratorFlags						InRecursionFlags = EPropertyValueIteratorFlags::FullRecursion,
		EFieldIteratorFlags::DeprecatedPropertyFlags	InDeprecatedPropertyFlags = EFieldIteratorFlags::IncludeDeprecated)
		: Begin(InStruct, InStructValue, InRecursionFlags, InDeprecatedPropertyFlags)
	{
	}

	friend TPropertyValueIterator<T> begin(const TPropertyValueRange& Range) { return Range.Begin; }
	friend TPropertyValueIterator<T> end(const TPropertyValueRange& Range) { return TPropertyValueIterator<T>(); }

	TPropertyValueIterator<T> Begin;
};

/**
 * Determine if this object has SomeObject in its archetype chain.
 */
inline bool UObject::IsBasedOnArchetype(  const UObject* const SomeObject ) const
{
	checkfSlow(this, TEXT("IsBasedOnArchetype() is called on a null pointer. Fix the call site."));
	if ( SomeObject != this )
	{
		for ( UObject* Template = GetArchetype(); Template; Template = Template->GetArchetype() )
		{
			if ( SomeObject == Template )
			{
				return true;
			}
		}
	}

	return false;
}


/*-----------------------------------------------------------------------------
	C++ property macros.
-----------------------------------------------------------------------------*/

static_assert(sizeof(bool) == sizeof(uint8), "Bool is not one byte.");

/** helper to calculate an array's dimensions **/
#define CPP_ARRAY_DIM(ArrayName, ClassName) \
	(sizeof(((ClassName*)0)->ArrayName) / sizeof(((ClassName*)0)->ArrayName[0]))
