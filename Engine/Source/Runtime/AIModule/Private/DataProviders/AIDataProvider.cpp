// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DataProviders/AIDataProvider.h"

//////////////////////////////////////////////////////////////////////////
// FAIDataProviderValue

bool FAIDataProviderValue::IsMatchingType(UProperty* PropType) const
{
	return true;
}

void FAIDataProviderValue::GetMatchingProperties(TArray<FName>& MatchingProperties) const
{
	if (DataBinding)
	{
		for (UProperty* Prop = DataBinding->GetClass()->PropertyLink; Prop; Prop = Prop->PropertyLinkNext)
		{
			if (Prop->HasAnyPropertyFlags(CPF_Edit))
			{
				continue;
			}

			if (IsMatchingType(Prop))
			{
				MatchingProperties.Add(Prop->GetFName());
			}
		}
	}
}

template<typename T>
T* FAIDataProviderValue::GetRawValuePtr() const
{
	return CachedProperty ? CachedProperty->ContainerPtrToValuePtr<T>(DataBinding) : nullptr;
}

void FAIDataProviderValue::BindData(const UObject* Owner, int32 RequestId) const
{
	if (DataBinding && ensure(Owner))
	{
		DataBinding->BindData(*Owner, RequestId);
		CachedProperty = DataBinding->GetClass()->FindPropertyByName(DataField);
	}
}

FString FAIDataProviderValue::ToString() const
{
	return IsDynamic() ? DataBinding->ToString(DataField) : ValueToString();
}

FString FAIDataProviderValue::ValueToString() const
{
	return TEXT("??");
}

//////////////////////////////////////////////////////////////////////////
// FAIDataProviderTypedValue

bool FAIDataProviderTypedValue::IsMatchingType(UProperty* PropType) const
{
	return PropType->GetClass() == PropertyType;
}

//////////////////////////////////////////////////////////////////////////
// FAIDataProviderStructValue

bool FAIDataProviderStructValue::IsMatchingType(UProperty* PropType) const
{
	UStructProperty* StructProp = Cast<UStructProperty>(PropType);
	if (StructProp)
	{
		// skip inital "struct " 
		FString CPPType = StructProp->GetCPPType(nullptr, CPPF_None).Mid(8);
		return CPPType == StructName;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// FAIDataProviderIntValue

FAIDataProviderIntValue::FAIDataProviderIntValue()
{
	PropertyType = UIntProperty::StaticClass();
}

int32 FAIDataProviderIntValue::GetValue() const
{
	int32* PropValue = GetRawValuePtr<int32>();
	return PropValue ? *PropValue : DefaultValue;
}

FString FAIDataProviderIntValue::ValueToString() const
{
	return TTypeToString<int32>().ToSanitizedString(DefaultValue);
}

//////////////////////////////////////////////////////////////////////////
// FAIDataProviderFloatValue

FAIDataProviderFloatValue::FAIDataProviderFloatValue()
{
	PropertyType = UFloatProperty::StaticClass();
}

float FAIDataProviderFloatValue::GetValue() const
{
	float* PropValue = GetRawValuePtr<float>();
	return PropValue ? *PropValue : DefaultValue;
}

FString FAIDataProviderFloatValue::ValueToString() const
{
	return TTypeToString<float>().ToSanitizedString(DefaultValue);
}

//////////////////////////////////////////////////////////////////////////
// FAIDataProviderBoolValue

FAIDataProviderBoolValue::FAIDataProviderBoolValue()
{
	PropertyType = UBoolProperty::StaticClass();
}

bool FAIDataProviderBoolValue::GetValue() const
{
	bool* PropValue = GetRawValuePtr<bool>();
	return PropValue ? *PropValue : DefaultValue;
}

FString FAIDataProviderBoolValue::ValueToString() const
{
	return DefaultValue ? GTrue.ToString() : GFalse.ToString();
}

//////////////////////////////////////////////////////////////////////////
// UAIDataProvider

UAIDataProvider::UAIDataProvider(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UAIDataProvider::BindData(const UObject& Owner, int32 RequestId)
{
	// empty in base class
}

FString UAIDataProvider::ToString(FName PropName) const
{
	FString ProviderName = GetClass()->GetName();
	int32 SplitIdx = 0;
	const bool bFound = ProviderName.FindChar(TEXT('_'), SplitIdx);
	if (bFound)
	{
		ProviderName = ProviderName.Mid(SplitIdx + 1);
	}

	ProviderName += TEXT('.');
	ProviderName += PropName.ToString();
	return ProviderName;
}
