// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// The platforms that may be compilation targets for C++ files.
	/// </summary>
	enum CppPlatform
	{
		Win32,
		Win64,
		Mac,
		XboxOne,
		PS4,
		Android,
		IOS,
		HTML5,
		Linux,
		TVOS,
		Switch,
	}

	/// <summary>
	/// Compiler configuration. This controls whether to use define debug macros and other compiler settings. Note that optimization level should be based on the bOptimizeCode variable rather than
	/// this setting, so it can be modified on a per-module basis without introducing an incompatibility between object files or PCHs.
	/// </summary>
	enum CppConfiguration
	{
		Debug,
		Development,
		Shipping
	}

	/// <summary>
	/// The optimization level that may be compilation targets for C# files.
	/// </summary>
	enum CSharpTargetConfiguration
	{
		Debug,
		Development,
	}

	/// <summary>
	/// The possible interactions between a precompiled header and a C++ file being compiled.
	/// </summary>
	enum PrecompiledHeaderAction
	{
		None,
		Include,
		Create
	}

	/// <summary>
	/// Encapsulates the compilation output of compiling a set of C++ files.
	/// </summary>
	class CPPOutput
	{
		public List<FileItem> ObjectFiles = new List<FileItem>();
		public List<FileItem> DebugDataFiles = new List<FileItem>();
		public FileItem PrecompiledHeaderFile = null;
	}

	/// <summary>
	/// Encapsulates the environment that a C++ file is compiled in.
	/// </summary>
	class CppCompileEnvironment
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
		/// The directory to put the output object/debug files in.
		/// </summary>
		public DirectoryReference OutputDirectory = null;

		/// <summary>
		/// The directory to put precompiled header files in. Experimental setting to allow using a path on a faster drive. Defaults to the standard output directory if not set.
		/// </summary>
		public DirectoryReference PCHOutputDirectory = null;

		/// <summary>
		/// The directory to shadow source files in for syncing to remote compile servers
		/// </summary>
		public DirectoryReference LocalShadowDirectory = null;

		/// <summary>
		/// The name of the header file which is precompiled.
		/// </summary>
		public FileReference PrecompiledHeaderIncludeFilename = null;

		/// <summary>
		/// Whether the compilation should create, use, or do nothing with the precompiled header.
		/// </summary>
		public PrecompiledHeaderAction PrecompiledHeaderAction = PrecompiledHeaderAction.None;

		/// <summary>
		/// Use run time type information
		/// </summary>
		public bool bUseRTTI = false;

		/// <summary>
		/// Use AVX instructions
		/// </summary>
		public bool bUseAVX = false;

		/// <summary>
		/// Enable buffer security checks.   This should usually be enabled as it prevents severe security risks.
		/// </summary>
		public bool bEnableBufferSecurityChecks = true;

		/// <summary>
		/// If true and unity builds are enabled, this module will build without unity.
		/// </summary>
		public bool bFasterWithoutUnity = false;

		/// <summary>
		/// The number of source files in this module before unity build will be activated for that module.  If set to
		/// anything besides -1, will override the default setting which is controlled by MinGameModuleSourceFilesForUnityBuild
		/// </summary>
		public int MinSourceFilesForUnityBuildOverride = 0;

		/// <summary>
		/// The minimum number of files that must use a pre-compiled header before it will be created and used.
		/// </summary>
		public int MinFilesUsingPrecompiledHeaderOverride = 0;

		/// <summary>
		/// Module uses a #import so must be built locally when compiling with SN-DBS
		/// </summary>
		public bool bBuildLocallyWithSNDBS = false;

		/// <summary>
		/// Enable exception handling
		/// </summary>
		public bool bEnableExceptions = false;

		/// <summary>
		/// Enable objective C exception handling
		/// </summary>
		public bool bEnableObjCExceptions = false;

		/// <summary>
		/// Whether to warn about the use of shadow variables
		/// </summary>
		public bool bEnableShadowVariableWarnings = false;

		/// <summary>
		/// Whether to treat shadow variable warnings as errors.
		/// </summary>
		public bool bShadowVariableWarningsAsErrors = false;

		/// <summary>
		/// Whether to warn about the use of undefined identifiers in #if expressions
		/// </summary>
		public bool bEnableUndefinedIdentifierWarnings = false;

		/// <summary>
		/// Whether to treat undefined identifier warnings as errors.
		/// </summary>
		public bool bUndefinedIdentifierWarningsAsErrors = false;

		/// <summary>
		/// True if compiler optimizations should be enabled. This setting is distinct from the configuration (see CPPTargetConfiguration).
		/// </summary>
		public bool bOptimizeCode = false;

		/// <summary>
		/// Whether to optimize for minimal code size
		/// </summary>
		public bool bOptimizeForSize = false;

		/// <summary>
		/// True if debug info should be created.
		/// </summary>
		public bool bCreateDebugInfo = true;

		/// <summary>
		/// True if we're compiling .cpp files that will go into a library (.lib file)
		/// </summary>
		public bool bIsBuildingLibrary = false;

		/// <summary>
		/// True if we're compiling a DLL
		/// </summary>
		public bool bIsBuildingDLL = false;

		/// <summary>
		/// Whether we should compile using the statically-linked CRT. This is not widely supported for the whole engine, but is required for programs that need to run without dependencies.
		/// </summary>
		public bool bUseStaticCRT = false;

		/// <summary>
		/// Whether to use the debug CRT in debug configurations
		/// </summary>
		public bool bUseDebugCRT = false;

		/// <summary>
		/// Whether to omit frame pointers or not. Disabling is useful for e.g. memory profiling on the PC
		/// </summary>
		public bool bOmitFramePointers = true;

		/// <summary>
		/// Whether we should compile with support for OS X 10.9 Mavericks. Used for some tools that we need to be compatible with this version of OS X.
		/// </summary>
		public bool bEnableOSX109Support = false;

		/// <summary>
		/// Whether PDB files should be used for Visual C++ builds.
		/// </summary>
		public bool bUsePDBFiles = false;

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
		/// Whether to log detailed timing info from the compiler
		/// </summary>
		public bool bPrintTimingInfo;

		/// <summary>
		/// When enabled, allows XGE to compile pre-compiled header files on remote machines.  Otherwise, PCHs are always generated locally.
		/// </summary>
		public bool bAllowRemotelyCompiledPCHs = false;

		/// <summary>
		/// The include paths to look for included files in.
		/// </summary>
		public readonly CppIncludePaths IncludePaths = new CppIncludePaths();

		/// <summary>
		/// List of header files to force include
		/// </summary>
		public List<FileReference> ForceIncludeFiles = new List<FileReference>();

		/// <summary>
		/// The C++ preprocessor definitions to use.
		/// </summary>
		public List<string> Definitions = new List<string>();

		/// <summary>
		/// Additional arguments to pass to the compiler.
		/// </summary>
		public string AdditionalArguments = "";

		/// <summary>
		/// A list of additional frameworks whose include paths are needed.
		/// </summary>
		public List<UEBuildFramework> AdditionalFrameworks = new List<UEBuildFramework>();

		/// <summary>
		/// The file containing the precompiled header data.
		/// </summary>
		public FileItem PrecompiledHeaderFile = null;

		/// <summary>
		/// Header file cache for this target
		/// </summary>
		public CPPHeaders Headers;

		/// <summary>
		/// Whether or not UHT is being built
		/// </summary>
		public bool bHackHeaderGenerator;

		/// <summary>
		/// Default constructor.
		/// </summary>
        public CppCompileEnvironment(CppPlatform Platform, CppConfiguration Configuration, string Architecture, CPPHeaders Headers)
		{
			this.Platform = Platform;
			this.Configuration = Configuration;
			this.Architecture = Architecture;
			this.Headers = Headers;
		}

		/// <summary>
		/// Copy constructor.
		/// </summary>
		/// <param name="Other">Environment to copy settings from</param>
		public CppCompileEnvironment(CppCompileEnvironment Other)
		{
			Platform = Other.Platform;
			Configuration = Other.Configuration;
			Architecture = Other.Architecture;
			OutputDirectory = Other.OutputDirectory;
			PCHOutputDirectory = Other.PCHOutputDirectory;
			LocalShadowDirectory = Other.LocalShadowDirectory;
			PrecompiledHeaderIncludeFilename = Other.PrecompiledHeaderIncludeFilename;
			PrecompiledHeaderAction = Other.PrecompiledHeaderAction;
			bUseRTTI = Other.bUseRTTI;
			bUseAVX = Other.bUseAVX;
			bFasterWithoutUnity = Other.bFasterWithoutUnity;
			MinSourceFilesForUnityBuildOverride = Other.MinSourceFilesForUnityBuildOverride;
			MinFilesUsingPrecompiledHeaderOverride = Other.MinFilesUsingPrecompiledHeaderOverride;
			bBuildLocallyWithSNDBS = Other.bBuildLocallyWithSNDBS;
			bEnableExceptions = Other.bEnableExceptions;
			bEnableObjCExceptions = Other.bEnableObjCExceptions;
			bShadowVariableWarningsAsErrors = Other.bShadowVariableWarningsAsErrors;
			bEnableShadowVariableWarnings = Other.bEnableShadowVariableWarnings;
			bUndefinedIdentifierWarningsAsErrors = Other.bUndefinedIdentifierWarningsAsErrors;
			bEnableUndefinedIdentifierWarnings = Other.bEnableUndefinedIdentifierWarnings;
			bOptimizeCode = Other.bOptimizeCode;
			bOptimizeForSize = Other.bOptimizeForSize;
			bCreateDebugInfo = Other.bCreateDebugInfo;
			bIsBuildingLibrary = Other.bIsBuildingLibrary;
			bIsBuildingDLL = Other.bIsBuildingDLL;
			bUseStaticCRT = Other.bUseStaticCRT;
			bUseDebugCRT = Other.bUseDebugCRT;
			bOmitFramePointers = Other.bOmitFramePointers;
			bEnableOSX109Support = Other.bEnableOSX109Support;
			bUsePDBFiles = Other.bUsePDBFiles;
			bSupportEditAndContinue = Other.bSupportEditAndContinue;
			bUseIncrementalLinking = Other.bUseIncrementalLinking;
			bAllowLTCG = Other.bAllowLTCG;
			bPrintTimingInfo = Other.bPrintTimingInfo;
			bAllowRemotelyCompiledPCHs = Other.bAllowRemotelyCompiledPCHs;
			IncludePaths = new CppIncludePaths(Other.IncludePaths);
			ForceIncludeFiles.AddRange(Other.ForceIncludeFiles);
			Definitions.AddRange(Other.Definitions);
			AdditionalArguments = Other.AdditionalArguments;
			AdditionalFrameworks.AddRange(Other.AdditionalFrameworks);
			PrecompiledHeaderFile = Other.PrecompiledHeaderFile;
			Headers = Other.Headers;
			bHackHeaderGenerator = Other.bHackHeaderGenerator;
		}
	}
}
