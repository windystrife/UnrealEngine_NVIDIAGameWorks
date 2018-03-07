// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "PropertyHandle.h"
#include "Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_Base.h"

class IDetailLayoutBuilder;

//#include "LandscapeEditorObject.h"

/**
 * Slate widgets customizer for the landscape brushes
 */

class FLandscapeEditorDetailCustomization_AlphaBrush : public FLandscapeEditorDetailCustomization_Base
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	static bool OnAssetDraggedOver(const UObject* InObject);
	static void OnAssetDropped(UObject* InObject, TSharedRef<IPropertyHandle> PropertyHandle_AlphaTexture);
};
