// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VectorStructCustomization.h"
#include "UObject/UnrealType.h"


TSharedRef<IPropertyTypeCustomization> FVectorStructCustomization::MakeInstance() 
{
	return MakeShareable(new FVectorStructCustomization);
}


void FVectorStructCustomization::GetSortedChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren)
{
	static const FName X("X");
	static const FName Y("Y");
	static const FName Z("Z");

	TSharedPtr<IPropertyHandle> VectorChildren[3];

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		if (PropertyName == X)
		{
			VectorChildren[0] = ChildHandle;
		}
		else if (PropertyName == Y)
		{
			VectorChildren[1] = ChildHandle;
		}
		else
		{
			check(PropertyName == Z);
			VectorChildren[2] = ChildHandle;
		}
	}

	OutChildren.Add(VectorChildren[0].ToSharedRef());
	OutChildren.Add(VectorChildren[1].ToSharedRef());
	OutChildren.Add(VectorChildren[2].ToSharedRef());
}
