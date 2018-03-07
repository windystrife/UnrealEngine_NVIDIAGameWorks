// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StructSerializer.h"
#include "UObject/UnrealType.h"
#include "IStructSerializerBackend.h"


/* Internal helpers
 *****************************************************************************/

namespace StructSerializer
{
	/**
	 * Gets the value from the given property.
	 *
	 * @param PropertyType The type name of the property.
	 * @param State The stack state that holds the property's data.
	 * @param Property A pointer to the property.
	 * @return A pointer to the property's value, or nullptr if it couldn't be found.
	 */
	template<typename UPropertyType, typename PropertyType>
	PropertyType* GetPropertyValue( const FStructSerializerState& State, UProperty* Property )
	{
		PropertyType* ValuePtr = nullptr;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(State.ValueProperty);

		if (ArrayProperty)
		{
			check(ArrayProperty->Inner == Property);

			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->template ContainerPtrToValuePtr<void>(State.ValueData));
			int32 Index = ArrayHelper.AddValue();
		
			ValuePtr = (PropertyType*)ArrayHelper.GetRawPtr( Index );
		}
		else
		{
			UPropertyType* TypedProperty = Cast<UPropertyType>(Property);
			check(TypedProperty != nullptr);

			ValuePtr = TypedProperty->template ContainerPtrToValuePtr<PropertyType>(State.ValueData);
		}

		return ValuePtr;
	}
}


/* FStructSerializer static interface
 *****************************************************************************/

