// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

class FDestructibleMeshDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:

	/** Hide any properties that aren't supported on destructible meshes */
	void HideUnsupportedProperties(IDetailLayoutBuilder &DetailBuilder);

	TArray< TWeakObjectPtr<UObject> > ObjectsCustomized;
};

