// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/KismetArrayLibrary.h"
#include "GameFramework/Actor.h"
#include "Blueprint/BlueprintSupport.h"

DEFINE_LOG_CATEGORY_STATIC(LogArray, Warning, All);

#define LOCTEXT_NAMESPACE "UKismetArrayLibrary"

//////////////////////////////////////////////////////////////////////////
// UKismetArrayLibrary

const FName GetOutOfBoundsWarning = FName("GetOutOfBoundsWarning");
const FName SetOutOfBoundsWarning = FName("SetOutOfBoundsWarning");
const FName InsertOutOfBoundsWarning = FName("InsertOutOfBoundsWarning");
const FName RemoveOutOfBoundsWarning = FName("RemoveOutOfBoundsWarning");
const FName ResizeArrayNegativeWarning = FName("ResizeArrayNegativeWarning");
const FName SwapElementsInArrayWarning = FName("SwapElementsInArrayWarning");

UKismetArrayLibrary::UKismetArrayLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			GetOutOfBoundsWarning,
			LOCTEXT("GetOutOfBoundsWarning", "Array read access out of bounds")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			SetOutOfBoundsWarning,
			LOCTEXT("SetOutOfBoundsWarning", "Array assignment access out of bounds")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			InsertOutOfBoundsWarning,
			LOCTEXT("InsertOutOfBoundsWarning", "Array insertion out of bounds")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			RemoveOutOfBoundsWarning,
			LOCTEXT("RemoveOutOfBoundsWarning", "Array removal out of bounds")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			ResizeArrayNegativeWarning,
			LOCTEXT("ResizeArrayNegativeWarning", "Array resized to negative size")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration(
			SwapElementsInArrayWarning,
			LOCTEXT("SwapElementsInArrayWarning", "Array swap access out of bounds")
		)
	);
}

void UKismetArrayLibrary::FilterArray(const TArray<AActor*>& TargetArray, TSubclassOf<class AActor> FilterClass, TArray<AActor*>& FilteredArray)
{
	FilteredArray.Empty();
	for (auto It = TargetArray.CreateConstIterator(); It; It++)
	{
		AActor* TargetElement = (*It);
		if (TargetElement && TargetElement->IsA(FilterClass))
		{
			FilteredArray.Add(TargetElement);
		}
	}
}

int32 UKismetArrayLibrary::GenericArray_Add(void* TargetArray, const UArrayProperty* ArrayProp, const void* NewItem)
{
	int32 NewIndex = INDEX_NONE;
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
		UProperty* InnerProp = ArrayProp->Inner;

		NewIndex = ArrayHelper.AddValue();
		InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(NewIndex), NewItem);
	}
	return NewIndex;
}

int32 UKismetArrayLibrary::GenericArray_AddUnique(void* TargetArray, const UArrayProperty* ArrayProp, const void* NewItem)
{
	int32 NewIndex = INDEX_NONE;
	if (TargetArray)
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
		UProperty* InnerProp = ArrayProp->Inner;

		if (GenericArray_Find(TargetArray, ArrayProp, NewItem) == INDEX_NONE)
		{
			NewIndex = ArrayHelper.AddValue();
			InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(NewIndex), NewItem);
		}
	}
	return NewIndex;
}

void UKismetArrayLibrary::GenericArray_Append(void* TargetArray, const UArrayProperty* TargetArrayProp, void* SourceArray, const UArrayProperty* SourceArrayProperty)
{
	if(TargetArray && SourceArray)
	{
		FScriptArrayHelper TargetArrayHelper(TargetArrayProp, TargetArray);
		FScriptArrayHelper SourceArrayHelper(SourceArrayProperty, SourceArray);

		if(SourceArrayHelper.Num() > 0)
		{
			UProperty* InnerProp = TargetArrayProp->Inner;

			int32 StartIdx = TargetArrayHelper.AddValues(SourceArrayHelper.Num());
			for(int32 x = 0; x < SourceArrayHelper.Num(); ++x, ++StartIdx)
			{
				InnerProp->CopySingleValueToScriptVM(TargetArrayHelper.GetRawPtr(StartIdx), SourceArrayHelper.GetRawPtr(x));
			}
		}
	}
}

void UKismetArrayLibrary::GenericArray_Insert(void* TargetArray, const UArrayProperty* ArrayProp, const void* NewItem, int32 Index)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
		UProperty* InnerProp = ArrayProp->Inner;

		if (ArrayHelper.IsValidIndex(Index)
			|| (Index >= 0 && Index <= ArrayHelper.Num()) )
		{
			ArrayHelper.InsertValues(Index, 1);
			InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(Index), NewItem);
		}
		else
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Attempted to insert an item into array %s out of bounds [%d/%d]!"), *ArrayProp->GetName(), Index, GetLastIndex(ArrayHelper)), ELogVerbosity::Warning, InsertOutOfBoundsWarning);
		}
	}
}

