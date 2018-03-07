// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Net.NetworkInformation;
using System.Threading;
using AutomationTool;
using UnrealBuildTool;
using Ionic.Zip;
using Tools.DotNETCommon;

public class AndroidPlatform : Platform
{
	// Maximum allowed OBB size (2 GiB)
	private const Int64 MaxOBBSizeAllowed = 2147483648;

	private const int DeployMaxParallelCommands = 6;

    private const string TargetAndroidLocation = "obb/";

	public AndroidPlatform()
		: base(UnrealTargetPlatform.Android)
	{
	}

	private static string GetSONameWithoutArchitecture(ProjectParams Params, string DecoratedExeName)
	{
		return Path.Combine(Path.GetDirectoryName(Params.ProjectGameExeFilename), DecoratedExeName) + ".so";
	}

	private static string GetFinalApkName(ProjectParams Params, string DecoratedExeName, bool bRenameUE4Game, string Architecture, string GPUArchitecture)
	{
		string ProjectDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(Params.RawProjectPath.FullName)), "Binaries/Android");

		if (Params.Prebuilt)
		{
			ProjectDir = Path.Combine(Params.BaseStageDirectory, "Android");
		}

		// Apk's go to project location, not necessarily where the .so is (content only packages need to output to their directory)
		string ApkName = Path.Combine(ProjectDir, DecoratedExeName) + Architecture + GPUArchitecture + ".apk";

		// if the source binary was UE4Game, handle using it or switching to project name
		if (Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename) == "UE4Game")
		{
			if (bRenameUE4Game)
			{
				// replace UE4Game with project name (only replace in the filename part)
				ApkName = Path.Combine(Path.GetDirectoryName(ApkName), Path.GetFileName(ApkName).Replace("UE4Game", Params.ShortProjectName));
			}
			else
			{
				// if we want to use UE4 directly then use it from the engine directory not project directory
				ApkName = ApkName.Replace(ProjectDir, Path.Combine(CmdEnv.LocalRoot, "Engine/Binaries/Android"));
			}
		}

		return ApkName;
	}

	private static string GetFinalSymbolizedSODirectory(DeploymentContext SC, string Architecture, string GPUArchitecture)
	{
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		int StoreVersion;
		Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "StoreVersion", out StoreVersion);

		return SC.ShortProjectName + "_Symbols_v" + StoreVersion + "/" + SC.ShortProjectName + Architecture + GPUArchitecture;
	}

	private static string GetFinalObbName(string ApkName)
	{
		// calculate the name for the .obb file
		string PackageName = GetPackageInfo(ApkName, false);
		if (PackageName == null)
		{
			throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package name from " + ApkName);
		}

		string PackageVersion = GetPackageInfo(ApkName, true);
		if (PackageVersion == null || PackageVersion.Length == 0)
		{
			throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package version from " + ApkName);
		}

		if (PackageVersion.Length > 0)
		{
			int IntVersion = int.Parse(PackageVersion);
			PackageVersion = IntVersion.ToString("0");
		}

		string ObbName = string.Format("main.{0}.{1}.obb", PackageVersion, PackageName);

		// plop the .obb right next to the executable
		ObbName = Path.Combine(Path.GetDirectoryName(ApkName), ObbName);

		return ObbName;
	}

	private static string GetDeviceObbName(string ApkName)
	{
        string ObbName = GetFinalObbName(ApkName);
        string PackageName = GetPackageInfo(ApkName, false);
        return TargetAndroidLocation + PackageName + "/" + Path.GetFileName(ObbName);
	}

    private static string GetStorageQueryCommand()
    {
		if (Utils.IsRunningOnMono)
		{
			return "shell 'echo $EXTERNAL_STORAGE'";
		}
		else
		{
			return "shell \"echo $EXTERNAL_STORAGE\"";
		}
    }

	enum EBatchType
	{
		Install,
		Uninstall,
		Symbolize,
	};
	private static string GetFinalBatchName(string ApkName, ProjectParams Params, string Architecture, string GPUArchitecture, bool bNoOBBInstall, EBatchType BatchType, UnrealTargetPlatform Target)
	{
		string Extension = ".bat";
		switch (Target)
		{
			default:
			case UnrealTargetPlatform.Win64:
				Extension = ".bat";
				break;

			case UnrealTargetPlatform.Linux:
				Extension = ".sh";
				break;

			case UnrealTargetPlatform.Mac:
				Extension = ".command";
				break;
		}

		switch(BatchType)
		{
			case EBatchType.Install:
			case EBatchType.Uninstall:
				return Path.Combine(Path.GetDirectoryName(ApkName), (BatchType == EBatchType.Uninstall ? "Uninstall_" : "Install_") + Params.ShortProjectName + (!bNoOBBInstall ? "_" : "_NoOBBInstall_") + Params.ClientConfigsToBuild[0].ToString() + Architecture + GPUArchitecture + Extension);
			case EBatchType.Symbolize:
				return Path.Combine(Path.GetDirectoryName(ApkName), "SymobilzeCrashDump_"  + Params.ShortProjectName + Architecture + GPUArchitecture + Extension);
		}
		return "";
	}

	private List<string> CollectPluginDataPaths(DeploymentContext SC)
	{
		// collect plugin extra data paths from target receipts
		List<string> PluginExtras = new List<string>();
		foreach (StageTarget Target in SC.StageTargets)
		{
			TargetReceipt Receipt = Target.Receipt;
			var Results = Receipt.AdditionalProperties.Where(x => x.Name == "AndroidPlugin");
			foreach (var Property in Results)
			{
				// Keep only unique paths
				string PluginPath = Property.Value;
				if (PluginExtras.FirstOrDefault(x => x == PluginPath) == null)
				{
					PluginExtras.Add(PluginPath);
					Log("AndroidPlugin: {0}", PluginPath);
				}
			}
		}
		return PluginExtras;
	}
	private bool BuildWithHiddenSymbolVisibility(DeploymentContext SC)
	{
		UnrealTargetConfiguration TargetConfiguration = SC.StageTargetConfigurations[0];
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool bBuild = false;
		return TargetConfiguration == UnrealTargetConfiguration.Shipping && (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildWithHiddenSymbolVisibility", out bBuild) && bBuild);
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException(ExitCode.Error_OnlyOneTargetConfigurationSupported, "Android is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}

		UnrealTargetConfiguration TargetConfiguration = SC.StageTargetConfigurations[0];

		IAndroidToolChain ToolChain = AndroidExports.CreateToolChain(Params.RawProjectPath);
		var Architectures = ToolChain.GetAllArchitectures();
		var GPUArchitectures = ToolChain.GetAllGPUArchitectures();
		bool bMakeSeparateApks = UnrealBuildTool.AndroidExports.ShouldMakeSeparateApks();
		bool bBuildWithHiddenSymbolVisibility = BuildWithHiddenSymbolVisibility(SC);

		var Deploy = AndroidExports.CreateDeploymentHandler(Params.RawProjectPath);
		bool bPackageDataInsideApk = Deploy.PackageDataInsideApk(false);

		string BaseApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, "", "");
		Log("BaseApkName = {0}", BaseApkName);

		// Create main OBB with entire contents of staging dir. This
		// includes any PAK files, movie files, etc.

		string LocalObbName = SC.StageDirectory.FullName+".obb";

		// Always delete the target OBB file if it exists
		if (File.Exists(LocalObbName))
		{
			File.Delete(LocalObbName);
		}

		// Now create the OBB as a ZIP archive.
		Log("Creating {0} from {1}", LocalObbName, SC.StageDirectory);
		using (ZipFile ObbFile = new ZipFile(LocalObbName))
		{
			ObbFile.CompressionMethod = CompressionMethod.None;
			ObbFile.CompressionLevel = Ionic.Zlib.CompressionLevel.None;
			ObbFile.UseZip64WhenSaving = Ionic.Zip.Zip64Option.Never;

			int ObbFileCount = 0;
			ObbFile.AddProgress +=
				delegate(object sender, AddProgressEventArgs e)
				{
					if (e.EventType == ZipProgressEventType.Adding_AfterAddEntry)
					{
						ObbFileCount += 1;
						Log("[{0}/{1}] Adding {2} to OBB",
							ObbFileCount, e.EntriesTotal,
							e.CurrentEntry.FileName);
					}
				};
			ObbFile.AddDirectory(SC.StageDirectory+"/"+SC.ShortProjectName, SC.ShortProjectName);
			try
			{
				ObbFile.Save();
			}
			catch (Exception)
			{
				Log("Failed to build OBB: " + LocalObbName);
				throw new AutomationException(ExitCode.Error_AndroidOBBError, "Stage Failed. Could not build OBB {0}. The file may be too big to fit in an OBB (2 GiB limit)", LocalObbName);
			}
		}

		// make sure the OBB is <= 2GiB
		FileInfo OBBFileInfo = new FileInfo(LocalObbName);
		Int64 ObbFileLength = OBBFileInfo.Length;
		if (ObbFileLength > MaxOBBSizeAllowed)
		{
			Log("OBB exceeds 2 GiB limit: " + ObbFileLength + " bytes");
			throw new AutomationException(ExitCode.Error_AndroidOBBError, "Stage Failed. OBB {0} exceeds 2 GiB limit)", LocalObbName);
		}

		// collect plugin extra data paths from target receipts
		Deploy.SetAndroidPluginData(Architectures, CollectPluginDataPaths(SC));

		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), UnrealTargetPlatform.Android);
		int MinSDKVersion;
		Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MinSDKVersion", out MinSDKVersion);
		int TargetSDKVersion = MinSDKVersion;
		Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "TargetSDKVersion", out TargetSDKVersion);
		Log("Target SDK Version" + TargetSDKVersion);

		foreach (string Architecture in Architectures)
		{
			foreach (string GPUArchitecture in GPUArchitectures)
			{
				string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "");
				if (!SC.IsCodeBasedProject)
				{
					string UE4SOName = GetFinalApkName(Params, SC.StageExecutables[0], false, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "");
                    UE4SOName = UE4SOName.Replace(".apk", ".so");
                    if (FileExists_NoExceptions(UE4SOName) == false)
					{
						Log("Failed to find game .so " + UE4SOName);
                        throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Could not find .so {0}. You may need to build the UE4 project with your target configuration and platform.", UE4SOName);
					}
				}
				
				              
				if (!Params.Prebuilt)
				{
					string CookFlavor = SC.FinalCookPlatform.IndexOf("_") > 0 ? SC.FinalCookPlatform.Substring(SC.FinalCookPlatform.IndexOf("_")) : "";
					string SOName = GetSONameWithoutArchitecture(Params, SC.StageExecutables[0]);
					Deploy.PrepForUATPackageOrDeploy(Params.RawProjectPath, Params.ShortProjectName, SC.ProjectRoot, SOName, SC.LocalRoot + "/Engine", Params.Distribution, CookFlavor, false);
				}

			    // Create APK specific OBB in case we have a detached OBB.
			    string DeviceObbName = "";
			    string ObbName = "";
				if (!bPackageDataInsideApk)
			    {
				    DeviceObbName = GetDeviceObbName(ApkName);
				    ObbName = GetFinalObbName(ApkName);
				    CopyFile(LocalObbName, ObbName);
			    }

				//figure out which platforms we need to create install files for
				bool bNeedsPCInstall = false;
				bool bNeedsMacInstall = false;
				bool bNeedsLinuxInstall = false;
				GetPlatformInstallOptions(SC, out bNeedsPCInstall, out bNeedsMacInstall, out bNeedsLinuxInstall);

				//helper delegate to prevent code duplication but allow us access to all the local variables we need
				var CreateInstallFilesAction = new Action<UnrealTargetPlatform>(Target =>
				{
					bool bIsPC = (Target == UnrealTargetPlatform.Win64);
					// Write install batch file(s).
					string BatchName = GetFinalBatchName(ApkName, Params, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "", false, EBatchType.Install, Target);
					string PackageName = GetPackageInfo(ApkName, false);
					// make a batch file that can be used to install the .apk and .obb files
					string[] BatchLines = GenerateInstallBatchFile(bPackageDataInsideApk, PackageName, ApkName, Params, ObbName, DeviceObbName, false, bIsPC, Params.Distribution, TargetSDKVersion > 22);
					File.WriteAllLines(BatchName, BatchLines);
					// make a batch file that can be used to uninstall the .apk and .obb files
					string UninstallBatchName = GetFinalBatchName(ApkName, Params, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "", false, EBatchType.Uninstall, Target);
					BatchLines = GenerateUninstallBatchFile(bPackageDataInsideApk, PackageName, ApkName, Params, ObbName, DeviceObbName, false, bIsPC);
					File.WriteAllLines(UninstallBatchName, BatchLines);

					string SymbolizeBatchName = GetFinalBatchName(ApkName, Params, Architecture, GPUArchitecture, false, EBatchType.Symbolize, Target);
					if(bBuildWithHiddenSymbolVisibility)
					{
						BatchLines = GenerateSymbolizeBatchFile(Params, PackageName, SC, Architecture, GPUArchitecture, bIsPC);
						File.WriteAllLines(SymbolizeBatchName, BatchLines);
					}

					if (Utils.IsRunningOnMono)
					{
						CommandUtils.FixUnixFilePermissions(BatchName);
						CommandUtils.FixUnixFilePermissions(UninstallBatchName);
						if(bBuildWithHiddenSymbolVisibility)
						{
							CommandUtils.FixUnixFilePermissions(SymbolizeBatchName);
						}
						//if(File.Exists(NoInstallBatchName)) 
						//{
						//    CommandUtils.FixUnixFilePermissions(NoInstallBatchName);
						//}
					}
				}
				);

				if (bNeedsPCInstall)
				{
					CreateInstallFilesAction.Invoke(UnrealTargetPlatform.Win64);
				}
				if (bNeedsMacInstall)
				{
					CreateInstallFilesAction.Invoke(UnrealTargetPlatform.Mac);
				}
				if (bNeedsLinuxInstall)
				{
					CreateInstallFilesAction.Invoke(UnrealTargetPlatform.Linux);
				}

				// If we aren't packaging data in the APK then lets write out a bat file to also let us test without the OBB
				// on the device.
				//String NoInstallBatchName = GetFinalBatchName(ApkName, Params, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "", true, false);
				// if(!bPackageDataInsideApk)
				//{
				//    BatchLines = GenerateInstallBatchFile(bPackageDataInsideApk, PackageName, ApkName, Params, ObbName, DeviceObbName, true);
				//    File.WriteAllLines(NoInstallBatchName, BatchLines);
				//}
			}
		}

		PrintRunTime();
	}

	private string[] GenerateInstallBatchFile(bool bPackageDataInsideApk, string PackageName, string ApkName, ProjectParams Params, string ObbName, string DeviceObbName, bool bNoObbInstall, bool bIsPC, bool bIsDistribution, bool bRequireRuntimeStoragePermission)
    {
        string[] BatchLines = null;
        string ReadPermissionGrantCommand = "shell pm grant " + PackageName + " android.permission.READ_EXTERNAL_STORAGE";
        string WritePermissionGrantCommand = "shell pm grant " + PackageName + " android.permission.WRITE_EXTERNAL_STORAGE";

		// We don't grant runtime permission for distribution build on purpose since we will push the obb file to the folder that doesn't require runtime storage permission.
		// This way developer can catch permission issue if they try to save/load game file in folder that requires runtime storage permission.
		bool bNeedGrantStoragePermission = bRequireRuntimeStoragePermission && !bIsDistribution;

		// We can't always push directly to Android/obb so uploads to Download then moves it
		bool bDontMoveOBB = bPackageDataInsideApk || !bIsDistribution;

		if (!bIsPC)
        {
			// If it is a distribution build, push to $STORAGE/Android/obb folder instead of $STORAGE/obb folder.
			// Note that $STORAGE/Android/obb will be the folder that contains the obb if you download the app from playstore.
			string OBBInstallCommand = bNoObbInstall ? "shell 'rm -r $EXTERNAL_STORAGE/" + DeviceObbName + "'" : "push " + Path.GetFileName(ObbName) + " $STORAGE/" + (bIsDistribution ? "Download/" : "") + DeviceObbName;

            Log("Writing shell script for install with {0}", bPackageDataInsideApk ? "data in APK" : "separate obb");
            BatchLines = new string[] {
						"#!/bin/sh",
						"cd \"`dirname \"$0\"`\"",
                        "ADB=",
						"if [ \"$ANDROID_HOME\" != \"\" ]; then ADB=$ANDROID_HOME/platform-tools/adb; else ADB=" +Environment.GetEnvironmentVariable("ANDROID_HOME") + "/platform-tools/adb; fi",
						"DEVICE=",
						"if [ \"$1\" != \"\" ]; then DEVICE=\"-s $1\"; fi",
						"echo",
						"echo Uninstalling existing application. Failures here can almost always be ignored.",
						"$ADB $DEVICE uninstall " + PackageName,
						"echo",
						"echo Installing existing application. Failures here indicate a problem with the device \\(connection or storage permissions\\) and are fatal.",
						"$ADB $DEVICE install " + Path.GetFileName(ApkName),
						"if [ $? -eq 0 ]; then",
                        "\techo",
						bNeedGrantStoragePermission ? "\techo Grant READ_EXTERNAL_STORAGE and WRITE_EXTERNAL_STORAGE to the apk for reading OBB or game file in external storage." : "",
						bNeedGrantStoragePermission ? "\t$ADB $DEVICE " + ReadPermissionGrantCommand : "",
						bNeedGrantStoragePermission ?"\t$ADB $DEVICE " + WritePermissionGrantCommand : "",
                        "\techo",
						"\techo Removing old data. Failures here are usually fine - indicating the files were not on the device.",
                        "\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/UE4Game/" + Params.ShortProjectName + "'",
						"\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/UE4Game/UE4CommandLine.txt" + "'",
						"\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/" + TargetAndroidLocation + PackageName + "'",
						"\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/Android/" + TargetAndroidLocation + PackageName + "'",
						bPackageDataInsideApk ? "" : "\techo",
						bPackageDataInsideApk ? "" : "\techo Installing new data. Failures here indicate storage problems \\(missing SD card or bad permissions\\) and are fatal.",
						bPackageDataInsideApk ? "" : "\tSTORAGE=$(echo \"`$ADB $DEVICE shell 'echo $EXTERNAL_STORAGE'`\" | cat -v | tr -d '^M')",
						bPackageDataInsideApk ? "" : "\t$ADB $DEVICE " + OBBInstallCommand,
						bPackageDataInsideApk ? "if [ 1 ]; then" : "\tif [ $? -eq 0 ]; then",
						bDontMoveOBB ? "" : "\t\t$ADB $DEVICE shell mv $STORAGE/Download/obb/" + PackageName + " $STORAGE/Android/obb/" + PackageName,
						"\t\techo",
						"\t\techo Installation successful",
						"\t\texit 0",
						"\tfi",
						"fi",
						"echo",
						"echo There was an error installing the game or the obb file. Look above for more info.",
						"echo",
						"echo Things to try:",
						"echo Check that the device (and only the device) is listed with \\\"$ADB devices\\\" from a command prompt.",
						"echo Make sure all Developer options look normal on the device",
						"echo Check that the device has an SD card.",
						"exit 1"
					};
        }
        else
        {
			string OBBInstallCommand = bNoObbInstall ? "shell rm -r %STORAGE%/" + DeviceObbName : "push " + Path.GetFileName(ObbName) + " %STORAGE%/" + (bIsDistribution ? "Download/" : "") + DeviceObbName;

			Log("Writing bat for install with {0}", bPackageDataInsideApk ? "data in APK" : "separate OBB");
            BatchLines = new string[] {
						"setlocal",
                        "set ANDROIDHOME=%ANDROID_HOME%",
                        "if \"%ANDROIDHOME%\"==\"\" set ANDROIDHOME="+Environment.GetEnvironmentVariable("ANDROID_HOME"),
						"set ADB=%ANDROIDHOME%\\platform-tools\\adb.exe",
						"set DEVICE=",
                        "if not \"%1\"==\"\" set DEVICE=-s %1",
                        "for /f \"delims=\" %%A in ('%ADB% %DEVICE% " + GetStorageQueryCommand() +"') do @set STORAGE=%%A",
						"@echo.",
						"@echo Uninstalling existing application. Failures here can almost always be ignored.",
						"%ADB% %DEVICE% uninstall " + PackageName,
						"@echo.",
						"@echo Installing existing application. Failures here indicate a problem with the device (connection or storage permissions) and are fatal.",
						"%ADB% %DEVICE% install " + Path.GetFileName(ApkName),
						"@if \"%ERRORLEVEL%\" NEQ \"0\" goto Error",
                        "%ADB% %DEVICE% shell rm -r %STORAGE%/UE4Game/" + Params.ShortProjectName,
						"%ADB% %DEVICE% shell rm -r %STORAGE%/UE4Game/UE4CommandLine.txt", // we need to delete the commandline in UE4Game or it will mess up loading
						"%ADB% %DEVICE% shell rm -r %STORAGE%/" + TargetAndroidLocation + PackageName,
						"%ADB% %DEVICE% shell rm -r %STORAGE%/Android/" + TargetAndroidLocation + PackageName,
						bPackageDataInsideApk ? "" : "@echo.",
						bPackageDataInsideApk ? "" : "@echo Installing new data. Failures here indicate storage problems (missing SD card or bad permissions) and are fatal.",
						bPackageDataInsideApk ? "" : "%ADB% %DEVICE% " + OBBInstallCommand,
						bPackageDataInsideApk ? "" : "if \"%ERRORLEVEL%\" NEQ \"0\" goto Error",
						bDontMoveOBB ? "" : "%ADB% %DEVICE% shell mv %STORAGE%/Download/obb/" + PackageName + " %STORAGE%/Android/obb/" + PackageName,
						bDontMoveOBB ? "" : "if \"%ERRORLEVEL%\" NEQ \"0\" goto Error",
						"@echo.",
						bNeedGrantStoragePermission ? "@echo Grant READ_EXTERNAL_STORAGE and WRITE_EXTERNAL_STORAGE to the apk for reading OBB file or game file in external storage." : "",
						bNeedGrantStoragePermission ? "%ADB% %DEVICE% " + ReadPermissionGrantCommand : "",
						bNeedGrantStoragePermission ? "%ADB% %DEVICE% " + WritePermissionGrantCommand : "",
                        "@echo.",
                        "@echo Installation successful",
						"goto:eof",
						":Error",
						"@echo.",
						"@echo There was an error installing the game or the obb file. Look above for more info.",
						"@echo.",
						"@echo Things to try:",
						"@echo Check that the device (and only the device) is listed with \"%ADB$ devices\" from a command prompt.",
						"@echo Make sure all Developer options look normal on the device",
						"@echo Check that the device has an SD card.",
						"@pause"
					};
        }
        return BatchLines;
    }

	private string[] GenerateUninstallBatchFile(bool bPackageDataInsideApk, string PackageName, string ApkName, ProjectParams Params, string ObbName, string DeviceObbName, bool bNoObbInstall, bool bIsPC)
	{
		string[] BatchLines = null;

		if (!bIsPC)
		{
			Log("Writing shell script for uninstall with {0}", bPackageDataInsideApk ? "data in APK" : "separate obb");
			BatchLines = new string[] {
						"#!/bin/sh",
						"cd \"`dirname \"$0\"`\"",
						"ADB=",
						"if [ \"$ANDROID_HOME\" != \"\" ]; then ADB=$ANDROID_HOME/platform-tools/adb; else ADB=" +Environment.GetEnvironmentVariable("ANDROID_HOME") + "/platform-tools/adb; fi",
						"DEVICE=",
						"if [ \"$1\" != \"\" ]; then DEVICE=\"-s $1\"; fi",
						"echo",
						"echo Uninstalling existing application. Failures here can almost always be ignored.",
						"$ADB $DEVICE uninstall " + PackageName,
						"echo",
						"echo Removing old data. Failures here are usually fine - indicating the files were not on the device.",
						"$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/UE4Game/" + Params.ShortProjectName + "'",
						"$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/UE4Game/UE4CommandLine.txt" + "'",
						"$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/" + TargetAndroidLocation + PackageName + "'",
						"$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/Android/" + TargetAndroidLocation + PackageName + "'",
						"echo",
						"echo Uninstall completed",
						"exit 0",
					};
		}
		else
		{
			Log("Writing bat for uninstall with {0}", bPackageDataInsideApk ? "data in APK" : "separate OBB");
			BatchLines = new string[] {
						"setlocal",
						"set ANDROIDHOME=%ANDROID_HOME%",
						"if \"%ANDROIDHOME%\"==\"\" set ANDROIDHOME="+Environment.GetEnvironmentVariable("ANDROID_HOME"),
						"set ADB=%ANDROIDHOME%\\platform-tools\\adb.exe",
						"set DEVICE=",
						"if not \"%1\"==\"\" set DEVICE=-s %1",
						"for /f \"delims=\" %%A in ('%ADB% %DEVICE% " + GetStorageQueryCommand() +"') do @set STORAGE=%%A",
						"@echo.",
						"@echo Uninstalling existing application. Failures here can almost always be ignored.",
						"%ADB% %DEVICE% uninstall " + PackageName,
						"@echo.",
						"echo Removing old data. Failures here are usually fine - indicating the files were not on the device.",
						"%ADB% %DEVICE% shell rm -r %STORAGE%/UE4Game/" + Params.ShortProjectName,
						"%ADB% %DEVICE% shell rm -r %STORAGE%/UE4Game/UE4CommandLine.txt", // we need to delete the commandline in UE4Game or it will mess up loading
						"%ADB% %DEVICE% shell rm -r %STORAGE%/" + TargetAndroidLocation + PackageName,
						"%ADB% %DEVICE% shell rm -r %STORAGE%/Android/" + TargetAndroidLocation + PackageName,
						"@echo.",
						"@echo Uninstall completed",
					};
		}
		return BatchLines;
	}

	private string[] GenerateSymbolizeBatchFile(ProjectParams Params, string PackageName, DeploymentContext SC, string Architecture, string GPUArchitecture, bool bIsPC)
	{
		string[] BatchLines = null;

		if (!bIsPC)
		{
			Log("Writing shell script for symbolize with {0}", "data in APK" );
			BatchLines = new string[] {
				"#!/bin/sh",
				"if [ $? -ne 0]; then",
				 "echo \"Required argument missing, pass a dump of adb crash log.\"",
				 "exit 1",
				"fi",
				"cd \"`dirname \"$0\"`\"",
				"NDKSTACK=",
				"if [ \"$ANDROID_NDK_ROOT\" != \"\" ]; then NDKSTACK=$%ANDROID_NDK_ROOT/ndk-stack; else ADB=" + Environment.GetEnvironmentVariable("ANDROID_NDK_ROOT") + "/ndk-stack; fi",
				"$NDKSTACK -sym " + GetFinalSymbolizedSODirectory(SC, Architecture, GPUArchitecture) + " -dump \"%1\" > " + Params.ShortProjectName + "_SymbolizedCallStackOutput.txt",
				"exit 0",
				};
		}
		else
		{
			Log("Writing bat for symbolize");
			BatchLines = new string[] {
						"@echo off",
						"IF %1.==. GOTO NoArgs",
						"setlocal",
						"set NDK_ROOT=%ANDROID_NDK_ROOT%",
						"if \"%ANDROID_NDK_ROOT%\"==\"\" set NDK_ROOT=\""+Environment.GetEnvironmentVariable("ANDROID_NDK_ROOT")+"\"",
						"set NDKSTACK=%NDK_ROOT%\ndk-stack.cmd",
						"",
						"%NDKSTACK% -sym "+GetFinalSymbolizedSODirectory(SC, Architecture, GPUArchitecture)+" -dump \"%1\" > "+ Params.ShortProjectName+"_SymbolizedCallStackOutput.txt",
						"",
						"goto:eof",
						"",
						"",
						":NoArgs",
						"echo.",
						"echo Required argument missing, pass a dump of adb crash log. (SymboliseCallStackDump C:\\adbcrashlog.txt)",
						"pause"
					};
		}
		return BatchLines;
	}

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException(ExitCode.Error_OnlyOneTargetConfigurationSupported, "Android is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}

		UnrealTargetConfiguration TargetConfiguration = SC.StageTargetConfigurations[0];
		IAndroidToolChain ToolChain = AndroidExports.CreateToolChain(Params.RawProjectPath);
		var Architectures = ToolChain.GetAllArchitectures();
		var GPUArchitectures = ToolChain.GetAllGPUArchitectures();
		bool bMakeSeparateApks = UnrealBuildTool.AndroidExports.ShouldMakeSeparateApks();
		bool bPackageDataInsideApk = UnrealBuildTool.AndroidExports.CreateDeploymentHandler(Params.RawProjectPath).PackageDataInsideApk(false);

		bool bAddedOBB = false;
		foreach (string Architecture in Architectures)
		{
			foreach (string GPUArchitecture in GPUArchitectures)
			{
				string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "");
				string ObbName = GetFinalObbName(ApkName);
				bool bBuildWithHiddenSymbolVisibility = BuildWithHiddenSymbolVisibility(SC);
				//string NoOBBBatchName = GetFinalBatchName(ApkName, Params, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "", true, false);

				// verify the files exist
				if (!FileExists(ApkName))
				{
					throw new AutomationException(ExitCode.Error_AppNotFound, "ARCHIVE FAILED - {0} was not found", ApkName);
				}
				if (!bPackageDataInsideApk && !FileExists(ObbName))
				{
                    throw new AutomationException(ExitCode.Error_ObbNotFound, "ARCHIVE FAILED - {0} was not found", ObbName);
				}

				if (bBuildWithHiddenSymbolVisibility)
				{
					string SymbolizedSODirectory = GetFinalSymbolizedSODirectory(SC, Architecture, GPUArchitecture);
					string SymbolizedSOPath = Path.Combine(Path.Combine(Path.GetDirectoryName(ApkName), SymbolizedSODirectory), "libUE4.so");
					if (!FileExists(SymbolizedSOPath))
					{
						throw new AutomationException(ExitCode.Error_SymbolizedSONotFound, "ARCHIVE FAILED - {0} was not found", SymbolizedSOPath);
					}

					// Add symbolized .so directory
					SC.ArchiveFiles(Path.GetDirectoryName(SymbolizedSOPath), Path.GetFileName(SymbolizedSOPath), true, null, SymbolizedSODirectory);
				}

				SC.ArchiveFiles(Path.GetDirectoryName(ApkName), Path.GetFileName(ApkName));
				if (!bPackageDataInsideApk && !bAddedOBB)
				{
					bAddedOBB = true;
					SC.ArchiveFiles(Path.GetDirectoryName(ObbName), Path.GetFileName(ObbName));
				}

				bool bNeedsPCInstall = false;
				bool bNeedsMacInstall = false;
				bool bNeedsLinuxInstall = false;
				GetPlatformInstallOptions(SC, out bNeedsPCInstall, out bNeedsMacInstall, out bNeedsLinuxInstall);

				//helper delegate to prevent code duplication but allow us access to all the local variables we need
				var CreateBatchFilesAndArchiveAction = new Action<UnrealTargetPlatform>(Target =>
				{
					string BatchName = GetFinalBatchName(ApkName, Params, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "", false, EBatchType.Install, Target);
					string UninstallBatchName = GetFinalBatchName(ApkName, Params, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "", false, EBatchType.Uninstall, Target);

					SC.ArchiveFiles(Path.GetDirectoryName(BatchName), Path.GetFileName(BatchName));
					SC.ArchiveFiles(Path.GetDirectoryName(UninstallBatchName), Path.GetFileName(UninstallBatchName));

					if(bBuildWithHiddenSymbolVisibility)
					{
						string SymbolizeBatchName = GetFinalBatchName(ApkName, Params, Architecture, GPUArchitecture, false, EBatchType.Symbolize, Target);
						SC.ArchiveFiles(Path.GetDirectoryName(SymbolizeBatchName), Path.GetFileName(SymbolizeBatchName));
					}
					//SC.ArchiveFiles(Path.GetDirectoryName(NoOBBBatchName), Path.GetFileName(NoOBBBatchName));
				}
				);

				//it's possible we will need both PC and Mac/Linux install files, do both
				if (bNeedsPCInstall)
				{
					CreateBatchFilesAndArchiveAction(UnrealTargetPlatform.Win64);
				}
				if (bNeedsMacInstall)
				{
					CreateBatchFilesAndArchiveAction(UnrealTargetPlatform.Mac);
				}
				if (bNeedsLinuxInstall)
				{
					CreateBatchFilesAndArchiveAction(UnrealTargetPlatform.Linux);
				}
			}
		}
	}

	private void GetPlatformInstallOptions(DeploymentContext SC, out bool bNeedsPCInstall, out bool bNeedsMacInstall, out bool bNeedsLinuxInstall)
	{
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool bGenerateAllPlatformInstall = false;
		Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bCreateAllPlatformsInstall", out bGenerateAllPlatformInstall);

		bNeedsPCInstall = bNeedsMacInstall = bNeedsLinuxInstall = false;

		if (bGenerateAllPlatformInstall)
		{
			bNeedsPCInstall = bNeedsMacInstall = bNeedsLinuxInstall = true;
		}
		else
		{
			if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Mac)
			{
				bNeedsMacInstall = true;
			}
			else if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Linux)
			{
				bNeedsLinuxInstall = true;
			}
			else
			{
				bNeedsPCInstall = true;
			}
		}
	}

	private string GetAdbCommandLine(ProjectParams Params, string SerialNumber, string Args)
	{
	    if (SerialNumber != "")
		{
			SerialNumber = "-s " + SerialNumber;
		}

		return string.Format("{0} {1}", SerialNumber, Args);
	}

	static string LastSpewFilename = "";

	public string ADBSpewFilter(string Message)
	{
		if (Message.StartsWith("[") && Message.Contains("%]"))
		{
			int LastIndex = Message.IndexOf(":");
			LastIndex = LastIndex == -1 ? Message.Length : LastIndex;

			if (Message.Length > 7)
			{
				string Filename = Message.Substring(7, LastIndex - 7);
				if (Filename == LastSpewFilename)
				{
					return null;
				}
				LastSpewFilename = Filename;
			}
			return Message;
		}
		return Message;
	}

	private IProcessResult RunAdbCommand(ProjectParams Params, string SerialNumber, string Args, string Input = null, ERunOptions Options = ERunOptions.Default)
	{
		string AdbCommand = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/platform-tools/adb" + (Utils.IsRunningOnMono ? "" : ".exe"));
		if (Options.HasFlag(ERunOptions.AllowSpew) || Options.HasFlag(ERunOptions.SpewIsVerbose))
		{
			LastSpewFilename = "";
			return Run(AdbCommand, GetAdbCommandLine(Params, SerialNumber, Args), Input, Options, SpewFilterCallback: new ProcessResult.SpewFilterCallbackType(ADBSpewFilter));
		}
		return Run(AdbCommand, GetAdbCommandLine(Params, SerialNumber, Args), Input, Options);
	}

	private string RunAndLogAdbCommand(ProjectParams Params, string SerialNumber, string Args, out int SuccessCode)
	{
		string AdbCommand = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/platform-tools/adb" + (Utils.IsRunningOnMono ? "" : ".exe"));
		LastSpewFilename = "";
		return RunAndLog(CmdEnv, AdbCommand, GetAdbCommandLine(Params, SerialNumber, Args), out SuccessCode, SpewFilterCallback: new ProcessResult.SpewFilterCallbackType(ADBSpewFilter));
	}

	public override void GetConnectedDevices(ProjectParams Params, out List<string> Devices)
	{
		Devices = new List<string>();
		IProcessResult Result = RunAdbCommand(Params, "", "devices");

		if (Result.Output.Length > 0)
		{
			string[] LogLines = Result.Output.Split(new char[] { '\n', '\r' });
			bool FoundList = false;
			for (int i = 0; i < LogLines.Length; ++i)
			{
				if (FoundList == false)
				{
					if (LogLines[i].StartsWith("List of devices attached"))
					{
						FoundList = true;
					}
					continue;
				}

				string[] DeviceLine = LogLines[i].Split(new char[] { '\t' });

				if (DeviceLine.Length == 2)
				{
					// the second param should be "device"
					// if it's not setup correctly it might be "unattached" or "powered off" or something like that
					// warning in that case
					if (DeviceLine[1] == "device")
					{
						Devices.Add("@" + DeviceLine[0]);
					}
					else
					{
						CommandUtils.LogWarning("Device attached but in bad state {0}:{1}", DeviceLine[0], DeviceLine[1]);
					}
				}
			}
		}
	}

	/*
	private class TimeRegion : System.IDisposable
	{
		private System.DateTime StartTime { get; set; }

		private string Format { get; set; }

		private System.Collections.Generic.List<object> FormatArgs { get; set; }

		public TimeRegion(string format, params object[] format_args)
		{
			Format = format;
			FormatArgs = new List<object>(format_args);
			StartTime = DateTime.UtcNow;
		}

		public void Dispose()
		{
			double total_time = (DateTime.UtcNow - StartTime).TotalMilliseconds / 1000.0;
			FormatArgs.Insert(0, total_time);
			CommandUtils.Log(Format, FormatArgs.ToArray());
		}
	}
	*/

	public override bool RetrieveDeployedManifests(ProjectParams Params, DeploymentContext SC, string DeviceName, out List<string> UFSManifests, out List<string> NonUFSManifests)
	{
		UFSManifests = null;
		NonUFSManifests = null;

		// Query the storage path from the device
		string DeviceStorageQueryCommand = GetStorageQueryCommand();
		IProcessResult StorageResult = RunAdbCommand(Params, DeviceName, DeviceStorageQueryCommand, null, ERunOptions.AppMustExist);
		String StorageLocation = StorageResult.Output.Trim();
		string RemoteDir = StorageLocation + "/UE4Game/" + Params.ShortProjectName;

		// Note: appends the device name to make the filename unique; these files will be deleted later during delta manifest generation
		// Replace colon with underscore for legal filename (colon may be present for wifi connected devices)
		string SanitizedDeviceName = DeviceName.Replace(":", "_");

		// Try retrieving the UFS files manifest files from the device
		string UFSManifestFileName = CombinePaths(SC.StageDirectory.FullName, SC.UFSDeployedManifestFileName + "_" + SanitizedDeviceName);
		IProcessResult UFSResult = RunAdbCommand(Params, DeviceName, " pull " + RemoteDir + "/" + SC.UFSDeployedManifestFileName + " \"" + UFSManifestFileName + "\"", null, ERunOptions.AppMustExist);
		if (!(UFSResult.Output.Contains("bytes") || UFSResult.Output.Contains("[100%]")))
		{
			return false;
		}

		// Try retrieving the non UFS files manifest files from the device
		string NonUFSManifestFileName = CombinePaths(SC.StageDirectory.FullName, SC.NonUFSDeployedManifestFileName + "_" + SanitizedDeviceName);
		IProcessResult NonUFSResult = RunAdbCommand(Params, DeviceName, " pull " + RemoteDir + "/" + SC.NonUFSDeployedManifestFileName + " \"" + NonUFSManifestFileName + "\"", null, ERunOptions.AppMustExist);
		if (!(NonUFSResult.Output.Contains("bytes") || NonUFSResult.Output.Contains("[100%]")))
		{
			// Did not retrieve both so delete one we did retrieve
			File.Delete(UFSManifestFileName);
			return false;
		}

		// Return the manifest files
		UFSManifests = new List<string>();
		UFSManifests.Add(UFSManifestFileName);
		NonUFSManifests = new List<string>();
		NonUFSManifests.Add(NonUFSManifestFileName);

		return true;
	}

	internal class LongestFirst : IComparer<string>
	{
		public int Compare(string a, string b)
		{
			if (a.Length == b.Length) return a.CompareTo(b);
			else return b.Length - a.Length;
		}
	}

    public override void Deploy(ProjectParams Params, DeploymentContext SC)
    {
		var AppArchitectures = AndroidExports.CreateToolChain(Params.RawProjectPath).GetAllArchitectures();

		foreach (var DeviceName in Params.DeviceNames)
        {
            string DeviceArchitecture = GetBestDeviceArchitecture(Params, DeviceName);
            string GPUArchitecture = GetBestGPUArchitecture(Params, DeviceName);

            string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, DeviceArchitecture, GPUArchitecture);

            // make sure APK is up to date (this is fast if so)
            var Deploy = AndroidExports.CreateDeploymentHandler(Params.RawProjectPath);
            if (!Params.Prebuilt)
            {
                string CookFlavor = SC.FinalCookPlatform.IndexOf("_") > 0 ? SC.FinalCookPlatform.Substring(SC.FinalCookPlatform.IndexOf("_")) : "";
				string SOName = GetSONameWithoutArchitecture(Params, SC.StageExecutables[0]);
				Deploy.SetAndroidPluginData(AppArchitectures, CollectPluginDataPaths(SC));
                Deploy.PrepForUATPackageOrDeploy(Params.RawProjectPath, Params.ShortProjectName, SC.ProjectRoot, SOName, SC.LocalRoot + "/Engine", Params.Distribution, CookFlavor, true);
            }

            // now we can use the apk to get more info
            string PackageName = GetPackageInfo(ApkName, false);

            // Setup the OBB name and add the storage path (queried from the device) to it
            string DeviceStorageQueryCommand = GetStorageQueryCommand();
            IProcessResult Result = RunAdbCommand(Params, DeviceName, DeviceStorageQueryCommand, null, ERunOptions.AppMustExist);
            String StorageLocation = Result.Output.Trim(); // "/mnt/sdcard";
            string DeviceObbName = StorageLocation + "/" + GetDeviceObbName(ApkName);
            string RemoteDir = StorageLocation + "/UE4Game/" + Params.ShortProjectName;

            // determine if APK out of date
            string APKLastUpdateTime = new FileInfo(ApkName).LastWriteTime.ToString();
            bool bNeedAPKInstall = true;
            if (Params.IterativeDeploy)
            {
                // Check for apk installed with this package name on the device
                IProcessResult InstalledResult = RunAdbCommand(Params, DeviceName, "shell pm list packages " + PackageName, null, ERunOptions.AppMustExist);
                if (InstalledResult.Output.Contains(PackageName))
                {
                    // See if apk is up to date on device
                    InstalledResult = RunAdbCommand(Params, DeviceName, "shell cat " + RemoteDir + "/APKFileStamp.txt", null, ERunOptions.AppMustExist);
                    if (InstalledResult.Output.StartsWith("APK: "))
                    {
                        if (InstalledResult.Output.Substring(5).Trim() == APKLastUpdateTime)
                            bNeedAPKInstall = false;

                        // Stop the previously running copy (uninstall/install did this before)
                        InstalledResult = RunAdbCommand(Params, DeviceName, "shell am force-stop " + PackageName, null, ERunOptions.AppMustExist);
                        if (InstalledResult.Output.Contains("Error"))
                        {
                            // force-stop not supported (Android < 3.0) so check if package is actually running
                            // Note: cannot use grep here since it may not be installed on device
                            InstalledResult = RunAdbCommand(Params, DeviceName, "shell ps", null, ERunOptions.AppMustExist);
                            if (InstalledResult.Output.Contains(PackageName))
                            {
                                // it is actually running so use the slow way to kill it (uninstall and reinstall)
                                bNeedAPKInstall = true;
                            }

                        }
                    }
                }
            }

            // install new APK if needed
            if (bNeedAPKInstall)
            {
                // try uninstalling an old app with the same identifier.
                int SuccessCode = 0;
                string UninstallCommandline = "uninstall " + PackageName;
                RunAndLogAdbCommand(Params, DeviceName, UninstallCommandline, out SuccessCode);

                // install the apk
                string InstallCommandline = "install \"" + ApkName + "\"";
                string InstallOutput = RunAndLogAdbCommand(Params, DeviceName, InstallCommandline, out SuccessCode);
                int FailureIndex = InstallOutput.IndexOf("Failure");

                // adb install doesn't always return an error code on failure, and instead prints "Failure", followed by an error code.
                if (SuccessCode != 0 || FailureIndex != -1)
                {
                    string ErrorMessage = string.Format("Installation of apk '{0}' failed", ApkName);
                    if (FailureIndex != -1)
                    {
                        string FailureString = InstallOutput.Substring(FailureIndex + 7).Trim();
                        if (FailureString != "")
                        {
                            ErrorMessage += ": " + FailureString;
                        }
                    }
                    if (ErrorMessage.Contains("OLDER_SDK"))
                    {
                        LogError("minSdkVersion is higher than Android version installed on device, possibly due to NDK API Level");
                    }
                    throw new AutomationException(ExitCode.Error_AppInstallFailed, ErrorMessage);
                }
                else
                {
                    // giving EXTERNAL_STORAGE_WRITE permission to the apk for API23+
                    // without this permission apk can't access to the assets put into the device
                    string ReadPermissionCommandLine = "shell pm grant " + PackageName + " android.permission.READ_EXTERNAL_STORAGE";
                    string WritePermissionCommandLine = "shell pm grant " + PackageName + " android.permission.WRITE_EXTERNAL_STORAGE";
                    RunAndLogAdbCommand(Params, DeviceName, ReadPermissionCommandLine, out SuccessCode);
                    RunAndLogAdbCommand(Params, DeviceName, WritePermissionCommandLine, out SuccessCode);
                }
            }

            // update the ue4commandline.txt
            // update and deploy ue4commandline.txt
            // always delete the existing commandline text file, so it doesn't reuse an old one
            FileReference IntermediateCmdLineFile = FileReference.Combine(SC.StageDirectory, "UE4CommandLine.txt");
            Project.WriteStageCommandline(IntermediateCmdLineFile, Params, SC);

            // copy files to device if we were staging
            if (SC.Stage)
            {
                // cache some strings
                string BaseCommandline = "push";

                HashSet<string> EntriesToDeploy = new HashSet<string>();

                if (Params.IterativeDeploy)
                {
                    // always send UE4CommandLine.txt (it was written above after delta checks applied)
                    EntriesToDeploy.Add(IntermediateCmdLineFile.FullName);

                    // Add non UFS files if any to deploy
                    String NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath(DeviceName);
                    if (File.Exists(NonUFSManifestPath))
                    {
                        string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
                        foreach (string Filename in NonUFSFiles.Split('\n'))
                        {
                            if (!string.IsNullOrEmpty(Filename) && !string.IsNullOrWhiteSpace(Filename))
                            {
                                EntriesToDeploy.Add(CombinePaths(SC.StageDirectory.FullName, Filename.Trim()));
                            }
                        }
                    }

                    // Add UFS files if any to deploy
                    String UFSManifestPath = SC.GetUFSDeploymentDeltaPath(DeviceName);
                    if (File.Exists(UFSManifestPath))
                    {
                        string UFSFiles = File.ReadAllText(UFSManifestPath);
                        foreach (string Filename in UFSFiles.Split('\n'))
                        {
                            if (!string.IsNullOrEmpty(Filename) && !string.IsNullOrWhiteSpace(Filename))
                            {
                                EntriesToDeploy.Add(CombinePaths(SC.StageDirectory.FullName, Filename.Trim()));
                            }
                        }
                    }

                    // For now, if too many files may be better to just push them all
                    if (EntriesToDeploy.Count > 500)
                    {
                        // make sure device is at a clean state
                        RunAdbCommand(Params, DeviceName, "shell rm -r " + RemoteDir);

                        EntriesToDeploy.Clear();
                        EntriesToDeploy.TrimExcess();
                        EntriesToDeploy.Add(SC.StageDirectory.FullName);
                    }
                }
                else
                {
                    // make sure device is at a clean state
                    RunAdbCommand(Params, DeviceName, "shell rm -r " + RemoteDir);

                    // Copy UFS files..
                    string[] Files = Directory.GetFiles(SC.StageDirectory.FullName, "*", SearchOption.AllDirectories);
                    System.Array.Sort(Files);

                    // Find all the files we exclude from copying. And include
                    // the directories we need to individually copy.
                    HashSet<string> ExcludedFiles = new HashSet<string>();
                    SortedSet<string> IndividualCopyDirectories
                        = new SortedSet<string>((IComparer<string>)new LongestFirst());
                    foreach (string Filename in Files)
                    {
                        bool Exclude = false;
                        // Don't push the apk, we install it
                        Exclude |= Path.GetExtension(Filename).Equals(".apk", StringComparison.InvariantCultureIgnoreCase);
                        // For excluded files we add the parent dirs to our
                        // tracking of stuff to individually copy.
                        if (Exclude)
                        {
                            ExcludedFiles.Add(Filename);
                            // We include all directories up to the stage root in having
                            // to individually copy the files.
                            for (string FileDirectory = Path.GetDirectoryName(Filename);
                                !FileDirectory.Equals(SC.StageDirectory);
                                FileDirectory = Path.GetDirectoryName(FileDirectory))
                            {
                                if (!IndividualCopyDirectories.Contains(FileDirectory))
                                {
                                    IndividualCopyDirectories.Add(FileDirectory);
                                }
                            }
                            if (!IndividualCopyDirectories.Contains(SC.StageDirectory.FullName))
                            {
                                IndividualCopyDirectories.Add(SC.StageDirectory.FullName);
                            }
                        }
                    }

                    // The directories are sorted above in "deepest" first. We can
                    // therefore start copying those individual dirs which will
                    // recreate the tree. As the subtrees will get copied at each
                    // possible individual level.
                    foreach (string DirectoryName in IndividualCopyDirectories)
                    {
                        string[] Entries
                            = Directory.GetFileSystemEntries(DirectoryName, "*", SearchOption.TopDirectoryOnly);
                        foreach (string Entry in Entries)
                        {
                            // We avoid excluded files and the individual copy dirs
                            // (the individual copy dirs will get handled as we iterate).
                            if (ExcludedFiles.Contains(Entry) || IndividualCopyDirectories.Contains(Entry))
                            {
                                continue;
                            }
                            else
                            {
                                EntriesToDeploy.Add(Entry);
                            }
                        }
                    }

                    if (EntriesToDeploy.Count == 0)
                    {
                        EntriesToDeploy.Add(SC.StageDirectory.FullName);
                    }
                }

                // We now have a minimal set of file & dir entries we need
                // to deploy. Files we deploy will get individually copied
                // and dirs will get the tree copies by default (that's
                // what ADB does).
                HashSet<IProcessResult> DeployCommands = new HashSet<IProcessResult>();
                foreach (string Entry in EntriesToDeploy)
                {
                    string FinalRemoteDir = RemoteDir;
                    string RemotePath = Entry.Replace(SC.StageDirectory.FullName, FinalRemoteDir).Replace("\\", "/");
                    string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, Entry, RemotePath);
                    // We run deploy commands in parallel to maximize the connection
                    // throughput.
                    DeployCommands.Add(
                        RunAdbCommand(Params, DeviceName, Commandline, null,
                            ERunOptions.Default | ERunOptions.NoWaitForExit));
                    // But we limit the parallel commands to avoid overwhelming
                    // memory resources.
                    if (DeployCommands.Count == DeployMaxParallelCommands)
                    {
                        while (DeployCommands.Count > DeployMaxParallelCommands / 2)
                        {
                            Thread.Sleep(1);
                            DeployCommands.RemoveWhere(
                                delegate (IProcessResult r)
                                {
                                    return r.HasExited;
                                });
                        }
                    }
                }
                foreach (IProcessResult deploy_result in DeployCommands)
                {
                    deploy_result.WaitForExit();
                }

                // delete the .obb file, since it will cause nothing we just deployed to be used
                RunAdbCommand(Params, DeviceName, "shell rm " + DeviceObbName);
            }
            else if (SC.Archive)
            {
                // deploy the obb if there is one
                string ObbPath = Path.Combine(SC.StageDirectory.FullName, GetFinalObbName(ApkName));
                if (File.Exists(ObbPath))
                {
                    // cache some strings
                    string BaseCommandline = "push";

                    string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, ObbPath, DeviceObbName);
                    RunAdbCommand(Params, DeviceName, Commandline);
                }
            }
            else
            {
                // cache some strings
                string BaseCommandline = "push";

                string FinalRemoteDir = RemoteDir;
                /*
			    // handle the special case of the UE4Commandline.txt when using content only game (UE4Game)
			    if (!Params.IsCodeBasedProject)
			    {
				    FinalRemoteDir = "/mnt/sdcard/UE4Game";
			    }
			    */

                string RemoteFilename = IntermediateCmdLineFile.FullName.Replace(SC.StageDirectory.FullName, FinalRemoteDir).Replace("\\", "/");
                string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, IntermediateCmdLineFile, RemoteFilename);
                RunAdbCommand(Params, DeviceName, Commandline);
            }

            // write new timestamp for APK (do it here since RemoteDir will now exist)
            if (bNeedAPKInstall)
            {
                int SuccessCode = 0;
                RunAndLogAdbCommand(Params, DeviceName, "shell \"echo 'APK: " + APKLastUpdateTime + "' > " + RemoteDir + "/APKFileStamp.txt\"", out SuccessCode);
            }
        }
    }

	/** Internal usage for GetPackageName */
	private static string PackageLine = null;
	private static Mutex PackageInfoMutex = new Mutex();
	private static string LaunchableActivityLine = null;

	/** Run an external exe (and capture the output), given the exe path and the commandline. */
	private static string GetPackageInfo(string ApkName, bool bRetrieveVersionCode)
	{
		// we expect there to be one, so use the first one
		string AaptPath = GetAaptPath();

		PackageInfoMutex.WaitOne();

		var ExeInfo = new ProcessStartInfo(AaptPath, "dump badging \"" + ApkName + "\"");
		ExeInfo.UseShellExecute = false;
		ExeInfo.RedirectStandardOutput = true;
		using (var GameProcess = Process.Start(ExeInfo))
		{
			PackageLine = null;
			LaunchableActivityLine = null;
			GameProcess.BeginOutputReadLine();
			GameProcess.OutputDataReceived += ParsePackageName;
			GameProcess.WaitForExit();
		}

		PackageInfoMutex.ReleaseMutex();

		string ReturnValue = null;
		if (PackageLine != null)
		{
			// the line should look like: package: name='com.epicgames.qagame' versionCode='1' versionName='1.0'
			string[] Tokens = PackageLine.Split("'".ToCharArray());
			int TokenIndex = bRetrieveVersionCode ? 3 : 1;
			if (Tokens.Length >= TokenIndex + 1)
			{
				ReturnValue = Tokens[TokenIndex];
			}
		}
		return ReturnValue;
	}

	/** Returns the launch activity name to launch (must call GetPackageInfo first), returns "com.epicgames.ue4.SplashActivity" default if not found */
	private static string GetLaunchableActivityName()
	{
		string ReturnValue = "com.epicgames.ue4.SplashActivity";
		if (LaunchableActivityLine != null)
		{
			// the line should look like: launchable-activity: name='com.epicgames.ue4.SplashActivity'  label='TappyChicken' icon=''
			string[] Tokens = LaunchableActivityLine.Split("'".ToCharArray());
			if (Tokens.Length >= 2)
			{
				ReturnValue = Tokens[1];
			}
		}
		return ReturnValue;
	}

	/** Simple function to pipe output asynchronously */
	private static void ParsePackageName(object Sender, DataReceivedEventArgs Event)
	{
		// DataReceivedEventHandler is fired with a null string when the output stream is closed.  We don't want to
		// print anything for that event.
		if (!String.IsNullOrEmpty(Event.Data))
		{
			if (PackageLine == null)
			{
				string Line = Event.Data;
				if (Line.StartsWith("package:"))
				{
					PackageLine = Line;
				}
			}
			if (LaunchableActivityLine == null)
			{
				string Line = Event.Data;
				if (Line.StartsWith("launchable-activity:"))
				{
					LaunchableActivityLine = Line;
				}
			}
		}
	}

	static private string CachedAaptPath = null;
	static private string LastAndroidHomePath = null;

	private static uint GetRevisionValue(string VersionString)
	{
		// read up to 4 sections (ie. 20.0.3.5), first section most significant
		// each section assumed to be 0 to 255 range
		uint Value = 0;
		try
		{
			string[] Sections= VersionString.Split(".".ToCharArray());
			Value |= (Sections.Length > 0) ? (uint.Parse(Sections[0]) << 24) : 0;
			Value |= (Sections.Length > 1) ? (uint.Parse(Sections[1]) << 16) : 0;
			Value |= (Sections.Length > 2) ? (uint.Parse(Sections[2]) <<  8) : 0;
			Value |= (Sections.Length > 3) ?  uint.Parse(Sections[3])        : 0;
		}
		catch (Exception)
		{
			// ignore poorly formed version
		}
		return Value;
	}	

	private static string GetAaptPath()
	{
		// return cached path if ANDROID_HOME has not changed
        string HomePath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%");
		if (CachedAaptPath != null && LastAndroidHomePath == HomePath)
		{
			return CachedAaptPath;
		}

		// get a list of the directories in build-tools.. may be more than one set installed (or none which is bad)
		string[] Subdirs = Directory.GetDirectories(Path.Combine(HomePath, "build-tools"));
        if (Subdirs.Length == 0)
        {
            throw new AutomationException(ExitCode.Error_AndroidBuildToolsPathNotFound, "Failed to find %ANDROID_HOME%/build-tools subdirectory. Run SDK manager and install build-tools.");
        }

		// valid directories will have a source.properties with the Pkg.Revision (there is no guarantee we can use the directory name as revision)
		string BestToolPath = null;
		uint BestVersion = 0;
		foreach (string CandidateDir in Subdirs)
		{
			string AaptFilename = Path.Combine(CandidateDir, Utils.IsRunningOnMono ? "aapt" : "aapt.exe");
			uint RevisionValue = 0;

			if (File.Exists(AaptFilename))
			{
				string SourcePropFilename = Path.Combine(CandidateDir, "source.properties");
				if (File.Exists(SourcePropFilename))
				{
					string[] PropertyContents = File.ReadAllLines(SourcePropFilename);
					foreach (string PropertyLine in PropertyContents)
					{
						if (PropertyLine.StartsWith("Pkg.Revision="))
						{
							RevisionValue = GetRevisionValue(PropertyLine.Substring(13));
							break;
						}
					}
				}
			}

			// remember it if newer version or haven't found one yet
			if (RevisionValue > BestVersion || BestToolPath == null)
			{
				BestVersion = RevisionValue;
				BestToolPath = AaptFilename;
			}
		}

		if (BestToolPath == null)
		{
            throw new AutomationException(ExitCode.Error_AndroidBuildToolsPathNotFound, "Failed to find %ANDROID_HOME%/build-tools subdirectory with aapt. Run SDK manager and install build-tools.");
		}

		CachedAaptPath = BestToolPath;
		LastAndroidHomePath = HomePath;

		Log("Using this aapt: {0}", CachedAaptPath);

		return CachedAaptPath;
	}

	private string GetBestDeviceArchitecture(ProjectParams Params, string DeviceName)
	{
		bool bMakeSeparateApks = UnrealBuildTool.AndroidExports.ShouldMakeSeparateApks();
		// if we are joining all .so's into a single .apk, there's no need to find the best one - there is no other one
		if (!bMakeSeparateApks)
		{
			return "";
		}

		var AppArchitectures = AndroidExports.CreateToolChain(Params.RawProjectPath).GetAllArchitectures();

		// ask the device
		IProcessResult ABIResult = RunAdbCommand(Params, DeviceName, " shell getprop ro.product.cpu.abi", null, ERunOptions.AppMustExist);

		// the output is just the architecture
		string DeviceArch = UnrealBuildTool.AndroidExports.GetUE4Arch(ABIResult.Output.Trim());

		// if the architecture wasn't built, look for a backup
		if (!AppArchitectures.Contains(DeviceArch))
		{
			// go from 64 to 32-bit
			if (DeviceArch == "-arm64")
			{
				DeviceArch = "-armv7";
			}
			// go from 64 to 32-bit
			else if (DeviceArch == "-x64")
			{
				if (!AppArchitectures.Contains("-x86"))
				{
					DeviceArch = "-x86";
				}
				// if it didn't have 32-bit x86, look for 64-bit arm for emulation
				// @todo android 64-bit: x86_64 most likely can't emulate arm64 at this ponit
// 				else if (Array.IndexOf(AppArchitectures, "-arm64") == -1)
// 				{
// 					DeviceArch = "-arm64";
// 				}
				// finally try for 32-bit arm emulation (Houdini)
				else
				{
					DeviceArch = "-armv7";
				}
			}
			// use armv7 (with Houdini emulation)
			else if (DeviceArch == "-x86")
			{
				DeviceArch = "-armv7";
			}
            else
            {
                // future-proof by dropping back to armv7 for unknown
                DeviceArch = "-armv7";
            }
		}

		// if after the fallbacks, we still don't have it, we can't continue
		if (!AppArchitectures.Contains(DeviceArch))
		{
            throw new AutomationException(ExitCode.Error_NoApkSuitableForArchitecture, "Unable to run because you don't have an apk that is usable on {0}. Looked for {1}", DeviceName, DeviceArch);
		}

		return DeviceArch;
	}

	private string GetBestGPUArchitecture(ProjectParams Params, string DeviceName)
	{
		bool bMakeSeparateApks = UnrealBuildTool.AndroidExports.ShouldMakeSeparateApks();
		// if we are joining all .so's into a single .apk, there's no need to find the best one - there is no other one
		if (!bMakeSeparateApks)
		{
			return "";
		}

		return "-es2";
	}

	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		//make a copy of the device names, we'll be working through them
		List<string> DeviceNames = new List<string>();
		//same with the package names
		List<string> PackageNames = new List<string>();

		//strip off the device, GPU architecture and extension (.so)
		int DashIndex = ClientApp.LastIndexOf("-");
		if (DashIndex >= 0)
		{
			ClientApp = ClientApp.Substring(0, DashIndex);
			DashIndex = ClientApp.LastIndexOf("-");
			if (DashIndex >= 0)
			{
				ClientApp = ClientApp.Substring(0, DashIndex);
			}
		}

		foreach (string DeviceName in Params.DeviceNames)
		{
			//save the device name
			DeviceNames.Add(DeviceName);

			//get the package name and save that
			string DeviceArchitecture = GetBestDeviceArchitecture(Params, DeviceName);
			string GPUArchitecture = GetBestGPUArchitecture(Params, DeviceName);
			string ApkName = GetFinalApkName(Params, Path.GetFileNameWithoutExtension(ClientApp), true, DeviceArchitecture, GPUArchitecture);
			if (!File.Exists(ApkName))
			{
				throw new AutomationException(ExitCode.Error_AppNotFound, "Failed to find application " + ApkName);
			}
			Console.WriteLine("Apk='{0}', ClientApp='{1}', ExeName='{2}'", ApkName, ClientApp, Params.ProjectGameExeFilename);

			// run aapt to get the name of the intent
			string PackageName = GetPackageInfo(ApkName, false);
			if (PackageName == null)
			{
				throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package name from " + ClientApp);
			}

			PackageNames.Add(PackageName);

			// clear the log for the device
			RunAdbCommand(Params, DeviceName, "logcat -c");

			// start the app on device!
			string CommandLine = "shell am start -n " + PackageName + "/" + GetLaunchableActivityName();
			RunAdbCommand(Params, DeviceName, CommandLine, null, ClientRunFlags);

			// save the output to the staging directory
			string LogPath = Path.Combine(Params.BaseStageDirectory, "Android\\logs");
			Directory.CreateDirectory(LogPath);
		}

		//now check if each device still has the game running, and time out if it's taking too long
		DateTime StartTime = DateTime.Now;
		int TimeOutSeconds = Params.RunTimeoutSeconds;

		while (DeviceNames.Count > 0)
		{
			for(int DeviceIndex = 0; DeviceIndex < DeviceNames.Count; DeviceIndex++)
			{
				string DeviceName = DeviceNames[DeviceIndex];
				
				//replace the port name in the case of deploy while adb is using wifi
				string SanitizedDeviceName = DeviceName.Replace(":", "_");

				bool FinishedRunning = false;
				IProcessResult ProcessesResult = RunAdbCommand(Params, DeviceName, "shell ps", null, ERunOptions.SpewIsVerbose);

				string RunningProcessList = ProcessesResult.Output;
				if (!RunningProcessList.Contains(PackageNames[DeviceIndex]))
				{
					FinishedRunning = true;
				}

				Thread.Sleep(1000);

				if(!FinishedRunning)
				{
					TimeSpan DeltaRunTime = DateTime.Now - StartTime;
					if ((DeltaRunTime.TotalSeconds > TimeOutSeconds) && (TimeOutSeconds != 0))
					{
						Log("Device: " + DeviceName + " timed out while waiting for run to finish");
						FinishedRunning = true;
					}
				}

				//log the results, then clear out the device from our list
				if(FinishedRunning)
				{
					// this is just to get the ue4 log to go to the output
					RunAdbCommand(Params, DeviceName, "logcat -d -s UE4 -s Debug");

					// get the log we actually want to save
					IProcessResult LogFileProcess = RunAdbCommand(Params, DeviceName, "logcat -d", null, ERunOptions.AppMustExist);

					string LogPath = Path.Combine(Params.BaseStageDirectory, "Android\\logs");
					string LogFilename = Path.Combine(LogPath, "devicelog" + SanitizedDeviceName + ".log");
					string ServerLogFilename = Path.Combine(CmdEnv.LogFolder, "devicelog" + SanitizedDeviceName + ".log");

					File.WriteAllText(LogFilename, LogFileProcess.Output);
					File.WriteAllText(ServerLogFilename, LogFileProcess.Output);

					DeviceNames.RemoveAt(DeviceIndex);
					PackageNames.RemoveAt(DeviceIndex);

					--DeviceIndex;
				}
			}
		}

		return null;
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// Add any Android shader cache files
		DirectoryReference ProjectShaderDir = DirectoryReference.Combine(Params.RawProjectPath.Directory, "Build", "ShaderCaches", "Android");
		if(DirectoryReference.Exists(ProjectShaderDir))
		{
			SC.StageFiles(StagedFileType.UFS, ProjectShaderDir, StageFilesSearch.AllDirectories);
		}
	}

    /// <summary>
    /// Gets cook platform name for this platform.
    /// </summary>
    /// <returns>Cook platform string.</returns>
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return "Android";
	}

	public override bool DeployLowerCaseFilenames()
	{
		return false;
	}

	public override string LocalPathToTargetPath(string LocalPath, string LocalRoot)
	{
		return LocalPath.Replace("\\", "/").Replace(LocalRoot, "../../..");
	}

	public override bool IsSupported { get { return true; } }

	public override PakType RequiresPak(ProjectParams Params)
	{
		// if packaging is enabled, always create a pak, otherwise use the Params.Pak value
		return Params.Package ? PakType.Always : PakType.DontCare;
	}
    public override bool SupportsMultiDeviceDeploy
    {
        get
        {
            return true;
        }
    }

    /*
        public override bool RequiresPackageToDeploy
        {
            get { return true; }
        }
    */

	public override List<string> GetDebugFileExtentions()
	{
		return new List<string> { };
	}

	public override void StripSymbols(FileReference SourceFile, FileReference TargetFile)
	{
		AndroidExports.StripSymbols(SourceFile, TargetFile);
	}
}

public class AndroidPlatformMulti : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
        return "Android_Multi";
    }
    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "Multi");
    }
}

public class AndroidPlatformATC : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
        return "Android_ATC";
    }

    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "ATC");
    }
}

public class AndroidPlatformDXT : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
        return "Android_DXT";
    }

    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "DXT");
    }
}

public class AndroidPlatformETC1 : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
        return "Android_ETC1";
    }

    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "ETC1");
    }
}

public class AndroidPlatformETC2 : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
        return "Android_ETC2";
    }
    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "ETC2");
    }
}

public class AndroidPlatformPVRTC : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
        return "Android_PVRTC";
    }
    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "PVRTC");
    }
}

public class AndroidPlatformASTC : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
        return "Android_ASTC";
    }
    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "ASTC");
    }
}