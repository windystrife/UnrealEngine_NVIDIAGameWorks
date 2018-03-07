// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;

/**
 * Provides the customization for all UWidgets.  Bindings, style disabling...etc.
 */
class FBlueprintWidgetCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(TSharedRef<FWidgetBlueprintEditor> InEditor, UBlueprint* InBlueprint)
	{
		return MakeShareable(new FBlueprintWidgetCustomization(InEditor, InBlueprint));
	}

	FBlueprintWidgetCustomization(TSharedRef<FWidgetBlueprintEditor> InEditor, UBlueprint* InBlueprint)
		: Editor(InEditor)
		, Blueprint(CastChecked<UWidgetBlueprint>(InBlueprint))
	{
	}
	
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

private:
	void PerformBindingCustomization(IDetailLayoutBuilder& DetailLayout);

	void CreateEventCustomization( IDetailLayoutBuilder& DetailLayout, UDelegateProperty* Property, UWidget* Widget );

	void CreateMulticastEventCustomization(IDetailLayoutBuilder& DetailLayout, FName ThisComponentName, UClass* PropertyClass, UMulticastDelegateProperty* Property);

	void ResetToDefault_RemoveBinding(TSharedPtr<IPropertyHandle> PropertyHandle);
private:

	TWeakPtr<FWidgetBlueprintEditor> Editor;

	UWidgetBlueprint* Blueprint;
};
