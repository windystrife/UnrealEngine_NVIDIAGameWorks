// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Linq;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

[Help("Builds a plugin, and packages it for distribution")]
[Help("Plugin", "Specify the path to the descriptor file for the plugin that should be packaged")]
[Help("NoHostPlatform", "Prevent compiling for the editor platform on the host")]
[Help("TargetPlatforms", "Specify a list of target platforms to build, separated by '+' characters (eg. -TargetPlatforms=Win32+Win64). Default is all the Rocket target platforms.")]
[Help("Package", "The path which the build artifacts should be packaged to, ready for distribution.")]
[Help("Unversioned", "Do not embed the current engine version into the descriptor")]
class BuildPlugin : BuildCommand
{
	public override void ExecuteBuild()
	{
		// Get the plugin filename
		string PluginParam = ParseParamValue("Plugin");
		if(PluginParam == null)
		{
			throw new AutomationException("Missing -Plugin=... argument");
		}

		// Check it exists
		FileReference PluginFile = new FileReference(PluginParam);
		if (!FileReference.Exists(PluginFile))
		{
			throw new AutomationException("Plugin '{0}' not found", PluginFile.FullName);
		}

		// Get the output directory
		string PackageParam = ParseParamValue("Package");
		if (PackageParam == null)
		{
			throw new AutomationException("Missing -Package=... argument");
		}

		// Make sure the packaging directory is valid
		DirectoryReference PackageDir = new DirectoryReference(PackageParam);
		if (PluginFile.IsUnderDirectory(PackageDir))
		{
			throw new AutomationException("Packaged plugin output directory must be different to source");
		}
		if (PackageDir.IsUnderDirectory(DirectoryReference.Combine(CommandUtils.RootDirectory, "Engine")))
		{
			throw new AutomationException("Output directory for packaged plugin must be outside engine directory");
		}

		// Clear the output directory of existing stuff
		if (DirectoryReference.Exists(PackageDir))
		{
			CommandUtils.DeleteDirectoryContents(PackageDir.FullName);
		}
		else
		{
			DirectoryReference.CreateDirectory(PackageDir);
		}

		// Create a placeholder FilterPlugin.ini with instructions on how to use it
		FileReference SourceFilterFile = FileReference.Combine(PluginFile.Directory, "Config", "FilterPlugin.ini");
		if (!FileReference.Exists(SourceFilterFile))
		{
			List<string> Lines = new List<string>();
			Lines.Add("[FilterPlugin]");
			Lines.Add("; This section lists additional files which will be packaged along with your plugin. Paths should be listed relative to the root plugin directory, and");
			Lines.Add("; may include \"...\", \"*\", and \"?\" wildcards to match directories, files, and individual characters respectively.");
			Lines.Add(";");
			Lines.Add("; Examples:");
			Lines.Add(";    /README.txt");
			Lines.Add(";    /Extras/...");
			Lines.Add(";    /Binaries/ThirdParty/*.dll");
			DirectoryReference.CreateDirectory(SourceFilterFile.Directory);
			CommandUtils.WriteAllLines_NoExceptions(SourceFilterFile.FullName, Lines.ToArray());
		}

		// Create a host project for the plugin. For script generator plugins, we need to have UHT be able to load it, which can only happen if it's enabled in a project.
		FileReference HostProjectFile = FileReference.Combine(PackageDir, "HostProject", "HostProject.uproject");
		FileReference HostProjectPluginFile = CreateHostProject(HostProjectFile, PluginFile);

		// Read the plugin
		CommandUtils.Log("Reading plugin from {0}...", HostProjectPluginFile);
		PluginDescriptor Plugin = PluginDescriptor.FromFile(HostProjectPluginFile);

		// Compile the plugin for all the target platforms
		List<UnrealTargetPlatform> HostPlatforms = ParseParam("NoHostPlatform")? new List<UnrealTargetPlatform>() : new List<UnrealTargetPlatform> { BuildHostPlatform.Current.Platform };
		List<UnrealTargetPlatform> TargetPlatforms = GetTargetPlatforms(this, BuildHostPlatform.Current.Platform);
		FileReference[] BuildProducts = CompilePlugin(HostProjectFile, HostProjectPluginFile, Plugin, HostPlatforms, TargetPlatforms, "");

		// Package up the final plugin data
		PackagePlugin(HostProjectPluginFile, BuildProducts, PackageDir, ParseParam("unversioned"));

		// Remove the host project
		if(!ParseParam("NoDeleteHostProject"))
		{
			CommandUtils.DeleteDirectory(HostProjectFile.Directory.FullName);
		}
	}

