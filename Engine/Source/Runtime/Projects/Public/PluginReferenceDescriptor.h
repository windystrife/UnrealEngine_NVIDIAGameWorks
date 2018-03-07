// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleDescriptor.h"
#include "CustomBuildSteps.h"
#include "LocalizationDescriptor.h"

/**
 * Descriptor for a plugin reference. Contains the information required to enable or disable a plugin for a given platform.
 */
struct PROJECTS_API FPluginReferenceDescriptor
{
	/** Name of the plugin */
	FString Name;

	/** Whether it should be enabled by default */
	bool bEnabled;

	/** Whether this plugin is optional, and the game should silently ignore it not being present */
	bool bOptional;
	
	/** Description of the plugin for users that do not have it installed. */
	FString Description;

	/** URL for this plugin on the marketplace, if the user doesn't have it installed. */
	FString MarketplaceURL;

	/** If enabled, list of platforms for which the plugin should be enabled (or all platforms if blank). */
	TArray<FString> WhitelistPlatforms;

	/** If enabled, list of platforms for which the plugin should be disabled. */
	TArray<FString> BlacklistPlatforms;
 
	/** If enabled, list of targets for which the plugin should be enabled (or all targets if blank). */
	TArray<FString> WhitelistTargets;

	/** If enabled, list of targets for which the plugin should be disabled. */
	TArray<FString> BlacklistTargets;

	/** The list of supported target platforms for this plugin. This field is copied from the plugin descriptor, and supplements the user's whitelisted and blacklisted platforms. */
	TArray<FString> SupportedTargetPlatforms;

	/** Constructor */
	FPluginReferenceDescriptor(const FString& InName = TEXT(""), bool bInEnabled = false);

	/** Determines whether the plugin is enabled for the given platform */
	bool IsEnabledForPlatform(const FString& Platform) const;

	/** Determines whether the plugin is enabled for the given target */
	bool IsEnabledForTarget(const FString& Target) const;

	/** Determines if the referenced plugin is supported for the given platform */
	bool IsSupportedTargetPlatform(const FString& Platform) const;

	/** Reads the descriptor from the given JSON object */
	bool Read(const FJsonObject& Object, FText& OutFailReason);

	/** Reads an array of modules from the given JSON object */
	static bool ReadArray(const FJsonObject& Object, const TCHAR* Name, TArray<FPluginReferenceDescriptor>& OutModules, FText& OutFailReason);

	/** Writes a descriptor to JSON */
	void Write(TJsonWriter<>& Writer) const;

	/** Writes an array of modules to JSON */
	static void WriteArray(TJsonWriter<>& Writer, const TCHAR* Name, const TArray<FPluginReferenceDescriptor>& Modules);
};
