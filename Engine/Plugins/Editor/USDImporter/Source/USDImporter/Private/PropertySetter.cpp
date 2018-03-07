// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PropertySetter.h"
#include "PropertyHelpers.h"
#include "UnrealType.h"
#include "CoreMinimal.h"
#include "Class.h"
#include "USDImporter.h"
#include "USDConversionUtils.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "USDImportPlugin"

typedef TFunction<void(void*, const FUsdAttribute&, UProperty*, int32)> FStructSetterFunction;

FUSDPropertySetter::FUSDPropertySetter(FUsdImportContext& InImportContext)
	: ImportContext(InImportContext)
{
	RegisterStructSetter(NAME_LinearColor,
		[this](void* Value, const FUsdAttribute& Attribute, UProperty* Property, int32 ArrayIndex)
		{
			FLinearColor& Color = *(FLinearColor*)Value;
			FUsdVector4Data Data;
			if (VerifyResult(Attribute.AsColor(Data, ArrayIndex), Attribute, Property))
			{
				Color = FLinearColor(Data.X, Data.Y, Data.Z, Data.W);
			}
		}
	);

	RegisterStructSetter(NAME_Color,
		[this](void* Value, const FUsdAttribute& Attribute, UProperty* Property, int32 ArrayIndex)
		{
			FColor& Color = *(FColor*)Value;
			FUsdVector4Data Data;
			if (VerifyResult(Attribute.AsColor(Data, ArrayIndex), Attribute, Property))
			{
				const bool bSRGB = true;
				Color = FLinearColor(Data.X, Data.Y, Data.Z, Data.W).ToFColor(bSRGB);
			}
		}
	);

	RegisterStructSetter(NAME_Vector2D,
		[this](void* Value, const FUsdAttribute& Attribute, UProperty* Property, int32 ArrayIndex)
		{
			FVector2D& Vec = *(FVector2D*)Value;
			FUsdVector2Data Data;
			if (VerifyResult(Attribute.AsVector2(Data, ArrayIndex), Attribute, Property))
			{
				Vec = FVector2D(Data.X, Data.Y);
			}
		}
	);

	RegisterStructSetter(NAME_Vector,
		[this](void* Value, const FUsdAttribute& Attribute, UProperty* Property, int32 ArrayIndex)
		{
			FVector& Vec = *(FVector*)Value;
			FUsdVectorData Data;
			if (VerifyResult(Attribute.AsVector3(Data, ArrayIndex), Attribute, Property))
			{
				Vec = FVector(Data.X, Data.Y, Data.Z);
			}
		}
	);

	RegisterStructSetter(NAME_Vector4,
		[this](void* Value, const FUsdAttribute& Attribute, UProperty* Property, int32 ArrayIndex)
		{
			FVector4& Vec = *(FVector4*)Value;
			FUsdVector4Data Data;
			if (VerifyResult(Attribute.AsVector4(Data, ArrayIndex), Attribute, Property))
			{
				Vec = FVector4(Data.X, Data.Y, Data.Z, Data.W);
			}
		}
	);

	RegisterStructSetter(NAME_Rotator,
		[this](void* Value, const FUsdAttribute& Attribute, UProperty* Property, int32 ArrayIndex)
		{
			FRotator& Rot = *(FRotator*)Value;
			FUsdVectorData Data;
			if (VerifyResult(Attribute.AsVector3(Data, ArrayIndex), Attribute, Property))
			{
				Rot = FRotator::MakeFromEuler(FVector(Data.X, Data.Y, Data.Z));
			}
		}
	);
}

