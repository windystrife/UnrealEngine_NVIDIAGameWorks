// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.CodeDom.Compiler;
using Microsoft.CSharp;
using Microsoft.Win32;
using System.Reflection;
using System.Diagnostics;
using UnrealBuildTool;
using Tools.DotNETCommon;

namespace AutomationTool
{
	/// <summary>
	/// Exception thrown by PreprocessScriptFile.
	/// </summary>
	class CompilationException : AutomationException
	{
		public CompilationException(string Filename, int StartLine, int StartColumn, int EndLine, int EndColumn, string Message, params string[] Args)
			: base(String.Format("Compilation Failed.\n>{0}({1},{2},{3},{4}): error: {5}", Path.GetFullPath(Filename), StartLine + 1, StartColumn + 1, EndLine + 1, EndColumn + 1,
			String.Format(Message, Args)))
		{
		}
	}

	/// <summary>
	/// Compiles and loads script assemblies.
	/// </summary>
	class ScriptCompiler
	{
		#region Fields
				
		private Dictionary<string, Type> ScriptCommands;
#if DEBUG
		const string BuildConfig = "Debug";
#else
		const string BuildConfig = "Development";
#endif
		const string DefaultScriptsDLLName = "AutomationScripts.Automation.dll";

		#endregion

		#region Compilation

		public ScriptCompiler()
		{
		}


		/// <summary>
		/// Finds and/or compiles all script files and assemblies.
		/// </summary>
		/// <param name="ScriptsForProjectFileName">Path to the current project. May be null, in which case we compile scripts for all projects.</param>
		/// <param name="AdditionalScriptsFolders">Additional script fodlers to look for source files in.</param>
		public void FindAndCompileAllScripts(string ScriptsForProjectFileName, List<string> AdditionalScriptsFolders)
		{
			bool DoCompile = false;
			if (GlobalCommandLine.Compile)
			{
				DoCompile = true;
			}

			// Compile only if not disallowed.
			if (DoCompile && !String.IsNullOrEmpty(CommandUtils.CmdEnv.MsBuildExe))
			{
				CleanupScriptsAssemblies();
				FindAndCompileScriptModules(ScriptsForProjectFileName, AdditionalScriptsFolders);
			}

			var ScriptAssemblies = new List<Assembly>();
			LoadPreCompiledScriptAssemblies(ScriptAssemblies);

			// Setup platforms
			Platform.InitializePlatforms(ScriptAssemblies.ToArray());

			// Instantiate all the automation classes for interrogation
			Log.TraceVerbose("Creating commands.");
			ScriptCommands = new Dictionary<string, Type>(StringComparer.InvariantCultureIgnoreCase);
			foreach (var CompiledScripts in ScriptAssemblies)
			{
				try
				{
					foreach (var ClassType in CompiledScripts.GetTypes())
					{
						if (ClassType.IsSubclassOf(typeof(BuildCommand)) && ClassType.IsAbstract == false)
						{
							if (ScriptCommands.ContainsKey(ClassType.Name) == false)
							{
								ScriptCommands.Add(ClassType.Name, ClassType);
							}
							else
							{
								Log.TraceWarning("Unable to add command {0} twice. Previous: {1}, Current: {2}", ClassType.Name,
									ClassType.AssemblyQualifiedName, ScriptCommands[ClassType.Name].AssemblyQualifiedName);
							}
						}
					}
				}
				catch (Exception Ex)
				{
					throw new AutomationException("Failed to add commands from {0}. {1}", CompiledScripts, Ex);
				}
			}
		}

