// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"

class FDetailWidgetRow;
class IDetailChildrenBuilder;
struct FVehicleTransmissionData;

class FVehicleTransmissionDataCustomization : public IPropertyTypeCustomization
{
public:
	
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() 
	{
		return MakeShareable(new FVehicleTransmissionDataCustomization);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

private:
	FVehicleTransmissionData * SelectedTransmission;

	enum EGearType
	{
		ForwardGear,
		ReverseGear,
		NeutralGear
	};

	void CreateGearUIHelper(FDetailWidgetRow& GearsSetup, FText Label, TSharedRef<IPropertyHandle> GearHandle, EGearType GearType);
	void BuildColumnsHeaderHelper(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow & GearsSetup);
	void RemoveGear(TSharedRef<IPropertyHandle> GearHandle);
	void AddGear(TSharedRef<IPropertyHandle> StructPropertyHandle);
	void EmptyGears(TSharedRef<IPropertyHandle> StructPropertyHandle);
	void CreateGearUIDelegate(TSharedRef<IPropertyHandle> PropertyHandle, int32 GearyIndex, IDetailChildrenBuilder& ChildrenBuilder);
	bool IsAutomaticEnabled() const;
};
