// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleDescriptor.h"
#include "CustomBuildSteps.h"
#include "LocalizationDescriptor.h"
#include "PluginReferenceDescriptor.h"

class FJsonObject;

/**
 * Setting for whether a plugin is enabled by default
 */ 
enum class EPluginEnabledByDefault : uint8
{
	Unspecified,
	Enabled,
	Disabled,
};

/**
 * Descriptor for plugins. Contains all the information contained within a .uplugin file.
 */
struct PROJECTS_API FPluginDescriptor
{
	/** Version number for the plugin.  The version number must increase with every version of the plugin, so that the system 
	    can determine whether one version of a plugin is newer than another, or to enforce other requirements.  This version
		number is not displayed in front-facing UI.  Use the VersionName for that. */
	int32 Version;

	/** Name of the version for this plugin.  This is the front-facing part of the version number.  It doesn't need to match
	    the version number numerically, but should be updated when the version number is increased accordingly. */
	FString VersionName;

	/** Friendly name of the plugin */
	FString FriendlyName;

	/** Description of the plugin */
	FString Description;

	/** The name of the category this plugin */
	FString Category;

	/** The company or individual who created this plugin.  This is an optional field that may be displayed in the user interface. */
	FString CreatedBy;

	/** Hyperlink URL string for the company or individual who created this plugin.  This is optional. */
	FString CreatedByURL;

	/** Documentation URL string. */
	FString DocsURL;

	/** Marketplace URL for this plugin. This URL will be embedded into projects that enable this plugin, so we can redirect to the marketplace if a user doesn't have it installed. */
	FString MarketplaceURL;

	/** Support URL/email for this plugin. */
	FString SupportURL;

	/** Version of the engine that this plugin is compatible with */
	FString EngineVersion;

	/** List of target platforms supported by this plugin. This list will be copied to any plugin reference from a project file, to allow filtering entire plugins from staged builds. */
	TArray<FString> SupportedTargetPlatforms;

	/** List of all modules associated with this plugin */
	TArray<FModuleDescriptor> Modules;

	/** List of all localization targets associated with this plugin */
	TArray<FLocalizationTargetDescriptor> LocalizationTargets;

	/** Whether this plugin should be enabled by default for all projects */
	EPluginEnabledByDefault EnabledByDefault;

	/** Can this plugin contain content? */
	bool bCanContainContent;

	/** Marks the plugin as beta in the UI */
	bool bIsBetaVersion;

	/** Signifies that the plugin was installed on top of the engine */
	bool bInstalled;

	/** For plugins that are under a platform folder (eg. /PS4/), determines whether compiling the plugin requires the build platform and/or SDK to be available */
	bool bRequiresBuildPlatform;

	/** For auto-generated plugins that should not be listed in the plugin browser for users to disable freely. */
	bool bIsHidden;

	/** Pre-build steps for each host platform */
	FCustomBuildSteps PreBuildSteps;

	/** Pre-build steps for each host platform */
	FCustomBuildSteps PostBuildSteps;

	/** Dependent plugins */
	TArray<FPluginReferenceDescriptor> Plugins;

	/** Constructor. */
	FPluginDescriptor();

	/** Loads the descriptor from the given file. */
	bool Load(const FString& FileName, FText& OutFailReason);

	/** Reads the descriptor from the given string */
	bool Read(const FString& Text, FText& OutFailReason);

	/** Reads the descriptor from the given JSON object */
	bool Read(const FJsonObject& Object, FText& OutFailReason);

	/** Saves the descriptor from the given file. */
	bool Save(const FString& FileName, FText& OutFailReason) const;

	/** Writes a descriptor to JSON */
	void Write(FString& Text) const;

	/** Writes a descriptor to JSON */
	void Write(TJsonWriter<>& Writer) const;

	/** Determines whether the plugin supports the given platform */
	bool SupportsTargetPlatform(const FString& Platform) const;
};

