// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using Ionic.Zip;
using Ionic.Zlib;
using System.Security.Principal;
using System.Threading;
using System.Diagnostics;
using Tools.DotNETCommon;
//using Manzana;

static class IOSEnvVarNames
{
	// Should we code sign when staging?  (defaults to 1 if not present)
	static public readonly string CodeSignWhenStaging = "uebp_CodeSignWhenStaging";
}

class IOSClientProcess : IProcessResult
{
	private IProcessResult	childProcess;
	private Thread			consoleLogWorker;
	//private bool			processConsoleLogs;
	
	public IOSClientProcess(IProcessResult inChildProcess, string inDeviceID)
	{
		childProcess = inChildProcess;
		
		// Startup another thread that collect device console logs
		//processConsoleLogs = true;
		consoleLogWorker = new Thread(() => ProcessConsoleOutput(inDeviceID));
		consoleLogWorker.Start();
	}
	
	public void StopProcess(bool KillDescendants = true)
	{
		childProcess.StopProcess(KillDescendants);
		StopConsoleOutput();
	}
	
	public bool HasExited
	{
		get
		{ 
			bool	result = childProcess.HasExited;
			
			if(result)
			{
				StopConsoleOutput();
			}
			
			return result; 
		}
	}
	
	public string GetProcessName()
	{
		return childProcess.GetProcessName();
	}

	public void OnProcessExited()
	{
		childProcess.OnProcessExited();
		StopConsoleOutput();
	}
	
	public void DisposeProcess()
	{
		childProcess.DisposeProcess();
	}
	
	public void StdOut(object sender, DataReceivedEventArgs e)
	{
		childProcess.StdOut(sender, e);
	}
	
	public void StdErr(object sender, DataReceivedEventArgs e)
	{
		childProcess.StdErr(sender, e);
	}
	
	public int ExitCode
	{
		get { return childProcess.ExitCode; }
		set { childProcess.ExitCode = value; }
	}
		
	public string Output
	{
		get { return childProcess.Output; }
	}

	public Process ProcessObject
	{
		get { return childProcess.ProcessObject; }
	}

	public new string ToString()
	{
		return childProcess.ToString();
	}
	
	public void WaitForExit()
	{
		childProcess.WaitForExit();
	}
	
	private void StopConsoleOutput()
	{
		//processConsoleLogs = false;
		consoleLogWorker.Join();
	}
	
	public void ProcessConsoleOutput(string inDeviceID)
	{		
// 		MobileDeviceInstance	targetDevice = null;
// 		foreach(MobileDeviceInstance curDevice in MobileDeviceInstanceManager.GetSnapshotInstanceList())
// 		{
// 			if(curDevice.DeviceId == inDeviceID)
// 			{
// 				targetDevice = curDevice;
// 				break;
// 			}
// 		}
// 		
// 		if(targetDevice == null)
// 		{
// 			return;
// 		}
// 		
// 		targetDevice.StartSyslogService();
// 		
// 		while(processConsoleLogs)
// 		{
// 			string logData = targetDevice.GetSyslogData();
// 			
// 			Console.WriteLine("DeviceLog: " + logData);
// 		}
// 		
// 		targetDevice.StopSyslogService();
	}

};

public class IOSPlatform : Platform
{
	bool bCreatedIPA = false;

	private string PlatformName = null;
	private string SDKName = null;

	public IOSPlatform()
		:this(UnrealTargetPlatform.IOS)
	{
	}

	public IOSPlatform(UnrealTargetPlatform TargetPlatform)
		:base(TargetPlatform)
	{
		PlatformName = TargetPlatform.ToString();
		SDKName = (TargetPlatform == UnrealTargetPlatform.TVOS) ? "appletvos" : "iphoneos";
	}

	// Run the integrated IPP code
	// @todo ipp: How to log the output in a nice UAT way?
	protected static int RunIPP(string IPPArguments)
	{
		List<string> Args = new List<string>();

		StringBuilder Token = null;
		int Index = 0;
		bool bInQuote = false;
		while (Index < IPPArguments.Length)
		{
			if (IPPArguments[Index] == '\"')
			{
				bInQuote = !bInQuote;
			}
			else if (IPPArguments[Index] == ' ' && !bInQuote)
			{
				if (Token != null)
				{
					Args.Add(Token.ToString());
					Token = null;
				}
			}
			else
			{
				if (Token == null)
				{
					Token = new StringBuilder();
				}
				Token.Append(IPPArguments[Index]);
			}
			Index++;
		}

		if (Token != null)
		{
			Args.Add(Token.ToString());
		}
	
		return RunIPP(Args.ToArray());
	}

	protected static int RunIPP(string[] Args)
	{
		return 5;//@TODO: Reintegrate IPP to IOS.Automation iPhonePackager.Program.RealMain(Args);
	}

	public override int RunCommand(string Command)
	{
		// run generic IPP commands through the interface
		if (Command.StartsWith("IPP:"))
		{
			return RunIPP(Command.Substring(4));
		}

		// non-zero error code
		return 4;
	}

	public virtual bool PrepForUATPackageOrDeploy(UnrealTargetConfiguration Config, FileReference ProjectFile, string InProjectName, DirectoryReference InProjectDirectory, string InExecutablePath, DirectoryReference InEngineDir, bool bForDistribution, string CookFlavor, bool bIsDataDeploy, bool bCreateStubIPA)
	{
		return IOSExports.PrepForUATPackageOrDeploy(Config, ProjectFile, InProjectName, InProjectDirectory, InExecutablePath, InEngineDir, bForDistribution, CookFlavor, bIsDataDeploy, bCreateStubIPA);
	}

    public virtual void GetProvisioningData(FileReference InProject, bool bDistribution, out string MobileProvision, out string SigningCertificate, out string TeamUUID, out bool bAutomaticSigning)
    {
		IOSExports.GetProvisioningData(InProject, bDistribution, out MobileProvision, out SigningCertificate, out TeamUUID, out bAutomaticSigning);
    }

	public virtual bool DeployGeneratePList(FileReference ProjectFile, UnrealTargetConfiguration Config, DirectoryReference ProjectDirectory, bool bIsUE4Game, string GameName, string ProjectName, DirectoryReference InEngineDir, DirectoryReference AppDirectory, out bool bSupportsPortrait, out bool bSupportsLandscape, out bool bSkipIcons)
	{
		return IOSExports.GeneratePList(ProjectFile, Config, ProjectDirectory, bIsUE4Game, GameName, ProjectName, InEngineDir, AppDirectory, out bSupportsPortrait, out bSupportsLandscape, out bSkipIcons);
	}

    protected string MakeIPAFileName( UnrealTargetConfiguration TargetConfiguration, ProjectParams Params )
	{
		string ProjectIPA = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", PlatformName, (Params.Distribution ? "Distro_" : "") + Params.ShortProjectName);
		if (TargetConfiguration != UnrealTargetConfiguration.Development)
		{
			ProjectIPA += "-" + PlatformType.ToString() + "-" + TargetConfiguration.ToString();
		}
		ProjectIPA += ".ipa";
		return ProjectIPA;
	}

	protected string MakeExeFileName( UnrealTargetConfiguration TargetConfiguration, ProjectParams Params )
	{
		string ProjectIPA = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", PlatformName, Params.ShortProjectName);
		if (TargetConfiguration != UnrealTargetConfiguration.Development)
		{
			ProjectIPA += "-" + PlatformType.ToString() + "-" + TargetConfiguration.ToString();
		}
		ProjectIPA += ".ipa";
		return ProjectIPA;
	}