	FileReference CreateHostProject(FileReference HostProjectFile, FileReference PluginFile)
	{
		DirectoryReference HostProjectDir = HostProjectFile.Directory;
		DirectoryReference.CreateDirectory(HostProjectDir);

		// Create the new project descriptor
		File.WriteAllText(HostProjectFile.FullName, "{ \"FileVersion\": 3, \"Plugins\": [ { \"Name\": \"" + PluginFile.GetFileNameWithoutExtension() + "\", \"Enabled\": true } ] }");

		// Get the plugin directory in the host project, and copy all the files in
		DirectoryReference HostProjectPluginDir = DirectoryReference.Combine(HostProjectDir, "Plugins", PluginFile.GetFileNameWithoutExtension());
		CommandUtils.ThreadedCopyFiles(PluginFile.Directory.FullName, HostProjectPluginDir.FullName);
		CommandUtils.DeleteDirectory(true, DirectoryReference.Combine(HostProjectPluginDir, "Intermediate").FullName);

		// Return the path to the plugin file in the host project
		return FileReference.Combine(HostProjectPluginDir, PluginFile.GetFileName());
	}

	FileReference[] CompilePlugin(FileReference HostProjectFile, FileReference HostProjectPluginFile, PluginDescriptor Plugin, List<UnrealTargetPlatform> HostPlatforms, List<UnrealTargetPlatform> TargetPlatforms, string AdditionalArgs)
	{
		List<FileReference> ReceiptFileNames = new List<FileReference>();

		// Build the host platforms
		if(HostPlatforms.Count > 0)
		{
			CommandUtils.Log("Building plugin for host platforms: {0}", String.Join(", ", HostPlatforms));
			foreach (UnrealTargetPlatform HostPlatform in HostPlatforms)
			{
				if (Plugin.bCanBeUsedWithUnrealHeaderTool)
				{
					CompilePluginWithUBT(null, HostProjectPluginFile, Plugin, "UnrealHeaderTool", TargetType.Program, HostPlatform, UnrealTargetConfiguration.Development, ReceiptFileNames, String.Format("{0} -plugin {1}", AdditionalArgs, CommandUtils.MakePathSafeToUseWithCommandLine(HostProjectPluginFile.FullName)));
				}
				CompilePluginWithUBT(HostProjectFile, HostProjectPluginFile, Plugin, "UE4Editor", TargetType.Editor, HostPlatform, UnrealTargetConfiguration.Development, ReceiptFileNames, AdditionalArgs);
			}
		}

		// Add the game targets
		if(TargetPlatforms.Count > 0)
		{
			CommandUtils.Log("Building plugin for target platforms: {0}", String.Join(", ", TargetPlatforms));
			foreach (UnrealTargetPlatform TargetPlatform in TargetPlatforms)
			{
				CompilePluginWithUBT(HostProjectFile, HostProjectPluginFile, Plugin, "UE4Game", TargetType.Game, TargetPlatform, UnrealTargetConfiguration.Development, ReceiptFileNames, null);
				CompilePluginWithUBT(HostProjectFile, HostProjectPluginFile, Plugin, "UE4Game", TargetType.Game, TargetPlatform, UnrealTargetConfiguration.Shipping, ReceiptFileNames, null);
			}
		}

		// Package the plugin to the output folder
		List<BuildProduct> BuildProducts = GetBuildProductsFromReceipts(CommandUtils.EngineDirectory, HostProjectFile.Directory, ReceiptFileNames);
		return BuildProducts.Select(x => x.Path).ToArray();
	}

