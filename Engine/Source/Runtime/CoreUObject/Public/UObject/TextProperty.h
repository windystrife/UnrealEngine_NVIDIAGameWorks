// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FText, UProperty> UTextProperty_Super;

class COREUOBJECT_API UTextProperty : public UTextProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UTextProperty, UTextProperty_Super, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UTextProperty)

public:

	typedef UTextProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UTextProperty(ECppProperty, int32 InOffset, uint64 InFlags)
		: TProperty(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	{
	}

	UTextProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags )
		:	TProperty( ObjectInitializer, EC_CppProperty, InOffset, InFlags)
	{
	}

	// UProperty interface
	virtual bool ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty) override;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const override;
	virtual void SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const override;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const override;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const override;
	virtual FString GetCPPTypeForwardDeclaration() const override;
	// End of UProperty interface

	/** Generate the correct C++ code for the given text value */
	static FString GenerateCppCodeForTextValue(const FText& InValue, const FString& Indent);

	static bool Identical_Implementation(const FText& A, const FText& B, uint32 PortFlags);
};