void UKismetArrayLibrary::GenericArray_Remove(void* TargetArray, const UArrayProperty* ArrayProp, int32 IndexToRemove)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
		if( ArrayHelper.IsValidIndex(IndexToRemove) )
		{
			ArrayHelper.RemoveValues(IndexToRemove, 1);
		}
		else
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Attempted to remove an item from an invalid index from array %s [%d/%d]!"), *ArrayProp->GetName(), IndexToRemove, GetLastIndex(ArrayHelper)), ELogVerbosity::Warning, RemoveOutOfBoundsWarning);
		}
	}
}


bool UKismetArrayLibrary::GenericArray_RemoveItem(void* TargetArray, const UArrayProperty* ArrayProp, const void* Item)
{
	bool bRemoved = false;

	if( TargetArray )
	{
		int32 IndexToRemove = GenericArray_Find(TargetArray, ArrayProp, Item);
		while(IndexToRemove != INDEX_NONE)
		{
			GenericArray_Remove(TargetArray, ArrayProp, IndexToRemove);
			bRemoved = true; //removed

			// See if there is another in the array
			IndexToRemove = GenericArray_Find(TargetArray, ArrayProp, Item);
		}
	}

	return bRemoved;
}

void UKismetArrayLibrary::GenericArray_Shuffle(void* TargetArray, const UArrayProperty* ArrayProp)
{
	if (TargetArray)
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
		int32 LastIndex = ArrayHelper.Num() - 1;
		for (int32 i = 0; i <= LastIndex; ++i)
		{
			int32 Index = FMath::RandRange(i, LastIndex);
			if (i != Index)
			{
				ArrayHelper.SwapValues(i, Index);
			}
		}
	}
}

void UKismetArrayLibrary::GenericArray_Clear(void* TargetArray, const UArrayProperty* ArrayProp)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

		ArrayHelper.EmptyValues();
	}
}

void UKismetArrayLibrary::GenericArray_Resize(void* TargetArray, const UArrayProperty* ArrayProp, int32 Size)
{
	if( TargetArray )
	{
		if(Size >= 0)
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
			ArrayHelper.Resize(Size);
		}
		else
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Attempted to resize an array using negative size: Array = %s, Size = %d!"), *ArrayProp->GetName(), Size), ELogVerbosity::Warning, ResizeArrayNegativeWarning);
		}
	}
}

int32 UKismetArrayLibrary::GenericArray_Length(const void* TargetArray, const UArrayProperty* ArrayProp)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

		return ArrayHelper.Num();
	}
	
	return 0;
}

int32 UKismetArrayLibrary::GenericArray_LastIndex(const void* TargetArray, const UArrayProperty* ArrayProp)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

		return ArrayHelper.Num() - 1;
	}

	return INDEX_NONE;
}

void UKismetArrayLibrary::GenericArray_Get(void* TargetArray, const UArrayProperty* ArrayProp, int32 Index, void* Item)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

		UProperty* InnerProp = ArrayProp->Inner;
		if( ArrayHelper.IsValidIndex(Index) )
		{
			InnerProp->CopyCompleteValueFromScriptVM(Item, ArrayHelper.GetRawPtr(Index));	
		}
		else
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Attempted to access index %d from array '%s' of length %d in '%s'!"),
				Index,
				*ArrayProp->GetName(),
				ArrayHelper.Num(),
				*GetPathNameSafe(ArrayProp->GetOuter())),
				ELogVerbosity::Warning,
				GetOutOfBoundsWarning);
			InnerProp->InitializeValue(Item);
		}
	}
}


void UKismetArrayLibrary::GenericArray_Set(void* TargetArray, const UArrayProperty* ArrayProp, int32 Index, const void* NewItem, bool bSizeToFit)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
		UProperty* InnerProp = ArrayProp->Inner;

		// Expand the array, if desired
		if (!ArrayHelper.IsValidIndex(Index) && bSizeToFit && (Index >= 0))
		{
			ArrayHelper.ExpandForIndex(Index);
		}

		if (ArrayHelper.IsValidIndex(Index))
		{
			InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(Index), NewItem);
		}
		else
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Attempted to set an invalid index on array %s [%d/%d]!"), *ArrayProp->GetName(), Index, GetLastIndex(ArrayHelper)), ELogVerbosity::Warning, SetOutOfBoundsWarning);
		}
	}
}

