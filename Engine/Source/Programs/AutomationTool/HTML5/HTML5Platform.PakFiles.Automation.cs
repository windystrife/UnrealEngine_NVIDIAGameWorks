// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Diagnostics;
using Tools.DotNETCommon;

public class HTMLPakAutomation
{
	ProjectParams Params;

	DeploymentContext SC;

	string PakOrderFileLocation;

	Dictionary<string, object> DependencyJson;

	public static bool CanCreateMapPaks(ProjectParams Param)
	{
        // TODO: setting this to false for now -- this will be converted to return CHUNK settings -- in part 2 of level streaming support for HTML5
        return false;

        //bool UseAsyncLevelLoading = false;
        //var ConfigCache = UnrealBuildTool.ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Param.RawProjectPath), UnrealTargetPlatform.HTML5);
        //ConfigCache.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "UseAsyncLevelLoading", out UseAsyncLevelLoading);

        //if (Param.Run)
        //	return false; 

        //return UseAsyncLevelLoading;
    }

	public HTMLPakAutomation(ProjectParams InParam, DeploymentContext InSC)
	{
		Params = InParam;
		SC = InSC;

		var PakOrderFileLocationBase = CommandUtils.CombinePaths(SC.ProjectRoot.FullName, "Build", SC.FinalCookPlatform, "FileOpenOrder");
		PakOrderFileLocation = CommandUtils.CombinePaths(PakOrderFileLocationBase, "GameOpenOrder.log");
		if (!CommandUtils.FileExists_NoExceptions(PakOrderFileLocation))
		{
			// Use a fall back, it doesn't matter if this file exists or not. GameOpenOrder.log is preferred however
			PakOrderFileLocation = CommandUtils.CombinePaths(PakOrderFileLocationBase, "EditorOpenOrder.log");
		}

		string PakPath = Path.Combine(new string[] { Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "HTML5", SC.ShortProjectName});
		if (Directory.Exists(PakPath))
		{
			Directory.Delete(PakPath,true);
		}

		// read in the json file.
		string JsonFile = CommandUtils.CombinePaths(new string[] { SC.ProjectRoot.FullName, "Saved", "Cooked", "HTML5", Params.ShortProjectName, "MapDependencyGraph.json" });
		string text = File.ReadAllText(JsonFile);

		DependencyJson = fastJSON.JSON.Instance.ToObject<Dictionary<string, object>>(text);

	}

	/// <summary>
	/// Create Emscripten Data package with Necessary Packages.
	///
	/// </summary>
	public void CreateEmscriptenDataPackage(string PackagePath, string FinalDataLocation)
	{
		string PythonPath = HTML5SDKInfo.Python();
		string PackagerPath = HTML5SDKInfo.EmscriptenPackager();

		using (new ScopedEnvVar("EM_CONFIG", HTML5SDKInfo.DOT_EMSCRIPTEN))
		{
			// we need to operate in the root
			using (new PushedDirectory(Path.Combine(PackagePath)))
			{
				string CmdLine = string.Format("\"{0}\" \"{1}\" --preload \"{2}\" --js-output=\"{1}.js\" --no-heap-copy", PackagerPath, FinalDataLocation, SC.ShortProjectName);
				CommandUtils.RunAndLog(CommandUtils.CmdEnv, PythonPath, CmdLine);
			}
		}
	}

	/// <summary>
	/// Create a Pak from Engine Contents.
	/// </summary>
	public void CreateEnginePak()
	{
		string StagedDir = Path.Combine(Params.BaseStageDirectory, "HTML5");// CommandUtils.CombinePaths(SC.ProjectRoot, "Saved", "Cooked", "HTML5");

		string PakPath = Path.Combine( new string [] { Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "HTML5", SC.ShortProjectName, "Content","Paks"});
		if (!Directory.Exists(PakPath))
		{
			Directory.CreateDirectory(PakPath);
		}

		// find list of files in the Engine Dir.
		string[] files = Directory.GetFiles( Path.Combine(StagedDir, "Engine"), "*", SearchOption.AllDirectories);

		Dictionary<string,string> UnrealPakResponseFile = new Dictionary<string,string>();

		// create response file structure.
		foreach( string file in files )
		{
			string MountPoint = file.Remove(0, StagedDir.Length + 1);
			UnrealPakResponseFile[file] = "../../../" +  MountPoint;
		}

		RunUnrealPak(UnrealPakResponseFile, PakPath + "\\EnginePak.pak", null, "", true);
	}


