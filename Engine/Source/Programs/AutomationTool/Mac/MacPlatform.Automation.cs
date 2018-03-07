// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

public class MacPlatform : Platform
{
	public MacPlatform()
		: base(UnrealTargetPlatform.Mac)
	{
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		const string NoEditorCookPlatform = "MacNoEditor";
		const string ServerCookPlatform = "MacServer";
		const string ClientCookPlatform = "MacClient";

		if (bDedicatedServer)
		{
			return ServerCookPlatform;
		}
		else if (bIsClientOnly)
		{
			return ClientCookPlatform;
		}
		else
		{
			return NoEditorCookPlatform;
		}
	}

	public override string GetEditorCookPlatform()
	{
		return "Mac";
	}

	private void StageAppBundle(DeploymentContext SC, DirectoryReference InPath, StagedDirectoryReference NewName)
	{
		// Files with DebugFileExtensions should always be DebugNonUFS
		List<string> DebugExtensions = GetDebugFileExtentions();
		foreach(FileReference InputFile in DirectoryReference.EnumerateFiles(InPath, "*", SearchOption.AllDirectories))
		{
			StagedFileReference OutputFile = StagedFileReference.Combine(NewName, InputFile.MakeRelativeTo(InPath));
			StagedFileType FileType = DebugExtensions.Any(x => InputFile.HasExtension(x))? StagedFileType.DebugNonUFS : StagedFileType.NonUFS;
			SC.StageFile(FileType, InputFile, OutputFile);
		}
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// Stage all the build products
		foreach (StageTarget Target in SC.StageTargets)
		{
			SC.StageBuildProductsFromReceipt(Target.Receipt, Target.RequireFilesExist, Params.bTreatNonShippingBinariesAsDebugFiles);
		}

		if (SC.bStageCrashReporter)
		{
			StagedDirectoryReference CrashReportClientPath = StagedDirectoryReference.Combine("Engine/Binaries", SC.PlatformDir, "CrashReportClient.app");
			StageAppBundle(SC, DirectoryReference.Combine(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir, "CrashReportClient.app"), CrashReportClientPath);
		}

		// Find the app bundle path
		List<FileReference> Exes = GetExecutableNames(SC);
		foreach (var Exe in Exes)
		{
			StagedDirectoryReference AppBundlePath = null;
			if (Exe.IsUnderDirectory(DirectoryReference.Combine(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir)))
			{
				AppBundlePath = StagedDirectoryReference.Combine(SC.ShortProjectName, "Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe.FullName) + ".app");
			}
			else if (Exe.IsUnderDirectory(DirectoryReference.Combine(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir)))
			{
				AppBundlePath = StagedDirectoryReference.Combine("Engine/Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe.FullName) + ".app");
			}

			// Copy the custom icon and Steam dylib, if needed
			if (AppBundlePath != null)
			{
				FileReference AppIconsFile = FileReference.Combine(SC.ProjectRoot, "Build", "Mac", "Application.icns");
				if(FileReference.Exists(AppIconsFile))
				{
					SC.StageFile(StagedFileType.NonUFS, AppIconsFile, StagedFileReference.Combine(AppBundlePath, "Contents", "Resources", "Application.icns"));
				}
			}
		}

		// Copy the splash screen, Mac specific
		FileReference SplashImage = FileReference.Combine(SC.ProjectRoot, "Content", "Splash", "Splash.bmp");
		if(FileReference.Exists(SplashImage))
		{
			SC.StageFile(StagedFileType.NonUFS, SplashImage);
		}

		// Stage the bootstrap executable
		if (!Params.NoBootstrapExe)
		{
			foreach (StageTarget Target in SC.StageTargets)
			{
				BuildProduct Executable = Target.Receipt.BuildProducts.FirstOrDefault(x => x.Type == BuildProductType.Executable);
				if (Executable != null)
				{
					// only create bootstraps for executables
					List<StagedFileReference> StagedFiles = SC.FilesToStage.NonUFSFiles.Where(x => x.Value == Executable.Path).Select(x => x.Key).ToList();
					if (StagedFiles.Count > 0 && Executable.Path.FullName.Replace("\\", "/").Contains("/" + TargetPlatformType.ToString() + "/"))
					{
						string BootstrapArguments = "";
						if (!ShouldStageCommandLine(Params, SC))
						{
							if (!SC.IsCodeBasedProject)
							{
								BootstrapArguments = String.Format("../../../{0}/{0}.uproject", SC.ShortProjectName);
							}
							else
							{
								BootstrapArguments = SC.ShortProjectName;
							}
						}

						string BootstrapExeName;
						if (SC.StageTargetConfigurations.Count > 1)
						{
							BootstrapExeName = Path.GetFileName(Executable.Path.FullName) + ".app";
						}
						else if (Params.IsCodeBasedProject)
						{
							BootstrapExeName = Target.Receipt.TargetName + ".app";
						}
						else
						{
							BootstrapExeName = SC.ShortProjectName + ".app";
						}

						string AppPath = Executable.Path.FullName.Substring(0, Executable.Path.FullName.LastIndexOf(".app/") + 4);
						foreach (var DestPath in StagedFiles)
						{
							string AppRelativePath = DestPath.Name.Substring(0, DestPath.Name.LastIndexOf(".app/") + 4);
							StageBootstrapExecutable(SC, BootstrapExeName, AppPath, AppRelativePath, BootstrapArguments);
						}
					}
				}
			}
		}

		// Copy the ShaderCache files, if they exist
		FileReference DrawCacheFile = FileReference.Combine(SC.ProjectRoot, "Content", "DrawCache.ushadercache");
		if(FileReference.Exists(DrawCacheFile))
		{
			SC.StageFile(StagedFileType.UFS, DrawCacheFile);
		}

		FileReference ByteCodeCacheFile = FileReference.Combine(SC.ProjectRoot, "Content", "ByteCodeCache.ushadercode");
		if(FileReference.Exists(ByteCodeCacheFile))
		{
			SC.StageFile(StagedFileType.UFS, ByteCodeCacheFile);
		}

		{
			// Stage any *.metallib files as NonUFS.
			// Get the final output directory for cooked data
			DirectoryReference CookOutputDir;
			if(!String.IsNullOrEmpty(Params.CookOutputDir))
			{
				CookOutputDir = DirectoryReference.Combine(new DirectoryReference(Params.CookOutputDir), SC.CookPlatform);
			}
			else if(Params.CookInEditor)
			{
				CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "EditorCooked", SC.CookPlatform);
			}
			else
			{
				CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "Cooked", SC.CookPlatform);
			}
			if(DirectoryReference.Exists(CookOutputDir))
			{
				List<FileReference> CookedFiles = DirectoryReference.EnumerateFiles(CookOutputDir, "*.metallib", SearchOption.AllDirectories).ToList();
				foreach(FileReference CookedFile in CookedFiles)
				{
					SC.StageFile(StagedFileType.NonUFS, CookedFile, new StagedFileReference(CookedFile.MakeRelativeTo(CookOutputDir)));
				}
			}
		}
	}

