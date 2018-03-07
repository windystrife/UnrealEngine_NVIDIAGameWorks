// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StructDeserializer.h"
#include "UObject/UnrealType.h"
#include "IStructDeserializerBackend.h"
#include "UObject/PropertyPortFlags.h"


/* Internal helpers
 *****************************************************************************/

namespace StructDeserializer
{
	/**
	 * Structure for the read state stack.
	 */
	struct FReadState
	{
		/** Holds the property's current array index. */
		int32 ArrayIndex;

		/** Holds a pointer to the property's data. */
		void* Data;

		/** Holds the property's meta data. */
		UProperty* Property;

		/** Holds a pointer to the UStruct describing the data. */
		UStruct* TypeInfo;
	};


	/**
	 * Finds the class for the given stack state.
	 *
	 * @param State The stack state to find the class for.
	 * @return The class, if found.
	 */
	UStruct* FindClass( const FReadState& State )
	{
		UStruct* Class = nullptr;

		if (State.Property != nullptr)
		{
			UProperty* ParentProperty = State.Property;
			UArrayProperty* ArrayProperty = Cast<UArrayProperty>(ParentProperty);

			if (ArrayProperty != nullptr)
			{
				ParentProperty = ArrayProperty->Inner;
			}

			UStructProperty* StructProperty = Cast<UStructProperty>(ParentProperty);
			UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(ParentProperty);

			if (StructProperty != nullptr)
			{
				Class = StructProperty->Struct;
			}
			else if (ObjectProperty != nullptr)
			{
				Class = ObjectProperty->PropertyClass;
			}
		}
		else
		{
			UObject* RootObject = static_cast<UObject*>(State.Data);
			Class = RootObject->GetClass();
		}

		return Class;
	}
}


/* FStructDeserializer static interface
 *****************************************************************************/

