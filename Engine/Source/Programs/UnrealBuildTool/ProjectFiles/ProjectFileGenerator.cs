// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Reflection;
using Microsoft.Win32;
using System.Linq;
using System.Diagnostics;
using Tools.DotNETCommon;
using System.Xml.Linq;
using System.Xml;

namespace UnrealBuildTool
{
	/// <summary>
	/// Represents a folder within the master project (e.g. Visual Studio solution)
	/// </summary>
	abstract class MasterProjectFolder
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InitOwnerProjectFileGenerator">Project file generator that owns this object</param>
		/// <param name="InitFolderName">Name for this folder</param>
		public MasterProjectFolder( ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName )
		{
			OwnerProjectFileGenerator = InitOwnerProjectFileGenerator;
			FolderName = InitFolderName;
		}

		/// Name of this folder
		public string FolderName
		{
			get;
			private set;
		}


		/// <summary>
		/// Adds a new sub-folder to this folder
		/// </summary>
		/// <param name="SubFolderName">Name of the new folder</param>
		/// <returns>The newly-added folder</returns>
		public MasterProjectFolder AddSubFolder( string SubFolderName )
		{
			MasterProjectFolder ResultFolder = null;

			List<string> FolderNames = SubFolderName.Split(new char[2] { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar }, 2, StringSplitOptions.RemoveEmptyEntries).ToList();
			string FirstFolderName = FolderNames[0];

			bool AlreadyExists = false;
			foreach (MasterProjectFolder ExistingFolder in SubFolders)
			{
				if (ExistingFolder.FolderName.Equals(FirstFolderName, StringComparison.InvariantCultureIgnoreCase))
				{
					// Already exists!
					ResultFolder = ExistingFolder;
					AlreadyExists = true;
					break;
				}
			}

			if (!AlreadyExists)
			{
				ResultFolder = OwnerProjectFileGenerator.AllocateMasterProjectFolder(OwnerProjectFileGenerator, FirstFolderName);
				SubFolders.Add(ResultFolder);
			}

			if (FolderNames.Count > 1)
			{
				ResultFolder = ResultFolder.AddSubFolder(FolderNames[1]);
			}

			return ResultFolder;
		}


		/// <summary>
		/// Recursively searches for the specified project and returns the folder that it lives in, or null if not found
		/// </summary>
		/// <param name="Project">The project file to look for</param>
		/// <returns>The found folder that the project is in, or null</returns>
		public MasterProjectFolder FindFolderForProject( ProjectFile Project )
		{
			foreach( MasterProjectFolder CurFolder in SubFolders )
			{
				MasterProjectFolder FoundFolder = CurFolder.FindFolderForProject( Project );
				if( FoundFolder != null )
				{
					return FoundFolder;
				}
			}

			foreach( ProjectFile ChildProject in ChildProjects )
			{
				if( ChildProject == Project )
				{
					return this;
				}
			}

			return null;
		}

		/// Owner project generator
		readonly ProjectFileGenerator OwnerProjectFileGenerator;

		/// Sub-folders
		public readonly List<MasterProjectFolder> SubFolders = new List<MasterProjectFolder>();

		/// Child projects
		public readonly List<ProjectFile> ChildProjects = new List<ProjectFile>();