void UKismetArrayLibrary::GenericArray_Swap(const void* TargetArray, const UArrayProperty* ArrayProp, int32 First, int32 Second)
{
	if (TargetArray)
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

		if (ArrayHelper.IsValidIndex(First) && ArrayHelper.IsValidIndex(Second))
		{
			// If First and Second indices are the same - nothing to do
			if (First != Second)
			{
				ArrayHelper.SwapValues(First, Second);
			}
		}
		else if (ArrayHelper.Num() == 0)
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Attempted to swap elements in empty array %s!"),
				*ArrayProp->GetName()),
				ELogVerbosity::Warning,
				SwapElementsInArrayWarning);
		}
		else
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Attempted to swap elements [%d] and [%d] in array %s. Available index range: [0 %d)!"),
				First,
				Second,
				*ArrayProp->GetName(),
				ArrayHelper.Num() - 1),
				ELogVerbosity::Warning,
				SwapElementsInArrayWarning);
		}
	}
}

int32 UKismetArrayLibrary::GenericArray_Find(const void* TargetArray, const UArrayProperty* ArrayProperty, const void* ItemToFind)
{
	int32 ResultIndex = INDEX_NONE;

	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, TargetArray);
		UProperty* InnerProp = ArrayProperty->Inner;

		// compare against each element in the array
		for (int32 Idx = 0; Idx < ArrayHelper.Num() && ResultIndex == INDEX_NONE; Idx++)
		{
			if (InnerProp->Identical(ItemToFind,ArrayHelper.GetRawPtr(Idx)))
			{
				ResultIndex = Idx;
			}
		}
	}

	// assign the resulting index
	return ResultIndex;
}

void UKismetArrayLibrary::GenericArray_SetArrayPropertyByName(UObject* OwnerObject, FName ArrayPropertyName, const void* SrcArrayAddr)
{
	if (OwnerObject != NULL)
	{
		UArrayProperty* ArrayProp = FindField<UArrayProperty>(OwnerObject->GetClass(), ArrayPropertyName);
		if (ArrayProp != NULL)
		{
			void* Dest = ArrayProp->ContainerPtrToValuePtr<void>(OwnerObject);
			ArrayProp->CopyValuesInternal(Dest, SrcArrayAddr, 1);
		}
	}
}

bool UKismetArrayLibrary::GenericArray_IsValidIndex(const void* TargetArray, const UArrayProperty* ArrayProp, int32 IndexToTest)
{
	bool ReturnBool = false;

	if (TargetArray)
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

		ReturnBool = ArrayHelper.IsValidIndex(IndexToTest);
	}

	return ReturnBool;
}

void UKismetArrayLibrary::GenericArray_HandleBool(const UProperty* Property, void* ItemPtr)
{
	const UBoolProperty* BoolProperty = Cast<const UBoolProperty>(Property);
	if (BoolProperty)
	{
		ensure((BoolProperty->ElementSize * BoolProperty->ArrayDim) == sizeof(uint8));
		BoolProperty->SetPropertyValue(ItemPtr, 0 != *(reinterpret_cast<uint8*>(ItemPtr)));
	}
}

//////////////////////////////////////////////////////////////////////////
// Stubs for the UFunctions declared as kismet callable.  These are never actually called...the CustomThunk code calls the appropriate native function with a void* reference to the array

int32 UKismetArrayLibrary::Array_Add(const TArray<int32>& TargetArray, const int32& NewItem)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
	return 0;
}

void UKismetArrayLibrary::Array_Shuffle(const TArray<int32>& TargetArray)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

void UKismetArrayLibrary::Array_Insert(const TArray<int32>& TargetArray, const int32& NewItem, int32 Index)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

void UKismetArrayLibrary::Array_Remove(const TArray<int32>& TargetArray, int32 IndexToRemove)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

bool UKismetArrayLibrary::Array_RemoveItem(const TArray<int32>& TargetArray, const int32& IndexToRemove)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
	return true;
}

void UKismetArrayLibrary::Array_Clear(const TArray<int32>& TargetArray)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

static void Array_Resize(const TArray<int32>& TargetArray, int32 Size)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

int32 UKismetArrayLibrary::Array_Length(const TArray<int32>& TargetArray)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
	return 0;
}

int32 UKismetArrayLibrary::Array_LastIndex(const TArray<int32>& TargetArray)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
	return 0;
}

void UKismetArrayLibrary::Array_Get(const TArray<int32>& TargetArray, int32 Index, int32& Item)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

void UKismetArrayLibrary::Array_Set(const TArray<int32>& TargetArray, int32 Index, const int32& NewItem, bool bSizeToFit)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

void UKismetArrayLibrary::Array_Swap(const TArray<int32>& TargetArray, int32 First, int32 Second)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

int32 UKismetArrayLibrary::Array_Find(const TArray<int32>& TargetArray, const int32& ItemToFind)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
	return 0;
}

void UKismetArrayLibrary::SetArrayPropertyByName(UObject* Object, FName PropertyName, const TArray<int32>& Value)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.
	check(0);
}

bool UKismetArrayLibrary::Array_IsValidIndex(const TArray<int32>& TargetArray, int32 IndexToTest)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.
	check(0);
	return true;
}

#undef LOCTEXT_NAMESPACE
