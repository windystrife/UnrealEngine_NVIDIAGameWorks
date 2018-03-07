// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY(LogType);

bool FPropertyValueIterator::NextValue(EPropertyValueIteratorFlags InRecursionFlags)
{
	if (PropertyIteratorStack.Num() == 0)
	{
		// Stack is done, return
		return false;
	}

	FPropertyValueStackEntry& Entry = PropertyIteratorStack.Last();

	// If we have pending values, deal with them
	if (Entry.ValueIndex < Entry.ValueArray.Num())
	{
		// Look for recursion on current value first
		const UProperty* Property = Entry.ValueArray[Entry.ValueIndex].Key;
		const void* PropertyValue = Entry.ValueArray[Entry.ValueIndex].Value;

		// For containers, insert at next index ahead of others
		int32 InsertIndex = Entry.ValueIndex + 1;

		// Handle container properties
		if (const UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
		{
			if (InRecursionFlags == EPropertyValueIteratorFlags::FullRecursion)
			{
				FScriptArrayHelper Helper(ArrayProperty, PropertyValue);
				for (int32 DynamicIndex = 0; DynamicIndex < Helper.Num(); ++DynamicIndex)
				{
					Entry.ValueArray.EmplaceAt(InsertIndex++, ArrayProperty->Inner, Helper.GetRawPtr(DynamicIndex));
				}
			}
		}
		else if (const UMapProperty* MapProperty = Cast<UMapProperty>(Property))
		{
			if (InRecursionFlags == EPropertyValueIteratorFlags::FullRecursion)
			{
				FScriptMapHelper Helper(MapProperty, PropertyValue);
				for (int32 DynamicIndex = 0; DynamicIndex < Helper.Num(); ++DynamicIndex)
				{
					if (Helper.IsValidIndex(DynamicIndex))
					{
						Entry.ValueArray.EmplaceAt(InsertIndex++, MapProperty->KeyProp, Helper.GetKeyPtr(DynamicIndex));
						Entry.ValueArray.EmplaceAt(InsertIndex++, MapProperty->ValueProp, Helper.GetValuePtr(DynamicIndex));
					}
				}
			}
		}
		else if (const USetProperty* SetProperty = Cast<USetProperty>(Property))
		{
			if (InRecursionFlags == EPropertyValueIteratorFlags::FullRecursion)
			{
				FScriptSetHelper Helper(SetProperty, PropertyValue);
				for (int32 DynamicIndex = 0; DynamicIndex < Helper.Num(); ++DynamicIndex)
				{
					if (Helper.IsValidIndex(DynamicIndex))
					{
						Entry.ValueArray.EmplaceAt(InsertIndex++, SetProperty->ElementProp, Helper.GetElementPtr(DynamicIndex));
					}
				}
			}
		}
		else if (const UStructProperty* StructProperty = Cast<UStructProperty>(Property))
		{
			if (InRecursionFlags == EPropertyValueIteratorFlags::FullRecursion)
			{
				// And child to stack and rerun. This invalidates Entry. We leave ValueIndex the same so it can be used for recursive property lookup
				PropertyIteratorStack.Emplace(StructProperty->Struct, PropertyValue, DeprecatedPropertyFlags);

				return NextValue(InRecursionFlags);
			}
		}

		// Else this is a normal property and has nothing to expand
		// We don't expand enum properties because EnumProperty handles value wrapping for us

		// We didn't recurse into a struct, so increment next value to check
		Entry.ValueIndex++;
	}

	// Out of pending values, try to add more
	if (Entry.ValueIndex == Entry.ValueArray.Num())
	{
		// If Field iterator is done, pop stack and run again as Entry is invalidated
		if (!Entry.FieldIterator)
		{
			PropertyIteratorStack.Pop();

			if (PropertyIteratorStack.Num() > 0)
			{
				// Increment value index as we delayed incrementing it when entering recursion
				PropertyIteratorStack.Last().ValueIndex++;
			
				return NextValue(InRecursionFlags);
			}

			return false;
		}

		// If nothing left in value array, add base properties for current field and increase field iterator
		const UProperty* Property = *Entry.FieldIterator;
		++Entry.FieldIterator;

		// Clear out existing value array
		Entry.ValueArray.Reset();
		Entry.ValueIndex = 0;

		// Handle static arrays 
		for (int32 StaticIndex = 0; StaticIndex != Property->ArrayDim; ++StaticIndex)
		{
			const void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Entry.StructValue, StaticIndex);

			Entry.ValueArray.Emplace(Property, PropertyValue);
		}
		return true;
	}
	
	return true;
}

void FPropertyValueIterator::IterateToNext()
{
	EPropertyValueIteratorFlags LocalRecursionFlags = RecursionFlags;

	if (bSkipRecursionOnce)
	{
		LocalRecursionFlags = EPropertyValueIteratorFlags::NoRecursion;
		bSkipRecursionOnce = false;
	}

	while (NextValue(LocalRecursionFlags))
	{
		// If this property is valid type, stop iteration
		FPropertyValueStackEntry& Entry = PropertyIteratorStack.Last();
		if (Entry.GetPropertyValue().Key->IsA(PropertyClass))
		{
			return;
		}

		// Reset recursion override as we've skipped the first property
		LocalRecursionFlags = RecursionFlags;
	}
}

void FPropertyValueIterator::GetPropertyChain(TArray<const UProperty*>& PropertyChain) const
{
	// Iterate over UStruct nesting, starting at the inner most property
	for (int32 StackIndex = PropertyIteratorStack.Num() - 1; StackIndex >= 0; StackIndex--)
	{
		const FPropertyValueStackEntry& Entry = PropertyIteratorStack[StackIndex];

		// Index should always be valid
		const UProperty* Property = Entry.ValueArray[Entry.ValueIndex].Key;

		while (Property)
		{
			// This handles container property nesting
			PropertyChain.Add(Property);

			Property = Cast<UProperty>(Property->GetOuter());
		}
	}
}