void FStructSerializer::Serialize( const void* Struct, UStruct& TypeInfo, IStructSerializerBackend& Backend, const FStructSerializerPolicies& Policies )
{
	using namespace StructSerializer;

	check(Struct != nullptr);

	// initialize serialization
	TArray<FStructSerializerState> StateStack;
	{
		FStructSerializerState NewState;
		{
			NewState.HasBeenProcessed = false;
			NewState.KeyData = nullptr;
			NewState.KeyProperty = nullptr;
			NewState.ValueData = Struct;
			NewState.ValueProperty = nullptr;
			NewState.ValueType = &TypeInfo;
		}

		StateStack.Push(NewState);
	}

	// process state stack
	while (StateStack.Num() > 0)
	{
		FStructSerializerState CurrentState = StateStack.Pop(/*bAllowShrinking=*/ false);

		// structures
		if ((CurrentState.ValueProperty == nullptr) || (CurrentState.ValueType == UStructProperty::StaticClass()))
		{
			if (!CurrentState.HasBeenProcessed)
			{
				const void* ValueData = CurrentState.ValueData;

				// write object start
				if (CurrentState.ValueProperty != nullptr)
				{
					UObject* Outer = CurrentState.ValueProperty->GetOuter();

					if ((Outer == nullptr) || (Outer->GetClass() != UArrayProperty::StaticClass()))
					{
						ValueData = CurrentState.ValueProperty->ContainerPtrToValuePtr<void>(CurrentState.ValueData);
					}
				}

				Backend.BeginStructure(CurrentState);

				CurrentState.HasBeenProcessed = true;
				StateStack.Push(CurrentState);

				// serialize fields
				if (CurrentState.ValueProperty != nullptr)
				{
					UStructProperty* StructProperty = Cast<UStructProperty>(CurrentState.ValueProperty);

					if (StructProperty != nullptr)
					{
						CurrentState.ValueType = StructProperty->Struct;
					}
					else
					{
						UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(CurrentState.ValueProperty);

						if (ObjectProperty != nullptr)
						{
							CurrentState.ValueType = ObjectProperty->PropertyClass;
						}
					}
				}

				TArray<FStructSerializerState> NewStates;

				for (TFieldIterator<UProperty> It(CurrentState.ValueType, EFieldIteratorFlags::IncludeSuper); It; ++It)
				{
					// Skip property if the filter function is set and rejects it.
					if (Policies.PropertyFilter && !Policies.PropertyFilter(*It, CurrentState.ValueProperty))
					{
						continue;
					}

					FStructSerializerState NewState;
					{
						NewState.HasBeenProcessed = false;
						NewState.KeyData = nullptr;
						NewState.KeyProperty = nullptr;
						NewState.ValueData = ValueData;
						NewState.ValueProperty = *It;
						NewState.ValueType = It->GetClass();
					}

					NewStates.Add(NewState);
				}

				// push child properties on stack (in reverse order)
				for (int Index = NewStates.Num() - 1; Index >= 0; --Index)
				{
					StateStack.Push(NewStates[Index]);
				}
			}
			else
			{
				Backend.EndStructure(CurrentState);
			}
		}

		// dynamic arrays
		else if (CurrentState.ValueType == UArrayProperty::StaticClass())
		{
			if (!CurrentState.HasBeenProcessed)
			{
				Backend.BeginArray(CurrentState);

				CurrentState.HasBeenProcessed = true;
				StateStack.Push(CurrentState);

				UArrayProperty* ArrayProperty = Cast<UArrayProperty>(CurrentState.ValueProperty);
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(CurrentState.ValueData));
				UProperty* ValueProperty = ArrayProperty->Inner;

				// push elements on stack (in reverse order)
				for (int Index = ArrayHelper.Num() - 1; Index >= 0; --Index)
				{
					FStructSerializerState NewState;
					{
						NewState.HasBeenProcessed = false;
						NewState.KeyData = nullptr;
						NewState.KeyProperty = nullptr;
						NewState.ValueData = ArrayHelper.GetRawPtr(Index);
						NewState.ValueProperty = ValueProperty;
						NewState.ValueType = ValueProperty->GetClass();
					}

					StateStack.Push(NewState);
				}
			}
			else
			{
				Backend.EndArray(CurrentState);
			}
		}

		// maps
		else if (CurrentState.ValueType == UMapProperty::StaticClass())
		{
			if (!CurrentState.HasBeenProcessed)
			{
				Backend.BeginStructure(CurrentState);

				CurrentState.HasBeenProcessed = true;
				StateStack.Push(CurrentState);

				UMapProperty* MapProperty = Cast<UMapProperty>(CurrentState.ValueProperty);
				FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(CurrentState.ValueData));
				UProperty* ValueProperty = MapProperty->ValueProp;
				
				// push key-value pairs on stack (in reverse order)
				for (int Index = MapHelper.Num() - 1; Index >= 0; --Index)
				{
					uint8* PairPtr = MapHelper.GetPairPtr(Index);

					FStructSerializerState NewState;
					{
						NewState.HasBeenProcessed = false;
						NewState.KeyData = PairPtr + MapProperty->MapLayout.KeyOffset;
						NewState.KeyProperty = MapProperty->KeyProp;
						NewState.ValueData = PairPtr;
						NewState.ValueProperty = ValueProperty;
						NewState.ValueType = ValueProperty->GetClass();
					}

					StateStack.Push(NewState);
				}
			}
			else
			{
				Backend.EndStructure(CurrentState);
			}
		}

		// sets
		else if (CurrentState.ValueType == USetProperty::StaticClass())
		{
			if (!CurrentState.HasBeenProcessed)
			{
				Backend.BeginArray(CurrentState);

				CurrentState.HasBeenProcessed = true;
				StateStack.Push(CurrentState);

				USetProperty* SetProperty = CastChecked<USetProperty>(CurrentState.ValueProperty);
				FScriptSetHelper SetHelper(SetProperty, CurrentState.ValueData);
				UProperty* ValueProperty = SetProperty->ElementProp;

				// push elements on stack
				for (int Index = SetHelper.Num() - 1; Index >= 0; --Index)
				{
					FStructSerializerState NewState;
					{
						NewState.HasBeenProcessed = false;
						NewState.KeyData = nullptr;
						NewState.KeyProperty = nullptr;
						NewState.ValueData = SetHelper.GetElementPtr(Index);
						NewState.ValueProperty = ValueProperty;
						NewState.ValueType = ValueProperty->GetClass();
					}

					StateStack.Push(NewState);
				}
			}
			else
			{
				Backend.EndArray(CurrentState);
			}
		}

		// static arrays
		else if (CurrentState.ValueProperty->ArrayDim > 1)
		{
			Backend.BeginArray(CurrentState);

			for (int32 ArrayIndex = 0; ArrayIndex < CurrentState.ValueProperty->ArrayDim; ++ArrayIndex)
			{
				Backend.WriteProperty(CurrentState, ArrayIndex);
			}

			Backend.EndArray(CurrentState);
		}

		// all other properties
		else
		{
			Backend.WriteProperty(CurrentState);
		}
	}
}