	// Determine if we should code sign
	protected bool GetCodeSignDesirability(ProjectParams Params)
	{
		//@TODO: Would like to make this true, as it's the common case for everyone else
		bool bDefaultNeedsSign = true;

		bool bNeedsSign = false;
		string EnvVar = InternalUtils.GetEnvironmentVariable(IOSEnvVarNames.CodeSignWhenStaging, bDefaultNeedsSign ? "1" : "0", /*bQuiet=*/ false);
		if (!bool.TryParse(EnvVar, out bNeedsSign))
		{
			int BoolAsInt;
			if (int.TryParse(EnvVar, out BoolAsInt))
			{
				bNeedsSign = BoolAsInt != 0;
			}
			else
			{
				bNeedsSign = bDefaultNeedsSign;
			}
		}

		if (!String.IsNullOrEmpty(Params.BundleName))
		{
			// Have to sign when a bundle name is specified
			bNeedsSign = true;
		}

		return bNeedsSign;
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		Log("Package {0}", Params.RawProjectPath);

		// ensure the ue4game binary exists, if applicable
		string FullExePath = CombinePaths(Path.GetDirectoryName(Params.ProjectGameExeFilename), SC.StageExecutables[0] + (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac ? ".stub" : ""));
		if (!SC.IsCodeBasedProject && !FileExists_NoExceptions(FullExePath))
		{
			LogError("Failed to find game binary " + FullExePath);
			throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Could not find binary {0}. You may need to build the UE4 project with your target configuration and platform.", FullExePath);
		}

        if (SC.StageTargetConfigurations.Count != 1)
        {
            throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
        }

        var TargetConfiguration = SC.StageTargetConfigurations[0];

		string MobileProvision;
		string SigningCertificate;
		string TeamUUID;
		bool bAutomaticSigning;
        GetProvisioningData(Params.RawProjectPath, Params.Distribution, out MobileProvision, out SigningCertificate, out TeamUUID, out bAutomaticSigning);

        //@TODO: We should be able to use this code on both platforms, when the following issues are sorted:
        //   - Raw executable is unsigned & unstripped (need to investigate adding stripping to IPP)
        //   - IPP needs to be able to codesign a raw directory
        //   - IPP needs to be able to take a .app directory instead of a Payload directory when doing RepackageFromStage (which would probably be renamed)
        //   - Some discrepancy in the loading screen pngs that are getting packaged, which needs to be investigated
        //   - Code here probably needs to be updated to write 0 byte files as 1 byte (difference with IPP, was required at one point when using Ionic.Zip to prevent issues on device, maybe not needed anymore?)
        if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
            // copy in all of the artwork and plist
            PrepForUATPackageOrDeploy(TargetConfiguration, Params.RawProjectPath,
				Params.ShortProjectName,
				Params.RawProjectPath.Directory,
				CombinePaths(Path.GetDirectoryName(Params.ProjectGameExeFilename), SC.StageExecutables[0]),
				DirectoryReference.Combine(SC.LocalRoot, "Engine"),
				Params.Distribution, 
				"",
				false, 
				false);

			// figure out where to pop in the staged files
			string AppDirectory = string.Format("{0}/Payload/{1}.app",
				Path.GetDirectoryName(Params.ProjectGameExeFilename),
				Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename));

			// delete the old cookeddata
			InternalUtils.SafeDeleteDirectory(AppDirectory + "/cookeddata", true);
			InternalUtils.SafeDeleteFile(AppDirectory + "/ue4commandline.txt", true);

