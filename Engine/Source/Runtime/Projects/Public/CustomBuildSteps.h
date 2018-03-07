// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/JsonWriter.h"

class FJsonObject;

/**
 * Descriptor for projects. Contains all the information contained within a .uproject file.
 */
struct PROJECTS_API FCustomBuildSteps
{
	/** Mapping from host platform to list of commands */
	TMap<FString, TArray<FString>> HostPlatformToCommands;

	/** Whether this object is empty */
	bool IsEmpty() const;

	/** Reads the descriptor from the given JSON object */
	void Read(const FJsonObject& Object, const FString& FieldName);

	/** Writes the descriptor to the given JSON object */
	void Write(TJsonWriter<>& Writer, const FString& FieldName) const;
};
