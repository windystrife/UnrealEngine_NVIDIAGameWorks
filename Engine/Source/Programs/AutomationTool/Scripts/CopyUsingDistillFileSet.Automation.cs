// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;using System.Collections.Generic;using System.Linq;using System.Text;using System.Threading.Tasks;using System.IO;using AutomationTool;using UnrealBuildTool;
using Tools.DotNETCommon;

public class CopyUsingDistillFileSet : BuildCommand{	public override void ExecuteBuild()	{		// Command parameters - not all required if using existing manifest		string ProjectPath = ParseParamValue("ProjectPath");		string ManifestFile = ParseParamValue("ManifestFile");		string UE4Exe = ParseParamValue("UE4Exe", "UE4Editor-Cmd.exe");		string Maps = ParseParamValue("Maps");		string Parameters = ParseParamValue("Parameters");		DirectoryReference FromDir = new DirectoryReference(ParseParamValue("FromDir"));		DirectoryReference ToDir = new DirectoryReference(ParseParamValue("ToDir"));
		bool UseExistingManifest = ParseParam("UseExistingManifest");

		List<string> SourceFiles;
		if (UseExistingManifest && FileExists_NoExceptions(ManifestFile))
		{
			// Read Source Files from Manifest
			List<string> Lines = new List<string>(ReadAllLines(ManifestFile));
			if (Lines.Count < 1)
			{
				throw new AutomationException("Manifest file {0} does not list any files.", ManifestFile);
			}
			SourceFiles = new List<string>();
			foreach (var ThisFile in Lines)
			{
				var TestFile = CombinePaths(ThisFile);
				if (!FileExists_NoExceptions(TestFile))
				{
					throw new AutomationException("GenerateDistillFileSets produced {0}, but {1} doesn't exist.", ThisFile, TestFile);
				}
				// we correct the case here
				var TestFileInfo = new FileInfo(TestFile);
				var FinalFile = CombinePaths(TestFileInfo.FullName);
				if (!FileExists_NoExceptions(FinalFile))
				{
					throw new AutomationException("GenerateDistillFileSets produced {0}, but {1} doesn't exist.", ThisFile, FinalFile);
				}
				SourceFiles.Add(FinalFile);
			}
		}
		else
		{
			// Run commandlet to get files required for maps
			FileReference Project = new FileReference(ProjectPath);
			string[] SplitMaps = null;
			if (!String.IsNullOrEmpty(Maps))
			{
				SplitMaps = Maps.Split(new char[] { '+', ';' }, StringSplitOptions.RemoveEmptyEntries).ToArray();
			}
			SourceFiles = GenerateDistillFileSetsCommandlet(Project, ManifestFile, UE4Exe, SplitMaps, Parameters);
		}
		// Convert Source file paths to output paths and copy		IEnumerable<FileReference> TargetFiles = SourceFiles.Select(x => FileReference.Combine(ToDir, new FileReference(x).MakeRelativeTo(FromDir)));		CommandUtils.ThreadedCopyFiles(SourceFiles, TargetFiles.Select(x => x.FullName).ToList());	}};