		private static void FindAndCompileScriptModules(string ScriptsForProjectFileName, List<string> AdditionalScriptsFolders)
		{
			Log.TraceInformation("Compiling scripts.");

			var OldCWD = Environment.CurrentDirectory;
			var UnrealBuildToolCWD = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source");

			Environment.CurrentDirectory = UnrealBuildToolCWD;

			// Configure the rules compiler
			// Get all game folders and convert them to build subfolders.
			List<DirectoryReference> AllGameFolders;
			if(ScriptsForProjectFileName == null)
			{
				AllGameFolders = UProjectInfo.AllProjectFiles.Select(x => x.Directory).ToList();
			}
			else
			{
				AllGameFolders = new List<DirectoryReference>{ new DirectoryReference(Path.GetDirectoryName(ScriptsForProjectFileName)) };
			}

			var AllAdditionalScriptFolders = new List<DirectoryReference>(AdditionalScriptsFolders.Select(x => new DirectoryReference(x)));
			foreach (var Folder in AllGameFolders)
			{
				var GameBuildFolder = DirectoryReference.Combine(Folder, "Build");
				if (DirectoryReference.Exists(GameBuildFolder))
				{
					AllAdditionalScriptFolders.Add(GameBuildFolder);
				}
			}

			Log.TraceVerbose("Discovering game folders.");

			var DiscoveredModules = UnrealBuildTool.RulesCompiler.FindAllRulesSourceFiles(UnrealBuildTool.RulesCompiler.RulesFileType.AutomationModule, GameFolders: AllGameFolders, ForeignPlugins: null, AdditionalSearchPaths: AllAdditionalScriptFolders);
			var ModulesToCompile = new List<string>(DiscoveredModules.Count);
			foreach (var ModuleFilename in DiscoveredModules)
			{
				if (HostPlatform.Current.IsScriptModuleSupported(ModuleFilename.GetFileNameWithoutAnyExtensions()))
				{
					ModulesToCompile.Add(ModuleFilename.FullName);
				}
				else
				{
					CommandUtils.LogVerbose("Script module {0} filtered by the Host Platform and will not be compiled.", ModuleFilename);
				}
			}

			if ((UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealBuildTool.UnrealTargetPlatform.Win64) ||
				(UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealBuildTool.UnrealTargetPlatform.Win32))
			{
				string Modules = string.Join(";", ModulesToCompile.ToArray());
				var UATProj = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine\Source\Programs\AutomationTool\Scripts\UAT.proj");
				var CmdLine = String.Format("\"{0}\" /p:Modules=\"{1}\" /p:Configuration={2} /verbosity:minimal /nologo", UATProj, Modules, BuildConfig);
				// suppress the run command because it can be long and intimidating, making the logs around this code harder to read.
				var Result = CommandUtils.Run(CommandUtils.CmdEnv.MsBuildExe, CmdLine, Options: CommandUtils.ERunOptions.Default | CommandUtils.ERunOptions.NoLoggingOfRunCommand | CommandUtils.ERunOptions.LoggingOfRunDuration);
				if (Result.ExitCode != 0)
				{
					throw new AutomationException(String.Format("Failed to build \"{0}\":{1}{2}", UATProj, Environment.NewLine, Result.Output));
				}
			}
			else
			{
				CompileModules(ModulesToCompile);
			}
			

			Environment.CurrentDirectory = OldCWD;
		}

		/// <summary>
		/// Compiles all script modules.
		/// </summary>
		/// <param name="Modules">Module project filenames.</param>
		/// <param name="CompiledModuleFilenames">The resulting compiled module assembly filenames.</param>
		private static void CompileModules(List<string> Modules)
		{
			Log.TraceInformation("Building script modules");
			// Make sure DefaultScriptsDLLName is compiled first
			var DefaultScriptsProjName = Path.ChangeExtension(DefaultScriptsDLLName, "csproj");
			foreach (var ModuleName in Modules)
			{
				if (ModuleName.IndexOf(DefaultScriptsProjName, StringComparison.InvariantCultureIgnoreCase) >= 0)
				{
					Log.TraceInformation("Building script module: {0}", ModuleName);
					try
					{
						CompileScriptModule(ModuleName);
					}
					catch (Exception Ex)
					{
						CommandUtils.LogError(LogUtils.FormatException(Ex));
						throw new AutomationException("Failed to compile module {0}", ModuleName);
					}
					break;
				}
			}

			// Second pass, compile everything else
			foreach (var ModuleName in Modules)
			{
				if (ModuleName.IndexOf(DefaultScriptsProjName, StringComparison.InvariantCultureIgnoreCase) < 0)
				{
					Log.TraceInformation("Building script module: {0}", ModuleName);
					try
					{
						CompileScriptModule(ModuleName);
					}
					catch (Exception Ex)
					{
						CommandUtils.LogError(LogUtils.FormatException(Ex));
						throw new AutomationException("Failed to compile module {0}", ModuleName);
					}
				}
			}
		}

