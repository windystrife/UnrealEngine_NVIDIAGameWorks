// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Xml;
using System.Diagnostics;
using System.Web.Script.Serialization;
using UnrealBuildTool;
using Tools.DotNETCommon;
using System.Linq;

namespace AutomationTool
{
	[Help("ForceMonolithic", "Toggle to combined the result into one executable")]
	[Help("ForceDebugInfo", "Forces debug info even in development builds")]
	[Help("NoXGE", "Toggle to disable the distributed build process")]
	[Help("ForceNonUnity", "Toggle to disable the unity build system")]
	[Help("ForceUnity", "Toggle to force enable the unity build system")]
	[Help("Licensee", "If set, this build is being compiled by a licensee")]
	public class UE4Build
	{
		private BuildCommand OwnerCommand;

		private String XGEConsoleExePath = "";

		public void PrepareBuildProduct(string File, bool DeleteProduct = true)
		{
			if (!BuildProductFiles.Contains(File))
			{
				if (DeleteProduct && DeleteBuildProducts)
				{
					CommandUtils.DeleteFile(File);
				}
			}
		}

		public bool HasBuildProduct(string InFile)
		{
			string File = CommandUtils.CombinePaths(InFile);
			foreach (var ExistingFile in BuildProductFiles)
			{
				if (ExistingFile.Equals(File, StringComparison.InvariantCultureIgnoreCase))
				{
					return true;
				}
			}
			return false;
		}

		public void AddBuildProduct(string InFile)
		{
			string File = CommandUtils.CombinePaths(InFile);
			if (!CommandUtils.FileExists(File) && !CommandUtils.DirectoryExists(File))
			{
				throw new AutomationException("BUILD FAILED specified file to AddBuildProduct {0} does not exist.", File);
			}
			if (!HasBuildProduct(InFile))
			{
				BuildProductFiles.Add(File);
			}
		}

		BuildManifest PrepareManifest(string ManifestName, bool bAddReceipt)
		{
			if (CommandUtils.FileExists(ManifestName) == false)
			{
				throw new AutomationException("BUILD FAILED UBT Manifest {0} does not exist.", ManifestName);
			}
			int FileNum = 0;
			string OutFile;
			while (true)
			{
				OutFile = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, String.Format("UBTManifest.{0}", FileNum) + Path.GetExtension(ManifestName));
				FileInfo ItemInfo = new FileInfo(OutFile);
				if (!ItemInfo.Exists)
				{
					break;
				}
				FileNum++;
			}
			CommandUtils.CopyFile(ManifestName, OutFile);
			CommandUtils.LogLog("Copied UBT manifest to {0}", OutFile);


			UnrealBuildTool.BuildManifest Manifest = CommandUtils.ReadManifest(ManifestName);
			foreach (string Item in Manifest.BuildProducts)
			{
				if(bAddReceipt && IsBuildReceipt(Item))
				{
					AddBuildProduct(Item);
				}
				else
				{
					PrepareBuildProduct(Item);
				}
			}
			foreach(string Item in Manifest.LibraryBuildProducts)
			{
				LibraryBuildProductFiles.Add(Item);
			}
			return Manifest;
		}

		static bool IsBuildReceipt(string FileName)
		{
			return FileName.EndsWith(".version", StringComparison.InvariantCultureIgnoreCase)
				|| FileName.EndsWith(".target", StringComparison.InvariantCultureIgnoreCase) 
				|| FileName.EndsWith(".modules", StringComparison.InvariantCultureIgnoreCase)
				|| FileName.EndsWith("buildid.txt", StringComparison.InvariantCultureIgnoreCase);
		}

		void AddBuildProductsFromManifest(string ManifestName)
		{
			if (CommandUtils.FileExists(ManifestName) == false)
			{
				throw new AutomationException("BUILD FAILED UBT Manifest {0} does not exist.", ManifestName);
			}
			UnrealBuildTool.BuildManifest Manifest = CommandUtils.ReadManifest(ManifestName);
			foreach (string Item in Manifest.BuildProducts)
			{
				if (!CommandUtils.FileExists_NoExceptions(Item) && !CommandUtils.DirectoryExists_NoExceptions(Item))
				{
					throw new AutomationException("BUILD FAILED {0} was in manifest but was not produced.", Item);
				}
				AddBuildProduct(Item);
			}
			foreach(string Item in Manifest.LibraryBuildProducts)
			{
				LibraryBuildProductFiles.Add(Item);
			}
		}

	
		/// True if UBT is compiled and ready to build!
		private bool bIsUBTReady = false;
		
		private void PrepareUBT()
		{			
			// Don't build UBT if we're running with pre-compiled binaries and if there's a debugger attached to this process.
			// With the debugger attached, even though deleting the exe will work, the pdb files are still locked and the build will fail.
			// Also, if we're running from VS then since UAT references UBT, we already have the most up-to-date version of UBT.exe
			if (!bIsUBTReady && GlobalCommandLine.Compile && !System.Diagnostics.Debugger.IsAttached)
			{
                string RebuildString = "";
                if (!GlobalCommandLine.IncrementalBuildUBT)
                {
                    CommandUtils.DeleteFile(UBTExecutable);
                    RebuildString = " /target:Rebuild";
                }

				CommandUtils.MsBuild(CommandUtils.CmdEnv,
						CommandUtils.CmdEnv.LocalRoot + @"/Engine/Source/Programs/UnrealBuildTool/" + HostPlatform.Current.UBTProjectName + @".csproj",
						"/verbosity:minimal /nologo" + RebuildString + " /property:Configuration=Development /property:Platform=AnyCPU",
						"BuildUBT");

				bIsUBTReady = true;
			}

			if (CommandUtils.FileExists(UBTExecutable) == false)
			{
				throw new AutomationException("UBT does not exist in {0}.", UBTExecutable);
			}
		}

		public class XGEItem
		{
			public UnrealBuildTool.BuildManifest Manifest;
			public string CommandLine;
			public UnrealBuildTool.UnrealTargetPlatform Platform;
			public string Config;
			public string TargetName;
			public FileReference UProjectPath;
			public List<string> XgeXmlFiles;
			public string OutputCaption;
		}