		/// Files in this folder.  These are files that aren't part of any project, but display in the IDE under the project folder
		/// and can be browsed/opened by the user easily in the user interface
		public readonly List<string> Files = new List<string>();		
	}

	/// <summary>
	/// The type of project files to generate
	/// </summary>
	enum ProjectFileFormat
	{
		Make,
		CMake,
		QMake,
		KDevelop,
		CodeLite,
		VisualStudio,
		VisualStudio2012,
		VisualStudio2013,
		VisualStudio2015,
		VisualStudio2017,
		XCode,
		Eddie,
		VisualStudioCode,
	}

	/// <summary>
	/// Static class containing 
	/// </summary>
	static class ProjectFileGeneratorSettings
	{
		/// <summary>
		/// Default list of project file formats to generate
		/// </summary>
		[XmlConfigFile(Category = "ProjectFileGenerator", Name = "Format")]
		public static string Format = null;

		/// <summary>
		/// Parses a list of project file formats from a string
		/// </summary>
		/// <param name="Formats"></param>
		/// <returns>Sequence of project file formats</returns>
		public static IEnumerable<ProjectFileFormat> ParseFormatList(string Formats)
		{
			foreach(string FormatName in Formats.Split('+').Select(x => x.Trim()))
			{
				ProjectFileFormat Format;
				if(Enum.TryParse(FormatName, true, out Format))
				{
					yield return Format;
				}
				else
				{
					Log.TraceError("Invalid project file format '{0}'", FormatName);
				}
			}
		}
	}

	/// <summary>
	/// Base class for all project file generators
	/// </summary>
	abstract class ProjectFileGenerator
	{
		/// Global static that enables generation of project files.  Doesn't actually compile anything.
		/// This is enabled only via UnrealBuildTool command-line.
		public static bool bGenerateProjectFiles = false;

		/// True if we're generating lightweight project files for a single game only, excluding most engine code, documentation, etc.
		public bool bGeneratingGameProjectFiles = false;

		/// Optional list of platforms to generate projects for
		readonly List<UnrealTargetPlatform> ProjectPlatforms = new List<UnrealTargetPlatform>();

		/// When bGeneratingGameProjectFiles=true, this is the game name we're generating projects for
		protected string GameProjectName = null;

		/// Global static that only adds platforms that are supported when generating a given target.
		/// This was the old behavior, and it resulted in scenarios where having an unsupported platform selected
		/// in the platform drop-down would silently 'switch' to building Win32.
		/// The new behavior is to add all platforms when generating a target, and then check if it is supported
		/// at build time. If it is not, then a BuildException is thrown informing the user of an unsupported platform.
		/// NOTE: This only matters when using "-AllProjects".  It can increase the project file load times though, because of all
		///       of the extra project configuration combinations we need to store
		public static bool bCreateDummyConfigsForUnsupportedPlatforms = true;

		/// Whether we should include configurations for "Test" and "Shipping" in generated projects (pass "-NoShippingConfigs" to disable this)
		public static bool bIncludeTestAndShippingConfigs = true;

		/// True if intellisense data should be generated (takes a while longer)
		bool bGenerateIntelliSenseData = true;

		/// True if we should include documentation in the generated projects
		[XmlConfigFile]
		bool bIncludeDocumentation = true;

		/// True if all documentation languages should be included in generated projects, otherwise only "INT" will be included
		bool bAllDocumentationLanguages = false;

		/// True if build targets should pass the -useprecompiled argument
		public static bool bUsePrecompiled = false;

		/// True if we should include engine source in the generated solution
		protected bool bIncludeEngineSource = true;

		/// True if shader source files should be included in the generated projects
		bool bIncludeShaderSource = true;

		/// True if build system files should be included
		bool bIncludeBuildSystemFiles = true;

		/// True if we should include config files (.ini files) in the generated project
		bool bIncludeConfigFiles = true;

		/// True if we should include localization files (.int/.kor/etc files) in the generated project
		bool bIncludeLocalizationFiles = false;

        /// True if we should include template files (.template files) in the generated project
        bool bIncludeTemplateFiles = true;

		/// True if we should include program projects in the generated solution
		protected bool IncludeEnginePrograms = true;

		/// True if we should include .NET Core projects in the generated solution
		bool bIncludeDotNETCoreProjects = false;

		/// True if we should reflect "Source" sub-directories on disk in the master project as master project directories.
		/// This arguably adds some visual clutter to the master project, but is truer to the on-disk file organization.
		bool bKeepSourceSubDirectories = true;

		/// Relative path to the directory where the master project file will be saved to
		public static DirectoryReference MasterProjectPath = UnrealBuildTool.RootDirectory; // We'll save the master project to our "root" folder

		/// Name of the UE4 engine project that contains all of the engine code, config files and other files
		public static readonly string EngineProjectFileNameBase = "UE4";

		/// Name of the UE4 enterprise project that contains all of the enterprise code, config files and other files
		public static readonly string EnterpriseProjectFileNameBase = "Studio";

		/// When ProjectsAreIntermediate is true, this is the directory to store generated project files
		// @todo projectfiles: Ideally, projects for game modules/targets would be created in the game's Intermediate folder!
		public static DirectoryReference IntermediateProjectFilesPath = DirectoryReference.Combine( UnrealBuildTool.EngineDirectory, "Intermediate", "ProjectFiles" );

		/// Path to timestamp file, recording when was the last time projects were created.
		public static string ProjectTimestampFile = Path.Combine(IntermediateProjectFilesPath.FullName, "Timestamp");

		/// Global static new line string used by ProjectFileGenerator to generate project files.
		public static readonly string NewLine = Environment.NewLine;

		/// If true, we'll parse subdirectories of third-party projects to locate source and header files to include in the
		/// generated projects.  This can make the generated projects quite a bit bigger, but makes it easier to open files
		/// directly from the IDE.
		bool bGatherThirdPartySource = false;

		/// Indicates whether we should process dot net core based C# projects
		bool AllowDotNetCoreProjects = false;

		/// Name of the master project file (e.g. base file name for the solution file for Visual Studio, or the Xcode project file on Mac)
		protected string MasterProjectName = "UE4";

		/// Maps all module names that were included in generated project files, to actual project file objects.
		/// @todo projectfiles: Nasty global static list.  This is only really used for IntelliSense, and to avoid extra folder searches for projects we've already cached source files for.
		public static readonly Dictionary<string, ProjectFile> ModuleToProjectFileMap = new Dictionary<string, ProjectFile>( StringComparer.InvariantCultureIgnoreCase );

		/// If generating project files for a single project, the path to its .uproject file.
		public readonly FileReference OnlyGameProject;

		/// When generating IntelliSense data, we may want to only generate data for a specific project file, even if other targets make use of modules
		/// in this project file.  This is useful to prevent unusual or hacky global definitions from Programs affecting the Editor/Engine modules.  We
		/// always want the most common and useful definitions to be set when working with solutions with many modules.
		public static ProjectFile OnlyGenerateIntelliSenseDataForProject
		{
			get;
			private set;
		}

	
		/// File extension for project files we'll be generating (e.g. ".vcxproj")
		abstract public string ProjectFileExtension
		{
			get;
		}

		/// True if we should include IntelliSense data in the generated project files when possible
		virtual public bool ShouldGenerateIntelliSenseData()
		{
			return bGenerateIntelliSenseData;
		}

		/// <summary>
		/// Default constructor.
		/// </summary>
		/// <param name="InOnlyGameProject">The project file passed in on the command line</param>
		public ProjectFileGenerator(FileReference InOnlyGameProject)
		{
			AllowDotNetCoreProjects = Environment.CommandLine.Contains("-dotnetcore");
			OnlyGameProject = InOnlyGameProject;
			XmlConfig.ApplyTo(this);
		}

		/// <summary>
		/// Adds all .automation.csproj files to the solution.
		/// </summary>
		void AddAutomationModules(List<FileReference> UnrealProjectFiles, MasterProjectFolder ProgramsFolder)
		{
			MasterProjectFolder Folder = ProgramsFolder.AddSubFolder("Automation");
			List<DirectoryReference> BuildFolders = new List<DirectoryReference>();
			foreach (FileReference UnrealProjectFile in UnrealProjectFiles)
			{
				DirectoryReference GameBuildFolder = DirectoryReference.Combine(UnrealProjectFile.Directory, "Build");
				if (DirectoryReference.Exists(GameBuildFolder))
				{
					BuildFolders.Add(GameBuildFolder);
				}
			}

			// Find all the automation modules .csproj files to add
			List<FileReference> ModuleFiles = RulesCompiler.FindAllRulesSourceFiles(RulesCompiler.RulesFileType.AutomationModule, null, ForeignPlugins:null, AdditionalSearchPaths: BuildFolders );
			foreach (FileReference ProjectFile in ModuleFiles)
			{
				if (FileReference.Exists(ProjectFile))
				{
					VCSharpProjectFile Project = new VCSharpProjectFile(ProjectFile);
					Project.ShouldBuildForAllSolutionTargets = true;
					AddExistingProjectFile(Project, bForceDevelopmentConfiguration: true);
                    AutomationProjectFiles.Add( Project );
					Folder.ChildProjects.Add( Project );
				}
			}
		}

		/// <summary>
		/// Finds all csproj within Engine/Source/Programs, and add them if their UE4CSharp.prog file exists.
		/// </summary>
		void DiscoverCSharpProgramProjects(MasterProjectFolder ProgramsFolder)
		{
			List<FileReference> FoundProjects = new List<FileReference>();
			DirectoryReference EngineProgramsSource = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Source", "Programs");
			DiscoverCSharpProgramProjectsRecursively(EngineProgramsSource, FoundProjects);
			foreach (FileReference FoundProject in FoundProjects)
			{
				VCSharpProjectFile Project = new VCSharpProjectFile(FoundProject);

				if (AllowDotNetCoreProjects || !Project.IsDotNETCoreProject())
				{
					Project.ShouldBuildForAllSolutionTargets = true;
					Project.ShouldBuildByDefaultForSolutionTargets = true;

					AddExistingProjectFile(Project, bForceDevelopmentConfiguration: false);
					ProgramsFolder.ChildProjects.Add(Project);
				}
			}
		}

		private static void DiscoverCSharpProgramProjectsRecursively(DirectoryReference SearchFolder, List<FileReference> FoundProjects)
		{
			// Scan all the files in this directory
			bool bSearchSubFolders = true;
			foreach (FileReference File in DirectoryLookupCache.EnumerateFiles(SearchFolder))
			{
				// If we find a csproj or sln, we should not recurse this directory.
				bool bIsCsProj = File.HasExtension(".csproj");
				bool bIsSln = File.HasExtension(".sln");
				bSearchSubFolders &= !(bIsCsProj || bIsSln);
				// If we found an sln, ignore completely.
				if (bIsSln)
				{
					break;
				}
				// For csproj files, add them to the sln if the UE4CSharp.prog file also exists.
				if (bIsCsProj && FileReference.Exists(FileReference.Combine(SearchFolder, "UE4CSharp.prog")))
				{
					FoundProjects.Add(File);
				}
			}

			// If we didn't find anything to stop the search, search all the subdirectories too
			if (bSearchSubFolders)
			{
				foreach (DirectoryReference SubDirectory in DirectoryLookupCache.EnumerateDirectories(SearchFolder))
				{
					DiscoverCSharpProgramProjectsRecursively(SubDirectory, FoundProjects);
				}
			}
		}

		/// <summary>
		/// Finds the game projects that we're generating project files for
		/// </summary>
		/// <returns>List of project files</returns>
		public List<FileReference> FindGameProjects()
		{
			List<FileReference> ProjectFiles = new List<FileReference>();
			if(bGeneratingGameProjectFiles)
			{
				ProjectFiles.Add(OnlyGameProject);
			}
			else
			{
				ProjectFiles.AddRange(UProjectInfo.AllProjectFiles.Where(x => ShouldGenerateProject(x)));
			}
			return ProjectFiles;
		}

		/// <summary>
		/// Determines if we should generate IDE project files for for the given project
		/// </summary>
		/// <param name="ProjectFile">The project to check</param>
		/// <returns>True to generate a project, false otherwise</returns>
		private static bool ShouldGenerateProject(FileReference ProjectFile)
		{
			DirectoryReference ProjectDirectory = ProjectFile.Directory;
			DirectoryReference SourceFolder = DirectoryReference.Combine(ProjectDirectory, "Source");
			DirectoryReference IntermediateSourceFolder = DirectoryReference.Combine(ProjectDirectory, "Intermediate", "Source");
			return DirectoryReference.Exists(SourceFolder) || DirectoryReference.Exists(IntermediateSourceFolder);
		}

		/// <summary>
		/// Generates a Visual Studio solution file and Visual C++ project files for all known engine and game targets.
		/// Does not actually build anything.
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		public virtual bool GenerateProjectFiles( String[] Arguments )
		{
			bool bSuccess = true;

			// Parse project generator options
			bool IncludeAllPlatforms = true;
			ConfigureProjectFileGeneration( Arguments, ref IncludeAllPlatforms);

			if (bGeneratingGameProjectFiles || UnrealBuildTool.IsEngineInstalled())
			{
				Log.TraceInformation("Discovering modules, targets and source code for project...");

				MasterProjectPath = OnlyGameProject.Directory;
					
				// Set the project file name
				MasterProjectName = OnlyGameProject.GetFileNameWithoutExtension();

				if (!DirectoryReference.Exists(DirectoryReference.Combine(MasterProjectPath, "Source")))
				{
					if (!DirectoryReference.Exists(DirectoryReference.Combine(MasterProjectPath, "Intermediate", "Source")))
					{
						if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
						{
							MasterProjectPath = UnrealBuildTool.EngineDirectory;
							GameProjectName = "UE4Game";
						}
						if (!DirectoryReference.Exists(DirectoryReference.Combine(MasterProjectPath, "Source")))
						{
							throw new BuildException("Directory '{0}' is missing 'Source' folder.", MasterProjectPath);
						}
					}
				}
				IntermediateProjectFilesPath = DirectoryReference.Combine(MasterProjectPath, "Intermediate", "ProjectFiles");
			}

			// Modify the name if specific platforms were given
			if (ProjectPlatforms.Count > 0)
			{
				// Sort the platforms names so we get consistent names
				List<string> SortedPlatformNames = new List<string>();
				foreach (UnrealTargetPlatform SpecificPlatform in ProjectPlatforms)
				{
					SortedPlatformNames.Add(SpecificPlatform.ToString());
				}
				SortedPlatformNames.Sort();

				MasterProjectName += "_";
				foreach (string SortedPlatform in SortedPlatformNames)
				{
					MasterProjectName += SortedPlatform;
					IntermediateProjectFilesPath = new DirectoryReference(IntermediateProjectFilesPath.FullName + SortedPlatform);
				}
			}

            // Optionally include the folder name in the project
            if (Environment.GetEnvironmentVariable("UE_NAME_PROJECT_AFTER_FOLDER") == "1")
            {
                MasterProjectName += "_" + Path.GetFileName(MasterProjectPath.ToString());
            }

			bool bCleanProjectFiles = Arguments.Any(x => x.Equals("-CleanProjects", StringComparison.InvariantCultureIgnoreCase));
			if (bCleanProjectFiles)
			{
				CleanProjectFiles(MasterProjectPath, MasterProjectName, IntermediateProjectFilesPath);
			}

			// Figure out which platforms we should generate project files for.
			string SupportedPlatformNames;
			SetupSupportedPlatformsAndConfigurations( IncludeAllPlatforms:IncludeAllPlatforms, SupportedPlatformNames:out SupportedPlatformNames );

			Log.TraceVerbose( "Detected supported platforms: " + SupportedPlatformNames );

			RootFolder = AllocateMasterProjectFolder( this, "<Root>" );

			// Build the list of games to generate projects for
			List<FileReference> AllGameProjects = FindGameProjects();

			// Find all of the module files.  This will filter out any modules or targets that don't belong to platforms
			// we're generating project files for.
			List<FileReference> AllModuleFiles = DiscoverModules(AllGameProjects);

			ProjectFile EngineProject = null;
			ProjectFile EnterpriseProject = null;
			Dictionary<DirectoryReference, ProjectFile> GameProjects = null;
			Dictionary<DirectoryReference, ProjectFile> ModProjects = null;
			Dictionary<FileReference, ProjectFile> ProgramProjects = null;
			Dictionary<DirectoryReference, ProjectFile> TemplateGameProjects = null;
			Dictionary<DirectoryReference, ProjectFile> SampleGameProjects = null;
			{
				// Setup buildable projects for all targets
				AddProjectsForAllTargets( AllGameProjects, out EngineProject, out EnterpriseProject, out GameProjects, out ModProjects, out ProgramProjects, out TemplateGameProjects, out SampleGameProjects );

				// Add all game projects and game config files
				AddAllGameProjects(GameProjects, SupportedPlatformNames, RootFolder);

				// Set the game to be the default project
				if(ModProjects.Count > 0)
				{
					DefaultProject = ModProjects.Values.First();
				}
				else if(bGeneratingGameProjectFiles && GameProjects.Count > 0)
				{
					DefaultProject = GameProjects.Values.First();
				}

				//Related Debug Project Files - Tuple here has the related Debug Project, SolutionFolder
				List<Tuple<ProjectFile, string>> DebugProjectFiles = new List<Tuple<ProjectFile, string>>();

				// Place projects into root level solution folders
				if ( bIncludeEngineSource )
				{
					// If we're still missing an engine project because we don't have any targets for it, make one up.
					if( EngineProject == null )
					{
						FileReference ProjectFilePath = FileReference.Combine(IntermediateProjectFilesPath, EngineProjectFileNameBase + ProjectFileExtension);

						bool bAlreadyExisted;
						EngineProject = FindOrAddProject(ProjectFilePath, true, out bAlreadyExisted);

						EngineProject.IsForeignProject = false;
						EngineProject.IsGeneratedProject = true;
						EngineProject.IsStubProject = true;
					}

					if( EngineProject != null )
					{
						RootFolder.AddSubFolder( "Engine" ).ChildProjects.Add( EngineProject );

						// Engine config files
						if( bIncludeConfigFiles )
						{
							AddEngineConfigFiles( EngineProject );
							if( IncludeEnginePrograms )
							{
								AddUnrealHeaderToolConfigFiles(EngineProject);
								AddUBTConfigFilesToEngineProject(EngineProject);
							}
						}

						// Engine Extras files
						AddEngineExtrasFiles(EngineProject);

						// Engine localization files
						if( bIncludeLocalizationFiles )
						{
							AddEngineLocalizationFiles( EngineProject );
						}

						// Engine template files
						if (bIncludeTemplateFiles)
						{
							AddEngineTemplateFiles( EngineProject );
						}

						if( bIncludeShaderSource )
						{
							Log.TraceVerbose( "Adding shader source code..." );

							// Find shader source files and generate stub project
							AddEngineShaderSource( EngineProject );
						}

						if( bIncludeBuildSystemFiles )
						{
							Log.TraceVerbose( "Adding build system files..." );

							AddEngineBuildFiles( EngineProject );
						}

						if( bIncludeDocumentation )
						{
							AddEngineDocumentation( EngineProject );
						}

						List<Tuple<ProjectFile, string>> NewProjectFiles = EngineProject.WriteDebugProjectFiles(InPlatforms: SupportedPlatforms, InConfigurations: SupportedConfigurations);

						if (NewProjectFiles != null)
						{
							DebugProjectFiles.AddRange(NewProjectFiles);
						}
					}

					if (EnterpriseProject != null)
					{
						RootFolder.AddSubFolder(UnrealBuildTool.EnterpriseDirectory.GetDirectoryName()).ChildProjects.Add(EnterpriseProject);
					}

					foreach( ProjectFile CurModProject in ModProjects.Values )
					{
						RootFolder.AddSubFolder("Mods").ChildProjects.Add(CurModProject);
					}

					foreach( ProjectFile CurGameProject in GameProjects.Values )
					{
						// Templates go under a different solution folder than games
						DirectoryReference TemplateGameDirectory = TemplateGameProjects.FirstOrDefault( FolderAndProject => FolderAndProject.Value == CurGameProject ).Key;
						DirectoryReference SampleGameDirectory = SampleGameProjects.FirstOrDefault( FolderAndProject => FolderAndProject.Value == CurGameProject ).Key;

						if( TemplateGameDirectory != null )
						{
							if( TemplateGameDirectory.IsUnderDirectory((UnrealBuildTool.EnterpriseDirectory)) )
							{
								RootFolder.AddSubFolder(UnrealBuildTool.EnterpriseDirectory.GetDirectoryName() + Path.DirectorySeparatorChar + "Templates").ChildProjects.Add(CurGameProject);
							}
							else
							{
								RootFolder.AddSubFolder("Templates").ChildProjects.Add(CurGameProject);
							}
						}
						else if( SampleGameDirectory != null )
						{
							if( SampleGameDirectory.IsUnderDirectory((UnrealBuildTool.EnterpriseDirectory)) )
                            {
								RootFolder.AddSubFolder(UnrealBuildTool.EnterpriseDirectory.GetDirectoryName() + Path.DirectorySeparatorChar + "Samples").ChildProjects.Add(CurGameProject);
							}
							else
							{
								RootFolder.AddSubFolder("Samples").ChildProjects.Add(CurGameProject);
							}
						}
						else
						{
							RootFolder.AddSubFolder( "Games" ).ChildProjects.Add( CurGameProject );
						}

						List<Tuple<ProjectFile, string>> NewProjectFiles = CurGameProject.WriteDebugProjectFiles(InPlatforms: SupportedPlatforms, InConfigurations: SupportedConfigurations);

						if (NewProjectFiles != null)
						{
							DebugProjectFiles.AddRange(NewProjectFiles);
						}

					}

					//Related Debug Project Files - Tuple has the related Debug Project, SolutionFolder
					foreach (Tuple<ProjectFile, string> DebugProjectFile in DebugProjectFiles)
					{						
						AddExistingProjectFile(DebugProjectFile.Item1, bForceDevelopmentConfiguration: false);

						//add it to the Android Debug Projects folder in the solution
						RootFolder.AddSubFolder(DebugProjectFile.Item2).ChildProjects.Add(DebugProjectFile.Item1);						
					}
					

					foreach( KeyValuePair<FileReference, ProjectFile> CurProgramProject in ProgramProjects )
					{
                        ProjectTarget Target = CurProgramProject.Value.ProjectTargets.FirstOrDefault(t => !String.IsNullOrEmpty(t.TargetRules.SolutionDirectory));

                        if (Target != null)
                        {
                            RootFolder.AddSubFolder(Target.TargetRules.SolutionDirectory).ChildProjects.Add(CurProgramProject.Value);
                        }
                        else
                        {
							if (CurProgramProject.Key.IsUnderDirectory(UnrealBuildTool.EnterpriseDirectory))
							{
								RootFolder.AddSubFolder(UnrealBuildTool.EnterpriseDirectory.GetDirectoryName() + Path.DirectorySeparatorChar + "Programs").ChildProjects.Add(CurProgramProject.Value);
							}
							else
							{
								RootFolder.AddSubFolder( "Programs" ).ChildProjects.Add( CurProgramProject.Value );
							}
                        }
					}

					// Add all of the config files for generated program targets
					AddEngineProgramConfigFiles( ProgramProjects );
				}
			}

			// Setup "stub" projects for all modules
			AddProjectsForAllModules(AllGameProjects, ProgramProjects, ModProjects, AllModuleFiles, bGatherThirdPartySource);

			{
				if( IncludeEnginePrograms )
				{
					MasterProjectFolder ProgramsFolder = RootFolder.AddSubFolder( "Programs" );

					// Add UnrealBuildTool to the master project
					AddUnrealBuildToolProject( ProgramsFolder );

					// Add AutomationTool to the master project
					ProgramsFolder.ChildProjects.Add(AddSimpleCSharpProject("AutomationTool", bShouldBuildForAllSolutionTargets: true, bForceDevelopmentConfiguration: true));

					// Add automation.csproj files to the master project
					AddAutomationModules(AllGameProjects, ProgramsFolder);

					// Discover C# programs which should additionally be included in the solution.
					DiscoverCSharpProgramProjects(ProgramsFolder);
                }


				// Eliminate all redundant master project folders.  E.g., folders which contain only one project and that project
				// has the same name as the folder itself.  To the user, projects "feel like" folders already in the IDE, so we
				// want to collapse them down where possible.
				EliminateRedundantMasterProjectSubFolders( RootFolder, "" );

				// Figure out which targets we need about IntelliSense for.  We only need to worry about targets for projects
				// that we're actually generating in this session.
				List<Tuple<ProjectFile, ProjectTarget>> IntelliSenseTargetFiles = new List<Tuple<ProjectFile, ProjectTarget>>();
				{
					// Engine targets
					if( EngineProject != null)
					{
						foreach( ProjectTarget ProjectTarget in EngineProject.ProjectTargets )
						{
							if( ProjectTarget.TargetFilePath != null )
							{
								// Only bother with the editor target.  We want to make sure that definitions are setup to be as inclusive as possible
								// for good quality IntelliSense.  For example, we want WITH_EDITORONLY_DATA=1, so using the editor targets works well.
								if( ProjectTarget.TargetRules.Type == TargetType.Editor )
								{ 
									IntelliSenseTargetFiles.Add( Tuple.Create(EngineProject, ProjectTarget) );
								}
							}
						}
					}

					// Enterprise targets
					if (EnterpriseProject != null)
					{
						foreach (ProjectTarget ProjectTarget in EnterpriseProject.ProjectTargets)
						{
							if (ProjectTarget.TargetFilePath != null)
							{
								// Only bother with the editor target.  We want to make sure that definitions are setup to be as inclusive as possible
								// for good quality IntelliSense.  For example, we want WITH_EDITORONLY_DATA=1, so using the editor targets works well.
								if (ProjectTarget.TargetRules.Type == TargetType.Editor)
								{
									IntelliSenseTargetFiles.Add(Tuple.Create(EnterpriseProject, ProjectTarget));
								}
							}
						}
					}

					// Program targets
					foreach( ProjectFile ProgramProject in ProgramProjects.Values )
					{
						foreach( ProjectTarget ProjectTarget in ProgramProject.ProjectTargets )
						{
							if( ProjectTarget.TargetFilePath != null )
							{
								IntelliSenseTargetFiles.Add( Tuple.Create( ProgramProject, ProjectTarget ) );
							}
						}
					}

					// Game/template targets
					foreach( ProjectFile GameProject in GameProjects.Values )
					{
						foreach( ProjectTarget ProjectTarget in GameProject.ProjectTargets )
						{
							if( ProjectTarget.TargetFilePath != null )
							{
								// Only bother with the editor target.  We want to make sure that definitions are setup to be as inclusive as possible
								// for good quality IntelliSense.  For example, we want WITH_EDITORONLY_DATA=1, so using the editor targets works well.
								if( ProjectTarget.TargetRules.Type == TargetType.Editor )
								{ 
									IntelliSenseTargetFiles.Add( Tuple.Create( GameProject, ProjectTarget ) );
								}
							}
						}
					}
				}

				// Generate IntelliSense data if we need to.  This involves having UBT simulate the action compilation of
				// the targets so that we can extra the compiler defines, include paths, etc.
				if(GenerateIntelliSenseData(Arguments, IntelliSenseTargetFiles))
				{
					WriteProjectFiles();
					Log.TraceVerbose( "Project generation complete ({0} generated, {1} imported)", GeneratedProjectFiles.Count, OtherProjectFiles.Count );
				}
			}

			return bSuccess;
		}

		/// <summary>
		/// Adds detected UBT configuration files (BuildConfiguration.xml) to engine project.
		/// </summary>
		/// <param name="EngineProject">Engine project to add files to.</param>
		private void AddUBTConfigFilesToEngineProject(ProjectFile EngineProject)
		{
			EngineProject.AddAliasedFileToProject(new AliasedFile(
					XmlConfig.GetSchemaLocation().FullName,
					Path.Combine("Programs", "UnrealBuildTool")
				));

			List<XmlConfig.InputFile> InputFiles = XmlConfig.FindInputFiles();
			foreach(XmlConfig.InputFile InputFile in InputFiles)
			{
				EngineProject.AddAliasedFileToProject(
						new AliasedFile(
							InputFile.Location.FullName,
							Path.Combine("Config", "UnrealBuildTool", InputFile.FolderName)
						)
					);
			}
		}

		/// <summary>
		/// Clean project files
		/// </summary>
		/// <param name="InMasterProjectDirectory">The master project directory</param>
		/// <param name="InMasterProjectName">The name of the master project</param>
		/// <param name="InIntermediateProjectFilesDirectory">The intermediate path of project files</param>
		public abstract void CleanProjectFiles(DirectoryReference InMasterProjectDirectory, string InMasterProjectName, DirectoryReference InIntermediateProjectFilesDirectory);

		/// <summary>
		/// Configures project generator based on command-line options
		/// </summary>
		/// <param name="Arguments">Arguments passed into the program</param>
		/// <param name="IncludeAllPlatforms">True if all platforms should be included</param>
		protected virtual void ConfigureProjectFileGeneration( String[] Arguments, ref bool IncludeAllPlatforms )
		{
			bool bAlwaysIncludeEngineModules = false;
			foreach( string CurArgument in Arguments )
			{
				if( CurArgument.StartsWith( "-" ) )
				{
					if (CurArgument.StartsWith( "-Platforms=", StringComparison.InvariantCultureIgnoreCase ))
					{
						// Parse the list... will be in Foo+Bar+New format
						string PlatformList = CurArgument.Substring(11);
						while (PlatformList.Length > 0)
						{
							string PlatformString = PlatformList;
							Int32 PlusIdx = PlatformList.IndexOf("+");
							if (PlusIdx != -1)
							{
								PlatformString = PlatformList.Substring(0, PlusIdx);
								PlatformList = PlatformList.Substring(PlusIdx + 1);
							}
							else
							{
								// We are on the last platform... clear the list to exit the loop
								PlatformList = "";
							}

							// Is the string a valid platform? If so, add it to the list
							UnrealTargetPlatform SpecifiedPlatform = UnrealTargetPlatform.Unknown;
							foreach (UnrealTargetPlatform PlatformParam in Enum.GetValues(typeof(UnrealTargetPlatform)))
							{
								if (PlatformString.Equals(PlatformParam.ToString(), StringComparison.InvariantCultureIgnoreCase))
								{
									SpecifiedPlatform = PlatformParam;
									break;
								}
							}

							if (SpecifiedPlatform != UnrealTargetPlatform.Unknown)
							{
								if (ProjectPlatforms.Contains(SpecifiedPlatform) == false)
								{
									ProjectPlatforms.Add(SpecifiedPlatform);
								}
							}
							else
							{
								Log.TraceWarning("ProjectFiles invalid platform specified: {0}", PlatformString);
							}
						}
					}
					else switch( CurArgument.ToUpperInvariant() )
					{
						case "-ALLPLATFORMS":
							IncludeAllPlatforms = true;
							break;

						case "-CURRENTPLATFORM":
							IncludeAllPlatforms = false;
							break;

						case "-THIRDPARTY":
							bGatherThirdPartySource = true;
							break;
						
						case "-GAME":
							// Generates project files for a single game
							bGeneratingGameProjectFiles = true;
							break;

						case "-ENGINE":
							// Forces engine modules and targets to be included in game-specific project files
							bAlwaysIncludeEngineModules = true;
							break;

						case "-NOINTELLISENSE":
							bGenerateIntelliSenseData = false;
							break;

						case "-INTELLISENSE":
							bGenerateIntelliSenseData = true;
							break;

						case "-SHIPPINGCONFIGS":
							bIncludeTestAndShippingConfigs = true;
							break;

						case "-NOSHIPPINGCONFIGS":
							bIncludeTestAndShippingConfigs = false;
							break;

						case "-DUMMYCONFIGS":
							bCreateDummyConfigsForUnsupportedPlatforms = true;
							break;

						case "-NODUMMYCONFIGS":
							bCreateDummyConfigsForUnsupportedPlatforms = false;
							break;

						case "-ALLLANGUAGES":
							bAllDocumentationLanguages = true;
							break;

						case "-USEPRECOMPILED":
							bUsePrecompiled = true;
							break;

						case "-VSCODE":
							bIncludeDotNETCoreProjects = true;
							break;
					}
				}
			}

			if( bGeneratingGameProjectFiles || UnrealBuildTool.IsEngineInstalled() )
			{
				if (OnlyGameProject == null)
				{
					throw new BuildException("A game project path was not specified, which is required when generating project files using an installed build or passing -game on the command line");
				}

				GameProjectName = OnlyGameProject.GetFileNameWithoutExtension();
				if (String.IsNullOrEmpty(GameProjectName))
				{
					throw new BuildException("A valid game project was not found in the specified location (" + OnlyGameProject.Directory.FullName + ")");
				}

				bool bInstalledEngineWithSource = UnrealBuildTool.IsEngineInstalled() && DirectoryReference.Exists(UnrealBuildTool.EngineSourceDirectory);

				bIncludeEngineSource = bAlwaysIncludeEngineModules || bInstalledEngineWithSource;
				bIncludeDocumentation = false;
				bIncludeBuildSystemFiles = false;
				bIncludeShaderSource = true;
				bIncludeTemplateFiles = false;
				bIncludeConfigFiles = true;
				IncludeEnginePrograms = bAlwaysIncludeEngineModules;
			}
			else
			{
				// At least one extra argument was specified, but we weren't expected it.  Ignored.
			}
		}


		/// <summary>
		/// Adds all game project files, including target projects and config files
		/// </summary>
		private void AddAllGameProjects(Dictionary<DirectoryReference, ProjectFile> GameProjects, string SupportedPlatformNames, MasterProjectFolder ProjectsFolder)
		{
			foreach( KeyValuePair<DirectoryReference, ProjectFile> GameFolderAndProjectFile in GameProjects )
			{
				DirectoryReference GameProjectDirectory = GameFolderAndProjectFile.Key;

				// @todo projectfiles: We have engine localization files, but should we also add GAME localization files?

				// Game config files
				if( bIncludeConfigFiles )
				{
					DirectoryReference GameConfigDirectory = DirectoryReference.Combine(GameProjectDirectory, "Config");
					if( DirectoryReference.Exists(GameConfigDirectory) )
					{
						ProjectFile GameProjectFile = GameFolderAndProjectFile.Value;
						GameProjectFile.AddFilesToProject( SourceFileSearch.FindFiles( GameConfigDirectory ), GameProjectDirectory );
					}
				}

				// Game build files
				if( bIncludeBuildSystemFiles )
				{
					var GameBuildDirectory = DirectoryReference.Combine(GameProjectDirectory, "Build");
					if( DirectoryReference.Exists(GameBuildDirectory) )
					{
						var SubdirectoryNamesToExclude = new List<string>();
						SubdirectoryNamesToExclude.Add("Receipts");
						SubdirectoryNamesToExclude.Add("Scripts");

						var GameProjectFile = GameFolderAndProjectFile.Value;
						GameProjectFile.AddFilesToProject( SourceFileSearch.FindFiles( GameBuildDirectory, SubdirectoryNamesToExclude ), GameProjectDirectory );
					}
				}

                DirectoryReference GameShaderDirectory = DirectoryReference.Combine(GameProjectDirectory, "Shaders");
                if (DirectoryReference.Exists(GameShaderDirectory))
                {
                    ProjectFile GameProjectFile = GameFolderAndProjectFile.Value;
                    GameProjectFile.AddFilesToProject(SourceFileSearch.FindFiles(GameShaderDirectory), GameProjectDirectory);
                }
            }
		}


		/// Adds all engine localization text files to the specified project
		private void AddEngineLocalizationFiles( ProjectFile EngineProject )
		{
			DirectoryReference EngineLocalizationDirectory = DirectoryReference.Combine( UnrealBuildTool.EngineDirectory, "Content", "Localization" );
			if( DirectoryReference.Exists(EngineLocalizationDirectory) )
			{
				EngineProject.AddFilesToProject( SourceFileSearch.FindFiles( EngineLocalizationDirectory ), UnrealBuildTool.EngineDirectory );
			}
		}


        /// Adds all engine template text files to the specified project
        private void AddEngineTemplateFiles( ProjectFile EngineProject )
        {
            DirectoryReference EngineTemplateDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Content", "Editor", "Templates");
            if (DirectoryReference.Exists(EngineTemplateDirectory))
            {
				EngineProject.AddFilesToProject( SourceFileSearch.FindFiles( EngineTemplateDirectory ), UnrealBuildTool.EngineDirectory );
			}
        }


		/// Adds all engine config files to the specified project
		private void AddEngineConfigFiles( ProjectFile EngineProject )
		{
            DirectoryReference EngineConfigDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Config" );
			if( DirectoryReference.Exists(EngineConfigDirectory) )
			{
				EngineProject.AddFilesToProject( SourceFileSearch.FindFiles( EngineConfigDirectory ), UnrealBuildTool.EngineDirectory );
			}
		}

		/// Adds all engine extras files to the specified project
		protected virtual void AddEngineExtrasFiles(ProjectFile EngineProject)
		{
		}

		/// Adds UnrealHeaderTool config files to the specified project
		private void AddUnrealHeaderToolConfigFiles(ProjectFile EngineProject)
		{
			DirectoryReference UHTConfigDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Programs", "UnrealHeaderTool", "Config");
			if (DirectoryReference.Exists(UHTConfigDirectory))
			{
				EngineProject.AddFilesToProject(SourceFileSearch.FindFiles(UHTConfigDirectory), UnrealBuildTool.EngineDirectory);
			}
		}

        /// <summary>
        /// Finds any additional plugin files.
        /// </summary>
        /// <returns>List of additional plugin files</returns>
		private List<FileReference> DiscoverExtraPlugins(List<FileReference> AllGameProjects)
		{
			List<FileReference> AddedPlugins = new List<FileReference>();

            foreach (FileReference GameProject in AllGameProjects)
			{
                // Check the user preference to see if they'd like to include nativized assets as a generated project.
                bool bIncludeNativizedAssets = false;
                ConfigHierarchy Config = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, GameProject.Directory, BuildHostPlatform.Current.Platform);
                if (Config != null)
                {
                    Config.TryGetValue("/Script/UnrealEd.ProjectPackagingSettings", "bIncludeNativizedAssetsInProjectGeneration", out bIncludeNativizedAssets);
                }

                // Note: Whether or not we include nativized assets here has no bearing on whether or not they actually get built.
                if (bIncludeNativizedAssets)
                {
                    AddedPlugins.AddRange(Plugins.EnumeratePlugins(DirectoryReference.Combine(GameProject.Directory, "Intermediate", "Plugins")).Where(x => x.GetFileNameWithoutExtension() == "NativizedAssets"));
                }
			}
			return AddedPlugins;
		}

		/// <summary>
		/// Finds all module files (filtering by platform)
		/// </summary>
		/// <returns>Filtered list of module files</returns>
		protected List<FileReference> DiscoverModules(List<FileReference> AllGameProjects)
		{
			List<FileReference> AllModuleFiles = new List<FileReference>();

			// Locate all modules (*.Build.cs files)
			List<FileReference> FoundModuleFiles = RulesCompiler.FindAllRulesSourceFiles( RulesCompiler.RulesFileType.Module, GameFolders: AllGameProjects.Select(x => x.Directory).ToList(), ForeignPlugins: DiscoverExtraPlugins(AllGameProjects), AdditionalSearchPaths:null );
			foreach( FileReference BuildFileName in FoundModuleFiles )
			{
				AllModuleFiles.Add( BuildFileName );
			}
			return AllModuleFiles;
		}

		/// <summary>
		/// List of non-redistributable folders
		/// </summary>
		private static string[] NoRedistFolders = new string[]
		{
			Path.DirectorySeparatorChar + "NoRedist" + Path.DirectorySeparatorChar,
			Path.DirectorySeparatorChar + "NotForLicensees" + Path.DirectorySeparatorChar
		};

		/// <summary>
		/// Checks if a module is in a non-redistributable folder
		/// </summary>
		private static bool IsNoRedistModule(FileReference ModulePath)
		{
			foreach (string NoRedistFolderName in NoRedistFolders)
			{
				if (ModulePath.FullName.IndexOf(NoRedistFolderName, StringComparison.InvariantCultureIgnoreCase) >= 0)
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Finds all target files (filtering by platform)
		/// </summary>
		/// <returns>Filtered list of target files</returns>
		protected List<FileReference> DiscoverTargets(List<FileReference> AllGameProjects)
		{
			List<FileReference> AllTargetFiles = new List<FileReference>();

			// Make a list of all platform name strings that we're *not* including in the project files
			List<string> UnsupportedPlatformNameStrings = Utils.MakeListOfUnsupportedPlatforms( SupportedPlatforms );

			// Locate all targets (*.Target.cs files)
			List<FileReference> FoundTargetFiles = RulesCompiler.FindAllRulesSourceFiles( RulesCompiler.RulesFileType.Target, AllGameProjects.Select(x => x.Directory).ToList(), ForeignPlugins: DiscoverExtraPlugins(AllGameProjects), AdditionalSearchPaths:null );
			foreach( FileReference CurTargetFile in FoundTargetFiles )
			{
				string CleanTargetFileName = Utils.CleanDirectorySeparators( CurTargetFile.FullName );

				// remove the local root
				string LocalRoot = UnrealBuildTool.RootDirectory.FullName;
				string Search = CleanTargetFileName;
				if (Search.StartsWith(LocalRoot, StringComparison.InvariantCultureIgnoreCase))
				{
					if (LocalRoot.EndsWith("\\") || LocalRoot.EndsWith("/"))
					{
						Search = Search.Substring(LocalRoot.Length - 1);
					}
					else
					{
						Search = Search.Substring(LocalRoot.Length);
					}
				}

				if (OnlyGameProject != null)
				{
					string ProjectRoot = OnlyGameProject.Directory.FullName;
					if (Search.StartsWith(ProjectRoot, StringComparison.InvariantCultureIgnoreCase))
					{
						if (ProjectRoot.EndsWith("\\") || ProjectRoot.EndsWith("/"))
						{
							Search = Search.Substring(ProjectRoot.Length - 1);
						}
						else
						{
							Search = Search.Substring(ProjectRoot.Length);
						}
					}
				}

				// Skip targets in unsupported platform directories
				bool IncludeThisTarget = true;
				foreach( string CurPlatformName in UnsupportedPlatformNameStrings )
				{
					if (Search.IndexOf(Path.DirectorySeparatorChar + CurPlatformName + Path.DirectorySeparatorChar, StringComparison.InvariantCultureIgnoreCase) != -1)
					{
						IncludeThisTarget = false;
						break;
					}
				}

				if( IncludeThisTarget )
				{
					AllTargetFiles.Add( CurTargetFile );
				}
			}

			return AllTargetFiles;
		}


	
		/// <summary>
		/// Recursively collapses all sub-folders that are redundant.  Should only be called after we're done adding
		/// files and projects to the master project.
		/// </summary>
		/// <param name="Folder">The folder whose sub-folders we should potentially collapse into</param>
		/// <param name="ParentMasterProjectFolderPath"></param>
		void EliminateRedundantMasterProjectSubFolders( MasterProjectFolder Folder, string ParentMasterProjectFolderPath )
		{
			// NOTE: This is for diagnostics output only
			string MasterProjectFolderPath = String.IsNullOrEmpty( ParentMasterProjectFolderPath ) ? Folder.FolderName : ( ParentMasterProjectFolderPath + "/" + Folder.FolderName );

			// We can eliminate folders that meet all of these requirements:
			//		1) Have only a single project file in them
			//		2) Have no files in the folder except project files, and no sub-folders
			//		3) The project file matches the folder name
			//
			// Additionally, if KeepSourceSubDirectories==false, we can eliminate directories called "Source".
			//
			// Also, we can kill folders that are completely empty.
			

			foreach( MasterProjectFolder SubFolder in Folder.SubFolders )
			{
				// Recurse
				EliminateRedundantMasterProjectSubFolders( SubFolder, MasterProjectFolderPath );
			}

			List<MasterProjectFolder> SubFoldersToAdd = new List<MasterProjectFolder>();
			List<MasterProjectFolder> SubFoldersToRemove = new List<MasterProjectFolder>();
			foreach( MasterProjectFolder SubFolder in Folder.SubFolders )
			{
				bool CanCollapseFolder = false;

				// 1)
				if( SubFolder.ChildProjects.Count == 1 )
				{
					// 2)
					if( SubFolder.Files.Count == 0 &&
						SubFolder.SubFolders.Count == 0 )
					{
						// 3)
						if (SubFolder.FolderName.Equals(SubFolder.ChildProjects[0].ProjectFilePath.GetFileNameWithoutAnyExtensions(), StringComparison.InvariantCultureIgnoreCase))
						{
							CanCollapseFolder = true;
						}
					}
				}

				if( !bKeepSourceSubDirectories )
				{
					if( SubFolder.FolderName.Equals( "Source", StringComparison.InvariantCultureIgnoreCase ) )
					{
						// Avoid collapsing the Engine's Source directory, since there are so many other solution folders in
						// the parent directory.
						if( !Folder.FolderName.Equals( "Engine", StringComparison.InvariantCultureIgnoreCase ) )
						{
							CanCollapseFolder = true;
						}
					}
				}

				if( SubFolder.ChildProjects.Count == 0 && SubFolder.Files.Count == 0 && SubFolder.SubFolders.Count == 0 )
				{
					// Folder is totally empty
					CanCollapseFolder = true;
				}

				if( CanCollapseFolder )
				{
					// OK, this folder is redundant and can be collapsed away.

					SubFoldersToAdd.AddRange( SubFolder.SubFolders );
					SubFolder.SubFolders.Clear();

					Folder.ChildProjects.AddRange( SubFolder.ChildProjects );
					SubFolder.ChildProjects.Clear();

					Folder.Files.AddRange( SubFolder.Files );
					SubFolder.Files.Clear();

					SubFoldersToRemove.Add( SubFolder );
				}
			}

			foreach( MasterProjectFolder SubFolderToRemove in SubFoldersToRemove )
			{
				Folder.SubFolders.Remove( SubFolderToRemove );
			}
			Folder.SubFolders.AddRange( SubFoldersToAdd );

			// After everything has been collapsed, do a bit of data validation
			Validate(Folder, ParentMasterProjectFolderPath);
		}

		/// <summary>
		/// Validate the specified Folder. Default implementation requires
		/// for project file names to be unique!
		/// </summary>
		/// <param name="Folder">Folder.</param>
		/// <param name="MasterProjectFolderPath">Parent master project folder path.</param>
		protected virtual void Validate(MasterProjectFolder Folder, string MasterProjectFolderPath)
		{
			foreach (ProjectFile CurChildProject in Folder.ChildProjects)
			{
				foreach (ProjectFile OtherChildProject in Folder.ChildProjects)
				{
					if (CurChildProject != OtherChildProject)
					{
						if (CurChildProject.ProjectFilePath.GetFileNameWithoutAnyExtensions().Equals(OtherChildProject.ProjectFilePath.GetFileNameWithoutAnyExtensions(), StringComparison.InvariantCultureIgnoreCase))
						{
							throw new BuildException("Detected collision between two project files with the same path in the same master project folder, " + OtherChildProject.ProjectFilePath.FullName + " and " + CurChildProject.ProjectFilePath.FullName + " (master project folder: " + MasterProjectFolderPath + ")");
						}
					}
				}
			}

			foreach (MasterProjectFolder SubFolder in Folder.SubFolders)
			{
				// If the parent folder already has a child project or file item with the same name as this sub-folder, then
				// that's considered an error (it should never have been allowed to have a folder name that collided
				// with project file names or file items, as that's not supported in Visual Studio.)
				foreach (ProjectFile CurChildProject in Folder.ChildProjects)
				{
					if (CurChildProject.ProjectFilePath.GetFileNameWithoutAnyExtensions().Equals(SubFolder.FolderName, StringComparison.InvariantCultureIgnoreCase))
					{
						throw new BuildException("Detected collision between a master project sub-folder " + SubFolder.FolderName + " and a project within the outer folder " + CurChildProject.ProjectFilePath + " (master project folder: " + MasterProjectFolderPath + ")");
					}
				}
				foreach (string CurFile in Folder.Files)
				{
					if (Path.GetFileName(CurFile).Equals(SubFolder.FolderName, StringComparison.InvariantCultureIgnoreCase))
					{
						throw new BuildException("Detected collision between a master project sub-folder " + SubFolder.FolderName + " and a file within the outer folder " + CurFile + " (master project folder: " + MasterProjectFolderPath + ")");
					}
				}
				foreach (MasterProjectFolder CurFolder in Folder.SubFolders)
				{
					if (CurFolder != SubFolder)
					{
						if (CurFolder.FolderName.Equals(SubFolder.FolderName, StringComparison.InvariantCultureIgnoreCase))
						{
							throw new BuildException("Detected collision between a master project sub-folder " + SubFolder.FolderName + " and a sibling folder " + CurFolder.FolderName + " (master project folder: " + MasterProjectFolderPath + ")");
						}
					}
				}
			}
		}

		/// <summary>
		/// Adds UnrealBuildTool to the master project
		/// </summary>
		private void AddUnrealBuildToolProject(MasterProjectFolder ProgramsFolder)
		{
			List<string> ProjectDirectoryNames = new List<string>();
			ProjectDirectoryNames.Add("UnrealBuildTool");

			if (AllowDotNetCoreProjects)
			{
				ProjectDirectoryNames.Add("UnrealBuildTool_NETCore");
			}

			foreach (string ProjectDirectoryName in ProjectDirectoryNames)
			{
				DirectoryReference ProjectDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Programs", ProjectDirectoryName);
				if (DirectoryReference.Exists(ProjectDirectory))
				{
					FileReference ProjectFileName = FileReference.Combine(ProjectDirectory, "UnrealBuildTool.csproj");

					if (FileReference.Exists(ProjectFileName))
					{
						VCSharpProjectFile UnrealBuildToolProject = new VCSharpProjectFile(ProjectFileName);
						UnrealBuildToolProject.ShouldBuildForAllSolutionTargets = true;

						if (bIncludeDotNETCoreProjects || !UnrealBuildToolProject.IsDotNETCoreProject())
						{
							if (!UnrealBuildToolProject.IsDotNETCoreProject())
							{
								// Store it off as we need it when generating target projects.
								UBTProject = UnrealBuildToolProject;
							}

							// Add the project
							AddExistingProjectFile(UnrealBuildToolProject, bNeedsAllPlatformAndConfigurations: true, bForceDevelopmentConfiguration: true);

							// Put this in a solution folder
							ProgramsFolder.ChildProjects.Add(UnrealBuildToolProject);
						}
					}
				}
			}
		}

		/// <summary>
		/// Adds a C# project to the master project
		/// </summary>
		/// <param name="ProjectName">Name of project file to add</param>
		/// <param name="bShouldBuildForAllSolutionTargets"></param>
		/// <param name="bForceDevelopmentConfiguration"></param>
		/// <param name="bShouldBuildByDefaultForSolutionTargets"></param>
		/// <returns>ProjectFile if the operation was successful, otherwise null.</returns>
		private VCSharpProjectFile AddSimpleCSharpProject(string ProjectName, bool bShouldBuildForAllSolutionTargets = false, bool bForceDevelopmentConfiguration = false, bool bShouldBuildByDefaultForSolutionTargets = true)
		{
			VCSharpProjectFile Project = null;

			FileReference ProjectFileName = FileReference.Combine( UnrealBuildTool.EngineSourceDirectory, "Programs", ProjectName, Path.GetFileName( ProjectName ) + ".csproj" );
			FileInfo Info = new FileInfo( ProjectFileName.FullName );
			if( Info.Exists )
			{
				Project = new VCSharpProjectFile(ProjectFileName);
				Project.ShouldBuildForAllSolutionTargets = bShouldBuildForAllSolutionTargets;
				Project.ShouldBuildByDefaultForSolutionTargets = bShouldBuildByDefaultForSolutionTargets;
				AddExistingProjectFile(Project, bForceDevelopmentConfiguration: bForceDevelopmentConfiguration);
			}
			else
			{
				throw new BuildException( ProjectFileName.FullName + " doesn't exist!" );
			}

			return Project;
		}

		/// <summary>
		/// Check the registry for MVC3 project support
		/// </summary>
		/// <param name="RootKey"></param>
		/// <param name="VisualStudioVersion"></param>
		/// <returns></returns>
		private bool CheckRegistryKey( RegistryKey RootKey, string VisualStudioVersion )
		{
			bool bInstalled = false;
			RegistryKey VSSubKey = RootKey.OpenSubKey( "SOFTWARE\\Microsoft\\VisualStudio\\" + VisualStudioVersion + "\\Projects\\{E53F8FEA-EAE0-44A6-8774-FFD645390401}" );
			if( VSSubKey != null )
			{
				bInstalled = true;
				VSSubKey.Close();
			}

			return bInstalled;
		}

		/// <summary>
		/// Check to see if a Visual Studio Extension is installed
		/// </summary>
		/// <param name="VisualStudioFolder"></param>
		/// <param name="VisualStudioVersion"></param>
		/// <param name="Extension"></param>
		/// <returns></returns>
		private bool CheckVisualStudioExtensionPackage( string VisualStudioFolder, string VisualStudioVersion, string Extension )
		{
			DirectoryInfo DirInfo = new DirectoryInfo( Path.Combine( VisualStudioFolder, VisualStudioVersion, "Extensions" ) );
			if( DirInfo.Exists )
			{
				List<FileInfo> PackageDefs = DirInfo.GetFiles( "*.pkgdef", SearchOption.AllDirectories ).ToList();
				List<string> PackageDefNames = PackageDefs.Select( x => x.Name ).ToList();
				if( PackageDefNames.Contains( Extension ) )
				{
					return true;
				}
			}

			return false;
		}

		/// <summary>
		/// Adds all of the config files for program targets to their project files
		/// </summary>
		private void AddEngineProgramConfigFiles( Dictionary<FileReference, ProjectFile> ProgramProjects )
		{
			if( bIncludeConfigFiles )
			{
				foreach( KeyValuePair<FileReference, ProjectFile> FileAndProject in ProgramProjects )
				{
					string ProgramName = FileAndProject.Key.GetFileNameWithoutAnyExtensions();
					ProjectFile ProgramProjectFile = FileAndProject.Value;

					// @todo projectfiles: The config folder for programs is kind of weird -- you end up going UP a few directories to get to it.  This stuff is not great.
					// @todo projectfiles: Fragile assumption here about Programs always being under /Engine/Programs

					DirectoryReference ProgramDirectory;

                    if ( FileAndProject.Key.IsUnderDirectory(UnrealBuildTool.EnterpriseDirectory) )
					{
						ProgramDirectory = DirectoryReference.Combine( UnrealBuildTool.EnterpriseDirectory, "Programs", ProgramName );
					}
					else
					{
						ProgramDirectory = DirectoryReference.Combine( UnrealBuildTool.EngineDirectory, "Programs", ProgramName );
					}

					DirectoryReference ProgramConfigDirectory = DirectoryReference.Combine( ProgramDirectory, "Config" );
					if( DirectoryReference.Exists(ProgramConfigDirectory) )
					{
						ProgramProjectFile.AddFilesToProject( SourceFileSearch.FindFiles( ProgramConfigDirectory ), ProgramDirectory );
					}
				}
			}
		}

		/// <summary>
		/// Generates data for IntelliSense (compile definitions, include paths)
		/// </summary>
		/// <param name="Arguments">Incoming command-line arguments to UBT</param>
		/// <param name="Targets">Targets to build for intellisense</param>
		/// <return>Whether the process was successful or not</return>
		private bool GenerateIntelliSenseData( String[] Arguments, List<Tuple<ProjectFile, ProjectTarget>> Targets )
		{
			bool bSuccess = true;
			if( ShouldGenerateIntelliSenseData() && Targets.Count > 0 )
			{
				BuildConfiguration BuildConfiguration = new BuildConfiguration();

				string ProgressInfoText = Utils.IsRunningOnMono ? "Generating data for project indexing..." : "Binding IntelliSense data...";
				using(ProgressWriter Progress = new ProgressWriter(ProgressInfoText, true))
				{
					for(int TargetIndex = 0; TargetIndex < Targets.Count; ++TargetIndex)
					{
						ProjectFile TargetProjectFile = Targets[ TargetIndex ].Item1;
						ProjectTarget CurTarget = Targets[ TargetIndex ].Item2;

						Log.TraceVerbose( "Found target: " + CurTarget.TargetFilePath.GetFileNameWithoutAnyExtensions() );

						string[] NewArguments = new string[ Arguments.Length + 4 ];
						NewArguments[ 0 ] = CurTarget.TargetFilePath.GetFileNameWithoutAnyExtensions();
						NewArguments[ 1 ] = BuildHostPlatform.Current.Platform.ToString();
						NewArguments[ 2 ] = UnrealTargetConfiguration.Development.ToString();
						NewArguments[ 3 ] = "-precompile";
						Array.Copy(Arguments, 0, NewArguments, 4, Arguments.Length);

						// We only want to update definitions and include paths for modules that are part of this target's project file.
						ProjectFileGenerator.OnlyGenerateIntelliSenseDataForProject = TargetProjectFile;

						// Clear the CachedIncludePaths field on every FileItem. It's faster to keep the cache of file items around, but we need to wipe
						// out any state.
						FileItem.ClearCachedIncludePaths();

						// Run UnrealBuildTool, pretending to build this target but instead only gathering data for IntelliSense (include paths and definitions).
						// No actual compiling or linking will happen because we early out using the ProjectFileGenerator.bGenerateProjectFiles global
						bSuccess = UnrealBuildTool.RunUBT( BuildConfiguration, NewArguments, CurTarget.UnrealProjectFilePath ) == ECompilationResult.Succeeded;
						ProjectFileGenerator.OnlyGenerateIntelliSenseDataForProject = null;

						if( !bSuccess )
						{
							break;
						}

						// Display progress
						Progress.Write(TargetIndex + 1, Targets.Count);
					}
				}
			}

			return bSuccess;
		}


		/// <summary>
		/// Selects which platforms and build configurations we want in the project file
		/// </summary>
		/// <param name="IncludeAllPlatforms">True if we should include ALL platforms that are supported on this machine.  Otherwise, only desktop platforms will be included.</param>
		/// <param name="SupportedPlatformNames">Output string for supported platforms, returned as comma-separated values.</param>
		protected virtual void SetupSupportedPlatformsAndConfigurations(bool IncludeAllPlatforms, out string SupportedPlatformNames)
		{
			StringBuilder SupportedPlatformsString = new StringBuilder();

			System.Array PlatformEnums = Enum.GetValues(typeof(UnrealTargetPlatform));
			foreach (UnrealTargetPlatform Platform in PlatformEnums)
			{
				// project is in the explicit platform list or we include them all, we add the valid desktop platforms as they are required
				bool bInProjectPlatformsList = (ProjectPlatforms.Count > 0) ? (IsValidDesktopPlatform(Platform) || ProjectPlatforms.Contains(Platform)) : true;

				// project is a desktop platform or we have specified some platforms explicitly
				bool IsRequiredPlatform = (IsValidDesktopPlatform(Platform) || ProjectPlatforms.Count > 0);

				// Only include desktop platforms unless we were explicitly asked to include all platforms or restricted to a single platform.
				if (bInProjectPlatformsList && (IncludeAllPlatforms || IsRequiredPlatform))
				{
					// If there is a build platform present, add it to the SupportedPlatforms list
					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform( Platform, true );
					if( BuildPlatform != null )
					{
						if (UnrealBuildTool.IsValidPlatform(Platform))
						{
							SupportedPlatforms.Add(Platform);

							if (SupportedPlatformsString.Length > 0)
							{
								SupportedPlatformsString.Append(", ");
							}
							SupportedPlatformsString.Append(Platform.ToString());
						}
					}
				}
			}

			// Add all configurations
			foreach( UnrealTargetConfiguration CurConfiguration in Enum.GetValues( typeof(UnrealTargetConfiguration) ) )
			{
				if( CurConfiguration != UnrealTargetConfiguration.Unknown )
				{
					if (UnrealBuildTool.IsValidConfiguration(CurConfiguration))
					{
						SupportedConfigurations.Add(CurConfiguration);
					}
				}
			}

			SupportedPlatformNames = SupportedPlatformsString.ToString();
		}

		/// <summary>
		/// Is this a valid platform. Used primarily for Installed vs non-Installed builds.
		/// </summary>
		/// <param name="InPlatform"></param>
		/// <returns>true if valid, false if not</returns>
		static public bool IsValidDesktopPlatform(UnrealTargetPlatform InPlatform)
		{
			switch (BuildHostPlatform.Current.Platform)
			{
				case UnrealTargetPlatform.Linux:
					return InPlatform == UnrealTargetPlatform.Linux;
				case UnrealTargetPlatform.Mac:
					return InPlatform == UnrealTargetPlatform.Mac;
				case UnrealTargetPlatform.Win64:
					return ((InPlatform == UnrealTargetPlatform.Win32) || (InPlatform == UnrealTargetPlatform.Win64));
				default:
					throw new BuildException("Invalid RuntimePlatform:" + BuildHostPlatform.Current.Platform);
			}
		}

		/// <summary>
		/// Find the game which contains a given input file.
		/// </summary>
		/// <param name="AllGames">All game folders</param>
		/// <param name="File">Full path of the file to search for</param>
		protected FileReference FindGameContainingFile(List<FileReference> AllGames, FileReference File)
		{
			foreach (FileReference Game in AllGames)
			{
				if (File.IsUnderDirectory(Game.Directory))
				{
					return Game;
				}
			}
			return null;
		}

		/// <summary>
		/// Finds all modules and code files, given a list of games to process
		/// </summary>
		/// <param name="AllGames">All game folders</param>
		/// <param name="ProgramProjects">All program projects</param>
		/// <param name="ModProjects">All mod projects</param>
		/// <param name="AllModuleFiles">List of *.Build.cs files for all engine programs and games</param>
		/// <param name="bGatherThirdPartySource">True to gather source code from third party projects too</param>
		protected void AddProjectsForAllModules( List<FileReference> AllGames, Dictionary<FileReference, ProjectFile> ProgramProjects, Dictionary<DirectoryReference, ProjectFile> ModProjects, List<FileReference> AllModuleFiles, bool bGatherThirdPartySource )
		{
			HashSet<ProjectFile> ProjectsWithPlugins = new HashSet<ProjectFile>();
			foreach( FileReference CurModuleFile in AllModuleFiles )
			{
				Log.TraceVerbose("AddProjectsForAllModules " + CurModuleFile);

				// The module's "base directory" is simply the directory where its xxx.Build.cs file is stored.  We'll always
				// harvest source files for this module in this base directory directory and all of its sub-directories.
				string ModuleName = CurModuleFile.GetFileNameWithoutAnyExtensions();		// Remove both ".cs" and ".Build"

				bool WantProjectFileForModule = true;

				// We'll keep track of whether this is an "engine" or "external" module.  This is determined below while loading module rules.
				bool IsEngineModule = UnrealBuildTool.IsUnderAnEngineDirectory(CurModuleFile.Directory);
				bool IsThirdPartyModule = CurModuleFile.IsUnderDirectory(UnrealBuildTool.EngineSourceThirdPartyDirectory);

				if( IsEngineModule && !bIncludeEngineSource )
				{
					// We were asked to exclude engine modules from the generated projects
					WantProjectFileForModule = false;
				}

				if( WantProjectFileForModule )
				{
					DirectoryReference BaseFolder;
					ProjectFile ProjectFile = FindProjectForModule(CurModuleFile, AllGames, ProgramProjects, ModProjects, out BaseFolder);

					// Update our module map
					ModuleToProjectFileMap[ ModuleName ] = ProjectFile;
					ProjectFile.IsGeneratedProject = true;

					// Only search subdirectories for non-external modules.  We don't want to add all of the source and header files
					// for every third-party module, unless we were configured to do so.
					bool SearchSubdirectories = !IsThirdPartyModule || bGatherThirdPartySource;

					if( bGatherThirdPartySource )
					{
						Log.TraceInformation( "Searching for third-party source files..." );
					}


					// Find all of the source files (and other files) and add them to the project
					List<FileReference> FoundFiles = SourceFileSearch.FindModuleSourceFiles( CurModuleFile, SearchSubdirectories:SearchSubdirectories );
					ProjectFile.AddFilesToProject( FoundFiles, BaseFolder );

					// Check if there's a plugin directory here
					if(!ProjectsWithPlugins.Contains(ProjectFile))
					{
						DirectoryReference PluginFolder = DirectoryReference.Combine(BaseFolder, "Plugins");
						if(DirectoryReference.Exists(PluginFolder))
						{
							// Add all the plugin files for this project
							foreach(FileReference PluginFileName in Plugins.EnumeratePlugins(PluginFolder))
							{
								if(!ModProjects.ContainsKey(PluginFileName.Directory))
								{
									AddPluginFilesToProject(PluginFileName, BaseFolder, ProjectFile);
								}
							}
						}
						ProjectsWithPlugins.Add(ProjectFile);
					}
				}
			}
		}

		private void AddPluginFilesToProject(FileReference PluginFileName, DirectoryReference BaseFolder, ProjectFile ProjectFile)
		{
			// Add the .uplugin file
			ProjectFile.AddFileToProject(PluginFileName, BaseFolder);

			// Add plugin config files if we have any
			if( bIncludeConfigFiles )
			{
				DirectoryReference PluginConfigFolder = DirectoryReference.Combine(PluginFileName.Directory, "Config");
				if(DirectoryReference.Exists(PluginConfigFolder))
				{
					ProjectFile.AddFilesToProject(SourceFileSearch.FindFiles(PluginConfigFolder), BaseFolder );
				}
			}

			// Add plugin "resource" files if we have any
			DirectoryReference PluginResourcesFolder = DirectoryReference.Combine(PluginFileName.Directory, "Resources");
			if(DirectoryReference.Exists(PluginResourcesFolder))
			{
				ProjectFile.AddFilesToProject(SourceFileSearch.FindFiles(PluginResourcesFolder), BaseFolder );
            }

            // Add plugin shader files if we have any
            DirectoryReference PluginShadersFolder = DirectoryReference.Combine(PluginFileName.Directory, "Shaders");
            if (DirectoryReference.Exists(PluginShadersFolder))
            {
                ProjectFile.AddFilesToProject(SourceFileSearch.FindFiles(PluginShadersFolder), BaseFolder);
            }
        }

		private ProjectFile FindProjectForModule(FileReference CurModuleFile, List<FileReference> AllGames, Dictionary<FileReference, ProjectFile> ProgramProjects, Dictionary<DirectoryReference, ProjectFile> ModProjects, out DirectoryReference BaseFolder)
		{
			string ProjectFileNameBase = null;

			string PossibleProgramTargetName = CurModuleFile.GetFileNameWithoutAnyExtensions();

			// @todo projectfiles: This works fine for now, but is pretty busted.  It assumes only one module per program and that it matches the program target file name. (see TTP 307091)
			if( ProgramProjects != null && ProgramProjects.Any( ProgramProject => PossibleProgramTargetName.Equals( ProgramProject.Key.GetFileNameWithoutAnyExtensions() ) ) )	// @todo projectfiles: When building (in mem projects), ProgramProjects will be null so we are using the UE4 project instead
			{
				ProjectFileNameBase = PossibleProgramTargetName;
				BaseFolder = CurModuleFile.Directory;
			}
			else if( CurModuleFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory) )
			{
				ProjectFileNameBase = EngineProjectFileNameBase;
				BaseFolder = UnrealBuildTool.EngineDirectory;
			}
			else if( CurModuleFile.IsUnderDirectory(UnrealBuildTool.EnterpriseSourceDirectory) ||
				CurModuleFile.IsUnderDirectory(DirectoryReference.Combine(UnrealBuildTool.EnterpriseDirectory, "Plugins")) )
			{
				ProjectFileNameBase = EnterpriseProjectFileNameBase;
				BaseFolder = UnrealBuildTool.EnterpriseDirectory;
			}
			else
			{
				// Check if it's a mod
				foreach(KeyValuePair<DirectoryReference, ProjectFile> ModProject in ModProjects)
				{
					if(CurModuleFile.IsUnderDirectory(ModProject.Key))
					{
						BaseFolder = ModProject.Key;
						return ModProject.Value;
					}
				}

				// Figure out which game project this target belongs to
				FileReference ProjectInfo = FindGameContainingFile(AllGames, CurModuleFile);
				if(ProjectInfo == null)
				{
					throw new BuildException( "Found a non-engine module file (" + CurModuleFile + ") that did not exist within any of the known game folders" );
				}
				BaseFolder = ProjectInfo.Directory;
				ProjectFileNameBase = ProjectInfo.GetFileNameWithoutExtension();
			}

			// Setup a project file entry for this module's project.  Remember, some projects may host multiple modules!
			FileReference ProjectFileName = FileReference.Combine( IntermediateProjectFilesPath, ProjectFileNameBase + ProjectFileExtension );
			bool bProjectAlreadyExisted;
			return FindOrAddProject( ProjectFileName, IncludeInGeneratedProjects:true, bAlreadyExisted:out bProjectAlreadyExisted );
		}

		/// <summary>
		/// Creates project entries for all known targets (*.Target.cs files)
		/// </summary>
		/// <param name="AllGames">All game folders</param>
		/// <param name="EngineProject">The engine project we created</param>
		/// <param name="EnterpriseProject">The enterprise project we created</param>
		/// <param name="GameProjects">Map of game folder name to all of the game projects we created</param>
		/// <param name="ModProjects">Map of mod folder name to all the mod projects we created</param>
		/// <param name="ProgramProjects">Map of program names to all of the program projects we created</param>
		/// <param name="TemplateGameProjects">Set of template game projects we found.  These will also be in the GameProjects map</param>
		/// <param name="SampleGameProjects">Set of sample game projects that were found</param>
		private void AddProjectsForAllTargets( List<FileReference> AllGames, out ProjectFile EngineProject, out ProjectFile EnterpriseProject,
			out Dictionary<DirectoryReference, ProjectFile> GameProjects, out Dictionary<DirectoryReference, ProjectFile> ModProjects,
			out Dictionary<FileReference, ProjectFile> ProgramProjects, out Dictionary<DirectoryReference, ProjectFile> TemplateGameProjects, out Dictionary<DirectoryReference, ProjectFile> SampleGameProjects )
		{
			// As we're creating project files, we'll also keep track of whether we created an "engine" project and return that if we have one
			EngineProject = null;
			EnterpriseProject = null;
			GameProjects = new Dictionary<DirectoryReference, ProjectFile>();
			ProgramProjects = new Dictionary<FileReference, ProjectFile>();
			TemplateGameProjects = new Dictionary<DirectoryReference, ProjectFile>();
			SampleGameProjects = new Dictionary<DirectoryReference, ProjectFile>();

			// Get some standard directories
			DirectoryReference EngineSourceProgramsDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Programs");
			DirectoryReference TemplatesDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "..", "Templates");
			DirectoryReference SamplesDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "..", "Samples");
			DirectoryReference EnterpriseSourceProgramsDirectory = DirectoryReference.Combine(UnrealBuildTool.EnterpriseSourceDirectory, "Programs");
			DirectoryReference EnterpriseTemplatesDirectory = DirectoryReference.Combine(UnrealBuildTool.EnterpriseDirectory, "Templates");
			DirectoryReference EnterpriseSamplesDirectory = DirectoryReference.Combine(UnrealBuildTool.EnterpriseDirectory, "Samples");

			// Find all of the target files.  This will filter out any modules or targets that don't
			// belong to platforms we're generating project files for.
			List<FileReference> AllTargetFiles = DiscoverTargets(AllGames);
			foreach( FileReference TargetFilePath in AllTargetFiles )
			{
				string TargetName = TargetFilePath.GetFileNameWithoutAnyExtensions();		// Remove both ".cs" and ".Target"

				// Check to see if this is an Engine target.  That is, the target is located under the "Engine" folder
				bool IsEngineTarget = false;
				bool IsEnterpriseTarget = false;
				bool WantProjectFileForTarget = true;
				if(TargetFilePath.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
				{
					// This is an engine target
					IsEngineTarget = true;

					if(TargetFilePath.IsUnderDirectory(EngineSourceProgramsDirectory))
					{
						WantProjectFileForTarget = IncludeEnginePrograms;
					}
					else if(TargetFilePath.IsUnderDirectory(UnrealBuildTool.EngineSourceDirectory))
					{
						WantProjectFileForTarget = bIncludeEngineSource;
					}
				}
				else if (TargetFilePath.IsUnderDirectory(UnrealBuildTool.EnterpriseSourceDirectory))
				{
					// This is an enterprise target
					IsEnterpriseTarget = true;

					if(TargetFilePath.IsUnderDirectory(EnterpriseSourceProgramsDirectory))
					{
						WantProjectFileForTarget = IncludeEnginePrograms;
					}
					else if(TargetFilePath.IsUnderDirectory(UnrealBuildTool.EnterpriseSourceDirectory))
					{
						WantProjectFileForTarget = bIncludeEngineSource;
					}
				}

				if (WantProjectFileForTarget)
				{
					RulesAssembly RulesAssembly;

					FileReference CheckProjectFile;
					if(!UProjectInfo.TryGetProjectForTarget(TargetName, out CheckProjectFile))
					{
						if(TargetFilePath.IsUnderDirectory(UnrealBuildTool.EnterpriseDirectory))
						{
							RulesAssembly = RulesCompiler.CreateEnterpriseRulesAssembly();
						}
						else
						{
							RulesAssembly = RulesCompiler.CreateEngineRulesAssembly();
						}
					}
					else
					{
						RulesAssembly = RulesCompiler.CreateProjectRulesAssembly(CheckProjectFile);
					}

					// Create target rules for all of the platforms and configuration combinations that we want to enable support for.
					// Just use the current platform as we only need to recover the target type and both should be supported for all targets...
					TargetRules TargetRulesObject = RulesAssembly.CreateTargetRules(TargetName, BuildHostPlatform.Current.Platform, UnrealTargetConfiguration.Development, "", CheckProjectFile, false);

					bool IsProgramTarget = false;

					DirectoryReference GameFolder = null;
					string ProjectFileNameBase = null;
					if (TargetRulesObject.Type == TargetType.Program)
					{
						IsProgramTarget = true;
						ProjectFileNameBase = TargetName;
					}
					else if (IsEngineTarget)
					{
						ProjectFileNameBase = EngineProjectFileNameBase;
					}
					else if (IsEnterpriseTarget)
					{
						ProjectFileNameBase = EnterpriseProjectFileNameBase;
					}
					else
					{
						// Figure out which game project this target belongs to
						FileReference ProjectInfo = FindGameContainingFile(AllGames, TargetFilePath);
						if (ProjectInfo == null)
						{
							throw new BuildException("Found a non-engine target file (" + TargetFilePath + ") that did not exist within any of the known game folders");
						}
						GameFolder = ProjectInfo.Directory;
						ProjectFileNameBase = ProjectInfo.GetFileNameWithoutExtension();
					}

					// @todo projectfiles: We should move all of the Target.cs files out of sub-folders to clean up the project directories a bit (e.g. GameUncooked folder)

					FileReference ProjectFilePath = FileReference.Combine(IntermediateProjectFilesPath, ProjectFileNameBase + ProjectFileExtension);

					if (TargetRulesObject.Type == TargetType.Game || TargetRulesObject.Type == TargetType.Client || TargetRulesObject.Type == TargetType.Server)
					{
						// Allow platforms to generate stub projects here...
						UEPlatformProjectGenerator.GenerateGameProjectStubs(
							InGenerator: this,
							InTargetName: TargetName,
							InTargetFilepath: TargetFilePath.FullName,
							InTargetRules: TargetRulesObject,
							InPlatforms: SupportedPlatforms,
							InConfigurations: SupportedConfigurations);
					}

					bool bProjectAlreadyExisted;
					ProjectFile ProjectFile = FindOrAddProject(ProjectFilePath, IncludeInGeneratedProjects: true, bAlreadyExisted: out bProjectAlreadyExisted);
					ProjectFile.IsForeignProject = bGeneratingGameProjectFiles && OnlyGameProject != null && TargetFilePath.IsUnderDirectory(OnlyGameProject.Directory);
					ProjectFile.IsGeneratedProject = true;
					ProjectFile.IsStubProject = UnrealBuildTool.IsProjectInstalled();
					if (TargetRulesObject.bBuildInSolutionByDefault.HasValue)
					{
						ProjectFile.ShouldBuildByDefaultForSolutionTargets = TargetRulesObject.bBuildInSolutionByDefault.Value;
					}

					// Check to see if this is a template target.  That is, the target is located under the "Templates" folder
					bool IsTemplateTarget = TargetFilePath.IsUnderDirectory(TemplatesDirectory) || TargetFilePath.IsUnderDirectory(EnterpriseTemplatesDirectory);
					bool IsSampleTarget = TargetFilePath.IsUnderDirectory(SamplesDirectory) || TargetFilePath.IsUnderDirectory(EnterpriseSamplesDirectory);

					DirectoryReference BaseFolder = null;
					if (IsProgramTarget)
					{
						ProgramProjects[TargetFilePath] = ProjectFile;
						BaseFolder = TargetFilePath.Directory;
					}
					else if (IsEngineTarget)
					{
						EngineProject = ProjectFile;
						BaseFolder = UnrealBuildTool.EngineDirectory;
						if (UnrealBuildTool.IsEngineInstalled())
						{
							// Allow engine projects to be created but not built for Installed Engine builds
							EngineProject.IsForeignProject = false;
							EngineProject.IsGeneratedProject = true;
							EngineProject.IsStubProject = true;
						}
					}
					else if (IsEnterpriseTarget)
					{
						EnterpriseProject = ProjectFile;
						BaseFolder = UnrealBuildTool.EnterpriseDirectory;
						if (UnrealBuildTool.IsEngineInstalled())
						{
							// Allow enterprise projects to be created but not built for Installed Engine builds
							EnterpriseProject.IsForeignProject = false;
							EnterpriseProject.IsGeneratedProject = true;
							EnterpriseProject.IsStubProject = true;
						}
					}
					else
					{
						GameProjects[GameFolder] = ProjectFile;
						if (IsTemplateTarget)
						{
							TemplateGameProjects[GameFolder] = ProjectFile;
						}
						else if (IsSampleTarget)
						{
							SampleGameProjects[GameFolder] = ProjectFile;
						}
						BaseFolder = GameFolder;

						if (!bProjectAlreadyExisted)
						{
							// Add the .uproject file for this game/template
							FileReference UProjectFilePath = FileReference.Combine(BaseFolder, ProjectFileNameBase + ".uproject");
							if (FileReference.Exists(UProjectFilePath))
							{
								ProjectFile.AddFileToProject(UProjectFilePath, BaseFolder);
							}
							else
							{
								throw new BuildException("Not expecting to find a game with no .uproject file.  File '{0}' doesn't exist", UProjectFilePath);
							}
						}

					}

					foreach (ProjectTarget ExistingProjectTarget in ProjectFile.ProjectTargets)
					{
						if (ExistingProjectTarget.TargetRules.Type == TargetRulesObject.Type)
						{
							throw new BuildException("Not expecting project {0} to already have a target rules of with configuration name {1} ({2}) while trying to add: {3}", ProjectFilePath, TargetRulesObject.Type.ToString(), ExistingProjectTarget.TargetRules.ToString(), TargetRulesObject.ToString());
						}

						// Not expecting to have both a game and a program in the same project.  These would alias because we share the project and solution configuration names (just because it makes sense to)
						if ((ExistingProjectTarget.TargetRules.Type == TargetType.Game && TargetRulesObject.Type == TargetType.Program) ||
							(ExistingProjectTarget.TargetRules.Type == TargetType.Program && TargetRulesObject.Type == TargetType.Game))
						{
							throw new BuildException("Not expecting project {0} to already have a Game/Program target ({1}) associated with it while trying to add: {2}", ProjectFilePath, ExistingProjectTarget.TargetRules.ToString(), TargetRulesObject.ToString());
						}
					}

					ProjectTarget ProjectTarget = new ProjectTarget()
						{
							TargetRules = TargetRulesObject,
							TargetFilePath = TargetFilePath,
							ProjectFilePath = ProjectFilePath,
							UnrealProjectFilePath = CheckProjectFile,
							SupportedPlatforms = UEBuildTarget.GetSupportedPlatforms(TargetRulesObject).Where(x => UEBuildPlatform.GetBuildPlatform(x, true) != null).ToArray(),
							CreateRulesDelegate = (Platform, Configuration) => RulesAssembly.CreateTargetRules(TargetName, Platform, Configuration, "", CheckProjectFile, false)
                        };

					if (TargetName == "ShaderCompileWorker")		// @todo projectfiles: Ideally, the target rules file should set this
					{
						ProjectTarget.ForceDevelopmentConfiguration = true;
					}

					ProjectFile.ProjectTargets.Add(ProjectTarget);

					// Make sure the *.Target.cs file is in the project.
					ProjectFile.AddFileToProject(TargetFilePath, BaseFolder);

					// We special case ShaderCompileWorker.  It needs to always be compiled in Development mode.
					Log.TraceVerbose("Generating target {0} for {1}", TargetRulesObject.Type.ToString(), ProjectFilePath);
				}
			}

			// Find all the mods for game projects
			ModProjects = new Dictionary<DirectoryReference, ProjectFile>();
			if(GameProjects.Count == 1)
			{
				KeyValuePair<DirectoryReference, ProjectFile> GameProject = GameProjects.First();
				foreach(PluginInfo PluginInfo in Plugins.ReadProjectPlugins(GameProject.Key))
				{
					if(PluginInfo.Descriptor.Modules != null && PluginInfo.Descriptor.Modules.Length > 0 && PluginInfo.Type == PluginType.Mod)
					{
						FileReference ModProjectFilePath = FileReference.Combine(PluginInfo.Directory, "Mods", PluginInfo.Name + ProjectFileExtension);

						bool bProjectAlreadyExisted;
						ProjectFile ModProjectFile = FindOrAddProject(ModProjectFilePath, IncludeInGeneratedProjects: true, bAlreadyExisted: out bProjectAlreadyExisted);
						ModProjectFile.IsForeignProject = GameProject.Value.IsForeignProject;
						ModProjectFile.IsGeneratedProject = true;
						ModProjectFile.IsStubProject = false;
						ModProjectFile.PluginFilePath = PluginInfo.File;
						ModProjectFile.ProjectTargets.AddRange(GameProject.Value.ProjectTargets);

						AddPluginFilesToProject(PluginInfo.File, PluginInfo.Directory, ModProjectFile);

						ModProjects.Add(PluginInfo.Directory, ModProjectFile);
					}
				}
			}
		}

		
		/// Adds shader source code to the specified project
		protected void AddEngineShaderSource( ProjectFile EngineProject )
		{
			// Setup a project file entry for this module's project.  Remember, some projects may host multiple modules!
			DirectoryReference ShadersDirectory = DirectoryReference.Combine( UnrealBuildTool.EngineDirectory, "Shaders" );

			List<string> SubdirectoryNamesToExclude = new List<string>();
			{
                // Don't include binary shaders in the project file.
                SubdirectoryNamesToExclude.Add( "Binaries" );
				// We never want shader intermediate files in our project file
				SubdirectoryNamesToExclude.Add( "PDBDump" );
				SubdirectoryNamesToExclude.Add( "WorkingDirectory" );
			}

			EngineProject.AddFilesToProject( SourceFileSearch.FindFiles( ShadersDirectory, SubdirectoryNamesToExclude ), UnrealBuildTool.EngineDirectory );
		}


		/// Adds engine build infrastructure files to the specified project
		protected void AddEngineBuildFiles( ProjectFile EngineProject )
		{
			DirectoryReference BuildDirectory = DirectoryReference.Combine( UnrealBuildTool.EngineDirectory, "Build" );

			List<string> SubdirectoryNamesToExclude = new List<string>();
			SubdirectoryNamesToExclude.Add("Receipts");

			EngineProject.AddFilesToProject( SourceFileSearch.FindFiles( BuildDirectory, SubdirectoryNamesToExclude ), UnrealBuildTool.EngineDirectory );
		}



		/// Adds engine documentation to the specified project
		protected void AddEngineDocumentation( ProjectFile EngineProject )
		{
			// NOTE: The project folder added here will actually be collapsed away later if not needed
			DirectoryReference DocumentationProjectDirectory = DirectoryReference.Combine( UnrealBuildTool.EngineDirectory, "Documentation" );
			DirectoryReference DocumentationSourceDirectory = DirectoryReference.Combine( UnrealBuildTool.EngineDirectory, "Documentation", "Source" );
			DirectoryInfo DirInfo = new DirectoryInfo( DocumentationProjectDirectory.FullName );
			if( DirInfo.Exists && DirectoryReference.Exists(DocumentationSourceDirectory) )
			{
				Log.TraceVerbose( "Adding documentation files..." );

				List<string> SubdirectoryNamesToExclude = new List<string>();
				{
					// We never want any of the images or attachment files included in our generated project
					SubdirectoryNamesToExclude.Add( "Images" );
					SubdirectoryNamesToExclude.Add( "Attachments" );

					// The API directory is huge, so don't include any of it
					SubdirectoryNamesToExclude.Add("API");

					// Omit Javascript source because it just confuses the Visual Studio IDE
					SubdirectoryNamesToExclude.Add( "Javascript" );
				}

				List<FileReference> DocumentationFiles = SourceFileSearch.FindFiles( DocumentationSourceDirectory, SubdirectoryNamesToExclude );

				// Filter out non-English documentation files if we were configured to do so
				if( !bAllDocumentationLanguages )
				{
					List<FileReference> FilteredDocumentationFiles = new List<FileReference>();
					foreach( FileReference DocumentationFile in DocumentationFiles )
					{
						bool bPassesFilter = true;
						if( DocumentationFile.FullName.EndsWith( ".udn", StringComparison.InvariantCultureIgnoreCase ) )
						{
							string LanguageSuffix = Path.GetExtension( Path.GetFileNameWithoutExtension( DocumentationFile.FullName ) );
							if( !String.IsNullOrEmpty( LanguageSuffix ) &&
								!LanguageSuffix.Equals( ".int", StringComparison.InvariantCultureIgnoreCase ) )
							{
								bPassesFilter = false;
							}
						}

						if( bPassesFilter )
						{
							FilteredDocumentationFiles.Add( DocumentationFile );
						}
					}
					DocumentationFiles = FilteredDocumentationFiles;
				}

				EngineProject.AddFilesToProject( DocumentationFiles, UnrealBuildTool.EngineDirectory );
			}
			else
			{
				Log.TraceVerbose("Skipping documentation project... directory not found");
			}
		}




		/// <summary>
		/// Adds a new project file and returns an object that represents that project file (or if the project file is already known, returns that instead.)
		/// </summary>
		/// <param name="FilePath">Full path to the project file</param>
		/// <param name="IncludeInGeneratedProjects">True if this project should be included in the set of generated projects.  Only matters when actually generating project files.</param>
		/// <param name="bAlreadyExisted">True if we already had this project file</param>
		/// <returns>Object that represents this project file in Unreal Build Tool</returns>
		public ProjectFile FindOrAddProject( FileReference FilePath, bool IncludeInGeneratedProjects, out bool bAlreadyExisted )
		{
			if( FilePath == null )
			{
				throw new BuildException( "Not valid to call FindOrAddProject() with an empty file path!" );
			}

			// Do we already have this project?
			ProjectFile ExistingProjectFile;
			if( ProjectFileMap.TryGetValue( FilePath, out ExistingProjectFile ) )
			{
				bAlreadyExisted = true;
				return ExistingProjectFile;
			}

			// Add a new project file for the specified path
			ProjectFile NewProjectFile = AllocateProjectFile( FilePath );
			ProjectFileMap[ FilePath ] = NewProjectFile;

			if( IncludeInGeneratedProjects )
			{
				GeneratedProjectFiles.Add( NewProjectFile );
			}

			bAlreadyExisted = false;
			return NewProjectFile;
		}


		/// <summary>
		/// Allocates a generator-specific project file object
		/// </summary>
		/// <param name="InitFilePath">Path to the project file</param>
		/// <returns>The newly allocated project file object</returns>
		protected abstract ProjectFile AllocateProjectFile( FileReference InitFilePath );


		/// <summary>
		/// Allocates a generator-specific master project folder object
		/// </summary>
		/// <param name="OwnerProjectFileGenerator">Project file generator that owns this object</param>
		/// <param name="FolderName">Name for this folder</param>
		/// <returns>The newly allocated project folder object</returns>
		public abstract MasterProjectFolder AllocateMasterProjectFolder(ProjectFileGenerator OwnerProjectFileGenerator, string FolderName );



		/// <summary>
		/// Returns a list of all the known project files
		/// </summary>
		/// <returns>Project file list</returns>
		public List<ProjectFile> AllProjectFiles
		{
			get
			{
				List<ProjectFile> CombinedList = new List<ProjectFile>();
				CombinedList.AddRange( GeneratedProjectFiles );
				CombinedList.AddRange( OtherProjectFiles );
				return CombinedList;
			}
		}

	
		/// <summary>
		/// Writes the project files to disk
		/// </summary>
		/// <returns>True if successful</returns>
		protected virtual bool WriteProjectFiles()
		{
            using(ProgressWriter Progress = new ProgressWriter("Writing project files...", true))
			{
				int TotalProjectFileCount = GeneratedProjectFiles.Count + 1;	// +1 for the master project file, which we'll save next

				for(int ProjectFileIndex = 0 ; ProjectFileIndex < GeneratedProjectFiles.Count; ++ProjectFileIndex )
				{
					ProjectFile CurProject = GeneratedProjectFiles[ ProjectFileIndex ];
					if( !CurProject.WriteProjectFile(
								InPlatforms: SupportedPlatforms,
								InConfigurations: SupportedConfigurations ) )
					{
						return false;
					}

					Progress.Write(ProjectFileIndex + 1, TotalProjectFileCount);
				}

				WriteMasterProjectFile( UBTProject: UBTProject );
				Progress.Write(TotalProjectFileCount, TotalProjectFileCount);
			}
			return true;
		}

		/// <summary>
		/// Writes the master project file (e.g. Visual Studio Solution file)
		/// </summary>
		/// <param name="UBTProject">The UnrealBuildTool project</param>
		/// <returns>True if successful</returns>
		protected abstract bool WriteMasterProjectFile( ProjectFile UBTProject );


		/// <summary>
		/// Writes the specified string content to a file.  Before writing to the file, it loads the existing file (if present) to see if the contents have changed
		/// </summary>
		/// <param name="FileName">File to write</param>
		/// <param name="NewFileContents">File content</param>
		/// <param name="InEncoding"></param>
		/// <returns>True if the file was saved, or if it didn't need to be overwritten because the content was unchanged</returns>
		public static bool WriteFileIfChanged( string FileName, string NewFileContents, Encoding InEncoding = null )
		{
			// Check to see if the file already exists, and if so, load it up
			string LoadedFileContent = null;
			bool FileAlreadyExists = File.Exists( FileName );
			if( FileAlreadyExists )
			{
				try
				{
					LoadedFileContent = File.ReadAllText( FileName );
				}
				catch( Exception )
				{
					Log.TraceInformation( "Error while trying to load existing file {0}.  Ignored.", FileName );
				}
			}


			// Don't bother saving anything out if the new file content is the same as the old file's content
			bool FileNeedsSave = true;
			if( LoadedFileContent != null )
			{
				bool bIgnoreProjectFileWhitespaces = true;
				if (ProjectFileComparer.CompareOrdinalIgnoreCase(LoadedFileContent, NewFileContents, bIgnoreProjectFileWhitespaces) == 0)
				{
					// Exact match!
					FileNeedsSave = false;
				}

				if( !FileNeedsSave )
				{
					Log.TraceVerbose( "Skipped saving {0} because contents haven't changed.", Path.GetFileName( FileName ) );
				}
			}

			if( FileNeedsSave )
			{
				// Save the file
				try
				{
					Directory.CreateDirectory( Path.GetDirectoryName( FileName ) );
                    // When WriteAllText is passed Encoding.UTF8 it likes to write a BOM marker
                    // at the start of the file (adding two bytes to the file length).  For most
                    // files this is only mildly annoying but for Makefiles it can actually make
                    // them un-useable.
                    // TODO(sbc): See if we can just drop the Encoding.UTF8 argument on all
                    // platforms.  In this case UTF8 encoding will still be used but without the
                    // BOM, which is, AFAICT, desirable in almost all cases.
					if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
                        File.WriteAllText(FileName, NewFileContents, new UTF8Encoding());
                    else
                        File.WriteAllText(FileName, NewFileContents, InEncoding != null ? InEncoding : Encoding.UTF8);
                    Log.TraceVerbose("Saved {0}", Path.GetFileName(FileName));
				}
				catch( Exception ex )
				{
					// Unable to write to the project file.
					string Message = string.Format("Error while trying to write file {0}.  The file is probably read-only.", FileName);
					Console.WriteLine();
					Log.TraceError(Message);
					throw new BuildException(ex, Message);
				}
			}

			return true;
		}

		/// <summary>
		/// Adds the given project to the OtherProjects list
		/// </summary>
		/// <param name="InProject">The project to add</param>
		/// <param name="bNeedsAllPlatformAndConfigurations"></param>
		/// <param name="bForceDevelopmentConfiguration"></param>
		/// <param name="bProjectDeploys"></param>
		/// <param name="InSupportedPlatforms"></param>
		/// <param name="InSupportedConfigurations"></param>
		/// <returns>True if successful</returns>
		public void AddExistingProjectFile(ProjectFile InProject, bool bNeedsAllPlatformAndConfigurations = false, bool bForceDevelopmentConfiguration = false, bool bProjectDeploys = false, List<UnrealTargetPlatform> InSupportedPlatforms = null, List<UnrealTargetConfiguration> InSupportedConfigurations = null)
		{
			if( InProject.ProjectTargets.Count != 0 )
			{
				throw new BuildException( "Expecting existing project to not have any ProjectTargets defined yet." );
			}
			
			ProjectTarget ProjectTarget = new ProjectTarget();
			ProjectTarget.SupportedPlatforms = new UnrealTargetPlatform[0];
			if( bForceDevelopmentConfiguration )
			{
				ProjectTarget.ForceDevelopmentConfiguration = true;
			}
			ProjectTarget.ProjectDeploys = bProjectDeploys;

			if (bNeedsAllPlatformAndConfigurations)
			{
				// Add all platforms
				Array AllPlatforms = Enum.GetValues(typeof(UnrealTargetPlatform));
				foreach (UnrealTargetPlatform CurPlatfrom in AllPlatforms)
				{
					ProjectTarget.ExtraSupportedPlatforms.Add(CurPlatfrom);
				}

				// Add all configurations
				Array AllConfigurations = Enum.GetValues(typeof(UnrealTargetConfiguration));
				foreach (UnrealTargetConfiguration CurConfiguration in AllConfigurations)
				{
					ProjectTarget.ExtraSupportedConfigurations.Add( CurConfiguration );
				}
			}
			else if (InSupportedPlatforms != null || InSupportedConfigurations != null)
			{
				if (InSupportedPlatforms != null)
				{
					// Add all explicitly specified platforms
					foreach (UnrealTargetPlatform CurPlatfrom in InSupportedPlatforms)
					{
						ProjectTarget.ExtraSupportedPlatforms.Add(CurPlatfrom);
					}
				}
				else
				{
					// Otherwise, add all platforms
					Array AllPlatforms = Enum.GetValues(typeof(UnrealTargetPlatform));
					foreach (UnrealTargetPlatform CurPlatfrom in AllPlatforms)
					{
						ProjectTarget.ExtraSupportedPlatforms.Add(CurPlatfrom);
					}
				}

				if (InSupportedConfigurations != null)
				{
					// Add all explicitly specified configurations
					foreach (UnrealTargetConfiguration CurConfiguration in InSupportedConfigurations)
					{
						ProjectTarget.ExtraSupportedConfigurations.Add(CurConfiguration);
					}
				}
				else
				{
					// Otherwise, add all configurations
					Array AllConfigurations = Enum.GetValues(typeof(UnrealTargetConfiguration));
					foreach (UnrealTargetConfiguration CurConfiguration in AllConfigurations)
					{
						ProjectTarget.ExtraSupportedConfigurations.Add(CurConfiguration);
					}
				}
			}
			else
			{
				// For existing project files, just support the default desktop platforms and configurations
				ProjectTarget.ExtraSupportedPlatforms.AddRange(Utils.GetPlatformsInClass(UnrealPlatformClass.Desktop));
				// Debug and Development only
				ProjectTarget.ExtraSupportedConfigurations.Add(UnrealTargetConfiguration.Debug);
				ProjectTarget.ExtraSupportedConfigurations.Add(UnrealTargetConfiguration.Development);
			}

			InProject.ProjectTargets.Add( ProjectTarget );

			// Existing projects must always have a GUID.  This will throw an exception if one isn't found.
			InProject.LoadGUIDFromExistingProject();

			OtherProjectFiles.Add( InProject );
		}

		/// The default project to be built for the solution.
		protected ProjectFile DefaultProject;

		/// The project for UnrealBuildTool.  Note that when generating project files for installed builds, we won't have
		/// an UnrealBuildTool project at all.
		protected ProjectFile UBTProject;

		/// List of platforms that we'll support in the project files
		protected List<UnrealTargetPlatform> SupportedPlatforms = new List<UnrealTargetPlatform>();

		/// List of build configurations that we'll support in the project files
		protected List<UnrealTargetConfiguration> SupportedConfigurations = new List<UnrealTargetConfiguration>();

		/// Map of project file names to their project files.  This includes every single project file in memory or otherwise that
		/// we know about so far.  Note that when generating project files, this map may even include project files that we won't
		/// be including in the generated projects.
		protected readonly Dictionary<FileReference, ProjectFile> ProjectFileMap = new Dictionary<FileReference, ProjectFile>();

		/// List of project files that we'll be generating
		protected readonly List<ProjectFile> GeneratedProjectFiles = new List<ProjectFile>();

		/// List of other project files that we want to include in a generated solution file, even though we
		/// aren't generating them ourselves.  Note that these may *not* always be C++ project files (e.g. C#)
		protected readonly List<ProjectFile> OtherProjectFiles = new List<ProjectFile>();

        protected readonly List<ProjectFile> AutomationProjectFiles = new List<ProjectFile>();

		/// List of top-level folders in the master project file
		protected MasterProjectFolder RootFolder;
	}

	/// <summary>
	/// Helper class used for comparing the existing and generated project files.
	/// </summary>
	class ProjectFileComparer
	{
		//static readonly string GUIDRegexPattern = "(\\{){0,1}[0-9a-fA-F]{8}\\-[0-9a-fA-F]{4}\\-[0-9a-fA-F]{4}\\-[0-9a-fA-F]{4}\\-[0-9a-fA-F]{12}(\\}){0,1}";
		//static readonly string GUIDReplaceString = "GUID";

		/// <summary>
		/// Used by CompareOrdinalIgnoreWhitespaceAndCase to determine if a whitespace can be ignored.
		/// </summary>
		/// <param name="Whitespace">Whitespace character.</param>
		/// <returns>true if the character can be ignored, false otherwise.</returns>
		static bool CanIgnoreWhitespace(char Whitespace)
		{
			// Only ignore spaces and tabs.
			return Whitespace == ' ' || Whitespace == '\t';
		}

		/*
		/// <summary>
		/// Replaces all GUIDs in the project file with "GUID" text.
		/// </summary>
		/// <param name="ProjectFileContents">Contents of the project file to remove GUIDs from.</param>
		/// <returns>String with all GUIDs replaced with "GUID" text.</returns>
		static string StripGUIDs(string ProjectFileContents)
		{
			// Replace all GUIDs with "GUID" text.
			return System.Text.RegularExpressions.Regex.Replace(ProjectFileContents, GUIDRegexPattern, GUIDReplaceString);
		}
		*/

		/// <summary>
		/// Compares two project files ignoring whitespaces, case and GUIDs.
		/// </summary>
		/// <remarks>
		/// Compares two specified String objects by evaluating the numeric values of the corresponding Char objects in each string.
		/// Only space and tabulation characters are ignored. Ignores leading whitespaces at the beginning of each line and 
		/// differences in whitespace sequences between matching non-whitespace sub-strings.
		/// </remarks>
		/// <param name="StrA">The first string to compare.</param>
		/// <param name="StrB">The second string to compare. </param>
		/// <returns>An integer that indicates the lexical relationship between the two comparands.</returns>
		public static int CompareOrdinalIgnoreWhitespaceAndCase(string StrA, string StrB)
		{
			// Remove GUIDs before processing the strings.
			//StrA = StripGUIDs(StrA);
			//StrB = StripGUIDs(StrB);

			int IndexA = 0;
			int IndexB = 0;
			while (IndexA < StrA.Length && IndexB < StrB.Length)
			{
				char A = Char.ToLowerInvariant(StrA[IndexA]);
				char B = Char.ToLowerInvariant(StrB[IndexB]);
				if (Char.IsWhiteSpace(A) && Char.IsWhiteSpace(B) && CanIgnoreWhitespace(A) && CanIgnoreWhitespace(B))
				{
					// Skip whitespaces in both strings
					for (IndexA++; IndexA < StrA.Length && Char.IsWhiteSpace(StrA[IndexA]) == true; IndexA++) ;
					for (IndexB++; IndexB < StrB.Length && Char.IsWhiteSpace(StrB[IndexB]) == true; IndexB++) ;
				}
				else if (Char.IsWhiteSpace(A) && IndexA > 0 && StrA[IndexA - 1] == '\n')
				{
					// Skip whitespaces in StrA at the beginning of each line
					for (IndexA++; IndexA < StrA.Length && Char.IsWhiteSpace(StrA[IndexA]) == true; IndexA++) ;
				}
				else if (Char.IsWhiteSpace(B) && IndexB > 0 && StrB[IndexB - 1] == '\n')
				{
					// Skip whitespaces in StrA at the beginning of each line
					for (IndexB++; IndexB < StrB.Length && Char.IsWhiteSpace(StrB[IndexB]) == true; IndexB++) ;
				}
				else if (A != B)
				{
					return A - B;
				}
				else
				{
					IndexA++;
					IndexB++;
				}
			}
			// Check if we reached the end in both strings
			return (StrA.Length - IndexA) - (StrB.Length - IndexB);
		}

		/// <summary>
		/// Compares two project files ignoring case and GUIDs.
		/// </summary>
		/// <param name="StrA">The first string to compare.</param>
		/// <param name="StrB">The second string to compare. </param>
		/// <returns>An integer that indicates the lexical relationship between the two comparands.</returns>
		public static int CompareOrdinalIgnoreCase(string StrA, string StrB)
		{
			// Remove GUIDs before processing the strings.
			//StrA = StripGUIDs(StrA);
			//StrB = StripGUIDs(StrB);

			// Use simple ordinal comparison.
			return String.Compare(StrA, StrB, StringComparison.InvariantCultureIgnoreCase);
		}

		/// <summary>
		/// Compares two project files ignoring case and GUIDs.
		/// </summary>
		/// <see cref="CompareOrdinalIgnoreWhitespaceAndCase"/>
		/// <param name="StrA">The first string to compare.</param>
		/// <param name="StrB">The second string to compare. </param>
		/// <param name="bIgnoreWhitespace">True if whitsapces should be ignored.</param>
		/// <returns>An integer that indicates the lexical relationship between the two comparands.</returns>
		public static int CompareOrdinalIgnoreCase(string StrA, string StrB, bool bIgnoreWhitespace)
		{
			if (bIgnoreWhitespace)
			{
				return CompareOrdinalIgnoreWhitespaceAndCase(StrA, StrB);
			}
			else
			{
				return CompareOrdinalIgnoreCase(StrA, StrB);
			}
		}
	}
}
