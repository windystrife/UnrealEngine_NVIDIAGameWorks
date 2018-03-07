// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Serialization/Archive.h"
#include "Features/IModularFeature.h"
#include "Modules/ModuleInterface.h"

class FArchive;
class UClass;

/** Build module type, mirrored in UEBuildModule.cs, enum UEBuildModuletype */
struct EBuildModuleType
{
	enum Type
	{
		Program,
		EngineRuntime,
		EngineDeveloper,
		EngineEditor,
		EngineThirdParty,
		GameRuntime,
		GameDeveloper,
		GameEditor,
		GameThirdParty,
		// NOTE: If you add a new value, make sure to update the ToString() method below!

		Max
	};

	friend FArchive& operator<<(FArchive& Ar, EBuildModuleType::Type& Type)
	{
		if (Ar.IsLoading())
		{
			uint8 Value;
			Ar << Value;
			Type = (EBuildModuleType::Type)Value;
		}
		else if (Ar.IsSaving())
		{
			uint8 Value = (uint8)Type;
			Ar << Value;
		}
		return Ar;
	}
	/**
	* Converts a string literal into EModuleType::Type value
	*
	* @param	The string to convert to EModuleType::Type
	*
	* @return	The enum value corresponding to the name
	*/
	static EBuildModuleType::Type Parse(const TCHAR* Value);
};

/**
 * The public interface to script generator plugins.
 */
class IScriptGeneratorPluginInterface : public IModuleInterface, public IModularFeature
{
public:

	/** Name of module that is going to be compiling generated script glue */
	virtual FString GetGeneratedCodeModuleName() const = 0;
	/** Returns true if this plugin supports exporting scripts for the specified target. This should handle game as well as editor target names */
	virtual bool SupportsTarget(const FString& TargetName) const = 0;
	/** Returns true if this plugin supports exporting scripts for the specified module */
	virtual bool ShouldExportClassesForModule(const FString& ModuleName, EBuildModuleType::Type ModuleType, const FString& ModuleGeneratedIncludeDirectory) const = 0;
	/** Initializes this plugin with build information */
	virtual void Initialize(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& IncludeBase) = 0;
	/** Exports a single class. May be called multiple times for the same class (as UHT processes the entire hierarchy inside modules. */
	virtual void ExportClass(class UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged) = 0;
	/** Called once all classes have been exported */
	virtual void FinishExport() = 0;
	/** Name of the generator plugin, mostly for debuggind purposes */
	virtual FString GetGeneratorName() const = 0;
	/** Finds a list of external dependencies which require UHT to be re-run */
	virtual void GetExternalDependencies(TArray<FString>& Dependencies) const { }
};