	string GetValueFromInfoPlist(string InfoPlist, string Key, string DefaultValue = "")
	{
		string Value = DefaultValue;
		string KeyString = "<key>" + Key + "</key>";
		int KeyIndex = InfoPlist.IndexOf(KeyString);
		if (KeyIndex > 0)
		{
			int ValueStartIndex = InfoPlist.IndexOf("<string>", KeyIndex + KeyString.Length) + "<string>".Length;
			int ValueEndIndex = InfoPlist.IndexOf("</string>", ValueStartIndex);
			if (ValueStartIndex > 0 && ValueEndIndex > ValueStartIndex)
			{
				Value = InfoPlist.Substring(ValueStartIndex, ValueEndIndex - ValueStartIndex);
			}
		}
		return Value;
	}

	void StageBootstrapExecutable(DeploymentContext SC, string ExeName, string TargetFile, string StagedRelativeTargetPath, string StagedArguments)
	{
		DirectoryReference InputApp = DirectoryReference.Combine(SC.LocalRoot, "Engine", "Binaries", SC.PlatformDir, "BootstrapPackagedGame.app");
		if (InternalUtils.SafeDirectoryExists(InputApp.FullName))
		{
			// Create the new bootstrap program
			DirectoryReference IntermediateDir = DirectoryReference.Combine(SC.ProjectRoot, "Intermediate", "Staging");
			InternalUtils.SafeCreateDirectory(IntermediateDir.FullName);

			DirectoryReference IntermediateApp = DirectoryReference.Combine(IntermediateDir, ExeName);
			if (DirectoryReference.Exists(IntermediateApp))
			{
				DirectoryReference.Delete(IntermediateApp, true);
			}
			CloneDirectory(InputApp.FullName, IntermediateApp.FullName);

			// Rename the executable
			string GameName = Path.GetFileNameWithoutExtension(ExeName);
			FileReference.Move(FileReference.Combine(IntermediateApp, "Contents", "MacOS", "BootstrapPackagedGame"), FileReference.Combine(IntermediateApp, "Contents", "MacOS", GameName));

			// Copy the icon
			string SrcInfoPlistPath = CombinePaths(TargetFile, "Contents", "Info.plist");
			string SrcInfoPlist = File.ReadAllText(SrcInfoPlistPath);

			string IconName = GetValueFromInfoPlist(SrcInfoPlist, "CFBundleIconFile");
			if (!string.IsNullOrEmpty(IconName))
			{
				string IconPath = CombinePaths(TargetFile, "Contents", "Resources", IconName + ".icns");
				InternalUtils.SafeCreateDirectory(CombinePaths(IntermediateApp.FullName, "Contents", "Resources"));
				File.Copy(IconPath, CombinePaths(IntermediateApp.FullName, "Contents", "Resources", IconName + ".icns"));
			}

			// Update Info.plist contents
			string DestInfoPlistPath = CombinePaths(IntermediateApp.FullName, "Contents", "Info.plist");
			string DestInfoPlist = File.ReadAllText(DestInfoPlistPath);

			string AppIdentifier = GetValueFromInfoPlist(SrcInfoPlist, "CFBundleIdentifier");
			if (AppIdentifier == "com.epicgames.UE4Game")
			{
				AppIdentifier = "";
			}

			string Copyright = GetValueFromInfoPlist(SrcInfoPlist, "NSHumanReadableCopyright");
			string BundleVersion = GetValueFromInfoPlist(SrcInfoPlist, "CFBundleVersion", "1");
			string ShortVersion = GetValueFromInfoPlist(SrcInfoPlist, "CFBundleShortVersionString", "1.0");

			DestInfoPlist = DestInfoPlist.Replace("com.epicgames.BootstrapPackagedGame", string.IsNullOrEmpty(AppIdentifier) ? "com.epicgames." + GameName + "_bootstrap" : AppIdentifier + "_bootstrap");
			DestInfoPlist = DestInfoPlist.Replace("BootstrapPackagedGame", GameName);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_ICON_FILE__", IconName);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_APP_TO_LAUNCH__", StagedRelativeTargetPath);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_COMMANDLINE__", StagedArguments);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_COPYRIGHT__", Copyright);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_BUNDLE_VERSION__", BundleVersion);
			DestInfoPlist = DestInfoPlist.Replace("__UE4_SHORT_VERSION__", ShortVersion);

