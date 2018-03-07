// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PluginDescriptor.h"

class FJsonObject;

/**
 * Descriptor for plugins. Contains all the information contained within a .uplugin file.
 */
struct PROJECTS_API FPluginManifestEntry
{
	/** Normalized path to the plugin file */
	FString File;

	/** The plugin descriptor */
	FPluginDescriptor Descriptor;
};

/**
 * Manifest of plugins. Descriptor for plugins. Contains all the information contained within a .uplugin file.
 */
struct PROJECTS_API FPluginManifest
{
	/** List of plugins in this manifest */
	TArray<FPluginManifestEntry> Contents;

	/** Loads the descriptor from the given file. */
	bool Load(const FString& FileName, FText& OutFailReason);

	/** Reads the descriptor from the given JSON object */
	bool Read(const FJsonObject& Object, FText& OutFailReason);
};