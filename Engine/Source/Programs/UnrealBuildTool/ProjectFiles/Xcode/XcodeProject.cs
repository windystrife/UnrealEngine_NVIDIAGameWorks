// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

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
	/// <summary>
	/// Info needed to make a file a member of specific group
	/// </summary>
	class XcodeSourceFile : ProjectFile.SourceFile
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public XcodeSourceFile(FileReference InitFilePath, DirectoryReference InitRelativeBaseFolder)
			: base(InitFilePath, InitRelativeBaseFolder)
		{
			FileGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			FileRefGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
		}

		/// <summary>
		/// File Guid for use in Xcode project
		/// </summary>
		public string FileGuid
		{
			get;
			private set;
		}

		public void ReplaceGuids(string NewFileGuid, string NewFileRefGuid)
		{
			FileGuid = NewFileGuid;
			FileRefGuid = NewFileRefGuid;
		}

		/// <summary>
		/// File reference Guid for use in Xcode project
		/// </summary>
		public string FileRefGuid
		{
			get;
			private set;
		}
	}

	/// <summary>
	/// Represents a group of files shown in Xcode's project navigator as a folder
	/// </summary>
	class XcodeFileGroup
	{
		public XcodeFileGroup(string InName, string InPath, bool InIsReference = false)
		{
			GroupName = InName;
			GroupPath = InPath;
			GroupGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			bIsReference = InIsReference;
		}

		public string GroupGuid;
		public string GroupName;
		public string GroupPath;
		public Dictionary<string, XcodeFileGroup> Children = new Dictionary<string, XcodeFileGroup>();
		public List<XcodeSourceFile> Files = new List<XcodeSourceFile>();
		public bool bIsReference;
	}

	class XcodeProjectFile : ProjectFile
	{
		FileReference OnlyGameProject;

		Dictionary<string, XcodeFileGroup> Groups = new Dictionary<string, XcodeFileGroup>();

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		/// <param name="InOnlyGameProject"></param>
		public XcodeProjectFile(FileReference InitFilePath, FileReference InOnlyGameProject)
			: base(InitFilePath)
		{
			OnlyGameProject = InOnlyGameProject;
		}

		public override string ToString()
		{
			return ProjectFilePath.GetFileNameWithoutExtension();
		}

		/// <summary>
		/// Gets Xcode file category based on its extension
		/// </summary>
		private string GetFileCategory(string Extension)
		{
			// @todo Mac: Handle more categories
			switch (Extension)
			{
				case ".framework":
					return "Frameworks";
				default:
					return "Sources";
			}
		}

		/// <summary>
		/// Gets Xcode file type based on its extension
		/// </summary>
		private string GetFileType(string Extension)
		{
			// @todo Mac: Handle more file types
			switch (Extension)
			{
				case ".c":
				case ".m":
					return "sourcecode.c.objc";
				case ".cc":
				case ".cpp":
				case ".mm":
					return "sourcecode.cpp.objcpp";
				case ".h":
				case ".inl":
				case ".pch":
					return "sourcecode.c.h";
				case ".framework":
					return "wrapper.framework";
				case ".plist":
					return "text.plist.xml";
				case ".png":
					return "image.png";
				case ".icns":
					return "image.icns";
				default:
					return "file.text";
			}
		}

		/// <summary>
		/// Returns true if Extension is a known extension for files containing source code
		/// </summary>
		private bool IsSourceCode(string Extension)
		{
			return Extension == ".c" || Extension == ".cc" || Extension == ".cpp" || Extension == ".m" || Extension == ".mm";
		}

		private bool ShouldIncludeFileInBuildPhaseSection(XcodeSourceFile SourceFile)
		{
			string FileExtension = SourceFile.Reference.GetExtension();

			if (IsSourceCode(FileExtension))
			{
				foreach (string PlatformName in Enum.GetNames(typeof(UnrealTargetPlatform)))
				{
					string AltName = PlatformName == "Win32" || PlatformName == "Win64" ? "windows" : PlatformName.ToLower();
					if ((SourceFile.Reference.FullName.ToLower().Contains("/" + PlatformName.ToLower() + "/") || SourceFile.Reference.FullName.ToLower().Contains("/" + AltName + "/"))
						&& PlatformName != "Mac" && PlatformName != "IOS" && PlatformName != "TVOS")
					{
						// Build phase is used for indexing only and indexing currently works only with files that can be compiled for Mac, so skip files for other platforms
						return false;
					}
				}

				return true;
			}

			return false;
		}

		/// <summary>
		/// Returns a project navigator group to which the file should belong based on its path.
		/// Creates a group tree if it doesn't exist yet.
		/// </summary>
		public XcodeFileGroup FindGroupByRelativePath(ref Dictionary<string, XcodeFileGroup> Groups, string RelativePath)
		{
			string[] Parts = RelativePath.Split(Path.DirectorySeparatorChar);
			string CurrentPath = "";
			Dictionary<string, XcodeFileGroup> CurrentParent = Groups;

			foreach (string Part in Parts)
			{
				XcodeFileGroup CurrentGroup;

				if (CurrentPath != "")
				{
					CurrentPath += Path.DirectorySeparatorChar;
				}

				CurrentPath += Part;

				if (!CurrentParent.ContainsKey(CurrentPath))
				{
					CurrentGroup = new XcodeFileGroup(Path.GetFileName(CurrentPath), CurrentPath);
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

				CurrentParent = CurrentGroup.Children;
			}

			return null;
		}

		/// <summary>
		/// Convert all paths to Apple/Unix format (with forward slashes)
		/// </summary>
		/// <param name="InPath">The path to convert</param>
		/// <returns>The normalized path</returns>
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

		/// <summary>
		/// Allocates a generator-specific source file object
		/// </summary>
		/// <param name="InitFilePath">Path to the source file on disk</param>
		/// <param name="InitProjectSubFolder">Optional sub-folder to put the file in.  If empty, this will be determined automatically from the file's path relative to the project file</param>
		/// <returns>The newly allocated source file object</returns>
		public override SourceFile AllocSourceFile(FileReference InitFilePath, DirectoryReference InitProjectSubFolder)
		{
			if (InitFilePath.GetFileName().StartsWith("."))
			{
				return null;
			}
			return new XcodeSourceFile(InitFilePath, InitProjectSubFolder);
		}

		/// <summary>
		/// Generates bodies of all sections that contain a list of source files plus a dictionary of project navigator groups.
		/// </summary>
		private void GenerateSectionsWithSourceFiles(StringBuilder PBXBuildFileSection, StringBuilder PBXFileReferenceSection, StringBuilder PBXSourcesBuildPhaseSection, string TargetAppGuid, string TargetName)
		{
			foreach (var CurSourceFile in SourceFiles)
			{
				XcodeSourceFile SourceFile = CurSourceFile as XcodeSourceFile;
				string FileName = SourceFile.Reference.GetFileName();
				string FileExtension = Path.GetExtension(FileName);
				string FilePath = SourceFile.Reference.MakeRelativeTo(ProjectFilePath.Directory);
				string FilePathMac = Utils.CleanDirectorySeparators(FilePath, '/');

				if (IsGeneratedProject)
				{
					PBXBuildFileSection.Append(string.Format("\t\t{0} /* {1} in {2} */ = {{isa = PBXBuildFile; fileRef = {3} /* {1} */; }};" + ProjectFileGenerator.NewLine,
						SourceFile.FileGuid,
						FileName,
						GetFileCategory(FileExtension),
						SourceFile.FileRefGuid));
				}

				PBXFileReferenceSection.Append(string.Format("\t\t{0} /* {1} */ = {{isa = PBXFileReference; explicitFileType = {2}; name = \"{1}\"; path = \"{3}\"; sourceTree = SOURCE_ROOT; }};" + ProjectFileGenerator.NewLine,
					SourceFile.FileRefGuid,
					FileName,
					GetFileType(FileExtension),
					FilePathMac));

				if (ShouldIncludeFileInBuildPhaseSection(SourceFile))
				{
					PBXSourcesBuildPhaseSection.Append("\t\t\t\t" + SourceFile.FileGuid + " /* " + FileName + " in Sources */," + ProjectFileGenerator.NewLine);
				}

				var ProjectRelativeSourceFile = CurSourceFile.Reference.MakeRelativeTo(ProjectFilePath.Directory);
				string RelativeSourceDirectory = Path.GetDirectoryName(ProjectRelativeSourceFile);
				// Use the specified relative base folder
				if (CurSourceFile.BaseFolder != null)	// NOTE: We are looking for null strings, not empty strings!
				{
					RelativeSourceDirectory = Path.GetDirectoryName(CurSourceFile.Reference.MakeRelativeTo(CurSourceFile.BaseFolder));
				}
				XcodeFileGroup Group = FindGroupByRelativePath(ref Groups, RelativeSourceDirectory);
				if (Group != null)
				{
					Group.Files.Add(SourceFile);
				}
			}

			PBXFileReferenceSection.Append(string.Format("\t\t{0} /* {1} */ = {{isa = PBXFileReference; explicitFileType = wrapper.application; path = {1}; sourceTree = BUILT_PRODUCTS_DIR; }};" + ProjectFileGenerator.NewLine, TargetAppGuid, TargetName));
		}

		private void AppendGroup(XcodeFileGroup Group, StringBuilder Content, bool bFilesOnly)
		{
			if (!Group.bIsReference)
			{
				if (!bFilesOnly)
				{
					Content.Append(string.Format("\t\t{0} = {{{1}", Group.GroupGuid, ProjectFileGenerator.NewLine));
					Content.Append("\t\t\tisa = PBXGroup;" + ProjectFileGenerator.NewLine);
					Content.Append("\t\t\tchildren = (" + ProjectFileGenerator.NewLine);

					foreach (XcodeFileGroup ChildGroup in Group.Children.Values)
					{
						Content.Append(string.Format("\t\t\t\t{0} /* {1} */,{2}", ChildGroup.GroupGuid, ChildGroup.GroupName, ProjectFileGenerator.NewLine));
					}
				}

				foreach (XcodeSourceFile File in Group.Files)
				{
					Content.Append(string.Format("\t\t\t\t{0} /* {1} */,{2}", File.FileRefGuid, File.Reference.GetFileName(), ProjectFileGenerator.NewLine));
				}

				if (!bFilesOnly)
				{
					Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
					Content.Append("\t\t\tname = \"" + Group.GroupName + "\";" + ProjectFileGenerator.NewLine);
					Content.Append("\t\t\tpath = \"" + Group.GroupPath + "\";" + ProjectFileGenerator.NewLine);
					Content.Append("\t\t\tsourceTree = \"<group>\";" + ProjectFileGenerator.NewLine);
					Content.Append("\t\t};" + ProjectFileGenerator.NewLine);

					foreach (XcodeFileGroup ChildGroup in Group.Children.Values)
					{
						AppendGroup(ChildGroup, Content, bFilesOnly: false);
					}
				}
			}
		}

		private void AppendBuildFileSection(StringBuilder Content, StringBuilder SectionContent)
		{
			Content.Append("/* Begin PBXBuildFile section */" + ProjectFileGenerator.NewLine);
			Content.Append(SectionContent);
			Content.Append("/* End PBXBuildFile section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendFileReferenceSection(StringBuilder Content, StringBuilder SectionContent)
		{
			Content.Append("/* Begin PBXFileReference section */" + ProjectFileGenerator.NewLine);
			Content.Append(SectionContent);
			Content.Append("/* End PBXFileReference section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendSourcesBuildPhaseSection(StringBuilder Content, StringBuilder SectionContent, string SourcesBuildPhaseGuid)
		{
			Content.Append("/* Begin PBXSourcesBuildPhase section */" + ProjectFileGenerator.NewLine);
			Content.Append(string.Format("\t\t{0} = {{{1}", SourcesBuildPhaseGuid, ProjectFileGenerator.NewLine));
			Content.Append("\t\t\tisa = PBXSourcesBuildPhase;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildActionMask = 2147483647;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tfiles = (" + ProjectFileGenerator.NewLine);
			Content.Append(SectionContent);
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\trunOnlyForDeploymentPostprocessing = 0;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);
			Content.Append("/* End PBXSourcesBuildPhase section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendGroupSection(StringBuilder Content, string MainGroupGuid, string ProductRefGroupGuid, string TargetAppGuid, string TargetName)
		{
			Content.Append("/* Begin PBXGroup section */" + ProjectFileGenerator.NewLine);

			// Main group
			Content.Append(string.Format("\t\t{0} = {{{1}", MainGroupGuid, ProjectFileGenerator.NewLine));
			Content.Append("\t\t\tisa = PBXGroup;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tchildren = (" + ProjectFileGenerator.NewLine);

			foreach (XcodeFileGroup Group in Groups.Values)
			{
				if (!string.IsNullOrEmpty(Group.GroupName))
				{
					Content.Append(string.Format("\t\t\t\t{0} /* {1} */,{2}", Group.GroupGuid, Group.GroupName, ProjectFileGenerator.NewLine));
				}
			}

			if (Groups.ContainsKey(""))
			{
				AppendGroup(Groups[""], Content, bFilesOnly: true);
			}

			Content.Append(string.Format("\t\t\t\t{0} /* Products */,{1}", ProductRefGroupGuid, ProjectFileGenerator.NewLine));
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tsourceTree = \"<group>\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);

			// Sources groups
			foreach (XcodeFileGroup Group in Groups.Values)
			{
				if (Group.GroupName != "")
				{
					AppendGroup(Group, Content, bFilesOnly: false);
				}
			}

			// Products group
			Content.Append(string.Format("\t\t{0} /* Products */ = {{{1}", ProductRefGroupGuid, ProjectFileGenerator.NewLine));
			Content.Append("\t\t\tisa = PBXGroup;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tchildren = (" + ProjectFileGenerator.NewLine);
			Content.Append(string.Format("\t\t\t\t{0} /* {1} */,{2}", TargetAppGuid, TargetName, ProjectFileGenerator.NewLine));
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tname = Products;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tsourceTree = \"<group>\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);

			Content.Append("/* End PBXGroup section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendLegacyTargetSection(StringBuilder Content, string TargetName, string TargetGuid, string TargetBuildConfigGuid, FileReference UProjectPath)
		{
			string UE4Dir = ConvertPath(Path.GetFullPath(Directory.GetCurrentDirectory() + "../../.."));
			string BuildToolPath = UE4Dir + "/Engine/Build/BatchFiles/Mac/Build.sh";

			Content.Append("/* Begin PBXLegacyTarget section */" + ProjectFileGenerator.NewLine);

			Content.Append("\t\t" + TargetGuid + " /* " + TargetName + " */ = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tisa = PBXLegacyTarget;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildArgumentsString = \"$(ACTION) $(UE_BUILD_TARGET_NAME) $(PLATFORM_NAME) $(UE_BUILD_TARGET_CONFIG)" + (UProjectPath == null ? "" : " \\\"" + UProjectPath.FullName + "\\\"") + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildConfigurationList = "  + TargetBuildConfigGuid + " /* Build configuration list for PBXLegacyTarget \"" + TargetName + "\" */;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildPhases = (" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildToolPath = \"" + BuildToolPath + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildWorkingDirectory = \"" + UE4Dir + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tdependencies = (" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tname = \"" + TargetName + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tpassBuildSettingsInEnvironment = 1;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tproductName = \"" + TargetName + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);

			Content.Append("/* End PBXLegacyTarget section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendRunTargetSection(StringBuilder Content, string TargetName, string TargetGuid, string TargetBuildConfigGuid, string TargetDependencyGuid, string TargetAppGuid)
		{
			Content.Append("/* Begin PBXNativeTarget section */" + ProjectFileGenerator.NewLine);

			Content.Append("\t\t" + TargetGuid + " /* " + TargetName + " */ = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tisa = PBXNativeTarget;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildConfigurationList = "  + TargetBuildConfigGuid + " /* Build configuration list for PBXNativeTarget \"" + TargetName + "\" */;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildPhases = (" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tdependencies = (" + ProjectFileGenerator.NewLine);
			if (!XcodeProjectFileGenerator.bGeneratingRunIOSProject && !XcodeProjectFileGenerator.bGeneratingRunTVOSProject)
			{
				Content.Append("\t\t\t\t" + TargetDependencyGuid + " /* PBXTargetDependency */," + ProjectFileGenerator.NewLine);
			}
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tname = \"" + TargetName + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tpassBuildSettingsInEnvironment = 1;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tproductName = \"" + TargetName + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tproductReference = \"" + TargetAppGuid + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tproductType = \"com.apple.product-type.application\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);

			Content.Append("/* End PBXNativeTarget section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendIndexTargetSection(StringBuilder Content, string TargetName, string TargetGuid, string TargetBuildConfigGuid, string SourcesBuildPhaseGuid)
		{
			Content.Append("/* Begin PBXNativeTarget section */" + ProjectFileGenerator.NewLine);

			Content.Append("\t\t" + TargetGuid + " /* " + TargetName + " */ = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tisa = PBXNativeTarget;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildConfigurationList = "  + TargetBuildConfigGuid + " /* Build configuration list for PBXNativeTarget \"" + TargetName + "\" */;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildPhases = (" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\t" + SourcesBuildPhaseGuid + " /* Sources */," + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tdependencies = (" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tname = \"" + TargetName + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tpassBuildSettingsInEnvironment = 1;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tproductName = \"" + TargetName + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tproductType = \"com.apple.product-type.library.static\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);

			Content.Append("/* End PBXNativeTarget section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendProjectSection(StringBuilder Content, string TargetName, string TargetGuid, string BuildTargetName, string BuildTargetGuid, string IndexTargetName, string IndexTargetGuid, string MainGroupGuid, string ProductRefGroupGuid, string ProjectGuid, string ProjectBuildConfigGuid, FileReference ProjectFile)
		{
			Content.Append("/* Begin PBXProject section */" + ProjectFileGenerator.NewLine);

			Content.Append("\t\t" + ProjectGuid + " /* Project object */ = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tisa = PBXProject;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tattributes = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\tLastUpgradeCheck = 0900;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\tORGANIZATIONNAME = \"Epic Games, Inc.\";" + ProjectFileGenerator.NewLine);
            Content.Append("\t\t\t\tTargetAttributes = {" + ProjectFileGenerator.NewLine);
            Content.Append("\t\t\t\t\t" + TargetGuid + " = {" + ProjectFileGenerator.NewLine);

			bool bAutomaticSigning = false;
			if (UnrealBuildTool.IsValidPlatform(UnrealTargetPlatform.IOS))
			{
				IOSPlatform IOSPlatform = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS));
				IOSProjectSettings ProjectSettings = IOSPlatform.ReadProjectSettings(ProjectFile);
				bAutomaticSigning = ProjectSettings.bAutomaticSigning;
			}

			if (UnrealBuildTool.IsValidPlatform(UnrealTargetPlatform.TVOS))
			{
				TVOSPlatform TVOSPlatform = ((TVOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.TVOS));
				TVOSProjectSettings ProjectSettings = TVOSPlatform.ReadProjectSettings(ProjectFile);
				TVOSProvisioningData ProvisioningData = TVOSPlatform.ReadProvisioningData(ProjectSettings);
				bAutomaticSigning = ProjectSettings.bAutomaticSigning;
			}

			if (bAutomaticSigning)
			{
				Content.Append("\t\t\t\t\t\tProvisioningStyle = Automatic;" + ProjectFileGenerator.NewLine);
			}
			else
			{
				Content.Append("\t\t\t\t\t\tProvisioningStyle = Manual;" + ProjectFileGenerator.NewLine);
			}
            Content.Append("\t\t\t\t\t};" + ProjectFileGenerator.NewLine);
            Content.Append("\t\t\t\t};" + ProjectFileGenerator.NewLine);
            Content.Append("\t\t\t};" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildConfigurationList = " + ProjectBuildConfigGuid + " /* Build configuration list for PBXProject \"" + TargetName + "\" */;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tcompatibilityVersion = \"Xcode 8.0\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tdevelopmentRegion = English;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\thasScannedForEncodings = 0;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tknownRegions = (" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\ten" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tmainGroup = " + MainGroupGuid + ";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tproductRefGroup = " + ProductRefGroupGuid + ";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tprojectDirPath = \"\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tprojectRoot = \"\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\ttargets = (" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t" + TargetGuid + " /* " + TargetName + " */," + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t" + BuildTargetGuid + " /* " + BuildTargetName + " */," + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t" + IndexTargetGuid + " /* " + IndexTargetName + " */," + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);

			Content.Append("/* End PBXProject section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendContainerItemProxySection(StringBuilder Content, string TargetName, string TargetGuid, string TargetProxyGuid, string ProjectGuid)
		{
			Content.Append("/* Begin PBXContainerItemProxy section */" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t" + TargetProxyGuid + " /* PBXContainerItemProxy */ = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tisa = PBXContainerItemProxy;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tcontainerPortal = " + ProjectGuid + " /* Project object */;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tproxyType = 1;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tremoteGlobalIDString = " + TargetGuid + ";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tremoteInfo = \"" + TargetName + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);
			Content.Append("/* End PBXContainerItemProxy section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendTargetDependencySection(StringBuilder Content, string TargetName, string TargetGuid, string TargetDependencyGuid, string TargetProxyGuid)
		{
			Content.Append("/* Begin PBXTargetDependency section */" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t" + TargetDependencyGuid + " /* PBXTargetDependency */ = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tisa = PBXTargetDependency;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\ttarget = " + TargetGuid + " /* " + TargetName + " */;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\ttargetProxy = " + TargetProxyGuid + " /* PBXContainerItemProxy */;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);
			Content.Append("/* End PBXTargetDependency section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendProjectBuildConfiguration(StringBuilder Content, string ConfigName, string ConfigGuid)
		{
			Content.Append("\t\t" + ConfigGuid + " /* \"" + ConfigName + "\" */ = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tisa = XCBuildConfiguration;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildSettings = {" + ProjectFileGenerator.NewLine);

			Content.Append("\t\t\t\tGCC_PREPROCESSOR_DEFINITIONS = (" + ProjectFileGenerator.NewLine);
			foreach (var Definition in IntelliSensePreprocessorDefinitions)
			{
				Content.Append("\t\t\t\t\t\"" + Definition.Replace("\"", "") + "\"," + ProjectFileGenerator.NewLine);
			}
			Content.Append("\t\t\t\t\t\"MONOLITHIC_BUILD=1\"," + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\t);" + ProjectFileGenerator.NewLine);

			Content.Append("\t\t\t\tHEADER_SEARCH_PATHS = (" + ProjectFileGenerator.NewLine);
			foreach (var SearchPath in IntelliSenseSystemIncludeSearchPaths)
			{
				string Path = SearchPath.Contains(" ") ? "\\\"" + SearchPath + "\\\"" : SearchPath;
				Content.Append("\t\t\t\t\t\"" + Path + "\"," + ProjectFileGenerator.NewLine);
			}
			Content.Append("\t\t\t\t);" + ProjectFileGenerator.NewLine);

			Content.Append("\t\t\t\tUSER_HEADER_SEARCH_PATHS = (" + ProjectFileGenerator.NewLine);
			foreach (var SearchPath in IntelliSenseIncludeSearchPaths)
			{
				string Path = SearchPath.Contains(" ") ? "\\\"" + SearchPath + "\\\"" : SearchPath;
				Content.Append("\t\t\t\t\t\"" + Path + "\"," + ProjectFileGenerator.NewLine);
			}
			Content.Append("\t\t\t\t);" + ProjectFileGenerator.NewLine);

			if (ConfigName == "Debug")
			{
				Content.Append("\t\t\t\tONLY_ACTIVE_ARCH = YES;" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\tENABLE_TESTABILITY = YES;" + ProjectFileGenerator.NewLine);
			}
			Content.Append("\t\t\t\tALWAYS_SEARCH_USER_PATHS = NO;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\tCLANG_CXX_LANGUAGE_STANDARD = \"c++0x\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\tGCC_ENABLE_CPP_RTTI = NO;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\tGCC_WARN_CHECK_SWITCH_STATEMENTS = NO;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\tUSE_HEADERMAP = NO;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t};" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tname = \"" + ConfigName + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);
		}

		private void AppendNativeTargetBuildConfiguration(StringBuilder Content, XcodeBuildConfig Config, string ConfigGuid, bool bIsAGame, FileReference ProjectFile)
		{
			bool bMacOnly = true;
			if (Config.ProjectTarget.TargetRules != null && XcodeProjectFileGenerator.ProjectFilePlatform.HasFlag(XcodeProjectFileGenerator.XcodeProjectFilePlatform.iOS))
			{
				if (Config.ProjectTarget.SupportedPlatforms.Contains(UnrealTargetPlatform.IOS))
				{
					bMacOnly = false;
				}
			}

			Content.Append("\t\t" + ConfigGuid + " /* \"" + Config.DisplayName + "\" */ = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tisa = XCBuildConfiguration;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildSettings = {" + ProjectFileGenerator.NewLine);

            string UE4Dir = ConvertPath(Path.GetFullPath(Directory.GetCurrentDirectory() + "../../.."));
			string MacExecutableDir = ConvertPath(Config.MacExecutablePath.Directory.FullName);
			string MacExecutableFileName = Config.MacExecutablePath.GetFileName();

			if (bMacOnly)
			{
				Content.Append("\t\t\t\tVALID_ARCHS = \"x86_64\";" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\tSUPPORTED_PLATFORMS = \"macosx\";" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\tPRODUCT_NAME = \"" + MacExecutableFileName + "\";" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\tCONFIGURATION_BUILD_DIR = \"" + MacExecutableDir + "\";" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\tCOMBINE_HIDPI_IMAGES = YES;" + ProjectFileGenerator.NewLine);
			}
			else
			{
				string IOSRunTimeVersion = null;
				string IOSRunTimeDevices = null;
				string TVOSRunTimeVersion = null;
				string TVOSRunTimeDevices = null;
				string ValidArchs = "x86_64";
				string SupportedPlatforms = "macosx";

				bool bAutomaticSigning = false;
                string UUID_IOS = "";
                string UUID_TVOS = "";
                string TEAM_IOS = "";
                string TEAM_TVOS = "";
                string IOS_CERT = "iPhone Developer";
                string TVOS_CERT = "iPhone Developer";
                if (UnrealBuildTool.IsValidPlatform(UnrealTargetPlatform.IOS))
                {
					IOSPlatform IOSPlatform = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS));
					IOSProjectSettings ProjectSettings = IOSPlatform.ReadProjectSettings(ProjectFile);
					IOSProvisioningData ProvisioningData = IOSPlatform.ReadProvisioningData(ProjectSettings);
					IOSRunTimeVersion = ProjectSettings.RuntimeVersion;
					IOSRunTimeDevices = ProjectSettings.RuntimeDevices;
					ValidArchs += " arm64 armv7 armv7s";
					SupportedPlatforms += " iphoneos";
					bAutomaticSigning = ProjectSettings.bAutomaticSigning;
					if (!bAutomaticSigning)
					{
						UUID_IOS = ProvisioningData.MobileProvisionUUID;
						IOS_CERT = ProvisioningData.SigningCertificate;
					}
                    TEAM_IOS = ProvisioningData.TeamUUID;
                }

                if (UnrealBuildTool.IsValidPlatform(UnrealTargetPlatform.TVOS))
				{
					TVOSPlatform TVOSPlatform = ((TVOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.TVOS));
					TVOSProjectSettings ProjectSettings = TVOSPlatform.ReadProjectSettings(ProjectFile);
					TVOSProvisioningData ProvisioningData = TVOSPlatform.ReadProvisioningData(ProjectSettings);
					TVOSRunTimeVersion = ProjectSettings.RuntimeVersion;
					TVOSRunTimeDevices = ProjectSettings.RuntimeDevices;
					if (ValidArchs == "x86_64")
					{
						ValidArchs += " arm64 armv7 armv7s";
					}
					SupportedPlatforms += " appletvos";
					if (!bAutomaticSigning)
					{
						UUID_TVOS = ProvisioningData.MobileProvisionUUID;
						TVOS_CERT = ProvisioningData.SigningCertificate;
					}
                    TEAM_TVOS = ProvisioningData.TeamUUID;
                }

                Content.Append("\t\t\t\tVALID_ARCHS = \"" + ValidArchs + "\";" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\tSUPPORTED_PLATFORMS = \"" + SupportedPlatforms + "\";" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\t\"PRODUCT_NAME[sdk=macosx*]\" = \"" + MacExecutableFileName + "\";" + ProjectFileGenerator.NewLine);
				if (IOSRunTimeVersion != null)
				{
					Content.Append("\t\t\t\tIPHONEOS_DEPLOYMENT_TARGET = " + IOSRunTimeVersion + ";" + ProjectFileGenerator.NewLine);
					Content.Append("\t\t\t\t\"PRODUCT_NAME[sdk=iphoneos*]\" = \"" + Config.BuildTarget + "\";" + ProjectFileGenerator.NewLine); // @todo: change to Path.GetFileName(Config.IOSExecutablePath) when we stop using payload
					Content.Append("\t\t\t\t\"TARGETED_DEVICE_FAMILY[sdk=iphoneos*]\" = \"" + IOSRunTimeDevices + "\";" + ProjectFileGenerator.NewLine);
                    Content.Append("\t\t\t\t\"SDKROOT[sdk=iphoneos]\" = iphoneos;" + ProjectFileGenerator.NewLine);
					if (!string.IsNullOrEmpty(TEAM_IOS))
					{
						Content.Append("\t\t\t\t\"DEVELOPMENT_TEAM[sdk=iphoneos*]\" = " + TEAM_IOS + ";" + ProjectFileGenerator.NewLine);
					}
					Content.Append("\t\t\t\t\"CODE_SIGN_IDENTITY[sdk=iphoneos*]\" = \"" + IOS_CERT + "\";" + ProjectFileGenerator.NewLine);
					if (!bAutomaticSigning && !string.IsNullOrEmpty(UUID_IOS))
					{
						Content.Append("\t\t\t\t\"PROVISIONING_PROFILE_SPECIFIER[sdk=iphoneos*]\" = \"" + UUID_IOS + "\";" + ProjectFileGenerator.NewLine);
					}
				}
                if (TVOSRunTimeVersion != null)
				{
					Content.Append("\t\t\t\tTVOS_DEPLOYMENT_TARGET = " + TVOSRunTimeVersion + ";" + ProjectFileGenerator.NewLine);
					Content.Append("\t\t\t\t\"PRODUCT_NAME[sdk=appletvos*]\" = \"" + Config.BuildTarget + "\";" + ProjectFileGenerator.NewLine); // @todo: change to Path.GetFileName(Config.TVOSExecutablePath) when we stop using payload
					Content.Append("\t\t\t\t\"TARGETED_DEVICE_FAMILY[sdk=appletvos*]\" = \"" + TVOSRunTimeDevices + "\";" + ProjectFileGenerator.NewLine);
                    Content.Append("\t\t\t\t\"SDKROOT[sdk=appletvos]\" = appletvos;" + ProjectFileGenerator.NewLine);
					if (!string.IsNullOrEmpty(TEAM_TVOS))
					{
						Content.Append("\t\t\t\t\"DEVELOPMENT_TEAM[sdk=appletvos*]\" = " + TEAM_TVOS + ";" + ProjectFileGenerator.NewLine);
					}
					Content.Append("\t\t\t\t\"CODE_SIGN_IDENTITY[sdk=appletvos*]\" = \"" + TVOS_CERT + "\";" + ProjectFileGenerator.NewLine);
					if (!bAutomaticSigning && !string.IsNullOrEmpty(UUID_TVOS))
					{
						Content.Append("\t\t\t\t\"PROVISIONING_PROFILE_SPECIFIER[sdk=appletvos*]\" = \"" + UUID_TVOS + "\";" + ProjectFileGenerator.NewLine);
					}
				}
                Content.Append("\t\t\t\t\"CONFIGURATION_BUILD_DIR[sdk=macosx*]\" = \"" + MacExecutableDir + "\";" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\t\"SDKROOT[sdk=macosx]\" = macosx;" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\tCOMBINE_HIDPI_IMAGES = YES;" + ProjectFileGenerator.NewLine);

				bool bIsUE4Game = Config.BuildTarget.Equals("UE4Game", StringComparison.InvariantCultureIgnoreCase);
				bool bIsUE4Client = Config.BuildTarget.Equals("UE4Client", StringComparison.InvariantCultureIgnoreCase);

				DirectoryReference GameDir = ProjectFile != null ? ProjectFile.Directory : null;
				string GamePath = GameDir != null ? ConvertPath(GameDir.FullName) : null;

				string IOSInfoPlistPath = null;
				string TVOSInfoPlistPath = null;
				string MacInfoPlistPath = null;
				string IOSEntitlementPath = null;
				if (bIsUE4Game)
				{
					IOSInfoPlistPath = UE4Dir + "/Engine/Intermediate/IOS/" + Config.BuildTarget + "-Info.plist";
					TVOSInfoPlistPath = UE4Dir + "/Engine/Intermediate/TVOS/" + Config.BuildTarget + "-Info.plist";
					MacInfoPlistPath = UE4Dir + "/Engine/Intermediate/Mac/" + MacExecutableFileName + "-Info.plist";
					IOSEntitlementPath = "";
					if (IOSRunTimeVersion != null)
					{
						Content.Append("\t\t\t\t\"CONFIGURATION_BUILD_DIR[sdk=iphoneos*]\" = \"" + UE4Dir + "/Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine);
					}
					if (TVOSRunTimeVersion != null)
					{
						Content.Append("\t\t\t\t\"CONFIGURATION_BUILD_DIR[sdk=appletvos*]\" = \"" + UE4Dir + "/Engine/Binaries/TVOS/Payload\";" + ProjectFileGenerator.NewLine);
					}
				}
				else if (bIsUE4Client)
				{
					IOSInfoPlistPath = UE4Dir + "/Engine/Intermediate/IOS/UE4Game-Info.plist";
					TVOSInfoPlistPath = UE4Dir + "/Engine/Intermediate/TVOS/UE4Game-Info.plist";
					MacInfoPlistPath = UE4Dir + "/Engine/Intermediate/Mac/" + MacExecutableFileName + "-Info.plist";
					IOSEntitlementPath = "";
					if (IOSRunTimeVersion != null)
					{
						Content.Append("\t\t\t\t\"CONFIGURATION_BUILD_DIR[sdk=iphoneos*]\" = \"" + UE4Dir + "/Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine);
					}
					if (TVOSRunTimeVersion != null)
					{
						Content.Append("\t\t\t\t\"CONFIGURATION_BUILD_DIR[sdk=appletvos*]\" = \"" + UE4Dir + "/Engine/Binaries/TVOS/Payload\";" + ProjectFileGenerator.NewLine);
					}
				}
				else if (bIsAGame)
				{
					IOSInfoPlistPath = GamePath + "/Intermediate/IOS/" + Config.BuildTarget + "-Info.plist";
					TVOSInfoPlistPath = GamePath + "/Intermediate/TVOS/" + Config.BuildTarget + "-Info.plist";
					MacInfoPlistPath = GamePath + "/Intermediate/Mac/" + MacExecutableFileName + "-Info.plist";
					IOSEntitlementPath = GamePath + "/Intermediate/IOS/" + Config.BuildTarget + ".entitlements";
					if (IOSRunTimeVersion != null)
					{
						Content.Append("\t\t\t\t\"CONFIGURATION_BUILD_DIR[sdk=iphoneos*]\" = \"" + GamePath + "/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine);
					}
					if (TVOSRunTimeVersion != null)
					{
						Content.Append("\t\t\t\t\"CONFIGURATION_BUILD_DIR[sdk=appletvos*]\" = \"" + GamePath + "/Binaries/TVOS/Payload\";" + ProjectFileGenerator.NewLine);
					}
				}
				else
				{
					if (GamePath == null)
					{
						IOSInfoPlistPath = UE4Dir + "/Engine/Intermediate/IOS/" + Config.BuildTarget + "-Info.plist";
						TVOSInfoPlistPath = UE4Dir + "/Engine/Intermediate/TVOS/" + Config.BuildTarget + "-Info.plist";
						MacInfoPlistPath = UE4Dir + "/Engine/Intermediate/Mac/" + MacExecutableFileName + "-Info.plist";
					}
					else
					{
						IOSInfoPlistPath = GamePath + "/Intermediate/IOS/" + Config.BuildTarget + "-Info.plist";
						TVOSInfoPlistPath = GamePath + "/Intermediate/TVOS/" + Config.BuildTarget + "-Info.plist";
						MacInfoPlistPath = GamePath + "/Intermediate/Mac/" + MacExecutableFileName + "-Info.plist";
					}
					if (IOSRunTimeVersion != null)
					{
						Content.Append("\t\t\t\t\"CONFIGURATION_BUILD_DIR[sdk=iphoneos*]\" = \"" + UE4Dir + "/Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine);
					}
					if (TVOSRunTimeVersion != null)
					{
						Content.Append("\t\t\t\t\"CONFIGURATION_BUILD_DIR[sdk=appletvos*]\" = \"" + UE4Dir + "/Engine/Binaries/TVOS/Payload\";" + ProjectFileGenerator.NewLine);
					}
				}

				if (XcodeProjectFileGenerator.bGeneratingRunIOSProject)
				{
					Content.Append("\t\t\t\tINFOPLIST_FILE = \"" + IOSInfoPlistPath + "\";" + ProjectFileGenerator.NewLine);
					Content.Append("\t\t\t\tCODE_SIGN_ENTITLEMENTS = \"" + IOSEntitlementPath + "\";" + ProjectFileGenerator.NewLine);
				}
				else if (XcodeProjectFileGenerator.bGeneratingRunTVOSProject)
				{
					Content.Append("\t\t\t\tINFOPLIST_FILE = \"" + TVOSInfoPlistPath + "\";" + ProjectFileGenerator.NewLine);
				}
				else
				{
					Content.Append("\t\t\t\t\"INFOPLIST_FILE[sdk=macosx*]\" = \"" + MacInfoPlistPath + "\";" + ProjectFileGenerator.NewLine);
					if (IOSRunTimeVersion != null)
					{
						Content.Append("\t\t\t\t\"INFOPLIST_FILE[sdk=iphoneos*]\" = \"" + IOSInfoPlistPath + "\";" + ProjectFileGenerator.NewLine);
						Content.Append("\t\t\t\t\"CODE_SIGN_ENTITLEMENTS[sdk=iphoneos*]\" = \"" + IOSEntitlementPath + "\";" + ProjectFileGenerator.NewLine);
					}
					if (TVOSRunTimeVersion != null)
					{
						Content.Append("\t\t\t\t\"INFOPLIST_FILE[sdk=appletvos*]\" = \"" + TVOSInfoPlistPath + "\";" + ProjectFileGenerator.NewLine);
					}
				}

				// Prepare a temp Info.plist file so Xcode has some basic info about the target immediately after opening the project.
				// This is needed for the target to pass the settings validation before code signing. UBT will overwrite this plist file later, with proper contents.
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					bool bCreateMacInfoPlist = !File.Exists(MacInfoPlistPath);
					bool bCreateIOSInfoPlist = !File.Exists(IOSInfoPlistPath) && IOSRunTimeVersion != null;
					bool bCreateTVOSInfoPlist = !File.Exists(TVOSInfoPlistPath) && TVOSRunTimeVersion != null;
					if (bCreateMacInfoPlist || bCreateIOSInfoPlist || bCreateTVOSInfoPlist)
					{
						DirectoryReference ProjectPath = GameDir;
						DirectoryReference EngineDir = DirectoryReference.Combine(new DirectoryReference(UE4Dir), "Engine");
						string GameName = Config.BuildTarget;
						if (ProjectPath == null)
						{
							ProjectPath = EngineDir;
						}
						if (bIsUE4Game)
						{
							ProjectPath = EngineDir;
							GameName = "UE4Game";
						}

						if (bCreateMacInfoPlist)
						{
							Directory.CreateDirectory(Path.GetDirectoryName(MacInfoPlistPath));
							UEDeployMac.GeneratePList(ProjectPath.FullName, bIsUE4Game, GameName, Config.BuildTarget, EngineDir.FullName, MacExecutableFileName);
						}
						if (bCreateIOSInfoPlist)
						{
							Directory.CreateDirectory(Path.GetDirectoryName(IOSInfoPlistPath));
                            bool bSupportPortrait, bSupportLandscape, bSkipIcons;
							UEDeployIOS.GenerateIOSPList(ProjectFile, Config.BuildConfig, ProjectPath.FullName, bIsUE4Game, GameName, Config.BuildTarget, EngineDir.FullName, ProjectPath + "/Binaries/IOS/Payload", out bSupportPortrait, out bSupportLandscape, out bSkipIcons);
						}
						if (bCreateTVOSInfoPlist)
						{
							Directory.CreateDirectory(Path.GetDirectoryName(TVOSInfoPlistPath));
							UEDeployTVOS.GenerateTVOSPList(ProjectPath.FullName, bIsUE4Game, GameName, Config.BuildTarget, EngineDir.FullName, ProjectPath + "/Binaries/TVOS/Payload");
						}
					}
				}
			}
			Content.Append("\t\t\t\tMACOSX_DEPLOYMENT_TARGET = " + MacToolChain.Settings.MacOSVersion + ";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\tSDKROOT = macosx;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\tGCC_PREFIX_HEADER = \"" + UE4Dir + "/Engine/Source/Editor/UnrealEd/Public/UnrealEd.h\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t};" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tname = \"" + Config.DisplayName + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);
		}

		private void AppendLegacyTargetBuildConfiguration(StringBuilder Content, XcodeBuildConfig Config, string ConfigGuid)
		{
			bool bMacOnly = true;
			if (Config.ProjectTarget.TargetRules != null && XcodeProjectFileGenerator.ProjectFilePlatform.HasFlag(XcodeProjectFileGenerator.XcodeProjectFilePlatform.iOS))
			{
				if (Config.ProjectTarget.SupportedPlatforms.Contains(UnrealTargetPlatform.IOS))
				{
					bMacOnly = false;
				}
			}

			Content.Append("\t\t" + ConfigGuid + " /* \"" + Config.DisplayName + "\" */ = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tisa = XCBuildConfiguration;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildSettings = {" + ProjectFileGenerator.NewLine);
			if (bMacOnly)
			{
				Content.Append("\t\t\t\tVALID_ARCHS = \"x86_64\";" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\tSUPPORTED_PLATFORMS = \"macosx\";" + ProjectFileGenerator.NewLine);
			}
			else
			{
				string ValidArchs = "x86_64";
				string SupportedPlatforms = "macosx";
				if (UnrealBuildTool.IsValidPlatform(UnrealTargetPlatform.IOS))
				{
					ValidArchs += " arm64 armv7 armv7s";
					SupportedPlatforms += " iphoneos";
				}
				if (UnrealBuildTool.IsValidPlatform(UnrealTargetPlatform.TVOS))
				{
					if (ValidArchs == "x86_64")
					{
						ValidArchs += " arm64 armv7 armv7s";
					}
					SupportedPlatforms += " appletvos";
				}
				Content.Append("\t\t\t\tVALID_ARCHS = \"" + ValidArchs + "\";" + ProjectFileGenerator.NewLine);
				Content.Append("\t\t\t\tSUPPORTED_PLATFORMS = \"" + SupportedPlatforms + "\";" + ProjectFileGenerator.NewLine);
			}
			Content.Append("\t\t\t\tUE_BUILD_TARGET_NAME = \"" + Config.BuildTarget + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t\tUE_BUILD_TARGET_CONFIG = \"" + Config.BuildConfig + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t};" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tname = \"" + Config.DisplayName + "\";" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);
		}

		private void AppendXCBuildConfigurationSection(StringBuilder Content, Dictionary<string, XcodeBuildConfig> ProjectBuildConfigs, Dictionary<string, XcodeBuildConfig> TargetBuildConfigs,
			Dictionary<string, XcodeBuildConfig> BuildTargetBuildConfigs, Dictionary<string, XcodeBuildConfig> IndexTargetBuildConfigs, bool bIsAGame, FileReference GameProjectPath)
		{
			Content.Append("/* Begin XCBuildConfiguration section */" + ProjectFileGenerator.NewLine);

			foreach (var Config in ProjectBuildConfigs)
			{
				AppendProjectBuildConfiguration(Content, Config.Value.DisplayName, Config.Key);
			}

			foreach (var Config in TargetBuildConfigs)
			{
				AppendNativeTargetBuildConfiguration(Content, Config.Value, Config.Key, bIsAGame, GameProjectPath);
			}

			foreach (var Config in BuildTargetBuildConfigs)
			{
				AppendLegacyTargetBuildConfiguration(Content, Config.Value, Config.Key);
			}

			foreach (var Config in IndexTargetBuildConfigs)
			{
				AppendNativeTargetBuildConfiguration(Content, Config.Value, Config.Key, bIsAGame, GameProjectPath);
			}

			Content.Append("/* End XCBuildConfiguration section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private void AppendXCConfigurationList(StringBuilder Content, string TypeName, string TargetName, string ConfigListGuid, Dictionary<string, XcodeBuildConfig> BuildConfigs)
		{
			Content.Append("\t\t" + ConfigListGuid + " /* Build configuration list for " + TypeName + " \"" + TargetName + "\" */ = {" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tisa = XCConfigurationList;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tbuildConfigurations = (" + ProjectFileGenerator.NewLine);
			foreach (var Config in BuildConfigs)
			{
				Content.Append("\t\t\t\t" + Config.Key + " /* \"" + Config.Value.DisplayName + "\" */," + ProjectFileGenerator.NewLine);
			}
			Content.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tdefaultConfigurationIsVisible = 0;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\tdefaultConfigurationName = Development;" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t};" + ProjectFileGenerator.NewLine);
		}

		private void AppendXCConfigurationListSection(StringBuilder Content, string TargetName, string BuildTargetName, string IndexTargetName, string ProjectConfigListGuid,
			Dictionary<string, XcodeBuildConfig> ProjectBuildConfigs, string TargetConfigListGuid, Dictionary<string, XcodeBuildConfig> TargetBuildConfigs,
			string BuildTargetConfigListGuid, Dictionary<string, XcodeBuildConfig> BuildTargetBuildConfigs,
			string IndexTargetConfigListGuid, Dictionary<string, XcodeBuildConfig> IndexTargetBuildConfigs)
		{
			Content.Append("/* Begin XCConfigurationList section */" + ProjectFileGenerator.NewLine);

			AppendXCConfigurationList(Content, "PBXProject", TargetName, ProjectConfigListGuid, ProjectBuildConfigs);
			AppendXCConfigurationList(Content, "PBXLegacyTarget", BuildTargetName, BuildTargetConfigListGuid, BuildTargetBuildConfigs);
			AppendXCConfigurationList(Content, "PBXNativeTarget", TargetName, TargetConfigListGuid, TargetBuildConfigs);
			AppendXCConfigurationList(Content, "PBXNativeTarget", IndexTargetName, IndexTargetConfigListGuid, IndexTargetBuildConfigs);

			Content.Append("/* End XCConfigurationList section */" + ProjectFileGenerator.NewLine);
		}

		public struct XcodeBuildConfig
		{
			public XcodeBuildConfig(string InDisplayName, string InBuildTarget, FileReference InMacExecutablePath, FileReference InIOSExecutablePath, FileReference InTVOSExecutablePath,
				ProjectTarget InProjectTarget, UnrealTargetConfiguration InBuildConfig)
			{
				DisplayName = InDisplayName;
				MacExecutablePath = InMacExecutablePath;
				IOSExecutablePath = InIOSExecutablePath;
				TVOSExecutablePath = InTVOSExecutablePath;
				BuildTarget = InBuildTarget;
				ProjectTarget = InProjectTarget;
				BuildConfig = InBuildConfig;
			}

			public string DisplayName;
			public FileReference MacExecutablePath;
			public FileReference IOSExecutablePath;
			public FileReference TVOSExecutablePath;
			public string BuildTarget;
			public ProjectTarget ProjectTarget;
			public UnrealTargetConfiguration BuildConfig;
		};

		private List<XcodeBuildConfig> GetSupportedBuildConfigs(List<UnrealTargetPlatform> Platforms, List<UnrealTargetConfiguration> Configurations)
		{
			var BuildConfigs = new List<XcodeBuildConfig>();

			string ProjectName = ProjectFilePath.GetFileNameWithoutExtension();

			foreach (var Configuration in Configurations)
			{
				if (UnrealBuildTool.IsValidConfiguration(Configuration))
				{
					foreach (var Platform in Platforms)
					{
						if (UnrealBuildTool.IsValidPlatform(Platform) && (Platform == UnrealTargetPlatform.Mac || Platform == UnrealTargetPlatform.IOS)) // @todo support other platforms
						{
							var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, true);
							if ((BuildPlatform != null) && (BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid))
							{
								// Now go through all of the target types for this project
								if (ProjectTargets.Count == 0)
								{
									throw new BuildException("Expecting at least one ProjectTarget to be associated with project '{0}' in the TargetProjects list ", ProjectFilePath);
								}

								foreach (var ProjectTarget in ProjectTargets)
								{
									if (MSBuildProjectFile.IsValidProjectPlatformAndConfiguration(ProjectTarget, Platform, Configuration))
									{
										// Figure out if this is a monolithic build
										bool bShouldCompileMonolithic = BuildPlatform.ShouldCompileMonolithicBinary(Platform);
										bShouldCompileMonolithic |= (ProjectTarget.CreateRulesDelegate(Platform, Configuration).GetLegacyLinkType(Platform, Configuration) == TargetLinkType.Monolithic);

										var ConfigName = Configuration.ToString();
										if (ProjectTarget.TargetRules.Type != TargetType.Game && ProjectTarget.TargetRules.Type != TargetType.Program)
										{
											ConfigName += " " + ProjectTarget.TargetRules.Type.ToString();
										}

										if (BuildConfigs.Where(Config => Config.DisplayName == ConfigName).ToList().Count == 0)
										{
											string TargetName = ProjectTarget.TargetFilePath.GetFileNameWithoutAnyExtensions();

											// Get the output directory
											DirectoryReference RootDirectory = UnrealBuildTool.EngineDirectory;
											if ((ProjectTarget.TargetRules.Type == TargetType.Game || ProjectTarget.TargetRules.Type == TargetType.Client || ProjectTarget.TargetRules.Type == TargetType.Server) && bShouldCompileMonolithic && !ProjectTarget.TargetRules.bOutputToEngineBinaries)
											{
												if(ProjectTarget.UnrealProjectFilePath != null)
												{
													RootDirectory = ProjectTarget.UnrealProjectFilePath.Directory;
												}
											}

											if(ProjectTarget.TargetRules.Type == TargetType.Program && !ProjectTarget.TargetRules.bOutputToEngineBinaries && ProjectTarget.UnrealProjectFilePath != null)
											{
												RootDirectory = ProjectTarget.UnrealProjectFilePath.Directory;
											}

											// Get the output directory
											DirectoryReference OutputDirectory = DirectoryReference.Combine(RootDirectory, "Binaries");

											string ExeName = TargetName;
											if (!bShouldCompileMonolithic && ProjectTarget.TargetRules.Type != TargetType.Program)
											{
												// Figure out what the compiled binary will be called so that we can point the IDE to the correct file
												if (ProjectTarget.TargetRules.Type != TargetType.Game)
												{
													ExeName = "UE4" + ProjectTarget.TargetRules.Type.ToString();
												}
											}

                                            if (BuildPlatform.Platform == UnrealTargetPlatform.Mac)
                                            {
                                                string MacExecutableName = UEBuildTarget.MakeBinaryFileName(ExeName, UnrealTargetPlatform.Mac, (ExeName == "UE4Editor" && Configuration == UnrealTargetConfiguration.DebugGame) ? UnrealTargetConfiguration.Development : Configuration, ProjectTarget.TargetRules.Architecture, ProjectTarget.TargetRules.UndecoratedConfiguration, UEBuildBinaryType.Executable, null);
                                                string IOSExecutableName = MacExecutableName.Replace("-Mac-", "-IOS-");
                                                string TVOSExecutableName = MacExecutableName.Replace("-Mac-", "-TVOS-");
                                                BuildConfigs.Add(new XcodeBuildConfig(ConfigName, TargetName, FileReference.Combine(OutputDirectory, "Mac", MacExecutableName), FileReference.Combine(OutputDirectory, "IOS", IOSExecutableName), FileReference.Combine(OutputDirectory, "TVOS", TVOSExecutableName), ProjectTarget, Configuration));
                                            }
                                            else if (BuildPlatform.Platform == UnrealTargetPlatform.IOS)
                                            {
                                                string IOSExecutableName = UEBuildTarget.MakeBinaryFileName(ExeName, UnrealTargetPlatform.IOS, (ExeName == "UE4Editor" && Configuration == UnrealTargetConfiguration.DebugGame) ? UnrealTargetConfiguration.Development : Configuration, ProjectTarget.TargetRules.Architecture, ProjectTarget.TargetRules.UndecoratedConfiguration, UEBuildBinaryType.Executable, null);
                                                string TVOSExecutableName = IOSExecutableName.Replace("-IOS-", "-TVOS-");
                                                string MacExecutableName = IOSExecutableName.Replace("-IOS-", "-Mac-");
                                                BuildConfigs.Add(new XcodeBuildConfig(ConfigName, TargetName, FileReference.Combine(OutputDirectory, "Mac", IOSExecutableName), FileReference.Combine(OutputDirectory, "IOS", IOSExecutableName), FileReference.Combine(OutputDirectory, "TVOS", TVOSExecutableName), ProjectTarget, Configuration));
                                            }
                                        }
									}
								}
							}
						}
					}
				}
			}

			return BuildConfigs;
		}

		private void WriteSchemeFile(string TargetName, string TargetGuid, string BuildTargetGuid, string IndexTargetGuid, bool bHasEditorConfiguration, string GameProjectPath)
		{
			string DefaultConfiguration = bHasEditorConfiguration && !XcodeProjectFileGenerator.bGeneratingRunIOSProject && !XcodeProjectFileGenerator.bGeneratingRunTVOSProject ? "Development Editor" : "Development";

			var Content = new StringBuilder();

			Content.Append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + ProjectFileGenerator.NewLine);
			Content.Append("<Scheme" + ProjectFileGenerator.NewLine);
			Content.Append("   LastUpgradeVersion = \"0710\"" + ProjectFileGenerator.NewLine);
			Content.Append("   version = \"1.3\">" + ProjectFileGenerator.NewLine);
			Content.Append("   <BuildAction" + ProjectFileGenerator.NewLine);
			Content.Append("      parallelizeBuildables = \"YES\"" + ProjectFileGenerator.NewLine);
			Content.Append("      buildImplicitDependencies = \"YES\">" + ProjectFileGenerator.NewLine);
			Content.Append("      <BuildActionEntries>" + ProjectFileGenerator.NewLine);
			Content.Append("         <BuildActionEntry" + ProjectFileGenerator.NewLine);
			Content.Append("            buildForTesting = \"YES\"" + ProjectFileGenerator.NewLine);
			Content.Append("            buildForRunning = \"YES\"" + ProjectFileGenerator.NewLine);
			Content.Append("            buildForProfiling = \"YES\"" + ProjectFileGenerator.NewLine);
			Content.Append("            buildForArchiving = \"YES\"" + ProjectFileGenerator.NewLine);
			Content.Append("            buildForAnalyzing = \"YES\">" + ProjectFileGenerator.NewLine);
			Content.Append("            <BuildableReference" + ProjectFileGenerator.NewLine);
			Content.Append("               BuildableIdentifier = \"primary\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BlueprintIdentifier = \"" + TargetGuid + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BuildableName = \"" + TargetName + ".app\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BlueprintName = \"" + TargetName + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("               ReferencedContainer = \"container:" + TargetName + ".xcodeproj\">" + ProjectFileGenerator.NewLine);
			Content.Append("            </BuildableReference>" + ProjectFileGenerator.NewLine);
			Content.Append("         </BuildActionEntry>" + ProjectFileGenerator.NewLine);
			Content.Append("      </BuildActionEntries>" + ProjectFileGenerator.NewLine);
			Content.Append("   </BuildAction>" + ProjectFileGenerator.NewLine);
			Content.Append("   <TestAction" + ProjectFileGenerator.NewLine);
			Content.Append("      buildConfiguration = \"" + DefaultConfiguration + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("      selectedDebuggerIdentifier = \"Xcode.DebuggerFoundation.Debugger.LLDB\"" + ProjectFileGenerator.NewLine);
			Content.Append("      selectedLauncherIdentifier = \"Xcode.DebuggerFoundation.Launcher.LLDB\"" + ProjectFileGenerator.NewLine);
			Content.Append("      shouldUseLaunchSchemeArgsEnv = \"YES\">" + ProjectFileGenerator.NewLine);
			Content.Append("      <Testables>" + ProjectFileGenerator.NewLine);
			Content.Append("      </Testables>" + ProjectFileGenerator.NewLine);
			Content.Append("      <MacroExpansion>" + ProjectFileGenerator.NewLine);
			Content.Append("            <BuildableReference" + ProjectFileGenerator.NewLine);
			Content.Append("               BuildableIdentifier = \"primary\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BlueprintIdentifier = \"" + TargetGuid + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BuildableName = \"" + TargetName + ".app\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BlueprintName = \"" + TargetName + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("               ReferencedContainer = \"container:" + TargetName + ".xcodeproj\">" + ProjectFileGenerator.NewLine);
			Content.Append("            </BuildableReference>" + ProjectFileGenerator.NewLine);
			Content.Append("      </MacroExpansion>" + ProjectFileGenerator.NewLine);
			Content.Append("      <AdditionalOptions>" + ProjectFileGenerator.NewLine);
			Content.Append("      </AdditionalOptions>" + ProjectFileGenerator.NewLine);
			Content.Append("   </TestAction>" + ProjectFileGenerator.NewLine);
			Content.Append("   <LaunchAction" + ProjectFileGenerator.NewLine);
			Content.Append("      buildConfiguration = \"" + DefaultConfiguration + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("      selectedDebuggerIdentifier = \"Xcode.DebuggerFoundation.Debugger.LLDB\"" + ProjectFileGenerator.NewLine);
			Content.Append("      selectedLauncherIdentifier = \"Xcode.DebuggerFoundation.Launcher.LLDB\"" + ProjectFileGenerator.NewLine);
			Content.Append("      launchStyle = \"0\"" + ProjectFileGenerator.NewLine);
			Content.Append("      useCustomWorkingDirectory = \"NO\"" + ProjectFileGenerator.NewLine);
			Content.Append("      ignoresPersistentStateOnLaunch = \"NO\"" + ProjectFileGenerator.NewLine);
			Content.Append("      debugDocumentVersioning = \"YES\"" + ProjectFileGenerator.NewLine);
			Content.Append("      debugServiceExtension = \"internal\"" + ProjectFileGenerator.NewLine);
			Content.Append("      allowLocationSimulation = \"YES\">" + ProjectFileGenerator.NewLine);
			Content.Append("      <BuildableProductRunnable" + ProjectFileGenerator.NewLine);
			Content.Append("         runnableDebuggingMode = \"0\">" + ProjectFileGenerator.NewLine);
			Content.Append("            <BuildableReference" + ProjectFileGenerator.NewLine);
			Content.Append("               BuildableIdentifier = \"primary\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BlueprintIdentifier = \"" + TargetGuid + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BuildableName = \"" + TargetName + ".app\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BlueprintName = \"" + TargetName + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("               ReferencedContainer = \"container:" + TargetName + ".xcodeproj\">" + ProjectFileGenerator.NewLine);
			Content.Append("            </BuildableReference>" + ProjectFileGenerator.NewLine);
			Content.Append("      </BuildableProductRunnable>" + ProjectFileGenerator.NewLine);
			if (bHasEditorConfiguration && TargetName != "UE4")
			{
				Content.Append("      <CommandLineArguments>" + ProjectFileGenerator.NewLine);
				if (IsForeignProject)
				{
					Content.Append("         <CommandLineArgument" + ProjectFileGenerator.NewLine);
					Content.Append("            argument = \"&quot;" + GameProjectPath + "&quot;\"" + ProjectFileGenerator.NewLine);
					Content.Append("            isEnabled = \"YES\">" + ProjectFileGenerator.NewLine);
					Content.Append("         </CommandLineArgument>" + ProjectFileGenerator.NewLine);
				}
				else
				{
					Content.Append("         <CommandLineArgument" + ProjectFileGenerator.NewLine);
					Content.Append("            argument = \"" + TargetName + "\"" + ProjectFileGenerator.NewLine);
					Content.Append("            isEnabled = \"YES\">" + ProjectFileGenerator.NewLine);
					Content.Append("         </CommandLineArgument>" + ProjectFileGenerator.NewLine);
				}
				// Always add a configuration argument
				Content.Append("         <CommandLineArgument" + ProjectFileGenerator.NewLine);
				Content.Append("            argument = \"-RunConfig=$(Configuration)\"" + ProjectFileGenerator.NewLine);
				Content.Append("            isEnabled = \"YES\">" + ProjectFileGenerator.NewLine);
				Content.Append("         </CommandLineArgument>" + ProjectFileGenerator.NewLine);
				Content.Append("      </CommandLineArguments>" + ProjectFileGenerator.NewLine);
			}
			Content.Append("      <AdditionalOptions>" + ProjectFileGenerator.NewLine);
			Content.Append("      </AdditionalOptions>" + ProjectFileGenerator.NewLine);
			Content.Append("   </LaunchAction>" + ProjectFileGenerator.NewLine);
			Content.Append("   <ProfileAction" + ProjectFileGenerator.NewLine);
			Content.Append("      buildConfiguration = \"" + DefaultConfiguration + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("      shouldUseLaunchSchemeArgsEnv = \"YES\"" + ProjectFileGenerator.NewLine);
			Content.Append("      savedToolIdentifier = \"\"" + ProjectFileGenerator.NewLine);
			Content.Append("      useCustomWorkingDirectory = \"NO\"" + ProjectFileGenerator.NewLine);
			Content.Append("      debugDocumentVersioning = \"YES\">" + ProjectFileGenerator.NewLine);
			Content.Append("      <BuildableProductRunnable" + ProjectFileGenerator.NewLine);
			Content.Append("         runnableDebuggingMode = \"0\">" + ProjectFileGenerator.NewLine);
			Content.Append("            <BuildableReference" + ProjectFileGenerator.NewLine);
			Content.Append("               BuildableIdentifier = \"primary\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BlueprintIdentifier = \"" + TargetGuid + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BuildableName = \"" + TargetName + ".app\"" + ProjectFileGenerator.NewLine);
			Content.Append("               BlueprintName = \"" + TargetName + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("               ReferencedContainer = \"container:" + TargetName + ".xcodeproj\">" + ProjectFileGenerator.NewLine);
			Content.Append("            </BuildableReference>" + ProjectFileGenerator.NewLine);
			Content.Append("      </BuildableProductRunnable>" + ProjectFileGenerator.NewLine);
			Content.Append("   </ProfileAction>" + ProjectFileGenerator.NewLine);
			Content.Append("   <AnalyzeAction" + ProjectFileGenerator.NewLine);
			Content.Append("      buildConfiguration = \"" + DefaultConfiguration + "\">" + ProjectFileGenerator.NewLine);
			Content.Append("   </AnalyzeAction>" + ProjectFileGenerator.NewLine);
			Content.Append("   <ArchiveAction" + ProjectFileGenerator.NewLine);
			Content.Append("      buildConfiguration = \"" + DefaultConfiguration + "\"" + ProjectFileGenerator.NewLine);
			Content.Append("      revealArchiveInOrganizer = \"YES\">" + ProjectFileGenerator.NewLine);
			Content.Append("   </ArchiveAction>" + ProjectFileGenerator.NewLine);
			Content.Append("</Scheme>" + ProjectFileGenerator.NewLine);

			DirectoryReference SchemesDir = new DirectoryReference(ProjectFilePath.FullName + "/xcshareddata/xcschemes");
			if (!DirectoryReference.Exists(SchemesDir))
			{
				DirectoryReference.CreateDirectory(SchemesDir);
			}

			string SchemeFilePath = SchemesDir + "/" + TargetName + ".xcscheme";
			File.WriteAllText(SchemeFilePath, Content.ToString(), new UTF8Encoding());

			Content.Clear();

			Content.Append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + ProjectFileGenerator.NewLine);
			Content.Append("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">" + ProjectFileGenerator.NewLine);
			Content.Append("<plist version=\"1.0\">" + ProjectFileGenerator.NewLine);
			Content.Append("<dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t<key>SchemeUserState</key>" + ProjectFileGenerator.NewLine);
			Content.Append("\t<dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t<key>" + TargetName + ".xcscheme_^#shared#^_</key>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t<dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t<key>orderHint</key>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t<integer>" + SchemeOrderHint.ToString() + "</integer>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t</dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t</dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t<key>SuppressBuildableAutocreation</key>" + ProjectFileGenerator.NewLine);
			Content.Append("\t<dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t<key>" + TargetGuid + "</key>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t<dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t<key>primary</key>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t<true/>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t</dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t<key>" + BuildTargetGuid + "</key>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t<dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t<key>primary</key>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t<true/>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t</dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t<key>" + IndexTargetGuid + "</key>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t<dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t<key>primary</key>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t\t<true/>" + ProjectFileGenerator.NewLine);
			Content.Append("\t\t</dict>" + ProjectFileGenerator.NewLine);
			Content.Append("\t</dict>" + ProjectFileGenerator.NewLine);
			Content.Append("</dict>" + ProjectFileGenerator.NewLine);
			Content.Append("</plist>" + ProjectFileGenerator.NewLine);

			DirectoryReference ManagementFileDir = new DirectoryReference(ProjectFilePath.FullName + "/xcuserdata/" + Environment.UserName + ".xcuserdatad/xcschemes");
			if (!DirectoryReference.Exists(ManagementFileDir))
			{
				DirectoryReference.CreateDirectory(ManagementFileDir);
			}

			string ManagementFilePath = ManagementFileDir + "/xcschememanagement.plist";
			File.WriteAllText(ManagementFilePath, Content.ToString(), new UTF8Encoding());

			SchemeOrderHint++;
		}

		/// Implements Project interface
		public override bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			bool bSuccess = true;

			var TargetName = ProjectFilePath.GetFileNameWithoutExtension();
			var TargetGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var TargetConfigListGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var TargetDependencyGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var TargetProxyGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var TargetAppGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var BuildTargetName = TargetName + "_Build";
			var BuildTargetGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var BuildTargetConfigListGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var IndexTargetName = TargetName + "_Index";
			var IndexTargetGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var IndexTargetConfigListGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var ProjectGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var ProjectConfigListGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var MainGroupGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var ProductRefGroupGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			var SourcesBuildPhaseGuid = XcodeProjectFileGenerator.MakeXcodeGuid();

			// Figure out all the desired configurations
			var BuildConfigs = GetSupportedBuildConfigs(InPlatforms, InConfigurations);
			if (BuildConfigs.Count == 0)
			{
				return true;
			}

			bool bIsAGame = false;
			FileReference GameProjectPath = null;
			foreach(ProjectTarget Target in ProjectTargets)
			{
				if(Target.UnrealProjectFilePath != null)
				{
					bIsAGame = true;
					GameProjectPath = Target.UnrealProjectFilePath;
					break;
				}
			}

			bool bHasEditorConfiguration = false;

			var ProjectBuildConfigs = new Dictionary<string, XcodeBuildConfig>();
			var TargetBuildConfigs = new Dictionary<string, XcodeBuildConfig>();
			var BuildTargetBuildConfigs = new Dictionary<string, XcodeBuildConfig>();
			var IndexTargetBuildConfigs = new Dictionary<string, XcodeBuildConfig>();
			foreach (var Config in BuildConfigs)
			{
				ProjectBuildConfigs[XcodeProjectFileGenerator.MakeXcodeGuid()] = Config;
				TargetBuildConfigs[XcodeProjectFileGenerator.MakeXcodeGuid()] = Config;
				BuildTargetBuildConfigs[XcodeProjectFileGenerator.MakeXcodeGuid()] = Config;
				IndexTargetBuildConfigs[XcodeProjectFileGenerator.MakeXcodeGuid()] = Config;

				if (Config.ProjectTarget.TargetRules.Type == TargetType.Editor)
				{
					bHasEditorConfiguration = true;
				}
			}

			var PBXBuildFileSection = new StringBuilder();
			var PBXFileReferenceSection = new StringBuilder();
			var PBXSourcesBuildPhaseSection = new StringBuilder();
			GenerateSectionsWithSourceFiles(PBXBuildFileSection, PBXFileReferenceSection, PBXSourcesBuildPhaseSection, TargetAppGuid, TargetName);

			var ProjectFileContent = new StringBuilder();

			ProjectFileContent.Append("// !$*UTF8*$!" + ProjectFileGenerator.NewLine);
			ProjectFileContent.Append("{" + ProjectFileGenerator.NewLine);
			ProjectFileContent.Append("\tarchiveVersion = 1;" + ProjectFileGenerator.NewLine);
			ProjectFileContent.Append("\tclasses = {" + ProjectFileGenerator.NewLine);
			ProjectFileContent.Append("\t};" + ProjectFileGenerator.NewLine);
			ProjectFileContent.Append("\tobjectVersion = 46;" + ProjectFileGenerator.NewLine);
			ProjectFileContent.Append("\tobjects = {" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);

			AppendBuildFileSection(ProjectFileContent, PBXBuildFileSection);
			AppendFileReferenceSection(ProjectFileContent, PBXFileReferenceSection);
			AppendSourcesBuildPhaseSection(ProjectFileContent, PBXSourcesBuildPhaseSection, SourcesBuildPhaseGuid);
			AppendContainerItemProxySection(ProjectFileContent, BuildTargetName, BuildTargetGuid, TargetProxyGuid, ProjectGuid);
			if (!XcodeProjectFileGenerator.bGeneratingRunIOSProject)
			{
				AppendTargetDependencySection(ProjectFileContent, BuildTargetName, BuildTargetGuid, TargetDependencyGuid, TargetProxyGuid);
			}
			AppendGroupSection(ProjectFileContent, MainGroupGuid, ProductRefGroupGuid, TargetAppGuid, TargetName);
			AppendLegacyTargetSection(ProjectFileContent, BuildTargetName, BuildTargetGuid, BuildTargetConfigListGuid, GameProjectPath);
			AppendRunTargetSection(ProjectFileContent, TargetName, TargetGuid, TargetConfigListGuid, TargetDependencyGuid, TargetAppGuid);
			AppendIndexTargetSection(ProjectFileContent, IndexTargetName, IndexTargetGuid, IndexTargetConfigListGuid, SourcesBuildPhaseGuid);
			AppendProjectSection(ProjectFileContent, TargetName, TargetGuid, BuildTargetName, BuildTargetGuid, IndexTargetName, IndexTargetGuid, MainGroupGuid, ProductRefGroupGuid, ProjectGuid, ProjectConfigListGuid, GameProjectPath);
			AppendXCBuildConfigurationSection(ProjectFileContent, ProjectBuildConfigs, TargetBuildConfigs, BuildTargetBuildConfigs, IndexTargetBuildConfigs, bIsAGame, GameProjectPath);
			AppendXCConfigurationListSection(ProjectFileContent, TargetName, BuildTargetName, IndexTargetName, ProjectConfigListGuid, ProjectBuildConfigs,
				TargetConfigListGuid, TargetBuildConfigs, BuildTargetConfigListGuid, BuildTargetBuildConfigs, IndexTargetConfigListGuid, IndexTargetBuildConfigs);

			ProjectFileContent.Append("\t};" + ProjectFileGenerator.NewLine);
			ProjectFileContent.Append("\trootObject = " + ProjectGuid + " /* Project object */;" + ProjectFileGenerator.NewLine);
			ProjectFileContent.Append("}" + ProjectFileGenerator.NewLine);

			if (bSuccess)
			{
				var PBXProjFilePath = ProjectFilePath + "/project.pbxproj";
				bSuccess = ProjectFileGenerator.WriteFileIfChanged(PBXProjFilePath.FullName, ProjectFileContent.ToString(), new UTF8Encoding());
			}

			if (bSuccess)
			{
				WriteSchemeFile(TargetName, TargetGuid, BuildTargetGuid, IndexTargetGuid, bHasEditorConfiguration, GameProjectPath != null ? GameProjectPath.FullName : "");
			}

			return bSuccess;
		}

		static private int SchemeOrderHint = 0;
	}
}