			File.WriteAllText(DestInfoPlistPath, DestInfoPlist);

			StageAppBundle(SC, IntermediateApp, new StagedDirectoryReference(ExeName));
		}
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		// package up the program, potentially with an installer for Mac
		PrintRunTime();
	}

	public override void ProcessArchivedProject(ProjectParams Params, DeploymentContext SC)
	{
		if (Params.CreateAppBundle)
		{
			string ExeName = SC.StageExecutables[0];
			string BundlePath = CombinePaths(SC.ArchiveDirectory.FullName, SC.ShortProjectName + ".app");

			if (SC.bIsCombiningMultiplePlatforms)
			{
				// when combining multiple platforms, don't merge the content into the .app, use the one in the Binaries directory
				BundlePath = CombinePaths(SC.ArchiveDirectory.FullName, SC.ShortProjectName, "Binaries", "Mac", ExeName + ".app");
				if (!Directory.Exists(BundlePath))
				{
					// if the .app wasn't there, just skip out (we don't require executables when combining)
					return;
				}
			}

			string TargetPath = CombinePaths(BundlePath, "Contents", "UE4");
			if (!SC.bIsCombiningMultiplePlatforms)
			{
				if (Directory.Exists(BundlePath))
				{
					Directory.Delete(BundlePath, true);
				}

				string SourceBundlePath = CombinePaths(SC.ArchiveDirectory.FullName, SC.ShortProjectName, "Binaries", "Mac", ExeName + ".app");
				if (!Directory.Exists(SourceBundlePath))
				{
					SourceBundlePath = CombinePaths(SC.ArchiveDirectory.FullName, "Engine", "Binaries", "Mac", ExeName + ".app");
					if (!Directory.Exists(SourceBundlePath))
					{
						SourceBundlePath = CombinePaths(SC.ArchiveDirectory.FullName, "Engine", "Binaries", "Mac", "UE4.app");
					}
				}
				Directory.Move(SourceBundlePath, BundlePath);

				if (DirectoryExists(TargetPath))
				{
					Directory.Delete(TargetPath, true);
				}

				// First, move all files and folders inside he app bundle
				string[] StagedFiles = Directory.GetFiles(SC.ArchiveDirectory.FullName, "*", SearchOption.TopDirectoryOnly);
				foreach (string FilePath in StagedFiles)
				{
					string TargetFilePath = CombinePaths(TargetPath, Path.GetFileName(FilePath));
					Directory.CreateDirectory(Path.GetDirectoryName(TargetFilePath));
					File.Move(FilePath, TargetFilePath);
				}

				string[] StagedDirectories = Directory.GetDirectories(SC.ArchiveDirectory.FullName, "*", SearchOption.TopDirectoryOnly);
				foreach (string DirPath in StagedDirectories)
				{
					string DirName = Path.GetFileName(DirPath);
					if (!DirName.EndsWith(".app"))
					{
						string TargetDirPath = CombinePaths(TargetPath, DirName);
						Directory.CreateDirectory(Path.GetDirectoryName(TargetDirPath));
						Directory.Move(DirPath, TargetDirPath);
					}
				}
			}

			// Update executable name, icon and entry in Info.plist
			string UE4GamePath = CombinePaths(BundlePath, "Contents", "MacOS", ExeName);
			if (ExeName != SC.ShortProjectName && File.Exists(UE4GamePath))
			{
				string GameExePath = CombinePaths(BundlePath, "Contents", "MacOS", SC.ShortProjectName);
				File.Delete(GameExePath);
				File.Move(UE4GamePath, GameExePath);

				string DefaultIconPath = CombinePaths(BundlePath, "Contents", "Resources", "UE4Game.icns");
				string CustomIconSrcPath = CombinePaths(BundlePath, "Contents", "Resources", "Application.icns");
				string CustomIconDestPath = CombinePaths(BundlePath, "Contents", "Resources", SC.ShortProjectName + ".icns");
				if (File.Exists(CustomIconSrcPath))
				{
					File.Delete(DefaultIconPath);
					if (File.Exists(CustomIconDestPath))
					{
						File.Delete(CustomIconDestPath);
					}
					File.Move(CustomIconSrcPath, CustomIconDestPath);
				}
				else if (File.Exists(DefaultIconPath))
				{
					if (File.Exists(CustomIconDestPath))
					{
						File.Delete(CustomIconDestPath);
					}
					File.Move(DefaultIconPath, CustomIconDestPath);
				}

				string InfoPlistPath = CombinePaths(BundlePath, "Contents", "Info.plist");
				string InfoPlistContents = File.ReadAllText(InfoPlistPath);
				InfoPlistContents = InfoPlistContents.Replace(ExeName, SC.ShortProjectName);
				InfoPlistContents = InfoPlistContents.Replace("<string>UE4Game</string>", "<string>" + SC.ShortProjectName + "</string>");
				File.Delete(InfoPlistPath);
				File.WriteAllText(InfoPlistPath, InfoPlistContents);
			}

			if (!SC.bIsCombiningMultiplePlatforms)
			{
				// creating these directories when the content isn't moved into the application causes it 
				// to fail to load, and isn't needed
				Directory.CreateDirectory(CombinePaths(TargetPath, "Engine", "Binaries", "Mac"));
				Directory.CreateDirectory(CombinePaths(TargetPath, SC.ShortProjectName, "Binaries", "Mac"));
			}
		}
	}

	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		if (!File.Exists(ClientApp))
		{
			if (Directory.Exists(ClientApp + ".app"))
			{
				ClientApp += ".app/Contents/MacOS/" + Path.GetFileName(ClientApp);
			}
			else
			{
				Int32 BaseDirLen = Params.BaseStageDirectory.Length;
				string StageSubDir = ClientApp.Substring(BaseDirLen, ClientApp.IndexOf("/", BaseDirLen + 1) - BaseDirLen);
				ClientApp = CombinePaths(Params.BaseStageDirectory, StageSubDir, Params.ShortProjectName + ".app/Contents/MacOS/" + Params.ShortProjectName);
			}
		}

		PushDir(Path.GetDirectoryName(ClientApp));
		// Always start client process and don't wait for exit.
		IProcessResult ClientProcess = Run(ClientApp, ClientCmdLine, null, ClientRunFlags | ERunOptions.NoWaitForExit);
		PopDir();

		return ClientProcess;
	}

	public override bool IsSupported { get { return true; } }

	public override bool ShouldUseManifestForUBTBuilds(string AddArgs)
	{
		// don't use the manifest to set up build products if we are compiling Mac under Windows and we aren't going to copy anything back to the PC
		bool bIsBuildingRemotely = UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac;
		bool bUseManifest = !bIsBuildingRemotely || AddArgs.IndexOf("-CopyAppBundleBackToDevice", StringComparison.InvariantCultureIgnoreCase) > 0;
		return bUseManifest;
	}
	public override List<string> GetDebugFileExtentions()
	{
		return new List<string> { ".dSYM" };
	}
	public override bool CanHostPlatform(UnrealTargetPlatform Platform)
	{
		if (Platform == UnrealTargetPlatform.IOS || Platform == UnrealTargetPlatform.Mac || Platform == UnrealTargetPlatform.TVOS)
		{
			return true;
		}
		return false;
	}

	public override bool ShouldStageCommandLine(ProjectParams Params, DeploymentContext SC)
	{
		return false; // !String.IsNullOrEmpty(Params.StageCommandline) || !String.IsNullOrEmpty(Params.RunCommandline) || (!Params.IsCodeBasedProject && Params.NoBootstrapExe);
	}

	public override bool SignExecutables(DeploymentContext SC, ProjectParams Params)
	{
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
			// Sign everything we built
			List<FileReference> FilesToSign = GetExecutableNames(SC);
			Log("RuntimeProjectRootDir: " + SC.RuntimeProjectRootDir);
			foreach (var Exe in FilesToSign)
			{
				Log("Signing: " + Exe);
				string AppBundlePath = "";
				if (Exe.IsUnderDirectory(DirectoryReference.Combine(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir)))
				{
					Log("Starts with Binaries");
					AppBundlePath = CombinePaths(SC.RuntimeProjectRootDir.FullName, "Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe.FullName) + ".app");
				}
				else if (Exe.IsUnderDirectory(DirectoryReference.Combine(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir)))
				{
					Log("Starts with Engine/Binaries");
					AppBundlePath = CombinePaths("Engine/Binaries", SC.PlatformDir, Path.GetFileNameWithoutExtension(Exe.FullName) + ".app");
				}
				Log("Signing: " + AppBundlePath);
				CodeSign.SignMacFileOrFolder(AppBundlePath);
			}

		}
		return true;
	}

	public override void StripSymbols(FileReference SourceFile, FileReference TargetFile)
	{
		MacExports.StripSymbols(SourceFile, TargetFile);
	}
}
