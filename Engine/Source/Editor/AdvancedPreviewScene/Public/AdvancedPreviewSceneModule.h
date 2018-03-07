// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterface.h"
#include "PropertyEditorDelegates.h"

class SWidget;
class FAdvancedPreviewScene;

class FAdvancedPreviewSceneModule : public IModuleInterface
{
public:
	/** Info about a per-instance details customization */
	struct FDetailCustomizationInfo
	{
		UStruct* Struct;
		FOnGetDetailCustomizationInstance OnGetDetailCustomizationInstance;
	};

	/** Info about a per-instance property type customization */
	struct FPropertyTypeCustomizationInfo
	{
		FName StructName;
		FOnGetPropertyTypeCustomizationInstance OnGetPropertyTypeCustomizationInstance;
	};

	// IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**  
	 * Create an advanced preview scene settings widget.
	 * 
	 * @param	InPreviewScene					The preview scene to create the widget for
	 * @param	InAdditionalSettings			Additional settings object to display in the view
	 * @param	InDetailCustomizations			Customizations to use for this details tab
	 * @param	InPropertyTypeCustomizations	Customizations to use for this details tab
	 * @return a new widget
	 */
	virtual TSharedRef<SWidget> CreateAdvancedPreviewSceneSettingsWidget(const TSharedRef<FAdvancedPreviewScene>& InPreviewScene, UObject* InAdditionalSettings = nullptr, const TArray<FDetailCustomizationInfo>& InDetailCustomizations = TArray<FDetailCustomizationInfo>(), const TArray<FPropertyTypeCustomizationInfo>& InPropertyTypeCustomizations = TArray<FPropertyTypeCustomizationInfo>());
};