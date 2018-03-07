// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Binding/DynamicPropertyPath.h"

#define LOCTEXT_NAMESPACE "UMG"

FPropertyPathSegment::FPropertyPathSegment()
	: Name(NAME_None)
	, ArrayIndex(INDEX_NONE)
	, Struct(nullptr)
	, Field(nullptr)
{

}

FPropertyPathSegment::FPropertyPathSegment(FString SegmentName)
	: ArrayIndex(INDEX_NONE)
	, Struct(nullptr)
	, Field(nullptr)
{
	// Parse the property name and (optional) array index
	int32 ArrayPos = SegmentName.Find(TEXT("["));
	if ( ArrayPos != INDEX_NONE )
	{
		FString IndexToken = SegmentName.RightChop(ArrayPos + 1).LeftChop(1);
		ArrayIndex = FCString::Atoi(*IndexToken);

		SegmentName = SegmentName.Left(ArrayPos);
	}

	Name = FName(*SegmentName);
}

UField* FPropertyPathSegment::Resolve(UStruct* InStruct) const
{
	if ( InStruct )
	{
		// only perform the find field work if the structure where this property would resolve to
		// has changed.  If it hasn't changed, the just return the UProperty we found last time.
		if ( InStruct != Struct )
		{
			Struct = InStruct;
			Field = FindField<UField>(InStruct, Name);
		}

		return Field;
	}

	return nullptr;
}

FDynamicPropertyPath::FDynamicPropertyPath()
{
}

FDynamicPropertyPath::FDynamicPropertyPath(FString Path)
{
	FString PropertyString;
	while ( Path.Split(TEXT("."), &PropertyString, &Path) )
	{
		if ( !PropertyString.IsEmpty() )
		{
			Segments.Add(FPropertyPathSegment(PropertyString));
		}
	}

	Segments.Add(FPropertyPathSegment(Path));
}

FDynamicPropertyPath::FDynamicPropertyPath(const TArray<FString>& PropertyChain)
{
	for ( const FString& Segment : PropertyChain )
	{
		Segments.Add(FPropertyPathSegment(Segment));
	}
}

#undef LOCTEXT_NAMESPACE
