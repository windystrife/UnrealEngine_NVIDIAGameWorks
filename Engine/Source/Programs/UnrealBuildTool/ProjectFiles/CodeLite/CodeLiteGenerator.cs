// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Xml;
using System.Xml.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	// Represents a folder within the master project. TODO Not using at the moment.
	class CodeLiteFolder : MasterProjectFolder
	{
		public CodeLiteFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
			: base(InitOwnerProjectFileGenerator, InitFolderName)
		{
		}
	}

	class CodeLiteGenerator : ProjectFileGenerator
	{
		public string SolutionExtension = ".workspace";
		public string CodeCompletionFileName = "CodeCompletionFolders.txt";
		public string CodeCompletionPreProcessorFileName = "CodeLitePreProcessor.txt";

		public CodeLiteGenerator(FileReference InOnlyGameProject)
			: base(InOnlyGameProject)
		{
		}

		//
		// Returns CodeLite's project filename extension.
		//
		override public string ProjectFileExtension
		{
			get
			{
				return ".project";
			}
		}
		protected override bool WriteMasterProjectFile(ProjectFile UBTProject)
		{
			var SolutionFileName = MasterProjectName + SolutionExtension;
			var CodeCompletionFile = MasterProjectName + CodeCompletionFileName;
			var CodeCompletionPreProcessorFile = MasterProjectName + CodeCompletionPreProcessorFileName;

			var FullCodeLiteMasterFile = Path.Combine(MasterProjectPath.FullName, SolutionFileName);
			var FullCodeLiteCodeCompletionFile = Path.Combine(MasterProjectPath.FullName, CodeCompletionFile);
			var FullCodeLiteCodeCompletionPreProcessorFile = Path.Combine(MasterProjectPath.FullName, CodeCompletionPreProcessorFile);

			//
			// HACK 
			// TODO This is for now a hack. According to the original submitter, Eranif (a CodeLite developer) will support code completion folders in *.workspace files.
			// We create a separate file with all the folder name in it to copy manually into the code completion
			// filed of CodeLite workspace. (Workspace Settings/Code Completion -> copy the content of the file threre.)
			List<string> IncludeDirectories = new List<string>();
			List<string> PreProcessor = new List<string>();

			foreach (var CurProject in GeneratedProjectFiles)
			{
				CodeLiteProject Project = CurProject as CodeLiteProject;
				if (Project == null)
				{
					continue;
				}

				foreach (var CurrentPath in Project.IntelliSenseIncludeSearchPaths)
				{
					// Convert relative path into absolute.
					DirectoryReference IntelliSenseIncludeSearchPath = DirectoryReference.Combine(Project.ProjectFilePath.Directory, CurrentPath);
					IncludeDirectories.Add(IntelliSenseIncludeSearchPath.FullName);
				}
				foreach (var CurrentPath in Project.IntelliSenseSystemIncludeSearchPaths)
				{
					// Convert relative path into absolute.
					DirectoryReference IntelliSenseSystemIncludeSearchPath = DirectoryReference.Combine(Project.ProjectFilePath.Directory, CurrentPath);
					IncludeDirectories.Add(IntelliSenseSystemIncludeSearchPath.FullName);
				}

				foreach (var CurDef in Project.IntelliSensePreprocessorDefinitions)
				{
					if (!PreProcessor.Contains(CurDef))
					{
						PreProcessor.Add(CurDef);
					}
				}

			}

			//
			// Write code completions data into files.
			//
			File.WriteAllLines(FullCodeLiteCodeCompletionFile, IncludeDirectories);
			File.WriteAllLines(FullCodeLiteCodeCompletionPreProcessorFile, PreProcessor);

			//
			// Write CodeLites Workspace
			//
			XmlWriterSettings settings = new XmlWriterSettings();
			settings.Indent = true;

			XElement CodeLiteWorkspace = new XElement("CodeLite_Workspace");
			XAttribute CodeLiteWorkspaceName = new XAttribute("Name", MasterProjectName);
			XAttribute CodeLiteWorkspaceSWTLW = new XAttribute("SWTLW", "Yes"); // This flag will only work in CodeLite version > 8.0. See below
			CodeLiteWorkspace.Add(CodeLiteWorkspaceName);
			CodeLiteWorkspace.Add(CodeLiteWorkspaceSWTLW);

			//
			// ATTN This part will work for the next release of CodeLite. That may
			// be CodeLite version > 8.0. CodeLite 8.0 does not have this functionality.
			// TODO Macros are ignored for now.
			//
			// Write Code Completion folders into the WorkspaceParserPaths section.
			//
			XElement CodeLiteWorkspaceParserPaths = new XElement("WorkspaceParserPaths");
			foreach (var CurrentPath in IncludeDirectories)
			{
				XElement CodeLiteWorkspaceParserPathInclude = new XElement("Include");
				XAttribute CodeLiteWorkspaceParserPath = new XAttribute("Path", CurrentPath);
				CodeLiteWorkspaceParserPathInclude.Add(CodeLiteWorkspaceParserPath);
				CodeLiteWorkspaceParserPaths.Add(CodeLiteWorkspaceParserPathInclude);

			}
			CodeLiteWorkspace.Add(CodeLiteWorkspaceParserPaths);

			//
			// Write project file information into CodeLite's workspace file.
			//
			foreach (var CurProject in AllProjectFiles)
			{
				var ProjectExtension = CurProject.ProjectFilePath.GetExtension();

				//
				// TODO For now ignore C# project files.
				//
				if (ProjectExtension == ".csproj")
				{
					continue;
				}

				//
				// Iterate through all targets.
				// 
				foreach (ProjectTarget CurrentTarget in CurProject.ProjectTargets)
				{
					string[] tmp = CurrentTarget.ToString().Split('.');
					string ProjectTargetFileName = CurProject.ProjectFilePath.Directory.MakeRelativeTo(MasterProjectPath) + "/" + tmp[0] + ProjectExtension;
					String ProjectName = tmp[0];


					XElement CodeLiteWorkspaceProject = new XElement("Project");
					XAttribute CodeLiteWorkspaceProjectName = new XAttribute("Name", ProjectName);
					XAttribute CodeLiteWorkspaceProjectPath = new XAttribute("Path", ProjectTargetFileName);
					XAttribute CodeLiteWorkspaceProjectActive = new XAttribute("Active", "No");
					CodeLiteWorkspaceProject.Add(CodeLiteWorkspaceProjectName);
					CodeLiteWorkspaceProject.Add(CodeLiteWorkspaceProjectPath);
					CodeLiteWorkspaceProject.Add(CodeLiteWorkspaceProjectActive);
					CodeLiteWorkspace.Add(CodeLiteWorkspaceProject);
				}
			}

			//
			// We need to create the configuration matrix. That will assign the project configuration to 
			// the samge workspace configuration.
			//
			XElement CodeLiteWorkspaceBuildMatrix = new XElement("BuildMatrix");
			foreach (UnrealTargetConfiguration CurConfiguration in SupportedConfigurations)
			{
				if (UnrealBuildTool.IsValidConfiguration(CurConfiguration))
				{
					XElement CodeLiteWorkspaceBuildMatrixConfiguration = new XElement("WorkspaceConfiguration");
					XAttribute CodeLiteWorkspaceProjectName = new XAttribute("Name", CurConfiguration.ToString());
					XAttribute CodeLiteWorkspaceProjectSelected = new XAttribute("Selected", "no");
					CodeLiteWorkspaceBuildMatrixConfiguration.Add(CodeLiteWorkspaceProjectName);
					CodeLiteWorkspaceBuildMatrixConfiguration.Add(CodeLiteWorkspaceProjectSelected);

					foreach (var CurProject in AllProjectFiles)
					{
						var ProjectExtension = CurProject.ProjectFilePath.GetExtension();

						//
						// TODO For now ignore C# project files.
						//
						if (ProjectExtension == ".csproj")
						{
							continue;
						}

						foreach (ProjectTarget target in CurProject.ProjectTargets)
						{
							string[] tmp = target.ToString().Split('.');
							String ProjectName = tmp[0];

							XElement CodeLiteWorkspaceBuildMatrixConfigurationProject = new XElement("Project");
							XAttribute CodeLiteWorkspaceBuildMatrixConfigurationProjectName = new XAttribute("Name", ProjectName);
							XAttribute CodeLiteWorkspaceBuildMatrixConfigurationProjectConfigName = new XAttribute("ConfigName", CurConfiguration.ToString());
							CodeLiteWorkspaceBuildMatrixConfigurationProject.Add(CodeLiteWorkspaceBuildMatrixConfigurationProjectName);
							CodeLiteWorkspaceBuildMatrixConfigurationProject.Add(CodeLiteWorkspaceBuildMatrixConfigurationProjectConfigName);
							CodeLiteWorkspaceBuildMatrixConfiguration.Add(CodeLiteWorkspaceBuildMatrixConfigurationProject);
						}
					}
					CodeLiteWorkspaceBuildMatrix.Add(CodeLiteWorkspaceBuildMatrixConfiguration);
				}
			}

			CodeLiteWorkspace.Add(CodeLiteWorkspaceBuildMatrix);
			CodeLiteWorkspace.Save(FullCodeLiteMasterFile);

			return true;
		}

		protected override ProjectFile AllocateProjectFile(FileReference InitFilePath)
		{
			return new CodeLiteProject(InitFilePath, OnlyGameProject);
		}

		public override MasterProjectFolder AllocateMasterProjectFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
		{
			return new CodeLiteFolder(InitOwnerProjectFileGenerator, InitFolderName);
		}

		public override void CleanProjectFiles(DirectoryReference InMasterProjectDirectory, string InMasterProjectName, DirectoryReference InIntermediateProjectFilesDirectory)
		{
			// TODO Delete all files here. Not finished yet.
			var SolutionFileName = InMasterProjectName + SolutionExtension;
			var CodeCompletionFile = InMasterProjectName + CodeCompletionFileName;
			var CodeCompletionPreProcessorFile = InMasterProjectName + CodeCompletionPreProcessorFileName;

			FileReference FullCodeLiteMasterFile = FileReference.Combine(InMasterProjectDirectory, SolutionFileName);
			FileReference FullCodeLiteCodeCompletionFile = FileReference.Combine(InMasterProjectDirectory, CodeCompletionFile);
			FileReference FullCodeLiteCodeCompletionPreProcessorFile = FileReference.Combine(InMasterProjectDirectory, CodeCompletionPreProcessorFile);

			if (FileReference.Exists(FullCodeLiteMasterFile))
			{
				FileReference.Delete(FullCodeLiteMasterFile);
			}
			if (FileReference.Exists(FullCodeLiteCodeCompletionFile))
			{
				FileReference.Delete(FullCodeLiteCodeCompletionFile);
			}
			if (FileReference.Exists(FullCodeLiteCodeCompletionPreProcessorFile))
			{
				FileReference.Delete(FullCodeLiteCodeCompletionPreProcessorFile);
			}

			// Delete the project files folder
			if (DirectoryReference.Exists(InIntermediateProjectFilesDirectory))
			{
				try
				{
					Directory.Delete(InIntermediateProjectFilesDirectory.FullName, true);
				}
				catch (Exception Ex)
				{
					Log.TraceInformation("Error while trying to clean project files path {0}. Ignored.", InIntermediateProjectFilesDirectory);
					Log.TraceInformation("\t" + Ex.Message);
				}
			}
		}

	}
}
