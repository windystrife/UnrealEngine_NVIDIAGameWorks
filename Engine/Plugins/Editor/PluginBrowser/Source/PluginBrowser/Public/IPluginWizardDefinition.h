// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Views/ITypedTableView.h"
#include "ModuleDescriptor.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"


struct FSlateDynamicImageBrush;


/**
 * Description of a plugin template
 */
struct FPluginTemplateDescription
{
	/** Name of this template in the GUI */
	FText Name;

	/** Description of this template in the GUI */
	FText Description;

	/** Name of the directory containing template files */
	FString OnDiskPath;

	/** Brush resource for the image that is dynamically loaded */
	TSharedPtr< FSlateDynamicImageBrush > PluginIconDynamicImageBrush;

	/** Can the plugin contain content? */
	bool bCanContainContent;

	/** What is the expected ModuleDescriptor type for this plugin?*/
	EHostType::Type ModuleDescriptorType;

	/** What is the expected Loading Phase for this plugin? */
	ELoadingPhase::Type LoadingPhase;

	/** Constructor */
	FPluginTemplateDescription(FText InName, FText InDescription, FString InOnDiskPath, bool InCanContainContent, EHostType::Type InModuleDescriptorType, ELoadingPhase::Type InLoadingPhase = ELoadingPhase::Default)
		: Name(InName)
		, Description(InDescription)
		, OnDiskPath(InOnDiskPath)
		, bCanContainContent(InCanContainContent)
		, ModuleDescriptorType(InModuleDescriptorType)
		, LoadingPhase(InLoadingPhase)
	{
	}
};

class IPluginWizardDefinition : public TSharedFromThis<IPluginWizardDefinition>
{
public:

	virtual ~IPluginWizardDefinition() {}

	/** Returns the plugin templates available to this definition */
	virtual const TArray<TSharedRef<FPluginTemplateDescription>>& GetTemplatesSource() const = 0;

	/** Changes the selection internally to match the supplied items */
	virtual void OnTemplateSelectionChanged(TArray<TSharedRef<FPluginTemplateDescription>> InSelectedItems, ESelectInfo::Type SelectInfo) = 0;

	/** Returns true if the definition has a valid template selection */
	virtual bool HasValidTemplateSelection() const = 0;

	/** Gets the list selection mode for this definition */
	virtual ESelectionMode::Type GetSelectionMode() const = 0;

	/** Returns the currently selected templates */
	virtual TArray<TSharedPtr<FPluginTemplateDescription>> GetSelectedTemplates() const = 0;

	/** Clears the template selection */
	virtual void ClearTemplateSelection() = 0;

	/** Returns true if this definition allows for the creation of engine plugins */
	virtual bool AllowsEnginePlugins() const = 0;

	/** Returns true if the wizard for this definition can show on startup */
	virtual bool CanShowOnStartup() const = 0;

	/** Returns true if the selected template can contain content */
	virtual bool CanContainContent() const = 0;

	/** Returns true if the selected template will generate code */
	virtual bool HasModules() const = 0;

	/** Returns true if the plugin is a mod */
	virtual bool IsMod() const = 0;

	/** Callback for when the 'Show on Startup' checkbox changes in the plugin wizard. Only used if the definition allows for game mod plugins */
	virtual void OnShowOnStartupCheckboxChanged(ECheckBoxState CheckBoxState) = 0;

	/** Gets the state of the 'Show on Startup' checkbox. Only used if the definition allows for game mod plugins */
	virtual ECheckBoxState GetShowOnStartupCheckBoxState() const = 0;

	/** Returns a custom header widget for the new plugin wizard, if desired. */
	virtual TSharedPtr<class SWidget> GetCustomHeaderWidget() = 0;

	/** Gets the instructions to be shown when creating a new plugin */
	virtual FText GetInstructions() const = 0;

	/** Gets the icon path for the current template selection. Returns true if the plugin requires a default icon */
	virtual bool GetPluginIconPath(FString& OutIconPath) const = 0;

	/** Gets the ModuleDescriptor for the plugin based on the selection */
	virtual EHostType::Type GetPluginModuleDescriptor() const = 0;

	/** Gets the LoadingPhase for the plugin based on the selection */
	virtual ELoadingPhase::Type GetPluginLoadingPhase() const = 0;

	/** Gets the icon path for the specified template. Returns true if it requires a default icon */
	virtual bool GetTemplateIconPath(TSharedRef<FPluginTemplateDescription> InTemplate, FString& OutIconPath) const = 0;

	/** Gets the folder path of the current template selection */
	virtual FString GetPluginFolderPath() const = 0;

	/** Gets the folders for the current template selection */
	virtual TArray<FString> GetFoldersForSelection() const = 0;

	/** Called when a plugin is created, with a bool indicating whether creation was actually successful. */
	virtual void PluginCreated(const FString& PluginName, bool bWasSuccessful) const = 0;
};