	/// <summary>
	/// Create a pak from a minimum set of game files
	/// </summary>
	public void CreateGamePak()
	{
		// create a pak of game config and first level files.

		string StagedDir = Path.Combine(Params.BaseStageDirectory, "HTML5");// CommandUtils.CombinePaths(SC.ProjectRoot, "Saved", "Cooked", "HTML5");

		string PakPath = Path.Combine(new string[] { Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "HTML5", Params.ShortProjectName, "Content", "Paks" });
		if (!Directory.Exists(PakPath))
		{
			Directory.CreateDirectory(PakPath);
		}

		List<string> AllFiles = new List<string>();
		try
		{
			string[] SlateFiles = Directory.GetFiles(Path.Combine(StagedDir, Params.ShortProjectName, "Content", "Slate"), "*", SearchOption.AllDirectories);
			AllFiles.AddRange(SlateFiles);
		}
		catch ( Exception /*Ex*/)
		{
			// not found.
		}

		try
		{
			string[] Resources = Directory.GetFiles(Path.Combine(StagedDir, Params.ShortProjectName, "Content", "Resources"), "*", SearchOption.AllDirectories);
			AllFiles.AddRange(Resources);
		}
		catch (Exception /*Ex*/)
		{
			// not found.
		}

		string[] ConfigFiles = Directory.GetFiles(Path.Combine(StagedDir, Params.ShortProjectName, "Config"), "*", SearchOption.AllDirectories);

		AllFiles.AddRange(ConfigFiles);

		string[] TopLevelFiles = Directory.GetFiles(Path.Combine(StagedDir, Params.ShortProjectName), "*", SearchOption.TopDirectoryOnly);

		AllFiles.AddRange(TopLevelFiles);

		Dictionary<string, string> UnrealPakResponseFile = new Dictionary<string, string>();

		// create response file structure.
		foreach (string file in AllFiles)
		{
			string MountPoint = file.Remove(0, StagedDir.Length + 1);
			UnrealPakResponseFile[file] = "../../../" + MountPoint;
		}

		RunUnrealPak(UnrealPakResponseFile, PakPath + "\\" + Params.ShortProjectName + ".pak", null, "", true);

	}

	/// <summary>
	/// Create ContentDirectory Pak.
	/// </summary>
	public void CreateContentDirectoryPak()
	{
		object[] Value = (object[])DependencyJson["ContentDirectoryAssets"];
		List<string> ContentDirectory = Array.ConvertAll<object, string>(Value, Convert.ToString).ToList();
		CreatePak("ContentDirectoryAssets", ContentDirectory);
	}


	/// <summary>
	/// Create Maps Paks based on JSON file created during Cooking. (-GenerateDependenciesForMaps)
	/// </summary>
	public void CreateMapPak()
	{
		// read in the json file.

		object[] Value = (object[])DependencyJson["ContentDirectoryAssets"];
		List<string> ContentDirectory = Array.ConvertAll<object, string>(Value, Convert.ToString).ToList();

		foreach (string Pak in DependencyJson.Keys)
		{
			object[] ObjectList = (object[])DependencyJson[Pak];
			List<string> Assets = Array.ConvertAll<object, string>(ObjectList, Convert.ToString).ToList();

			// add itself.
			Assets.Add(Pak);

			if (Pak != "ContentDirectoryAssets")
			{
				// remove content directory assets.
				Assets = Assets.Except(ContentDirectory).ToList();
				string[] Tokens = Pak.Split('/');
				CreatePak(Tokens[Tokens.Length - 1], Assets);
			}
		}

	}

	/// <summary>
	/// Create Delta Paks based on Level Transitions.
	/// </summary>
	/// <param name="MapFrom"></param>
	/// <param name="MapTo"></param>
	public void CreateDeltaMapPaks(string MapFrom, string MapTo)
	{
		foreach (string NameKey in DependencyJson.Keys)
		{
			if (NameKey.Contains(MapFrom))
			{
				MapFrom = NameKey;
			}
			if (NameKey.Contains(MapTo))
			{
				MapTo = NameKey;
			}
		}

		List<string> MapFromAssets = Array.ConvertAll<object, string>(DependencyJson[MapFrom] as object[], Convert.ToString).ToList();
		MapFromAssets.Add(MapFrom);
		List<string> MapToAssets = Array.ConvertAll<object, string>(DependencyJson[MapTo] as object[], Convert.ToString).ToList();
		MapToAssets.Add(MapTo);

		object[] Value = (object[])DependencyJson["ContentDirectoryAssets"];
		List<string> ContentDirectory = Array.ConvertAll<object, string>(Value, Convert.ToString).ToList();

		// Delta.
		List<string> DeltaAssets = MapToAssets.Except(MapFromAssets).Except(ContentDirectory).ToList();

		// create a pak file for this delta.
		string[] Tokens_MapFrom = MapFrom.Split('/');
		string[] Tokens_MapTo = MapTo.Split('/');

		string DeltaPakName = Tokens_MapFrom[Tokens_MapFrom.Length - 1] + "_" + Tokens_MapTo[Tokens_MapTo.Length - 1];
		CreatePak(DeltaPakName, DeltaAssets);
	}

