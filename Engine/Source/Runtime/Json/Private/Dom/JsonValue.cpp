// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"


const TArray< TSharedPtr<FJsonValue> > FJsonValue::EMPTY_ARRAY;
const TSharedPtr<FJsonObject> FJsonValue::EMPTY_OBJECT(new FJsonObject());


double FJsonValue::AsNumber() const
{
	double Number = 0.0;

	if (!TryGetNumber(Number))
	{
		ErrorMessage(TEXT("Number"));
	}

	return Number;
}


FString FJsonValue::AsString() const 
{
	FString String;

	if (!TryGetString(String))
	{
		ErrorMessage(TEXT("String"));
	}

	return String;
}


bool FJsonValue::AsBool() const 
{
	bool Bool = false;

	if (!TryGetBool(Bool))
	{
		ErrorMessage(TEXT("Boolean")); 
	}

	return Bool;
}


const TArray< TSharedPtr<FJsonValue> >& FJsonValue::AsArray() const 
{
	const TArray<TSharedPtr<FJsonValue>> *Array = &EMPTY_ARRAY;

	if (!TryGetArray(Array))
	{
		ErrorMessage(TEXT("Array")); 
	}

	return *Array;
}


const TSharedPtr<FJsonObject>& FJsonValue::AsObject() const 
{
	const TSharedPtr<FJsonObject> *Object = &EMPTY_OBJECT;

	if (!TryGetObject(Object))
	{
		ErrorMessage(TEXT("Object"));
	}

	return *Object;
}


bool FJsonValue::TryGetNumber( int32& OutNumber ) const
{
	double Double;

	if (TryGetNumber(Double) && (Double >= INT_MIN) && (Double <= INT_MAX))
	{
		if (Double >= 0.0)
		{
			OutNumber = (int32)(Double + 0.5);
		}
		else
		{
			OutNumber = (int32)(Double - 0.5);
		}
		
		return true;
	}

	return false;
}


bool FJsonValue::TryGetNumber( uint32& OutNumber ) const
{
	double Double;

	if (TryGetNumber(Double) && (Double >= 0.0) && (Double <= UINT_MAX))
	{
		OutNumber = (uint32)(Double + 0.5);

		return true;
	}

	return false;
}


bool FJsonValue::TryGetNumber( int64& OutNumber ) const
{
	double Double;

	if (TryGetNumber(Double) && (Double >= INT64_MIN) && (Double <= INT64_MAX))
	{
		if (Double >= 0.0)
		{
			OutNumber = (int64)(Double + 0.5);
		}
		else
		{
			OutNumber = (int64)(Double - 0.5);
		}
		
		return true;
	}

	return false;
}


//static 
bool FJsonValue::CompareEqual( const FJsonValue& Lhs, const FJsonValue& Rhs )
{
	if (Lhs.Type != Rhs.Type)
	{
		return false;
	}

	switch (Lhs.Type)
	{
	case EJson::None:
	case EJson::Null:
		return true;

	case EJson::String:
		return Lhs.AsString() == Rhs.AsString();

	case EJson::Number:
		return Lhs.AsNumber() == Rhs.AsNumber();

	case EJson::Boolean:
		return Lhs.AsBool() == Rhs.AsBool();

	case EJson::Array:
		{
			const TArray< TSharedPtr<FJsonValue> >& LhsArray = Lhs.AsArray();
			const TArray< TSharedPtr<FJsonValue> >& RhsArray = Rhs.AsArray();

			if (LhsArray.Num() != RhsArray.Num())
			{
				return false;
			}

			// compare each element
			for (int32 i = 0; i < LhsArray.Num(); ++i)
			{
				if (!CompareEqual(*LhsArray[i], *RhsArray[i]))
				{
					return false;
				}
			}
		}
		return true;

	case EJson::Object:
		{
			const TSharedPtr<FJsonObject>& LhsObject = Lhs.AsObject();
			const TSharedPtr<FJsonObject>& RhsObject = Rhs.AsObject();

			if (LhsObject.IsValid() != RhsObject.IsValid())
			{
				return false;
			}

			if (LhsObject.IsValid())
			{
				if (LhsObject->Values.Num() != RhsObject->Values.Num())
				{
					return false;
				}

				// compare each element
				for (const auto& It : LhsObject->Values)
				{
					const FString& Key = It.Key;
					const TSharedPtr<FJsonValue>* RhsValue = RhsObject->Values.Find(Key);
					if (RhsValue == NULL)
					{
						// not found in both objects
						return false;
					}

					const TSharedPtr<FJsonValue>& LhsValue = It.Value;

					if (LhsValue.IsValid() != RhsValue->IsValid())
					{
						return false;
					}

					if (LhsValue.IsValid())
					{
						if (!CompareEqual(*LhsValue.Get(), *RhsValue->Get()))
						{
							return false;
						}
					}
				}
			}
		}
		return true;

	default:
		return false;
	}
}

void FJsonValue::ErrorMessage(const FString& InType) const
{
	UE_LOG(LogJson, Error, TEXT("Json Value of type '%s' used as a '%s'."), *GetType(), *InType);
}
