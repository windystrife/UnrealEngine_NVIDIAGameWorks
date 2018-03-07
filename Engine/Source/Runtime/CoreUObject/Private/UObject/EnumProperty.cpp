// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/EnumProperty.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectThreadContext.h"
#include "PropertyTag.h"
#include "Templates/ChooseClass.h"
#include "Templates/IsSigned.h"

namespace UE4EnumProperty_Private
{
	template <typename OldIntType>
	void ConvertIntToEnumProperty(FArchive& Ar, UEnumProperty* EnumProp, UNumericProperty* UnderlyingProp, UEnum* Enum, void* Obj)
	{
		OldIntType OldValue;
		Ar << OldValue;

		using LargeIntType = typename TChooseClass<TIsSigned<OldIntType>::Value, int64, uint64>::Result;

		LargeIntType NewValue = OldValue;
		if (!UnderlyingProp->CanHoldValue(NewValue) || !Enum->IsValidEnumValue(NewValue))
		{
			UE_LOG(
				LogClass,
				Warning,
				TEXT("Failed to find valid enum value '%s' for enum type '%s' when converting property '%s' during property loading - setting to '%s'"),
				*Lex::ToString(OldValue),
				*Enum->GetName(),
				*EnumProp->GetName(),
				*Enum->GetNameByValue(Enum->GetMaxEnumValue()).ToString()
			);

			NewValue = Enum->GetMaxEnumValue();
		}

		UnderlyingProp->SetIntPropertyValue(Obj, NewValue);
	}

	struct FEnumPropertyFriend
	{
		static const int32 EnumOffset = STRUCT_OFFSET(UEnumProperty, Enum);
		static const int32 UnderlyingPropOffset = STRUCT_OFFSET(UEnumProperty, UnderlyingProp);
	};
}

UEnumProperty::UEnumProperty(const FObjectInitializer& ObjectInitializer, UEnum* InEnum)
	: UProperty(ObjectInitializer)
	, Enum(InEnum)
{
	// This is expected to be set post-construction by AddCppProperty
	UnderlyingProp = nullptr;
}

UEnumProperty::UEnumProperty(const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UEnum* InEnum)
	: UProperty(ObjectInitializer, EC_CppProperty, InOffset, InFlags | CPF_HasGetValueTypeHash)
	, Enum(InEnum)
{
	// This is expected to be set post-construction by AddCppProperty
	UnderlyingProp = nullptr;
}

void UEnumProperty::AddCppProperty(UProperty* Inner)
{
	check(!UnderlyingProp);
	UnderlyingProp = CastChecked<UNumericProperty>(Inner);
	if (UnderlyingProp && UnderlyingProp->HasAnyPropertyFlags(CPF_HasGetValueTypeHash))
	{
		PropertyFlags |= CPF_HasGetValueTypeHash;
	}
}

void UEnumProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	check(UnderlyingProp);

	if (Enum && Ar.UseToResolveEnumerators())
	{
		int64 IntValue = UnderlyingProp->GetSignedIntPropertyValue(Value);
		int64 ResolvedIndex = Enum->ResolveEnumerator(Ar, IntValue);
		UnderlyingProp->SetIntPropertyValue(Value, ResolvedIndex);
		return;
	}

	// Loading
	if (Ar.IsLoading())
	{
		FName EnumValueName;
		Ar << EnumValueName;

		int64 NewEnumValue = 0;

		if (Enum)
		{
			// Make sure enum is properly populated
			if (Enum->HasAnyFlags(RF_NeedLoad))
			{
				Ar.Preload(Enum);
			}

			// There's no guarantee EnumValueName is still present in Enum, in which case Value will be set to the enum's max value.
			// On save, it will then be serialized as NAME_None.
			const int32 EnumIndex = Enum->GetIndexByName(EnumValueName, EGetByNameFlags::ErrorIfNotFound);
			if (EnumIndex == INDEX_NONE)
			{
				NewEnumValue = Enum->GetMaxEnumValue();
			}
			else
			{
				NewEnumValue = Enum->GetValueByIndex(EnumIndex);
			}
		}

		UnderlyingProp->SetIntPropertyValue(Value, NewEnumValue);
	}
	// Saving
	else if (Ar.IsSaving())
	{
		FName EnumValueName;
		if (Enum)
		{
			const int64 IntValue = UnderlyingProp->GetSignedIntPropertyValue(Value);

			if (Enum->IsValidEnumValue(IntValue))
			{
				EnumValueName = Enum->GetNameByValue(IntValue);
			}
		}

		Ar << EnumValueName;
	}
	else
	{
		UnderlyingProp->SerializeItem(Ar, Value, Defaults);
	}
}

bool UEnumProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8>* MetaData) const
{
	Ar.SerializeBits(Data, FMath::CeilLogTwo64(Enum->GetMaxEnumValue()));
	return 1;
}

void UEnumProperty::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	Ar << Enum;
	if (Enum != nullptr)
	{
		Ar.Preload(Enum);
	}
	Ar << UnderlyingProp;
	if (UnderlyingProp != nullptr)
	{
		Ar.Preload(UnderlyingProp);
	}
}

