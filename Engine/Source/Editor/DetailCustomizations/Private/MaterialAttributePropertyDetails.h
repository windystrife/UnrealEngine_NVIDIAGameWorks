// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "IDetailCustomization.h"
#include "PropertyHandle.h"

class IDetailChildrenBuilder;
class IDetailLayoutBuilder;

/**
*	Customization for material attribute get/set nodes to handle GUID-Name conversions.
*/
class FMaterialAttributePropertyDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	void OnBuildChild(TSharedRef<IPropertyHandle> ChildHandle, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder);

private:
	TArray<TPair<FString, FGuid>>	AttributeNameToIDList;
	TArray<TSharedPtr<FString>>		AttributeDisplayNameList;
};
