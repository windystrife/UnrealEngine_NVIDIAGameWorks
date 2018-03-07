// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editor/PropertyEditor/Public/IDetailCustomization.h"
#include "SGameplayTagWidget.h"

class IDetailLayoutBuilder;

//////////////////////////////////////////////////////////////////////////
// FGameplayTagsSettingsCustomization

class FGameplayTagsSettingsCustomization : public IDetailCustomization
{
public:
	FGameplayTagsSettingsCustomization();
	virtual ~FGameplayTagsSettingsCustomization();

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:

	/** Callback for when a tag changes */
	void OnTagChanged();

	/** Module callback for when the tag tree changes */
	void OnTagTreeChanged();

	TSharedPtr<SGameplayTagWidget> TagWidget;
};
