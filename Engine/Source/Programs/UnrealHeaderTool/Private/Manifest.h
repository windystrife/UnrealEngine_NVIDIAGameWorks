// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeneratedCodeVersion.h"
#include "IScriptGeneratorPluginInterface.h"

class FArchive;

struct FManifestModule
{
	/** The name of the module */
	FString Name;

	/** Module type */
	EBuildModuleType::Type ModuleType;

	/** Long package name for this module's UObject class */
	FString LongPackageName;

	/** Base directory of this module on disk */
	FString BaseDirectory;

	/** The directory to which #includes from this module should be relative */
	FString IncludeBase;

	/** Directory where generated include files should go */
	FString GeneratedIncludeDirectory;

	/** List of C++ public 'Classes' header files with UObjects in them (legacy) */
	TArray<FString> PublicUObjectClassesHeaders;

	/** List of C++ public header files with UObjects in them */
	TArray<FString> PublicUObjectHeaders;

	/** List of C++ private header files with UObjects in them */
	TArray<FString> PrivateUObjectHeaders;

	/** Absolute path to the module's PCH */
	FString PCH;

	/** Base (i.e. extensionless) path+filename of where to write out the module's .generated.* files */
	FString GeneratedCPPFilenameBase;

	/** Whether or not to write out headers that have changed */
	bool SaveExportedHeaders;

	/** Version of generated code. */
	EGeneratedCodeVersion GeneratedCodeVersion;

	/** Returns true if module headers were modified since last code generation. */
	bool NeedsRegeneration() const;

	/** Returns true if modules are compatible. Used to determine if module data can be loaded from makefile. */
	bool IsCompatibleWith(const FManifestModule& ManifestModule);

	friend FArchive& operator<<(FArchive& Ar, FManifestModule& ManifestModule)
	{
		Ar << ManifestModule.Name;
		Ar << ManifestModule.ModuleType;
		Ar << ManifestModule.LongPackageName;
		Ar << ManifestModule.BaseDirectory;
		Ar << ManifestModule.IncludeBase;
		Ar << ManifestModule.GeneratedIncludeDirectory;
		Ar << ManifestModule.PublicUObjectClassesHeaders;
		Ar << ManifestModule.PublicUObjectHeaders;
		Ar << ManifestModule.PrivateUObjectHeaders;
		Ar << ManifestModule.PCH;
		Ar << ManifestModule.GeneratedCPPFilenameBase;
		Ar << ManifestModule.SaveExportedHeaders;
		Ar << ManifestModule.GeneratedCodeVersion;

		return Ar;
	}

	bool ShouldForceRegeneration() const
	{
		return bForceRegeneration;
	}

	void ForceRegeneration()
	{
		bForceRegeneration = true;
	}

private:
	bool bForceRegeneration;
};


struct FManifest
{
	bool    IsGameTarget;
	FString RootLocalPath;
	FString RootBuildPath;
	FString TargetName;
	FString ExternalDependenciesFile;
	
	/** Ordered list of modules that define UObjects or UStructs, which we may need to generate
	    code for.  The list is in module dependency order, such that most dependent modules appear first. */
	TArray<FManifestModule> Modules;

	/**
	 * Loads a *.uhtmanifest from the specified filename.
	 *
	 * @param Filename The filename of the manifest to load.
	 * @return The loaded module info.
	 */
	static FManifest LoadFromFile(const FString& Filename);

	friend FArchive& operator<<(FArchive& Ar, FManifest& Manifest)
	{
		Ar << Manifest.IsGameTarget;
		Ar << Manifest.RootLocalPath;
		Ar << Manifest.RootBuildPath;
		Ar << Manifest.TargetName;
		Ar << Manifest.Modules;

		return Ar;
	}
};