		/// <summary>
		/// Builds a script module (csproj file)
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <returns></returns>
		private static bool CompileScriptModule(string ProjectFile)
		{
			if (!ProjectFile.EndsWith(".csproj", StringComparison.InvariantCultureIgnoreCase))
			{
				throw new AutomationException(String.Format("Unable to build Project {0}. Not a valid .csproj file.", ProjectFile));
			}
			if (!CommandUtils.FileExists(ProjectFile))
			{
				throw new AutomationException(String.Format("Unable to build Project {0}. Project file not found.", ProjectFile));
			}

			var CmdLine = String.Format("\"{0}\" /verbosity:quiet /nologo /target:Build /property:Configuration={1} /property:Platform=AnyCPU /p:TreatWarningsAsErrors=false /p:NoWarn=\"612,618,672\" /p:BuildProjectReferences=true",
				ProjectFile, BuildConfig);

			// Compile the project
			var Result = CommandUtils.Run(CommandUtils.CmdEnv.MsBuildExe, CmdLine);
			if (Result.ExitCode != 0)
			{
				throw new AutomationException(String.Format("Failed to build \"{0}\":{1}{2}", ProjectFile, Environment.NewLine, Result.Output));
			}
			else
			{
				// Remove .Automation.csproj and copy to target dir
				Log.TraceVerbose("Successfully compiled {0}", ProjectFile);
			}
			return Result.ExitCode == 0;
		}

		/// <summary>
		/// Loads all precompiled assemblies (DLLs that end with *Scripts.dll).
		/// </summary>
		/// <param name="OutScriptAssemblies">List to store all loaded assemblies.</param>
		private static void LoadPreCompiledScriptAssemblies(List<Assembly> OutScriptAssemblies)
		{
			CommandUtils.LogVerbose("Loading precompiled script DLLs");

			bool DefaultScriptsDLLFound = false;
			var ScriptsLocation = GetScriptAssemblyFolder();
			if (CommandUtils.DirectoryExists(ScriptsLocation))
			{
				var ScriptDLLFiles = Directory.GetFiles(ScriptsLocation, "*.Automation.dll", SearchOption.AllDirectories);

				CommandUtils.LogVerbose("Found {0} script DLL(s).", ScriptDLLFiles.Length);
				foreach (var ScriptsDLLFilename in ScriptDLLFiles)
				{

					if (!HostPlatform.Current.IsScriptModuleSupported(CommandUtils.GetFilenameWithoutAnyExtensions(ScriptsDLLFilename)))
					{
						CommandUtils.LogVerbose("Script module {0} filtered by the Host Platform and will not be loaded.", ScriptsDLLFilename);
						continue;
					}
					// Load the assembly into our app domain
					CommandUtils.LogVerbose("Loading script DLL: {0}", ScriptsDLLFilename);
					try
					{
						var Dll = AppDomain.CurrentDomain.Load(AssemblyName.GetAssemblyName(ScriptsDLLFilename));
						OutScriptAssemblies.Add(Dll);
						// Check if this is the default scripts DLL.
						if (!DefaultScriptsDLLFound && String.Compare(Path.GetFileName(ScriptsDLLFilename), DefaultScriptsDLLName, true) == 0)
						{
							DefaultScriptsDLLFound = true;
						}
					}
					catch (Exception Ex)
					{
						throw new AutomationException("Failed to load script DLL: {0}: {1}", ScriptsDLLFilename, Ex.Message);
					}
				}
			}
			else
			{
				CommandUtils.LogError("Scripts folder {0} does not exist!", ScriptsLocation);
			}

			// The default scripts DLL is required!
			if (!DefaultScriptsDLLFound)
			{
				throw new AutomationException("{0} was not found or could not be loaded, can't run scripts.", DefaultScriptsDLLName);
			}
		}

		private void CleanupScriptsAssemblies()
		{
			CommandUtils.LogVerbose("Cleaning up script DLL folder");
			CommandUtils.DeleteDirectory(GetScriptAssemblyFolder());

			// Bug in PortalPublishingTool caused these DLLs to be copied into Engine/Binaries/DotNET. Delete any files left over.
			DirectoryReference BinariesDir = DirectoryReference.Combine(CommandUtils.RootDirectory, "Engine", "Binaries", "DotNET");
			foreach (FileReference FileToDelete in DirectoryReference.EnumerateFiles(BinariesDir, "*.automation.dll"))
			{
				CommandUtils.DeleteFile(FileToDelete.FullName);
			}
			foreach (FileReference FileToDelete in DirectoryReference.EnumerateFiles(BinariesDir, "*.automation.pdb"))
			{
				CommandUtils.DeleteFile(FileToDelete.FullName);
			}
		}

		private static string GetScriptAssemblyFolder()
		{
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Binaries", "DotNET", "AutomationScripts");
		}

		#endregion

		#region Properties

		public Dictionary<string, Type> Commands
		{
			get { return ScriptCommands; }
		}

		#endregion
	}
}
