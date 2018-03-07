// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

[Help("Builds common tools used by the engine which are not part of typical editor or game builds. Useful when syncing source-only on GitHub.")]
[Help("platforms=<X>+<Y>+...", "Specifies on or more platforms to build for (defaults to the current host platform)")]
[Help("manifest=<Path>", "Writes a manifest of all the build products to the given path")]
public class BuildCommonTools : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("************************* BuildCommonTools");

		// Get the list of platform names
		string[] PlatformNames = ParseParamValue("platforms", BuildHostPlatform.Current.Platform.ToString()).Split('+');

		// Parse the platforms
		List<UnrealBuildTool.UnrealTargetPlatform> Platforms = new List<UnrealTargetPlatform>();
		foreach(string PlatformName in PlatformNames)
		{
			UnrealBuildTool.UnrealTargetPlatform Platform;
			if(!UnrealBuildTool.UnrealTargetPlatform.TryParse(PlatformName, true, out Platform))
			{
				throw new AutomationException("Unknown platform specified on command line - '{0}' - valid platforms are {1}", PlatformName, String.Join("/", Enum.GetNames(typeof(UnrealBuildTool.UnrealTargetPlatform))));
			}
			Platforms.Add(Platform);
		}

		// Add all the platforms if specified
		if(ParseParam("allplatforms"))
		{
			foreach(UnrealTargetPlatform Platform in Enum.GetValues(typeof(UnrealTargetPlatform)))
			{
				if(!Platforms.Contains(Platform))
				{
					Platforms.Add(Platform);
				}
			}
		}

		// Get the agenda
		List<string> ExtraBuildProducts = new List<string>();
		UE4Build.BuildAgenda Agenda = MakeAgenda(Platforms.ToArray(), ExtraBuildProducts);

		// Build everything. We don't want to touch version files for GitHub builds -- these are "programmer builds" and won't have a canonical build version
		UE4Build Builder = new UE4Build(this);
		Builder.Build(Agenda, InUpdateVersionFiles: false);

		// Add UAT and UBT to the build products
		Builder.AddUATFilesToBuildProducts();
		Builder.AddUBTFilesToBuildProducts();

		// Add all the extra build products
		foreach(string ExtraBuildProduct in ExtraBuildProducts)
		{
			Builder.AddBuildProduct(ExtraBuildProduct);
		}

		// Make sure all the build products exist
		UE4Build.CheckBuildProducts(Builder.BuildProductFiles);

		// Write the manifest if needed
		string ManifestPath = ParseParamValue("manifest");
		if(ManifestPath != null)
		{
			SortedSet<string> Files = new SortedSet<string>();
			foreach(string BuildProductFile in Builder.BuildProductFiles)
			{
				Files.Add(BuildProductFile);
			}
			File.WriteAllLines(ManifestPath, Files.ToArray());
		}
	}

	public static UE4Build.BuildAgenda MakeAgenda(UnrealBuildTool.UnrealTargetPlatform[] Platforms, List<string> ExtraBuildProducts)
	{
		// Create the build agenda
		UE4Build.BuildAgenda Agenda = new UE4Build.BuildAgenda();

		// C# binaries
		Agenda.SwarmProject = @"Engine\Source\Programs\UnrealSwarm\UnrealSwarm.sln";
		Agenda.DotNetProjects.Add(@"Engine/Source/Editor/SwarmInterface/DotNET/SwarmInterface.csproj");
		Agenda.DotNetProjects.Add(@"Engine/Source/Programs/DotNETCommon/DotNETUtilities/DotNETUtilities.csproj");
		Agenda.DotNetProjects.Add(@"Engine/Source/Programs/RPCUtility/RPCUtility.csproj");
		Agenda.DotNetProjects.Add(@"Engine/Source/Programs/UnrealControls/UnrealControls.csproj");

		// Windows binaries
		if(Platforms.Contains(UnrealBuildTool.UnrealTargetPlatform.Win64))
		{
			Agenda.AddTarget("CrashReportClient", UnrealBuildTool.UnrealTargetPlatform.Win64, UnrealBuildTool.UnrealTargetConfiguration.Development);
			Agenda.AddTarget("CrashReportClient", UnrealBuildTool.UnrealTargetPlatform.Win32, UnrealBuildTool.UnrealTargetConfiguration.Development);
			Agenda.AddTarget("UnrealHeaderTool", UnrealBuildTool.UnrealTargetPlatform.Win64, UnrealBuildTool.UnrealTargetConfiguration.Development);
			Agenda.AddTarget("UnrealPak", UnrealBuildTool.UnrealTargetPlatform.Win64, UnrealBuildTool.UnrealTargetConfiguration.Development);
			Agenda.AddTarget("UnrealLightmass", UnrealBuildTool.UnrealTargetPlatform.Win64, UnrealBuildTool.UnrealTargetConfiguration.Development);
			Agenda.AddTarget("ShaderCompileWorker", UnrealBuildTool.UnrealTargetPlatform.Win64, UnrealBuildTool.UnrealTargetConfiguration.Development);
			Agenda.AddTarget("UnrealVersionSelector", UnrealBuildTool.UnrealTargetPlatform.Win64, UnrealBuildTool.UnrealTargetConfiguration.Shipping);
			Agenda.AddTarget("BootstrapPackagedGame", UnrealBuildTool.UnrealTargetPlatform.Win64, UnrealBuildTool.UnrealTargetConfiguration.Shipping);
			Agenda.AddTarget("BootstrapPackagedGame", UnrealBuildTool.UnrealTargetPlatform.Win32, UnrealBuildTool.UnrealTargetConfiguration.Shipping);
			Agenda.AddTarget("UnrealCEFSubProcess", UnrealBuildTool.UnrealTargetPlatform.Win64, UnrealBuildTool.UnrealTargetConfiguration.Development);
			Agenda.AddTarget("UnrealCEFSubProcess", UnrealBuildTool.UnrealTargetPlatform.Win32, UnrealBuildTool.UnrealTargetConfiguration.Development);
		}

		// Mac binaries
		if(Platforms.Contains(UnrealBuildTool.UnrealTargetPlatform.Mac))
		{
			Agenda.AddTarget("CrashReportClient", UnrealBuildTool.UnrealTargetPlatform.Mac, UnrealBuildTool.UnrealTargetConfiguration.Development, InAddArgs: "-CopyAppBundleBackToDevice");
			Agenda.AddTarget("UnrealPak", UnrealBuildTool.UnrealTargetPlatform.Mac, UnrealBuildTool.UnrealTargetConfiguration.Development, InAddArgs: "-CopyAppBundleBackToDevice");
			Agenda.AddTarget("UnrealLightmass", UnrealBuildTool.UnrealTargetPlatform.Mac, UnrealBuildTool.UnrealTargetConfiguration.Development, InAddArgs: "-CopyAppBundleBackToDevice");
			Agenda.AddTarget("ShaderCompileWorker", UnrealBuildTool.UnrealTargetPlatform.Mac, UnrealBuildTool.UnrealTargetConfiguration.Development, InAddArgs: "-CopyAppBundleBackToDevice");
			Agenda.AddTarget("UE4EditorServices", UnrealBuildTool.UnrealTargetPlatform.Mac, UnrealBuildTool.UnrealTargetConfiguration.Development, InAddArgs: "-CopyAppBundleBackToDevice");
			Agenda.AddTarget("UnrealCEFSubProcess", UnrealBuildTool.UnrealTargetPlatform.Mac, UnrealBuildTool.UnrealTargetConfiguration.Development, InAddArgs: "-CopyAppBundleBackToDevice");
		}

		// Linux binaries
		if (Platforms.Contains(UnrealBuildTool.UnrealTargetPlatform.Linux))
		{
			Agenda.AddTarget("CrashReportClient", UnrealBuildTool.UnrealTargetPlatform.Linux, UnrealBuildTool.UnrealTargetConfiguration.Development);
			Agenda.AddTarget("UnrealCEFSubProcess", UnrealBuildTool.UnrealTargetPlatform.Linux, UnrealBuildTool.UnrealTargetConfiguration.Development);
		}

		// iOS binaries
		if(Platforms.Contains(UnrealBuildTool.UnrealTargetPlatform.IOS))
		{
			Agenda.DotNetProjects.Add(@"Engine/Source/Programs/iOS/iPhonePackager/iPhonePackager.csproj");
			ExtraBuildProducts.Add(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/iOS/iPhonePackager.exe"));

			Agenda.DotNetProjects.Add(@"Engine/Source/Programs/iOS/DeploymentServer/DeploymentServer.csproj");
			ExtraBuildProducts.Add(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/iOS/DeploymentServer.exe"));

			Agenda.DotNetProjects.Add(@"Engine/Source/Programs/iOS/DeploymentInterface/DeploymentInterface.csproj");
			ExtraBuildProducts.Add(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/iOS/DeploymentInterface.dll"));

			Agenda.DotNetProjects.Add(@"Engine/Source/Programs/iOS/MobileDeviceInterface/MobileDeviceInterface.csproj");
			ExtraBuildProducts.Add(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/iOS/MobileDeviceInterface.dll"));
		}

		// PS4 binaries
		if(Platforms.Contains(UnrealBuildTool.UnrealTargetPlatform.PS4))
		{
			Agenda.AddTarget("PS4MapFileUtil", UnrealBuildTool.UnrealTargetPlatform.Win64, UnrealBuildTool.UnrealTargetConfiguration.Development);

			Agenda.DotNetProjects.Add(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Source/Programs/PS4/PS4DevKitUtil/PS4DevKitUtil.csproj"));
			ExtraBuildProducts.Add(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/PS4/PS4DevKitUtil.exe"));

			Agenda.DotNetProjects.Add(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Source/Programs/PS4/PS4SymbolTools/PS4SymbolTool.csproj"));
			ExtraBuildProducts.Add(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/PS4/PS4SymbolTool.exe"));
		}

		// Xbox One binaries
		if(Platforms.Contains(UnrealBuildTool.UnrealTargetPlatform.XboxOne))
		{
			Agenda.AddTarget("XboxOnePDBFileUtil", UnrealBuildTool.UnrealTargetPlatform.Win64, UnrealBuildTool.UnrealTargetConfiguration.Development);
		}

		// HTML5 binaries
		if (Platforms.Contains(UnrealBuildTool.UnrealTargetPlatform.HTML5))
		{
			Agenda.DotNetProjects.Add(@"Engine/Source/Programs/HTML5/HTML5LaunchHelper/HTML5LaunchHelper.csproj");
			ExtraBuildProducts.Add(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/HTML5LaunchHelper.exe"));
		}

		return Agenda;
	}
}

public class ZipProjectUp : BuildCommand
{
	public override void ExecuteBuild()
	{
		// Get Directories
		string ProjectDirectory = ParseParamValue("project", "");
		string InstallDirectory = ParseParamValue("install", "");
		ProjectDirectory = Path.GetDirectoryName(ProjectDirectory);

		Log("Started zipping project up");
		Log("Project directory: {0}", ProjectDirectory);
		Log("Install directory: {0}", InstallDirectory);
		Log("Packaging up the project...");

		// Setup filters
		FileFilter Filter = new FileFilter();
		Filter.Include("/Config/...");
		Filter.Include("/Content/...");
		Filter.Include("/Source/...");
		Filter.Include("*.uproject");

		ZipFiles(new FileReference(InstallDirectory), new DirectoryReference(ProjectDirectory), Filter);

		Log("Completed zipping project up");
	}
}
