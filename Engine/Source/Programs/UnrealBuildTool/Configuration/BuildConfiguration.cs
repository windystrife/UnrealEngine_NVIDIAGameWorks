// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Xml;
using System.Reflection;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class BuildConfiguration
	{
		/// <summary>
		/// Whether to ignore import library files that are out of date when building targets.  Set this to true to improve iteration time.
		/// By default we don't bother relinking targets if only a dependent .lib has changed, as chances are that
		/// the import library wasn't actually different unless a dependent header file of this target was also changed,
		/// in which case the target would be rebuilt automatically.
		/// </summary>
		[XmlConfigFile]
		public bool bIgnoreOutdatedImportLibraries = true;

		/// <summary>
		/// Whether to generate command line dependencies for compile actions when requested
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-SkipActionHistory", Value = "false")]
		public bool bUseActionHistory = true;

		/// <summary>
		/// Whether debug info should be written to the console.
		/// </summary>
		[XmlConfigFile]
		public bool bPrintDebugInfo = false;

		/// <summary>
		/// Allows logging to a file
		/// </summary>
		[XmlConfigFile]
		public string LogFilename;

		/// <summary>
		/// Prints performance diagnostics about include dependencies and other bits
		/// </summary>
		[XmlConfigFile]
		public bool bPrintPerformanceInfo = false;

		/// <summary>
		/// Whether to log detailed action stats. This forces local execution.
		/// </summary>
		[XmlConfigFile]
		public bool bLogDetailedActionStats = false;

		/// <summary>
		/// Whether XGE may be used.
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-NoXGE", Value = "false")]
		public bool bAllowXGE = true;

		/// <summary>
		/// Whether we should just export the XGE XML and pretend it succeeded
		/// </summary>
		public bool bXGEExport;

		/// <summary>
		/// Whether SN-DBS may be used.
		/// </summary>
		[XmlConfigFile]
		public bool bAllowSNDBS = true;

		/// <summary>
		/// Whether or not to delete outdated produced items.
		/// </summary>
		[XmlConfigFile]
		public bool bShouldDeleteAllOutdatedProducedItems = false;

		/// <summary>
		/// What level of logging we wish to show
		/// </summary>
		[XmlConfigFile]
		public string LogLevel = "Log";

		/// <summary>
		/// Whether we should export a JSON file containing detailed target information.
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-JsonExport")]
		public string JsonExportFile = null;

		/// <summary>
		/// Skip building; just do setup and terminate.
		/// </summary>
		[CommandLine("-SkipBuild")]
		public bool bSkipBuild = false;

		/// <summary>
		/// Whether the dependency cache includes pre-resolved include locations so UBT doesn't have to re-resolve each include location just to check the timestamp.
		/// This is technically not fully correct because the dependency cache is global and each module could have a different set of include paths that could cause headers
		/// to resolve files differently. In practice this is not the case, and significantly speeds up UBT when nothing is to be done.
		/// </summary>
		[XmlConfigFile]
		public bool bUseIncludeDependencyResolveCache = true;

		/// <summary>
		/// Used to test the dependency resolve cache. This will verify the resolve cache has no conflicts by resolving every time and checking against any previous resolve attempts.
		/// </summary>
		[XmlConfigFile]
		public bool bTestIncludeDependencyResolveCache = false;

		/// <summary>
		/// Enables support for very fast iterative builds by caching target data.  Turning this on causes Unreal Build Tool to emit
		/// 'UBT Makefiles' for targets when they're built the first time.  Subsequent builds will load these Makefiles and begin
		/// outdatedness checking and build invocation very quickly.  The caveat is that if source files are added or removed to
		/// the project, UBT will need to gather information about those in order for your build to complete successfully.  Currently,
		/// you must run the project file generator after adding/removing source files to tell UBT to re-gather this information.
		/// 
		/// Events that can invalidate the 'UBT Makefile':  
		///		- Adding/removing .cpp files
		///		- Adding/removing .h files with UObjects
		///		- Adding new UObject types to a file that didn't previously have any
		///		- Changing global build settings (most settings in this file qualify.)
		///		- Changed code that affects how Unreal Header Tool works
		///	
		///	You can force regeneration of the 'UBT Makefile' by passing the '-gather' argument, or simply regenerating project files
		///
		///	This also enables the fast include file dependency scanning and caching system that allows Unreal Build Tool to detect out 
		/// of date dependencies very quickly.  When enabled, a deep C++ include graph does not have to be generated, and instead
		/// we only scan and cache indirect includes for after a dependent build product was already found to be out of date.  During the
		/// next build, we'll load those cached indirect includes and check for outdatedness.
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-NoUBTMakefiles", Value = "false")]
		public bool bUseUBTMakefiles = true;

		/// <summary>
		/// Whether DMUCS/Distcc may be used.
		/// Distcc requires some setup - so by default disable it so we don't break local or remote building
		/// </summary>
		[XmlConfigFile]
		public bool bAllowDistcc = false;

		/// <summary>
		/// If specified, we will only build this particular source file, ignore all other outputs.  Useful for testing non-Unity builds.
		/// </summary>
		public string SingleFileToCompile = null;

		/// <summary>
		/// Whether to skip checking for files identified by the junk manifest
		/// </summary>
		[XmlConfigFile]
		public bool bIgnoreJunk = false;

		/// <summary>
		/// Whether to generate a manifest file that contains the files to add to Perforce
		/// </summary>
		[CommandLine("-GenerateManifest")]
		public bool bGenerateManifest = false;

		/// <summary>
		/// Whether to 'clean' the given project
		/// </summary>
		[CommandLine("-Clean")]
		public bool bCleanProject = false;

		/// <summary>
		/// If we are just running the deployment step, specifies the path to the given deployment settings
		/// </summary>
		[CommandLine("-Deploy")]
		public FileReference DeployTargetFile = null;

		/// <summary>
		/// If true, force header regeneration. Intended for the build machine
		/// </summary>
		[CommandLine("-ForceHeaderGeneration")]
		[XmlConfigFile(Category = "UEBuildConfiguration")]
		public bool bForceHeaderGeneration = false;

		/// <summary>
		/// If true, do not build UHT, assume it is already built
		/// </summary>
		[CommandLine("-NoBuildUHT")]
		[XmlConfigFile(Category = "UEBuildConfiguration")]
		public bool bDoNotBuildUHT = false;

		/// <summary>
		/// If true, fail if any of the generated header files is out of date.
		/// </summary>
		[CommandLine("-FailIfGeneratedCodeChanges")]
		[XmlConfigFile(Category = "UEBuildConfiguration")]
		public bool bFailIfGeneratedCodeChanges = false;

		/// <summary>
		/// True if hot-reload from IDE is allowed
		/// </summary>
		[XmlConfigFile(Category = "UEBuildConfiguration")]
		[ConfigFile(ConfigHierarchyType.Engine, "BuildConfiguration")]
		public bool bAllowHotReloadFromIDE = true;

		/// <summary>
		/// If true, the Debug version of UnrealHeaderTool will be build and run instead of the Development version.
		/// </summary>
		[XmlConfigFile(Category = "UEBuildConfiguration")]
		public static bool bForceDebugUnrealHeaderTool = false;

		/// <summary>
		/// When true, the targets won't execute their link actions if there was nothing to compile
		/// </summary>
		[CommandLine("-CanSkipLink")]
		public bool bSkipLinkingWhenNothingToCompile = false;

		/// <summary>
		/// Default constructor. Reads settings from the XmlConfig files.
		/// </summary>
		public BuildConfiguration()
		{
			XmlConfig.ApplyTo(this);
		}
	}
}
