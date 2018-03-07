// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MathStructCustomizations.h"
#include "IDetailCustomNodeBuilder.h"
#include "ColorGradingVectorCustomization.h"

/**
 * Customizes FVector4 structs.
 * We override Vector4 because the color grading controls are made with vector4
 */
class FVector4StructCustomization
	: public FMathStructCustomization
{
public:
	FVector4StructCustomization();
	~FVector4StructCustomization();
	/** @return A new instance of this class */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	//////////////////////////////////////////////////////////////////////////
	/** FMathStructCustomization interface */

	virtual void MakeHeaderRow(TSharedRef<IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;	

private:
	// We specialize the detail display of color grading vector property. The color grading mode is specified inside the metadata of the UProperty
	TSharedPtr<FColorGradingVectorCustomization> GetOrCreateColorGradingVectorCustomization(TSharedRef<IPropertyHandle>& StructPropertyHandle);

	TSharedPtr<FColorGradingVectorCustomization> ColorGradingVectorCustomization;
};