void FUSDPropertySetter::ApplyPropertiesToActor(AActor* SpawnedActor, IUsdPrim* Prim, const FString& StartingPropertyPath)
{
	ApplyPropertiesFromUsdAttributes(Prim, SpawnedActor, StartingPropertyPath);

	// find prims that represent complicated properties 
	int32 NumChildren = Prim->GetNumChildren();
	for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		IUsdPrim* Child = Prim->GetChild(ChildIndex);

		// Children with transforms are other actors so their entire hierarchy is skipped
		if (!Child->HasTransform() || Child->IsUnrealProperty())
		{
			// Special case. The child itself is an unreal property  
			if (Child->IsUnrealProperty())
			{
				FString PropertyPath = CombinePropertyPaths(StartingPropertyPath, USDToUnreal::ConvertString(Child->GetUnrealPropertyPath()));

				ApplyPropertiesToActor(SpawnedActor, Child, PropertyPath);

				/*TArray<UProperty*> PropertyChain;
				PropertyHelpers::FPropertyAddress PropertyAddress = PropertyHelpers::FindProperty(*SpawnedActor, PropertyPath, PropertyChain);
				if (PropertyAddress.Property != nullptr && PropertyAddress.Address != nullptr)
				{
				SetFromUSDValue(PropertyAddress, Child, FUsdAttribute(), INDEX_NONE);
				}*/
			}
			else
			{
				// Look for loose properties on prims
				ApplyPropertiesToActor(SpawnedActor, Child, StartingPropertyPath);
			}
		}
	}

	SpawnedActor->PostEditChange();

}

void FUSDPropertySetter::RegisterStructSetter(FName StructName, FStructSetterFunction Function)
{
	StructToSetterMap.Add(StructName, Function);
}

void FUSDPropertySetter::ApplyPropertiesFromUsdAttributes(IUsdPrim* Prim, AActor* SpawnedActor, const FString& StartingPropertyPath)
{
	const std::vector<FUsdAttribute>& Attributes = Prim->GetUnrealPropertyAttributes();

	// For map properties.  Ignore prims set by maps
	TSet<const FUsdAttribute*> AttribsToIgnore;

	for (const FUsdAttribute& Attribute : Attributes)
	{
		if (!AttribsToIgnore.Contains(&Attribute))
		{
			const FString PropertyPath = CombinePropertyPaths(StartingPropertyPath, USDToUnreal::ConvertString(Attribute.GetUnrealPropertyPath()));

			TArray<UProperty*> PropertyChain;
			PropertyHelpers::FPropertyAddress PropertyAddress = PropertyHelpers::FindProperty(SpawnedActor, SpawnedActor->GetClass(), PropertyPath, PropertyChain, true);

			if (PropertyAddress.Property != nullptr && PropertyAddress.Address != nullptr)
			{
				SetFromUSDValue(PropertyAddress, Prim, Attribute, INDEX_NONE);

				if (PropertyAddress.Property->IsA<UMapProperty>())
				{
					// Special case for maps.  SetFromUSDValue can set props we're iterating on.  Ignore these 
					// to avoid duplicate setting
					const FUsdAttribute* Key = nullptr;
					TArray<const FUsdAttribute*> Values;
					if (FindMapKeyAndValues(Prim, Key, Values))
					{
						AttribsToIgnore.Add(Key);
						AttribsToIgnore.Append(Values);
					}
				}
			}
			else
			{
				ImportContext.AddErrorMessage(
					EMessageSeverity::Error, FText::Format(LOCTEXT("CouldNotFindProperty", "Could not find property '{0}' for prim '{1}'"),
						FText::FromString(PropertyPath),
						FText::FromString(USDToUnreal::ConvertString(Prim->GetPrimName())))
				);
			}
		}
	}
}