void UEnumProperty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UEnumProperty* This = CastChecked<UEnumProperty>(InThis);
	Collector.AddReferencedObject(This->Enum, This);
	Collector.AddReferencedObject(This->UnderlyingProp, This);
	Super::AddReferencedObjects(InThis, Collector);
}

FString UEnumProperty::GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags) const
{
	check(Enum);
	check(UnderlyingProp);

	const bool bNonNativeEnum = Enum->GetClass() != UEnum::StaticClass(); // cannot use RF_Native flag, because in UHT the flag is not set

	if (!Enum->CppType.IsEmpty())
	{
		return Enum->CppType;
	}

	FString EnumName = Enum->GetName();

	// This would give the wrong result if it's a namespaced type and the CppType hasn't
	// been set, but we do this here in case existing code relies on it... somehow.
	if ((CPPExportFlags & CPPF_BlueprintCppBackend) && bNonNativeEnum)
	{
		ensure(Enum->CppType.IsEmpty());
		FString Result = ::UnicodeToCPPIdentifier(EnumName, false, TEXT("E__"));
		return Result;
	}

	return EnumName;
}

void UEnumProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	check(Enum);
	check(UnderlyingProp);

	UNumericProperty* LocalUnderlyingProp = UnderlyingProp;

	if (PortFlags & PPF_ExportCpp)
	{
		const int64 ActualValue = LocalUnderlyingProp->GetSignedIntPropertyValue(PropertyValue);
		const int64 MaxValue = Enum->GetMaxEnumValue();
		const int64 GoodValue = Enum->IsValidEnumValue(ActualValue) ? ActualValue : MaxValue;
		const bool bNonNativeEnum = Enum->GetClass() != UEnum::StaticClass();
		ensure(!bNonNativeEnum || Enum->CppType.IsEmpty());
		const FString FullyQualifiedEnumName = bNonNativeEnum ? ::UnicodeToCPPIdentifier(Enum->GetName(), false, TEXT("E__"))
			: (Enum->CppType.IsEmpty() ? Enum->GetName() : Enum->CppType);
		if (GoodValue == MaxValue)
		{
			// not all native enums have Max value declared
			ValueStr += FString::Printf(TEXT("(%s)(%ull)"), *FullyQualifiedEnumName, ActualValue);
		}
		else
		{
			ValueStr += FString::Printf(TEXT("%s::%s"), *FullyQualifiedEnumName,
				*Enum->GetNameStringByValue(GoodValue));
		}
		return;
	}

	if (!(PortFlags & PPF_ConsoleVariable))
	{
		int64 Value = LocalUnderlyingProp->GetSignedIntPropertyValue(PropertyValue);

		// if the value is the max value (the autogenerated *_MAX value), export as "INVALID", unless we're exporting text for copy/paste (for copy/paste,
		// the property text value must actually match an entry in the enum's names array)
		bool bIsValid = Enum->IsValidEnumValue(Value);
		bool bIsMax = Value == Enum->GetMaxEnumValue();
		if (bIsValid && (!bIsMax || (PortFlags & PPF_Copy)))
		{
			// We do not want to export the enum text for non-display uses, localization text is very dynamic and would cause issues on import
			if (PortFlags & PPF_PropertyWindow)
			{
				ValueStr += Enum->GetDisplayNameTextByValue(Value).ToString();
			}
			else
			{
				ValueStr += Enum->GetNameStringByValue(Value);
			}
		}
		else
		{
			ValueStr += TEXT("(INVALID)");
		}
	}
	else
	{
		UnderlyingProp->ExportTextItem(ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, ExportRootScope);
	}
}

const TCHAR* UEnumProperty::ImportText_Internal(const TCHAR* InBuffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText) const
{
	check(Enum);
	check(UnderlyingProp);
	
	if (!(PortFlags & PPF_ConsoleVariable))
	{
		FString Temp;
		if (const TCHAR* Buffer = UPropertyHelpers::ReadToken(InBuffer, Temp, true))
		{
			int32 EnumIndex = Enum->GetIndexByName(*Temp);
			if (EnumIndex == INDEX_NONE && Temp.IsNumeric())
			{
				int64 EnumValue = INDEX_NONE;
				Lex::FromString(EnumValue, *Temp);
				EnumIndex = Enum->GetIndexByValue(EnumValue);
			}
			if (EnumIndex != INDEX_NONE)
			{
				UnderlyingProp->SetIntPropertyValue(Data, Enum->GetValueByIndex(EnumIndex));
				return Buffer;
			}

			// Enum could not be created from value. This indicates a bad value so
			// return null so that the caller of ImportText can generate a more meaningful
			// warning/error
			FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
			UE_LOG(LogClass, Warning, TEXT("In asset '%s', there is an enum property of type '%s' with an invalid value of '%s'"), *GetPathNameSafe(ThreadContext.SerializedObject), *Enum->GetName(), *Temp);
			return nullptr;
		}
	}

	const TCHAR* Result = UnderlyingProp->ImportText(InBuffer, Data, PortFlags, Parent, ErrorText);
	return Result;
}

