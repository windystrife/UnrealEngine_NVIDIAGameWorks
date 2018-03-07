// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "HAL/PlatformTime.h"
#include "IPluginManager.h"

/**
 * Type of hierarchy node
 */
enum class ENativeClassHierarchyNodeType : uint8
{
	Folder,
	Class,
};

/**
 *  Contains high-level information about the plugin module relevant to the native class hierarchy.                                                                      
 */
struct FNativeClassHierarchyPluginModuleInfo
{
	/** Name of the module*/
	FName Name;

	/** Indicator of where the module was loaded from (Engine or GameProject)*/
	EPluginLoadedFrom LoadedFrom;
};

/**
 * Type used as a key in a map to resolve name conflicts between folders and classes
 */
struct FNativeClassHierarchyNodeKey
{
	FNativeClassHierarchyNodeKey(const FName InName, const ENativeClassHierarchyNodeType InType)
		: Name(InName)
		, Type(InType)
	{
	}

	FORCEINLINE bool operator==(const FNativeClassHierarchyNodeKey& Other) const
	{
		return Name == Other.Name && Type == Other.Type;
	}

	FORCEINLINE bool operator!=(const FNativeClassHierarchyNodeKey& Other) const
	{
		return Name != Other.Name || Type != Other.Type;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FNativeClassHierarchyNodeKey& InKey)
	{
		uint32 KeyHash = 0;
		KeyHash = HashCombine(KeyHash, GetTypeHash(InKey.Name));
		KeyHash = HashCombine(KeyHash, GetTypeHash(InKey.Type));
		return KeyHash;
	}

	/** Name of this entry */
	FName Name;

	/** Type of this entry */
	ENativeClassHierarchyNodeType Type;
};

/**
 * Single node in the class hierarchy
 */
struct FNativeClassHierarchyNode
{
	/** Helper function to make a folder node entry */
	static TSharedRef<FNativeClassHierarchyNode> MakeFolderEntry(FName InEntryName, FString InEntryPath);

	/** Helper function to make a class node entry */
	static TSharedRef<FNativeClassHierarchyNode> MakeClassEntry(UClass* InClass, FName InClassModuleName, FString InClassModuleRelativePath, FString InEntryPath);

	void AddChild(TSharedRef<FNativeClassHierarchyNode> ChildEntry);

	/** Type of node, folder or class */
	ENativeClassHierarchyNodeType Type;

	/** The class this node is for (Type == Class) */
	UClass* Class;

	/** The name of the module the class is in (Type == Class) */
	FName ClassModuleName;

	/** Folder this class is in, relative to the class module (Type == Class) */
	FString ClassModuleRelativePath;

	/** Name used when showing this entry in the UI */
	FName EntryName;

	/** Path to this entry in the class hierarchy (not the same as the location on disk) */
	FString EntryPath;

	/** Child entries (Type == Folder) */
	TMap<FNativeClassHierarchyNodeKey, TSharedPtr<FNativeClassHierarchyNode>> Children;

	/** Which type of plugin this data was originally loaded from (if loaded from a plugin)*/
	EPluginLoadedFrom LoadedFrom;
};

/**
 * A filter used when querying the native class hierarchy.
 * Each component element is processed as an 'OR' operation while all the components are processed together as an 'AND' operation.
 */
struct FNativeClassHierarchyFilter
{
	/** The filter component for class paths */
	TArray<FName> ClassPaths;
	/** If true, ClassPaths components will be recursive */
	bool bRecursivePaths;

	FNativeClassHierarchyFilter()
	{
		bRecursivePaths = false;
	}

	/** Appends the other filter to this one */
	void Append(const FNativeClassHierarchyFilter& Other)
	{
		ClassPaths.Append(Other.ClassPaths);

		bRecursivePaths |= Other.bRecursivePaths;
	}

	/** Returns true if this filter has no entries */
	bool IsEmpty() const
	{
		return ClassPaths.Num() == 0;
	}

	/** Clears this filter of all entries */
	void Clear()
	{
		ClassPaths.Empty();

		bRecursivePaths = false;

		ensure(IsEmpty());
	}
};

/** 
 * Generates a hierarchical tree of native UObject classes based on their location in the file system as used by the asset view when showing C++ classes
 * This keeps its class hierarchy up-to-date as modules are loaded/unloaded
 */
class FNativeClassHierarchy
{
public:
	/** Constructor and destructor */
	FNativeClassHierarchy();
	~FNativeClassHierarchy();

	/** Get the delegate called when classes are added or removed from this class hierarchy */
	FSimpleMulticastDelegate& OnClassHierarchyUpdated()
	{
		return ClassHierarchyUpdatedDelegate;
	}

	/**
	 * Does the given path contain classes, optionally also testing sub-paths?
	 *
	 * @param InClassPath - The path to query classes in
	 * @param bRecursive - If true, the supplied path will be tested recursively
	 */
	bool HasClasses(const FName InClassPath, const bool bRecursive = false) const;

