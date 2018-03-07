// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using System.Xml.XPath;
using System.Xml.Linq;
using System.Linq;
using System.Text;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{

	class EddieSourceFile : ProjectFile.SourceFile
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public EddieSourceFile(FileReference InitFilePath, DirectoryReference InitRelativeBaseFolder)
			: base(InitFilePath, InitRelativeBaseFolder)
		{
		}
	}
	
	class EddieFolder
	{
		public EddieFolder(string InName, string InPath)
		{
			Name = InName;
			Path = InPath;
			FullPath = null;
			bIsModuleFolder = false;
		}
		
		public string Name;
		public string Path;
		public string FullPath;
		public string WorksetPath;
		public bool bIsModuleFolder;
		public Dictionary<string, EddieFolder> Folders = new Dictionary<string, EddieFolder>();
		public List<EddieSourceFile> Files = new List<EddieSourceFile>();
	}
	
	class EddieProjectFile : ProjectFile
	{
		FileReference OnlyGameProject;
		
		Dictionary<string, EddieFolder> Folders = new Dictionary<string, EddieFolder>();
		
		public EddieProjectFile(FileReference InitFilePath, FileReference InOnlyGameProject)
			: base(InitFilePath)
		{
			OnlyGameProject = InOnlyGameProject;
		}

		public override string ToString()
		{
			return ProjectFilePath.GetFileNameWithoutExtension();
		}
	
		private bool IsSourceCode(string Extension)
		{
			return Extension == ".c" || Extension == ".cc" || Extension == ".cpp" || Extension == ".m" || Extension == ".mm" || Extension == ".cs";
		}
		
		public override SourceFile AllocSourceFile(FileReference InitFilePath, DirectoryReference InitProjectSubFolder)
		{
			if (InitFilePath.GetFileName().StartsWith("."))
			{
				return null;
			}
			return new EddieSourceFile(InitFilePath, InitProjectSubFolder);
		}
		
		public EddieFolder FindFolderByRelativePath(ref Dictionary<string, EddieFolder> Groups, string RelativePath)
		{
			string[] Parts = RelativePath.Split(Path.DirectorySeparatorChar);
			string CurrentPath = "";
			Dictionary<string, EddieFolder> CurrentParent = Groups;

			foreach (string Part in Parts)
			{
				EddieFolder CurrentGroup;

				if (CurrentPath != "")
				{
					CurrentPath += Path.DirectorySeparatorChar;
				}

				CurrentPath += Part;

				if (!CurrentParent.ContainsKey(CurrentPath))
				{
					CurrentGroup = new EddieFolder(Path.GetFileName(CurrentPath), CurrentPath);
					CurrentParent.Add(CurrentPath, CurrentGroup);
				}
				else
				{
					CurrentGroup = CurrentParent[CurrentPath];
				}

				if (CurrentPath == RelativePath)
				{
					return CurrentGroup;
				}

				CurrentParent = CurrentGroup.Folders;
			}

			return null;
		}
		
		private static string ConvertPath(string InPath)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				if (InPath[1] != ':')
				{
					throw new BuildException("Can only convert full paths ({0})", InPath);
				}

				string MacPath = string.Format("{0}/{1}/{2}/{3}",
					RemoteToolChain.UserDevRootMac,
					Environment.MachineName,
					InPath[0].ToString().ToUpper(),
					InPath.Substring(3));

				// clean the path
				MacPath = MacPath.Replace("\\", "/");

				return MacPath;
			}
			else
			{
				return InPath.Replace("\\", "/");
			}
		}
		
		private void ParseSourceFilesIntoGroups()
		{
			foreach (var CurSourceFile in SourceFiles)
			{
				EddieSourceFile SourceFile = CurSourceFile as EddieSourceFile;
				string FileName = SourceFile.Reference.GetFileName();
				string FileExtension = Path.GetExtension(FileName);
				string FilePath = SourceFile.Reference.MakeRelativeTo(ProjectFilePath.Directory);
				string FilePathMac = Utils.CleanDirectorySeparators(FilePath, '/');
				
				var ProjectRelativeSourceFile = CurSourceFile.Reference.MakeRelativeTo(ProjectFilePath.Directory);
				string RelativeSourceDirectory = Path.GetDirectoryName(ProjectRelativeSourceFile);
				// Use the specified relative base folder
				if (CurSourceFile.BaseFolder != null)	// NOTE: We are looking for null strings, not empty strings!
				{
					RelativeSourceDirectory = Path.GetDirectoryName(CurSourceFile.Reference.MakeRelativeTo(CurSourceFile.BaseFolder));
				}
				EddieFolder Group = FindFolderByRelativePath(ref Folders, RelativeSourceDirectory);
				if (Group != null)
				{
					if (FileName.EndsWith(".build.cs") || FileName.EndsWith(".Build.cs") || FileName.EndsWith(".csproj"))
					{
						Group.bIsModuleFolder = true;
						Group.WorksetPath = ProjectFilePath.FullName + "." + Group.Name + ".wkst";
					}
				
					Group.FullPath = Path.GetDirectoryName(SourceFile.Reference.FullName);
					Group.Files.Add(SourceFile);
				}
			}
		}
		
		private void EmitProject(StringBuilder Content, Dictionary<string, EddieFolder> Folders)
		{
			foreach (var CurGroup in Folders)
			{
                if (Path.GetFileName(CurGroup.Key) != "Documentation")
                {
                    Content.Append("AddFileGroup \"" + Path.GetFileName(CurGroup.Key) + "\" \"" + (CurGroup.Value.FullPath != null ? CurGroup.Value.FullPath : CurGroup.Value.Path) + "\"" + ProjectFileGenerator.NewLine);
                
                    if (CurGroup.Value.bIsModuleFolder)
                    {
                        var ProjectFileContent = new StringBuilder();
                
                        ProjectFileContent.Append("# @Eddie Workset@" + ProjectFileGenerator.NewLine);
                        ProjectFileContent.Append("AddWorkset \"" + Path.GetFileName(CurGroup.Key) + ".wkst\" \"" + CurGroup.Value.WorksetPath + "\"" + ProjectFileGenerator.NewLine);
                
                        ProjectFileContent.Append("AddFileGroup \"" + Path.GetFileName(CurGroup.Key) + "\" \"" + (CurGroup.Value.FullPath != null ? CurGroup.Value.FullPath : CurGroup.Value.Path) + "\"" + ProjectFileGenerator.NewLine);
                
                        EmitProject(ProjectFileContent, CurGroup.Value.Folders);
                        
                        foreach (EddieSourceFile File in CurGroup.Value.Files)
                        {
                            ProjectFileContent.Append("AddFile \"" + File.Reference.GetFileName() + "\" \"" + File.Reference.FullName + "\"" + ProjectFileGenerator.NewLine);
                        }
                        
                        
                        ProjectFileContent.Append("EndFileGroup \"" + Path.GetFileName(CurGroup.Key) + "\"" + ProjectFileGenerator.NewLine);
                
                        ProjectFileGenerator.WriteFileIfChanged(CurGroup.Value.WorksetPath, ProjectFileContent.ToString(), new UTF8Encoding());
                    
                        Content.Append("AddFile \"" + Path.GetFileName(CurGroup.Key) + "\" \"" + CurGroup.Value.WorksetPath + "\"" + ProjectFileGenerator.NewLine);
                    }
                    else
                    {
                        EmitProject(Content, CurGroup.Value.Folders);
                        
                        foreach (EddieSourceFile File in CurGroup.Value.Files)
                        {
                            Content.Append("AddFile \"" + File.Reference.GetFileName() + "\" \"" + File.Reference.FullName + "\"" + ProjectFileGenerator.NewLine);
                        }
                    }
                    
                    Content.Append("EndFileGroup \"" + Path.GetFileName(CurGroup.Key) + "\"" + ProjectFileGenerator.NewLine);
                }
			}
		}
		
		public override bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			bool bSuccess = false;
			
			var TargetName = ProjectFilePath.GetFileNameWithoutExtension();
			
			FileReference GameProjectPath = null;
			foreach(ProjectTarget Target in ProjectTargets)
			{
				if(Target.UnrealProjectFilePath != null)
				{
					GameProjectPath = Target.UnrealProjectFilePath;
					break;
				}
			}
			
			var ProjectFileContent = new StringBuilder();
			
			ProjectFileContent.Append("# @Eddie Workset@" + ProjectFileGenerator.NewLine);
			ProjectFileContent.Append("AddWorkset \"" + this.ToString() + ".wkst\" \"" + ProjectFilePath.FullName + "\"" + ProjectFileGenerator.NewLine);
			
			ParseSourceFilesIntoGroups();
			EmitProject(ProjectFileContent, Folders);
			
			bSuccess = ProjectFileGenerator.WriteFileIfChanged(ProjectFilePath.FullName, ProjectFileContent.ToString(), new UTF8Encoding());
			
			return bSuccess;
		}
	}

}