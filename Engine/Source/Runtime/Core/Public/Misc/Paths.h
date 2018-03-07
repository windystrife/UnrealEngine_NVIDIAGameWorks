// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"

/**
 * Path helpers for retrieving game dir, engine dir, etc.
 */
class CORE_API FPaths
{
public:

	/**
	 * Should the "saved" directory structures be rooted in the user dir or relative to the "engine/game" 
	 */
	static bool ShouldSaveToUserDir();

	/**
	 * Returns the directory the application was launched from (useful for commandline utilities)
	 */
	static FString LaunchDir();
	 
	/** 
	 * Returns the base directory of the "core" engine that can be shared across
	 * several games or across games & mods. Shaders and base localization files
	 * e.g. reside in the engine directory.
	 *
	 * @return engine directory
	 */
	static FString EngineDir();

	/**
	* Returns the root directory for user-specific engine files. Always writable.
	*
	* @return root user directory
	*/
	static FString EngineUserDir();

	/**
	* Returns the root directory for user-specific engine files which can be shared between versions. Always writable.
	*
	* @return root user directory
	*/
	static FString EngineVersionAgnosticUserDir();

	/** 
	 * Returns the content directory of the "core" engine that can be shared across
	 * several games or across games & mods. 
	 *
	 * @return engine content directory
	 */
	static FString EngineContentDir();

	/**
	 * Returns the directory the root configuration files are located.
	 *
	 * @return root config directory
	 */
	static FString EngineConfigDir();

	/**
	 * Returns the intermediate directory of the engine
	 *
	 * @return content directory
	 */
	static FString EngineIntermediateDir();

	/**
	 * Returns the saved directory of the engine
	 *
	 * @return Saved directory.
	 */
	static FString EngineSavedDir();

	/**
	 * Returns the plugins directory of the engine
	 *
	 * @return Plugins directory.
	 */
	static FString EnginePluginsDir();

	/** 
	* Returns the base directory enterprise directory.
	*
	* @return enterprise directory
	*/
	static FString EnterpriseDir();

	/**
	* Returns the enterprise plugins directory
	*
	* @return Plugins directory.
	*/
	static FString EnterprisePluginsDir();

	/**
	 * Returns the root directory of the engine directory tree
	 *
	 * @return Root directory.
	 */
	static FString RootDir();

	/**
	 * Returns the base directory of the current project by looking at FApp::GetProjectName().
	 * This is usually a subdirectory of the installation
	 * root directory and can be overridden on the command line to allow self
	 * contained mod support.
	 *
	 * @return base directory
	 */
	static FString ProjectDir();

	DEPRECATED(4.18, "FPaths::GameDir() has been superseded by FPaths::ProjectDir().")
	static FORCEINLINE FString GameDir() { return ProjectDir(); }

	/**
	* Returns the root directory for user-specific game files.
	*
	* @return game user directory
	*/
	static FString ProjectUserDir();

	DEPRECATED(4.18, "FPaths::GameUserDir() has been superseded by FPaths::ProjectUserDir().")
	static FORCEINLINE FString GameUserDir() { return ProjectUserDir(); }

	/**
	 * Returns the content directory of the current game by looking at FApp::GetProjectName().
	 *
	 * @return content directory
	 */
	static FString ProjectContentDir();

	DEPRECATED(4.18, "FPaths::GameContentDir() has been superseded by FPaths::ProjectContentDir().")
	static FORCEINLINE FString GameContentDir() { return ProjectContentDir(); }

	/**
	* Returns the directory the root configuration files are located.
	*
	* @return root config directory
	*/
	static FString ProjectConfigDir();

	DEPRECATED(4.18, "FPaths::GameConfigDir() has been superseded by FPaths::ProjectConfigDir().")
	static FORCEINLINE FString GameConfigDir() { return ProjectConfigDir(); }

	/**
	 * Returns the saved directory of the current game by looking at FApp::GetProjectName().
	 *
	 * @return saved directory
	 */
	static FString ProjectSavedDir();

	DEPRECATED(4.18, "FPaths::GameSavedDir() has been superseded by FPaths::ProjectSavedDir().")
	static FORCEINLINE FString GameSavedDir() { return ProjectSavedDir(); }

	/**
	 * Returns the intermediate directory of the current game by looking at FApp::GetProjectName().
	 *
	 * @return intermediate directory
	 */
	static FString ProjectIntermediateDir();

