// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Encapsulates the environment that is used to link object files.
	/// </summary>
	class LinkEnvironment
	{
		/// <summary>
		/// The platform to be compiled/linked for.
		/// </summary>
		public readonly CppPlatform Platform;

		/// <summary>
		/// The configuration to be compiled/linked for.
		/// </summary>
		public readonly CppConfiguration Configuration;

		/// <summary>
		/// The architecture that is being compiled/linked (empty string by default)
		/// </summary>
		public readonly string Architecture;

		/// <summary>
		/// On Mac, indicates the path to the target's application bundle
		/// </summary>
		public DirectoryReference BundleDirectory;

		/// <summary>
		/// The directory to put the non-executable files in (PDBs, import library, etc)
		/// </summary>
		public DirectoryReference OutputDirectory;

		/// <summary>
		/// Intermediate file directory
		/// </summary>
		public DirectoryReference IntermediateDirectory;

		/// <summary>
		/// The directory to shadow source files in for syncing to remote compile servers
		/// </summary>
		public DirectoryReference LocalShadowDirectory = null;

		/// <summary>
		/// The file path for the executable file that is output by the linker.
		/// </summary>
		public List<FileReference> OutputFilePaths = new List<FileReference>();

		/// <summary>
		/// Returns the OutputFilePath is there is only one entry in OutputFilePaths
		/// </summary>
		public FileReference OutputFilePath
		{
			get
			{
				if (OutputFilePaths.Count != 1)
				{
					throw new BuildException("Attempted to use LinkEnvironmentConfiguration.OutputFilePath property, but there are multiple (or no) OutputFilePaths. You need to handle multiple in the code that called this (size = {0})", OutputFilePaths.Count);
				}
				return OutputFilePaths[0];
			}
		}

		/// <summary>
		/// The project file for this target
		/// </summary>
		public FileReference ProjectFile = null;

		/// <summary>
		/// A list of the paths used to find libraries.
		/// </summary>
		public List<string> LibraryPaths = new List<string>();

		/// <summary>
		/// A list of libraries to exclude from linking.
		/// </summary>
		public List<string> ExcludedLibraries = new List<string>();

		/// <summary>
		/// A list of additional libraries to link in.
		/// </summary>
		public List<string> AdditionalLibraries = new List<string>();

		/// <summary>
		/// A list of additional frameworks to link in.
		/// </summary>
		public List<UEBuildFramework> AdditionalFrameworks = new List<UEBuildFramework>();

		/// <summary>
		/// For builds that execute on a remote machine (e.g. iPhone), this list contains additional files that
		/// need to be copied over in order for the app to link successfully.  Source/header files and PCHs are
		/// automatically copied.  Usually this is simply a list of precompiled third party library dependencies.
		/// </summary>
		public List<string> AdditionalShadowFiles = new List<string>();

		/// <summary>
		/// The iOS/Mac frameworks to link in
		/// </summary>
		public List<string> Frameworks = new List<string>();
		public List<string> WeakFrameworks = new List<string>();

		/// <summary>
		/// iOS/Mac resources that should be copied to the app bundle
		/// </summary>
		public List<UEBuildBundleResource> AdditionalBundleResources = new List<UEBuildBundleResource>();

		/// <summary>
		/// A list of the dynamically linked libraries that shouldn't be loaded until they are first called
		/// into.
		/// </summary>
		public List<string> DelayLoadDLLs = new List<string>();

		/// <summary>
		/// Additional arguments to pass to the linker.
		/// </summary>
		public string AdditionalArguments = "";

		/// <summary>
		/// True if debug info should be created.
		/// </summary>
		public bool bCreateDebugInfo = true;

		/// <summary>
		/// True if debug symbols that are cached for some platforms should not be created.
		/// </summary>
		public bool bDisableSymbolCache = false;

		/// <summary>
		/// True if we're compiling .cpp files that will go into a library (.lib file)
		/// </summary>
		public bool bIsBuildingLibrary = false;

		/// <summary>
		/// True if we're compiling a DLL
		/// </summary>
		public bool bIsBuildingDLL = false;

		/// <summary>
		/// True if this is a console application that's being build
		/// </summary>
		public bool bIsBuildingConsoleApplication = false;

		/// <summary>
		/// This setting is replaced by UEBuildBinaryConfiguration.bBuildAdditionalConsoleApp.
		/// </summary>
		[Obsolete("This setting is replaced by UEBuildBinaryConfiguration.bBuildAdditionalConsoleApp. It is explicitly set to true for editor targets, and defaults to false otherwise.")]
		public bool bBuildAdditionalConsoleApplication { set { } }

		/// <summary>
		/// If set, overrides the program entry function on Windows platform.  This is used by the base UE4
		/// program so we can link in either command-line mode or windowed mode without having to recompile the Launch module.
		/// </summary>
		public string WindowsEntryPointOverride = String.Empty;

		/// <summary>
		/// True if we're building a EXE/DLL target with an import library, and that library is needed by a dependency that
		/// we're directly dependent on.
		/// </summary>
		public bool bIsCrossReferenced = false;

		/// <summary>
		/// True if we should include dependent libraries when building a static library
		/// </summary>
		public bool bIncludeDependentLibrariesInLibrary = false;

		/// <summary>
		/// True if the application we're linking has any exports, and we should be expecting the linker to
		/// generate a .lib and/or .exp file along with the target output file
		/// </summary>
		public bool bHasExports = true;

		/// <summary>
		/// True if we're building a .NET assembly (e.g. C# project)
		/// </summary>
		public bool bIsBuildingDotNetAssembly = false;

		/// <summary>
		/// The default stack memory size allocation
		/// </summary>
		public int DefaultStackSize = 5000000;

		/// <summary>
		/// The amount of the default stack size to commit initially. Set to 0 to allow the OS to decide.
		/// </summary>
		public int DefaultStackSizeCommit = 0;

		/// <summary>
		/// Whether to optimize for minimal code size
		/// </summary>
		public bool bOptimizeForSize = false;

		/// <summary>
		/// Whether to omit frame pointers or not. Disabling is useful for e.g. memory profiling on the PC
		/// </summary>
		public bool bOmitFramePointers = true;

		/// <summary>
		/// Whether to support edit and continue.  Only works on Microsoft compilers in 32-bit compiles.
		/// </summary>
		public bool bSupportEditAndContinue;

		/// <summary>
		/// Whether to use incremental linking or not.
		/// </summary>
		public bool bUseIncrementalLinking;

		/// <summary>
		/// Whether to allow the use of LTCG (link time code generation) 
		/// </summary>
		public bool bAllowLTCG;

		/// <summary>
		/// Whether to request the linker create a map file as part of the build
		/// </summary>
		public bool bCreateMapFile;

		/// <summary>
		/// Whether to allow the use of ASLR (address space layout randomization) if supported.
		/// </summary>
		public bool bAllowALSR;

		/// <summary>
		/// Whether PDB files should be used for Visual C++ builds.
		/// </summary>
		public bool bUsePDBFiles;

		/// <summary>
		/// Whether to use the :FASTLINK option when building with /DEBUG to create local PDBs
		/// </summary>
		public bool bUseFastPDBLinking;

		/// <summary>
		/// Whether to log detailed timing information
		/// </summary>
		public bool bPrintTimingInfo;

		/// <summary>
		/// Bundle version for Mac apps
		/// </summary>
		public string BundleVersion;

		/// <summary>
		/// Whether we're linking in monolithic mode. Determines if linking should produce import library file. Relevant only for VC++, clang stores imports in shared library.
		/// </summary>
		public bool bShouldCompileMonolithic = false;

		/// <summary>
		/// A list of the object files to be linked.
		/// </summary>
		public List<FileItem> InputFiles = new List<FileItem>();

		/// <summary>
		/// A list of dependent static or import libraries that need to be linked.
		/// </summary>
		public List<FileItem> InputLibraries = new List<FileItem>();

		/// <summary>
		/// The default resource file to link in to every binary if a custom one is not provided
		/// </summary>
		public List<FileItem> DefaultResourceFiles = new List<FileItem>();

		/// <summary>
		/// Resource files which should be compiled into every binary
		/// </summary>
		public List<FileItem> CommonResourceFiles = new List<FileItem>();

		/// <summary>
		/// Provides a Module Definition File (.def) to the linker to describe various attributes of a DLL.
		/// Necessary when exporting functions by ordinal values instead of by name.
		/// </summary>
		public string ModuleDefinitionFile;

		/// <summary>
		/// Default constructor.
		/// </summary>
		public LinkEnvironment(CppPlatform Platform, CppConfiguration Configuration, string Architecture)
		{
			this.Platform = Platform;
			this.Configuration = Configuration;
			this.Architecture = Architecture;
		}

		/// <summary>
		/// Copy constructor.
		/// </summary>
		public LinkEnvironment(LinkEnvironment Other)
		{
			Platform = Other.Platform;
			Configuration = Other.Configuration;
			Architecture = Other.Architecture;
			BundleDirectory = Other.BundleDirectory;
			OutputDirectory = Other.OutputDirectory;
			IntermediateDirectory = Other.IntermediateDirectory;
			LocalShadowDirectory = Other.LocalShadowDirectory;
			OutputFilePaths = Other.OutputFilePaths.ToList();
			ProjectFile = Other.ProjectFile;
			LibraryPaths.AddRange(Other.LibraryPaths);
			ExcludedLibraries.AddRange(Other.ExcludedLibraries);
			AdditionalLibraries.AddRange(Other.AdditionalLibraries);
			Frameworks.AddRange(Other.Frameworks);
			AdditionalShadowFiles.AddRange(Other.AdditionalShadowFiles);
			AdditionalFrameworks.AddRange(Other.AdditionalFrameworks);
			WeakFrameworks.AddRange(Other.WeakFrameworks);
			AdditionalBundleResources.AddRange(Other.AdditionalBundleResources);
			DelayLoadDLLs.AddRange(Other.DelayLoadDLLs);
			AdditionalArguments = Other.AdditionalArguments;
			bCreateDebugInfo = Other.bCreateDebugInfo;
			bIsBuildingLibrary = Other.bIsBuildingLibrary;
            bDisableSymbolCache = Other.bDisableSymbolCache;
			bIsBuildingDLL = Other.bIsBuildingDLL;
			bIsBuildingConsoleApplication = Other.bIsBuildingConsoleApplication;
			WindowsEntryPointOverride = Other.WindowsEntryPointOverride;
			bIsCrossReferenced = Other.bIsCrossReferenced;
			bHasExports = Other.bHasExports;
			bIsBuildingDotNetAssembly = Other.bIsBuildingDotNetAssembly;
			DefaultStackSize = Other.DefaultStackSize;
			DefaultStackSizeCommit = Other.DefaultStackSizeCommit;
			bOptimizeForSize = Other.bOptimizeForSize;
			bOmitFramePointers = Other.bOmitFramePointers;
			bSupportEditAndContinue = Other.bSupportEditAndContinue;
			bUseIncrementalLinking = Other.bUseIncrementalLinking;
			bAllowLTCG = Other.bAllowLTCG;
			bCreateMapFile = Other.bCreateMapFile;
			bAllowALSR = Other.bAllowALSR;
			bUsePDBFiles = Other.bUsePDBFiles;
			bUseFastPDBLinking = Other.bUseFastPDBLinking;
			bPrintTimingInfo = Other.bPrintTimingInfo;
			BundleVersion = Other.BundleVersion;
			InputFiles.AddRange(Other.InputFiles);
			InputLibraries.AddRange(Other.InputLibraries);
			DefaultResourceFiles.AddRange(Other.DefaultResourceFiles);
			CommonResourceFiles.AddRange(Other.CommonResourceFiles);
			ModuleDefinitionFile = Other.ModuleDefinitionFile;
        }
	}
}
