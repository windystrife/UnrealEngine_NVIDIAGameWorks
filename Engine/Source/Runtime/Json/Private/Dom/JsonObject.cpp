// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Dom/JsonObject.h"


void FJsonObject::SetField( const FString& FieldName, const TSharedPtr<FJsonValue>& Value )
{
	this->Values.Add(FieldName, Value);
}


void FJsonObject::RemoveField( const FString& FieldName )
{
	this->Values.Remove(FieldName);
}


double FJsonObject::GetNumberField( const FString& FieldName ) const
{
	return GetField<EJson::None>(FieldName)->AsNumber();
}


bool FJsonObject::TryGetNumberField( const FString& FieldName, double& OutNumber ) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}


bool FJsonObject::TryGetNumberField( const FString& FieldName, int32& OutNumber ) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}


bool FJsonObject::TryGetNumberField( const FString& FieldName, uint32& OutNumber ) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}


void FJsonObject::SetNumberField( const FString& FieldName, double Number )
{
	this->Values.Add(FieldName, MakeShareable(new FJsonValueNumber(Number)));
}


FString FJsonObject::GetStringField( const FString& FieldName ) const
{
	return GetField<EJson::None>(FieldName)->AsString();
}


bool FJsonObject::TryGetStringField( const FString& FieldName, FString& OutString ) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetString(OutString);
}


bool FJsonObject::TryGetStringArrayField( const FString& FieldName, TArray<FString>& OutArray ) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);

	if (!Field.IsValid())
	{
		return false;
	}

	const TArray< TSharedPtr<FJsonValue> > *Array;

	if (!Field->TryGetArray(Array))
	{
		return false;
	}

	for (int Idx = 0; Idx < Array->Num(); Idx++)
	{
		FString Element;

		if (!(*Array)[Idx]->TryGetString(Element))
		{
			return false;
		}

		OutArray.Add(Element);
	}

	return true;
}


void FJsonObject::SetStringField( const FString& FieldName, const FString& StringValue )
{
	this->Values.Add(FieldName, MakeShareable(new FJsonValueString(StringValue)));
}


bool FJsonObject::GetBoolField( const FString& FieldName ) const
{
	return GetField<EJson::None>(FieldName)->AsBool();
}


bool FJsonObject::TryGetBoolField( const FString& FieldName, bool& OutBool ) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetBool(OutBool);
}


void FJsonObject::SetBoolField( const FString& FieldName, bool InValue )
{
	this->Values.Add(FieldName, MakeShareable( new FJsonValueBoolean(InValue)));
}


const TArray<TSharedPtr<FJsonValue>>& FJsonObject::GetArrayField( const FString& FieldName ) const
{
	return GetField<EJson::Array>(FieldName)->AsArray();
}


bool FJsonObject::TryGetArrayField(const FString& FieldName, const TArray< TSharedPtr<FJsonValue> >*& OutArray) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetArray(OutArray);
}


void FJsonObject::SetArrayField( const FString& FieldName, const TArray< TSharedPtr<FJsonValue> >& Array )
{
	this->Values.Add(FieldName, MakeShareable(new FJsonValueArray(Array)));
}


const TSharedPtr<FJsonObject>& FJsonObject::GetObjectField( const FString& FieldName ) const
{
	return GetField<EJson::Object>(FieldName)->AsObject();
}


bool FJsonObject::TryGetObjectField( const FString& FieldName, const TSharedPtr<FJsonObject>*& OutObject ) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetObject(OutObject);
}


void FJsonObject::SetObjectField( const FString& FieldName, const TSharedPtr<FJsonObject>& JsonObject )
{
	if (JsonObject.IsValid())
	{
		this->Values.Add(FieldName, MakeShareable(new FJsonValueObject(JsonObject.ToSharedRef())));
	}
	else
	{
		this->Values.Add(FieldName, MakeShareable( new FJsonValueNull()));
	}
}