	void CompilePluginWithUBT(FileReference HostProjectFile, FileReference HostProjectPluginFile, PluginDescriptor Plugin, string TargetName, TargetType TargetType, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, List<FileReference> ReceiptFileNames, string InAdditionalArgs)
	{
		// Find a list of modules that need to be built for this plugin
		List<string> ModuleNames = new List<string>();
		if (Plugin.Modules != null)
		{
			foreach (ModuleDescriptor Module in Plugin.Modules)
			{
				bool bBuildDeveloperTools = (TargetType == TargetType.Editor || TargetType == TargetType.Program);
				bool bBuildEditor = (TargetType == TargetType.Editor);
				bool bBuildRequiresCookedData = (TargetType != TargetType.Editor && TargetType != TargetType.Program);
				if (Module.IsCompiledInConfiguration(Platform, TargetType, bBuildDeveloperTools, bBuildEditor, bBuildRequiresCookedData))
				{
					ModuleNames.Add(Module.Name);
				}
			}
		}

		// Add these modules to the build agenda
		if(ModuleNames.Count > 0)
		{
			string Arguments = "-iwyu";// String.Format("-plugin {0}", CommandUtils.MakePathSafeToUseWithCommandLine(PluginFile.FullName));
			foreach(string ModuleName in ModuleNames)
			{
				Arguments += String.Format(" -module {0}", ModuleName);
			}

			string Architecture = PlatformExports.GetDefaultArchitecture(Platform, HostProjectFile);

			FileReference ReceiptFileName = TargetReceipt.GetDefaultPath(HostProjectPluginFile.Directory, TargetName, Platform, Configuration, Architecture);
			Arguments += String.Format(" -receipt {0}", CommandUtils.MakePathSafeToUseWithCommandLine(ReceiptFileName.FullName));
			ReceiptFileNames.Add(ReceiptFileName);
			
			if(!String.IsNullOrEmpty(InAdditionalArgs))
			{
				Arguments += InAdditionalArgs;
			}

			CommandUtils.RunUBT(CmdEnv, UE4Build.GetUBTExecutable(), String.Format("{0} {1} {2}{3} {4}", TargetName, Platform, Configuration, (HostProjectFile == null)? "" : String.Format(" -project=\"{0}\"", HostProjectFile.FullName), Arguments));
		}
	}

	static List<BuildProduct> GetBuildProductsFromReceipts(DirectoryReference EngineDir, DirectoryReference ProjectDir, List<FileReference> ReceiptFileNames)
	{
		List<BuildProduct> BuildProducts = new List<BuildProduct>();
		foreach(FileReference ReceiptFileName in ReceiptFileNames)
		{
			TargetReceipt Receipt;
			if(!TargetReceipt.TryRead(ReceiptFileName, EngineDir, ProjectDir, out Receipt))
			{
				throw new AutomationException("Missing or invalid target receipt ({0})", ReceiptFileName);
			}
			BuildProducts.AddRange(Receipt.BuildProducts);
		}
		return BuildProducts;
	}

	static void PackagePlugin(FileReference SourcePluginFile, IEnumerable<FileReference> BuildProducts, DirectoryReference TargetDir, bool bUnversioned)
	{
		DirectoryReference SourcePluginDir = SourcePluginFile.Directory;

		// Copy all the files to the output directory
		FileReference[] SourceFiles = FilterPluginFiles(SourcePluginFile, BuildProducts).ToArray();
		foreach(FileReference SourceFile in SourceFiles)
		{
			FileReference TargetFile = FileReference.Combine(TargetDir, SourceFile.MakeRelativeTo(SourcePluginDir));
			CommandUtils.CopyFile(SourceFile.FullName, TargetFile.FullName);
			CommandUtils.SetFileAttributes(TargetFile.FullName, ReadOnly: false);
		}

		// Get the output plugin filename
		FileReference TargetPluginFile = FileReference.Combine(TargetDir, SourcePluginFile.GetFileName());
		PluginDescriptor NewDescriptor = PluginDescriptor.FromFile(TargetPluginFile);
		NewDescriptor.bEnabledByDefault = null;
		NewDescriptor.bInstalled = true;
		if(!bUnversioned)
		{
			BuildVersion Version;
			if(BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				NewDescriptor.EngineVersion = String.Format("{0}.{1}.0", Version.MajorVersion, Version.MinorVersion);
			}
		}
		NewDescriptor.Save(TargetPluginFile.FullName);
	}