	DEPRECATED(4.18, "FPaths::GameIntermediateDir() has been superseded by FPaths::ProjectIntermediateDir().")
	static FORCEINLINE FString GameIntermediateDir() { return ProjectIntermediateDir(); }

	/**
	 * Returns the plugins directory of the current game by looking at FApp::GetProjectName().
	 *
	 * @return plugins directory
	 */
	static FString ProjectPluginsDir();

	DEPRECATED(4.18, "FPaths::GamePluginsDir() has been superseded by FPaths::ProjectPluginsDir().")
	static FORCEINLINE FString GamePluginsDir() { return ProjectPluginsDir(); }

	/**
	 * Returns the mods directory of the current project by looking at FApp::GetProjectName().
	 *
	 * @return mods directory
	 */
	static FString ProjectModsDir();

	/*
	* Returns the writable directory for downloaded data that persists across play sessions.
	*/
	static FString ProjectPersistentDownloadDir();

	DEPRECATED(4.18, "FPaths::GamePersistentDownloadDir() has been superseded by FPaths::ProjectPersistentDownloadDir().")
	static FORCEINLINE FString GamePersistentDownloadDir() { return ProjectPersistentDownloadDir(); }

	/**
	 * Returns the directory the engine uses to look for the source leaf ini files. This
	 * can't be an .ini variable for obvious reasons.
	 *
	 * @return source config directory
	 */
	static FString SourceConfigDir();

	/**
	 * Returns the directory the engine saves generated config files.
	 *
	 * @return config directory
	 */
	static FString GeneratedConfigDir();

	/**
	 * Returns the directory the engine stores sandbox output
	 *
	 * @return sandbox directory
	 */
	static FString SandboxesDir();

	/**
	 * Returns the directory the engine uses to output profiling files.
	 *
	 * @return log directory
	 */
	static FString ProfilingDir();

	/**
	 * Returns the directory the engine uses to output screenshot files.
	 *
	 * @return screenshot directory
	 */
	static FString ScreenShotDir();

	/**
	 * Returns the directory the engine uses to output BugIt files.
	 *
	 * @return screenshot directory
	 */
	static FString BugItDir();

	/**
	 * Returns the directory the engine uses to output user requested video capture files.
	 *
	 * @return Video capture directory
	 */
	static FString VideoCaptureDir();
	
	/**
	 * Returns the directory the engine uses to output logs. This currently can't 
	 * be an .ini setting as the game starts logging before it can read from .ini
	 * files.
	 *
	 * @return log directory
	 */
	static FString ProjectLogDir();

	DEPRECATED(4.18, "FPaths::GameLogDir() has been superseded by FPaths::ProjectLogDir().")
	static FORCEINLINE FString GameLogDir() { return ProjectLogDir(); }

	/**
	 * @return The directory for automation save files
	 */
	static FString AutomationDir();

	/**
	 * @return The directory for automation save files that are meant to be deleted every run
	 */
	static FString AutomationTransientDir();

	/**
	* @return The directory for automation log files.
	*/
	static FString AutomationLogDir();

	/**
	 * @return The directory for local files used in cloud emulation or support
	 */
	static FString CloudDir();

		/**
	 * @return The directory that contains subfolders for developer-specific content
	 */
	static FString GameDevelopersDir();

	/**
	 * @return The directory that contains developer-specific content for the current user
	 */
	static FString GameUserDeveloperDir();

	/**
	 * @return The directory for temp files used for diffing
	 */
	static FString DiffDir();

	/** 
	 * Returns a list of engine-specific localization paths
	 */
	static const TArray<FString>& GetEngineLocalizationPaths();

	/** 
	 * Returns a list of editor-specific localization paths
	 */
	static const TArray<FString>& GetEditorLocalizationPaths();

	/** 
	 * Returns a list of property name localization paths
	 */
	static const TArray<FString>& GetPropertyNameLocalizationPaths();

		/** 
	 * Returns a list of tool tip localization paths
	 */
	static const TArray<FString>& GetToolTipLocalizationPaths();

	/** 
	 * Returns a list of game-specific localization paths
	 */
	static const TArray<FString>& GetGameLocalizationPaths();

	/**
	 * Returns the saved directory that is not game specific. This is usually the same as
	 * EngineSavedDir().
	 *
	 * @return saved directory
	 */
	static FString GameAgnosticSavedDir();