	/// <summary>
	/// Writes a pak response file to disk
	/// </summary>
	/// <param name="Filename"></param>
	/// <param name="ResponseFile"></param>
	private static void WritePakResponseFile(string Filename, Dictionary<string, string> ResponseFile, bool Compressed)
	{
		Directory.CreateDirectory(Path.GetDirectoryName(Filename));
		using (var Writer = new StreamWriter(Filename, false, new System.Text.UTF8Encoding(true)))
		{
			foreach (var Entry in ResponseFile)
			{
				string Line = String.Format("\"{0}\" \"{1}\"", Entry.Key, Entry.Value);
// this will be enabled again in the near future when IndexedDB is used more extensively
// (this will happen when the new filesystem feature goes in [post multi-threading] for wasm)
//				if (Compressed)
//				{
//					Line += " -compress";
//				}
				Writer.WriteLine(Line);
			}
		}
	}

	/// <summary>
	/// Create a Pak file
	/// </summary>
	private void RunUnrealPak(Dictionary<string, string> UnrealPakResponseFile, string OutputLocation, string EncryptionKeys, string PlatformOptions, bool Compressed)
	{
		if (UnrealPakResponseFile.Count < 1)
		{
			return;
		}
		string PakName = Path.GetFileNameWithoutExtension(OutputLocation);
		string UnrealPakResponseFileName = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, "PakList_" + PakName + ".txt");
		WritePakResponseFile(UnrealPakResponseFileName, UnrealPakResponseFile, Compressed);

		var UnrealPakExe = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealPak.exe");

		string CmdLine = CommandUtils.MakePathSafeToUseWithCommandLine(OutputLocation) + " -create=" + CommandUtils.MakePathSafeToUseWithCommandLine(UnrealPakResponseFileName);
		if (!String.IsNullOrEmpty(EncryptionKeys))
		{
			CmdLine += " -sign=" + CommandUtils.MakePathSafeToUseWithCommandLine(EncryptionKeys);
		}
		if (GlobalCommandLine.Installed)
		{
			CmdLine += " -installed";
		}
		CmdLine += " -order=" + CommandUtils.MakePathSafeToUseWithCommandLine(PakOrderFileLocation);
		if (GlobalCommandLine.UTF8Output)
		{
			CmdLine += " -UTF8Output";
		}
		CmdLine += PlatformOptions;
		CommandUtils.RunAndLog(CommandUtils.CmdEnv, UnrealPakExe, CmdLine, Options: CommandUtils.ERunOptions.Default | CommandUtils.ERunOptions.UTF8Output);
	}

	/// <summary>
	/// Create a Pak.
	/// </summary>
	/// <param name="MapFrom"></param>
	/// <param name="MapTo"></param>
	private void CreatePak(string Name, List<string> Assets)
	{
		string CookedDir = Path.Combine(Params.BaseStageDirectory, "HTML5");

		string PakPath = Path.Combine(new string[] { Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "HTML5", Params.ShortProjectName, "Content", "Paks" });
		if (!Directory.Exists(PakPath))
		{
			Directory.CreateDirectory(PakPath);
		}

		Assets.Add(Name);
		List<string> Files = new List<string>();
		foreach( string Asset in Assets)
		{
			string GameDir = "/Game/";
			if (Asset.StartsWith(GameDir))
			{
				string PathOnDiskasset = CommandUtils.CombinePaths(CookedDir, Params.ShortProjectName, "Content", Asset.Remove(0, GameDir.Length) + ".uasset");
				string PathOnDiskmap   = CommandUtils.CombinePaths(CookedDir, Params.ShortProjectName, "Content", Asset.Remove(0, GameDir.Length)+ ".umap");

				if (File.Exists(PathOnDiskmap))
				{
					Files.Add(PathOnDiskmap);
				}
				else if (File.Exists(PathOnDiskasset))
				{
					Files.Add(PathOnDiskasset);
				}
				else
				{
					System.Console.WriteLine(Asset + " not found, skipping !!");
				}
			}
		}

		// we have all the files.
		Dictionary<string, string> UnrealPakResponseFile = new Dictionary<string, string>();

		// create response file structure.
		foreach (string file in Files)
		{
			string MountPoint = file.Remove(0, CookedDir.Length + 1);
			UnrealPakResponseFile[file] ="../../../" + MountPoint;
		}
		RunUnrealPak(UnrealPakResponseFile, PakPath + "\\"+ Name + ".pak", null, "", true);
	}

}
