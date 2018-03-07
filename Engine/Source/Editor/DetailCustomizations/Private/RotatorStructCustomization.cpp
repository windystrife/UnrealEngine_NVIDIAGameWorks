// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RotatorStructCustomization.h"
#include "UObject/UnrealType.h"


TSharedRef<IPropertyTypeCustomization> FRotatorStructCustomization::MakeInstance() 
{
	return MakeShareable(new FRotatorStructCustomization);
}


void FRotatorStructCustomization::GetSortedChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren)
{
	static const FName Roll("Roll");
	static const FName Pitch("Pitch");
	static const FName Yaw("Yaw");

	TSharedPtr< IPropertyHandle > RotatorChildren[3];

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		if (PropertyName == Roll)
		{
			RotatorChildren[0] = ChildHandle;
		}
		else if (PropertyName == Pitch)
		{
			RotatorChildren[1] = ChildHandle;
		}
		else
		{
			check(PropertyName == Yaw);
			RotatorChildren[2] = ChildHandle;
		}
	}

	OutChildren.Add(RotatorChildren[0].ToSharedRef());
	OutChildren.Add(RotatorChildren[1].ToSharedRef());
	OutChildren.Add(RotatorChildren[2].ToSharedRef());
}