	/**
	 * @return The directory where engine source code files are kept
	 */
	static FString EngineSourceDir();

	/**
	 * @return The directory where game source code files are kept
	 */
	static FString GameSourceDir();

	/**
	 * @return The directory where feature packs are kept
	 */
	static FString FeaturePackDir();

	/**
	 * Checks whether the path to the project file, if any, is set.
	 *
	 * @return true if the path is set, false otherwise.
	 */
	static bool IsProjectFilePathSet();
	
	/**
	 * Gets the path to the project file.
	 *
	 * @return Project file path.
	 */
	static FString GetProjectFilePath();

	/**
	 * Sets the path to the project file.
	 *
	 * @param NewGameProjectFilePath - The project file path to set.
	 */
	static void SetProjectFilePath( const FString& NewGameProjectFilePath );

	/**
	 * Gets the extension for this filename.
	 *
	 * @param	bIncludeDot		if true, includes the leading dot in the result
	 *
	 * @return	the extension of this filename, or an empty string if the filename doesn't have an extension.
	 */
	static FString GetExtension( const FString& InPath, bool bIncludeDot=false );

	// Returns the filename (with extension), minus any path information.
	static FString GetCleanFilename(const FString& InPath);

	// Returns the filename (with extension), minus any path information.
	static FString GetCleanFilename(FString&& InPath);

	// Returns the same thing as GetCleanFilename, but without the extension
	static FString GetBaseFilename( const FString& InPath, bool bRemovePath=true );

	// Returns the path in front of the filename
	static FString GetPath(const FString& InPath);

	// Returns the path in front of the filename
	static FString GetPath(FString&& InPath);

	/** Changes the extension of the given filename (does nothing if the file has no extension) */
	static FString ChangeExtension(const FString& InPath, const FString& InNewExtension);

	/** Sets the extension of the given filename (like ChangeExtension, but also applies the extension if the file doesn't have one) */
	static FString SetExtension(const FString& InPath, const FString& InNewExtension);

	/** @return true if this file was found, false otherwise */
	static bool FileExists(const FString& InPath);

	/** @return true if this directory was found, false otherwise */
	static bool DirectoryExists(const FString& InPath);

	/** @return true if this path represents a drive */
	static bool IsDrive(const FString& InPath);

	/** @return true if this path is relative */
	static bool IsRelative(const FString& InPath);

	/** Convert all / and \ to TEXT("/") */
	static void NormalizeFilename(FString& InPath);

	/**
	 * Checks if two paths are the same.
	 *
	 * @param PathA First path to check.
	 * @param PathB Second path to check.
	 *
	 * @returns True if both paths are the same. False otherwise.
	 */
	static bool IsSamePath(const FString& PathA, const FString& PathB);

	/** Normalize all / and \ to TEXT("/") and remove any trailing TEXT("/") if the character before that is not a TEXT("/") or a colon */
	static void NormalizeDirectoryName(FString& InPath);

	/**
	 * Takes a fully pathed string and eliminates relative pathing (eg: annihilates ".." with the adjacent directory).
	 * Assumes all slashes have been converted to TEXT('/').
	 * For example, takes the string:
	 *	BaseDirectory/SomeDirectory/../SomeOtherDirectory/Filename.ext
	 * and converts it to:
	 *	BaseDirectory/SomeOtherDirectory/Filename.ext
	 */
	static bool CollapseRelativeDirectories(FString& InPath);

	/**
	 * Removes duplicate slashes in paths.
	 * Assumes all slashes have been converted to TEXT('/').
	 * For example, takes the string:
	 *	BaseDirectory/SomeDirectory//SomeOtherDirectory////Filename.ext
	 * and converts it to:
	 *	BaseDirectory/SomeDirectory/SomeOtherDirectory/Filename.ext
	 */
	static void RemoveDuplicateSlashes(FString& InPath);

	/**
	 * Make fully standard "Unreal" pathname:
	 *    - Normalizes path separators [NormalizeFilename]
	 *    - Removes extraneous separators  [NormalizeDirectoryName, as well removing adjacent separators]
	 *    - Collapses internal ..'s
	 *    - Makes relative to Engine\Binaries\<Platform> (will ALWAYS start with ..\..\..)
	 */
	static void MakeStandardFilename(FString& InPath);

