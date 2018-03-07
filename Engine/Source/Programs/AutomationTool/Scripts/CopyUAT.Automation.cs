// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

/// <summary>
/// Copies all UAT and UBT build products to a directory
/// </summary>
public class CopyUAT : BuildCommand
{
	public override void ExecuteBuild()
	{
		// Get the output directory
		string TargetDirParam = ParseParamValue("TargetDir");
		if(TargetDirParam == null)
		{
			throw new AutomationException("Missing -TargetDir=... argument to CopyUAT");
		}

		// Construct a dummy UE4Build object to get a list of the UAT and UBT build products
		UE4Build Build = new UE4Build(this);
		Build.AddUATFilesToBuildProducts();
		if(ParseParam("WithLauncher"))
		{
			Build.AddUATLauncherFilesToBuildProducts();
		}
		Build.AddUBTFilesToBuildProducts();

		// Get a list of all the input files
		List<FileReference> SourceFiles = new List<FileReference>();
		foreach(string BuildProductFile in Build.BuildProductFiles)
		{
			FileReference SourceFile = new FileReference(BuildProductFile);
			SourceFiles.Add(SourceFile);

			FileReference SourceSymbolFile = SourceFile.ChangeExtension(".pdb");
			if(FileReference.Exists(SourceSymbolFile))
			{
				SourceFiles.Add(SourceSymbolFile);
			}

			FileReference DocumentationFile = SourceFile.ChangeExtension(".xml");
			if(FileReference.Exists(DocumentationFile))
			{
				SourceFiles.Add(DocumentationFile);
			}
		}

		// Copy all the files over
		DirectoryReference TargetDir = new DirectoryReference(TargetDirParam);
		foreach(FileReference SourceFile in SourceFiles)
		{
			FileReference TargetFile = FileReference.Combine(TargetDir, SourceFile.MakeRelativeTo(CommandUtils.RootDirectory));
			DirectoryReference.CreateDirectory(TargetFile.Directory);
			CommandUtils.CopyFile(SourceFile.FullName, TargetFile.FullName);
		}

		Log("Copied {0} files to {1}", SourceFiles.Count, TargetDir);
	}
}
