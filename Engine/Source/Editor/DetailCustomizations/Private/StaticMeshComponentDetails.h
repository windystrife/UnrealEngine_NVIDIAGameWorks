// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;

class FStaticMeshComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	/**
	 * @return Which UI to show whether or not override lightmap res is enabled
	 */
	int HandleNoticeSwitcherWidgetIndex() const;

	/**
	* @return Accessor for the static mesh's light resolution value.
	*/
	TOptional<int32> GetStaticMeshLightResValue() const;
private:
	TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
	TSharedPtr<IPropertyHandle> OverrideLightResProperty;
};
