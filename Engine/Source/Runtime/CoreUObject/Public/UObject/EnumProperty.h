// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectMacros.h"

class UEnum;
class UNumericProperty;

namespace UE4EnumProperty_Private
{
	struct FEnumPropertyFriend;
}

class COREUOBJECT_API UEnumProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UEnumProperty, UProperty, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UEnumProperty)

public:
	UEnumProperty(const FObjectInitializer& ObjectInitializer, UEnum* InEnum);
	UEnumProperty(const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UEnum* InEnum);

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	// End of UObject interface

	// UField interface
	virtual void AddCppProperty(UProperty* Property) override;
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
	virtual int32 GetMinAlignment() const override;
	virtual bool SameType(const UProperty* Other) const override;
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	// End of UProperty interface

	/**
	 * Set the UEnum of this property.
	 * @note May only be called once to lazily initialize the property when using the default constructor.
	 */
	FORCEINLINE void SetEnum(UEnum* InEnum)
	{
		checkf(!Enum, TEXT("UEnumProperty enum may only be set once"));
		Enum = InEnum;
	}

	/**
	 * Returns a pointer to the UEnum of this property.
	 */
	FORCEINLINE UEnum* GetEnum() const
	{
		return Enum;
	}

	/**
	 * Returns the numeric property which represents the integral type of the enum.
	 */
	FORCEINLINE UNumericProperty* GetUnderlyingProperty() const
	{
		return UnderlyingProp;
	}

private:
	virtual uint32 GetValueTypeHashInternal(const void* Src) const override;

	friend struct UE4EnumProperty_Private::FEnumPropertyFriend;
	
#if HACK_HEADER_GENERATOR
public:
#endif
	UNumericProperty* UnderlyingProp; // The property which represents the underlying type of the enum
	UEnum* Enum; // The enum represented by this property
};