	/**
	 * Does the given path contain folders, optionally also testing sub-paths?
	 *
	 * @param InClassPath - The path to query classes in
	 * @param bRecursive - If true, the supplied path will be tested recursively
	 */
	bool HasFolders(const FName InClassPath, const bool bRecursive = false) const;

	/**
	 * Work out which classes known to the class hierarchy match the given filter
	 *
	 * @param Filter - The filter to apply when working out which classes match
	 * @param OutClasses - Array to be populated with matching classes
	 */
	void GetMatchingClasses(const FNativeClassHierarchyFilter& Filter, TArray<UClass*>& OutClasses) const;

	/**
	 * Work out which folders known to the class hierarchy match the given filter
	 *
	 * @param Filter - The filter to apply when working out which folders match
	 * @param OutFolders - Array to be populated with matching folder paths (these are in the form "/Classes_Name/ModuleName/SubFolder")
	 */
	void GetMatchingFolders(const FNativeClassHierarchyFilter& Filter, TArray<FString>& OutFolders) const;

	/**
	 * Get all folders known to the class hierarchy
	 *
	 * @param OutClassRoots - Array to be populated with the known class path root folders (these are in the form "/Classes_Name")
	 * @param OutClassFolders - Array to be populated with known class folder paths (these are in the form "/Classes_Name/ModuleName/SubFolder")
	 * @param bIncludeEngineClasses - Whether we should include the "/Classes_Engine" root (and its associated folders) in the returned results
	 * @param bIncludePluginClasses - Whether we should include plugin class path roots (and their associated folders) in the returned results (these are all roots except for "/Classes_Game" and "/Classes_Engine")
	 */
	void GetClassFolders(TArray<FName>& OutClassRoots, TArray<FString>& OutClassFolders, const bool bIncludeEngineClasses = true, const bool bIncludePluginClasses = true) const;

	/**
	 * Given a class path, work out the corresponding filesystem path on disk
	 * eg) Given "/Classes_Game/MyGame/MyAwesomeCode", we might resolve that to "../../../MyGame/Source/MyGame/MyAwesomeCode"
	 *
	 * @param InClassPath - The class path to try and resolve, eg) "/Classes_Game/MyGame/MyAwesomeCode"
	 * @param OutFileSystemPath - The string to fill with the resolved filesystem path, eg) "../../../MyGame/Source/MyGame/MyAwesomeCode"
	 * 
	 * @return true if the file system path could be resolved and OutFileSystemPath was filled in, false otherwise
	 */
	bool GetFileSystemPath(const FString& InClassPath, FString& OutFileSystemPath) const;

	/**
	 * Work out the class path that should be used for the given class
	 *
	 * @param InClass - The class path to try and get the class path for
	 * @param OutClassPath - The string to fill with the resolved class path, eg) "/Classes_Game/MyGame/MyAwesomeCode/MyAwesomeClass"
	 * @param bIncludeClassName - true to include the class name on the returned class path, false to get the class path of the containing folder
	 * 
	 * @return true if the class path could be resolved and OutClassPath was filled in, false otherwise
	 */
	bool GetClassPath(UClass* InClass, FString& OutClassPath, const bool bIncludeClassName = true) const;

	/**
	 * This will add a transient folder into the hierarchy
	 * The folder will be lost unless a class is added to it before the hierarchy is next re-populated
	 *
	 * @param InClassPath - The location of the new folder (in class path form - eg) "/Classes_Game/MyGame/MyAwesomeCode")
	 */
	void AddFolder(const FString& InClassPath);

private:
	struct FAddClassMetrics
	{
		FAddClassMetrics()
			: StartTime(FPlatformTime::Seconds())
			, NumClassesAdded(0)
			, NumFoldersAdded(0)
		{
		}

		double StartTime;
		int32 NumClassesAdded;
		int32 NumFoldersAdded;
	};

	/**
	 * Test to see whether the given node has any child classes
	 *
	 * @param HierarchyNode - The node to test the children of
	 * @param bRecurse - True to recurse into sub-folders, false to only check the given node
	 *
	 * @return True if the node has child classes, false otherwise
	 */
	static bool HasClassesRecursive(const TSharedRef<FNativeClassHierarchyNode>& HierarchyNode, const bool bRecurse = true);

	/**
	 * Test to see whether the given node has any child folders
	 *
	 * @param HierarchyNode - The node to test the children of
	 * @param bRecurse - True to recurse into sub-folders, false to only check the given node
	 *
	 * @return True if the node has child folders, false otherwise
	 */
	static bool HasFoldersRecursive(const TSharedRef<FNativeClassHierarchyNode>& HierarchyNode, const bool bRecurse = true);