bool FStructDeserializer::Deserialize( void* OutStruct, UStruct& TypeInfo, IStructDeserializerBackend& Backend, const FStructDeserializerPolicies& Policies )
{
	using namespace StructDeserializer;

	check(OutStruct != nullptr);

	// initialize deserialization
	FReadState CurrentState;
	{
		CurrentState.ArrayIndex = 0;
		CurrentState.Data = OutStruct;
		CurrentState.Property = nullptr;
		CurrentState.TypeInfo = &TypeInfo;
	}

	TArray<FReadState> StateStack;
	EStructDeserializerBackendTokens Token;

	// process state stack
	while (Backend.GetNextToken(Token))
	{
		FString PropertyName = Backend.GetCurrentPropertyName();

		switch (Token)
		{
		case EStructDeserializerBackendTokens::ArrayEnd:
			{
				if (StateStack.Num() == 0)
				{
					UE_LOG(LogSerialization, Verbose, TEXT("Malformed input: Found ArrayEnd without matching ArrayStart"));

					return false;
				}

				CurrentState = StateStack.Pop(/*bAllowShrinking*/ false);
			}
			break;

		case EStructDeserializerBackendTokens::ArrayStart:
			{
				FReadState NewState;

				NewState.Property = FindField<UProperty>(CurrentState.TypeInfo, *PropertyName);

				if (NewState.Property != nullptr)
				{
					// handle array property
					if (Policies.PropertyFilter && Policies.PropertyFilter(NewState.Property, CurrentState.Property))
					{
						continue;
					}

					NewState.ArrayIndex = 0;
					NewState.Data = CurrentState.Data;
					NewState.TypeInfo = FindClass(NewState);

					StateStack.Push(CurrentState);
					CurrentState = NewState;
				}
				else
				{
					// error: array property not found
					if (Policies.MissingFields != EStructDeserializerErrorPolicies::Ignore)
					{
						UE_LOG(LogSerialization, Verbose, TEXT("The array property '%s' does not exist"), *PropertyName);
					}

					if (Policies.MissingFields == EStructDeserializerErrorPolicies::Error)
					{
						return false;
					}

					Backend.SkipArray();
				}
			}
			break;

		case EStructDeserializerBackendTokens::Error:
			{
				return false;
			}

		case EStructDeserializerBackendTokens::Property:
			{
				if (PropertyName.IsEmpty())
				{
					// handle array element
					UArrayProperty* ArrayProperty = Cast<UArrayProperty>(CurrentState.Property);
					UProperty* Property = nullptr;

					if (ArrayProperty != nullptr)
					{
						// dynamic array element
						Property = ArrayProperty->Inner;
					}
					else
					{
						// static array element
						Property = CurrentState.Property;
					}

					if (Property == nullptr)
					{
						// error: no meta data for array element
						if (Policies.MissingFields != EStructDeserializerErrorPolicies::Ignore)
						{
							UE_LOG(LogSerialization, Verbose, TEXT("Failed to serialize array element %i"), CurrentState.ArrayIndex);
						}

						return false;
					}
					else if (!Backend.ReadProperty(Property, CurrentState.Property, CurrentState.Data, CurrentState.ArrayIndex))
					{
						UE_LOG(LogSerialization, Verbose, TEXT("The array element '%s[%i]' could not be read (%s)"), *PropertyName, CurrentState.ArrayIndex, *Backend.GetDebugString());
					}

					++CurrentState.ArrayIndex;
				}
				else if ((CurrentState.Property != nullptr) && (CurrentState.Property->GetClass() == UMapProperty::StaticClass()))
				{
					// handle map element
					UMapProperty* MapProperty = Cast<UMapProperty>(CurrentState.Property);
					FScriptMapHelper MapHelper(MapProperty, CurrentState.Data);
					UProperty* Property = MapProperty->ValueProp;

					int32 PairIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
					uint8* PairPtr = MapHelper.GetPairPtr(PairIndex);

					MapProperty->KeyProp->ImportText(*PropertyName, PairPtr + MapProperty->MapLayout.KeyOffset, PPF_None, nullptr);

					if (!Backend.ReadProperty(Property, CurrentState.Property, PairPtr, CurrentState.ArrayIndex))
					{
						UE_LOG(LogSerialization, Verbose, TEXT("An item in map '%s' could not be read (%s)"), *PropertyName, *Backend.GetDebugString());
					}
				}
				else if ((CurrentState.Property != nullptr) && (CurrentState.Property->GetClass() == USetProperty::StaticClass()))
				{
					// handle set element
					USetProperty* SetProperty = Cast<USetProperty>(CurrentState.Property);
					FScriptSetHelper SetHelper(SetProperty, SetProperty->ContainerPtrToValuePtr<void>(CurrentState.Data));
					UProperty* Property = SetProperty->ElementProp;

					const int32 ElementIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
					uint8* ElementPtr = SetHelper.GetElementPtr(ElementIndex);

					if (!Backend.ReadProperty(Property, CurrentState.Property, ElementPtr, CurrentState.ArrayIndex))
					{
						UE_LOG(LogSerialization, Verbose, TEXT("An item in Set '%s' could not be read (%s)"), *PropertyName, *Backend.GetDebugString());
					}
				}
				else
				{
					// handle scalar property
					UProperty* Property = FindField<UProperty>(CurrentState.TypeInfo, *PropertyName);

					if (Property != nullptr)
					{
						if (Policies.PropertyFilter && Policies.PropertyFilter(Property, CurrentState.Property))
						{
							continue;
						}

						if (!Backend.ReadProperty(Property, CurrentState.Property, CurrentState.Data, CurrentState.ArrayIndex))
						{
							UE_LOG(LogSerialization, Verbose, TEXT("The property '%s' could not be read (%s)"), *PropertyName, *Backend.GetDebugString());
						}
					}
					else
					{
						// error: scalar property not found
						if (Policies.MissingFields != EStructDeserializerErrorPolicies::Ignore)
						{
							UE_LOG(LogSerialization, Verbose, TEXT("The property '%s' does not exist"), *PropertyName);
						}

						if (Policies.MissingFields == EStructDeserializerErrorPolicies::Error)
						{
							return false;
						}
					}
				}
			}
			break;

		case EStructDeserializerBackendTokens::StructureEnd:
			{
				// rehash if value was a map
				UMapProperty* MapProperty = Cast<UMapProperty>(CurrentState.Property);
				if (MapProperty != nullptr)
				{			
					FScriptMapHelper MapHelper(MapProperty, CurrentState.Data);
					MapHelper.Rehash();
				}

				// rehash if value was a set
				USetProperty* SetProperty = Cast<USetProperty>(CurrentState.Property);
				if (SetProperty != nullptr)
				{			
					FScriptSetHelper SetHelper(SetProperty, CurrentState.Data);
					SetHelper.Rehash();
				}

				if (StateStack.Num() == 0)
				{
					return true;
				}

				CurrentState = StateStack.Pop(/*bAllowShrinking*/ false);
			}
			break;

		case EStructDeserializerBackendTokens::StructureStart:
			{
				FReadState NewState;

				if (PropertyName.IsEmpty())
				{
					// skip root structure
					if (CurrentState.Property == nullptr)
					{
						continue;
					}

					// handle struct element inside array
					UArrayProperty* ArrayProperty = Cast<UArrayProperty>(CurrentState.Property);

					if (ArrayProperty == nullptr)
					{
						UE_LOG(LogSerialization, Verbose, TEXT("Found unnamed value outside of array"));

						return false;
					}

					FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(CurrentState.Data));
					const int32 ArrayIndex = ArrayHelper.AddValue();

					NewState.Property = ArrayProperty->Inner;
					NewState.Data = ArrayHelper.GetRawPtr(ArrayIndex);
				}
				else if ((CurrentState.Property != nullptr) && (CurrentState.Property->GetClass() == UMapProperty::StaticClass()))
				{
					// handle map or struct element inside map
					UMapProperty* MapProperty = Cast<UMapProperty>(CurrentState.Property);
					FScriptMapHelper MapHelper(MapProperty, CurrentState.Data);
					int32 PairIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
					uint8* PairPtr = MapHelper.GetPairPtr(PairIndex);
					
					NewState.Data = PairPtr + MapHelper.MapLayout.ValueOffset;
					NewState.Property = MapProperty->ValueProp;

					MapProperty->KeyProp->ImportText(*PropertyName, PairPtr + MapProperty->MapLayout.KeyOffset, PPF_None, nullptr);
				}
				else if ((CurrentState.Property != nullptr) && (CurrentState.Property->GetClass() == USetProperty::StaticClass()))
				{
					// handle map or struct element inside map
					USetProperty* SetProperty = Cast<USetProperty>(CurrentState.Property);
					FScriptSetHelper SetHelper(SetProperty, CurrentState.Data);
					const int32 ElementIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
					uint8* ElementPtr = SetHelper.GetElementPtr(ElementIndex);

					NewState.Data = ElementPtr + SetHelper.SetLayout.ElementOffset;
					NewState.Property = SetProperty->ElementProp;
				}
				else
				{
					NewState.Property = FindField<UProperty>(CurrentState.TypeInfo, *PropertyName);

					if (NewState.Property == nullptr)
					{
						// error: map, set, or struct property not found
						if (Policies.MissingFields != EStructDeserializerErrorPolicies::Ignore)
						{
							UE_LOG(LogSerialization, Verbose, TEXT("Map, Set, or struct property '%s' not found"), *PropertyName);
						}

						if (Policies.MissingFields == EStructDeserializerErrorPolicies::Error)
						{
							return false;
						}
					}
					else if (NewState.Property->GetClass() == UMapProperty::StaticClass())
					{
						// handle map property
						UMapProperty* MapProperty = Cast<UMapProperty>(NewState.Property);
						NewState.Data = MapProperty->ContainerPtrToValuePtr<void>(CurrentState.Data, CurrentState.ArrayIndex);

						FScriptMapHelper MapHelper(MapProperty, NewState.Data);
						MapHelper.EmptyValues();
					}
					else if (NewState.Property->GetClass() == USetProperty::StaticClass())
					{
						// handle set property
						USetProperty* SetProperty = Cast<USetProperty>(NewState.Property);
						NewState.Data = SetProperty->ContainerPtrToValuePtr<void>(CurrentState.Data, CurrentState.ArrayIndex);

						FScriptSetHelper SetHelper(SetProperty, NewState.Data);
						SetHelper.EmptyElements();
					}
					else
					{
						// handle struct property
						NewState.Data = NewState.Property->ContainerPtrToValuePtr<void>(CurrentState.Data);
					}
				}

				if (NewState.Property != nullptr)
				{
					// skip struct property if property filter is set and rejects it
					if (Policies.PropertyFilter && !Policies.PropertyFilter(NewState.Property, CurrentState.Property))
					{
						Backend.SkipStructure();
						continue;
					}

					NewState.ArrayIndex = 0;
					NewState.TypeInfo = FindClass(NewState);

					StateStack.Push(CurrentState);
					CurrentState = NewState;
				}
				else
				{
					// error: structured property not found
					Backend.SkipStructure();

					if (Policies.MissingFields != EStructDeserializerErrorPolicies::Ignore)
					{
						UE_LOG(LogSerialization, Verbose, TEXT("Structured property '%s' not found"), *PropertyName);
					}

					if (Policies.MissingFields == EStructDeserializerErrorPolicies::Error)
					{
						return false;
					}
				}
			}

		default:

			continue;
		}
	}

	// root structure not completed
	return false;
}