void FUSDPropertySetter::SetFromUSDValue(PropertyHelpers::FPropertyAddress& PropertyAddress, IUsdPrim* Prim, const FUsdAttribute& Attribute, int32 ArrayIndex)
{
	UProperty* Property = PropertyAddress.Property;
	void* PropertyValue = Property->ContainerPtrToValuePtr<void>(PropertyAddress.Address);
	if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
	{
		int ArraySize = Attribute.GetArraySize();
		if (ArraySize == INDEX_NONE)
		{
			ImportContext.AddErrorMessage(
				EMessageSeverity::Error, FText::Format(LOCTEXT("IncompatibleArrayTypes", "Tried to set ArrayProperty '{0}' from non-array USD attribute '{1}'"),
					FText::FromName(Property->GetFName()),
					FText::FromString(USDToUnreal::ConvertString(Attribute.GetAttributeName())))
			);
		}
		else
		{
			FScriptArrayHelper Helper(ArrayProperty, PropertyValue);
			if (Helper.Num() > 0)
			{
				Helper.EmptyAndAddValues(ArraySize);
			}
			else if(ArraySize > 0)
			{
				Helper.ExpandForIndex(ArraySize - 1);
			}

			for (int32 Index = 0; Index < ArraySize; ++Index)
			{
				PropertyHelpers::FPropertyAddress NewAddress;
				NewAddress.Property = ArrayProperty->Inner;
				NewAddress.Address = Helper.GetRawPtr(Index);
				SetFromUSDValue(NewAddress, Prim, Attribute, Index);
			}
		}
	}
	else if (UMapProperty* MapProperty = Cast<UMapProperty>(Property))
	{
		// Find Key Attribute and Value attributes.  Note, we dont use the attribute passed in
		const FUsdAttribute* Key = nullptr;
		TArray<const FUsdAttribute*> Values;

		bool bResult = FindMapKeyAndValues(Prim, Key, Values);

		if (bResult)
		{
			void* MapData = PropertyValue;

			FScriptMapHelper Helper(MapProperty, MapData);

			int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();

			PropertyHelpers::FPropertyAddress KeyAddress;
			KeyAddress.Property = MapProperty->KeyProp;
			KeyAddress.Address = Helper.GetKeyPtr(NewIndex);
			SetFromUSDValue(KeyAddress, Prim, *Key, INDEX_NONE);

			UProperty* ValueProperty = MapProperty->ValueProp;

			// Handle struct propertery with multiple values. This means the values represent inner properties on the struct.
			if (UStructProperty* StructProp = Cast<UStructProperty>(ValueProperty))
			{
				uint8* StructAddress = Helper.GetValuePtr(NewIndex);

				for (const FUsdAttribute* ValuePtr : Values)
				{
					const FUsdAttribute& ValueRef = *ValuePtr;
					FString PropertyPath = USDToUnreal::ConvertString(ValueRef.GetUnrealPropertyPath());
					TArray<UProperty*> ValuePropertyChain;
					PropertyHelpers::FPropertyAddress ValueAddress = PropertyHelpers::FindProperty((void*)StructAddress, StructProp->Struct, PropertyPath, ValuePropertyChain, true);

					if (PropertyAddress.Property != nullptr && PropertyAddress.Address != nullptr)
					{
						SetFromUSDValue(ValueAddress, Prim, ValueRef, INDEX_NONE);
					}
					else
					{
						ImportContext.AddErrorMessage(
							EMessageSeverity::Error, FText::Format(LOCTEXT("CouldNotFindProperty", "Could not find property '{0}' for prim '{1}'"),
								FText::FromString(PropertyPath),
								FText::FromString(USDToUnreal::ConvertString(Prim->GetPrimName())))
						);
					}
				}
			}
			else
			{
				if (Values.Num() == 1)
				{
					PropertyHelpers::FPropertyAddress ValueAddress;
					ValueAddress.Property = ValueProperty;
					ValueAddress.Address = Helper.GetKeyPtr(NewIndex);

					SetFromUSDValue(PropertyAddress, Prim, *Values[0], INDEX_NONE);
				}
				else
				{
					ImportContext.AddErrorMessage(
						EMessageSeverity::Error, FText::Format(LOCTEXT("IncompatibleMapValueProperty", "Map property value '{0}' has multiple values but is not a structure"),
							FText::FromName(MapProperty->GetFName()))
					);
				}
			}

			// Remove duplicate values.  This is not ideal as it involves a linear search through the map.
			// However it is necessary since the new key must be completely copied into a valid address first (which means making a duplicate entry in the map)
			for (int32 ExistingIndex = 0; ExistingIndex < Helper.Num(); ++ExistingIndex)
			{
				if (ExistingIndex != NewIndex && Helper.IsValidIndex(ExistingIndex))
				{
					void* ExistingAddress = Helper.GetKeyPtr(ExistingIndex);
					if (MapProperty->KeyProp->Identical(ExistingAddress, KeyAddress.Address))
					{
						// New is identical to one that already existed. Remove the new value
						Helper.RemoveAt(ExistingIndex);
					}
				}
			}

			Helper.Rehash();
		}
		else
		{
			ImportContext.AddErrorMessage(
				EMessageSeverity::Error, FText::Format(LOCTEXT("InvalidMapValueProperty", "Map property value '{0}' could not be set. Missing Key or Value"),
					FText::FromName(MapProperty->GetFName()))
			);
		}
	}
	else if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
	{
		// Look for special struct types.  Custom struct types with no setter are assumed to have fully qualified path to the inner properties of the struct
		FStructSetterFunction Func = StructToSetterMap.FindRef(StructProperty->Struct->GetFName());
		if (Func)
		{
			Func(PropertyValue, Attribute, Property, ArrayIndex);
		}
		else
		{
			// Struct has no direct way to be set
			ImportContext.AddErrorMessage(
				EMessageSeverity::Error, FText::Format(LOCTEXT("InvalidPropertyNoConversion", "Property '{0}' could not be set.  No conversion exists between Unreal Type '{1}' and USD type '{2}'"),
					FText::FromName(StructProperty->GetFName()),
					FText::FromString(StructProperty->GetCPPType(nullptr, 0)),
					FText::FromString(USDToUnreal::ConvertString(Attribute.GetTypeName())))
			);
		}
	}
	else if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		// see if we were passed a string for the enum
		const UEnum* Enum = EnumProperty->GetEnum();
		check(Enum);
		const char* String = nullptr;
		if (VerifyResult(Attribute.AsString(String, ArrayIndex), Attribute, Property))
		{
			FName Value = USDToUnreal::ConvertName(String);
			int64 IntValue = Enum->GetValueByName(Value);
			if (IntValue != INDEX_NONE)
			{
				EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(PropertyValue, IntValue);
			}
			else
			{
				ImportContext.AddErrorMessage(
					EMessageSeverity::Error, FText::Format(LOCTEXT("MissingEnumValue", "Tried to set EnumProperty '{0}' with invalid enum entry '{1}'"),
						FText::FromName(Property->GetFName()),
						FText::FromName(Value))
				);
			}
		}
	}
	else if (UNumericProperty* NumericProperty = Cast<UNumericProperty>(Property))
	{
		if (NumericProperty->IsFloatingPoint())
		{
			double Value = 0;
			if (VerifyResult(Attribute.AsDouble(Value, ArrayIndex), Attribute, Property))
			{
				NumericProperty->SetFloatingPointPropertyValue(PropertyValue, Value);
			}
		}
		else if (NumericProperty->IsInteger())
		{
			if (Attribute.IsUnsigned())
			{
				uint64_t Value = 0;
				if (VerifyResult(Attribute.AsUnsignedInt(Value, ArrayIndex), Attribute, Property))
				{
					NumericProperty->SetIntPropertyValue(PropertyValue, static_cast<uint64>(Value));
				}
			}
			else
			{
				int64_t Value = 0;
				if (VerifyResult(Attribute.AsInt(Value, ArrayIndex), Attribute, Property))
				{
					NumericProperty->SetIntPropertyValue(PropertyValue, static_cast<int64>(Value));
				}
			}
		}
	}
	else if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
	{
		bool Value = false;
		if (VerifyResult(Attribute.AsBool(Value, ArrayIndex), Attribute, Property))
		{
			BoolProperty->SetPropertyValue(PropertyValue, Value);
		}
	}
	else if (UStrProperty* StringProperty = Cast<UStrProperty>(Property))
	{
		const char* Value = nullptr;
		if (VerifyResult(Attribute.AsString(Value, ArrayIndex), Attribute, Property))
		{
			StringProperty->SetPropertyValue(PropertyValue, USDToUnreal::ConvertString(Value));
		}
	}
	else if (UNameProperty* NameProperty = Cast<UNameProperty>(Property))
	{
		const char* Value = nullptr;
		if (VerifyResult(Attribute.AsString(Value, ArrayIndex), Attribute, Property))
		{
			NameProperty->SetPropertyValue(PropertyValue, USDToUnreal::ConvertName(Value));
		}
	}
	else if (UTextProperty* TextProperty = Cast<UTextProperty>(Property))
	{
		const char* Value = nullptr;
		if (VerifyResult(Attribute.AsString(Value, ArrayIndex), Attribute, Property))
		{
			TextProperty->SetPropertyValue(PropertyValue, FText::FromString(USDToUnreal::ConvertString(Value)));
		}
	}
	else if (UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Property))
	{
		const char* Value = nullptr;
		if (VerifyResult(Attribute.AsString(Value, ArrayIndex), Attribute, Property))
		{
			UObject* Object = LoadObject<UObject>(nullptr, *USDToUnreal::ConvertString(Value), nullptr);
			if (Object)
			{
				ObjectProperty->SetObjectPropertyValue(PropertyValue, Object);
			}
			else
			{
				ImportContext.AddErrorMessage(
					EMessageSeverity::Error, FText::Format(LOCTEXT("MissingObjectPropertyValue", "Property '{0}' could not be set.  Could not find object {1}"),
						FText::FromName(Property->GetFName()),
						FText::FromString(USDToUnreal::ConvertString(Value)))
				);
			}
		}
	}
	else
	{
		// Property has no direct way to be set
		ImportContext.AddErrorMessage(
			EMessageSeverity::Error, FText::Format(LOCTEXT("InvalidPropertyNoConversion", "Property '{0}' could not be set.  No conversion exists between Unreal Type '{1}' and USD type '{2}'"),
				FText::FromName(Property->GetFName()),
				FText::FromString(Property->GetCPPType()),
				FText::FromString(USDToUnreal::ConvertString(Attribute.GetTypeName())))
		);
	}
}

