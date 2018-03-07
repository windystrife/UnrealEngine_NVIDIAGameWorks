// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Customization/InterpolationParameterDetails.h"
#include "IDetailsView.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"

#define LOCTEXT_NAMESPACE "InterpolationParameterDetails"

void FInterpolationParameterDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		IDetailPropertyRow& Property = ChildBuilder.AddProperty(ChildHandle);
	}
}


#undef LOCTEXT_NAMESPACE