	/** Takes an "Unreal" pathname and converts it to a platform filename. */
	static void MakePlatformFilename(FString& InPath);

	/** 
	 * Assuming both paths (or filenames) are relative to the base dir, find the relative path to the InPath.
	 *
	 * @Param InPath Path to make this path relative to.
	 * @return Path relative to InPath.
	 */
	static bool MakePathRelativeTo( FString& InPath, const TCHAR* InRelativeTo );

	/**
	 * Converts a relative path name to a fully qualified name relative to the process BaseDir().
	 */
	static FString ConvertRelativePathToFull(const FString& InPath);

	/**
	 * Converts a relative path name to a fully qualified name relative to the process BaseDir().
	 */
	static FString ConvertRelativePathToFull(FString&& InPath);

	/**
	 * Converts a relative path name to a fully qualified name relative to the specified BasePath.
	 */
	static FString ConvertRelativePathToFull(const FString& BasePath, const FString& InPath);

	/**
	 * Converts a relative path name to a fully qualified name relative to the specified BasePath.
	 */
	static FString ConvertRelativePathToFull(const FString& BasePath, FString&& InPath);

	/**
	 * Converts a relative path name to a fully qualified name relative to the specified BasePath.
	 */
	static FString ConvertRelativePathToFull(FString&& BasePath, const FString& InPath);

	/**
	 * Converts a relative path name to a fully qualified name relative to the specified BasePath.
	 */
	static FString ConvertRelativePathToFull(FString&& BasePath, FString&& InPath);

	/**
	 * Converts a normal path to a sandbox path (in Saved/Sandboxes).
	 *
	 * @param InSandboxName The name of the sandbox.
	 */
	static FString ConvertToSandboxPath( const FString& InPath, const TCHAR* InSandboxName );

	/**
	 * Converts a sandbox (in Saved/Sandboxes) path to a normal path.
	 *
	 * @param InSandboxName The name of the sandbox.
	 */
	static FString ConvertFromSandboxPath( const FString& InPath, const TCHAR* InSandboxName );

	/** 
	 * Creates a temporary filename with the specified prefix.
	 *
	 * @param Path The file pathname.
	 * @param Prefix The file prefix.
	 * @param Extension File extension ('.' required).
	 */
	static FString CreateTempFilename( const TCHAR* Path, const TCHAR* Prefix = TEXT(""), const TCHAR* Extension = TEXT(".tmp") );

	/** 
	 * Validates that the parts that make up the path contain no invalid characters as dictated by the operating system
	 * Note that this is a different set of restrictions to those imposed by FPackageName
	 *
	 * @param InPath - path to validate
	 * @param OutReason - optional parameter to fill with the failure reason
	 */
	static bool ValidatePath( const FString& InPath, FText* OutReason = nullptr );

	/**
	 * Parses a fully qualified or relative filename into its components (filename, path, extension).
	 *
	 * @param	Path		[out] receives the value of the path portion of the input string
	 * @param	Filename	[out] receives the value of the filename portion of the input string
	 * @param	Extension	[out] receives the value of the extension portion of the input string
	 */
	static void Split( const FString& InPath, FString& PathPart, FString& FilenamePart, FString& ExtensionPart );

	/** Gets the relative path to get from BaseDir to RootDirectory  */
	static const FString& GetRelativePathToRoot();

	template <typename... PathTypes>
	FORCEINLINE static FString Combine(PathTypes&&... InPaths)
	{
		const TCHAR* Paths[] = { GetTCharPtr(Forward<PathTypes>(InPaths))... };
		FString Out;
		
		CombineInternal(Out, Paths, ARRAY_COUNT(Paths));
		return Out;
	}

protected:

	static void CombineInternal(FString& OutPath, const TCHAR** Paths, int32 NumPaths);

private:
	FORCEINLINE static const TCHAR* GetTCharPtr(const TCHAR* Ptr)
	{
		return Ptr;
	}

	FORCEINLINE static const TCHAR* GetTCharPtr(const FString& Str)
	{
		return *Str;
	}

	/** Holds the path to the currently loaded game project file. */
	static FString GameProjectFilePath;

	/** Thread protection for above path */
	FORCEINLINE static FCriticalSection* GameProjectFilePathLock() 
	{
		static FCriticalSection Lock;
		return &Lock; 
	}
};