bool FUSDPropertySetter::FindMapKeyAndValues(IUsdPrim* Prim, const FUsdAttribute*& OutKey, TArray<const FUsdAttribute*>& OutValues)
{
	const std::vector<FUsdAttribute>& Attributes = Prim->GetUnrealPropertyAttributes();

	bool bFoundKey = false;
	for (const FUsdAttribute& Attrib : Attributes)
	{
		if (USDToUnreal::ConvertString(Attrib.GetUnrealPropertyPath()) == TEXT("_KEY"))
		{
			OutKey = &Attrib;
			bFoundKey = true;
		}
		else
		{
			OutValues.Add(&Attrib);
		}
	}

	// A valid map has one key and at least one value.  Anything more than 1 value assumes the values are in a struct
	return bFoundKey && OutValues.Num() > 0;
}

bool FUSDPropertySetter::VerifyResult(bool bResult, const FUsdAttribute& Attribute, UProperty* Property)
{
	if (!bResult)
	{
		ImportContext.AddErrorMessage(
			EMessageSeverity::Error, FText::Format(LOCTEXT("IncompatibleType", "Could not set property '{0}'.  Unreal type '{1}' is incompatible with USD type '{2}'"),
				FText::FromName(Property->GetFName()),
				FText::FromString(Property->GetCPPType()),
				FText::FromString(USDToUnreal::ConvertString(Attribute.GetTypeName())))
		);
	}

	return bResult;
}

FString FUSDPropertySetter::CombinePropertyPaths(const FString& Path1, const FString& Path2)
{
	FString FinalPath;
	if (Path1.IsEmpty())
	{
		FinalPath = Path2;
	}
	else if (Path2.IsEmpty())
	{
		FinalPath = Path1;
	}
	else if (Path2.StartsWith(TEXT("[")) && Path2.EndsWith(TEXT("]")))
	{
		// Array path.  Dont add a "."
		FinalPath = Path1 + Path2;
	}
	else
	{
		FinalPath = Path1 + TEXT(".") + Path2;
	}

	return FinalPath;
}

#undef LOCTEXT_NAMESPACE