FString UEnumProperty::GetCPPMacroType(FString& ExtendedTypeText) const
{
	ExtendedTypeText = Enum->GetName();
	return TEXT("ENUM");
}

FString UEnumProperty::GetCPPTypeForwardDeclaration() const
{
	check(Enum);
	check(Enum->GetCppForm() == UEnum::ECppForm::EnumClass);

	return FString::Printf(TEXT("enum class %s : %s;"), *Enum->GetName(), *UnderlyingProp->GetCPPType());
}

void UEnumProperty::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	OutDeps.Add(UnderlyingProp);
	OutDeps.Add(Enum);
}

void UEnumProperty::LinkInternal(FArchive& Ar)
{
	check(UnderlyingProp);

	Ar.Preload(UnderlyingProp);
	UnderlyingProp->Link(Ar);

	this->ElementSize = UnderlyingProp->ElementSize;
	this->PropertyFlags |= CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor;

	PropertyFlags |= (UnderlyingProp->PropertyFlags & CPF_HasGetValueTypeHash);
}

bool UEnumProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	return UnderlyingProp->Identical(A, B, PortFlags);
}

int32 UEnumProperty::GetMinAlignment() const
{
	return UnderlyingProp->GetMinAlignment();
}

bool UEnumProperty::SameType(const UProperty* Other) const
{
	return Super::SameType(Other) && static_cast<const UEnumProperty*>(Other)->Enum == Enum;
}

bool UEnumProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty)
{
	if ((Enum == nullptr) || (UnderlyingProp == nullptr))
	{
		bOutAdvanceProperty = false;
		return false;
	}

	bOutAdvanceProperty = true;

	if (Tag.Type == NAME_ByteProperty)
	{
		uint8 PreviousValue;
		if (Tag.EnumName == NAME_None)
		{
			// If we're a nested property the EnumName tag got lost. Handle this case for backward compatibility reasons
			UProperty* const PropertyOwner = Cast<UProperty>(GetOuterUField());

			if (PropertyOwner)
			{
				FPropertyTag InnerPropertyTag;
				InnerPropertyTag.Type = Tag.Type;
				InnerPropertyTag.EnumName = Enum->GetFName();
				InnerPropertyTag.ArrayIndex = 0;

				PreviousValue = (uint8)UNumericProperty::ReadEnumAsInt64(Ar, DefaultsStruct, InnerPropertyTag);
			}
			else
			{
				// a byte property gained an enum
				Ar << PreviousValue;
			}
		}
		else
		{
			// attempt to find the old enum and get the byte value from the serialized enum name
			PreviousValue = (uint8)UNumericProperty::ReadEnumAsInt64(Ar, DefaultsStruct, Tag);
		}

		// now copy the value into the object's address space
		UnderlyingProp->SetIntPropertyValue(ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex), (uint64)PreviousValue);
	}
	else if (Tag.Type == NAME_Int8Property)
	{
		UE4EnumProperty_Private::ConvertIntToEnumProperty<int8>(Ar, this, UnderlyingProp, Enum, ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex));
	}
	else if (Tag.Type == NAME_Int16Property)
	{
		UE4EnumProperty_Private::ConvertIntToEnumProperty<int16>(Ar, this, UnderlyingProp, Enum, ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex));
	}
	else if (Tag.Type == NAME_IntProperty)
	{
		UE4EnumProperty_Private::ConvertIntToEnumProperty<int32>(Ar, this, UnderlyingProp, Enum, ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex));
	}
	else if (Tag.Type == NAME_Int64Property)
	{
		UE4EnumProperty_Private::ConvertIntToEnumProperty<int64>(Ar, this, UnderlyingProp, Enum, ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex));
	}
	else if (Tag.Type == NAME_UInt16Property)
	{
		UE4EnumProperty_Private::ConvertIntToEnumProperty<uint16>(Ar, this, UnderlyingProp, Enum, ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex));
	}
	else if (Tag.Type == NAME_UInt32Property)
	{
		UE4EnumProperty_Private::ConvertIntToEnumProperty<uint32>(Ar, this, UnderlyingProp, Enum, ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex));
	}
	else if (Tag.Type == NAME_UInt64Property)
	{
		UE4EnumProperty_Private::ConvertIntToEnumProperty<uint64>(Ar, this, UnderlyingProp, Enum, ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex));
	}
	else
	{
		bOutAdvanceProperty = false;
	}

	return bOutAdvanceProperty;
}

uint32 UEnumProperty::GetValueTypeHashInternal(const void* Src) const
{
	check(UnderlyingProp);
	return UnderlyingProp->GetValueTypeHash(Src);
}


IMPLEMENT_CORE_INTRINSIC_CLASS(UEnumProperty, UProperty,
	{
		Class->EmitObjectReference(UE4EnumProperty_Private::FEnumPropertyFriend::EnumOffset, TEXT("Enum"));
		Class->EmitObjectReference(UE4EnumProperty_Private::FEnumPropertyFriend::UnderlyingPropOffset, TEXT("UnderlyingProp"));
	}
);