			if (!Params.IterativeDeploy)
			{
				// copy the Staged files to the AppDirectory
				string[] StagedFiles = Directory.GetFiles (SC.StageDirectory.FullName, "*", SearchOption.AllDirectories);
				foreach (string Filename in StagedFiles)
				{
					string DestFilename = Filename.Replace (SC.StageDirectory.FullName, AppDirectory);
					Directory.CreateDirectory (Path.GetDirectoryName (DestFilename));
					InternalUtils.SafeCopyFile (Filename, DestFilename, true);
				}
			}
			else
			{
				// copy just the root stage directory files
				string[] StagedFiles = Directory.GetFiles (SC.StageDirectory.FullName, "*", SearchOption.TopDirectoryOnly);
				foreach (string Filename in StagedFiles)
				{
					string DestFilename = Filename.Replace (SC.StageDirectory.FullName, AppDirectory);
					Directory.CreateDirectory (Path.GetDirectoryName (DestFilename));
					InternalUtils.SafeCopyFile (Filename, DestFilename, true);
				}
			}
		}

        bCreatedIPA = false;
		bool bNeedsIPA = false;
		if (Params.IterativeDeploy)
		{
            if (Params.Devices.Count != 1)
            {
                throw new AutomationException("Can only interatively deploy to a single device, but {0} were specified", Params.Devices.Count);
            }
            
            String NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath(Params.DeviceNames[0]);
			// check to determine if we need to update the IPA
			if (File.Exists(NonUFSManifestPath))
			{
				string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
				string[] Lines = NonUFSFiles.Split('\n');
				bNeedsIPA = Lines.Length > 0 && !string.IsNullOrWhiteSpace(Lines[0]);
			}
		}

		if (String.IsNullOrEmpty(Params.Provision))
		{
			Params.Provision = MobileProvision;
		}
		if (String.IsNullOrEmpty(Params.Certificate))
		{
			Params.Certificate = SigningCertificate;
		}
		if (String.IsNullOrEmpty(Params.Team))
		{
			Params.Team = TeamUUID;
		}

		Params.AutomaticSigning = bAutomaticSigning;

		// Scheme name and configuration for code signing with Xcode project
		string SchemeName = Params.IsCodeBasedProject ? Params.RawProjectPath.GetFileNameWithoutExtension() : "UE4";
		string SchemeConfiguration = TargetConfiguration.ToString();
		if (Params.Client)
		{
			SchemeConfiguration += " Client";
		}

		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
		{
			var ProjectIPA = MakeIPAFileName(TargetConfiguration, Params);
			var ProjectStub = Path.GetFullPath(Params.ProjectGameExeFilename);

			// package a .ipa from the now staged directory
			var IPPExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/IPhonePackager.exe");

			LogLog("ProjectName={0}", Params.ShortProjectName);
			LogLog("ProjectStub={0}", ProjectStub);
			LogLog("ProjectIPA={0}", ProjectIPA);
			LogLog("IPPExe={0}", IPPExe);

			bool cookonthefly = Params.CookOnTheFly || Params.SkipCookOnTheFly;

			// if we are incremental check to see if we need to even update the IPA
			if (!Params.IterativeDeploy || !File.Exists(ProjectIPA) || bNeedsIPA)
			{
				// delete the .ipa to make sure it was made
				DeleteFile(ProjectIPA);

				bCreatedIPA = true;

				if (IOSExports.UseRPCUtil())
				{
					string IPPArguments = "RepackageFromStage \"" + (Params.IsCodeBasedProject ? Params.RawProjectPath.FullName : "Engine") + "\"";
					IPPArguments += " -config " + TargetConfiguration.ToString();
					IPPArguments += " -schemename " + SchemeName + " -schemeconfig \"" + SchemeConfiguration + "\"";

					if (TargetConfiguration == UnrealTargetConfiguration.Shipping)
					{
						IPPArguments += " -compress=best";
					}

					// Determine if we should sign
					bool bNeedToSign = GetCodeSignDesirability(Params);

					if (!String.IsNullOrEmpty(Params.BundleName))
					{
						// Have to sign when a bundle name is specified
						bNeedToSign = true;
						IPPArguments += " -bundlename " + Params.BundleName;
					}

					if (bNeedToSign)
					{
						IPPArguments += " -sign";
						if (Params.Distribution)
						{
							IPPArguments += " -distribution";
						}
					}

					IPPArguments += (cookonthefly ? " -cookonthefly" : "");
					IPPArguments += " -stagedir \"" + CombinePaths(Params.BaseStageDirectory, PlatformName) + "\"";
					IPPArguments += " -project \"" + Params.RawProjectPath + "\"";
					if (Params.IterativeDeploy)
					{
						IPPArguments += " -iterate";
					}
					if (!string.IsNullOrEmpty(Params.Provision))
					{
						IPPArguments += " -provision \"" + Params.Provision + "\""; 
					}
					if (!string.IsNullOrEmpty(Params.Certificate))
					{
						IPPArguments += " -certificate \"" + Params.Certificate + "\"";
					}
					if (PlatformName == "TVOS")
					{
						IPPArguments += " -tvos";
					}
					RunAndLog(CmdEnv, IPPExe, IPPArguments);
				}
				else
				{
					List<string> IPPArguments = new List<string>();
					IPPArguments.Add("RepackageFromStage");
					IPPArguments.Add(Params.IsCodeBasedProject ? Params.RawProjectPath.FullName : "Engine");
					IPPArguments.Add("-config");
					IPPArguments.Add(TargetConfiguration.ToString());
					IPPArguments.Add("-schemename");
					IPPArguments.Add(SchemeName);
					IPPArguments.Add("-schemeconfig");
					IPPArguments.Add("\"" + SchemeConfiguration + "\"");

					if (TargetConfiguration == UnrealTargetConfiguration.Shipping)
					{
						IPPArguments.Add("-compress=best");
					}

					// Determine if we should sign
					bool bNeedToSign = GetCodeSignDesirability(Params);

					if (!String.IsNullOrEmpty(Params.BundleName))
					{
						// Have to sign when a bundle name is specified
						bNeedToSign = true;
						IPPArguments.Add("-bundlename");
						IPPArguments.Add(Params.BundleName);
					}

					if (bNeedToSign)
					{
						IPPArguments.Add("-sign");
					}

					if (cookonthefly)
					{
						IPPArguments.Add(" -cookonthefly");
					}
					IPPArguments.Add(" -stagedir");
					IPPArguments.Add(CombinePaths(Params.BaseStageDirectory, "IOS"));
					IPPArguments.Add(" -project");
					IPPArguments.Add(Params.RawProjectPath.FullName);
					if (Params.IterativeDeploy)
					{
						IPPArguments.Add(" -iterate");
					}
					if (!string.IsNullOrEmpty(Params.Provision))
					{
						IPPArguments.Add(" -provision");
						IPPArguments.Add(Params.Provision);
					}
					if (!string.IsNullOrEmpty(Params.Certificate))
					{
						IPPArguments.Add(" -certificate");
						IPPArguments.Add(Params.Certificate);
					}

					if (RunIPP(IPPArguments.ToArray()) != 0)
					{
						throw new AutomationException("IPP Failed");
					}
				}
			}

			// verify the .ipa exists
			if (!FileExists(ProjectIPA))
			{
				throw new AutomationException(ExitCode.Error_FailedToCreateIPA, "PACKAGE FAILED - {0} was not created", ProjectIPA);
			}

			if (WorkingCL > 0)
			{
				// Open files for add or edit
				var ExtraFilesToCheckin = new List<string>
				{
					ProjectIPA
				};

				// check in the .ipa along with everything else
				UE4Build.AddBuildProductsToChangelist(WorkingCL, ExtraFilesToCheckin);
			}

			//@TODO: This automatically deploys after packaging, useful for testing on PC when iterating on IPP
			//Deploy(Params, SC);
		}
		else
		{
			// create the ipa
			string IPAName = CombinePaths(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", PlatformName, (Params.Distribution ? "Distro_" : "") + Params.ShortProjectName + (SC.StageTargetConfigurations[0] != UnrealTargetConfiguration.Development ? ("-" + PlatformName + "-" + SC.StageTargetConfigurations[0].ToString()) : "") + ".ipa");

			if (!Params.IterativeDeploy || !File.Exists(IPAName) || bNeedsIPA)
			{
				bCreatedIPA = true;

				// code sign the app
				CodeSign(Path.GetDirectoryName(Params.ProjectGameExeFilename), Params.IsCodeBasedProject ? Params.ShortProjectName : Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename), Params.RawProjectPath, SC.StageTargetConfigurations[0], SC.LocalRoot.FullName, Params.ShortProjectName, Path.GetDirectoryName(Params.RawProjectPath.FullName), SC.IsCodeBasedProject, Params.Distribution, Params.Provision, Params.Certificate, Params.Team, Params.AutomaticSigning, SchemeName, SchemeConfiguration);

				// now generate the ipa
				PackageIPA(Path.GetDirectoryName(Params.ProjectGameExeFilename), Params.IsCodeBasedProject ? Params.ShortProjectName : Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename), Params.ShortProjectName, Path.GetDirectoryName(Params.RawProjectPath.FullName), SC.StageTargetConfigurations[0], Params.Distribution);
			}
		}

		PrintRunTime();
	}

	private string EnsureXcodeProjectExists(FileReference RawProjectPath, string LocalRoot, string ShortProjectName, string ProjectRoot, bool IsCodeBasedProject, out bool bWasGenerated)
	{
		// first check for ue4.xcodeproj
		bWasGenerated = false;
		string RawProjectDir = RawProjectPath.Directory.FullName;
		string XcodeProj = RawProjectPath.FullName.Replace(".uproject", "_" + PlatformName + ".xcworkspace");
		if (!Directory.Exists(RawProjectDir + "/Source") && !Directory.Exists(RawProjectDir + "/Intermediate/Source"))
		{
			XcodeProj = CombinePaths(CmdEnv.LocalRoot, "Engine", Path.GetFileName(XcodeProj));
		}
		Console.WriteLine ("Project: " + XcodeProj);
		{
			// project.xcodeproj doesn't exist, so generate temp project
			string Arguments = "-project=\"" + RawProjectPath + "\"";
			Arguments += " -platforms=" + PlatformName + " -game -nointellisense -" + PlatformName + "deployonly -ignorejunk";

			// If engine is installed then UBT doesn't need to be built
			if (Automation.IsEngineInstalled())
			{
				// Get the path to UBT
				string InstalledUBT = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/UnrealBuildTool.exe");
				Arguments = "-XcodeProjectFiles " + Arguments;
				RunUBT(CmdEnv, InstalledUBT, Arguments);
			}
			else
			{
				string Script = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh");
				string CWD = Directory.GetCurrentDirectory();
				Directory.SetCurrentDirectory(Path.GetDirectoryName(Script));
				Run(Script, Arguments, null, ERunOptions.Default);
				Directory.SetCurrentDirectory(CWD);
			}

			bWasGenerated = true;

			if (!Directory.Exists (XcodeProj))
			{
				// something very bad happened
				throw new AutomationException("iOS couldn't find the appropriate Xcode Project " + XcodeProj);
			}
		}

		return XcodeProj;
	}

	private void CodeSign(string BaseDirectory, string GameName, FileReference RawProjectPath, UnrealTargetConfiguration TargetConfig, string LocalRoot, string ProjectName, string ProjectDirectory, bool IsCode, bool Distribution = false, string Provision = null, string Certificate = null, string Team = null, bool bAutomaticSigning = false, string SchemeName = null, string SchemeConfiguration = null)
	{
		// check for the proper xcodeproject
		bool bWasGenerated = false;
		string XcodeProj = EnsureXcodeProjectExists (RawProjectPath, LocalRoot, ProjectName, ProjectDirectory, IsCode, out bWasGenerated);

		string Arguments = "UBT_NO_POST_DEPLOY=true";
		Arguments += " /usr/bin/xcrun xcodebuild build -workspace \"" + XcodeProj + "\"";
		Arguments += " -scheme '";
		Arguments += SchemeName != null ? SchemeName : GameName;
		Arguments += "'";
		Arguments += " -configuration \"" + (SchemeConfiguration != null ? SchemeConfiguration : TargetConfig.ToString()) + "\"";
		Arguments += " -destination generic/platform=" + (PlatformName == "TVOS" ? "tvOS" : "iOS");
		Arguments += " -sdk " + SDKName;

		if (bAutomaticSigning)
		{
			Arguments += " DEVELOPMENT_TEAM=\"" + Team + "\"";
			Arguments += " CODE_SIGN_IDENTITY=\"iPhone Developer\"";
		}
		else
		{
			if (!string.IsNullOrEmpty(Certificate))
			{
				Arguments += " CODE_SIGN_IDENTITY=\"" + Certificate + "\"";
			}
			else
			{
				Arguments += " CODE_SIGN_IDENTITY=" + (Distribution ? "\"iPhone Distribution\"" : "\"iPhone Developer\"");
			}
			if (!string.IsNullOrEmpty(Provision))
			{
				// read the provision to get the UUID
				if (File.Exists(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Provision))
				{
					string UUID = "";
					string AllText = File.ReadAllText(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Provision);
					int idx = AllText.IndexOf("<key>UUID</key>");
					if (idx > 0)
					{
						idx = AllText.IndexOf("<string>", idx);
						if (idx > 0)
						{
							idx += "<string>".Length;
							UUID = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
	                        Arguments += " PROVISIONING_PROFILE_SPECIFIER=" + UUID;
	                    }
	                }
				}
			}
		}
		IProcessResult Result = Run ("/usr/bin/env", Arguments, null, ERunOptions.Default);
		if (bWasGenerated)
		{
			InternalUtils.SafeDeleteDirectory( XcodeProj, true);
		}
		if (Result.ExitCode != 0)
		{
			throw new AutomationException(ExitCode.Error_FailedToCodeSign, "CodeSign Failed");
		}
	}

	private void PackageIPA(string BaseDirectory, string GameName, string ProjectName, string ProjectDirectory, UnrealTargetConfiguration TargetConfig, bool Distribution = false)
	{
		// create the ipa
		string IPAName = CombinePaths(ProjectDirectory, "Binaries", PlatformName, (Distribution ? "Distro_" : "") + ProjectName + (TargetConfig != UnrealTargetConfiguration.Development ? ("-" + PlatformName + "-" + TargetConfig.ToString()) : "") + ".ipa");
		// delete the old one
		if (File.Exists(IPAName))
		{
			File.Delete(IPAName);
		}

		// make the subdirectory if needed
		string DestSubdir = Path.GetDirectoryName(IPAName);
		if (!Directory.Exists(DestSubdir))
		{
			Directory.CreateDirectory(DestSubdir);
		}

		// set up the directories
		string ZipWorkingDir = String.Format("Payload/{0}.app/", GameName);
		string ZipSourceDir = string.Format("{0}/Payload/{1}.app", BaseDirectory, GameName);

		// create the file
		using (ZipFile Zip = new ZipFile())
		{
            // Set encoding to support unicode filenames
            Zip.AlternateEncodingUsage = ZipOption.Always;
            Zip.AlternateEncoding = Encoding.UTF8;

			// set the compression level
			if (Distribution)
			{
				Zip.CompressionLevel = CompressionLevel.BestCompression;
			}

			// add the entire directory
			Zip.AddDirectory(ZipSourceDir, ZipWorkingDir);

			// Update permissions to be UNIX-style
			// Modify the file attributes of any added file to unix format
			foreach (ZipEntry E in Zip.Entries)
			{
				const byte FileAttributePlatform_NTFS = 0x0A;
				const byte FileAttributePlatform_UNIX = 0x03;
				const byte FileAttributePlatform_FAT = 0x00;

				const int UNIX_FILETYPE_NORMAL_FILE = 0x8000;
				//const int UNIX_FILETYPE_SOCKET = 0xC000;
				//const int UNIX_FILETYPE_SYMLINK = 0xA000;
				//const int UNIX_FILETYPE_BLOCKSPECIAL = 0x6000;
				const int UNIX_FILETYPE_DIRECTORY = 0x4000;
				//const int UNIX_FILETYPE_CHARSPECIAL = 0x2000;
				//const int UNIX_FILETYPE_FIFO = 0x1000;

				const int UNIX_EXEC = 1;
				const int UNIX_WRITE = 2;
				const int UNIX_READ = 4;


				int MyPermissions = UNIX_READ | UNIX_WRITE;
				int OtherPermissions = UNIX_READ;

				int PlatformEncodedBy = (E.VersionMadeBy >> 8) & 0xFF;
				int LowerBits = 0;

				// Try to preserve read-only if it was set
				bool bIsDirectory = E.IsDirectory;

				// Check to see if this 
				bool bIsExecutable = false;
				if (Path.GetFileNameWithoutExtension(E.FileName).Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
				{
					bIsExecutable = true;
				}

				if (bIsExecutable)
				{
					// The executable will be encrypted in the final distribution IPA and will compress very poorly, so keeping it
					// uncompressed gives a better indicator of IPA size for our distro builds
					E.CompressionLevel = CompressionLevel.None;
				}

				if ((PlatformEncodedBy == FileAttributePlatform_NTFS) || (PlatformEncodedBy == FileAttributePlatform_FAT))
				{
					FileAttributes OldAttributes = E.Attributes;
					//LowerBits = ((int)E.Attributes) & 0xFFFF;

					if ((OldAttributes & FileAttributes.Directory) != 0)
					{
						bIsDirectory = true;
					}

					// Permissions
					if ((OldAttributes & FileAttributes.ReadOnly) != 0)
					{
						MyPermissions &= ~UNIX_WRITE;
						OtherPermissions &= ~UNIX_WRITE;
					}
				}

				if (bIsDirectory || bIsExecutable)
				{
					MyPermissions |= UNIX_EXEC;
					OtherPermissions |= UNIX_EXEC;
				}

				// Re-jigger the external file attributes to UNIX style if they're not already that way
				if (PlatformEncodedBy != FileAttributePlatform_UNIX)
				{
					int NewAttributes = bIsDirectory ? UNIX_FILETYPE_DIRECTORY : UNIX_FILETYPE_NORMAL_FILE;

					NewAttributes |= (MyPermissions << 6);
					NewAttributes |= (OtherPermissions << 3);
					NewAttributes |= (OtherPermissions << 0);

					// Now modify the properties
					E.AdjustExternalFileAttributes(FileAttributePlatform_UNIX, (NewAttributes << 16) | LowerBits);
				}
			}

			// Save it out
			Zip.Save(IPAName);
		}
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		//		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
		{

			// copy any additional framework assets that will be needed at runtime
			{
				DirectoryReference SourcePath = DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), "Intermediate", "IOS", "FrameworkAssets");
				if ( DirectoryReference.Exists( SourcePath ) )
				{
					SC.StageFiles(StagedFileType.SystemNonUFS, SourcePath, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
				}
			}


			// copy the plist (only if code signing, as it's protected by the code sign blob in the executable and can't be modified independently)
			if (GetCodeSignDesirability(Params))
			{
				DirectoryReference SourcePath = DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : DirectoryReference.Combine(SC.LocalRoot, "Engine")), "Intermediate", PlatformName);
				FileReference TargetPListFile = FileReference.Combine(SourcePath, (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game") + "-Info.plist");
//				if (!File.Exists(TargetPListFile))
				{
					// ensure the plist, entitlements, and provision files are properly copied
					Console.WriteLine("CookPlat {0}, this {1}", GetCookPlatform(false, false), ToString());
					if (!SC.IsCodeBasedProject)
					{
						UnrealBuildTool.PlatformExports.SetRemoteIniPath(SC.ProjectRoot.FullName);
					}

                    if (SC.StageTargetConfigurations.Count != 1)
                    {
                        throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
                    }

                    var TargetConfiguration = SC.StageTargetConfigurations[0];
                    bool bSupportsPortrait = false;
                    bool bSupportsLandscape = false;
                    bool bSkipIcons = false;

                    DeployGeneratePList(
							SC.RawProjectPath,	
							TargetConfiguration,
                            (SC.IsCodeBasedProject ? SC.ProjectRoot : DirectoryReference.Combine(SC.LocalRoot, "Engine")),
                            !SC.IsCodeBasedProject,
                            (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game"),
                            SC.ShortProjectName, DirectoryReference.Combine(SC.LocalRoot, "Engine"),
														DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : DirectoryReference.Combine(SC.LocalRoot, "Engine")), "Binaries", PlatformName, "Payload", (SC.IsCodeBasedProject ? SC.ShortProjectName : "UE4Game") + ".app"),
                            out bSupportsPortrait,
                            out bSupportsLandscape,
                            out bSkipIcons);

                    // copy the plist to the stage dir
                    SC.StageFile(StagedFileType.SystemNonUFS, TargetPListFile, new StagedFileReference("Info.plist"));

                    // copy the icons/launch screens from the engine
                    {
                        DirectoryReference DataPath = DirectoryReference.Combine(SC.EngineRoot, "Build", "IOS", "Resources", "Graphics");
						StageImageAndIconFiles(DataPath, bSupportsPortrait, bSupportsLandscape, SC, bSkipIcons);
					}

                    // copy the icons/launch screens from the game (may stomp the engine copies)
                    {
                        DirectoryReference DataPath = DirectoryReference.Combine(SC.ProjectRoot, "Build", "IOS", "Resources", "Graphics");
						StageImageAndIconFiles(DataPath, bSupportsPortrait, bSupportsLandscape, SC, bSkipIcons);
                    }
                }

                // copy the udebugsymbols if they exist
                {
                    ConfigHierarchy PlatformGameConfig;
                    bool bIncludeSymbols = false;
                    if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
                    {
                        PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGenerateCrashReportSymbols", out bIncludeSymbols);
                    }
                    if (bIncludeSymbols)
                    {
                        FileReference SymbolFileName = FileReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), "Binaries", "IOS", SC.StageExecutables[0] + ".udebugsymbols");
						if(FileReference.Exists(SymbolFileName))
						{
							SC.StageFile(StagedFileType.NonUFS, SymbolFileName, new StagedFileReference((Params.ShortProjectName + ".udebugsymbols").ToLowerInvariant()));
						}
                    }
                }
			}
		}
        {
			StageMovieFiles(DirectoryReference.Combine(SC.EngineRoot, "Content", "Movies"), SC);
			StageMovieFiles(DirectoryReference.Combine(SC.ProjectRoot, "Content", "Movies"), SC);
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
			if (DirectoryReference.Exists(CookOutputDir))
			{
				List<FileReference> CookedFiles = DirectoryReference.EnumerateFiles(CookOutputDir, "*.metallib", SearchOption.AllDirectories).ToList();
				foreach (FileReference CookedFile in CookedFiles)
				{
					SC.StageFile(StagedFileType.NonUFS, CookedFile, new StagedFileReference(CookedFile.MakeRelativeTo(CookOutputDir)));
				}
			}
		}
    }

	private void StageImageAndIconFiles(DirectoryReference DataPath, bool bSupportsPortrait, bool bSupportsLandscape, DeploymentContext SC, bool bSkipIcons)
	{
		if(DirectoryReference.Exists(DataPath))
		{
			List<string> ImageFileNames = new List<string>();
			if (bSupportsPortrait)
			{
				ImageFileNames.Add("Default-IPhone6.png");
				ImageFileNames.Add("Default-IPhone6Plus-Portrait.png");
				ImageFileNames.Add("Default-Portrait@2x.png");
				ImageFileNames.Add("Default-Portrait-1336@2x.png");
                ImageFileNames.Add("Default-IPhoneX-Portrait.png");
            }
			if (bSupportsLandscape)
			{
				ImageFileNames.Add("Default-IPhone6-Landscape.png");
				ImageFileNames.Add("Default-IPhone6Plus-Landscape.png");
				ImageFileNames.Add("Default-Landscape@2x.png");
				ImageFileNames.Add("Default-Landscape-1336@2x.png");
                ImageFileNames.Add("Default-IPhoneX-Landscape.png");
            }
			ImageFileNames.Add("Default@2x.png");
			ImageFileNames.Add("Default-568h@2x.png");

			foreach(string ImageFileName in ImageFileNames)
			{
				FileReference ImageFile = FileReference.Combine(DataPath, ImageFileName);
				if(FileReference.Exists(ImageFile))
				{
					SC.StageFile(StagedFileType.SystemNonUFS, ImageFile, new StagedFileReference(ImageFileName));
				}
			}

            if (!bSkipIcons)
            {
                SC.StageFiles(StagedFileType.SystemNonUFS, DataPath, "Icon*.png", StageFilesSearch.TopDirectoryOnly, StagedDirectoryReference.Root);
            }
		}
	}

	private void StageMovieFiles(DirectoryReference InputDir, DeploymentContext SC)
	{
		if(DirectoryReference.Exists(InputDir))
		{
			foreach(FileReference InputFile in DirectoryReference.EnumerateFiles(InputDir, "*", SearchOption.AllDirectories))
			{
				if(!InputFile.HasExtension(".uasset") && !InputFile.HasExtension(".umap"))
				{
					SC.StageFile(StagedFileType.NonUFS, InputFile);
				}
			}
		}
	}

    public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}
		var TargetConfiguration = SC.StageTargetConfigurations[0];
		var ProjectIPA = MakeIPAFileName( TargetConfiguration, Params );

		// verify the .ipa exists
		if (!FileExists(ProjectIPA))
		{
			throw new AutomationException("ARCHIVE FAILED - {0} was not found", ProjectIPA);
		}

		ConfigHierarchy PlatformGameConfig;
		bool bXCArchive = false;
		if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGenerateXCArchive", out bXCArchive);
		}

		if (bXCArchive && Utils.IsRunningOnMono)
		{
			// Always put the archive in the current user's Library/Developer/Xcode/Archives path if not on the build machine
			WindowsIdentity id = WindowsIdentity.GetCurrent(); 
			string ArchivePath = "/Users/" + id.Name + "/Library/Developer/Xcode/Archives";
            if (IsBuildMachine)
            {
                ArchivePath = Params.ArchiveDirectoryParam;
            }
			if (!DirectoryExists(ArchivePath))
			{
				CreateDirectory(ArchivePath);
			}

			Console.WriteLine("Generating xc archive package in " + ArchivePath);

			string ArchiveName = Path.Combine(ArchivePath, Path.GetFileNameWithoutExtension(ProjectIPA) + ".xcarchive");
			if (!DirectoryExists(ArchiveName))
			{
				CreateDirectory(ArchiveName);
			}
			DeleteDirectoryContents(ArchiveName);

			// create the Products archive folder
			CreateDirectory(Path.Combine(ArchiveName, "Products", "Applications"));

			// copy in the application
			string AppName = Path.GetFileNameWithoutExtension(ProjectIPA) + ".app";
			using (ZipFile Zip = new ZipFile(ProjectIPA))
			{
				Zip.ExtractAll(ArchivePath, ExtractExistingFileAction.OverwriteSilently);

				List<string> Dirs = new List<string>(Directory.EnumerateDirectories(Path.Combine(ArchivePath, "Payload"), "*.app"));
				AppName = Dirs[0].Substring(Dirs[0].LastIndexOf(UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac ? "\\" : "/") + 1);
				CopyDirectory_NoExceptions(Path.Combine(ArchivePath, "Payload", AppName), Path.Combine(ArchiveName, "Products", "Applications", AppName));
			}

			// copy in the dSYM if found
			var ProjectExe = MakeExeFileName( TargetConfiguration, Params );
			string dSYMName = (SC.IsCodeBasedProject ? Path.GetFileNameWithoutExtension(ProjectExe) : "UE4Game") + ".dSYM";
            string dSYMDestName = AppName + ".dSYM";
            string dSYMSrcPath = Path.Combine(SC.ProjectBinariesFolder.FullName, dSYMName);
            string dSYMZipSrcPath = Path.Combine(SC.ProjectBinariesFolder.FullName, dSYMName + ".zip");
            if (File.Exists(dSYMZipSrcPath))
            {
                // unzip the dsym
                using (ZipFile Zip = new ZipFile(dSYMZipSrcPath))
                {
                    Zip.ExtractAll(SC.ProjectBinariesFolder.FullName, ExtractExistingFileAction.OverwriteSilently);
                }
            }

			if(DirectoryExists(dSYMSrcPath))
			{
				// Create the dsyms archive folder
				CreateDirectory(Path.Combine(ArchiveName, "dSYMs"));
				string dSYMDstPath = Path.Combine(ArchiveName, "dSYMs", dSYMDestName);
				// /Volumes/MacOSDrive1/pfEpicWorkspace/Dev-Platform/Samples/Sandbox/PlatformShowcase/Binaries/IOS/PlatformShowcase.dSYM/Contents/Resources/DWARF/PlatformShowcase
				CopyFile_NoExceptions(Path.Combine(dSYMSrcPath, "Contents", "Resources", "DWARF", SC.IsCodeBasedProject ? Path.GetFileNameWithoutExtension(ProjectExe) : "UE4Game"), dSYMDstPath);
			}
			else if (File.Exists(dSYMSrcPath))
			{
				// Create the dsyms archive folder
				CreateDirectory(Path.Combine(ArchiveName, "dSYMs"));
				string dSYMDstPath = Path.Combine(ArchiveName, "dSYMs", dSYMDestName);
				CopyFile_NoExceptions(dSYMSrcPath, dSYMDstPath);
			}

			// copy in the bitcode symbol maps if found
			string[] bcmapfiles = Directory.GetFiles(SC.ProjectBinariesFolder.FullName, "*.bcsymbolmap");
			if(bcmapfiles.Length > 0)
			{
				// Create the dsyms archive folder
				CreateDirectory(Path.Combine(ArchiveName, "BCSymbolMaps"));
				foreach (string symbolSrcFilePath in bcmapfiles)
				{
					string symbolLeafFileName = Path.GetFileName(symbolSrcFilePath);
					string bcDstFilePath = Path.Combine(ArchiveName, "BCSymbolMaps", symbolLeafFileName);
					CopyFile_NoExceptions(symbolSrcFilePath, bcDstFilePath);
				}
			}

			// get the settings from the app plist file
			string AppPlist = Path.Combine(ArchiveName, "Products", "Applications", AppName, "Info.plist");
			string OldPListData = File.Exists(AppPlist) ? File.ReadAllText(AppPlist) : "";

			// bundle identifier
			int index = OldPListData.IndexOf("CFBundleIdentifier");
			index = OldPListData.IndexOf("<string>", index) + 8;
			int length = OldPListData.IndexOf("</string>", index) - index;
			string BundleIdentifier = OldPListData.Substring(index, length);

			// short version
			index = OldPListData.IndexOf("CFBundleShortVersionString");
			index = OldPListData.IndexOf("<string>", index) + 8;
			length = OldPListData.IndexOf("</string>", index) - index;
			string BundleShortVersion = OldPListData.Substring(index, length);

			// bundle version
			index = OldPListData.IndexOf("CFBundleVersion");
			index = OldPListData.IndexOf("<string>", index) + 8;
			length = OldPListData.IndexOf("</string>", index) - index;
			string BundleVersion = OldPListData.Substring(index, length);

			// date we made this
			const string Iso8601DateTimeFormat = "yyyy-MM-ddTHH:mm:ssZ";
			string TimeStamp = DateTime.UtcNow.ToString(Iso8601DateTimeFormat);

			// create the archive plist
			StringBuilder Text = new StringBuilder();
			Text.AppendLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
			Text.AppendLine("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
			Text.AppendLine("<plist version=\"1.0\">");
			Text.AppendLine("<dict>");
			Text.AppendLine("\t<key>ApplicationProperties</key>");
			Text.AppendLine("\t<dict>");
			Text.AppendLine("\t\t<key>ApplicationPath</key>");
			Text.AppendLine("\t\t<string>Applications/" + AppName + "</string>");
			Text.AppendLine("\t\t<key>CFBundleIdentifier</key>");
			Text.AppendLine(string.Format("\t\t<string>{0}</string>", BundleIdentifier));
			Text.AppendLine("\t\t<key>CFBundleShortVersionString</key>");
			Text.AppendLine(string.Format("\t\t<string>{0}</string>", BundleShortVersion));
			Text.AppendLine("\t\t<key>CFBundleVersion</key>");
			Text.AppendLine(string.Format("\t\t<string>{0}</string>", BundleVersion));
			Text.AppendLine("\t\t<key>SigningIdentity</key>");
			Text.AppendLine(string.Format("\t\t<string>{0}</string>", Params.Certificate));
			Text.AppendLine("\t</dict>");
			Text.AppendLine("\t<key>ArchiveVersion</key>");
			Text.AppendLine("\t<integer>2</integer>");
			Text.AppendLine("\t<key>CreationDate</key>");
			Text.AppendLine(string.Format("\t<date>{0}</date>", TimeStamp));
			Text.AppendLine("\t<key>DefaultToolchainInfo</key>");
			Text.AppendLine("\t<dict>");
			Text.AppendLine("\t\t<key>DisplayName</key>");
			Text.AppendLine("\t\t<string>Xcode 7.3 Default</string>");
			Text.AppendLine("\t\t<key>Identifier</key>");
			Text.AppendLine("\t\t<string>com.apple.dt.toolchain.XcodeDefault</string>");
			Text.AppendLine("\t</dict>");
			Text.AppendLine("\t<key>Name</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", SC.ShortProjectName));
			Text.AppendLine("\t<key>SchemeName</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", SC.ShortProjectName));
			Text.AppendLine("</dict>");
			Text.AppendLine("</plist>");
			File.WriteAllText(Path.Combine(ArchiveName, "Info.plist"), Text.ToString());
        }
        else if (bXCArchive && !Utils.IsRunningOnMono)
		{
			LogWarning("Can not produce an XCArchive on windows");
		}
		SC.ArchiveFiles(Path.GetDirectoryName(ProjectIPA), Path.GetFileName(ProjectIPA));
	}

	public override bool RetrieveDeployedManifests(ProjectParams Params, DeploymentContext SC, string DeviceName, out List<string> UFSManifests, out List<string> NonUFSManifests)
	{
        if (Params.Devices.Count != 1)
        {
            throw new AutomationException("Can only retrieve deployed manifests from a single device, but {0} were specified", Params.Devices.Count);
        }

        bool Result = true;
		UFSManifests = new List<string>();
		NonUFSManifests = new List<string>();
		var DeployServer = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/DeploymentServer.exe");
		try
		{
			var TargetConfiguration = SC.StageTargetConfigurations[0];
			string BundleIdentifier = "";
			if (File.Exists(Params.BaseStageDirectory + "/" + PlatformName + "/Info.plist"))
			{
				string Contents = File.ReadAllText(SC.StageDirectory + "/Info.plist");
				int Pos = Contents.IndexOf("CFBundleIdentifier");
				Pos = Contents.IndexOf("<string>", Pos) + 8;
				int EndPos = Contents.IndexOf("</string>", Pos);
				BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
			}
			RunAndLog(CmdEnv, DeployServer, "Backup -file \"" + CombinePaths(Params.BaseStageDirectory, PlatformName, SC.UFSDeployedManifestFileName) + "\" -file \"" + CombinePaths(Params.BaseStageDirectory, PlatformName, SC.NonUFSDeployedManifestFileName) + "\"" + (String.IsNullOrEmpty(Params.DeviceNames[0]) ? "" : " -device " + Params.DeviceNames[0]) + " -bundle " + BundleIdentifier);

			string[] ManifestFiles = Directory.GetFiles(CombinePaths(Params.BaseStageDirectory, PlatformName), "*_Manifest_UFS*.txt");
			UFSManifests.AddRange(ManifestFiles);

			ManifestFiles = Directory.GetFiles(CombinePaths(Params.BaseStageDirectory, PlatformName), "*_Manifest_NonUFS*.txt");
			NonUFSManifests.AddRange(ManifestFiles);
		}
		catch (System.Exception)
		{
			// delete any files that did get copied
			string[] Manifests = Directory.GetFiles(CombinePaths(Params.BaseStageDirectory, PlatformName), "*_Manifest_*.txt");
			foreach (string Manifest in Manifests)
			{
				File.Delete(Manifest);
			}
			Result = false;
		}

		return Result;
	}

	public override void Deploy(ProjectParams Params, DeploymentContext SC)
    {
        if (Params.Devices.Count != 1)
        {
            throw new AutomationException("Can only deploy to a single specified device, but {0} were specified", Params.Devices.Count);
        }

        if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException ("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}
		if (Params.Distribution)
		{
			throw new AutomationException("iOS cannot deploy a package made for distribution.");
		}
		var TargetConfiguration = SC.StageTargetConfigurations[0];
		var ProjectIPA = MakeIPAFileName(TargetConfiguration, Params);
		var StagedIPA = SC.StageDirectory + "\\" + Path.GetFileName(ProjectIPA);

		// verify the .ipa exists
		if (!FileExists(StagedIPA))
		{
			StagedIPA = ProjectIPA;
			if(!FileExists(StagedIPA))
			{
				throw new AutomationException("DEPLOY FAILED - {0} was not found", ProjectIPA);
			}
		}

		// if iterative deploy, determine the file delta
		string BundleIdentifier = "";
		bool bNeedsIPA = true;
		if (Params.IterativeDeploy)
		{
			if (File.Exists(Params.BaseStageDirectory + "/" + PlatformName + "/Info.plist"))
			{
				string Contents = File.ReadAllText(SC.StageDirectory + "/Info.plist");
				int Pos = Contents.IndexOf("CFBundleIdentifier");
				Pos = Contents.IndexOf("<string>", Pos) + 8;
				int EndPos = Contents.IndexOf("</string>", Pos);
				BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
			}

			// check to determine if we need to update the IPA
			String NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath(Params.DeviceNames[0]);
			if (File.Exists(NonUFSManifestPath))
			{
				string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
				string[] Lines = NonUFSFiles.Split('\n');
				bNeedsIPA = Lines.Length > 0 && !string.IsNullOrWhiteSpace(Lines[0]);
			}
		}

		// Add a commandline for this deploy, if the config allows it.
		string AdditionalCommandline = (Params.FileServer || Params.CookOnTheFly) ? "" : (" -additionalcommandline " + "\"" + Params.RunCommandline + "\"");

		// deploy the .ipa
		var DeployServer = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/DeploymentServer.exe");

		// check for it in the stage directory
		string CurrentDir = Directory.GetCurrentDirectory();
		Directory.SetCurrentDirectory(CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/"));
		if (!Params.IterativeDeploy || bCreatedIPA || bNeedsIPA)
		{
			RunAndLog(CmdEnv, DeployServer, "Install -ipa \"" + Path.GetFullPath(StagedIPA) + "\"" + (String.IsNullOrEmpty(Params.DeviceNames[0]) ? "" : " -device " + Params.DeviceNames[0]) + AdditionalCommandline);
		}
		if (Params.IterativeDeploy)
		{
			// push over the changed files
			RunAndLog(CmdEnv, DeployServer, "Deploy -manifest \"" + CombinePaths(Params.BaseStageDirectory, PlatformName, DeploymentContext.UFSDeployDeltaFileName + (Params.Devices.Count == 0 ? "" : Params.DeviceNames[0])) + "\"" + (Params.Devices.Count == 0 ? "" : " -device " + Params.DeviceNames[0]) + AdditionalCommandline + " -bundle " + BundleIdentifier);
		}
		Directory.SetCurrentDirectory (CurrentDir);
        PrintRunTime();
    }

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return "IOS";
	}

	public override bool DeployLowerCaseFilenames()
	{
		// we shouldn't modify the case on files like Info.plist or the icons
		return true;
	}

	public override string LocalPathToTargetPath(string LocalPath, string LocalRoot)
	{
		return LocalPath.Replace("\\", "/").Replace(LocalRoot, "../../..");
	}

	public override bool IsSupported { get { return true; } }

	public override bool LaunchViaUFE { get { return UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac; } }

	public override bool UseAbsLog
	{
		get
		{
			return !LaunchViaUFE;
		}
	}

	public override StagedFileReference Remap(StagedFileReference Dest)
	{
		return new StagedFileReference("cookeddata/" + Dest.Name);
	}
	
    public override List<string> GetDebugFileExtentions()
    {
        return new List<string> { ".dsym", ".udebugsymbols" };
    }
	
// 	void MobileDeviceConnected(object sender, ConnectEventArgs args)
// 	{
// 	}
// 	
// 	void MobileDeviceDisconnected(object sender, ConnectEventArgs args)
// 	{
// 	}

	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
            if (Params.Devices.Count != 1)
            {
                throw new AutomationException("Can only run on a single specified device, but {0} were specified", Params.Devices.Count);
            }
			
			// This code only cares about connected devices so just call the run loop a few times to get the existing connected devices
// 			MobileDeviceInstanceManager.Initialize(MobileDeviceConnected, MobileDeviceDisconnected);
// 			for(int i = 0; i < 4; ++i)
// 			{
// 				System.Threading.Thread.Sleep(1);
// 				CoreFoundationRunLoop.RunLoopRunInMode(CoreFoundationRunLoop.kCFRunLoopDefaultMode(), 0.25, 0);
// 			}
// 			
            /*			string AppDirectory = string.Format("{0}/Payload/{1}.app",
				Path.GetDirectoryName(Params.ProjectGameExeFilename), 
				Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename));
			string GameName = Path.GetFileNameWithoutExtension (ClientApp);
			if (GameName.Contains ("-IOS-"))
			{
				GameName = GameName.Substring (0, GameName.IndexOf ("-IOS-"));
			}
			string GameApp = AppDirectory + "/" + GameName;
			bWasGenerated = false;
			XcodeProj = EnsureXcodeProjectExists (Params.RawProjectPath, CmdEnv.LocalRoot, Params.ShortProjectName, GetDirectoryName(Params.RawProjectPath), Params.IsCodeBasedProject, out bWasGenerated);
			string Arguments = "UBT_NO_POST_DEPLOY=true /usr/bin/xcrun xcodebuild test -project \"" + XcodeProj + "\"";
			Arguments += " -scheme '";
			Arguments += GameName;
			Arguments += " - iOS'";
			Arguments += " -configuration " + Params.ClientConfigsToBuild [0].ToString();
			Arguments += " -destination 'platform=iOS,id=" + Params.Device.Substring(Params.Device.IndexOf("@")+1) + "'";
			Arguments += " TEST_HOST=\"";
			Arguments += GameApp;
			Arguments += "\" BUNDLE_LOADER=\"";
			Arguments += GameApp + "\"";*/
            string BundleIdentifier = "";
			if (File.Exists(Params.BaseStageDirectory + "/"+ PlatformName + "/Info.plist"))
			{
				string Contents = File.ReadAllText(Params.BaseStageDirectory + "/" + PlatformName + "/Info.plist");
				int Pos = Contents.IndexOf("CFBundleIdentifier");
				Pos = Contents.IndexOf("<string>", Pos) + 8;
				int EndPos = Contents.IndexOf("</string>", Pos);
				BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
			}
			string Arguments = "/usr/bin/instruments";
			Arguments += " -w '" + Params.DeviceNames[0] + "'";
			Arguments += " -t 'Activity Monitor'";
			Arguments += " -D \"" + Params.BaseStageDirectory + "/" + PlatformName + "/launch.trace\"";
			Arguments += " '" + BundleIdentifier + "'";
			IProcessResult ClientProcess = Run ("/usr/bin/env", Arguments, null, ClientRunFlags | ERunOptions.NoWaitForExit);
			return new IOSClientProcess(ClientProcess, Params.DeviceNames[0]);
		}
		else
		{
			IProcessResult Result = new ProcessResult("DummyApp", null, false);
			Result.ExitCode = 0;
			return Result;
		}
	}

	public override void PostRunClient(IProcessResult Result, ProjectParams Params)
	{
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
			string LaunchTracePath = Params.BaseStageDirectory + "/" + PlatformName + "/launch.trace";
			Console.WriteLine ("Deleting " + LaunchTracePath);
			if (Directory.Exists(LaunchTracePath))
			{
				Directory.Delete (LaunchTracePath, true);
			}

			switch (Result.ExitCode)
			{
				case 253:
                    throw new AutomationException(ExitCode.Error_DeviceNotSetupForDevelopment, "Launch Failure");
				case 255:
                    throw new AutomationException(ExitCode.Error_DeviceOSNewerThanSDK, "Launch Failure");
			}
		}
	}

	private static int GetChunkCount(ProjectParams Params, DeploymentContext SC)
	{
		var ChunkListFilename = GetChunkPakManifestListFilename(Params, SC);
		var ChunkArray = ReadAllLines(ChunkListFilename);
		return ChunkArray.Length;
	}

	private static string GetChunkPakManifestListFilename(ProjectParams Params, DeploymentContext SC)
	{
		return CombinePaths(GetTmpPackagingPath(Params, SC), "pakchunklist.txt");
	}

	private static string GetTmpPackagingPath(ProjectParams Params, DeploymentContext SC)
	{
		return CombinePaths(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Saved", "TmpPackaging", SC.StageTargetPlatform.GetCookPlatform(SC.DedicatedServer, false));
	}

	private static StringBuilder AppendKeyValue(StringBuilder Text, string Key, object Value, int Level)
	{
		// create indent level
		string Indent = "";
		for (int i = 0; i < Level; ++i)
		{
			Indent += "\t";
		}

		// output key if we have one
		if (Key != null)
		{
			Text.AppendLine (Indent + "<key>" + Key + "</key>");
		}

		// output value
		if (Value is Array)
		{
			Text.AppendLine (Indent + "<array>");
			Array ValArray = Value as Array;
			foreach (var Item in ValArray)
			{
				AppendKeyValue (Text, null, Item, Level + 1);
			}
			Text.AppendLine (Indent + "</array>");
		}
		else if (Value is Dictionary<string, object>)
		{
			Text.AppendLine (Indent + "<dict>");
			Dictionary<string,object> ValDict = Value as Dictionary<string, object>;
			foreach (var Item in ValDict)
			{
				AppendKeyValue (Text, Item.Key, Item.Value, Level + 1);
			}
			Text.AppendLine (Indent + "</dict>");
		}
		else if (Value is string)
		{
			Text.AppendLine (Indent + "<string>" + Value + "</string>");
		}
		else if (Value is bool)
		{
			if ((bool)Value == true)
			{
				Text.AppendLine (Indent + "<true/>");
			}
			else
			{
				Text.AppendLine (Indent + "<false/>");
			}
		}
		else
		{
			Console.WriteLine ("PLIST: Unknown array item type");
		}
		return Text;
	}

	private static void GeneratePlist(Dictionary<string, object> KeyValues, string PlistFile)
	{
		// generate the plist file
		StringBuilder Text = new StringBuilder();

		// boiler plate top
		Text.AppendLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
		Text.AppendLine("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
		Text.AppendLine("<plist version=\"1.0\">");
		Text.AppendLine("<dict>");

		foreach (var KeyValue in KeyValues)
		{
			AppendKeyValue(Text, KeyValue.Key, KeyValue.Value, 1);
		}
		Text.AppendLine("</dict>");
		Text.AppendLine("</plist>");

		// write the file out
		if (!Directory.Exists(Path.GetDirectoryName(PlistFile)))
		{
			Directory.CreateDirectory(Path.GetDirectoryName(PlistFile));
		}
		File.WriteAllText(PlistFile, Text.ToString());
	}

	private static void GenerateAssetPlist(string BundleIdentifier, string[] Tags, string AssetDir)
	{
		Dictionary<string, object> KeyValues = new Dictionary<string, object> ();
		KeyValues.Add ("CFBundleIdentifier", BundleIdentifier);
		KeyValues.Add ("Tags", Tags);
		GeneratePlist(KeyValues, CombinePaths(AssetDir, "Info.plist"));
	}

	private static void GenerateAssetPackManifestPlist(KeyValuePair<string, string>[] ChunkData, string AssetDir)
	{
		Dictionary<string, object>[] Resources = new Dictionary<string, object>[ChunkData.Length];
		for (int i = 0; i < ChunkData.Length; ++i)
		{
			Dictionary<string, object> Data = new Dictionary<string, object> ();
			Data.Add ("URL", CombinePaths ("OnDemandResources", ChunkData[i].Value));
			Data.Add ("bundleKey", ChunkData [i].Key);
			Data.Add ("isStreamable", false);
			Resources [i] = Data;
		}

		Dictionary<string, object> KeyValues = new Dictionary<string, object> ();
		KeyValues.Add ("resources", Resources);
		GeneratePlist(KeyValues, CombinePaths(AssetDir, "AssetPackManifest.plist"));
	}

	private static void GenerateOnDemandResourcesPlist(KeyValuePair<string, string>[] ChunkData, string AssetDir)
	{
		Dictionary<string, object> RequestTags = new Dictionary<string, object> ();
		Dictionary<string, object> AssetPacks = new Dictionary<string, object> ();
		Dictionary<string, object> Requests = new Dictionary<string, object> ();
		for (int i = 0; i < ChunkData.Length; ++i)
		{
			string ChunkName = "Chunk" + (i + 1).ToString ();
			RequestTags.Add (ChunkName, new string[] { ChunkData [i].Key });
			AssetPacks.Add (ChunkData [i].Key, new string[] { ("pak" + ChunkName + "-ios.pak").ToLowerInvariant () });
			Dictionary<string, object> Packs = new Dictionary<string, object> ();
			Packs.Add ("NSAssetPacks", new string[] { ChunkData [i].Key });
			Requests.Add (ChunkName, Packs);
		}

		Dictionary<string, object> KeyValues = new Dictionary<string, object> ();
		KeyValues.Add ("NSBundleRequestTags", RequestTags);
		KeyValues.Add ("NSBundleResourceRequestAssetPacks", AssetPacks);
		KeyValues.Add ("NSBundleResourceRequestTags", Requests);
		GeneratePlist(KeyValues, CombinePaths(AssetDir, "OnDemandResources.plist"));
	}

	public override void PostStagingFileCopy(ProjectParams Params, DeploymentContext SC)
	{
/*		if (Params.CreateChunkInstall)
		{
			// get the bundle identifier
			string BundleIdentifier = "";
			if (File.Exists(Params.BaseStageDirectory + "/" + PlatformName + "/Info.plist"))
			{
				string Contents = File.ReadAllText(SC.StageDirectory + "/Info.plist");
				int Pos = Contents.IndexOf("CFBundleIdentifier");
				Pos = Contents.IndexOf("<string>", Pos) + 8;
				int EndPos = Contents.IndexOf("</string>", Pos);
				BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
			}

			// generate the ODR resources
			// create the ODR directory
			string DestSubdir = SC.StageDirectory + "/OnDemandResources";
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}

			// read the chunk list and generate the data
			var ChunkCount = GetChunkCount(Params, SC);
			var ChunkData = new KeyValuePair<string, string>[ChunkCount - 1];
			for (int i = 1; i < ChunkCount; ++i)
			{
				// chunk name
				string ChunkName = "Chunk" + i.ToString ();

				// asset name
				string AssetPack = BundleIdentifier + ".Chunk" + i.ToString () + ".assetpack";

				// bundle key
				byte[] bytes = new byte[ChunkName.Length * sizeof(char)];
				System.Buffer.BlockCopy(ChunkName.ToCharArray(), 0, bytes, 0, bytes.Length);
				string BundleKey = BundleIdentifier + ".asset-pack-" + BitConverter.ToString(System.Security.Cryptography.MD5.Create().ComputeHash(bytes)).Replace("-", string.Empty);

				// add to chunk data
				ChunkData[i-1] = new KeyValuePair<string, string>(BundleKey, AssetPack);

				// create the sub directory
				string AssetDir = CombinePaths (DestSubdir, AssetPack);
				if (!Directory.Exists(AssetDir))
				{
					Directory.CreateDirectory(AssetDir);
				}

				// generate the Info.plist for each ODR bundle (each chunk for install past 0)
				GenerateAssetPlist (BundleKey, new string[] { ChunkName }, AssetDir);

				// copy the files to the OnDemandResources directory
				string PakName = "pakchunk" + i.ToString ();
				string FileName =  PakName + "-" + PlatformName.ToLower() + ".pak";
				string P4Change = "UnknownCL";
				string P4Branch = "UnknownBranch";
				if (CommandUtils.P4Enabled)
				{
					P4Change = CommandUtils.P4Env.ChangelistString;
					P4Branch = CommandUtils.P4Env.BuildRootEscaped;
				}
				string ChunkInstallBasePath = CombinePaths(SC.ProjectRoot.FullName, "ChunkInstall", SC.FinalCookPlatform);
				string RawDataPath = CombinePaths(ChunkInstallBasePath, P4Branch + "-CL-" + P4Change, PakName);
				string RawDataPakPath = CombinePaths(RawDataPath, PakName + "-" + SC.FinalCookPlatform + ".pak");
				string DestFile = CombinePaths (AssetDir, FileName);
				CopyFile (RawDataPakPath, DestFile);
			}

			// generate the AssetPackManifest.plist
			GenerateAssetPackManifestPlist (ChunkData, SC.StageDirectory.FullName);

			// generate the OnDemandResources.plist
			GenerateOnDemandResourcesPlist (ChunkData, SC.StageDirectory.FullName);
		}*/
	}

	public override bool StageMovies
	{
		get { return false; }
	}

	public override bool RequiresPackageToDeploy
	{
		get { return true; }
	}

	public override HashSet<StagedFileReference> GetFilesForCRCCheck()
	{
		HashSet<StagedFileReference> FileList = base.GetFilesForCRCCheck();
		FileList.Add(new StagedFileReference("Info.plist"));
		return FileList;
	}
    public override bool SupportsMultiDeviceDeploy
    {
        get
        {
            return true;
        }
    }

	public override void StripSymbols(FileReference SourceFile, FileReference TargetFile)
	{
		IOSExports.StripSymbols(PlatformType, SourceFile, TargetFile);
	}

	#region Hooks

	public override void PreBuildAgenda(UE4Build Build, UE4Build.BuildAgenda Agenda)
	{
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac && !Automation.IsEngineInstalled())
		{
			Agenda.DotNetProjects.Add(@"Engine\Source\Programs\IOS\MobileDeviceInterface\MobileDeviceInterface.csproj");
			Agenda.DotNetProjects.Add(@"Engine\Source\Programs\IOS\iPhonePackager\iPhonePackager.csproj");
			Agenda.DotNetProjects.Add(@"Engine\Source\Programs\IOS\DeploymentServer\DeploymentServer.csproj");
		}
	}

	#endregion
}
