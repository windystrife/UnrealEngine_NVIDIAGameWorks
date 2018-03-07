// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	abstract class UEToolChain
	{
		/// <summary>
		/// The C++ platform that this toolchain supports
		/// </summary>
		public readonly CppPlatform CppPlatform;

		public UEToolChain(CppPlatform InCppPlatform)
		{
			CppPlatform = InCppPlatform;
		}

		public abstract CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName, ActionGraph ActionGraph);

		public virtual CPPOutput CompileRCFiles(CppCompileEnvironment Environment, List<FileItem> RCFiles, ActionGraph ActionGraph)
		{
			CPPOutput Result = new CPPOutput();
			return Result;
		}

		public abstract FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph);
		public virtual FileItem[] LinkAllFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph)
		{
			return new FileItem[] { LinkFiles(LinkEnvironment, bBuildImportLibraryOnly, ActionGraph) };
		}


		/// <summary>
		/// Get the name of the response file for the current linker environment and output file
		/// </summary>
		/// <param name="LinkEnvironment"></param>
		/// <param name="OutputFile"></param>
		/// <returns></returns>
		public static FileReference GetResponseFileName(LinkEnvironment LinkEnvironment, FileItem OutputFile)
		{
			// Construct a relative path for the intermediate response file
			return FileReference.Combine(LinkEnvironment.IntermediateDirectory, OutputFile.Reference.GetFileName() + ".response");
		}

		public virtual ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment ExecutableLinkEnvironment, ActionGraph ActionGraph)
		{
			return new List<FileItem>();
		}

		public virtual void SetUpGlobalEnvironment(ReadOnlyTargetRules Target)
		{
			ParseProjectSettings();
		}

		public virtual void ParseProjectSettings()
		{
		}

		public virtual void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
		}

		public virtual void FinalizeOutput(ReadOnlyTargetRules Target, List<FileItem> OutputItems, ActionGraph ActionGraph)
		{
		}

		/// <summary>
		/// Adds a build product and its associated debug file to a receipt.
		/// </summary>
		/// <param name="OutputFile">Build product to add</param>
		/// <param name="OutputType">The type of build product</param>
		public virtual bool ShouldAddDebugFileToReceipt(FileReference OutputFile, BuildProductType OutputType)
		{
			return true;
		}

		protected virtual void AddPrerequisiteSourceFile(CppCompileEnvironment CompileEnvironment, FileItem SourceFile, List<FileItem> PrerequisiteItems)
		{
			PrerequisiteItems.Add(SourceFile);

			if (!CompileEnvironment.Headers.bUseUBTMakefiles)	// In fast build iteration mode, we'll gather includes later on
			{
				// @todo ubtmake: What if one of the prerequisite files has become missing since it was updated in our cache? (usually, because a coder eliminated the source file)
				//		-> Two CASES:
				//				1) NOT WORKING: Non-unity file went away (SourceFile in this context).  That seems like an existing old use case.  Compile params or Response file should have changed?
				//				2) WORKING: Indirect file went away (unity'd original source file or include).  This would return a file that no longer exists and adds to the prerequiteitems list
				List<FileItem> IncludedFileList = CompileEnvironment.Headers.FindAndCacheAllIncludedFiles(SourceFile, CompileEnvironment.IncludePaths, bOnlyCachedDependencies: false);
				if (IncludedFileList != null)
				{
					foreach (FileItem IncludedFile in IncludedFileList)
					{
						PrerequisiteItems.Add(IncludedFile);
					}
				}
			}
		}

		public virtual void SetupBundleDependencies(List<UEBuildBinary> Binaries, string GameName)
		{

		}

		public virtual void FixBundleBinariesPaths(UEBuildTarget Target, List<UEBuildBinary> Binaries)
		{

		}

        public virtual string GetSDKVersion()
        {
            return "Not Applicable";
        }
	};
}