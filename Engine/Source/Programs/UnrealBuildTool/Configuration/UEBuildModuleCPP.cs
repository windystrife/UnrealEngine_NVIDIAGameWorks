using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// A module that is compiled from C++ code.
	/// </summary>
	class UEBuildModuleCPP : UEBuildModule
	{
		public class AutoGenerateCppInfoClass
		{
			public class BuildInfoClass
			{
				/// <summary>
				/// The wildcard of the *.gen.cpp file which was generated for the module
				/// </summary>
				public readonly string FileWildcard;

				public BuildInfoClass(string InWildcard)
				{
					Debug.Assert(InWildcard != null);

					FileWildcard = InWildcard;
				}
			}

			/// <summary>
			/// Information about how to build the .gen.cpp files. If this is null, then we're not building .gen.cpp files for this module.
			/// </summary>
			public BuildInfoClass BuildInfo;

			public AutoGenerateCppInfoClass(BuildInfoClass InBuildInfo)
			{
				BuildInfo = InBuildInfo;
			}
		}

		/// <summary>
		/// Information about the .gen.cpp file.  If this is null then this module doesn't have any UHT-produced code.
		/// </summary>
		public AutoGenerateCppInfoClass AutoGenerateCppInfo = null;

		public class SourceFilesClass
		{
			public readonly List<FileItem> MissingFiles = new List<FileItem>();
			public readonly List<FileItem> CPPFiles = new List<FileItem>();
			public readonly List<FileItem> CFiles = new List<FileItem>();
			public readonly List<FileItem> CCFiles = new List<FileItem>();
			public readonly List<FileItem> MMFiles = new List<FileItem>();
			public readonly List<FileItem> RCFiles = new List<FileItem>();
			public readonly List<FileItem> OtherFiles = new List<FileItem>();

			public int Count
			{
				get
				{
					return MissingFiles.Count +
						   CPPFiles.Count +
						   CFiles.Count +
						   CCFiles.Count +
						   MMFiles.Count +
						   RCFiles.Count +
						   OtherFiles.Count;
				}
			}

			/// <summary>
			/// Copy from list to list helper.
			/// </summary>
			/// <param name="From">Source list.</param>
			/// <param name="To">Destination list.</param>
			private static void CopyFromListToList(List<FileItem> From, List<FileItem> To)
			{
				To.Clear();
				To.AddRange(From);
			}

			/// <summary>
			/// Copies file lists from other SourceFilesClass to this.
			/// </summary>
			/// <param name="Other">Source object.</param>
			public void CopyFrom(SourceFilesClass Other)
			{
				CopyFromListToList(Other.MissingFiles, MissingFiles);
				CopyFromListToList(Other.CPPFiles, CPPFiles);
				CopyFromListToList(Other.CFiles, CFiles);
				CopyFromListToList(Other.CCFiles, CCFiles);
				CopyFromListToList(Other.MMFiles, MMFiles);
				CopyFromListToList(Other.RCFiles, RCFiles);
				CopyFromListToList(Other.OtherFiles, OtherFiles);
			}
		}

		/// <summary>
		/// Adds additional source cpp files for this module.
		/// </summary>
		/// <param name="Files">Files to add.</param>
		public void AddAdditionalCPPFiles(IEnumerable<FileItem> Files)
		{
			SourceFiles.AddRange(Files);
			SourceFilesToBuild.CPPFiles.AddRange(Files);
		}

		/// <summary>
		/// All the source files for this module
		/// </summary>
		public readonly List<FileItem> SourceFiles = new List<FileItem>();

		/// <summary>
		/// A list of the absolute paths of source files to be built in this module.
		/// </summary>
		public readonly SourceFilesClass SourceFilesToBuild = new SourceFilesClass();

		/// <summary>
		/// A list of the source files that were found for the module.
		/// </summary>
		public readonly SourceFilesClass SourceFilesFound = new SourceFilesClass();

		/// <summary>
		/// The directory for this module's generated code
		/// </summary>
		public readonly DirectoryReference GeneratedCodeDirectory;

		/// <summary>
		/// The preprocessor definitions used to compile this module's private implementation.
		/// </summary>
		HashSet<string> Definitions;

		/// When set, allows this module to report compiler definitions and include paths for Intellisense
		IntelliSenseGatherer IntelliSenseGatherer;

		public List<string> IncludeSearchPaths = new List<string>();

		public class ProcessedDependenciesClass
		{
			/// <summary>
			/// The file, if any, which is used as the unique PCH for this module
			/// </summary>
			public FileItem UniquePCHHeaderFile = null;
		}

		/// <summary>
		/// The processed dependencies for the class
		/// </summary>
		public ProcessedDependenciesClass ProcessedDependencies = null;

		/// <summary>
		/// List of invalid include directives. These are buffered up and output before we start compiling.
		/// </summary>
		public List<string> InvalidIncludeDirectiveMessages;

		/// <summary>
		/// Hack to skip adding definitions to compile environment. They will be baked into source code by external code.
		/// </summary>
		public bool bSkipDefinitionsForCompileEnvironment = false;

		public IEnumerable<string> FindGeneratedCppFiles()
		{
			return ((null == GeneratedCodeDirectory) || !DirectoryReference.Exists(GeneratedCodeDirectory))
				? Enumerable.Empty<string>()
				: DirectoryReference.EnumerateFiles(GeneratedCodeDirectory, "*.gen.cpp", SearchOption.TopDirectoryOnly).Select((Dir) => Dir.FullName);
		}

		protected override void GetReferencedDirectories(HashSet<DirectoryReference> Directories)
		{
			base.GetReferencedDirectories(Directories);

			foreach(FileItem SourceFile in SourceFiles)
			{
				Directories.Add(SourceFile.Reference.Directory);
			}
		}

		/// <summary>
		/// Categorizes source files into per-extension buckets
		/// </summary>
		private static void CategorizeSourceFiles(IEnumerable<FileItem> InSourceFiles, SourceFilesClass OutSourceFiles)
		{
			foreach (FileItem SourceFile in InSourceFiles)
			{
				string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();
				if (!SourceFile.bExists)
				{
					OutSourceFiles.MissingFiles.Add(SourceFile);
				}
				else if (Extension == ".CPP")
				{
					OutSourceFiles.CPPFiles.Add(SourceFile);
				}
				else if (Extension == ".C")
				{
					OutSourceFiles.CFiles.Add(SourceFile);
				}
				else if (Extension == ".CC")
				{
					OutSourceFiles.CCFiles.Add(SourceFile);
				}
				else if (Extension == ".MM" || Extension == ".M")
				{
					OutSourceFiles.MMFiles.Add(SourceFile);
				}
				else if (Extension == ".RC")
				{
					OutSourceFiles.RCFiles.Add(SourceFile);
				}
				else
				{
					OutSourceFiles.OtherFiles.Add(SourceFile);
				}
			}
		}

		/// <summary>
		/// List of whitelisted circular dependencies. Please do NOT add new modules here; refactor to allow the modules to be decoupled instead.
		/// </summary>
		static readonly KeyValuePair<string, string>[] WhitelistedCircularDependencies =
		{
			new KeyValuePair<string, string>("Engine", "AIModule"),
			new KeyValuePair<string, string>("Engine", "Landscape"),
			new KeyValuePair<string, string>("Engine", "UMG"),
			new KeyValuePair<string, string>("Engine", "GameplayTags"),
			new KeyValuePair<string, string>("Engine", "MaterialShaderQualitySettings"),
			new KeyValuePair<string, string>("Engine", "UnrealEd"),
			new KeyValuePair<string, string>("PacketHandler", "ReliabilityHandlerComponent"),
			new KeyValuePair<string, string>("GameplayDebugger", "AIModule"),
			new KeyValuePair<string, string>("GameplayDebugger", "GameplayTasks"),
			new KeyValuePair<string, string>("Engine", "CinematicCamera"),
			new KeyValuePair<string, string>("Engine", "CollisionAnalyzer"),
			new KeyValuePair<string, string>("Engine", "LogVisualizer"),
			new KeyValuePair<string, string>("Engine", "Kismet"),
			new KeyValuePair<string, string>("Landscape", "UnrealEd"),
			new KeyValuePair<string, string>("Landscape", "MaterialUtilities"),
			new KeyValuePair<string, string>("LocalizationDashboard", "LocalizationService"),
			new KeyValuePair<string, string>("LocalizationDashboard", "MainFrame"),
			new KeyValuePair<string, string>("LocalizationDashboard", "TranslationEditor"),
			new KeyValuePair<string, string>("Documentation", "SourceControl"),
			new KeyValuePair<string, string>("UnrealEd", "GraphEditor"),
			new KeyValuePair<string, string>("UnrealEd", "Kismet"),
			new KeyValuePair<string, string>("UnrealEd", "AudioEditor"),
			new KeyValuePair<string, string>("BlueprintGraph", "KismetCompiler"),
			new KeyValuePair<string, string>("BlueprintGraph", "UnrealEd"),
			new KeyValuePair<string, string>("BlueprintGraph", "GraphEditor"),
			new KeyValuePair<string, string>("BlueprintGraph", "Kismet"),
			new KeyValuePair<string, string>("BlueprintGraph", "CinematicCamera"),
			new KeyValuePair<string, string>("ConfigEditor", "PropertyEditor"),
			new KeyValuePair<string, string>("SourceControl", "UnrealEd"),
			new KeyValuePair<string, string>("Kismet", "BlueprintGraph"),
			new KeyValuePair<string, string>("Kismet", "UMGEditor"),
			new KeyValuePair<string, string>("MovieSceneTools", "Sequencer"),
			new KeyValuePair<string, string>("Sequencer", "MovieSceneTools"),
			new KeyValuePair<string, string>("AIModule", "AITestSuite"),
			new KeyValuePair<string, string>("GameplayTasks", "UnrealEd"),
			new KeyValuePair<string, string>("AnimGraph", "UnrealEd"),
			new KeyValuePair<string, string>("AnimGraph", "GraphEditor"),
			new KeyValuePair<string, string>("MaterialUtilities", "Landscape"),
			new KeyValuePair<string, string>("HierarchicalLODOutliner", "UnrealEd"),
			new KeyValuePair<string, string>("PixelInspectorModule", "UnrealEd"),
			new KeyValuePair<string, string>("GameplayAbilitiesEditor", "BlueprintGraph"),
            new KeyValuePair<string, string>("UnrealEd", "ViewportInteraction"),
            new KeyValuePair<string, string>("UnrealEd", "VREditor"),
            new KeyValuePair<string, string>("LandscapeEditor", "ViewportInteraction"),
            new KeyValuePair<string, string>("LandscapeEditor", "VREditor"),
            new KeyValuePair<string, string>("FoliageEdit", "ViewportInteraction"),
            new KeyValuePair<string, string>("FoliageEdit", "VREditor"),
            new KeyValuePair<string, string>("MeshPaint", "ViewportInteraction"),
            new KeyValuePair<string, string>("MeshPaint", "VREditor"),
            new KeyValuePair<string, string>("MeshPaintMode", "ViewportInteraction"),
            new KeyValuePair<string, string>("MeshPaintMode", "VREditor"),
            new KeyValuePair<string, string>("Sequencer", "ViewportInteraction"),
        };


		public UEBuildModuleCPP(
			string InName,
			UHTModuleType InType,
			DirectoryReference InModuleDirectory,
			DirectoryReference InGeneratedCodeDirectory,
			IntelliSenseGatherer InIntelliSenseGatherer,
			IEnumerable<FileItem> InSourceFiles,
			ModuleRules InRules,
			bool bInBuildSourceFiles,
			FileReference InRulesFile,
			List<RuntimeDependency> InRuntimeDependencies
			)
			: base(
					InName,
					InType,
					InModuleDirectory,
					InRules,
					InRulesFile,
					InRuntimeDependencies
				)
		{
			GeneratedCodeDirectory = InGeneratedCodeDirectory;
			IntelliSenseGatherer = InIntelliSenseGatherer;

			SourceFiles = InSourceFiles.ToList();

			CategorizeSourceFiles(InSourceFiles, SourceFilesFound);
			if (bInBuildSourceFiles)
			{
				SourceFilesToBuild.CopyFrom(SourceFilesFound);
			}

			Definitions = HashSetFromOptionalEnumerableStringParameter(InRules.Definitions);
			foreach (string Def in Definitions)
			{
				Log.TraceVerbose("Compile Env {0}: {1}", Name, Def);
			}

			foreach(string CircularlyReferencedModuleName in Rules.CircularlyReferencedDependentModules)
			{
				if(CircularlyReferencedModuleName != "BlueprintContext" && !WhitelistedCircularDependencies.Any(x => x.Key == Name && x.Value == CircularlyReferencedModuleName))
				{
					Log.TraceWarning("Found reference between '{0}' and '{1}'. Support for circular references is being phased out; please do not introduce new ones.", Name, CircularlyReferencedModuleName);
				}
			}
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile(ReadOnlyTargetRules Target, UEToolChain ToolChain, CppCompileEnvironment BinaryCompileEnvironment, List<PrecompiledHeaderTemplate> SharedPCHs, ISourceFileWorkingSet WorkingSet, ActionGraph ActionGraph)
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(BinaryCompileEnvironment.Platform);

			List<FileItem> LinkInputFiles = new List<FileItem>();
			if (ProjectFileGenerator.bGenerateProjectFiles && IntelliSenseGatherer == null)
			{
				// Nothing to do for IntelliSense, bail out early
				return LinkInputFiles;
			}

			CppCompileEnvironment ModuleCompileEnvironment = CreateModuleCompileEnvironment(Target, BinaryCompileEnvironment);
			IncludeSearchPaths = ModuleCompileEnvironment.IncludePaths.UserIncludePaths.ToList();
			IncludeSearchPaths.AddRange(ModuleCompileEnvironment.IncludePaths.SystemIncludePaths.ToList());

			if (IntelliSenseGatherer != null)
			{
				// Update project file's set of preprocessor definitions and include paths
				IntelliSenseGatherer.AddIntelliSensePreprocessorDefinitions(ModuleCompileEnvironment.Definitions);
				IntelliSenseGatherer.AddInteliiSenseIncludePaths(ModuleCompileEnvironment.IncludePaths.SystemIncludePaths, bAddingSystemIncludes: true);
				IntelliSenseGatherer.AddInteliiSenseIncludePaths(ModuleCompileEnvironment.IncludePaths.UserIncludePaths, bAddingSystemIncludes: false);

				// Bail out.  We don't need to actually compile anything while generating project files.
				return LinkInputFiles;
			}

			// Throw an error if the module's source file list referenced any non-existent files.
			if (SourceFilesToBuild.MissingFiles.Count > 0)
			{
				throw new BuildException(
					"UBT ERROR: Module \"{0}\" references non-existent files:\n{1} (perhaps a file was added to the project but not checked in)",
					Name,
					string.Join("\n", SourceFilesToBuild.MissingFiles.Select(M => M.AbsolutePath))
				);
			}

			{
				// Process all of the header file dependencies for this module
				this.CachePCHUsageForModuleSourceFiles(Target, ModuleCompileEnvironment);

				// Make sure our RC files have cached includes.  
				foreach (FileItem RCFile in SourceFilesToBuild.RCFiles)
				{
					// The default resource file (PCLaunch.rc) is created in a module-agnostic way, so we want to avoid overriding the include paths for it
					if(RCFile.CachedIncludePaths == null)
					{
						RCFile.CachedIncludePaths = ModuleCompileEnvironment.IncludePaths;
					}
				}
			}


			// Check to see if this is an Engine module (including program or plugin modules).  That is, the module is located under the "Engine" folder
			bool IsPluginModule = (Target.ProjectFile != null && ModuleDirectory.IsUnderDirectory(DirectoryReference.Combine(Target.ProjectFile.Directory, "Plugins")));
			bool IsGameModule = !IsPluginModule && !UnrealBuildTool.IsUnderAnEngineDirectory(ModuleDirectory);

			// Should we force a precompiled header to be generated for this module?  Usually, we only bother with a
			// precompiled header if there are at least several source files in the module (after combining them for unity
			// builds.)  But for game modules, it can be convenient to always have a precompiled header to single-file
			// changes to code is really quick to compile.
			int MinFilesUsingPrecompiledHeader = Target.MinFilesUsingPrecompiledHeader;
			if (Rules.MinFilesUsingPrecompiledHeaderOverride != 0)
			{
				MinFilesUsingPrecompiledHeader = Rules.MinFilesUsingPrecompiledHeaderOverride;
			}
			else if (IsGameModule && Target.bForcePrecompiledHeaderForGameModules)
			{
				// This is a game module with only a small number of source files, so go ahead and force a precompiled header
				// to be generated to make incremental changes to source files as fast as possible for small projects.
				MinFilesUsingPrecompiledHeader = 1;
			}


			// Engine modules will always use unity build mode unless MinSourceFilesForUnityBuildOverride is specified in
			// the module rules file.  By default, game modules only use unity of they have enough source files for that
			// to be worthwhile.  If you have a lot of small game modules, consider specifying MinSourceFilesForUnityBuildOverride=0
			// in the modules that you don't typically iterate on source files in very frequently.
			int MinSourceFilesForUnityBuild = 0;
			if (Rules.MinSourceFilesForUnityBuildOverride != 0)
			{
				MinSourceFilesForUnityBuild = Rules.MinSourceFilesForUnityBuildOverride;
			}
			else if (IsGameModule)
			{
				// Game modules with only a small number of source files are usually better off having faster iteration times
				// on single source file changes, so we forcibly disable unity build for those modules
				MinSourceFilesForUnityBuild = Target.MinGameModuleSourceFilesForUnityBuild;
			}


			// Should we use unity build mode for this module?
			bool bModuleUsesUnityBuild = false;
			if (Target.bUseUnityBuild || Target.bForceUnityBuild)
			{
				if (Target.bForceUnityBuild)
				{
					Log.TraceVerbose("Module '{0}' using unity build mode (bForceUnityBuild enabled for this module)", this.Name);
					bModuleUsesUnityBuild = true;
				}
				else if (Rules.bFasterWithoutUnity)
				{
					Log.TraceVerbose("Module '{0}' not using unity build mode (bFasterWithoutUnity enabled for this module)", this.Name);
					bModuleUsesUnityBuild = false;
				}
				else if (SourceFilesToBuild.CPPFiles.Count < MinSourceFilesForUnityBuild)
				{
					Log.TraceVerbose("Module '{0}' not using unity build mode (module with fewer than {1} source files)", this.Name, MinSourceFilesForUnityBuild);
					bModuleUsesUnityBuild = false;
				}
				else
				{
					Log.TraceVerbose("Module '{0}' using unity build mode", this.Name);
					bModuleUsesUnityBuild = true;
				}
			}
			else
			{
				Log.TraceVerbose("Module '{0}' not using unity build mode", this.Name);
			}

			// Set up the environment with which to compile the CPP files
			CppCompileEnvironment CompileEnvironment = ModuleCompileEnvironment;
			if (Target.bUsePCHFiles)
			{
				// If this module has an explicit PCH, use that
				if(Rules.PrivatePCHHeaderFile != null)
				{
					PrecompiledHeaderInstance Instance = CreatePrivatePCH(ToolChain, FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.PrivatePCHHeaderFile)), CompileEnvironment, ActionGraph);

					CompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
					CompileEnvironment.Definitions.Clear();
					CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
					CompileEnvironment.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Reference;
					CompileEnvironment.PrecompiledHeaderFile = Instance.Output.PrecompiledHeaderFile;

					LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
				}

				// Try to find a suitable shared PCH for this module
				if (CompileEnvironment.PrecompiledHeaderFile == null && SharedPCHs.Count > 0 && !CompileEnvironment.bIsBuildingLibrary && Rules.PCHUsage != ModuleRules.PCHUsageMode.NoSharedPCHs)
				{
					// Find all the dependencies of this module
					HashSet<UEBuildModule> ReferencedModules = new HashSet<UEBuildModule>();
					GetAllDependencyModules(new List<UEBuildModule>(), ReferencedModules, bIncludeDynamicallyLoaded: false, bForceCircular: false, bOnlyDirectDependencies: true);

					// Find the first shared PCH module we can use
					PrecompiledHeaderTemplate Template = SharedPCHs.FirstOrDefault(x => ReferencedModules.Contains(x.Module));
					if(Template != null && Template.IsValidFor(CompileEnvironment))
					{
						PrecompiledHeaderInstance Instance = FindOrCreateSharedPCH(ToolChain, Template, ModuleCompileEnvironment.bOptimizeCode, ModuleCompileEnvironment.bUseRTTI, ActionGraph);

						FileReference PrivateDefinitionsFile = FileReference.Combine(CompileEnvironment.OutputDirectory, String.Format("Definitions.{0}.h", Name));
						using (StringWriter Writer = new StringWriter())
						{
							// Remove the module _API definition for cases where there are circular dependencies between the shared PCH module and modules using it
							Writer.WriteLine("#undef {0}", ModuleApiDefine);

							// Games may choose to use shared PCHs from the engine, so allow them to change the value of these macros
							if(!IsEngineModule())
							{
								Writer.WriteLine("#undef UE_IS_ENGINE_MODULE");
								Writer.WriteLine("#undef DEPRECATED_FORGAME");
								Writer.WriteLine("#define DEPRECATED_FORGAME DEPRECATED");
							}

							WriteDefinitions(CompileEnvironment.Definitions, Writer);
							FileItem.CreateIntermediateTextFile(PrivateDefinitionsFile, Writer.ToString());
						}

						CompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
						CompileEnvironment.Definitions.Clear();
						CompileEnvironment.ForceIncludeFiles.Add(PrivateDefinitionsFile);
						CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
						CompileEnvironment.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Reference;
						CompileEnvironment.PrecompiledHeaderFile = Instance.Output.PrecompiledHeaderFile;

						LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
					}
				}

				// If there was one header that was included first by enough C++ files, use it as the precompiled header. Only use precompiled headers for projects with enough files to make the PCH creation worthwhile.
				if (CompileEnvironment.PrecompiledHeaderFile == null && SourceFilesToBuild.CPPFiles.Count >= MinFilesUsingPrecompiledHeader && ProcessedDependencies != null)
				{
					PrecompiledHeaderInstance Instance = CreatePrivatePCH(ToolChain, ProcessedDependencies.UniquePCHHeaderFile, CompileEnvironment, ActionGraph);

					CompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
					CompileEnvironment.Definitions.Clear();
					CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
					CompileEnvironment.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Reference;
					CompileEnvironment.PrecompiledHeaderFile = Instance.Output.PrecompiledHeaderFile;

					LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
				}
			}

			// Compile CPP files
			List<FileItem> CPPFilesToCompile = SourceFilesToBuild.CPPFiles;
			if (bModuleUsesUnityBuild)
			{
				CPPFilesToCompile = Unity.GenerateUnityCPPs(Target, CPPFilesToCompile, CompileEnvironment, WorkingSet, Rules.ShortName ?? Name);
				LinkInputFiles.AddRange(CompileUnityFilesWithToolChain(Target, ToolChain, CompileEnvironment, ModuleCompileEnvironment, CPPFilesToCompile, ActionGraph).ObjectFiles);
			}
			else
			{
				LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(CompileEnvironment, CPPFilesToCompile, Name, ActionGraph).ObjectFiles);
			}

			// Compile all the generated CPP files
			if (AutoGenerateCppInfo != null && AutoGenerateCppInfo.BuildInfo != null && !CompileEnvironment.bHackHeaderGenerator)
			{
				string[] GeneratedFiles = Directory.GetFiles(Path.GetDirectoryName(AutoGenerateCppInfo.BuildInfo.FileWildcard), Path.GetFileName(AutoGenerateCppInfo.BuildInfo.FileWildcard));
				if(GeneratedFiles.Length > 0)
				{
					// Create a compile environment for the generated files. We can disable creating debug info here to improve link times.
					CppCompileEnvironment GeneratedCPPCompileEnvironment = CompileEnvironment;
					if(GeneratedCPPCompileEnvironment.bCreateDebugInfo && Target.bDisableDebugInfoForGeneratedCode)
					{
						GeneratedCPPCompileEnvironment = new CppCompileEnvironment(GeneratedCPPCompileEnvironment);
						GeneratedCPPCompileEnvironment.bCreateDebugInfo = false;
					}

					// Compile all the generated files
					List<FileItem> GeneratedFileItems = new List<FileItem>();
					foreach (string GeneratedFilename in GeneratedFiles)
					{
						FileItem GeneratedCppFileItem = FileItem.GetItemByPath(GeneratedFilename);

						CachePCHUsageForModuleSourceFile(CompileEnvironment, GeneratedCppFileItem);

						// @todo ubtmake: Check for ALL other places where we might be injecting .cpp or .rc files for compiling without caching CachedCPPIncludeInfo first (anything platform specific?)
						GeneratedFileItems.Add(GeneratedCppFileItem);
					}

					if (bModuleUsesUnityBuild)
					{
						GeneratedFileItems = Unity.GenerateUnityCPPs(Target, GeneratedFileItems, GeneratedCPPCompileEnvironment, WorkingSet, (Rules.ShortName ?? Name) + ".gen");
						LinkInputFiles.AddRange(CompileUnityFilesWithToolChain(Target, ToolChain, GeneratedCPPCompileEnvironment, ModuleCompileEnvironment, GeneratedFileItems, ActionGraph).ObjectFiles);
					}
					else
					{
						LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(GeneratedCPPCompileEnvironment, GeneratedFileItems, Name, ActionGraph).ObjectFiles);
					}
				}
			}

			// Compile C files directly. Do not use a PCH here, because a C++ PCH is not compatible with C source files.
			LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(ModuleCompileEnvironment, SourceFilesToBuild.CFiles, Name, ActionGraph).ObjectFiles);

			// Compile CC files directly.
			LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(CompileEnvironment, SourceFilesToBuild.CCFiles, Name, ActionGraph).ObjectFiles);

			// Compile MM files directly.
			LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(CompileEnvironment, SourceFilesToBuild.MMFiles, Name, ActionGraph).ObjectFiles);

			// Compile RC files.
			LinkInputFiles.AddRange(ToolChain.CompileRCFiles(ModuleCompileEnvironment, SourceFilesToBuild.RCFiles, ActionGraph).ObjectFiles);

			return LinkInputFiles;
		}

		/// <summary>
		/// Create a shared PCH template for this module, which allows constructing shared PCH instances in the future
		/// </summary>
		/// <param name="Target">The target which owns this module</param>
		/// <param name="BaseCompileEnvironment">Base compile environment for this target</param>
		/// <returns>Template for shared PCHs</returns>
		public PrecompiledHeaderTemplate CreateSharedPCHTemplate(UEBuildTarget Target, CppCompileEnvironment BaseCompileEnvironment)
		{
			CppCompileEnvironment CompileEnvironment = CreateSharedPCHCompileEnvironment(Target, BaseCompileEnvironment);
			FileItem HeaderFile = FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.SharedPCHHeaderFile));
			HeaderFile.CachedIncludePaths = CompileEnvironment.IncludePaths;
			DirectoryReference OutputDir = (CompileEnvironment.PCHOutputDirectory != null)? DirectoryReference.Combine(CompileEnvironment.PCHOutputDirectory, Name) : CompileEnvironment.OutputDirectory;
			return new PrecompiledHeaderTemplate(this, CompileEnvironment, HeaderFile, OutputDir);
		}

		/// <summary>
		/// Creates a precompiled header action to generate a new pch file 
		/// </summary>
		/// <param name="ToolChain">The toolchain to generate the PCH</param>
		/// <param name="HeaderFile"></param>
		/// <param name="ModuleCompileEnvironment"></param>
		/// <param name="ActionGraph">Graph containing build actions</param>
		/// <returns>The created PCH instance.</returns>
		private PrecompiledHeaderInstance CreatePrivatePCH(UEToolChain ToolChain, FileItem HeaderFile, CppCompileEnvironment ModuleCompileEnvironment, ActionGraph ActionGraph)
		{
			// Cache the header file include paths. This file could have been a shared PCH too, so ignore if the include paths are already set.
			if(HeaderFile.CachedIncludePaths == null)
			{
				HeaderFile.CachedIncludePaths = ModuleCompileEnvironment.IncludePaths;
			}

			// Create the wrapper file, which sets all the definitions needed to compile it
			FileReference WrapperLocation = FileReference.Combine(ModuleCompileEnvironment.OutputDirectory, String.Format("PCH.{0}.h", Name));
			FileItem WrapperFile = CreatePCHWrapperFile(WrapperLocation, ModuleCompileEnvironment.Definitions, HeaderFile);

			// Create a new C++ environment that is used to create the PCH.
			CppCompileEnvironment CompileEnvironment = new CppCompileEnvironment(ModuleCompileEnvironment);
			CompileEnvironment.Definitions.Clear();
			CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
			CompileEnvironment.PrecompiledHeaderIncludeFilename = WrapperFile.Reference;
			CompileEnvironment.OutputDirectory = ModuleCompileEnvironment.OutputDirectory;
			CompileEnvironment.bOptimizeCode = ModuleCompileEnvironment.bOptimizeCode;

			// Create the action to compile the PCH file.
			CPPOutput Output = ToolChain.CompileCPPFiles(CompileEnvironment, new List<FileItem>() { WrapperFile }, Name, ActionGraph);
			return new PrecompiledHeaderInstance(WrapperFile, CompileEnvironment.bOptimizeCode, CompileEnvironment.bUseRTTI, Output);
		}

		/// <summary>
		/// Generates a precompiled header instance from the given template, or returns an existing one if it already exists
		/// </summary>
		/// <param name="ToolChain">The toolchain being used to build this module</param>
		/// <param name="Template">The PCH template</param>
		/// <param name="bOptimizeCode">Whether optimization should be enabled for this PCH</param>
		/// <param name="bUseRTTI">Whether to enable RTTI for this PCH</param>
		/// <param name="ActionGraph">Graph containing build actions</param>
		/// <returns>Instance of a PCH</returns>
		public PrecompiledHeaderInstance FindOrCreateSharedPCH(UEToolChain ToolChain, PrecompiledHeaderTemplate Template, bool bOptimizeCode, bool bUseRTTI, ActionGraph ActionGraph)
		{
			PrecompiledHeaderInstance Instance = Template.Instances.Find(x => x.bOptimizeCode == bOptimizeCode && x.bUseRTTI == bUseRTTI);
			if(Instance == null)
			{
				// Create a suffix to distinguish this shared PCH variant from any others. Currently only optimized and non-optimized shared PCHs are supported.
				string Variant = "";
				if(bOptimizeCode != Template.BaseCompileEnvironment.bOptimizeCode)
				{
					if(bOptimizeCode)
					{
						Variant += ".Optimized";
					}
					else
					{
						Variant += ".NonOptimized";
					}
				}
				if(bUseRTTI != Template.BaseCompileEnvironment.bUseRTTI)
				{
					if (bUseRTTI)
					{
						Variant += ".RTTI";
					}
					else
					{
						Variant += ".NonRTTI";
					}
				}

				// Create the wrapper file, which sets all the definitions needed to compile it
				FileReference WrapperLocation = FileReference.Combine(Template.OutputDir, String.Format("SharedPCH.{0}{1}.h", Template.Module.Name, Variant));
				FileItem WrapperFile = CreatePCHWrapperFile(WrapperLocation, Template.BaseCompileEnvironment.Definitions, Template.HeaderFile);

				// Create the compile environment for this PCH
				CppCompileEnvironment CompileEnvironment = new CppCompileEnvironment(Template.BaseCompileEnvironment);
				CompileEnvironment.Definitions.Clear();
				CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
				CompileEnvironment.PrecompiledHeaderIncludeFilename = WrapperFile.Reference;
				CompileEnvironment.OutputDirectory = Template.OutputDir;
				CompileEnvironment.bOptimizeCode = bOptimizeCode;
				CompileEnvironment.bUseRTTI = bUseRTTI;

				// Create the PCH
				CPPOutput Output = ToolChain.CompileCPPFiles(CompileEnvironment, new List<FileItem>() { WrapperFile }, "Shared", ActionGraph);
				Instance = new PrecompiledHeaderInstance(WrapperFile, bOptimizeCode, bUseRTTI, Output);
				Template.Instances.Add(Instance);
			}
			return Instance;
		}

		/// <summary>
		/// Compiles the provided CPP unity files. Will
		/// </summary>
		private CPPOutput CompileUnityFilesWithToolChain(ReadOnlyTargetRules Target, UEToolChain ToolChain, CppCompileEnvironment CompileEnvironment, CppCompileEnvironment ModuleCompileEnvironment, List<FileItem> SourceFiles, ActionGraph ActionGraph)
		{
			List<FileItem> NormalFiles = new List<FileItem>();
			List<FileItem> AdaptiveFiles = new List<FileItem>();

			bool bAdaptiveUnityDisablesPCH = (Target.bAdaptiveUnityDisablesPCH && Rules.PCHUsage == ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs);

			if ((Target.bAdaptiveUnityDisablesOptimizations || bAdaptiveUnityDisablesPCH) && !Target.bStressTestUnity)
			{
				foreach (FileItem File in SourceFiles)
				{
					// Basic check as to whether something in this module is/isn't a unity file...
					if (File.ToString().StartsWith(Unity.ModulePrefix))
					{
						NormalFiles.Add(File);
					}
					else
					{
						AdaptiveFiles.Add(File);
					}
				}
			}
			else
			{
				NormalFiles.AddRange(SourceFiles);
			}

			CPPOutput OutputFiles = new CPPOutput();

			if (NormalFiles.Count > 0)
			{
				OutputFiles = ToolChain.CompileCPPFiles(CompileEnvironment, NormalFiles, Name, ActionGraph);
			}

			if (AdaptiveFiles.Count > 0)
			{
				// Create the new compile environment. Always turn off PCH due to different compiler settings.
				CppCompileEnvironment AdaptiveUnityEnvironment = new CppCompileEnvironment(ModuleCompileEnvironment);
				if(Target.bAdaptiveUnityDisablesOptimizations)
				{
					AdaptiveUnityEnvironment.bOptimizeCode = false;
				}
				AdaptiveUnityEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.None;

				// Compile the files
				CPPOutput AdaptiveOutput = ToolChain.CompileCPPFiles(AdaptiveUnityEnvironment, AdaptiveFiles, Name, ActionGraph);

				// Merge output
				OutputFiles.ObjectFiles.AddRange(AdaptiveOutput.ObjectFiles);
				OutputFiles.DebugDataFiles.AddRange(AdaptiveOutput.DebugDataFiles);
			}

			return OutputFiles;
		}

		/// <summary>
		/// Create a header file containing the module definitions, which also includes the PCH itself. Including through another file is necessary on 
		/// Clang, since we get warnings about #pragma once otherwise, but it also allows us to consistently define the preprocessor state on all 
		/// platforms.
		/// </summary>
		/// <param name="OutputFile">The output file to create</param>
		/// <param name="Definitions">Definitions required by the PCH</param>
		/// <param name="IncludedFile">The PCH file to include</param>
		/// <returns>FileItem for the created file</returns>
		static FileItem CreatePCHWrapperFile(FileReference OutputFile, IEnumerable<string> Definitions, FileItem IncludedFile)
		{
			// Build the contents of the wrapper file
			StringBuilder WrapperContents = new StringBuilder();
			using (StringWriter Writer = new StringWriter(WrapperContents))
			{
				Writer.WriteLine("// PCH for {0}", IncludedFile.AbsolutePath);
				WriteDefinitions(Definitions, Writer);
				Writer.WriteLine("#include \"{0}\"", IncludedFile.AbsolutePath);
			}

			// Create the item
			FileItem WrapperFile = FileItem.CreateIntermediateTextFile(OutputFile, WrapperContents.ToString());
			WrapperFile.CachedIncludePaths = IncludedFile.CachedIncludePaths;

			// Touch it if the included file is newer, to make sure our timestamp dependency checking is accurate.
			if (IncludedFile.LastWriteTime > WrapperFile.LastWriteTime)
			{
				File.SetLastWriteTimeUtc(WrapperFile.AbsolutePath, DateTime.UtcNow);
				WrapperFile.ResetFileInfo();
			}
			return WrapperFile;
		}

		/// <summary>
		/// Write a list of macro definitions to an output file
		/// </summary>
		/// <param name="Definitions">List of definitions</param>
		/// <param name="Writer">Writer to receive output</param>
		static void WriteDefinitions(IEnumerable<string> Definitions, TextWriter Writer)
		{
			foreach(string Definition in Definitions)
			{
				int EqualsIdx = Definition.IndexOf('=');
				if(EqualsIdx == -1)
				{
					Writer.WriteLine("#define {0} 1", Definition);
				}
				else
				{
					Writer.WriteLine("#define {0} {1}", Definition.Substring(0, EqualsIdx), Definition.Substring(EqualsIdx + 1));
				}
			}
		}

		public static FileItem CachePCHUsageForModuleSourceFile(CppCompileEnvironment ModuleCompileEnvironment, FileItem CPPFile)
		{
			if (!CPPFile.bExists)
			{
				throw new BuildException("Required source file not found: " + CPPFile.AbsolutePath);
			}

			DateTime PCHCacheTimerStart = DateTime.UtcNow;

			// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking
			// for header dependencies
			CPPFile.CachedIncludePaths = ModuleCompileEnvironment.IncludePaths;

			FileItem PCHFile = ModuleCompileEnvironment.Headers.CachePCHUsageForCPPFile(CPPFile, ModuleCompileEnvironment.IncludePaths, ModuleCompileEnvironment.Platform);

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				double PCHCacheTime = (DateTime.UtcNow - PCHCacheTimerStart).TotalSeconds;
				TotalPCHCacheTime += PCHCacheTime;
			}

			return PCHFile;
		}


		public void CachePCHUsageForModuleSourceFiles(ReadOnlyTargetRules Target, CppCompileEnvironment ModuleCompileEnvironment)
		{
			if(Rules == null || Rules.PCHUsage == ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs || Rules.PrivatePCHHeaderFile != null)
			{
				if(InvalidIncludeDirectiveMessages == null)
				{
					// Find all the source files in this module
					List<FileReference> ModuleFiles = SourceFileSearch.FindModuleSourceFiles(RulesFile);

					// Find headers used by the source file.
					Dictionary<string, FileReference> NameToHeaderFile = new Dictionary<string, FileReference>();
					foreach(FileReference ModuleFile in ModuleFiles)
					{
						if(ModuleFile.HasExtension(".h"))
						{
							NameToHeaderFile[ModuleFile.GetFileNameWithoutExtension()] = ModuleFile;
						}
					}

					// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking for header dependencies
					foreach (FileItem CPPFile in SourceFilesFound.CPPFiles)
					{
						CPPFile.CachedIncludePaths = ModuleCompileEnvironment.IncludePaths;
					}

					// Find the directly included files for each source file, and make sure it includes the matching header if possible
					InvalidIncludeDirectiveMessages = new List<string>();
					if (Rules != null && Rules.bEnforceIWYU && Target.bEnforceIWYU)
					{
						foreach (FileItem CPPFile in SourceFilesFound.CPPFiles)
						{
							List<DependencyInclude> DirectIncludeFilenames = ModuleCompileEnvironment.Headers.GetDirectIncludeDependencies(CPPFile, bOnlyCachedDependencies: false);
							if (DirectIncludeFilenames.Count > 0)
							{
								string IncludeName = Path.GetFileNameWithoutExtension(DirectIncludeFilenames[0].IncludeName);
								string ExpectedName = CPPFile.Reference.GetFileNameWithoutExtension();
								if (String.Compare(IncludeName, ExpectedName, StringComparison.InvariantCultureIgnoreCase) != 0)
								{
									FileReference HeaderFile;
									if (NameToHeaderFile.TryGetValue(ExpectedName, out HeaderFile) && !IgnoreMismatchedHeader(ExpectedName))
									{
										InvalidIncludeDirectiveMessages.Add(String.Format("{0}(1): error: Expected {1} to be first header included.", CPPFile.Reference, HeaderFile.GetFileName()));
									}
								}
							}
						}
					}
				}
			}
			else
			{
				if (ProcessedDependencies == null)
				{
					DateTime PCHCacheTimerStart = DateTime.UtcNow;

					bool bFoundAProblemWithPCHs = false;

					FileItem UniquePCH = null;
					foreach (FileItem CPPFile in SourceFilesFound.CPPFiles)	// @todo ubtmake: We're not caching CPPEnvironments for .c/.mm files, etc.  Even though they don't use PCHs, they still have #includes!  This can break dependency checking!
					{
						// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking
						// for header dependencies
						CPPFile.CachedIncludePaths = ModuleCompileEnvironment.IncludePaths;

						// Find headers used by the source file.
						FileItem PCH = ModuleCompileEnvironment.Headers.CachePCHUsageForCPPFile(CPPFile, ModuleCompileEnvironment.IncludePaths, ModuleCompileEnvironment.Platform);
						if (PCH == null)
						{
							throw new BuildException("Source file \"{0}\" is not including any headers.  We expect all modules to include a header file for precompiled header generation.  Please add an #include statement.", CPPFile.AbsolutePath);
						}

						if (UniquePCH == null)
						{
							UniquePCH = PCH;
						}
						else if (!UniquePCH.Info.Name.Equals(PCH.Info.Name, StringComparison.InvariantCultureIgnoreCase))		// @todo ubtmake: We do a string compare on the file name (not path) here, because sometimes the include resolver will pick an Intermediate copy of a PCH header file and throw off our comparisons
						{
							// OK, looks like we have multiple source files including a different header file first.  We'll keep track of this and print out
							// helpful information afterwards.
							bFoundAProblemWithPCHs = true;
						}
					}

					ProcessedDependencies = new ProcessedDependenciesClass { UniquePCHHeaderFile = UniquePCH };


					if (bFoundAProblemWithPCHs)
					{
						// Map from pch header string to the source files that use that PCH
						Dictionary<FileReference, List<FileItem>> UsageMapPCH = new Dictionary<FileReference, List<FileItem>>();
						foreach (FileItem CPPFile in SourceFilesToBuild.CPPFiles)
						{
							// Create a new entry if not in the pch usage map
							List<FileItem> Files;
							if (!UsageMapPCH.TryGetValue(CPPFile.PrecompiledHeaderIncludeFilename, out Files))
							{
								Files = new List<FileItem>();
								UsageMapPCH.Add(CPPFile.PrecompiledHeaderIncludeFilename, Files);
							}
							Files.Add(CPPFile);
						}

						if (UnrealBuildTool.bPrintDebugInfo)
						{
							Log.TraceVerbose("{0} PCH files for module {1}:", UsageMapPCH.Count, Name);
							int MostFilesIncluded = 0;
							foreach (KeyValuePair<FileReference, List<FileItem>> CurPCH in UsageMapPCH)
							{
								if (CurPCH.Value.Count > MostFilesIncluded)
								{
									MostFilesIncluded = CurPCH.Value.Count;
								}

								Log.TraceVerbose("   {0}  ({1} files including it: {2}, ...)", CurPCH.Key, CurPCH.Value.Count, CurPCH.Value[0].AbsolutePath);
							}
						}

						if (UsageMapPCH.Count > 1)
						{
							// Keep track of the PCH file that is most used within this module
							FileReference MostFilesAreIncludingPCH = null;
							int MostFilesIncluded = 0;
							foreach (KeyValuePair<FileReference, List<FileItem>> CurPCH in UsageMapPCH.Where(PCH => PCH.Value.Count > MostFilesIncluded))
							{
								MostFilesAreIncludingPCH = CurPCH.Key;
								MostFilesIncluded = CurPCH.Value.Count;
							}

							// Find all of the files that are not including our "best" PCH header
							StringBuilder FilesNotIncludingBestPCH = new StringBuilder();
							foreach (KeyValuePair<FileReference, List<FileItem>> CurPCH in UsageMapPCH.Where(PCH => PCH.Key != MostFilesAreIncludingPCH))
							{
								foreach (FileItem SourceFile in CurPCH.Value)
								{
									FilesNotIncludingBestPCH.AppendFormat("{0} (including {1})\n", SourceFile.AbsolutePath, CurPCH.Key);
								}
							}

							// Bail out and let the user know which source files may need to be fixed up
							throw new BuildException(
								"All source files in module \"{0}\" must include the same precompiled header first.  Currently \"{1}\" is included by most of the source files.  The following source files are not including \"{1}\" as their first include:\n\n{2}\n\nTo compile this module without implicit precompiled headers, add \"PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;\" to {0}.build.cs.",
								Name,
								MostFilesAreIncludingPCH,
								FilesNotIncludingBestPCH);
						}
					}

					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						double PCHCacheTime = (DateTime.UtcNow - PCHCacheTimerStart).TotalSeconds;
						TotalPCHCacheTime += PCHCacheTime;
					}
				}
			}
		}

		private bool IgnoreMismatchedHeader(string ExpectedName)
		{
			switch(ExpectedName)
			{
				case "Stats2":
				case "DynamicRHI":
				case "RHICommandList":
				case "RHIUtilities":
					return true;
			}
			switch(Name)
			{
				case "D3D11RHI":
				case "D3D12RHI":
				case "VulkanRHI":
				case "OpenGLDrv":
				case "MetalRHI":
				case "PS4RHI":
                case "Gnmx":
				case "OnlineSubsystemIOS":
				case "OnlineSubsystemLive":
					return true;
			}
			return false;
		}

		/// <summary>
		/// Determine whether optimization should be enabled for a given target
		/// </summary>
		/// <param name="Setting">The optimization setting from the rules file</param>
		/// <param name="Configuration">The active target configuration</param>
		/// <param name="bIsEngineModule">Whether the current module is an engine module</param>
		/// <returns>True if optimization should be enabled</returns>
		public static bool ShouldEnableOptimization(ModuleRules.CodeOptimization Setting, UnrealTargetConfiguration Configuration, bool bIsEngineModule)
		{
			switch(Setting)
			{
				case ModuleRules.CodeOptimization.Never:
					return false;
				case ModuleRules.CodeOptimization.Default:
				case ModuleRules.CodeOptimization.InNonDebugBuilds:
					return (Configuration == UnrealTargetConfiguration.Debug)? false : (Configuration != UnrealTargetConfiguration.DebugGame || bIsEngineModule);
				case ModuleRules.CodeOptimization.InShippingBuildsOnly:
					return (Configuration == UnrealTargetConfiguration.Shipping);
				default:
					return true;
			}
		}

		/// <summary>
		/// Determines whether the current module is an engine module, as opposed to a target-specific module
		/// </summary>
		/// <returns>True if it is an engine module, false otherwise</returns>
		public bool IsEngineModule()
		{
			return UnrealBuildTool.IsUnderAnEngineDirectory(ModuleDirectory) && !ModuleDirectory.IsUnderDirectory(UnrealBuildTool.EngineSourceProgramsDirectory) && Name != "UE4Game" &&
				!ModuleDirectory.IsUnderDirectory(DirectoryReference.Combine(UnrealBuildTool.EnterpriseSourceDirectory, "Programs"));
		}

		/// <summary>
		/// Creates a compile environment from a base environment based on the module settings.
		/// </summary>
		/// <param name="Target">Rules for the target being built</param>
		/// <param name="BaseCompileEnvironment">An existing environment to base the module compile environment on.</param>
		/// <returns>The new module compile environment.</returns>
		public CppCompileEnvironment CreateModuleCompileEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment BaseCompileEnvironment)
		{
			CppCompileEnvironment Result = new CppCompileEnvironment(BaseCompileEnvironment);

			if (Binary == null)
			{
				// Adding this check here as otherwise the call to Binary.Config.IntermediateDirectory will give an 
				// unhandled exception
				throw new BuildException("Module {0} is required by this target, but is not compiled into any binary. Check any references to it are correct for this configuration, and whether it can be built.", this.ToString());
			}

			// Check if this is an engine module
			bool bIsEngineModule = IsEngineModule();

			// Override compile environment
			Result.bFasterWithoutUnity = Rules.bFasterWithoutUnity;
			Result.bOptimizeCode = ShouldEnableOptimization(Rules.OptimizeCode, Target.Configuration, bIsEngineModule);
			Result.bUseRTTI = Rules.bUseRTTI || Target.bForceEnableRTTI; 
			Result.bUseAVX = Rules.bUseAVX;
			Result.bEnableBufferSecurityChecks = Rules.bEnableBufferSecurityChecks;
			Result.MinSourceFilesForUnityBuildOverride = Rules.MinSourceFilesForUnityBuildOverride;
			Result.MinFilesUsingPrecompiledHeaderOverride = Rules.MinFilesUsingPrecompiledHeaderOverride;
			Result.bBuildLocallyWithSNDBS = Rules.bBuildLocallyWithSNDBS;
			Result.bEnableExceptions |= Rules.bEnableExceptions;
			Result.bEnableObjCExceptions |= Rules.bEnableObjCExceptions;
			Result.bEnableShadowVariableWarnings = Rules.bEnableShadowVariableWarnings;
			Result.bEnableUndefinedIdentifierWarnings = Rules.bEnableUndefinedIdentifierWarnings;
			Result.bUseStaticCRT = Target.bUseStaticCRT;
			Result.OutputDirectory = DirectoryReference.Combine(Binary.Config.IntermediateDirectory, Rules.ShortName ?? Name);
			Result.PCHOutputDirectory = (Result.PCHOutputDirectory == null)? null : DirectoryReference.Combine(Result.PCHOutputDirectory, Name);

			// Set the macro used to check whether monolithic headers can be used
			if (bIsEngineModule && (!Rules.bEnforceIWYU || !Target.bEnforceIWYU))
			{
				Result.Definitions.Add("SUPPRESS_MONOLITHIC_HEADER_WARNINGS=1");
			}

			// Add a macro for when we're compiling an engine module, to enable additional compiler diagnostics through code.
			if (bIsEngineModule)
			{
				Result.Definitions.Add("UE_IS_ENGINE_MODULE=1");
			}
			else
			{
				Result.Definitions.Add("UE_IS_ENGINE_MODULE=0");
			}

			// Switch the optimization flag if we're building a game module. Also pass the definition for building in DebugGame along (see ModuleManager.h for notes).
			if (!bIsEngineModule)
			{
				if (Target.Configuration == UnrealTargetConfiguration.DebugGame)
				{
					Result.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=1");
				}
				else
				{
					Result.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=0");
				}
			}

			// For game modules, set the define for the project name. This will be used by the IMPLEMENT_PRIMARY_GAME_MODULE macro.
			if (!bIsEngineModule)
			{
				if (Target.ProjectFile != null)
				{
					string ProjectName = Target.ProjectFile.GetFileNameWithoutExtension();
					Result.Definitions.Add(String.Format("UE_PROJECT_NAME={0}", ProjectName));
				}
			}

			// Add the module's private definitions.
			Result.Definitions.AddRange(Definitions);

			// Setup the compile environment for the module.
			SetupPrivateCompileEnvironment(Result.IncludePaths.UserIncludePaths, Result.IncludePaths.SystemIncludePaths, Result.Definitions, Result.AdditionalFrameworks);

			// @hack to skip adding definitions to compile environment, they will be baked into source code files
			if (bSkipDefinitionsForCompileEnvironment)
			{
				Result.Definitions.Clear();
				Result.IncludePaths.UserIncludePaths = new HashSet<string>(BaseCompileEnvironment.IncludePaths.UserIncludePaths);
			}

			return Result;
		}

		/// <summary>
		/// Creates a compile environment for a shared PCH from a base environment based on the module settings.
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="BaseCompileEnvironment">An existing environment to base the module compile environment on.</param>
		/// <returns>The new shared PCH compile environment.</returns>
		public CppCompileEnvironment CreateSharedPCHCompileEnvironment(UEBuildTarget Target, CppCompileEnvironment BaseCompileEnvironment)
		{
			CppCompileEnvironment CompileEnvironment = new CppCompileEnvironment(BaseCompileEnvironment);

			// Use the default optimization setting for 
			bool bIsEngineModule = IsEngineModule();
			CompileEnvironment.bOptimizeCode = ShouldEnableOptimization(ModuleRules.CodeOptimization.Default, Target.Configuration, bIsEngineModule);

			// Override compile environment
			CompileEnvironment.bIsBuildingDLL = !Target.ShouldCompileMonolithic();
			CompileEnvironment.bIsBuildingLibrary = false;
			CompileEnvironment.bUseStaticCRT = (Target.Rules != null && Target.Rules.bUseStaticCRT);
			CompileEnvironment.OutputDirectory = DirectoryReference.Combine(Binary.Config.IntermediateDirectory, Name);
			CompileEnvironment.PCHOutputDirectory = (CompileEnvironment.PCHOutputDirectory == null)? null : DirectoryReference.Combine(CompileEnvironment.PCHOutputDirectory, Name);

			// Add a macro for when we're compiling an engine module, to enable additional compiler diagnostics through code.
			if (bIsEngineModule)
			{
				CompileEnvironment.Definitions.Add("UE_IS_ENGINE_MODULE=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("UE_IS_ENGINE_MODULE=0");
			}

			// Switch the optimization flag if we're building a game module. Also pass the definition for building in DebugGame along (see ModuleManager.h for notes).
			if (!bIsEngineModule)
			{
				if (Target.Configuration == UnrealTargetConfiguration.DebugGame)
				{
					CompileEnvironment.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=1");
				}
				else
				{
					CompileEnvironment.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=0");
				}
			}

			// Add the module's private definitions.
			CompileEnvironment.Definitions.AddRange(Definitions);

			// Find all the modules that are part of the public compile environment for this module.
			List<UEBuildModule> Modules = new List<UEBuildModule>();
			Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag = new Dictionary<UEBuildModule, bool>();
			FindModulesInPublicCompileEnvironment(Modules, ModuleToIncludePathsOnlyFlag);

			// Now set up the compile environment for the modules in the original order that we encountered them
			foreach (UEBuildModule Module in Modules)
			{
				Module.AddModuleToCompileEnvironment(null, ModuleToIncludePathsOnlyFlag[Module], CompileEnvironment.IncludePaths.UserIncludePaths, CompileEnvironment.IncludePaths.SystemIncludePaths, CompileEnvironment.Definitions, CompileEnvironment.AdditionalFrameworks);
			}
			return CompileEnvironment;
		}

		public class UHTModuleInfoCacheType
		{
			public UHTModuleInfoCacheType(IEnumerable<string> InHeaderFilenames, UHTModuleInfo InInfo)
			{
				HeaderFilenames = InHeaderFilenames;
				Info = InInfo;
			}

			public IEnumerable<string> HeaderFilenames = null;
			public UHTModuleInfo Info = null;
		}

		private UHTModuleInfoCacheType UHTModuleInfoCache = null;

		/// Total time spent generating PCHs for modules (not actually compiling, but generating the PCH's input data)
		public static double TotalPCHGenTime = 0.0;

		/// Time spent caching which PCH header is included by each module and source file
		public static double TotalPCHCacheTime = 0.0;


		/// <summary>
		/// If any of this module's source files contain UObject definitions, this will return those header files back to the caller
		/// </summary>
		/// <returns></returns>
		public UHTModuleInfoCacheType GetCachedUHTModuleInfo(EGeneratedCodeVersion GeneratedCodeVersion)
		{
			if (UHTModuleInfoCache == null)
			{
				List<FileReference> HeaderFiles = new List<FileReference>();
				FileSystemName[] ExcludedFolders = UEBuildPlatform.GetBuildPlatform(Rules.Target.Platform, true).GetExcludedFolderNames();
				FindHeaders(new DirectoryInfo(ModuleDirectory.FullName), ExcludedFolders, HeaderFiles);
				UHTModuleInfo Info = ExternalExecution.CreateUHTModuleInfo(HeaderFiles, Name, RulesFile, ModuleDirectory, Type, GeneratedCodeVersion);
				UHTModuleInfoCache = new UHTModuleInfoCacheType(Info.PublicUObjectHeaders.Concat(Info.PublicUObjectClassesHeaders).Concat(Info.PrivateUObjectHeaders).Select(x => x.AbsolutePath).ToList(), Info);
			}

			return UHTModuleInfoCache;
		}

		/// <summary>
		/// Find all the headers under the given base directory, excluding any other platform folders.
		/// </summary>
		/// <param name="BaseDir">Base directory to search</param>
		/// <param name="ExcludeFolders">Array of folders to exclude</param>
		/// <param name="Headers">Receives the list of headers that was found</param>
		static void FindHeaders(DirectoryInfo BaseDir, FileSystemName[] ExcludeFolders, List<FileReference> Headers)
		{
			if (!ExcludeFolders.Any(x => x.DisplayName.Equals(BaseDir.Name, StringComparison.InvariantCultureIgnoreCase)))
			{
				foreach (DirectoryInfo SubDir in BaseDir.EnumerateDirectories())
				{
					FindHeaders(SubDir, ExcludeFolders, Headers);
				}
				foreach (FileInfo File in BaseDir.EnumerateFiles("*.h"))
				{
					Headers.Add(new FileReference(File));
				}
			}
		}

		public override void GetAllDependencyModules(List<UEBuildModule> ReferencedModules, HashSet<UEBuildModule> IgnoreReferencedModules, bool bIncludeDynamicallyLoaded, bool bForceCircular, bool bOnlyDirectDependencies)
		{
			List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>();
			AllDependencyModules.AddRange(PrivateDependencyModules);
			AllDependencyModules.AddRange(PublicDependencyModules);
			if (bIncludeDynamicallyLoaded)
			{
				AllDependencyModules.AddRange(DynamicallyLoadedModules);
				AllDependencyModules.AddRange(PlatformSpecificDynamicallyLoadedModules);
			}

			foreach (UEBuildModule DependencyModule in AllDependencyModules)
			{
				if (!IgnoreReferencedModules.Contains(DependencyModule))
				{
					// Don't follow circular back-references!
					bool bIsCircular = HasCircularDependencyOn(DependencyModule.Name);
					if (bForceCircular || !bIsCircular)
					{
						IgnoreReferencedModules.Add(DependencyModule);

						if (!bOnlyDirectDependencies)
						{
							// Recurse into dependent modules first
							DependencyModule.GetAllDependencyModules(ReferencedModules, IgnoreReferencedModules, bIncludeDynamicallyLoaded, bForceCircular, bOnlyDirectDependencies);
						}

						ReferencedModules.Add(DependencyModule);
					}
				}
			}
		}

		public override void RecursivelyAddPrecompiledModules(List<UEBuildModule> Modules)
		{
			if (!Modules.Contains(this))
			{
				Modules.Add(this);

				// Get the dependent modules
				List<UEBuildModule> DependentModules = new List<UEBuildModule>();
				if (PrivateDependencyModules != null)
				{
					DependentModules.AddRange(PrivateDependencyModules);
				}
				if (PublicDependencyModules != null)
				{
					DependentModules.AddRange(PublicDependencyModules);
				}
				if (DynamicallyLoadedModules != null)
				{
					DependentModules.AddRange(DynamicallyLoadedModules);
				}
				if (PlatformSpecificDynamicallyLoadedModules != null)
				{
					DependentModules.AddRange(PlatformSpecificDynamicallyLoadedModules);
				}

				// Find modules for each of them, and add their dependencies too
				foreach (UEBuildModule DependentModule in DependentModules)
				{
					DependentModule.RecursivelyAddPrecompiledModules(Modules);
				}
			}
		}
	}
}
