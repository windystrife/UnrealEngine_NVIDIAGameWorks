// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;

class FMeshMergingSettingsObjectCustomization : public IDetailCustomization
{
public:
	~FMeshMergingSettingsObjectCustomization();

	/** Begin IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder) override;
	/** End IDetailCustomization interface */

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();
protected:
	EVisibility ArePropertiesVisible(const int32 VisibleType) const;
	bool AreMaterialPropertiesEnabled() const;
	TSharedPtr<IPropertyHandle> EnumProperty;
private:
};