	/**
	 * Update OutClasses with any classes that are children of the given node
	 *
	 * @param HierarchyNode - The node to add the children of
	 * @param OutClasses - Array to be populated with the child classes
	 * @param bRecurse - True to recurse into sub-folders, false to only check the given node
	 */
	static void GetClassesRecursive(const TSharedRef<FNativeClassHierarchyNode>& HierarchyNode, TArray<UClass*>& OutClasses, const bool bRecurse = true);

	/**
	 * Update OutFolders with any folders that are children of the given node
	 *
	 * @param HierarchyNode - The node to add the children of
	 * @param OutFolders - Array to be populated with the child folders
	 * @param bRecurse - True to recurse into sub-folders, false to only check the given node
	 */
	static void GetFoldersRecursive(const TSharedRef<FNativeClassHierarchyNode>& HierarchyNode, TArray<FString>& OutFolders, const bool bRecurse = true);

	/**
	 * Populate OutMatchingNodes with the nodes that correspond to the given class paths
	 *
	 * @param InClassPaths - The class paths to find the nodes for, or an empty list to get all root nodes
	 * @param OutMatchingNodes - Array to be populated the nodes that correspond to the given class paths
	 */
	void GatherMatchingNodesForPaths(const TArrayView<const FName>& InClassPaths, TArray<TSharedRef<FNativeClassHierarchyNode>, TInlineAllocator<4>>& OutMatchingNodes) const;

	/**
	 * Completely clear and re-populate the known class hierarchy
	 */
	void PopulateHierarchy();

	/**
	 * Append any classes from the given module to the known class hierarchy
	 *
	 * @param InModuleName - The name of the module to add
	 */
	void AddClassesForModule(const FName& InModuleName);

	/**
	 * Remove any classes in the given module from the known class hierarchy
	 *
	 * @param InModuleName - The name of the module to remove
	 */
	void RemoveClassesForModule(const FName& InModuleName);

	/**
	 * Add a single class to the known class hierarchy, creating any folders as required
	 *
	 * @param InClass - The class that is to be added
	 * @param InGameModules - The list of modules that belong to the "/Classes_Game" root
	 * @param InPluginModules - The list of modules that belong to neither the "/Classes_Game" or "/Classes_Engine" roots
	 * @param AddClassMetrics - Metrics to update as new classes and folders are added (used to report population performance)
	 */
	void AddClass(UClass* InClass, const TSet<FName>& InGameModules, const TMap<FName, FNativeClassHierarchyPluginModuleInfo>& InPluginModules, FAddClassMetrics& AddClassMetrics);

	/**
	 * Called when we're notified that a module has changed status
	 *
	 * @param InModuleName - The module that changed status
	 * @param InModuleChangeReason - Why the module changed status
	 */
	void OnModulesChanged(FName InModuleName, EModuleChangeReason InModuleChangeReason);

	/**
	 * Called when we're notified that a module has been hot-reloaded
	 *
	 * @param bWasTriggeredAutomatically - True if the hot-reload was automatically triggered, or false if it was from a user action
	 */
	void OnHotReload(bool bWasTriggeredAutomatically);

	/**
	 * Given a class, work out which module it belongs to
	 *
	 * @param InClass - The class we want to find the module for
	 *
	 * @return The name of the module that holds the class, eg) "CoreUObject"
	 */
	static FName GetClassModuleName(UClass* InClass);

	/**
	 * Given a module, work out which root path it should use as a parent
	 *
	 * @param InModuleName - The module we want to find the root path for
	 * @param InGameModules - The list of modules that belong to the "/Classes_Game" root
	 * @param InPluginModules - The list of modules that belong to neither the "/Classes_Game" or "/Classes_Engine" roots
	 * @param WhereLoadedFrom - Determine where this class was loaded from.
	 * 
	 * @return The name of the root path to use, eg "/Classes_Game"
	 */
	static FName GetClassPathRootForModule(const FName& InModuleName, const TSet<FName>& InGameModules, const TMap<FName, FNativeClassHierarchyPluginModuleInfo>& InPluginModules, EPluginLoadedFrom& OutWhereLoadedFrom);

	/**
	 * Calculate the list of modules that belong to the "/Classes_Game" root
	 */
	static TSet<FName> GetGameModules();

	/**
	 * Calculate the list of modules that belong to neither the "/Classes_Game" or "/Classes_Engine" roots
	 */
	static TMap<FName, FNativeClassHierarchyPluginModuleInfo> GetPluginModules();

	/** Root level nodes corresponding to the root folders used by the Content Browser, eg) Classes_Engine, Classes_Game, etc */
	TMap<FName, TSharedPtr<FNativeClassHierarchyNode>> RootNodes;

	/** Delegate called when the class hierarchy is updated */
	FSimpleMulticastDelegate ClassHierarchyUpdatedDelegate;
};
