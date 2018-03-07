// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Linq;
using System.Xml.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Represents a folder within the master project (e.g. Visual Studio solution)
	/// </summary>
	class VisualStudioSolutionFolder : MasterProjectFolder
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public VisualStudioSolutionFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
			: base(InitOwnerProjectFileGenerator, InitFolderName)
		{
			// Generate a unique GUID for this folder
			// NOTE: When saving generated project files, we ignore differences in GUIDs if every other part of the file
			//       matches identically with the pre-existing file
			FolderGUID = Guid.NewGuid();
		}


		/// GUID for this folder
		public Guid FolderGUID
		{
			get;
			private set;
		}
	}

	enum VCProjectFileFormat
	{
		Default,          // Default to the best installed version, but allow SDKs to override
		VisualStudio2012, // Unsupported
		VisualStudio2013, // Unsupported
		VisualStudio2015,
		VisualStudio2017,
	}

	/// <summary>
	/// Visual C++ project file generator implementation
	/// </summary>
	class VCProjectFileGenerator : ProjectFileGenerator
	{
		/// <summary>
		/// The version of Visual Studio to generate project files for.
		/// </summary>
		[XmlConfigFile(Name = "Version")]
		VCProjectFileFormat ProjectFileFormat = VCProjectFileFormat.Default;

		/// <summary>
		/// Whether to add the -FastPDB option to build command lines by default
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		bool bAddFastPDBToProjects = false;

		/// <summary>
		/// Whether to generate per-file intellisense data
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		bool bUsePerFileIntellisense = false;

		/// <summary>
		/// Whether to include a dependency on ShaderCompileWorker when generating project files for the editor.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		bool bEditorDependsOnShaderCompileWorker = true;

		/// <summary>
		/// Override for the build tool to use in generated projects. If the compiler version is specified on the command line, we use the same argument on the 
		/// command line for generated projects.
		/// </summary>
		string BuildToolOverride;

		/// <summary>
		/// Default constructor
		/// </summary>
		/// <param name="InOnlyGameProject">The single project to generate project files for, or null</param>
		/// <param name="InProjectFileFormat">Override the project file format to use</param>
		/// <param name="InOverrideCompiler">Override the compiler version to use</param>
		public VCProjectFileGenerator(FileReference InOnlyGameProject, VCProjectFileFormat InProjectFileFormat, WindowsCompiler InOverrideCompiler)
			: base(InOnlyGameProject)
		{
			XmlConfig.ApplyTo(this);

			if(InProjectFileFormat != VCProjectFileFormat.Default)
			{
				ProjectFileFormat = InProjectFileFormat;
			}

			if(InOverrideCompiler == WindowsCompiler.VisualStudio2015)
			{
				BuildToolOverride = "-2015";
			}
			else if(InOverrideCompiler == WindowsCompiler.VisualStudio2017)
			{
				BuildToolOverride = "-2017";
			}
		}

		/// File extension for project files we'll be generating (e.g. ".vcxproj")
		override public string ProjectFileExtension
		{
			get
			{
				return ".vcxproj";
			}
		}

		/// <summary>
		/// </summary>
		public override void CleanProjectFiles(DirectoryReference InMasterProjectDirectory, string InMasterProjectName, DirectoryReference InIntermediateProjectFilesDirectory)
		{
			FileReference MasterProjectFile = FileReference.Combine(InMasterProjectDirectory, InMasterProjectName);
			FileReference MasterProjDeleteFilename = MasterProjectFile + ".sln";
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				FileReference.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".sdf";
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				FileReference.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".suo";
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				FileReference.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".v11.suo";
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				FileReference.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".v12.suo";
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				FileReference.Delete(MasterProjDeleteFilename);
			}

			// Delete the project files folder
			if (DirectoryReference.Exists(InIntermediateProjectFilesDirectory))
			{
				try
				{
					DirectoryReference.Delete(InIntermediateProjectFilesDirectory, true);
				}
				catch (Exception Ex)
				{
					Log.TraceInformation("Error while trying to clean project files path {0}. Ignored.", InIntermediateProjectFilesDirectory);
					Log.TraceInformation("\t" + Ex.Message);
				}
			}
		}

		/// <summary>
		/// Allocates a generator-specific project file object
		/// </summary>
		/// <param name="InitFilePath">Path to the project file</param>
		/// <returns>The newly allocated project file object</returns>
		protected override ProjectFile AllocateProjectFile(FileReference InitFilePath)
		{
			return new VCProjectFile(InitFilePath, OnlyGameProject, ProjectFileFormat, bAddFastPDBToProjects, bUsePerFileIntellisense, BuildToolOverride);
		}


		/// ProjectFileGenerator interface
		public override MasterProjectFolder AllocateMasterProjectFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
		{
			return new VisualStudioSolutionFolder(InitOwnerProjectFileGenerator, InitFolderName);
		}

		/// "4.0", "12.0", or "14.0", etc...
		static public string GetProjectFileToolVersionString(VCProjectFileFormat ProjectFileFormat)
		{
			switch (ProjectFileFormat)
            {
                case VCProjectFileFormat.VisualStudio2012:
                    return "4.0";
				case VCProjectFileFormat.VisualStudio2013:
					return "12.0";
				case VCProjectFileFormat.VisualStudio2015:
					return "14.0";
				case VCProjectFileFormat.VisualStudio2017:
					return "15.0";
			}
			return string.Empty;
		}

		/// for instance: <PlatformToolset>v110</PlatformToolset>
		static public string GetProjectFilePlatformToolsetVersionString(VCProjectFileFormat ProjectFileFormat)
		{
            switch (ProjectFileFormat)
            {
                case VCProjectFileFormat.VisualStudio2012:
                    return "v110";
                case VCProjectFileFormat.VisualStudio2013:
                    return "v120";
                case VCProjectFileFormat.VisualStudio2015:
					return "v140";
				case VCProjectFileFormat.VisualStudio2017:
                    return "v141";
            }
			return string.Empty;
		}

		/// This is the platform name that Visual Studio is always guaranteed to support.  We'll use this as
		/// a platform for any project configurations where our actual platform is not supported by the
		/// installed version of Visual Studio (e.g, "iOS")
		public const string DefaultPlatformName = "Win32";

		/// The platform name that must be used for .NET projects
		public const string DotNetPlatformName = "Any CPU";


		/// <summary>
		/// Configures project generator based on command-line options
		/// </summary>
		/// <param name="Arguments">Arguments passed into the program</param>
		/// <param name="IncludeAllPlatforms">True if all platforms should be included</param>
		protected override void ConfigureProjectFileGeneration(String[] Arguments, ref bool IncludeAllPlatforms)
		{
			// Call parent implementation first
			base.ConfigureProjectFileGeneration(Arguments, ref IncludeAllPlatforms);
		}

		/// <summary>
		/// Adds Extra files that are specific to Visual Studio projects
		/// </summary>
		/// <param name="EngineProject">Project to add files to</param>
		protected override void AddEngineExtrasFiles(ProjectFile EngineProject)
		{
			base.AddEngineExtrasFiles(EngineProject);

			// Add our UE4.natvis file
			FileReference NatvisFilePath = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Extras", "VisualStudioDebugging", "UE4.natvis");
			if (FileReference.Exists(NatvisFilePath))
			{
				EngineProject.AddFileToProject(NatvisFilePath, UnrealBuildTool.EngineDirectory);
			}
		}

		/// <summary>
		/// Selects which platforms and build configurations we want in the project file
		/// </summary>
		/// <param name="IncludeAllPlatforms">True if we should include ALL platforms that are supported on this machine.  Otherwise, only desktop platforms will be included.</param>
		/// <param name="SupportedPlatformNames">Output string for supported platforms, returned as comma-separated values.</param>
		protected override void SetupSupportedPlatformsAndConfigurations(bool IncludeAllPlatforms, out string SupportedPlatformNames)
		{
			// Call parent implementation to figure out the actual platforms
			base.SetupSupportedPlatformsAndConfigurations(IncludeAllPlatforms, out SupportedPlatformNames);

			// If we have a non-default setting for visual studio, check the compiler exists. If not, revert to the default.
			if(ProjectFileFormat == VCProjectFileFormat.VisualStudio2015)
			{
				DirectoryReference VCInstallDir;
				if (!WindowsPlatform.TryGetVCInstallDir(WindowsCompiler.VisualStudio2015, out VCInstallDir))
				{
					Log.TraceWarning("Visual Studio C++ 2015 installation not found - ignoring preferred project file format.");
					ProjectFileFormat = VCProjectFileFormat.Default;
				}
			}
			else if(ProjectFileFormat == VCProjectFileFormat.VisualStudio2017)
			{
				DirectoryReference VCInstallDir;
				if (!WindowsPlatform.TryGetVCInstallDir(WindowsCompiler.VisualStudio2017, out VCInstallDir))
				{
					Log.TraceWarning("Visual Studio C++ 2017 installation not found - ignoring preferred project file format.");
					ProjectFileFormat = VCProjectFileFormat.Default;
				}
			}

			// Certain platforms override the project file format because their debugger add-ins may not yet support the latest
			// version of Visual Studio.  This is their chance to override that.
			// ...but only if the user didn't override this via the command-line.
			if (ProjectFileFormat == VCProjectFileFormat.Default)
			{
				// Pick the best platform installed by default
				DirectoryReference VCInstallDir;
				if (WindowsPlatform.TryGetVCInstallDir(WindowsCompiler.VisualStudio2015, out VCInstallDir))
				{
					ProjectFileFormat = VCProjectFileFormat.VisualStudio2015;
				}
				else if (WindowsPlatform.TryGetVCInstallDir(WindowsCompiler.VisualStudio2017, out VCInstallDir))
				{
					ProjectFileFormat = VCProjectFileFormat.VisualStudio2017;
				}

				// Allow the SDKs to override
				foreach (UnrealTargetPlatform SupportedPlatform in SupportedPlatforms)
				{
					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(SupportedPlatform, true);
					if (BuildPlatform != null)
					{
						// Don't worry about platforms that we're missing SDKs for
						if (BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid)
						{
							VCProjectFileFormat ProposedFormat = ProjectFileFormat;

							// Reduce the Visual Studio version to the max supported by each platform we plan to include.
							if (ProjectFileFormat == VCProjectFileFormat.Default || ProposedFormat < ProjectFileFormat)
							{
								ProjectFileFormat = ProposedFormat;
							}
						}
					}
				}
			}
		}


		/// <summary>
		/// Used to sort VC solution config names along with the config and platform values
		/// </summary>
		class VCSolutionConfigCombination
		{
			/// Visual Studio solution configuration name for this config+platform
			public string VCSolutionConfigAndPlatformName;

			/// Configuration name
			public UnrealTargetConfiguration Configuration;

			/// Platform name
			public UnrealTargetPlatform Platform;

			/// The target configuration name
			public string TargetConfigurationName;

			public override string ToString()
			{
				return String.Format("{0}={1} {2} {3}", VCSolutionConfigAndPlatformName, Configuration, Platform, TargetConfigurationName);
			}
		}


		/// <summary>
		/// Composes a string to use for the Visual Studio solution configuration, given a build configuration and target rules configuration name
		/// </summary>
		/// <param name="Configuration">The build configuration</param>
		/// <param name="TargetConfigurationName">The target rules configuration name</param>
		/// <returns>The generated solution configuration name</returns>
		string MakeSolutionConfigurationName(UnrealTargetConfiguration Configuration, string TargetConfigurationName)
		{
			string SolutionConfigName = Configuration.ToString();

			// Don't bother postfixing "Game" or "Program" -- that will be the default when using "Debug", "Development", etc.
			// Also don't postfix "RocketGame" when we're building Rocket game projects.  That's the only type of game there is in that case!
			if (!TargetConfigurationName.Equals(TargetType.Game.ToString(), StringComparison.InvariantCultureIgnoreCase) &&
				!TargetConfigurationName.Equals(TargetType.Program.ToString(), StringComparison.InvariantCultureIgnoreCase))
			{
				SolutionConfigName += " " + TargetConfigurationName;
			}

			return SolutionConfigName;
		}

		/// <summary>
		/// Writes the project files to disk
		/// </summary>
		/// <returns>True if successful</returns>
		protected override bool WriteProjectFiles()
		{
			if(!base.WriteProjectFiles())
			{
				return false;
			}

			// Write AutomationReferences file
			if (AutomationProjectFiles.Any())
			{
				XNamespace NS = XNamespace.Get("http://schemas.microsoft.com/developer/msbuild/2003");

				DirectoryReference AutomationToolDir = DirectoryReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Programs", "AutomationTool");
				new XDocument(
					new XElement(NS + "Project",
						new XAttribute("ToolsVersion", VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat)),
						new XAttribute("DefaultTargets", "Build"),
						new XElement(NS + "ItemGroup",
							from AutomationProject in AutomationProjectFiles
							select new XElement(NS + "ProjectReference",
								new XAttribute("Include", AutomationProject.ProjectFilePath.MakeRelativeTo(AutomationToolDir)),
								new XElement(NS + "Project", (AutomationProject as VCSharpProjectFile).ProjectGUID.ToString("B")),
								new XElement(NS + "Name", AutomationProject.ProjectFilePath.GetFileNameWithoutExtension()),
								new XElement(NS + "Private", "false")
							)
						)
					)
				).Save(FileReference.Combine(AutomationToolDir, "AutomationTool.csproj.References").FullName);
			}

			return true;
		}


		protected override bool WriteMasterProjectFile(ProjectFile UBTProject)
		{
			bool bSuccess = true;

			string SolutionFileName = MasterProjectName + ".sln";

			// Setup solution file content
			StringBuilder VCSolutionFileContent = new StringBuilder();

			const string VersionTag = "# UnrealEngineGeneratedSolutionVersion=1.0";

			// Solution file header
			if (ProjectFileFormat == VCProjectFileFormat.VisualStudio2017)
			{
				VCSolutionFileContent.Append(
					ProjectFileGenerator.NewLine +
					"Microsoft Visual Studio Solution File, Format Version 12.00" + ProjectFileGenerator.NewLine +
					"# Visual Studio 15" + ProjectFileGenerator.NewLine +
					"VisualStudioVersion = 15.0.25807.0" + ProjectFileGenerator.NewLine +
					"MinimumVisualStudioVersion = 10.0.40219.1" + ProjectFileGenerator.NewLine);
			}
			else if (ProjectFileFormat == VCProjectFileFormat.VisualStudio2015)
			{
				VCSolutionFileContent.Append(
					ProjectFileGenerator.NewLine +
					"Microsoft Visual Studio Solution File, Format Version 12.00" + ProjectFileGenerator.NewLine +
					"# Visual Studio 14" + ProjectFileGenerator.NewLine +
					"VisualStudioVersion = 14.0.22310.1" + ProjectFileGenerator.NewLine +
					"MinimumVisualStudioVersion = 10.0.40219.1" + ProjectFileGenerator.NewLine);
			}
			else if (ProjectFileFormat == VCProjectFileFormat.VisualStudio2013)
			{
				VCSolutionFileContent.Append(
					ProjectFileGenerator.NewLine +
					"Microsoft Visual Studio Solution File, Format Version 12.00" + ProjectFileGenerator.NewLine +
					"# Visual Studio 2013" + ProjectFileGenerator.NewLine +
					VersionTag + ProjectFileGenerator.NewLine);
            }
            else if (ProjectFileFormat == VCProjectFileFormat.VisualStudio2012)
            {
                VCSolutionFileContent.Append(
                    ProjectFileGenerator.NewLine +
                    "Microsoft Visual Studio Solution File, Format Version 12.00" + ProjectFileGenerator.NewLine +
                    "# Visual Studio 2012" + ProjectFileGenerator.NewLine +
                    VersionTag + ProjectFileGenerator.NewLine);
            }
			else
			{
				throw new BuildException("Unexpected ProjectFileFormat");
			}

			// Find the projects for ShaderCompileWorker and UnrealLightmass
			ProjectFile ShaderCompileWorkerProject = null;
			ProjectFile UnrealLightmassProject = null;
			foreach (ProjectFile Project in AllProjectFiles)
			{
				if (Project.ProjectTargets.Count == 1)
				{
					FileReference TargetFilePath = Project.ProjectTargets[0].TargetFilePath;
					if (TargetFilePath != null)
					{
						string TargetFileName = TargetFilePath.GetFileNameWithoutAnyExtensions();
						if (TargetFileName.Equals("ShaderCompileWorker", StringComparison.InvariantCultureIgnoreCase))
						{
							ShaderCompileWorkerProject = Project;
						}
						else if (TargetFileName.Equals("UnrealLightmass", StringComparison.InvariantCultureIgnoreCase))
						{
							UnrealLightmassProject = Project;
						}
					}
					if (ShaderCompileWorkerProject != null
						&& UnrealLightmassProject != null)
					{
						break;
					}
				}
			}

			// Solution folders, files and project entries
			{
				// This the GUID that Visual Studio uses to identify a solution folder
				string SolutionFolderEntryGUID = "{2150E333-8FDC-42A3-9474-1A3956D46DE8}";

				// Solution folders
				{
					List<MasterProjectFolder> AllSolutionFolders = new List<MasterProjectFolder>();
					System.Action<List<MasterProjectFolder> /* Folders */ > GatherFoldersFunction = null;
					GatherFoldersFunction = FolderList =>
						{
							AllSolutionFolders.AddRange(FolderList);
							foreach (MasterProjectFolder CurSubFolder in FolderList)
							{
								GatherFoldersFunction(CurSubFolder.SubFolders);
							}
						};
					GatherFoldersFunction(RootFolder.SubFolders);

					foreach (VisualStudioSolutionFolder CurFolder in AllSolutionFolders)
					{
						string FolderGUIDString = CurFolder.FolderGUID.ToString("B").ToUpperInvariant();
						VCSolutionFileContent.Append(
								"Project(\"" + SolutionFolderEntryGUID + "\") = \"" + CurFolder.FolderName + "\", \"" + CurFolder.FolderName + "\", \"" + FolderGUIDString + "\"" + ProjectFileGenerator.NewLine);

						// Add any files that are inlined right inside the solution folder
						if (CurFolder.Files.Count > 0)
						{
							VCSolutionFileContent.Append(
									"	ProjectSection(SolutionItems) = preProject" + ProjectFileGenerator.NewLine);
							foreach (string CurFile in CurFolder.Files)
							{
								// Syntax is:  <relative file path> = <relative file path>
								VCSolutionFileContent.Append(
									"		" + CurFile + " = " + CurFile + ProjectFileGenerator.NewLine);
							}
							VCSolutionFileContent.Append(
									"	EndProjectSection" + ProjectFileGenerator.NewLine);
						}

						VCSolutionFileContent.Append(
								"EndProject" + ProjectFileGenerator.NewLine
							);
					}
				}


				// Project files
				foreach (MSBuildProjectFile CurProject in AllProjectFiles)
				{
					// Visual Studio uses different GUID types depending on the project type
					string ProjectTypeGUID = CurProject.ProjectTypeGUID;

					// NOTE: The project name in the solution doesn't actually *have* to match the project file name on disk.  However,
					//       we prefer it when it does match so we use the actual file name here.
					string ProjectNameInSolution = CurProject.ProjectFilePath.GetFileNameWithoutExtension();

					// Use the existing project's GUID that's already known to us
					string ProjectGUID = CurProject.ProjectGUID.ToString("B").ToUpperInvariant();

					VCSolutionFileContent.Append(
							"Project(\"" + ProjectTypeGUID + "\") = \"" + ProjectNameInSolution + "\", \"" + CurProject.ProjectFilePath.MakeRelativeTo(ProjectFileGenerator.MasterProjectPath) + "\", \"" + ProjectGUID + "\"" + ProjectFileGenerator.NewLine);

					// Setup dependency on UnrealBuildTool, if we need that.  This makes sure that UnrealBuildTool is
					// freshly compiled before kicking off any build operations on this target project
					if (!CurProject.IsStubProject)
					{
						List<ProjectFile> Dependencies = new List<ProjectFile>();
						if (CurProject.IsGeneratedProject && UBTProject != null && CurProject != UBTProject)
						{
							Dependencies.Add(UBTProject);
							Dependencies.AddRange(UBTProject.DependsOnProjects);
						}
						if (bEditorDependsOnShaderCompileWorker && !bUsePrecompiled && CurProject.IsGeneratedProject && ShaderCompileWorkerProject != null && CurProject.ProjectTargets.Any(x => x.TargetRules != null && x.TargetRules.Type == TargetType.Editor))
						{
							Dependencies.Add(ShaderCompileWorkerProject);
						}
						Dependencies.AddRange(CurProject.DependsOnProjects);

						if (Dependencies.Count > 0)
						{
							VCSolutionFileContent.Append("\tProjectSection(ProjectDependencies) = postProject" + ProjectFileGenerator.NewLine);

							// Setup any addition dependencies this project has...
							foreach (ProjectFile DependsOnProject in Dependencies)
							{
								string DependsOnProjectGUID = ((MSBuildProjectFile)DependsOnProject).ProjectGUID.ToString("B").ToUpperInvariant();
								VCSolutionFileContent.Append("\t\t" + DependsOnProjectGUID + " = " + DependsOnProjectGUID + ProjectFileGenerator.NewLine);
							}

							VCSolutionFileContent.Append("\tEndProjectSection" + ProjectFileGenerator.NewLine);
						}
					}

					VCSolutionFileContent.Append(
							"EndProject" + ProjectFileGenerator.NewLine
						);
				}
			}

			// Solution configuration platforms.  This is just a list of all of the platforms and configurations that
			// appear in Visual Studio's build configuration selector.
			List<VCSolutionConfigCombination> SolutionConfigCombinations = new List<VCSolutionConfigCombination>();

			// The "Global" section has source control, solution configurations, project configurations,
			// preferences, and project hierarchy data
			{
				VCSolutionFileContent.Append(
					"Global" + ProjectFileGenerator.NewLine);
				{
					{
						VCSolutionFileContent.Append(
							"	GlobalSection(SolutionConfigurationPlatforms) = preSolution" + ProjectFileGenerator.NewLine);

						Dictionary<string, Tuple<UnrealTargetConfiguration, string>> SolutionConfigurationsValidForProjects = new Dictionary<string, Tuple<UnrealTargetConfiguration, string>>();
						HashSet<UnrealTargetPlatform> PlatformsValidForProjects = new HashSet<UnrealTargetPlatform>();

						foreach (UnrealTargetConfiguration CurConfiguration in SupportedConfigurations)
						{
							if (UnrealBuildTool.IsValidConfiguration(CurConfiguration))
							{
								foreach (UnrealTargetPlatform CurPlatform in SupportedPlatforms)
								{
									if (UnrealBuildTool.IsValidPlatform(CurPlatform))
									{
										foreach (ProjectFile CurProject in AllProjectFiles)
										{
											if (!CurProject.IsStubProject)
											{
												if (CurProject.ProjectTargets.Count == 0)
												{
													throw new BuildException("Expecting project '" + CurProject.ProjectFilePath + "' to have at least one ProjectTarget associated with it!");
												}

												// Figure out the set of valid target configuration names
												foreach (ProjectTarget ProjectTarget in CurProject.ProjectTargets)
												{
													if (VCProjectFile.IsValidProjectPlatformAndConfiguration(ProjectTarget, CurPlatform, CurConfiguration))
													{
														PlatformsValidForProjects.Add(CurPlatform);

														// Default to a target configuration name of "Game", since that will collapse down to an empty string
														string TargetConfigurationName = TargetType.Game.ToString();
														if (ProjectTarget.TargetRules != null)
														{
															TargetConfigurationName = ProjectTarget.TargetRules.Type.ToString();
														}

														string SolutionConfigName = MakeSolutionConfigurationName(CurConfiguration, TargetConfigurationName);
														SolutionConfigurationsValidForProjects[SolutionConfigName] = new Tuple<UnrealTargetConfiguration, string>(CurConfiguration, TargetConfigurationName);
													}
												}
											}
										}
									}
								}
							}
						}

						foreach (UnrealTargetPlatform CurPlatform in PlatformsValidForProjects)
						{
							foreach (KeyValuePair<string, Tuple<UnrealTargetConfiguration, string>> SolutionConfigKeyValue in SolutionConfigurationsValidForProjects)
							{
								// e.g.  "Development|Win64 = Development|Win64"
								string SolutionConfigName = SolutionConfigKeyValue.Key;
								UnrealTargetConfiguration Configuration = SolutionConfigKeyValue.Value.Item1;
								string TargetConfigurationName = SolutionConfigKeyValue.Value.Item2;

								string SolutionPlatformName = CurPlatform.ToString();

								string SolutionConfigAndPlatformPair = SolutionConfigName + "|" + SolutionPlatformName;
								SolutionConfigCombinations.Add(
										new VCSolutionConfigCombination
										{
											VCSolutionConfigAndPlatformName = SolutionConfigAndPlatformPair,
											Configuration = Configuration,
											Platform = CurPlatform,
											TargetConfigurationName = TargetConfigurationName
										}
									);
							}
						}

						// Sort the list of solution platform strings alphabetically (Visual Studio prefers it)
						SolutionConfigCombinations.Sort(
								new Comparison<VCSolutionConfigCombination>(
									(x, y) => { return String.Compare(x.VCSolutionConfigAndPlatformName, y.VCSolutionConfigAndPlatformName, StringComparison.InvariantCultureIgnoreCase); }
								)
							);

						HashSet<string> AppendedSolutionConfigAndPlatformNames = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);
						foreach (VCSolutionConfigCombination SolutionConfigCombination in SolutionConfigCombinations)
						{
							// We alias "Game" and "Program" to both have the same solution configuration, so we're careful not to add the same combination twice.
							if (!AppendedSolutionConfigAndPlatformNames.Contains(SolutionConfigCombination.VCSolutionConfigAndPlatformName))
							{
								VCSolutionFileContent.Append(
									"		" + SolutionConfigCombination.VCSolutionConfigAndPlatformName + " = " + SolutionConfigCombination.VCSolutionConfigAndPlatformName + ProjectFileGenerator.NewLine);
								AppendedSolutionConfigAndPlatformNames.Add(SolutionConfigCombination.VCSolutionConfigAndPlatformName);
							}
						}

						VCSolutionFileContent.Append(
							"	EndGlobalSection" + ProjectFileGenerator.NewLine);
					}


					// Assign each project's "project configuration" to our "solution platform + configuration" pairs.  This
					// also sets up which projects are actually built when building the solution.
					{
						VCSolutionFileContent.Append(
							"	GlobalSection(ProjectConfigurationPlatforms) = postSolution" + ProjectFileGenerator.NewLine);

						List<VCSolutionConfigCombination> CombinationsThatWereMatchedToProjects = new List<VCSolutionConfigCombination>();

						foreach (MSBuildProjectFile CurProject in AllProjectFiles)
						{
							// NOTE: We don't emit solution configuration entries for "stub" projects.  Those projects are only
							// built using UnrealBuildTool and don't require a presence in the solution project list

							// NOTE: We also process projects that were "imported" here, hoping to match those to our solution
							//       configurations.  In some cases this may not be successful though.  Imported projects
							//       should always be carefully setup to match our project generator's solution configs.
							if (!CurProject.IsStubProject)
							{
								if (CurProject.ProjectTargets.Count == 0)
								{
									throw new BuildException("Expecting project '" + CurProject.ProjectFilePath + "' to have at least one ProjectTarget associated with it!");
								}
								bool IsProgramProject = CurProject.ProjectTargets[0].TargetRules != null && CurProject.ProjectTargets[0].TargetRules.Type == TargetType.Program;

								HashSet<string> GameOrProgramConfigsAlreadyMapped = new HashSet<string>();
								foreach (VCSolutionConfigCombination SolutionConfigCombination in SolutionConfigCombinations)
								{
									// Handle aliasing of Program and Game target configuration names
									if ((IsProgramProject && GameOrProgramConfigsAlreadyMapped.Add(SolutionConfigCombination.VCSolutionConfigAndPlatformName)) ||
										IsProgramProject && SolutionConfigCombination.TargetConfigurationName != TargetType.Game.ToString() ||
										!IsProgramProject && SolutionConfigCombination.TargetConfigurationName != TargetType.Program.ToString())
									{
										string TargetConfigurationName = SolutionConfigCombination.TargetConfigurationName;
										if (IsProgramProject)
										{
											TargetConfigurationName = TargetType.Program.ToString();
										}

										// Now, we want to find a target in this project that maps to the current solution config combination.  Only up to one target should
										// and every solution config combination should map to at least one target in one project (otherwise we shouldn't have added it!).
										ProjectTarget MatchingProjectTarget = null;
										foreach (ProjectTarget ProjectTarget in CurProject.ProjectTargets)
										{
											bool IsMatchingCombination = VCProjectFile.IsValidProjectPlatformAndConfiguration(ProjectTarget, SolutionConfigCombination.Platform, SolutionConfigCombination.Configuration);
											if (ProjectTarget.TargetRules != null)
											{
												if (TargetConfigurationName != ProjectTarget.TargetRules.Type.ToString())
												{
													// Solution configuration name for this combination doesn't match this target's configuration name.  It's not buildable.
													IsMatchingCombination = false;
												}
											}
											else
											{
												// UBT gets a pass because it is a dependency of every single configuration combination
												if (CurProject != UBTProject &&
													!CurProject.ShouldBuildForAllSolutionTargets &&
													TargetConfigurationName != TargetType.Game.ToString())
												{
													// Can't build non-generated project in configurations except for the default (Game)
													IsMatchingCombination = false;
												}
											}

											if (IsMatchingCombination)
											{
												if (MatchingProjectTarget != null)
												{
													// Not expecting more than one target to match a single solution configuration per project!
													throw new BuildException("Not expecting more than one target for project " + CurProject.ProjectFilePath + " to match solution configuration " + SolutionConfigCombination.VCSolutionConfigAndPlatformName);
												}

												MatchingProjectTarget = ProjectTarget;

												// NOTE: For faster perf, we could "break" here and bail out early, but that would circumvent the error checking
												//		 for multiple targets within a project that may map to a single solution configuration.
											}
										}

										UnrealTargetConfiguration SolutionConfiguration = SolutionConfigCombination.Configuration;
										UnrealTargetPlatform SolutionPlatform = SolutionConfigCombination.Platform;


										if (MatchingProjectTarget == null)
										{
											// The current configuration/platform and target configuration name doesn't map to anything our project actually supports.
											// We'll map it to a default config.
											SolutionConfiguration = UnrealTargetConfiguration.Development;

											// Prefer using Win64 as the default, but fall back to a platform the project file actually supports if needed.  This is for
											// projects that can never be compiled in Windows, such as UnrealLaunchDaemon which is an iOS-only program
											SolutionPlatform = UnrealTargetPlatform.Win64;
											if (CurProject.ProjectTargets[0].TargetRules != null)
											{
												if (!CurProject.ProjectTargets[0].SupportedPlatforms.Contains(SolutionPlatform))
												{
													SolutionPlatform = CurProject.ProjectTargets[0].SupportedPlatforms[0];
												}
											}


											if (IsProgramProject)
											{
												TargetConfigurationName = TargetType.Program.ToString();
											}
											else
											{
												TargetConfigurationName = TargetType.Game.ToString();
											}
										}


										// If the project wants to always build in "Development", regardless of what the solution
										// configuration is set to, then we'll do that here.  This is used for projects like
										// UnrealBuildTool and ShaderCompileWorker
										if (MatchingProjectTarget != null)
										{
											if (MatchingProjectTarget.ForceDevelopmentConfiguration)
											{
												SolutionConfiguration = UnrealTargetConfiguration.Development;
											}
										}

										// Always allow SCW and UnrealLighmass to build in editor configurations
										if (MatchingProjectTarget == null && SolutionConfigCombination.TargetConfigurationName == TargetType.Editor.ToString() && SolutionConfigCombination.Platform == UnrealTargetPlatform.Win64)
										{
											if (CurProject == ShaderCompileWorkerProject)
											{
												MatchingProjectTarget = ShaderCompileWorkerProject.ProjectTargets[0];
											}
											else if (CurProject == UnrealLightmassProject)
											{
												MatchingProjectTarget = UnrealLightmassProject.ProjectTargets[0];
											}
										}

										string ProjectConfigName;
										string ProjectPlatformName;
										if (CurProject.IsStubProject)
										{
											if (SolutionPlatform != UnrealTargetPlatform.Unknown || SolutionConfiguration != UnrealTargetConfiguration.Unknown)
											{
												throw new BuildException("Stub project was expecting platform and configuration type to be set to Unknown");
											}
											ProjectPlatformName = MSBuildProjectFile.StubProjectPlatformName;
											ProjectConfigName = MSBuildProjectFile.StubProjectConfigurationName;
										}
										else
										{
											CurProject.MakeProjectPlatformAndConfigurationNames(SolutionPlatform, SolutionConfiguration, TargetConfigurationName, out ProjectPlatformName, out ProjectConfigName);
										}

										string ProjectConfigAndPlatformPair = ProjectConfigName.ToString() + "|" + ProjectPlatformName.ToString();

										// e.g.  "{4232C52C-680F-4850-8855-DC39419B5E9B}.Debug|iOS.ActiveCfg = iOS_Debug|Win32"
										string CurProjectGUID = CurProject.ProjectGUID.ToString("B").ToUpperInvariant();
										VCSolutionFileContent.Append(
											"		" + CurProjectGUID + "." + SolutionConfigCombination.VCSolutionConfigAndPlatformName + ".ActiveCfg = " + ProjectConfigAndPlatformPair + ProjectFileGenerator.NewLine);


										// Set whether this project configuration should be built when the user initiates "build solution"
										if (MatchingProjectTarget != null && CurProject.ShouldBuildByDefaultForSolutionTargets)
										{
											// Some targets are "dummy targets"; they only exist to show user friendly errors in VS. Weed them out here, and don't set them to build by default.
											List<UnrealTargetPlatform> SupportedPlatforms = null;
											if (MatchingProjectTarget.TargetRules != null)
											{
												SupportedPlatforms = new List<UnrealTargetPlatform>();
												SupportedPlatforms.AddRange(MatchingProjectTarget.SupportedPlatforms);
											}
											if (SupportedPlatforms == null || SupportedPlatforms.Contains(SolutionConfigCombination.Platform))
											{
												VCSolutionFileContent.Append(
														"		" + CurProjectGUID + "." + SolutionConfigCombination.VCSolutionConfigAndPlatformName + ".Build.0 = " + ProjectConfigAndPlatformPair + ProjectFileGenerator.NewLine);

												UEPlatformProjectGenerator ProjGen = UEPlatformProjectGenerator.GetPlatformProjectGenerator(SolutionConfigCombination.Platform, true);
												if (MatchingProjectTarget.ProjectDeploys ||
													((ProjGen != null) && (ProjGen.GetVisualStudioDeploymentEnabled(SolutionPlatform, SolutionConfiguration) == true)))
												{
													VCSolutionFileContent.Append(
															"		" + CurProjectGUID + "." + SolutionConfigCombination.VCSolutionConfigAndPlatformName + ".Deploy.0 = " + ProjectConfigAndPlatformPair + ProjectFileGenerator.NewLine);
												}
											}
										}

										CombinationsThatWereMatchedToProjects.Add(SolutionConfigCombination);
									}
								}
							}
						}

						// Check for problems
						foreach (VCSolutionConfigCombination SolutionConfigCombination in SolutionConfigCombinations)
						{
							if (!CombinationsThatWereMatchedToProjects.Contains(SolutionConfigCombination))
							{
								throw new BuildException("Unable to find a ProjectTarget that matches the solution configuration/platform mapping: " + SolutionConfigCombination.Configuration.ToString() + ", " + SolutionConfigCombination.Platform.ToString() + ", " + SolutionConfigCombination.TargetConfigurationName);
							}
						}
						VCSolutionFileContent.Append(
							"	EndGlobalSection" + ProjectFileGenerator.NewLine);
					}


					// Setup other solution properties
					{
						VCSolutionFileContent.Append(
							"	GlobalSection(SolutionProperties) = preSolution" + ProjectFileGenerator.NewLine);

						// HideSolutionNode sets whether or not the top-level solution entry is completely hidden in the UI.
						// We don't want that, as we need users to be able to right click on the solution tree item.
						VCSolutionFileContent.Append(
							"		HideSolutionNode = FALSE" + ProjectFileGenerator.NewLine);

						VCSolutionFileContent.Append(
							"	EndGlobalSection" + ProjectFileGenerator.NewLine);
					}



					// Solution directory hierarchy
					{
						VCSolutionFileContent.Append(
							"	GlobalSection(NestedProjects) = preSolution" + ProjectFileGenerator.NewLine);

						// Every entry in this section is in the format "Guid1 = Guid2".  Guid1 is the child project (or solution
						// filter)'s GUID, and Guid2 is the solution filter directory to parent the child project (or solution
						// filter) to.  This sets up the hierarchical solution explorer tree for all solution folders and projects.

						System.Action<StringBuilder /* VCSolutionFileContent */, List<MasterProjectFolder> /* Folders */ > FolderProcessorFunction = null;
						FolderProcessorFunction = (LocalVCSolutionFileContent, LocalMasterProjectFolders) =>
							{
								foreach (VisualStudioSolutionFolder CurFolder in LocalMasterProjectFolders)
								{
									string CurFolderGUIDString = CurFolder.FolderGUID.ToString("B").ToUpperInvariant();

									foreach (MSBuildProjectFile ChildProject in CurFolder.ChildProjects)
									{
										//	e.g. "{BF6FB09F-A2A6-468F-BE6F-DEBE07EAD3EA} = {C43B6BB5-3EF0-4784-B896-4099753BCDA9}"
										LocalVCSolutionFileContent.Append(
											"		" + ChildProject.ProjectGUID.ToString("B").ToUpperInvariant() + " = " + CurFolderGUIDString + ProjectFileGenerator.NewLine);
									}

									foreach (VisualStudioSolutionFolder SubFolder in CurFolder.SubFolders)
									{
										//	e.g. "{BF6FB09F-A2A6-468F-BE6F-DEBE07EAD3EA} = {C43B6BB5-3EF0-4784-B896-4099753BCDA9}"
										LocalVCSolutionFileContent.Append(
											"		" + SubFolder.FolderGUID.ToString("B").ToUpperInvariant() + " = " + CurFolderGUIDString + ProjectFileGenerator.NewLine);
									}

									// Recurse into subfolders
									FolderProcessorFunction(LocalVCSolutionFileContent, CurFolder.SubFolders);
								}
							};
						FolderProcessorFunction(VCSolutionFileContent, RootFolder.SubFolders);

						VCSolutionFileContent.Append(
							"	EndGlobalSection" + ProjectFileGenerator.NewLine);
					}
				}

				VCSolutionFileContent.Append(
					"EndGlobal" + ProjectFileGenerator.NewLine);
			}


			// Save the solution file
			if (bSuccess)
			{
				string SolutionFilePath = FileReference.Combine(MasterProjectPath, SolutionFileName).FullName;
				bSuccess = WriteFileIfChanged(SolutionFilePath, VCSolutionFileContent.ToString());
			}


			// Save a solution config file which selects the development editor configuration by default.
			if (bSuccess)
			{
				// Figure out the filename for the SUO file. VS will automatically import the options from earlier versions if necessary.
				FileReference SolutionOptionsFileName;
				switch (ProjectFileFormat)
                {
                    case VCProjectFileFormat.VisualStudio2012:
						SolutionOptionsFileName = FileReference.Combine(MasterProjectPath, Path.ChangeExtension(SolutionFileName, "v11.suo"));
                        break;
					case VCProjectFileFormat.VisualStudio2013:
						SolutionOptionsFileName = FileReference.Combine(MasterProjectPath, Path.ChangeExtension(SolutionFileName, "v12.suo"));
						break;
					case VCProjectFileFormat.VisualStudio2015:
						SolutionOptionsFileName = FileReference.Combine(MasterProjectPath, ".vs", Path.GetFileNameWithoutExtension(SolutionFileName), "v14", ".suo");
						break;
					case VCProjectFileFormat.VisualStudio2017:
						SolutionOptionsFileName = FileReference.Combine(MasterProjectPath, ".vs", Path.GetFileNameWithoutExtension(SolutionFileName), "v15", ".suo");
						break;
					default:
						throw new BuildException("Unsupported Visual Studio version");
				}

				// Check it doesn't exist before overwriting it. Since these files store the user's preferences, it'd be bad form to overwrite them.
				if (!FileReference.Exists(SolutionOptionsFileName))
				{
					DirectoryReference.CreateDirectory(SolutionOptionsFileName.Directory);

					VCSolutionOptions Options = new VCSolutionOptions();

					// Set the default configuration and startup project
					VCSolutionConfigCombination DefaultConfig = SolutionConfigCombinations.Find(x => x.Configuration == UnrealTargetConfiguration.Development && x.Platform == UnrealTargetPlatform.Win64 && x.TargetConfigurationName == "Editor");
					if (DefaultConfig != null)
					{
						List<VCBinarySetting> Settings = new List<VCBinarySetting>();
						Settings.Add(new VCBinarySetting("ActiveCfg", DefaultConfig.VCSolutionConfigAndPlatformName));
						if (DefaultProject != null)
						{
							Settings.Add(new VCBinarySetting("StartupProject", ((MSBuildProjectFile)DefaultProject).ProjectGUID.ToString("B")));
						}
						Options.SetConfiguration(Settings);
					}

					// Mark all the projects as closed by default, apart from the startup project
					VCSolutionExplorerState ExplorerState = new VCSolutionExplorerState();
					foreach (ProjectFile ProjectFile in AllProjectFiles)
					{
						string ProjectName = ProjectFile.ProjectFilePath.GetFileNameWithoutExtension();
						if (ProjectFile == DefaultProject)
						{
							ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(ProjectName, new string[] { ProjectName }));
						}
						else
						{
							ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(ProjectName, new string[] { }));
						}
					}
					if (IncludeEnginePrograms)
					{
						ExplorerState.OpenProjects.Add(new Tuple<string, string[]>("Automation", new string[0]));
					}
					Options.SetExplorerState(ExplorerState);

					// Write the file
					if (Options.Sections.Count > 0)
					{
						Options.Write(SolutionOptionsFileName.FullName);
					}
				}
			}

			return bSuccess;
		}


		/// <summary>
		/// Takes a string and "cleans it up" to make it parsable by the Visual Studio source control provider's file format
		/// </summary>
		/// <param name="Str">String to clean up</param>
		/// <returns>The cleaned up string</returns>
		public string CleanupStringForSCC(string Str)
		{
			string Cleaned = Str;

			// SCC is expecting paths to contain only double-backslashes for path separators.  It's a bit weird but we need to do it.
			Cleaned = Cleaned.Replace(Path.DirectorySeparatorChar.ToString(), Path.DirectorySeparatorChar.ToString() + Path.DirectorySeparatorChar.ToString());
			Cleaned = Cleaned.Replace(Path.AltDirectorySeparatorChar.ToString(), Path.DirectorySeparatorChar.ToString() + Path.DirectorySeparatorChar.ToString());

			// SCC is expecting not to see spaces in these strings, so we'll replace spaces with "\u0020"
			Cleaned = Cleaned.Replace(" ", "\\u0020");

			return Cleaned;
		}

	}

}