		XGEItem XGEPrepareBuildWithUBT(string TargetName, UnrealBuildTool.UnrealTargetPlatform Platform, string Config, FileReference UprojectPath, bool ForceMonolithic = false, bool ForceNonUnity = false, bool ForceDebugInfo = false, string InAddArgs = "", bool ForceUnity = false, Dictionary<string, string> EnvVars = null)
		{
			string AddArgs = "";
			if (UprojectPath != null)
			{
				AddArgs += " " + CommandUtils.MakePathSafeToUseWithCommandLine(UprojectPath.FullName);
			}
			AddArgs += " -NoUBTMakefiles";
			AddArgs += " " + InAddArgs;
			if (ForceMonolithic)
			{
				AddArgs += " -monolithic";
			}
			if (ForceNonUnity)
			{
				AddArgs += " -disableunity";
			}
			if (ForceUnity)
			{
				AddArgs += " -forceunity";
			}
			if (ForceDebugInfo)
			{
				AddArgs += " -forcedebuginfo";
			}

			PrepareUBT();

            string UBTManifest = GetUBTManifest(UprojectPath, AddArgs);

			CommandUtils.DeleteFile(UBTManifest);
			XGEItem Result = new XGEItem();

			ClearExportedXGEXML();

			using(TelemetryStopwatch GenerateManifestStopwatch = new TelemetryStopwatch("GenerateXGEManifest.{0}.{1}.{2}", TargetName, Platform.ToString(), Config))
			{
				CommandUtils.RunUBT(CommandUtils.CmdEnv, UBTExecutable: UBTExecutable, Project: UprojectPath, Target: TargetName, Platform: Platform.ToString(), Config: Config, AdditionalArgs: "-generatemanifest -nobuilduht -xgeexport" + AddArgs, EnvVars: EnvVars);
			}

			PrepareManifest(UBTManifest, true);

			Result.Platform = Platform;
			Result.Config = Config;
			Result.TargetName = TargetName;
			Result.UProjectPath = UprojectPath;
			Result.Manifest = CommandUtils.ReadManifest(UBTManifest);
			Result.OutputCaption = String.Format("{0}-{1}-{2}", TargetName, Platform.ToString(), Config.ToString());
			CommandUtils.DeleteFile(UBTManifest);

			Result.CommandLine = UBTExecutable + " " + CommandUtils.UBTCommandline(Project: UprojectPath, Target: TargetName, Platform: Platform.ToString(), Config: Config, AdditionalArgs: "-noxge -nobuilduht" + AddArgs);

			Result.XgeXmlFiles = new List<string>();
			foreach (var XGEFile in FindXGEFiles())
			{
				if (!CommandUtils.FileExists_NoExceptions(XGEFile))
				{
					throw new AutomationException("BUILD FAILED: Couldn't find file: {0}", XGEFile);
				}
				int FileNum = 0;
				string OutFile;
				while (true)
				{
					OutFile = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, String.Format("UBTExport.{0}.xge.xml", FileNum));
					FileInfo ItemInfo = new FileInfo(OutFile);
					if (!ItemInfo.Exists)
					{
						break;
					}
					FileNum++;
				}
				CommandUtils.CopyFile(XGEFile, OutFile);
				Result.XgeXmlFiles.Add(OutFile);
			}
			ClearExportedXGEXML();
			return Result;
		}

		void XGEFinishBuildWithUBT(XGEItem Item)
		{
			if(!Item.CommandLine.Contains("-nolink"))
			{
				// run the deployment steps, if necessary
				foreach(string DeployTargetFile in Item.Manifest.DeployTargetFiles)
				{
					CommandUtils.RunUBT(CommandUtils.CmdEnv, UBTExecutable, String.Format("-deploy=\"{0}\" -nomutex", DeployTargetFile));
				}
			}

			foreach (string ManifestItem in Item.Manifest.BuildProducts)
			{
				if (!CommandUtils.FileExists_NoExceptions(ManifestItem))
				{
					throw new AutomationException("BUILD FAILED {0} was in manifest but was not produced.", ManifestItem);
				}
				AddBuildProduct(ManifestItem);
			}
		}

		void XGEDeleteBuildProducts(UnrealBuildTool.BuildManifest Manifest)
		{
			foreach (string Item in Manifest.BuildProducts)
			{
				CommandUtils.DeleteFile(Item);
			}
		}

		void CleanWithUBT(string TargetName, UnrealBuildTool.UnrealTargetPlatform Platform, string Config, FileReference UprojectPath, bool ForceMonolithic = false, bool ForceNonUnity = false, bool ForceDebugInfo = false, string InAddArgs = "", bool ForceUnity = false, Dictionary<string, string> EnvVars = null)
		{
			string AddArgs = "";
			if (UprojectPath != null)
			{
				AddArgs += " " + CommandUtils.MakePathSafeToUseWithCommandLine(UprojectPath.FullName);
			}
			AddArgs += " -NoUBTMakefiles";
			AddArgs += " " + InAddArgs;
			if (ForceMonolithic)
			{
				AddArgs += " -monolithic";
			}
			if (ForceNonUnity)
			{
				AddArgs += " -disableunity";
			}
			if (ForceUnity)
			{
				AddArgs += " -forceunity";
			}
			if (ForceDebugInfo)
			{
				AddArgs += " -forcedebuginfo";
			}
			if (!TargetName.Equals("UnrealHeaderTool", StringComparison.InvariantCultureIgnoreCase))
			{
				AddArgs += " -nobuilduht";
			}

			PrepareUBT();
			using(TelemetryStopwatch CleanStopwatch = new TelemetryStopwatch("CleanWithUBT.{0}.{1}.{2}", TargetName, Platform.ToString(), Config))
			{
				CommandUtils.RunUBT(CommandUtils.CmdEnv, UBTExecutable: UBTExecutable, Project: UprojectPath, Target: TargetName, Platform: Platform.ToString(), Config: Config, AdditionalArgs: "-clean" + AddArgs, EnvVars: EnvVars);
			}
        }

		BuildManifest BuildWithUBT(string TargetName, UnrealBuildTool.UnrealTargetPlatform TargetPlatform, string Config, FileReference UprojectPath, bool ForceMonolithic = false, bool ForceNonUnity = false, bool ForceDebugInfo = false, bool ForceFlushMac = false, bool DisableXGE = false, string InAddArgs = "", bool ForceUnity = false, Dictionary<string, string> EnvVars = null)
		{
			string AddArgs = "";
			if (UprojectPath != null)
			{
				AddArgs += " " + CommandUtils.MakePathSafeToUseWithCommandLine(UprojectPath.FullName);
			}
			AddArgs += " -NoUBTMakefiles";
			AddArgs += " " + InAddArgs;
			if (ForceMonolithic)
			{
				AddArgs += " -monolithic";
			}
			if (ForceNonUnity)
			{
				AddArgs += " -disableunity";
			}
			if (ForceUnity)
			{
				AddArgs += " -forceunity";
			}
			if (ForceDebugInfo)
			{
				AddArgs += " -forcedebuginfo";
			}
			if (ForceFlushMac)
			{
				AddArgs += " -flushmac";
			}
			if (DisableXGE)
			{
				AddArgs += " -noxge";
			}

			PrepareUBT();

			// let the platform determine when to use the manifest
            bool UseManifest = Platform.GetPlatform(TargetPlatform).ShouldUseManifestForUBTBuilds(AddArgs);

			BuildManifest Manifest = null;
			if (UseManifest)
			{
                string UBTManifest = GetUBTManifest(UprojectPath, AddArgs);

				CommandUtils.DeleteFile(UBTManifest);
				using(TelemetryStopwatch PrepareManifestStopwatch = new TelemetryStopwatch("PrepareUBTManifest.{0}.{1}.{2}", TargetName, TargetPlatform.ToString(), Config))
				{
					CommandUtils.RunUBT(CommandUtils.CmdEnv, UBTExecutable: UBTExecutable, Project: UprojectPath, Target: TargetName, Platform: TargetPlatform.ToString(), Config: Config, AdditionalArgs: AddArgs + " -generatemanifest", EnvVars: EnvVars);
				}
                Manifest = PrepareManifest(UBTManifest, false);
			}

			using(TelemetryStopwatch CompileStopwatch = new TelemetryStopwatch("Compile.{0}.{1}.{2}", TargetName, TargetPlatform.ToString(), Config))
			{
				CommandUtils.RunUBT(CommandUtils.CmdEnv, UBTExecutable: UBTExecutable, Project: UprojectPath, Target: TargetName, Platform: TargetPlatform.ToString(), Config: Config, AdditionalArgs: AddArgs, EnvVars: EnvVars);
			}

			if (UseManifest)
			{
                string UBTManifest = GetUBTManifest(UprojectPath, AddArgs);

				AddBuildProductsFromManifest(UBTManifest);

				CommandUtils.DeleteFile(UBTManifest);
			}
			return Manifest;
		}

		string[] DotNetProductExtenstions()
		{
			return new string[] 
			{
				".dll",
				".pdb",
				".exe.config",
				".exe",
				"exe.mdb"
			};
		}

		string[] SwarmBuildProducts()
		{
			return new string[]
            {
                "AgentInterface",
                "SwarmAgent",
                "SwarmCoordinator",
                "SwarmCoordinatorInterface",
                "SwarmInterface",
				"SwarmCommonUtils"
            };
		}

		void PrepareBuildProductsForCSharpProj(string CsProj)
		{
			string BaseOutput = CommandUtils.CmdEnv.LocalRoot + @"/Engine/Binaries/DotNET/" + Path.GetFileNameWithoutExtension(CsProj);
			foreach (var Ext in DotNetProductExtenstions())
			{
				PrepareBuildProduct(BaseOutput + Ext, false); // we don't delete these because we are not sure they are actually a build product
			}
		}

		void AddBuildProductsForCSharpProj(string CsProj)
		{
			string BaseOutput = CommandUtils.CmdEnv.LocalRoot + @"/Engine/Binaries/DotNET/" + Path.GetFileNameWithoutExtension(CsProj);
			foreach (var Ext in DotNetProductExtenstions())
			{
				if (CommandUtils.FileExists(BaseOutput + Ext))
				{
					AddBuildProduct(BaseOutput + Ext);
				}
			}
		}

		void AddIOSBuildProductsForCSharpProj(string CsProj)
		{
			string BaseOutput = CommandUtils.CmdEnv.LocalRoot + @"/Engine/Binaries/DotNET/IOS/" + Path.GetFileNameWithoutExtension(CsProj);
			foreach (var Ext in DotNetProductExtenstions())
			{
				if (CommandUtils.FileExists(BaseOutput + Ext))
				{
					AddBuildProduct(BaseOutput + Ext);
				}
			}
		}

		void AddSwarmBuildProducts()
		{
			foreach (var SwarmProduct in SwarmBuildProducts())
			{
				string DotNETOutput = CommandUtils.CmdEnv.LocalRoot + @"/Engine/Binaries/DotNET/" + SwarmProduct;
				string Win64Output = CommandUtils.CmdEnv.LocalRoot + @"/Engine/Binaries/Win64/" + SwarmProduct;
				foreach (var Ext in DotNetProductExtenstions())
				{
					if (CommandUtils.FileExists(DotNETOutput + Ext))
					{
						AddBuildProduct(DotNETOutput + Ext);
					}
				}
				foreach (var Ext in DotNetProductExtenstions())
				{
					if (CommandUtils.FileExists(Win64Output + Ext))
					{
						AddBuildProduct(Win64Output + Ext);
					}
				}
			}
		}

		/// <summary>
		/// Updates the engine version files
		/// </summary>
		public List<FileReference> UpdateVersionFiles(bool ActuallyUpdateVersionFiles = true, int? ChangelistNumberOverride = null, int? CompatibleChangelistNumberOverride = null, string Build = null, bool? IsPromotedOverride = null, bool bSkipHeader = false)
		{
			bool bIsLicenseeVersion = ParseParam("Licensee");
			bool bIsPromotedBuild = IsPromotedOverride.HasValue? IsPromotedOverride.Value : (ParseParamInt("Promoted", 1) != 0);
			bool bDoUpdateVersionFiles = CommandUtils.P4Enabled && ActuallyUpdateVersionFiles;		
			int ChangelistNumber = 0;
			if (bDoUpdateVersionFiles)
			{
				ChangelistNumber = ChangelistNumberOverride.HasValue? ChangelistNumberOverride.Value : CommandUtils.P4Env.Changelist;
			}
			int CompatibleChangelistNumber = ChangelistNumber;
			if(bDoUpdateVersionFiles && CompatibleChangelistNumberOverride.HasValue)
			{
				CompatibleChangelistNumber = CompatibleChangelistNumberOverride.Value;
			}

			string Branch = OwnerCommand.ParseParamValue("Branch");
			if (String.IsNullOrEmpty(Branch))
			{
				Branch = CommandUtils.P4Enabled ? CommandUtils.EscapePath(CommandUtils.P4Env.Branch) : "";
			}

			return StaticUpdateVersionFiles(ChangelistNumber, CompatibleChangelistNumber, Branch, Build, bIsLicenseeVersion, bIsPromotedBuild, bDoUpdateVersionFiles, bSkipHeader);
		}

		public static List<FileReference> StaticUpdateVersionFiles(int ChangelistNumber, int CompatibleChangelistNumber, string Branch, string Build, bool bIsLicenseeVersion, bool bIsPromotedBuild, bool bDoUpdateVersionFiles, bool bSkipHeader)
		{
			string ChangelistString = (ChangelistNumber != 0 && bDoUpdateVersionFiles)? ChangelistNumber.ToString() : String.Empty;

			var Result = new List<FileReference>();
			{
				FileReference VerFile = BuildVersion.GetDefaultFileName();
				if (bDoUpdateVersionFiles)
				{
					CommandUtils.LogLog("Updating {0} with:", VerFile);
					CommandUtils.LogLog("  Changelist={0}", ChangelistNumber);
					CommandUtils.LogLog("  CompatibleChangelist={0}", CompatibleChangelistNumber);
					CommandUtils.LogLog("  IsLicenseeVersion={0}", bIsLicenseeVersion? 1 : 0);
					CommandUtils.LogLog("  IsPromotedBuild={0}", bIsPromotedBuild? 1 : 0);
					CommandUtils.LogLog("  BranchName={0}", Branch);

					BuildVersion Version;
					if(!BuildVersion.TryRead(VerFile, out Version))
					{
						Version = new BuildVersion();
					}

					Version.Changelist = ChangelistNumber;
					if(CompatibleChangelistNumber > 0)
					{
						Version.CompatibleChangelist = CompatibleChangelistNumber;
					}
					Version.IsLicenseeVersion = bIsLicenseeVersion? 1 : 0;
					Version.IsPromotedBuild = bIsPromotedBuild? 1 : 0;
					Version.BranchName = Branch;

					VersionFileUpdater.MakeFileWriteable(VerFile.FullName);

					Version.Write(VerFile);
				}
				else
				{
					CommandUtils.LogVerbose("{0} will not be updated because P4 is not enabled.", VerFile);
				}
				Result.Add(VerFile);
			}

            FileReference EngineVersionFile = FileReference.Combine(CommandUtils.EngineDirectory, "Source", "Runtime", "Launch", "Resources", "Version.h");
            {
                FileReference VerFile = EngineVersionFile;
				if (bDoUpdateVersionFiles && !bSkipHeader)
				{
					CommandUtils.LogLog("Updating {0} with:", VerFile);
					CommandUtils.LogLog(" #define	BRANCH_NAME  {0}", Branch);
					CommandUtils.LogLog(" #define	BUILT_FROM_CHANGELIST  {0}", ChangelistString);
					if(CompatibleChangelistNumber > 0)
					{
						CommandUtils.LogLog(" #define	ENGINE_COMPATIBLE_CL_VERSION  {0}", CompatibleChangelistNumber);
					}
					if (Build != null)
					{
						CommandUtils.LogLog(" #define	BUILD_VERSION  {0}", Build);
					}
					CommandUtils.LogLog(" #define   ENGINE_IS_LICENSEE_VERSION  {0}", bIsLicenseeVersion ? "1" : "0");
					CommandUtils.LogLog(" #define   ENGINE_IS_PROMOTED_BUILD  {0}", bIsPromotedBuild? "1" : "0");

					VersionFileUpdater VersionH = new VersionFileUpdater(VerFile);
					VersionH.ReplaceLine("#define BRANCH_NAME ", "\"" + Branch + "\"");
					VersionH.ReplaceLine("#define BUILT_FROM_CHANGELIST ", ChangelistString);
					if (Build != null)
					{
						VersionH.ReplaceLine("#define BUILD_VERSION ", "L\"" + Build + "\"");
					}
					if(CompatibleChangelistNumber > 0)
					{
						VersionH.ReplaceLine("#define ENGINE_COMPATIBLE_CL_VERSION", CompatibleChangelistNumber.ToString(), bIsLicenseeVersion? 0 : 1);
					}
					VersionH.ReplaceLine("#define ENGINE_IS_LICENSEE_VERSION ", bIsLicenseeVersion ? "1" : "0");
					VersionH.ReplaceLine("#define ENGINE_IS_PROMOTED_BUILD ", bIsPromotedBuild? "1" : "0");

                    VersionH.Commit();
                }
				else
				{
					CommandUtils.LogVerbose("{0} will not be updated because P4 is not enabled.", VerFile);
				}
                Result.Add(VerFile);
            }

            {
                // Use Version.h data to update MetaData.cs so the assemblies match the engine version.
                FileReference VerFile = FileReference.Combine(CommandUtils.EngineDirectory, "Source", "Programs", "DotNETCommon", "MetaData.cs");

				if (bDoUpdateVersionFiles)
                {
                    // Get the MAJOR/MINOR/PATCH from the Engine Version file, as it is authoritative. The rest we get from the P4Env.
                    string NewInformationalVersion = FEngineVersionSupport.FromVersionFile(EngineVersionFile.FullName, ChangelistNumber).ToString();

                    CommandUtils.LogLog("Updating {0} with AssemblyInformationalVersion: {1}", VerFile, NewInformationalVersion);

                    VersionFileUpdater VersionH = new VersionFileUpdater(VerFile);
                    VersionH.SetAssemblyInformationalVersion(NewInformationalVersion);
                    VersionH.Commit();
                }
                else
                {
                    CommandUtils.LogVerbose("{0} will not be updated because P4 is not enabled.", VerFile);
                }
                Result.Add(VerFile);
            }

			return Result;
		}

		/// <summary>
		/// Updates the public key file (decryption)
		/// </summary>
		public void UpdatePublicKey(string KeyFilename)
		{
			string Modulus = "\"0x0\"";
			string PublicKeyExponent = "\"0x0\"";

			if (!string.IsNullOrEmpty(KeyFilename))
			{
				var Lines = ReadEncryptionKeys(KeyFilename);
				if (Lines != null && Lines.Length >= 3)
				{
					Modulus = String.Format("\"{0}\"", Lines[1]);
					PublicKeyExponent = String.Format("\"{0}\"", Lines[2]);
				}
				else
				{
					CommandUtils.LogError("{0} doesn't look like a valid encryption key file or value.", KeyFilename);
				}
			}

			FileReference VerFile = FileReference.Combine(CommandUtils.EngineDirectory, "Source", "Runtime", "PakFile", "Private", "PublicKey.inl");

			CommandUtils.LogVerbose("Updating {0} with:", VerFile);
			CommandUtils.LogVerbose(" #define DECRYPTION_KEY_EXPONENT {0}", PublicKeyExponent);
			CommandUtils.LogVerbose(" #define DECRYPTION_KEY_MODULUS {0}", Modulus);

			VersionFileUpdater PublicKeyInl = new VersionFileUpdater(VerFile);
			PublicKeyInl.ReplaceLine("#define DECRYPTION_KEY_EXPONENT ", PublicKeyExponent);
			PublicKeyInl.ReplaceLine("#define DECRYPTION_KEY_MODULUS ", Modulus);
			PublicKeyInl.Commit();
		}

		/// <summary>
		/// Parses the encryption keys from the command line or loads them from a file.
		/// </summary>
		/// <param name="KeyFilenameOrValues">Key values or filename</param>
		/// <returns>List of three encruption key values: private exponent, modulus and public exponent.</returns>
		private string[] ReadEncryptionKeys(string KeyFilenameOrValues)
		{
			string[] Values;
			if (KeyFilenameOrValues.StartsWith("0x", StringComparison.InvariantCultureIgnoreCase))
			{
				Values = KeyFilenameOrValues.Split(new char[] { '+' }, StringSplitOptions.RemoveEmptyEntries);
			}
			else
			{
				Values = CommandUtils.ReadAllLines(KeyFilenameOrValues);
			}
			return Values;
		}

		[DebuggerDisplay("{TargetName} {Platform} {Config}")]
		public class BuildTarget
		{
			/// Name of the target
			public string TargetName;

			/// For code-based projects with a .uproject, the TargetName isn't enough for UBT to find the target, this will point UBT to the target
			public FileReference UprojectPath;

			/// Platform to build
			public UnrealBuildTool.UnrealTargetPlatform Platform;

			/// Configuration to build
			public UnrealBuildTool.UnrealTargetConfiguration Config;

			/// Extra UBT args
			public string UBTArgs;
		}


		public class BuildAgenda
		{
			/// Full .NET solution files that we will compile and include in the build.  Currently we assume the output
			/// binary file names match the solution file base name, but with various different binary file extensions.
			public List<string> DotNetSolutions = new List<string>();

			/// .NET .csproj files that will be compiled and included in the build.  Currently we assume the output
			/// binary file names match the solution file base name, but with various different binary file extensions.
			public List<string> DotNetProjects = new List<string>();

			/// These are .NET project binary base file names that we want to include and label with the build, but
			/// we won't be compiling these projects directly ourselves (however, they may be incidentally build or
			/// published by a different project that we are building.)  We'll look for various .NET binary files with
			/// this base file name but with different extensions.
			public List<string> ExtraDotNetFiles = new List<string>();

			/// These are .NET project binary files that are specific to IOS and found in the IOS subdirectory.  We define
			/// these buildproducts as existing in the DotNET\IOS directory and assume that the output binary file names match
			/// the solution file base name, but with various binary file extensions
			public List<string> IOSDotNetProjects = new List<string>();

			/// These are .NET project binary files that are specific to HTML5.  We define these build products as existing in the 
			/// DotNET directory and assume that the output binary file names match
			/// the solution file base name, but with various binary file extensions
			public List<string> HTML5DotNetProjects = new List<string>();

			public string SwarmProject = "";


			/// List of targets to build.  These can be various Unreal projects, programs or libraries in various configurations
			public List<BuildTarget> Targets = new List<BuildTarget>();

			/// Used for special jobs to test things. At the moment, debugging some megaXGE thing
			public bool SpecialTestFlag = false;

			public bool DoRetries = false;

			/// <summary>
			/// Adds a target with the specified configuration.
			/// </summary>
			/// <param name="TargetName">Name of the target</param>
			/// <param name="InPlatform">Platform</param>
			/// <param name="InConfiguration">Configuration</param>
			/// <param name="InUprojectPath">Path to optional uproject file</param>
			/// <param name="InAddArgs">Specifies additional arguments for UBT</param>
			public void AddTarget(string TargetName, UnrealBuildTool.UnrealTargetPlatform InPlatform, UnrealBuildTool.UnrealTargetConfiguration InConfiguration, FileReference InUprojectPath = null, string InAddArgs = "")
			{
				// Is this platform a compilable target?
				if (!Platform.GetPlatform(InPlatform).CanBeCompiled())
				{
					return;
				}

				Targets.Add(new BuildTarget()
				{
					TargetName = TargetName,
					Platform = InPlatform,
					Config = InConfiguration,
					UprojectPath = InUprojectPath,
					UBTArgs = InAddArgs,
				});
			}

			/// <summary>
			/// Adds multiple targets with the specified configuration.
			/// </summary>
			/// <param name="TargetNames">List of targets.</param>
			/// <param name="InPlatform">Platform</param>
			/// <param name="InConfiguration">Configuration</param>
			/// <param name="InUprojectPath">Path to optional uproject file</param>
			/// <param name="InAddArgs">Specifies additional arguments for UBT</param>
			public void AddTargets(string[] TargetNames, UnrealBuildTool.UnrealTargetPlatform InPlatform, UnrealBuildTool.UnrealTargetConfiguration InConfiguration, FileReference InUprojectPath = null, string InAddArgs = "")
			{
				// Is this platform a compilable target?
				if (!Platform.GetPlatform(InPlatform).CanBeCompiled())
				{
					return;
				}

				foreach (var Target in TargetNames)
				{
					Targets.Add(new BuildTarget()
					{
						TargetName = Target,
						Platform = InPlatform,
						Config = InConfiguration,
						UprojectPath = InUprojectPath,
						UBTArgs = InAddArgs,
					});
				}
			}
		}


		public UE4Build(BuildCommand Command)
		{
			OwnerCommand = Command;
			BuildProductFiles.Clear();
			LibraryBuildProductFiles.Clear();
		}

		public List<string> FindXGEFiles()
		{
			var Result = new List<string>();
			var Root = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"\Engine\Intermediate\Build");			
			Result.AddRange(CommandUtils.FindFiles_NoExceptions("*.xge.xml", false, Root));
			Result.Sort();
			return Result;
		}

		public string XGEConsoleExe()
		{
			if (string.IsNullOrEmpty(XGEConsoleExePath))
			{
				string[] PathDirs = Environment.GetEnvironmentVariable("PATH").Split(Path.PathSeparator);
				foreach (string PathDir in PathDirs)
				{
					try
					{
						string FullPath = Path.Combine(PathDir, "xgConsole" + Platform.GetExeExtension(UnrealBuildTool.BuildHostPlatform.Current.Platform));
						if (CommandUtils.FileExists(FullPath))
						{
							XGEConsoleExePath = FullPath;
							break;
						}
					}
					catch
					{
					}
				}
			}
			return XGEConsoleExePath;
		}

		public bool ProcessXGEItems(List<XGEItem> Actions, string XGETool, string Args, string TaskFilePath, bool DoRetries, bool SpecialTestFlag, bool ShowProgress)
		{
			foreach(XGEItem Item in Actions)
			{
				if(Item.Manifest.PreBuildScripts != null)
				{
					Utils.ExecuteCustomBuildSteps(Item.Manifest.PreBuildScripts.Select(x => new FileReference(x)).ToArray());
				}
			}

			TelemetryStopwatch CombineXGEStopwatch = new TelemetryStopwatch("CombineXGEItemFiles.{0}", Path.GetFileNameWithoutExtension(XGETool));

			XmlDocument XGETaskDocument;	
			if (!CombineXGEItemFiles(Actions, TaskFilePath, out XGETaskDocument))
			{
				if (DeleteBuildProducts)
				{
					throw new AutomationException("Unable to find xge xmls: " + CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, "*.xge.xml"));
				}
				else
				{
					CommandUtils.LogVerbose("Incremental build, apparently everything was up to date, no XGE jobs produced.");
				}
			}
			else
			{
				XmlElement EnvironmentsElement = XGETaskDocument.CreateElement("Environments");// Make sure all the tasks have a unique prefix
				if(ShowProgress)
				{
					List<XmlElement> AllToolElements = new List<XmlElement>();
					foreach(XmlElement EnvironmentElement in EnvironmentsElement.GetElementsByTagName("Environment"))
					{
						foreach(XmlElement ToolsElement in EnvironmentElement.GetElementsByTagName("Tools"))
						{
							foreach(XmlElement ToolElement in ToolsElement.GetElementsByTagName("Tool"))
							{
								AllToolElements.Add(ToolElement);
							}
						}
					}
					for(int Idx = 0; Idx < AllToolElements.Count; Idx++)
					{
						XmlElement ToolElement = AllToolElements[Idx];
						if (ToolElement.HasAttribute("OutputPrefix"))
						{
							ToolElement.SetAttribute("OutputPrefix", ToolElement.Attributes["OutputPrefix"].Value + String.Format(" [@progress increment 1/{0}]", AllToolElements.Count));
						}
						else
						{
							ToolElement.SetAttribute("OutputPrefix", String.Format(" [@progress increment 1/{0} skipline]", AllToolElements.Count));
						}
					}
				}

				// Write the XGE task XML to a temporary file.
				using (FileStream OutputFileStream = new FileStream(TaskFilePath, FileMode.Create, FileAccess.Write))
				{
					XGETaskDocument.Save(OutputFileStream);
				}
				if (!CommandUtils.FileExists(TaskFilePath))
				{
					throw new AutomationException("Unable to find xge xml: " + TaskFilePath);
				}

				CombineXGEStopwatch.Finish();

				if(XGETool == null)
				{
					CommandUtils.PushDir(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"\Engine\Source"));
					try
					{
						int ExitCode = ParallelExecutor.Execute(TaskFilePath, OwnerCommand.ParseParam("StopOnErrors"));
						if(ExitCode != 0)
						{
							return false;
						}
					}
					finally
					{
						CommandUtils.PopDir();
					}
				}
				else
				{
					using (TelemetryStopwatch ProcessXGEStopwatch = new TelemetryStopwatch("ProcessXGE.{0}", Path.GetFileNameWithoutExtension(XGETool)))
					{
						int Retries = DoRetries ? 2 : 1;
						int ConnectionRetries = 4;
						for (int i = 0; i < Retries; i++)
						{
							try
							{
								while (true)
								{
									CommandUtils.LogVerbose("Running {0} *******", XGETool);
									CommandUtils.PushDir(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"\Engine\Source"));
									int SuccesCode;
									string LogFile = CommandUtils.GetRunAndLogOnlyName(CommandUtils.CmdEnv, XGETool);
									string Output = CommandUtils.RunAndLog(XGETool, Args, out SuccesCode, LogFile);
									bool bOutputContainsProject = Output.Contains("------Project:");
									CommandUtils.PopDir();
									if (ConnectionRetries > 0 && (SuccesCode == 4 || SuccesCode == 2) && !bOutputContainsProject && XGETool != null)
									{
										CommandUtils.LogWarning(String.Format("{0} failure on the local connection timeout", XGETool));
										if (ConnectionRetries < 2)
										{
											System.Threading.Thread.Sleep(60000);
										}
										ConnectionRetries--;
										continue;
									}
									else if (SuccesCode != 0)
									{
										throw new AutomationException("Command failed (Result:{3}): {0} {1}. See logfile for details: '{2}' ",
																		XGETool, Args, Path.GetFileName(LogFile), SuccesCode);
									}
									CommandUtils.LogVerbose("{0} {1} Done *******", XGETool, Args);
									break;
								}
								break;
							}
							catch (Exception Ex)
							{
								if (i + 1 >= Retries)
								{
									throw;
								}
								CommandUtils.LogWarning("{0} failed on try {1}, deleting products to force a rebuild: {2}", XGETool, i + 1, Ex.ToString());
								foreach (var Item in Actions)
								{
									XGEDeleteBuildProducts(Item.Manifest);
								}
							}
						}
					}
				}
			}
			foreach(XGEItem Item in Actions)
			{
				if(Item.Manifest.PostBuildScripts != null)
				{
					Utils.ExecuteCustomBuildSteps(Item.Manifest.PostBuildScripts.Select(x => new FileReference(x)).ToArray());
				}
			}
			foreach (var Item in Actions)
			{
				XGEFinishBuildWithUBT(Item);
			}
			return true;
		}

        private static bool CombineXGEItemFiles(List<XGEItem> Actions, string TaskFilePath, out XmlDocument XGETaskDocument)
        {
            XGETaskDocument = new XmlDocument();

			// <BuildSet FormatVersion="1">...</BuildSet>
			XmlElement BuildSetElement = XGETaskDocument.CreateElement("BuildSet");
			XGETaskDocument.AppendChild(BuildSetElement);
			BuildSetElement.SetAttribute("FormatVersion", "1");

			// <Environments>...</Environments>
			XmlElement EnvironmentsElement = XGETaskDocument.CreateElement("Environments");
			BuildSetElement.AppendChild(EnvironmentsElement);

			int Job = 0;
			int Env = 0;
			Dictionary<string, XmlElement> EnvStringToEnv = new Dictionary<string, XmlElement>();
			Dictionary<string, XmlElement> EnvStringToProject = new Dictionary<string, XmlElement>();
			Dictionary<string, string> ParamsToTool = new Dictionary<string, string>();
			Dictionary<string, XmlElement> ParamsToToolElement = new Dictionary<string, XmlElement>();
			Dictionary<string, string> ToolToAction = new Dictionary<string, string>();
			foreach (var Item in Actions)
			{
				var CurrentDependencies = new List<string>();
				foreach (var XGEFile in Item.XgeXmlFiles)
				{
					if (!CommandUtils.FileExists_NoExceptions(XGEFile))
					{
						throw new AutomationException("BUILD FAILED: Couldn't find file: {0}", XGEFile);
					}
					var TargetFile = TaskFilePath + "." + Path.GetFileName(XGEFile);
					CommandUtils.CopyFile(XGEFile, TargetFile);
					CommandUtils.CopyFile_NoExceptions(XGEFile, TaskFilePath);

					XmlReaderSettings XmlSettings = new XmlReaderSettings();
					XmlSettings.DtdProcessing = DtdProcessing.Ignore;
					XmlSettings.XmlResolver = null;

					XmlDocument UBTTask = new XmlDocument();
                    using (XmlReader Reader = XmlReader.Create(XGEFile, XmlSettings))
					{
						UBTTask.Load(Reader);
					}

					CommandUtils.DeleteFile(XGEFile);

					var All = new List<string>();
					{
						var Elements = UBTTask.GetElementsByTagName("Variable");
						foreach (XmlElement Element in Elements)
						{
							string Pair = Element.Attributes["Name"].Value + "=" + Element.Attributes["Value"].Value;
							All.Add(Pair);
						}
					}
					All.Sort();
					string AllString = "";
					foreach (string Element in All)
					{
						AllString += Element + "\n";
					}
					XmlElement ToolsElement;
					XmlElement ProjectElement;

					if (EnvStringToEnv.ContainsKey(AllString))
					{
						ToolsElement = EnvStringToEnv[AllString];
						ProjectElement = EnvStringToProject[AllString];
					}
					else
					{
						string EnvName = string.Format("Env_{0}", Env);
						Env++;
						// <Environment Name="Win32">...</Environment>
						XmlElement EnvironmentElement = XGETaskDocument.CreateElement("Environment");
						EnvironmentsElement.AppendChild(EnvironmentElement);
						EnvironmentElement.SetAttribute("Name", EnvName);

						// <Tools>...</Tools>
						ToolsElement = XGETaskDocument.CreateElement("Tools");
						EnvironmentElement.AppendChild(ToolsElement);

						{
							// <Variables>...</Variables>
							XmlElement VariablesElement = XGETaskDocument.CreateElement("Variables");
							EnvironmentElement.AppendChild(VariablesElement);

							var Elements = UBTTask.GetElementsByTagName("Variable");
							foreach (XmlElement Element in Elements)
							{
								// <Variable>...</Variable>
								XmlElement VariableElement = XGETaskDocument.CreateElement("Variable");
								VariablesElement.AppendChild(VariableElement);
								VariableElement.SetAttribute("Name", Element.Attributes["Name"].Value);
								VariableElement.SetAttribute("Value", Element.Attributes["Value"].Value);
							}
						}

						// <Project Name="Default" Env="Default">...</Project>
						ProjectElement = XGETaskDocument.CreateElement("Project");
						BuildSetElement.AppendChild(ProjectElement);
						ProjectElement.SetAttribute("Name", EnvName);
						ProjectElement.SetAttribute("Env", EnvName);

						EnvStringToEnv.Add(AllString, ToolsElement);
						EnvStringToProject.Add(AllString, ProjectElement);

					}

					Dictionary<string, string> ToolToTool = new Dictionary<string, string>();
					Dictionary<string, string> ActionToAction = new Dictionary<string, string>();

					{
						var Elements = UBTTask.GetElementsByTagName("Tool");
						foreach (XmlElement Element in Elements)
						{
							string Key = Element.Attributes["Path"].Value;
							Key += " ";
							Key += Element.Attributes["Params"].Value;

							//hack hack hack
							string ElementParams = Element.Attributes["Params"].Value;
							if (!String.IsNullOrEmpty(ElementParams))
							{
								int YcIndex = ElementParams.IndexOf(" /Yc\"");
								if (YcIndex >= 0)
								{
									// /Fp&quot;D:\BuildFarm\buildmachine_++depot+UE4\Engine\Intermediate\BuildData\Win64\UE4Editor\Development\SharedPCHs\CoreUObject.h.pch&quot
									string Fp = " /Fp\"";
									int FpIndex = ElementParams.IndexOf(Fp, YcIndex);
									if (FpIndex >= 0)
									{
										int EndIndex = ElementParams.IndexOf("\"", FpIndex + Fp.Length);
										if (EndIndex >= 0)
										{
											string PCHFileName = ElementParams.Substring(FpIndex + Fp.Length, EndIndex - FpIndex - Fp.Length);
											if (PCHFileName.Contains(@"\SharedPCHs\") && PCHFileName.Contains(@"\UE4Editor\"))
											{
												Key = "SharedEditorPCH$ " + PCHFileName;
												CommandUtils.LogLog("Hack: detected Shared PCH, which will use a different key {0}", Key);
											}
										}
									}
								}

							}

							string ToolName = string.Format("{0}_{1}", Element.Attributes["Name"].Value, Job);
							string OriginalToolName = ToolName;

							if (ParamsToTool.ContainsKey(Key))
							{
								ToolName = ParamsToTool[Key];
								ToolToTool.Add(OriginalToolName, ToolName);

								XmlElement ToolElement = ParamsToToolElement[Key];
								ToolElement.SetAttribute("GroupPrefix", ToolElement.Attributes["GroupPrefix"].Value + " + " + Item.OutputCaption);
							}
							else
							{
								// <Tool ... />
								XmlElement ToolElement = XGETaskDocument.CreateElement("Tool");
								ToolsElement.AppendChild(ToolElement);

								ParamsToTool.Add(Key, ToolName);
								ParamsToToolElement.Add(Key, ToolElement);

								ToolElement.SetAttribute("Name", ToolName);
								ToolElement.SetAttribute("AllowRemote", Element.Attributes["AllowRemote"].Value);
								if (Element.HasAttribute("OutputPrefix"))
								{
									ToolElement.SetAttribute("OutputPrefix", Element.Attributes["OutputPrefix"].Value);
								}
								ToolElement.SetAttribute("GroupPrefix", "** For " + Item.OutputCaption);

								ToolElement.SetAttribute("Params", Element.Attributes["Params"].Value);
								ToolElement.SetAttribute("Path", Element.Attributes["Path"].Value);
								if(Element.HasAttribute("VCCompiler"))
								{
									ToolElement.SetAttribute("VCCompiler", Element.Attributes["VCCompiler"].Value);
								}
								ToolElement.SetAttribute("SkipIfProjectFailed", Element.Attributes["SkipIfProjectFailed"].Value);
								if (Element.HasAttribute("AutoReserveMemory"))
								{
									ToolElement.SetAttribute("AutoReserveMemory", Element.Attributes["AutoReserveMemory"].Value);
								}
								ToolElement.SetAttribute("OutputFileMasks", Element.Attributes["OutputFileMasks"].Value);
								//ToolElement.SetAttribute("AllowRestartOnLocal", "false");  //vs2012 can't really restart, so we will go with this for now
								if (Element.Attributes["OutputFileMasks"].Value == "PCLaunch.rc.res")
								{
									// total hack, when doing clean compiles, this output directory does not exist, we need to create it now
									string Parms = Element.Attributes["Params"].Value;
									string Start = "/fo \"";
									int StartIndex = Parms.IndexOf(Start);
									if (StartIndex >= 0)
									{
										Parms = Parms.Substring(StartIndex + Start.Length);
										int EndIndex = Parms.IndexOf("\"");
										if (EndIndex > 0)
										{
											string ResLocation = CommandUtils.CombinePaths(Parms.Substring(0, EndIndex));
											if (!CommandUtils.DirectoryExists_NoExceptions(CommandUtils.GetDirectoryName(ResLocation)))
											{
												CommandUtils.CreateDirectory(CommandUtils.GetDirectoryName(ResLocation));
											}
										}
									}
								}
							}
						}
					}
					{
						var NextDependencies = new List<string>();

						var Elements = UBTTask.GetElementsByTagName("Task");
						foreach (XmlElement Element in Elements)
						{
							string ToolName = string.Format("{0}_{1}", Element.Attributes["Tool"].Value, Job);
							string ActionName = string.Format("{0}_{1}", Element.Attributes["Name"].Value, Job);
							string OriginalActionName = ActionName;

							if (ToolToTool.ContainsKey(ToolName))
							{
								ToolName = ToolToTool[ToolName];
								ActionName = ToolToAction[ToolName];
								ActionToAction.Add(OriginalActionName, ActionName);
							}
							else
							{
								ActionToAction.Add(OriginalActionName, ActionName);
								ToolToAction.Add(ToolName, ActionName);
								// <Task ... />
								XmlElement TaskElement = XGETaskDocument.CreateElement("Task");
								ProjectElement.AppendChild(TaskElement);

								TaskElement.SetAttribute("SourceFile", Element.Attributes["SourceFile"].Value);
								if (Element.HasAttribute("Caption"))
								{
									TaskElement.SetAttribute("Caption", Element.Attributes["Caption"].Value);
								}
								TaskElement.SetAttribute("Name", ActionName);
								NextDependencies.Add(ActionName);
								TaskElement.SetAttribute("Tool", ToolName);
								TaskElement.SetAttribute("WorkingDir", Element.Attributes["WorkingDir"].Value);
								TaskElement.SetAttribute("SkipIfProjectFailed", Element.Attributes["SkipIfProjectFailed"].Value);

								string NewDepends = "";
								if (Element.HasAttribute("DependsOn"))
								{
									string Depends = Element.Attributes["DependsOn"].Value;
									while (Depends.Length > 0)
									{
										string ThisAction = Depends;
										int Semi = Depends.IndexOf(";");
										if (Semi >= 0)
										{
											ThisAction = Depends.Substring(0, Semi);
											Depends = Depends.Substring(Semi + 1);
										}
										else
										{
											Depends = "";
										}
										if (ThisAction.Length > 0)
										{
											if (NewDepends.Length > 0)
											{
												NewDepends += ";";
											}
											string ActionJob = ThisAction + string.Format("_{0}", Job);
											if (!ActionToAction.ContainsKey(ActionJob))
											{
												throw new AutomationException("Action not found '{0}' in {1}->{2}", ActionJob, XGEFile, TargetFile);
												// the XGE schedule is not topologically sorted. Hmmm. Well, we make a scary assumption here that this 
											}
											NewDepends += ActionToAction[ActionJob];
										}
									}
								}
								foreach (var Dep in CurrentDependencies)
								{
									if (NewDepends.Length > 0)
									{
										NewDepends += ";";
									}
									NewDepends += Dep;
								}
								if (NewDepends != "")
								{
									TaskElement.SetAttribute("DependsOn", NewDepends);
								}
							}

						}
						CurrentDependencies.AddRange(NextDependencies);
					}
					Job++;
				}
			}
			return (Job > 0);
		}

		public void ClearExportedXGEXML()
		{
			foreach (var XGEFile in FindXGEFiles())
			{
				CommandUtils.DeleteFile(XGEFile);
			}
		}

		public bool CanUseXGE(UnrealBuildTool.UnrealTargetPlatform Platform)
		{
			return UnrealBuildTool.PlatformExports.CanUseXGE(Platform);
		}

		private bool ParseParam(string Name)
		{
			return OwnerCommand != null && OwnerCommand.ParseParam(Name);
		}

		private string ParseParamValue(string Name)
		{
			return (OwnerCommand != null)? OwnerCommand.ParseParamValue(Name) : null;
		}

		private int ParseParamInt(string Name, int Default = 0)
		{
			return (OwnerCommand != null)? OwnerCommand.ParseParamInt(Name, Default) : Default;
		}

		/// <summary>
		/// Executes a build.
		/// </summary>
		/// <param name="Agenda">Build agenda.</param>
		/// <param name="InDeleteBuildProducts">if specified, determines if the build products will be deleted before building. If not specified -clean parameter will be used,</param>
		/// <param name="InUpdateVersionFiles">True if the version files are to be updated </param>
		/// <param name="InForceNoXGE">If true will force XGE off</param>
		/// <param name="InUseParallelExecutor">If true AND XGE not present or not being used then use ParallelExecutor</param>
		public void Build(BuildAgenda Agenda, bool? InDeleteBuildProducts = null, bool InUpdateVersionFiles = true, bool InForceNoXGE = false, bool InUseParallelExecutor = false, bool InForceNonUnity = false, bool InForceUnity = false, bool InShowProgress = false, Dictionary<UnrealBuildTool.UnrealTargetPlatform, Dictionary<string, string>> PlatformEnvVars = null, int? InChangelistNumberOverride = null, Dictionary<BuildTarget, BuildManifest> InTargetToManifest = null)
		{
			if (!CommandUtils.CmdEnv.HasCapabilityToCompile)
			{
				throw new AutomationException("You are attempting to compile on a machine that does not have a supported compiler!");
			}
			DeleteBuildProducts = InDeleteBuildProducts.HasValue ? InDeleteBuildProducts.Value : ParseParam("Clean");
			if (InUpdateVersionFiles)
			{
				UpdateVersionFiles(ActuallyUpdateVersionFiles: true, ChangelistNumberOverride: InChangelistNumberOverride);
			}

			//////////////////////////////////////

			// make a set of unique platforms involved
			var UniquePlatforms = new List<UnrealBuildTool.UnrealTargetPlatform>();
			foreach (var Target in Agenda.Targets)
			{
				if (!UniquePlatforms.Contains(Target.Platform))
				{
					UniquePlatforms.Add(Target.Platform);
				}
			}

			// allow all involved platforms to hook into the agenda
			foreach (var TargetPlatform in UniquePlatforms)
			{
				Platform.GetPlatform(TargetPlatform).PreBuildAgenda(this, Agenda);
			}


			foreach (var File in Agenda.ExtraDotNetFiles)
			{
				PrepareBuildProductsForCSharpProj(Path.Combine(CommandUtils.CmdEnv.LocalRoot, File));
			}

			if (Agenda.SwarmProject != "")
			{
				string SwarmSolution = Path.Combine(CommandUtils.CmdEnv.LocalRoot, Agenda.SwarmProject);
				PrepareBuildProductsForCSharpProj(SwarmSolution);

				CommandUtils.BuildSolution(CommandUtils.CmdEnv, SwarmSolution, "Development", "Mixed Platforms");
				AddSwarmBuildProducts();
			}

			foreach (var DotNetSolution in Agenda.DotNetSolutions)
			{
				string Solution = Path.Combine(CommandUtils.CmdEnv.LocalRoot, DotNetSolution);

				PrepareBuildProductsForCSharpProj(Solution);

				CommandUtils.BuildSolution(CommandUtils.CmdEnv, Solution, "Development", "Any CPU");

				AddBuildProductsForCSharpProj(Solution);
			}

			foreach (var DotNetProject in Agenda.DotNetProjects)
			{
				string CsProj = Path.Combine(CommandUtils.CmdEnv.LocalRoot, DotNetProject);

				PrepareBuildProductsForCSharpProj(CsProj);

				CommandUtils.BuildCSharpProject(CommandUtils.CmdEnv, CsProj);

				AddBuildProductsForCSharpProj(CsProj);
			}

			foreach (var IOSDotNetProject in Agenda.IOSDotNetProjects)
			{
				string IOSCsProj = Path.Combine(CommandUtils.CmdEnv.LocalRoot, IOSDotNetProject);

				PrepareBuildProductsForCSharpProj(IOSCsProj);

				CommandUtils.BuildCSharpProject(CommandUtils.CmdEnv, IOSCsProj);

				AddIOSBuildProductsForCSharpProj(IOSCsProj);
			}

			foreach (var HTML5DotNetProject in Agenda.HTML5DotNetProjects)
			{
				string CsProj = Path.Combine(CommandUtils.CmdEnv.LocalRoot, HTML5DotNetProject);

				PrepareBuildProductsForCSharpProj(CsProj);

				CommandUtils.BuildCSharpProject(CommandUtils.CmdEnv, CsProj);

				AddBuildProductsForCSharpProj(CsProj);
			}

			foreach (var File in Agenda.ExtraDotNetFiles)
			{
				AddBuildProductsForCSharpProj(Path.Combine(CommandUtils.CmdEnv.LocalRoot, File));
			}

			bool bForceMonolithic = ParseParam("ForceMonolithic");
			bool bForceNonUnity = ParseParam("ForceNonUnity") || InForceNonUnity;
			bool bForceUnity = ParseParam("ForceUnity") || InForceUnity;
			bool bForceDebugInfo = ParseParam("ForceDebugInfo");
			bool bUsedXGE = false;
			bool bCanUseXGE = !ParseParam("NoXGE") && !InForceNoXGE;
			string XGEConsole = XGEConsoleExe();
			if (bCanUseXGE && string.IsNullOrEmpty(XGEConsole))
			{
				CommandUtils.LogLog("XGE was requested, but is unavailable, so we won't use it.");
				bCanUseXGE = false;
			}

			// only run ParallelExecutor if not running XGE (and we've requested ParallelExecutor and it exists)
			bool bCanUseParallelExecutor = !bCanUseXGE && InUseParallelExecutor && (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64);
			CommandUtils.LogLog("************************* UE4Build:");
			CommandUtils.LogLog("************************* ForceMonolithic: {0}", bForceMonolithic);
			CommandUtils.LogLog("************************* ForceNonUnity:{0} ", bForceNonUnity);
			CommandUtils.LogLog("************************* ForceDebugInfo: {0}", bForceDebugInfo);
			CommandUtils.LogLog("************************* UseXGE: {0}", bCanUseXGE);
			CommandUtils.LogLog("************************* UseParallelExecutor: {0}", bCanUseParallelExecutor);

			// process XGE files
			if (bCanUseXGE || bCanUseParallelExecutor)
			{
				string TaskFilePath = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, @"UAT_XGE.xml");

				if (DeleteBuildProducts)
				{
					foreach (var Target in Agenda.Targets)
					{
						CleanWithUBT(Target.TargetName, Target.Platform, Target.Config.ToString(), Target.UprojectPath, bForceMonolithic, bForceNonUnity, bForceDebugInfo, Target.UBTArgs, bForceUnity);
					}
				}
				foreach (var Target in Agenda.Targets)
				{
					if (Target.TargetName == "UnrealHeaderTool" || !CanUseXGE(Target.Platform))
					{
						// When building a target for Mac or iOS, use UBT's -flushmac option to clean up the remote builder
						bool bForceFlushMac = DeleteBuildProducts && (Target.Platform == UnrealBuildTool.UnrealTargetPlatform.Mac || Target.Platform == UnrealBuildTool.UnrealTargetPlatform.IOS);
						CommandUtils.LogSetProgress(InShowProgress, "Building header tool...");
						BuildManifest Manifest = BuildWithUBT(Target.TargetName, Target.Platform, Target.Config.ToString(), Target.UprojectPath, bForceMonolithic, bForceNonUnity, bForceDebugInfo, bForceFlushMac, !bCanUseXGE || !CanUseXGE(Target.Platform), Target.UBTArgs, bForceUnity);
						if(InTargetToManifest != null)
						{
							InTargetToManifest[Target] = Manifest;
						}
					}
				}

				List<XGEItem> XGEItems = new List<XGEItem>();
				CommandUtils.LogSetProgress(InShowProgress, "Generating headers...");
				foreach (var Target in Agenda.Targets)
				{
					if (Target.TargetName != "UnrealHeaderTool" && CanUseXGE(Target.Platform))
					{
						XGEItem Item = XGEPrepareBuildWithUBT(Target.TargetName, Target.Platform, Target.Config.ToString(), Target.UprojectPath, bForceMonolithic, bForceNonUnity, bForceDebugInfo, Target.UBTArgs, bForceUnity);
						if(InTargetToManifest != null)
						{
							InTargetToManifest[Target] = Item.Manifest;
						}
						XGEItems.Add(Item);
					}
				}
				if (XGEItems.Count > 0)
				{
					string XGETool = null;
					string Args = null;
					if (bCanUseXGE) 
					{
						XGETool = XGEConsoleExePath;
						Args = "\"" + TaskFilePath + "\" /Rebuild /MaxCPUS=200";
						if (ParseParam("StopOnErrors"))
						{
							Args += " /StopOnErrors";
						}

						// A bug in the UCRT can cause XGE to hang on VS2015 builds. Figure out if this hang is likely to effect this build and workaround it if able.
						string XGEVersion = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Xoreax\IncrediBuild\Builder", "Version", null) as string;
						if (XGEVersion != null)
						{
							// Per Xoreax support, subtract 1001000 from the registry value to get the build number of the installed XGE.
							int XGEBuildNumber;
							if (Int32.TryParse(XGEVersion, out XGEBuildNumber) && XGEBuildNumber - 1001000 >= 1659)
							{
								Args += " /no_watchdog_thread";
							}
						}
					}

					if (!bCanUseParallelExecutor && String.IsNullOrEmpty(XGETool))
					{
						throw new AutomationException("BUILD FAILED: no tool present to process XGE files");
					}

					CommandUtils.LogSetProgress(InShowProgress, "Building...");
					if (!ProcessXGEItems(XGEItems, XGETool, Args, TaskFilePath, Agenda.DoRetries, Agenda.SpecialTestFlag, InShowProgress))
					{
						throw new AutomationException("BUILD FAILED: {0} failed, retries not enabled:", XGETool);
					}
					else
					{
						bUsedXGE = true;
					}
				}
				else
				{
					bUsedXGE = true; // if there was nothing to build, we still consider this a success
				}
			}

			if (!bUsedXGE)
			{
				if (DeleteBuildProducts)
				{
					foreach (var Target in Agenda.Targets)
					{
						CleanWithUBT(Target.TargetName, Target.Platform, Target.Config.ToString(), Target.UprojectPath, bForceMonolithic, bForceNonUnity, bForceDebugInfo, Target.UBTArgs, bForceUnity);
					}
				}
				foreach (var Target in Agenda.Targets)
				{
					// When building a target for Mac or iOS, use UBT's -flushmac option to clean up the remote builder
					bool bForceFlushMac = DeleteBuildProducts && (Target.Platform == UnrealBuildTool.UnrealTargetPlatform.Mac || Target.Platform == UnrealBuildTool.UnrealTargetPlatform.IOS);
					BuildManifest Manifest = BuildWithUBT(Target.TargetName, Target.Platform, Target.Config.ToString(), Target.UprojectPath, bForceMonolithic, bForceNonUnity, bForceDebugInfo, bForceFlushMac, true, Target.UBTArgs, bForceUnity);
					if(InTargetToManifest != null)
					{
						InTargetToManifest[Target] = Manifest;
					}
				}
			}

			// NOTE: OK, we're done building projects.  You can build additional targets after this.  Call FinalizebuildProducts() when done.
		}


		/// <summary>
		/// Checks to make sure there was at least one build product, and that all files exist.  Also, logs them all out.
		/// </summary>
		/// <param name="BuildProductFiles">List of files</param>
		public static void CheckBuildProducts(List<string> BuildProductFiles)
		{
			// Check build products
			{
				CommandUtils.LogLog("Build products *******");
				if (BuildProductFiles.Count < 1)
				{
					CommandUtils.Log("No build products were made");
				}
				else
				{
					foreach (var Product in BuildProductFiles)
					{
						if (!CommandUtils.FileExists(Product) && !CommandUtils.DirectoryExists(Product))
						{
							throw new AutomationException("BUILD FAILED {0} was a build product but no longer exists", Product);
						}
						CommandUtils.LogLog(Product);
					}
				}
				CommandUtils.LogLog("End Build products *******");
			}
		}


		/// <summary>
		/// Adds or edits existing files at head revision, expecting an exclusive lock, resolving by clobbering any existing version
		/// </summary>
		/// <param name="Files">List of files to check out</param>
		public static void AddBuildProductsToChangelist(int WorkingCL, List<string> Files)
		{
			CommandUtils.Log("Adding {0} build products to changelist {1}...", Files.Count, WorkingCL);
			foreach (var File in Files)
			{
				CommandUtils.P4.Sync("-f -k " + CommandUtils.MakePathSafeToUseWithCommandLine(File) + "#head"); // sync the file without overwriting local one
				if (!CommandUtils.FileExists(File))
				{
					throw new AutomationException("BUILD FAILED {0} was a build product but no longer exists", File);
				}

				CommandUtils.P4.ReconcileNoDeletes(WorkingCL, CommandUtils.MakePathSafeToUseWithCommandLine(File));

				// Change file type on binary files to be always writeable.
				var FileStats = CommandUtils.P4.FStat(File);

                if (CommandUtils.IsProbablyAMacOrIOSExe(File))
                {
                    if (FileStats.Type == P4FileType.Binary && (FileStats.Attributes & (P4FileAttributes.Executable | P4FileAttributes.Writeable)) != (P4FileAttributes.Executable | P4FileAttributes.Writeable))
                    {
                        CommandUtils.P4.ChangeFileType(File, (P4FileAttributes.Executable | P4FileAttributes.Writeable));
                    }
                }
                else
                {
					if (IsBuildProduct(File, FileStats) && (FileStats.Attributes & P4FileAttributes.Writeable) != P4FileAttributes.Writeable)
                    {
                        CommandUtils.P4.ChangeFileType(File, P4FileAttributes.Writeable);
                    }

                }                    
			}
		}

		/// <summary>
		/// Determines if this file is a build product.
		/// </summary>
		/// <param name="File">File path</param>
		/// <param name="FileStats">P4 file stats.</param>
		/// <returns>True if this is a Windows build product. False otherwise.</returns>
		private static bool IsBuildProduct(string File, P4FileStat FileStats)
		{
			if(FileStats.Type == P4FileType.Binary || IsBuildReceipt(File))
			{
				return true;
			}

			return FileStats.Type == P4FileType.Text && File.EndsWith(".exe.config", StringComparison.InvariantCultureIgnoreCase);
		}

		/// <summary>
		/// Add UBT files to build products
		/// </summary>
		public void AddUBTFilesToBuildProducts()
		{
			if (!GlobalCommandLine.Compile)
			{
				CommandUtils.LogVerbose("We are being asked to copy the UBT build products, but we are running precompiled, so this does not make much sense.");
			}

			var UBTLocation = Path.GetDirectoryName(GetUBTExecutable());
			var UBTFiles = new List<string>(new string[] 
					{
						"UnrealBuildTool.exe",
						"UnrealBuildTool.exe.config"
					});

			foreach (var UBTFile in UBTFiles)
			{
				var UBTProduct = CommandUtils.CombinePaths(UBTLocation, UBTFile);
				if (!CommandUtils.FileExists_NoExceptions(UBTProduct))
				{
					throw new AutomationException("Cannot add UBT to the build products because {0} does not exist.", UBTProduct);
				}
				AddBuildProduct(UBTProduct);
			}
		}

		/// <summary>
		/// Copy the UAT files to their precompiled location, and add them as build products
		/// </summary>
		public void AddUATLauncherFilesToBuildProducts()
		{
            var DotNetOutputLocation = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Binaries", "DotNET");

			var UATFiles = new List<string>(new string[] 
					{
						"AutomationToolLauncher.exe",
						"AutomationToolLauncher.exe.config",
					});

			foreach (var UATFile in UATFiles)
			{
				var OutputFile = CommandUtils.CombinePaths(DotNetOutputLocation, UATFile);
				if (!CommandUtils.FileExists_NoExceptions(OutputFile))
				{
					throw new AutomationException("Cannot add UAT to the build products because {0} does not exist.", OutputFile);
				}
				AddBuildProduct(OutputFile);
			}
		}

		/// <summary>
		/// Copy the UAT files to their precompiled location, and add them as build products
		/// </summary>
		public void AddUATFilesToBuildProducts()
		{
			// Find all DLLs (scripts and their dependencies)
			const string ScriptsPostfix = ".dll";
            var DotNetOutputLocation = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Binaries", "DotNET");
			var UATScriptsLocation = CommandUtils.CombinePaths(DotNetOutputLocation, "AutomationScripts");
			var UATScripts = Directory.GetFiles(UATScriptsLocation, "*" + ScriptsPostfix, SearchOption.AllDirectories);
			if (CommandUtils.IsNullOrEmpty(UATScripts))
			{
				throw new AutomationException("No automation scripts found in {0}. Cannot add UAT files to the build products.", UATScriptsLocation);
			}

			var UATFiles = new List<string>(new string[] 
					{
						"AutomationTool.exe",
						"AutomationTool.exe.config",
						"UnrealBuildTool.exe",
						"UnrealBuildTool.exe.config",
						"AutomationUtils.Automation.dll",
						"DotNETUtilities.dll",
						"MobileDeviceInterface.dll"
					});

			foreach (var UATFile in UATFiles)
			{
				var OutputFile = CommandUtils.CombinePaths(DotNetOutputLocation, UATFile);
				if (!CommandUtils.FileExists_NoExceptions(OutputFile))
				{
					throw new AutomationException("Cannot add UAT to the build products because {0} does not exist.", OutputFile);
				}
				AddBuildProduct(OutputFile);
			}

			// All scripts are expected to exist in DotNET/AutomationScripts subfolder.
			foreach (var UATScriptFilePath in UATScripts)
			{
				if (!CommandUtils.FileExists_NoExceptions(UATScriptFilePath))
				{
					throw new AutomationException("Cannot add UAT to the build products because {0} does not exist.", UATScriptFilePath);
				}

				AddBuildProduct(UATScriptFilePath);
			}
		}

		string GetUBTManifest(FileReference UProjectPath, string InAddArgs)
		{
			// Can't write to Engine directory on installed builds
			if (Automation.IsEngineInstalled() && UProjectPath != null)
			{
				return Path.Combine(Path.GetDirectoryName(UProjectPath.FullName), "Intermediate/Build/Manifest.xml");				
			}
			else
			{
				return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"/Engine/Intermediate/Build/Manifest.xml");
			}
		}

		public static string GetUBTExecutable()
		{
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/UnrealBuildTool.exe");
		}

		public string UBTExecutable
		{
			get
			{
				return GetUBTExecutable();							
			}
		}

		// List of everything we built so far
		public readonly List<string> BuildProductFiles = new List<string>();

		// Library files that were part of the build
		public readonly List<string> LibraryBuildProductFiles = new List<string>();

		private bool DeleteBuildProducts = true;
	}
}
