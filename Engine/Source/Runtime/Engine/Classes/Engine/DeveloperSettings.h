// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
class SWidget;

#include "DeveloperSettings.generated.h"

/**
 * The base class of any auto discovered settings object.
 */
UCLASS(Abstract)
class ENGINE_API UDeveloperSettings : public UObject
{
	GENERATED_BODY()

public:
	UDeveloperSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Gets the settings container name for the settings, either Project or Editor */
	virtual FName GetContainerName() const;
	/** Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc. */
	virtual FName GetCategoryName() const;
	/** The unique name for your section of settings, uses the class's FName. */
	virtual FName GetSectionName() const;

#if WITH_EDITOR
	/** Gets the section text, uses the classes DisplayName by default. */
	virtual FText GetSectionText() const;
	/** Gets the description for the section, uses the classes ToolTip by default. */
	virtual FText GetSectionDescription() const;

	/** Whether or not this class supports auto registration or if the settings have a custom setup */
	virtual bool SupportsAutoRegistration() const { return true; }
#endif

	/** Gets a custom widget for the settings.  This is only for very custom situations. */
	virtual TSharedPtr<SWidget> GetCustomSettingsWidget() const;

protected:
	/**
	 * The category name to use, overrides the one detected by looking at the config=... class metadata.
	 * Arbitrary category names are not supported, this must map to an existing category we support in the settings
	 * viewer.
	 */
	FName CategoryName;

	/** The Section name, is the short name for the settings.  If not filled in, will be the FName of the class. */
	FName SectionName;
	
#if WITH_EDITOR

	/** Populates all properties that have 'ConsoleVariable' meta data with the respective console variable values */
	void ImportConsoleVariableValues();
	/** If property has 'ConsoleVariable' meta data, exports the property value to the specified console variable */
	void ExportValuesToConsoleVariables(UProperty* PropertyThatChanged);

#endif
};