	static IEnumerable<FileReference> FilterPluginFiles(FileReference PluginFile, IEnumerable<FileReference> BuildProducts)
	{
		// Set up the default filter
		FileFilter Filter = new FileFilter();
		Filter.AddRuleForFile(PluginFile, PluginFile.Directory, FileFilterType.Include);
		Filter.AddRuleForFiles(BuildProducts, PluginFile.Directory, FileFilterType.Include);
		Filter.Include("/Binaries/ThirdParty/...");
		Filter.Include("/Resources/...");
		Filter.Include("/Content/...");
		Filter.Include("/Intermediate/Build/.../Inc/...");
		Filter.Include("/Source/...");

		// Add custom rules for each platform
		FileReference FilterFile = FileReference.Combine(PluginFile.Directory, "Config", "FilterPlugin.ini");
		if(FileReference.Exists(FilterFile))
		{
			CommandUtils.Log("Reading filter rules from {0}", FilterFile);
			Filter.ReadRulesFromFile(FilterFile, "FilterPlugin");
		}

		// Apply the standard exclusion rules
		Filter.ExcludeRestrictedFolders();

		// Apply the filter to the plugin directory
		return Filter.ApplyToDirectory(PluginFile.Directory, true);
	}

	static List<UnrealTargetPlatform> GetTargetPlatforms(BuildCommand Command, UnrealTargetPlatform HostPlatform)
	{
		List<UnrealTargetPlatform> TargetPlatforms = new List<UnrealTargetPlatform>();
		if(!Command.ParseParam("NoTargetPlatforms"))
		{
			// Only interested in building for Platforms that support code projects
			TargetPlatforms = PlatformExports.GetRegisteredPlatforms().Where(x => InstalledPlatformInfo.IsValidPlatform(x, EProjectType.Code)).ToList();

			// only build Mac on Mac
			if (HostPlatform != UnrealTargetPlatform.Mac && TargetPlatforms.Contains(UnrealTargetPlatform.Mac))
			{
				TargetPlatforms.Remove(UnrealTargetPlatform.Mac);
			}
			// only build Windows on Windows
			if (HostPlatform != UnrealTargetPlatform.Win64 && TargetPlatforms.Contains(UnrealTargetPlatform.Win64))
			{
				TargetPlatforms.Remove(UnrealTargetPlatform.Win64);
				TargetPlatforms.Remove(UnrealTargetPlatform.Win32);
			}
			// build Linux on Windows and Linux
			if (HostPlatform != UnrealTargetPlatform.Win64 && HostPlatform != UnrealTargetPlatform.Linux && TargetPlatforms.Contains(UnrealTargetPlatform.Linux))
			{
				TargetPlatforms.Remove(UnrealTargetPlatform.Linux);
			}

			// Remove any platforms that aren't enabled on the command line
			string TargetPlatformFilter = Command.ParseParamValue("TargetPlatforms", null);
			if(TargetPlatformFilter != null)
			{
				List<UnrealTargetPlatform> NewTargetPlatforms = new List<UnrealTargetPlatform>();
				foreach (string TargetPlatformName in TargetPlatformFilter.Split(new char[]{ '+' }, StringSplitOptions.RemoveEmptyEntries))
				{
					UnrealTargetPlatform TargetPlatform;
					if(!Enum.TryParse(TargetPlatformName, out TargetPlatform))
					{
						throw new AutomationException("Unknown target platform '{0}' specified on command line");
					}
					else if(TargetPlatforms.Contains(TargetPlatform))
					{
						NewTargetPlatforms.Add(TargetPlatform);
					}
				}
				TargetPlatforms = NewTargetPlatforms;
			}
		}
		return TargetPlatforms;
	}
}

