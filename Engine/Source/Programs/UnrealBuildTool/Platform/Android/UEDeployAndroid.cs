// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;
using System.Xml.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class UEDeployAndroid : UEBuildDeploy, IAndroidDeploy
	{
		// Minimum Android SDK that must be used for Java compiling
		readonly int MinimumSDKLevel = 23;

		// Minimum SDK version needed for Gradle based on active plugins (14 is for Google Play Services 11.0.4)
		private int MinimumSDKLevelForGradle = 14;

		// Reserved Java keywords not allowed in package names without modification
		static private string[] JavaReservedKeywords = new string[] {
			"abstract", "assert", "boolean", "break", "byte", "case", "catch", "char", "class", "const", "continue", "default", "do",
			"double", "else", "enum", "extends", "final", "finally", "float", "for", "goto", "if", "implements", "import", "instanceof",
			"int", "interface", "long", "native", "new", "package", "private", "protected", "public", "return", "short", "static",
			"strictfp", "super", "switch", "sychronized", "this", "throw", "throws", "transient", "try", "void", "volatile", "while",
			"false", "null", "true"
		};

		/// <summary>
		/// Internal usage for GetApiLevel
		/// </summary>
		private List<string> PossibleApiLevels = null;

		private FileReference ProjectFile;

		public UEDeployAndroid(FileReference InProjectFile)
		{
			ProjectFile = InProjectFile;
		}

		// Enable Gradle instead of Ant (project-level setting for now)
		private bool bGradleEnabled = false;

		private UnrealPluginLanguage UPL = null;
		private bool ARCorePluginEnabled = false;
		private bool FacebookPluginEnabled = false;
		private bool GearVRPluginEnabled = false;
		private bool GoogleVRPluginEnabled = false;
		private bool CrashlyticsPluginEnabled = false;

		public void SetAndroidPluginData(List<string> Architectures, List<string> inPluginExtraData)
		{
			List<string> NDKArches = new List<string>();
			foreach (var Arch in Architectures)
			{
				NDKArches.Add(GetNDKArch(Arch));
			}

			// check if certain plugins are enabled
			ARCorePluginEnabled = false;
			FacebookPluginEnabled = false;
			GoogleVRPluginEnabled = false;
			GearVRPluginEnabled = false;
			CrashlyticsPluginEnabled = false;
			foreach (var Plugin in inPluginExtraData)
			{
				// check if the Facebook plugin was enabled
				if (Plugin.Contains("OnlineSubsystemFacebook_UPL"))
				{
					FacebookPluginEnabled = true;
					continue;
				}

				// check if the ARCore plugin was enabled
				if (Plugin.Contains("GoogleARCoreBase_APL"))
				{
					ARCorePluginEnabled = true;
					continue;
				}

				// check if the Gear VR plugin was enabled
				if (Plugin.Contains("GearVR_APL"))
				{
					GearVRPluginEnabled = true;
					continue;
				}

				// check if the GoogleVR plugin was enabled
				if (Plugin.Contains("GoogleVRHMD"))
				{
					GoogleVRPluginEnabled = true;
					continue;
				}

				// check if Crashlytics plugin was enabled
				// NOTE: There is a thirdparty plugin using Crashlytics_UPL_Android.xml which shouldn't use this code so check full name
				if (Plugin.Contains("Crashlytics_UPL.xml"))
				{
					CrashlyticsPluginEnabled = true;
					continue;
				}
			}

			UPL = new UnrealPluginLanguage(ProjectFile, inPluginExtraData, NDKArches, "http://schemas.android.com/apk/res/android", "xmlns:android=\"http://schemas.android.com/apk/res/android\"", UnrealTargetPlatform.Android);
//			APL.SetTrace();
		}

		private void SetMinimumSDKLevelForGradle()
		{
			if (FacebookPluginEnabled)
			{
				MinimumSDKLevelForGradle = Math.Max(MinimumSDKLevelForGradle, 15);
			}
			if (ARCorePluginEnabled)
			{
				MinimumSDKLevelForGradle = Math.Max(MinimumSDKLevelForGradle, 19);
			}
		}

		/// <summary>
		/// Simple function to pipe output asynchronously
		/// </summary>
		private void ParseApiLevel(object Sender, DataReceivedEventArgs Event)
		{
			// DataReceivedEventHandler is fired with a null string when the output stream is closed.  We don't want to
			// print anything for that event.
			if (!String.IsNullOrEmpty(Event.Data))
			{
				string Line = Event.Data;
				if (Line.StartsWith("id:"))
				{
					// the line should look like: id: 1 or "android-19"
					string[] Tokens = Line.Split("\"".ToCharArray());
					if (Tokens.Length >= 2)
					{
						PossibleApiLevels.Add(Tokens[1]);
					}
				}
			}
		}

		private ConfigHierarchy GetConfigCacheIni(ConfigHierarchyType Type)
		{
			return ConfigCache.ReadHierarchy(Type, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
		}

		private string GetLatestSDKApiLevel(AndroidToolChain ToolChain)
		{
			// get a list of SDK platforms
			string PlatformsDir = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/platforms");
			if (!Directory.Exists(PlatformsDir))
			{
				throw new BuildException("No platforms found in {0}", PlatformsDir);
			}

			// return the latest of them
			string[] PlatformDirectories = Directory.GetDirectories(PlatformsDir);
			if (PlatformDirectories != null && PlatformDirectories.Length > 0)
			{
				return ToolChain.GetLargestApiLevel(PlatformDirectories);
			}

			throw new BuildException("Can't make an APK without an API installed ({0} does not contain any SDKs)", PlatformsDir);
		}

		private int GetApiLevelInt(string ApiString)
		{
			int VersionInt = 0;
			if (ApiString.Contains("-"))
			{
				int Version;
				if (int.TryParse(ApiString.Substring(ApiString.LastIndexOf('-') + 1), out Version))
				{
					VersionInt = Version;
				}
			}
			return VersionInt;
		}

		private string CachedSDKLevel = null;
		private string GetSdkApiLevel(AndroidToolChain ToolChain)
		{
			if (CachedSDKLevel == null)
			{
				// ask the .ini system for what version to use
				ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
				string SDKLevel;
				Ini.GetString("/Script/AndroidPlatformEditor.AndroidSDKSettings", "SDKAPILevel", out SDKLevel);

				// if we want to use whatever version the ndk uses, then use that
				if (SDKLevel == "matchndk")
				{
					SDKLevel = ToolChain.GetNdkApiLevel();
				}

				// run a command and capture output
				if (SDKLevel == "latest")
				{
					SDKLevel = GetLatestSDKApiLevel(ToolChain);
				}

				// make sure it is at least android-23
				int SDKLevelInt = GetApiLevelInt(SDKLevel);
				if (SDKLevelInt < MinimumSDKLevel)
				{
					Console.WriteLine("Requires at least SDK API level {0}, currently set to '{1}'", MinimumSDKLevel, SDKLevel);
					SDKLevel = GetLatestSDKApiLevel(ToolChain);

					SDKLevelInt = GetApiLevelInt(SDKLevel);
					if (SDKLevelInt < MinimumSDKLevel)
					{
						throw new BuildException("Can't make an APK without API 'android-" + MinimumSDKLevel.ToString() + "' minimum installed (see \"android.bat list targets\")");
					}
				}

				Console.WriteLine("Building Java with SDK API level '{0}'", SDKLevel);
				CachedSDKLevel = SDKLevel;
			}

			return CachedSDKLevel;
		}

		private string CachedBuildToolsVersion = null;
		private string LastAndroidHomePath = null;

		private uint GetRevisionValue(string VersionString)
		{
			// read up to 4 sections (ie. 20.0.3.5), first section most significant
			// each section assumed to be 0 to 255 range
			uint Value = 0;
			try
			{
				string[] Sections = VersionString.Split(".".ToCharArray());
				Value |= (Sections.Length > 0) ? (uint.Parse(Sections[0]) << 24) : 0;
				Value |= (Sections.Length > 1) ? (uint.Parse(Sections[1]) << 16) : 0;
				Value |= (Sections.Length > 2) ? (uint.Parse(Sections[2]) << 8) : 0;
				Value |= (Sections.Length > 3) ? uint.Parse(Sections[3]) : 0;
			}
			catch (Exception)
			{
				// ignore poorly formed version
			}
			return Value;
		}

		private string GetBuildToolsVersion(bool bGradle)
		{
			// return cached path if ANDROID_HOME has not changed
			string HomePath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%");
			if (CachedBuildToolsVersion != null && LastAndroidHomePath == HomePath)
			{
				return CachedBuildToolsVersion;
			}

			// get a list of the directories in build-tools.. may be more than one set installed (or none which is bad)
			string[] Subdirs = Directory.GetDirectories(Path.Combine(HomePath, "build-tools"));
			if (Subdirs.Length == 0)
			{
				throw new BuildException("Failed to find %ANDROID_HOME%/build-tools subdirectory. Run SDK manager and install build-tools.");
			}

			// valid directories will have a source.properties with the Pkg.Revision (there is no guarantee we can use the directory name as revision)
			string BestVersionString = null;
			uint BestVersion = 0;
			foreach (string CandidateDir in Subdirs)
			{
				string AaptFilename = Path.Combine(CandidateDir, Utils.IsRunningOnMono ? "aapt" : "aapt.exe");
				string RevisionString = "";
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
								RevisionString = PropertyLine.Substring(13);
								RevisionValue = GetRevisionValue(RevisionString);
								break;
							}
						}
					}
				}

				// remember it if newer version or haven't found one yet
				if (RevisionValue > BestVersion || BestVersionString == null)
				{
					BestVersion = RevisionValue;
					BestVersionString = RevisionString;
				}
			}

			if (BestVersionString == null)
			{
				throw new BuildException("Failed to find %ANDROID_HOME%/build-tools subdirectory with aapt. Run SDK manager and install build-tools.");
			}

			// with Gradle enabled use at least 24.0.2 (will be installed by Gradle if missing)
			if (bGradle && (BestVersion < ((24 << 24) | (0 << 16) | (2 << 8))))
			{
				BestVersionString = "24.0.2";
			}

			CachedBuildToolsVersion = BestVersionString;
			LastAndroidHomePath = HomePath;

			Console.WriteLine("Building with Build Tools version '{0}'", CachedBuildToolsVersion);

			return CachedBuildToolsVersion;
		}

		private bool IsVulkanSDKAvailable()
		{
			bool bHaveVulkan = false;

			// First look for VulkanSDK (two possible env variables)
			string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");
			if (String.IsNullOrEmpty(VulkanSDKPath))
			{
				VulkanSDKPath = Environment.GetEnvironmentVariable("VK_SDK_PATH");
			}

			// Note: header is the same for all architectures so just use arch-arm
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
			string NDKVulkanIncludePath = NDKPath + "/platforms/android-24/arch-arm/usr/include/vulkan";

			// Use NDK Vulkan header if discovered, or VulkanSDK if available
			if (File.Exists(NDKVulkanIncludePath + "/vulkan.h"))
			{
				bHaveVulkan = true;
			}
			else
			if (!String.IsNullOrEmpty(VulkanSDKPath))
			{
				bHaveVulkan = true;
			}

			return bHaveVulkan;
		}

		public static string GetOBBVersionNumber(int PackageVersion)
		{
			string VersionString = PackageVersion.ToString("0");
			return VersionString;
		}

		public bool PackageDataInsideApk(bool bDisallowPackagingDataInApk)
		{
			return PackageDataInsideApk(bDisallowPackagingDataInApk, null);
		}

		public bool PackageDataInsideApk(bool bDisallowPackagingDataInApk, ConfigHierarchy Ini)
		{
			if (bDisallowPackagingDataInApk)
			{
				return false;
			}

			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			}

			// we check this a lot, so make it easy 
			bool bPackageDataInsideApk;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bPackageDataInsideApk", out bPackageDataInsideApk);

			return bPackageDataInsideApk;
		}

		public bool UseExternalFilesDir(bool bDisallowExternalFilesDir, ConfigHierarchy Ini = null)
		{
			if (bDisallowExternalFilesDir)
			{
				return false;
			}

			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			}

			// we check this a lot, so make it easy 
			bool bUseExternalFilesDir;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bUseExternalFilesDir", out bUseExternalFilesDir);

			return bUseExternalFilesDir;
		}

		public bool IsPackagingForDaydream(ConfigHierarchy Ini = null)
		{
			// always false if the GoogleVR plugin wasn't enabled
			if (!GoogleVRPluginEnabled)
			{
				return false;
			}

			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			}

            List<string> GoogleVRCaps = new List<string>();
            if (Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "GoogleVRCaps", out GoogleVRCaps))
            {
                return GoogleVRCaps.Contains("Daydream33") || GoogleVRCaps.Contains("Daydream63");
            }
            else
            {
                // the default values for the VRCaps are Cardboard and Daydream33, so unless the
                // developer changes the mode, there will be no setting string to look up here
                return true;
            }
        }

		public bool IsPackagingForGearVR(ConfigHierarchy Ini = null)
		{
			// always false if the GearVR plugin wasn't enabled
			if (!GearVRPluginEnabled)
			{
				return false;
			}

			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			}

			bool bPackageForGearVR = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bPackageForGearVR", out bPackageForGearVR);

			return bPackageForGearVR;
		}

		public bool DisableVerifyOBBOnStartUp(ConfigHierarchy Ini = null)
		{
			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			}

			// we check this a lot, so make it easy 
			bool bDisableVerifyOBBOnStartUp;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bDisableVerifyOBBOnStartUp", out bDisableVerifyOBBOnStartUp);

			return bDisableVerifyOBBOnStartUp;
		}

		private static string GetAntPath()
		{
			// look up an ANT_HOME env var
			string AntHome = Environment.GetEnvironmentVariable("ANT_HOME");
			if (!string.IsNullOrEmpty(AntHome) && Directory.Exists(AntHome))
			{
				string AntPath = AntHome + "/bin/ant" + (Utils.IsRunningOnMono ? "" : ".bat");
				// use it if found
				if (File.Exists(AntPath))
				{
					return AntPath;
				}
			}

			// otherwise, look in the eclipse install for the ant plugin (matches the unzipped Android ADT from Google)
			string PluginDir = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/../eclipse/plugins");
			if (Directory.Exists(PluginDir))
			{
				string[] Plugins = Directory.GetDirectories(PluginDir, "org.apache.ant*");
				// use the first one with ant.bat
				if (Plugins.Length > 0)
				{
					foreach (string PluginName in Plugins)
					{
						string AntPath = PluginName + "/bin/ant" + (Utils.IsRunningOnMono ? "" : ".bat");
						// use it if found
						if (File.Exists(AntPath))
						{
							return AntPath;
						}
					}
				}
			}

			throw new BuildException("Unable to find ant.bat (via %ANT_HOME% or %ANDROID_HOME%/../eclipse/plugins/org.apache.ant*");
		}


		private static void CopyFileDirectory(string SourceDir, string DestDir, Dictionary<string, string> Replacements = null)
		{
			if (!Directory.Exists(SourceDir))
			{
				return;
			}

			string[] Files = Directory.GetFiles(SourceDir, "*.*", SearchOption.AllDirectories);
			foreach (string Filename in Files)
			{
				// skip template files
				if (Path.GetExtension(Filename) == ".template")
				{
					continue;
				}

				// make the dst filename with the same structure as it was in SourceDir
				string DestFilename = Path.Combine(DestDir, Utils.MakePathRelativeTo(Filename, SourceDir));
				if (File.Exists(DestFilename))
				{
					File.Delete(DestFilename);
				}

				// make the subdirectory if needed
				string DestSubdir = Path.GetDirectoryName(DestFilename);
				if (!Directory.Exists(DestSubdir))
				{
					Directory.CreateDirectory(DestSubdir);
				}

				// some files are handled specially
				string Ext = Path.GetExtension(Filename);
				if (Ext == ".xml" && Replacements != null)
				{
					string Contents = File.ReadAllText(Filename);

					// replace some variables
					foreach (var Pair in Replacements)
					{
						Contents = Contents.Replace(Pair.Key, Pair.Value);
					}

					// write out file
					File.WriteAllText(DestFilename, Contents);
				}
				else
				{
					File.Copy(Filename, DestFilename);

					// remove any read only flags
					FileInfo DestFileInfo = new FileInfo(DestFilename);
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
				}
			}
		}

		private static void DeleteDirectory(string InPath, string SubDirectoryToKeep = "")
		{
			// skip the dir we want to
			if (String.Compare(Path.GetFileName(InPath), SubDirectoryToKeep, true) == 0)
			{
				return;
			}

			// delete all files in here
			string[] Files;
			try
			{
				Files = Directory.GetFiles(InPath);
			}
			catch (Exception)
			{
				// directory doesn't exist so all is good
				return;
			}
			foreach (string Filename in Files)
			{
				try
				{
					// remove any read only flags
					FileInfo FileInfo = new FileInfo(Filename);
					FileInfo.Attributes = FileInfo.Attributes & ~FileAttributes.ReadOnly;
					FileInfo.Delete();
				}
				catch (Exception)
				{
					Log.TraceInformation("Failed to delete all files in directory {0}. Continuing on...", InPath);
				}
			}

			string[] Dirs = Directory.GetDirectories(InPath, "*.*", SearchOption.TopDirectoryOnly);
			foreach (string Dir in Dirs)
			{
				DeleteDirectory(Dir, SubDirectoryToKeep);
				// try to delete the directory, but allow it to fail (due to SubDirectoryToKeep still existing)
				try
				{
					Directory.Delete(Dir);
				}
				catch (Exception)
				{
					// do nothing
				}
			}
		}

		private void CleanCopyDirectory(string SourceDir, string DestDir)
		{
			// remove directory and any subdirectories before copying
			DeleteDirectory(DestDir);
			CopyFileDirectory(SourceDir, DestDir);
		}

		public string GetUE4BuildFilePath(String EngineDirectory)
		{
			return Path.GetFullPath(Path.Combine(EngineDirectory, "Build/Android/Java"));
		}

		public string GetUE4JavaSrcPath()
		{
			return Path.Combine("src", "com", "epicgames", "ue4");
		}

		public string GetUE4JavaFilePath(String EngineDirectory)
		{
			return Path.GetFullPath(Path.Combine(GetUE4BuildFilePath(EngineDirectory), GetUE4JavaSrcPath()));
		}

		public string GetUE4JavaBuildSettingsFileName(String EngineDirectory)
		{
			return Path.Combine(GetUE4JavaFilePath(EngineDirectory), "JavaBuildSettings.java");
		}

		public string GetUE4JavaDownloadShimFileName(string Directory)
		{
			return Path.Combine(Directory, "DownloadShim.java");
		}

		public string GetUE4TemplateJavaSourceDir(string Directory)
		{
			return Path.Combine(GetUE4BuildFilePath(Directory), "JavaTemplates");
		}

		public string GetUE4TemplateJavaDestination(string Directory, string FileName)
		{
			return Path.Combine(Directory, FileName);
		}

		public string GetUE4JavaOBBDataFileName(string Directory)
		{
			return Path.Combine(Directory, "OBBData.java");
		}

		public class TemplateFile
		{
			public string SourceFile;
			public string DestinationFile;
		}

		private void MakeDirectoryIfRequired(string DestFilename)
		{
			string DestSubdir = Path.GetDirectoryName(DestFilename);
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}
		}

		private int CachedStoreVersion = -1;

		public int GetStoreVersion()
		{
			if (CachedStoreVersion < 1)
			{
				var Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
				int StoreVersion = 1;
				Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "StoreVersion", out StoreVersion);

				bool bUseChangeListAsStoreVersion = false;
				Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bUseChangeListAsStoreVersion", out bUseChangeListAsStoreVersion);

				// override store version with changelist if enabled and is build machine
				if (bUseChangeListAsStoreVersion && Environment.GetEnvironmentVariable("IsBuildMachine") == "1")
				{
					int Changelist = 0;
					if (int.TryParse(EngineChangelist, out Changelist))
					{
						if (Changelist != 0)
						{
							StoreVersion = Changelist;
						}
					}
				}

				CachedStoreVersion = StoreVersion;
			}

			return CachedStoreVersion;
		}

		public void WriteJavaOBBDataFile(string FileName, string PackageName, List<string> ObbSources)
		{

			Log.TraceInformation("\n==== Writing to OBB data file {0} ====", FileName);

			int StoreVersion = GetStoreVersion();

			string[] obbDataFile = File.Exists(FileName) ? File.ReadAllLines(FileName) : null;

			StringBuilder obbData = new StringBuilder("package " + PackageName + ";\n\n");
			obbData.Append("public class OBBData\n{\n");
			obbData.Append("public static class XAPKFile {\npublic final boolean mIsMain;\npublic final String mFileVersion;\n");
			obbData.Append("public final long mFileSize;\nXAPKFile(boolean isMain, String fileVersion, long fileSize) {\nmIsMain = isMain;\nmFileVersion = fileVersion;\nmFileSize = fileSize;\n");
			obbData.Append("}\n}\n\n");

			// write the data here
			obbData.Append("public static final XAPKFile[] xAPKS = {\n");
			// For each obb file... but we only have one... for now anyway.
			bool first = ObbSources.Count > 1;
			foreach (string ObbSource in ObbSources)
			{
				obbData.Append("new XAPKFile(\ntrue, // true signifies a main file\n");
				obbData.AppendFormat("\"{0}\", // the version of the APK that the file was uploaded against\n", GetOBBVersionNumber(StoreVersion));
				obbData.AppendFormat("{0}L // the length of the file in bytes\n", File.Exists(ObbSource) ? new FileInfo(ObbSource).Length : 0);
				obbData.AppendFormat("){0}\n", first ? "," : "");
				first = false;
			}
			obbData.Append("};\n"); // close off data

			//
			obbData.Append("};\n"); // close class definition off

			if (obbDataFile == null || !obbDataFile.SequenceEqual((obbData.ToString()).Split('\n')))
			{
				MakeDirectoryIfRequired(FileName);
				using (StreamWriter outputFile = new StreamWriter(FileName, false))
				{
					var obbSrc = obbData.ToString().Split('\n');
					foreach (var line in obbSrc)
					{
						outputFile.WriteLine(line);
					}
				}
			}
			else
			{
				Log.TraceInformation("\n==== OBB data file up to date so not writing. ====");
			}
		}

		public void WriteJavaDownloadSupportFiles(string ShimFileName, IEnumerable<TemplateFile> TemplateFiles, Dictionary<string, string> replacements)
		{
			// Deal with the Shim first as that is a known target and is easy to deal with
			// If it exists then read it
			string[] DestFileContent = File.Exists(ShimFileName) ? File.ReadAllLines(ShimFileName) : null;

			StringBuilder ShimFileContent = new StringBuilder("package com.epicgames.ue4;\n\n");

			ShimFileContent.AppendFormat("import {0}.OBBDownloaderService;\n", replacements["$$PackageName$$"]);
			ShimFileContent.AppendFormat("import {0}.DownloaderActivity;\n", replacements["$$PackageName$$"]);

			// Do OBB file checking without using DownloadActivity to avoid transit to another activity
			ShimFileContent.Append("import android.app.Activity;\n");
			ShimFileContent.Append("import com.google.android.vending.expansion.downloader.Helpers;\n");
			ShimFileContent.AppendFormat("import {0}.OBBData;\n", replacements["$$PackageName$$"]);

			ShimFileContent.Append("\n\npublic class DownloadShim\n{\n");
			ShimFileContent.Append("\tpublic static OBBDownloaderService DownloaderService;\n");
			ShimFileContent.Append("\tpublic static DownloaderActivity DownloadActivity;\n");
			ShimFileContent.Append("\tpublic static Class<DownloaderActivity> GetDownloaderType() { return DownloaderActivity.class; }\n");

			// Do OBB file checking without using DownloadActivity to avoid transit to another activity
			ShimFileContent.Append("\tpublic static boolean expansionFilesDelivered(Activity activity) {\n");
			ShimFileContent.Append("\t\tfor (OBBData.XAPKFile xf : OBBData.xAPKS) {\n");
			ShimFileContent.Append("\t\t\tString fileName = Helpers.getExpansionAPKFileName(activity, xf.mIsMain, xf.mFileVersion);\n");
			ShimFileContent.Append("\t\t\tGameActivity.Log.debug(\"Checking for file : \" + fileName);\n");
			ShimFileContent.Append("\t\t\tString fileForNewFile = Helpers.generateSaveFileName(activity, fileName);\n");
			ShimFileContent.Append("\t\t\tString fileForDevFile = Helpers.generateSaveFileNameDevelopment(activity, fileName);\n");
			ShimFileContent.Append("\t\t\tGameActivity.Log.debug(\"which is really being resolved to : \" + fileForNewFile + \"\\n Or : \" + fileForDevFile);\n");
			ShimFileContent.Append("\t\t\tif (Helpers.doesFileExist(activity, fileName, xf.mFileSize, false)) {\n");
			ShimFileContent.Append("\t\t\t\tGameActivity.Log.debug(\"Found OBB here: \" + fileForNewFile);\n");
			ShimFileContent.Append("\t\t\t}\n");
			ShimFileContent.Append("\t\t\telse if (Helpers.doesFileExistDev(activity, fileName, xf.mFileSize, false)) {\n");
			ShimFileContent.Append("\t\t\t\tGameActivity.Log.debug(\"Found OBB here: \" + fileForDevFile);\n");
			ShimFileContent.Append("\t\t\t}\n");
			ShimFileContent.Append("\t\t\telse return false;\n");
			ShimFileContent.Append("\t\t}\n");
			ShimFileContent.Append("\t\treturn true;\n");
			ShimFileContent.Append("\t}\n");

			ShimFileContent.Append("}\n");
			Log.TraceInformation("\n==== Writing to shim file {0} ====", ShimFileName);

			// If they aren't the same then dump out the settings
			if (DestFileContent == null || !DestFileContent.SequenceEqual((ShimFileContent.ToString()).Split('\n')))
			{
				MakeDirectoryIfRequired(ShimFileName);
				using (StreamWriter outputFile = new StreamWriter(ShimFileName, false))
				{
					var shimSrc = ShimFileContent.ToString().Split('\n');
					foreach (var line in shimSrc)
					{
						outputFile.WriteLine(line);
					}
				}
			}
			else
			{
				Log.TraceInformation("\n==== Shim data file up to date so not writing. ====");
			}

			// Now we move on to the template files
			foreach (var template in TemplateFiles)
			{
				string[] templateSrc = File.ReadAllLines(template.SourceFile);
				string[] templateDest = File.Exists(template.DestinationFile) ? File.ReadAllLines(template.DestinationFile) : null;

				for (int i = 0; i < templateSrc.Length; ++i)
				{
					string srcLine = templateSrc[i];
					bool changed = false;
					foreach (var kvp in replacements)
					{
						if (srcLine.Contains(kvp.Key))
						{
							srcLine = srcLine.Replace(kvp.Key, kvp.Value);
							changed = true;
						}
					}
					if (changed)
					{
						templateSrc[i] = srcLine;
					}
				}

				Log.TraceInformation("\n==== Writing to template target file {0} ====", template.DestinationFile);

				if (templateDest == null || templateSrc.Length != templateDest.Length || !templateSrc.SequenceEqual(templateDest))
				{
					MakeDirectoryIfRequired(template.DestinationFile);
					using (StreamWriter outputFile = new StreamWriter(template.DestinationFile, false))
					{
						foreach (var line in templateSrc)
						{
							outputFile.WriteLine(line);
						}
					}
				}
				else
				{
					Log.TraceInformation("\n==== Template target file up to date so not writing. ====");
				}
			}
		}

		public void WriteCrashlyticsResources(string UEBuildPath, string PackageName, string ApplicationDisplayName)
		{
			System.DateTime CurrentDateTime = System.DateTime.Now;
			string BuildID = Guid.NewGuid().ToString();

			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			string VersionDisplayName = "";
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "VersionDisplayName", out VersionDisplayName);

			StringBuilder CrashPropertiesContent = new StringBuilder("");
			CrashPropertiesContent.Append("# This file is automatically generated by Crashlytics to uniquely\n");
			CrashPropertiesContent.Append("# identify individual builds of your Android application.\n");
			CrashPropertiesContent.Append("#\n");
			CrashPropertiesContent.Append("# Do NOT modify, delete, or commit to source control!\n");
			CrashPropertiesContent.Append("#\n");
			CrashPropertiesContent.Append("# " + CurrentDateTime.ToString("D") + "\n");
			CrashPropertiesContent.Append("version_name=" + VersionDisplayName + "\n");
			CrashPropertiesContent.Append("package_name=" + PackageName + "\n");
			CrashPropertiesContent.Append("build_id=" + BuildID + "\n");
			CrashPropertiesContent.Append("version_code=" + GetStoreVersion().ToString() + "\n");

			string CrashPropertiesFileName = Path.Combine(UEBuildPath, "assets", "crashlytics-build.properties");
			MakeDirectoryIfRequired(CrashPropertiesFileName);
			File.WriteAllText(CrashPropertiesFileName, CrashPropertiesContent.ToString());
			Log.TraceInformation("==== Write {0}  ====", CrashPropertiesFileName);

			StringBuilder BuildIDContent = new StringBuilder("");
			BuildIDContent.Append("<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>\n");
			BuildIDContent.Append("<resources xmlns:tools=\"http://schemas.android.com/tools\">\n");
			BuildIDContent.Append("<!--\n");
			BuildIDContent.Append("  This file is automatically generated by Crashlytics to uniquely\n");
			BuildIDContent.Append("  identify individual builds of your Android application.\n");
			BuildIDContent.Append("\n");
			BuildIDContent.Append("  Do NOT modify, delete, or commit to source control!\n");
			BuildIDContent.Append("-->\n");
			BuildIDContent.Append("<string tools:ignore=\"UnusedResources, TypographyDashes\" name=\"com.crashlytics.android.build_id\" translatable=\"false\">" + BuildID + "</string>\n");
			BuildIDContent.Append("</resources>\n");

			string BuildIDFileName = Path.Combine(UEBuildPath, "res", "values", "com_crashlytics_build_id.xml");
			MakeDirectoryIfRequired(BuildIDFileName);
			File.WriteAllText(BuildIDFileName, BuildIDContent.ToString());
			Log.TraceInformation("==== Write {0}  ====", BuildIDFileName);
		}

		private static string GetNDKArch(string UE4Arch)
		{
			switch (UE4Arch)
			{
				case "-armv7":	return "armeabi-v7a";
                case "-arm64":  return "arm64-v8a";
				case "-x64":	return "x86_64";
				case "-x86":	return "x86";

				default: throw new BuildException("Unknown UE4 architecture {0}", UE4Arch);
			}
		}

		public static string GetUE4Arch(string NDKArch)
		{
			switch (NDKArch)
			{
				case "armeabi-v7a": return "-armv7";
                case "arm64-v8a":   return "-arm64";
                case "x86":         return "-x86";
                case "arm64":       return "-arm64";
				case "x86_64":
				case "x64":			return "-x64";
					
//				default: throw new BuildException("Unknown NDK architecture '{0}'", NDKArch);
                // future-proof by returning armv7 for unknown
                default:            return "-armv7";
			}
		}

		private static void StripDebugSymbols(string SourceFileName, string TargetFileName, string UE4Arch)
		{
			// Copy the file and remove read-only if necessary
			File.Copy(SourceFileName, TargetFileName, true);
			FileAttributes Attribs = File.GetAttributes(TargetFileName);
			if (Attribs.HasFlag(FileAttributes.ReadOnly))
			{
				File.SetAttributes(TargetFileName, Attribs & ~FileAttributes.ReadOnly);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = AndroidToolChain.GetStripExecutablePath(UE4Arch);
			StartInfo.Arguments = "--strip-debug \"" + TargetFileName + "\"";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}

		private static void CopySTL(AndroidToolChain ToolChain, string UE4BuildPath, string UE4Arch, string NDKArch, bool bForDistribution)
		{
			string GccVersion = "4.6";
			if (Directory.Exists(Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/gnu-libstdc++/4.9")))
			{
				GccVersion = "4.9";
			}
			else if (Directory.Exists(Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/gnu-libstdc++/4.8")))
			{
				GccVersion = "4.8";
			}

			// copy it in!
			string SourceSTLSOName = Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/gnu-libstdc++/") + GccVersion + "/libs/" + NDKArch + "/libgnustl_shared.so";
			string FinalSTLSOName = UE4BuildPath + "/libs/" + NDKArch + "/libgnustl_shared.so";

			// check to see if libgnustl_shared.so is newer than last time we copied (or needs stripping for distribution)
			bool bFileExists = File.Exists(FinalSTLSOName);
			TimeSpan Diff = File.GetLastWriteTimeUtc(FinalSTLSOName) - File.GetLastWriteTimeUtc(SourceSTLSOName);
			if (bForDistribution || !bFileExists || Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
			{
				if (bFileExists)
				{
					File.Delete(FinalSTLSOName);
				}
				Directory.CreateDirectory(Path.GetDirectoryName(FinalSTLSOName));
				if (bForDistribution)
				{
					// Strip debug symbols for distribution builds
					StripDebugSymbols(SourceSTLSOName, FinalSTLSOName, UE4Arch);
				}
				else
				{
					File.Copy(SourceSTLSOName, FinalSTLSOName, true);
				}
			}
		}

		private void CopyGfxDebugger(string UE4BuildPath, string UE4Arch, string NDKArch)
		{
			string AndroidGraphicsDebugger;
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "AndroidGraphicsDebugger", out AndroidGraphicsDebugger);

			switch (AndroidGraphicsDebugger.ToLower())
			{
				case "mali":
					{
						string MaliGraphicsDebuggerPath;
						AndroidPlatformSDK.GetPath(Ini, "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MaliGraphicsDebuggerPath", out MaliGraphicsDebuggerPath);
						if (Directory.Exists(MaliGraphicsDebuggerPath))
						{
							Directory.CreateDirectory(Path.Combine(UE4BuildPath, "libs", NDKArch));
							string MaliLibSrcPath = Path.Combine(MaliGraphicsDebuggerPath, @"target\android-non-root\arm", NDKArch, "libMGD.so");
							if (!File.Exists(MaliLibSrcPath))
							{
								// in v4.3.0 library location was changed
								MaliLibSrcPath = Path.Combine(MaliGraphicsDebuggerPath, @"target\android\arm\unrooted", NDKArch, "libMGD.so");
							}
							string MaliLibDstPath = Path.Combine(UE4BuildPath, "libs", NDKArch, "libMGD.so");

							Console.WriteLine("Copying {0} to {1}", MaliLibSrcPath, MaliLibDstPath);
							File.Copy(MaliLibSrcPath, MaliLibDstPath, true); 
						}
					}
					break;
				case "renderdoc":
					{
						string RenderDocPath;
						AndroidPlatformSDK.GetPath(Ini, "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "RenderDocPath", out RenderDocPath);
						if (Directory.Exists(RenderDocPath))
						{
							Directory.CreateDirectory(Path.Combine(UE4BuildPath, "libs", NDKArch));
							string RenderDocLibSrcPath = Path.Combine(RenderDocPath, @"android\lib", NDKArch, "libVkLayer_GLES_RenderDoc.so");
							string RenderDocLibDstPath = Path.Combine(UE4BuildPath, "libs", NDKArch, "libVkLayer_GLES_RenderDoc.so");

							Console.WriteLine("Copying {0} to {1}", RenderDocLibSrcPath, RenderDocLibDstPath);
							File.Copy(RenderDocLibSrcPath, RenderDocLibDstPath, true);
						}
					}
					break;

				// @TODO: Add NVIDIA Gfx Debugger
				/*
				case "nvidia":
					{
						Directory.CreateDirectory(UE4BuildPath + "/libs/" + NDKArch);
						File.Copy("F:/NVPACK/android-kk-egl-t124-a32/Stripped_libNvPmApi.Core.so", UE4BuildPath + "/libs/" + NDKArch + "/libNvPmApi.Core.so", true);
						File.Copy("F:/NVPACK/android-kk-egl-t124-a32/Stripped_libNvidia_gfx_debugger.so", UE4BuildPath + "/libs/" + NDKArch + "/libNvidia_gfx_debugger.so", true);
					}
					break;
				*/
				default:
					break;
			}
		}

		void CopyVulkanValidationLayers(string UE4BuildPath, string UE4Arch, string NDKArch, string Configuration)
		{
			bool bSupportsVulkan = false;
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSupportsVulkan", out bSupportsVulkan);

			bool bCopyVulkanLayers = bSupportsVulkan && (Configuration == "Debug" || Configuration == "Development");
			if (bCopyVulkanLayers)
			{
				string VulkanLayersDir = Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/third_party/vulkan/src/build-android/jniLibs/") + NDKArch;
				if (Directory.Exists(VulkanLayersDir))
				{
					Console.WriteLine("Copying vulkan layers from {0}", VulkanLayersDir);
					string DestDir = Path.Combine(UE4BuildPath, "libs", NDKArch);
					Directory.CreateDirectory(DestDir);
					CopyFileDirectory(VulkanLayersDir, DestDir);
				}
			}
		}

		private static int RunCommandLineProgramAndReturnResult(string WorkingDirectory, string Command, string Params, string OverrideDesc = null, bool bUseShellExecute = false)
		{
			if (OverrideDesc == null)
			{
				Log.TraceInformation("\nRunning: " + Command + " " + Params);
			}
			else if (OverrideDesc != "")
			{
				Log.TraceInformation(OverrideDesc);
				Log.TraceVerbose("\nRunning: " + Command + " " + Params);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDirectory;
			StartInfo.FileName = Command;
			StartInfo.Arguments = Params;
			StartInfo.UseShellExecute = bUseShellExecute;
			StartInfo.WindowStyle = ProcessWindowStyle.Minimized;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			Proc.Start();
			Proc.WaitForExit();

			return Proc.ExitCode;
		}

		private static void RunCommandLineProgramWithException(string WorkingDirectory, string Command, string Params, string OverrideDesc = null, bool bUseShellExecute = false)
		{
			if (OverrideDesc == null)
			{
				Log.TraceInformation("\nRunning: " + Command + " " + Params);
			}
			else if (OverrideDesc != "")
			{
				Log.TraceInformation(OverrideDesc);
				Log.TraceVerbose("\nRunning: " + Command + " " + Params);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDirectory;
			StartInfo.FileName = Command;
			StartInfo.Arguments = Params;
			StartInfo.UseShellExecute = bUseShellExecute;
			StartInfo.WindowStyle = ProcessWindowStyle.Minimized;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			Proc.Start();
			Proc.WaitForExit();

			// android bat failure
			if (Proc.ExitCode != 0)
			{
				throw new BuildException("{0} failed with args {1}", Command, Params);
			}
		}

		private bool CheckApplicationName(string UE4BuildPath, string ProjectName, out string ApplicationDisplayName)
		{
			string StringsXMLPath = Path.Combine(UE4BuildPath, "res/values/strings.xml");

			ApplicationDisplayName = null;
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ApplicationDisplayName", out ApplicationDisplayName);

			// use project name if display name is left blank
			if (String.IsNullOrWhiteSpace(ApplicationDisplayName))
			{
				ApplicationDisplayName = ProjectName;
			}

			// replace escaped characters (note: changes &# pattern before &, then patches back to allow escaped character codes in the string)
			ApplicationDisplayName = ApplicationDisplayName.Replace("&#", "$@#$").Replace("&", "&amp;").Replace("'", "\\'").Replace("\"", "\\\"").Replace("<", "&lt;").Replace(">", "&gt;").Replace("$@#$", "&#");

			// if it doesn't exist, need to repackage
			if (!File.Exists(StringsXMLPath))
			{
				return true;
			}

			// read it and see if needs to be updated
			string Contents = File.ReadAllText(StringsXMLPath);

			// find the key
			string AppNameTag = "<string name=\"app_name\">";
			int KeyIndex = Contents.IndexOf(AppNameTag);

			// if doesn't exist, need to repackage
			if (KeyIndex < 0)
			{
				return true;
			}

			// get the current value
			KeyIndex += AppNameTag.Length;
			int TagEnd = Contents.IndexOf("</string>", KeyIndex);
			if (TagEnd < 0)
			{
				return true;
			}
			string CurrentApplicationName = Contents.Substring(KeyIndex, TagEnd - KeyIndex);

			// no need to do anything if matches
			if (CurrentApplicationName == ApplicationDisplayName)
			{
				// name matches, no need to force a repackage
				return false;
			}

			// need to repackage
			return true;
		}

		private void UpdateProjectProperties(AndroidToolChain ToolChain, string UE4BuildPath, string ProjectName)
		{
			Log.TraceInformation("\n===={0}====UPDATING BUILD CONFIGURATION FILES====================================================", DateTime.Now.ToString());

			// get all of the libs (from engine + project)
			string JavaLibsDir = Path.Combine(UE4BuildPath, "JavaLibs");
			string[] LibDirs = Directory.GetDirectories(JavaLibsDir);

			// get existing project.properties lines (if any)
			string ProjectPropertiesFile = Path.Combine(UE4BuildPath, "project.properties");
			string[] PropertiesLines = new string[] { };
			if (File.Exists(ProjectPropertiesFile))
			{
				PropertiesLines = File.ReadAllLines(ProjectPropertiesFile);
			}

			// figure out how many libraries were already listed (if there were more than this listed, then we need to start the file over, because we need to unreference a library)
			int NumOutstandingAlreadyReferencedLibs = 0;
			foreach (string Line in PropertiesLines)
			{
				if (Line.StartsWith("android.library.reference."))
				{
					NumOutstandingAlreadyReferencedLibs++;
				}
			}

			// now go through each one and verify they are listed in project properties, and if not, add them
			List<string> LibsToBeAdded = new List<string>();
			foreach (string LibDir in LibDirs)
			{
				// put it in terms of the subdirectory that would be in the project.properties
				string RelativePath = "JavaLibs/" + Path.GetFileName(LibDir);

				// now look for this in the existing file
				bool bWasReferencedAlready = false;
				foreach (string Line in PropertiesLines)
				{
					if (Line.StartsWith("android.library.reference.") && Line.EndsWith(RelativePath))
					{
						// this lib was already referenced, don't need to readd
						bWasReferencedAlready = true;
						break;
					}
				}

				if (bWasReferencedAlready)
				{
					// if it was, no further action needed, and count it off
					NumOutstandingAlreadyReferencedLibs--;
				}
				else
				{
					// otherwise, we need to add it to the project properties
					LibsToBeAdded.Add(RelativePath);
				}
			}

			// now at this point, if there are any outstanding already referenced libs, we have too many, so we have to start over
			if (NumOutstandingAlreadyReferencedLibs > 0)
			{
				// @todo android: If a user had a project.properties in the game, NEVER do this
				Log.TraceInformation("There were too many libs already referenced in project.properties, tossing it");
				File.Delete(ProjectPropertiesFile);

				LibsToBeAdded.Clear();
				foreach (string LibDir in LibDirs)
				{
					// put it in terms of the subdirectory that would be in the project.properties
					LibsToBeAdded.Add("JavaLibs/" + Path.GetFileName(LibDir));
				}
			}

			// now update the project for each library
			string AndroidCommandPath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/tools/android" + (Utils.IsRunningOnMono ? "" : ".bat"));
			string UpdateCommandLine = "--silent update project --subprojects --name " + ProjectName + " --path . --target " + GetSdkApiLevel(ToolChain);
			foreach (string Lib in LibsToBeAdded)
			{
				string LocalUpdateCommandLine = UpdateCommandLine + " --library " + Lib;

				// make sure each library has a build.xml - --subprojects doesn't create build.xml files, but it will create project.properties
				// and later code needs each lib to have a build.xml
				RunCommandLineProgramWithException(UE4BuildPath, AndroidCommandPath, "--silent update lib-project --path " + Lib + " --target " + GetSdkApiLevel(ToolChain), "");
				RunCommandLineProgramWithException(UE4BuildPath, AndroidCommandPath, LocalUpdateCommandLine, "Updating project.properties, local.properties, and build.xml for " + Path.GetFileName(Lib) + "...");
			}

		}


		private string GetAllBuildSettings(AndroidToolChain ToolChain, string BuildPath, bool bForDistribution, bool bMakeSeparateApks, bool bPackageDataInsideApk, bool bDisableVerifyOBBOnStartUp, bool bUseExternalFilesDir)
		{
			// make the settings string - this will be char by char compared against last time
			StringBuilder CurrentSettings = new StringBuilder();
			CurrentSettings.AppendLine(string.Format("NDKROOT={0}", Environment.GetEnvironmentVariable("NDKROOT")));
			CurrentSettings.AppendLine(string.Format("ANDROID_HOME={0}", Environment.GetEnvironmentVariable("ANDROID_HOME")));
			CurrentSettings.AppendLine(string.Format("ANT_HOME={0}", Environment.GetEnvironmentVariable("ANT_HOME")));
			CurrentSettings.AppendLine(string.Format("JAVA_HOME={0}", Environment.GetEnvironmentVariable("JAVA_HOME")));
			CurrentSettings.AppendLine(string.Format("SDKVersion={0}", GetSdkApiLevel(ToolChain)));
			CurrentSettings.AppendLine(string.Format("bForDistribution={0}", bForDistribution));
			CurrentSettings.AppendLine(string.Format("bMakeSeparateApks={0}", bMakeSeparateApks));
			CurrentSettings.AppendLine(string.Format("bPackageDataInsideApk={0}", bPackageDataInsideApk));
			CurrentSettings.AppendLine(string.Format("bDisableVerifyOBBOnStartUp={0}", bDisableVerifyOBBOnStartUp));
			CurrentSettings.AppendLine(string.Format("bUseExternalFilesDir={0}", bUseExternalFilesDir));

			// all AndroidRuntimeSettings ini settings in here
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			ConfigHierarchySection Section = Ini.FindSection("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");
			if (Section != null)
			{
				foreach (string Key in Section.KeyNames)
				{
					IEnumerable<string> Values;
					Section.TryGetValues(Key, out Values);

					foreach (string Value in Values)
					{
						CurrentSettings.AppendLine(string.Format("{0}={1}", Key, Value));
					}
				}
			}

			Section = Ini.FindSection("/Script/AndroidPlatformEditor.AndroidSDKSettings");
			if (Section != null)
			{
				foreach (string Key in Section.KeyNames)
				{
					IEnumerable<string> Values;
					Section.TryGetValues(Key, out Values);
					foreach (string Value in Values)
					{
						CurrentSettings.AppendLine(string.Format("{0}={1}", Key, Value));
					}
				}
			}

			var Arches = ToolChain.GetAllArchitectures();
			foreach (string Arch in Arches)
			{
				CurrentSettings.AppendFormat("Arch={0}{1}", Arch, Environment.NewLine);
			}

			var GPUArchitectures = ToolChain.GetAllGPUArchitectures();
			foreach (string GPUArch in GPUArchitectures)
			{
				CurrentSettings.AppendFormat("GPUArch={0}{1}", GPUArch, Environment.NewLine);
			}

			return CurrentSettings.ToString();
		}

		private bool CheckDependencies(AndroidToolChain ToolChain, string ProjectName, string ProjectDirectory, string UE4BuildFilesPath, string GameBuildFilesPath, string EngineDirectory, List<string> SettingsFiles,
			string CookFlavor, string OutputPath, string UE4BuildPath, bool bMakeSeparateApks, bool bPackageDataInsideApk)
		{
			var Arches = ToolChain.GetAllArchitectures();
			var GPUArchitectures = ToolChain.GetAllGPUArchitectures();

			// check all input files (.so, java files, .ini files, etc)
			bool bAllInputsCurrent = true;
			foreach (string Arch in Arches)
			{
				foreach (string GPUArch in GPUArchitectures)
				{
					string SourceSOName = AndroidToolChain.InlineArchName(OutputPath, Arch, GPUArch);
					// if the source binary was UE4Game, replace it with the new project name, when re-packaging a binary only build
					string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UE4Game", ProjectName);
					string DestApkName = Path.Combine(ProjectDirectory, "Binaries/Android/") + ApkFilename + ".apk";

					// if we making multiple Apks, we need to put the architecture into the name
					if (bMakeSeparateApks)
					{
						DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch, GPUArch);
					}

					// check to see if it's out of date before trying the slow make apk process (look at .so and all Engine and Project build files to be safe)
					List<String> InputFiles = new List<string>();
					InputFiles.Add(SourceSOName);
					InputFiles.AddRange(Directory.EnumerateFiles(UE4BuildFilesPath, "*.*", SearchOption.AllDirectories));
					if (Directory.Exists(GameBuildFilesPath))
					{
						InputFiles.AddRange(Directory.EnumerateFiles(GameBuildFilesPath, "*.*", SearchOption.AllDirectories));
					}

					// make sure changed java files will rebuild apk
					InputFiles.AddRange(SettingsFiles);

					// rebuild if .pak files exist for OBB in APK case
					if (bPackageDataInsideApk)
					{
						string PAKFileLocation = ProjectDirectory + "/Saved/StagedBuilds/Android" + CookFlavor + "/" + ProjectName + "/Content/Paks";
						if (Directory.Exists(PAKFileLocation))
						{
							var PakFiles = Directory.EnumerateFiles(PAKFileLocation, "*.pak", SearchOption.TopDirectoryOnly);
							foreach (var Name in PakFiles)
							{
								InputFiles.Add(Name);
							}
						}
					}

					// look for any newer input file
					DateTime ApkTime = File.GetLastWriteTimeUtc(DestApkName);
					foreach (var InputFileName in InputFiles)
					{
						if (File.Exists(InputFileName))
						{
							// skip .log files
							if (Path.GetExtension(InputFileName) == ".log")
							{
								continue;
							}
							DateTime InputFileTime = File.GetLastWriteTimeUtc(InputFileName);
							if (InputFileTime.CompareTo(ApkTime) > 0)
							{
								bAllInputsCurrent = false;
								Log.TraceInformation("{0} is out of date due to newer input file {1}", DestApkName, InputFileName);
								break;
							}
						}
					}
				}
			}

			return bAllInputsCurrent;
		}

		private int ConvertDepthBufferIniValue(string IniValue)
		{
			switch (IniValue.ToLower())
			{
				case "bits16":
					return 16;
				case "bits24":
					return 24;
				case "bits32":
					return 32;
				default:
					return 0;
			}
		}

		private string ConvertOrientationIniValue(string IniValue)
		{
			switch (IniValue.ToLower())
			{
				case "portrait":
					return "portrait";
				case "reverseportrait":
					return "reversePortrait";
				case "sensorportrait":
					return "sensorPortrait";
				case "landscape":
					return "landscape";
				case "reverselandscape":
					return "reverseLandscape";
				case "sensorlandscape":
					return "sensorLandscape";
				case "sensor":
					return "sensor";
				case "fullsensor":
					return "fullSensor";
				default:
					return "landscape";
			}
		}

		private void DetermineScreenOrientationRequirements(out bool bNeedPortrait, out bool bNeedLandscape)
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			string Orientation;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "Orientation", out Orientation);

			bNeedLandscape = false;
			bNeedPortrait = false;

			switch (Orientation.ToLower())
			{
				case "portrait":
					bNeedPortrait = true;
					break;
				case "reverseportrait":
					bNeedPortrait = true;
					break;
				case "sensorportrait":
					bNeedPortrait = true;
					break;

				case "landscape":
					bNeedLandscape = true;
					break;
				case "reverselandscape":
					bNeedLandscape = true;
					break;
				case "sensorlandscape":
					bNeedLandscape = true;
					break;

				case "sensor":
					bNeedPortrait = true;
					bNeedLandscape = true;
					break;
				case "fullsensor":
					bNeedPortrait = true;
					bNeedLandscape = true;
					break;

				default:
					bNeedPortrait = true;
					bNeedLandscape = true;
					break;
			}
		}

		private void PickDownloaderScreenOrientation(string UE4BuildPath, bool bNeedPortrait, bool bNeedLandscape)
		{
			// Remove unused downloader_progress.xml to prevent missing resource
			if (!bNeedPortrait)
			{
				string LayoutPath = UE4BuildPath + "/res/layout-port/downloader_progress.xml";
				if (File.Exists(LayoutPath))
				{
					File.Delete(LayoutPath);
				}
			}
			if (!bNeedLandscape)
			{
				string LayoutPath = UE4BuildPath + "/res/layout-land/downloader_progress.xml";
				if (File.Exists(LayoutPath))
				{
					File.Delete(LayoutPath);
				}
			}

			// Loop through each of the resolutions (only /res/drawable/ is required, others are optional)
			string[] Resolutions = new string[] { "/res/drawable/", "/res/drawable-ldpi/", "/res/drawable-mdpi/", "/res/drawable-hdpi/", "/res/drawable-xhdpi/" };
			foreach (string ResolutionPath in Resolutions)
			{
				string PortraitFilename = UE4BuildPath + ResolutionPath + "downloadimagev.png";
				if (bNeedPortrait)
				{
					if (!File.Exists(PortraitFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Log.TraceWarning("Warning: Downloader screen source image {0} not available, downloader screen will not function properly!", PortraitFilename);
					}
				}
				else
				{
					// Remove unused image
					if (File.Exists(PortraitFilename))
					{
						File.Delete(PortraitFilename);
					}
				}

				string LandscapeFilename = UE4BuildPath + ResolutionPath + "downloadimageh.png";
				if (bNeedLandscape)
				{
					if (!File.Exists(LandscapeFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Log.TraceWarning("Warning: Downloader screen source image {0} not available, downloader screen will not function properly!", LandscapeFilename);
					}
				}
				else
				{
					// Remove unused image
					if (File.Exists(LandscapeFilename))
					{
						File.Delete(LandscapeFilename);
					}
				}
			}
		}
	
		private void PackageForDaydream(string UE4BuildPath)
		{
			bool bPackageForDaydream = IsPackagingForDaydream();

			if (!bPackageForDaydream)
			{
				// If this isn't a Daydream App, we need to make sure to remove
				// Daydream specific assets.

				// Remove the Daydream app  tile background.
				string AppTileBackgroundPath = UE4BuildPath + "/res/drawable-nodpi/vr_icon_background.png";
				if (File.Exists(AppTileBackgroundPath))
				{
					File.Delete(AppTileBackgroundPath);
				}

				// Remove the Daydream app tile icon.
				string AppTileIconPath = UE4BuildPath + "/res/drawable-nodpi/vr_icon.png";
				if (File.Exists(AppTileIconPath))
				{
					File.Delete(AppTileIconPath);
				}
			}
		}

		private void PickSplashScreenOrientation(string UE4BuildPath, bool bNeedPortrait, bool bNeedLandscape)
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			bool bShowLaunchImage = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bShowLaunchImage", out bShowLaunchImage);
			bool bPackageForGearVR = IsPackagingForGearVR(Ini); ;
			bool bPackageForDaydream = IsPackagingForDaydream(Ini);
			
			//override the parameters if we are not showing a launch image or are packaging for GearVR and Daydream
			if (bPackageForGearVR || bPackageForDaydream || !bShowLaunchImage)
			{
				bNeedPortrait = bNeedLandscape = false;
			}

			// Remove unused styles.xml to prevent missing resource
			if (!bNeedPortrait)
			{
				string StylesPath = UE4BuildPath + "/res/values-port/styles.xml";
				if (File.Exists(StylesPath))
				{
					File.Delete(StylesPath);
				}
			}
			if (!bNeedLandscape)
			{
				string StylesPath = UE4BuildPath + "/res/values-land/styles.xml";
				if (File.Exists(StylesPath))
				{
					File.Delete(StylesPath);
				}
			}

			// Loop through each of the resolutions (only /res/drawable/ is required, others are optional)
			string[] Resolutions = new string[] { "/res/drawable/", "/res/drawable-ldpi/", "/res/drawable-mdpi/", "/res/drawable-hdpi/", "/res/drawable-xhdpi/" };
			foreach (string ResolutionPath in Resolutions)
			{
				string PortraitFilename = UE4BuildPath + ResolutionPath + "splashscreen_portrait.png";
				if (bNeedPortrait)
				{
					if (!File.Exists(PortraitFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Log.TraceWarning("Warning: Splash screen source image {0} not available, splash screen will not function properly!", PortraitFilename);
					}
				}
				else
				{
					// Remove unused image
					if (File.Exists(PortraitFilename))
					{
						File.Delete(PortraitFilename);
					}
				}

				string LandscapeFilename = UE4BuildPath + ResolutionPath + "splashscreen_landscape.png";
				if (bNeedLandscape)
				{
					if (!File.Exists(LandscapeFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Log.TraceWarning("Warning: Splash screen source image {0} not available, splash screen will not function properly!", LandscapeFilename);
					}
				}
				else
				{
					// Remove unused image
					if (File.Exists(LandscapeFilename))
					{
						File.Delete(LandscapeFilename);
					}
				}
			}
		}

		private string CachedPackageName = null;

		private bool IsLetter(char Input)
		{
			return (Input >= 'A' && Input <= 'Z') || (Input >= 'a' && Input <= 'z');
		}

		private bool IsDigit(char Input)
		{
			return (Input >= '0' && Input <= '9');
		}

		private string GetPackageName(string ProjectName)
		{
			if (CachedPackageName == null)
			{
				ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
				string PackageName;
				Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "PackageName", out PackageName);

				if (PackageName.Contains("[PROJECT]"))
				{
					// project name must start with a letter
					if (!IsLetter(ProjectName[0]))
					{
						throw new BuildException("Package name segments must all start with a letter. Please replace [PROJECT] with a valid name");
					}

					// hyphens not allowed so change them to underscores in project name
					if (ProjectName.Contains("-"))
					{
						Trace.TraceWarning("Project name contained hyphens, converted to underscore");
						ProjectName = ProjectName.Replace("-", "_");
					}

					// check for special characters
					for (int Index = 0; Index < ProjectName.Length; Index++)
					{
						char c = ProjectName[Index];
						if (c != '.' && c != '_' && !IsDigit(c) && !IsLetter(c))
						{
							throw new BuildException("Project name contains illegal characters (only letters, numbers, and underscore allowed); please replace [PROJECT] with a valid name");
						}
					}

					PackageName = PackageName.Replace("[PROJECT]", ProjectName);
				}

				// verify minimum number of segments
				string[] PackageParts = PackageName.Split('.');
				int SectionCount = PackageParts.Length;
				if (SectionCount < 2)
				{
					throw new BuildException("Package name must have at least 2 segments separated by periods (ex. com.projectname, not projectname); please change in Android Project Settings. Currently set to '" + PackageName + "'");
				}

				// hyphens not allowed
				if (PackageName.Contains("-"))
				{
					throw new BuildException("Package names may not contain hyphens; please change in Android Project Settings. Currently set to '" + PackageName + "'");
				}

				// do not allow special characters
				for (int Index = 0; Index < PackageName.Length; Index++)
				{
					char c = PackageName[Index];
					if (c != '.' && c != '_' && !IsDigit(c) && !IsLetter(c))
					{
						throw new BuildException("Package name contains illegal characters (only letters, numbers, and underscore allowed); please change in Android Project Settings. Currently set to '" + PackageName + "'");
					}
				}

				// validate each segment
				for (int Index = 0; Index < SectionCount; Index++)
				{
					if (PackageParts[Index].Length < 1)
					{
						throw new BuildException("Package name segments must have at least one letter; please change in Android Project Settings. Currently set to '" + PackageName + "'");
					}

					if (!IsLetter(PackageParts[Index][0]))
					{
						throw new BuildException("Package name segments must start with a letter; please change in Android Project Settings. Currently set to '" + PackageName + "'");
					}

					// cannot use Java reserved keywords
					foreach (string Keyword in JavaReservedKeywords)
					{
						if (PackageParts[Index] == Keyword)
						{
							throw new BuildException("Package name segments must not be a Java reserved keyword (" + Keyword + "); please change in Android Project Settings. Currently set to '" + PackageName + "'");
						}
					}
				}

				Console.WriteLine("Using package name: '{0}'", PackageName);
				CachedPackageName = PackageName;
			}

			return CachedPackageName;
		}

		private string GetPublicKey()
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			string PlayLicenseKey = "";
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "GooglePlayLicenseKey", out PlayLicenseKey);
			return PlayLicenseKey;
		}

		private bool bHaveReadEngineVersion = false;
		private string EngineMajorVersion = "4";
		private string EngineMinorVersion = "0";
		private string EnginePatchVersion = "0";
		private string EngineChangelist = "0";

		private string ReadEngineVersion(string EngineDirectory)
		{
			if (!bHaveReadEngineVersion)
			{
				string EngineVersionFile = Path.Combine(EngineDirectory, "Source", "Runtime", "Launch", "Resources", "Version.h");
				string[] EngineVersionLines = File.ReadAllLines(EngineVersionFile);
				for (int i = 0; i < EngineVersionLines.Length; ++i)
				{
					if (EngineVersionLines[i].StartsWith("#define ENGINE_MAJOR_VERSION"))
					{
						EngineMajorVersion = EngineVersionLines[i].Split('\t')[1].Trim(' ');
					}
					else if (EngineVersionLines[i].StartsWith("#define ENGINE_MINOR_VERSION"))
					{
						EngineMinorVersion = EngineVersionLines[i].Split('\t')[1].Trim(' ');
					}
					else if (EngineVersionLines[i].StartsWith("#define ENGINE_PATCH_VERSION"))
					{
						EnginePatchVersion = EngineVersionLines[i].Split('\t')[1].Trim(' ');
					}
						else if (EngineVersionLines[i].StartsWith("#define BUILT_FROM_CHANGELIST"))
						{
							EngineChangelist = EngineVersionLines[i].Split(new char[] { ' ', '\t' })[2].Trim(' ');
						}
				}

				bHaveReadEngineVersion = true;
			}

			return EngineMajorVersion + "." + EngineMinorVersion + "." + EnginePatchVersion;
		}


		private string GenerateManifest(AndroidToolChain ToolChain, string ProjectName, string EngineDirectory, bool bIsForDistribution, bool bPackageDataInsideApk, string GameBuildFilesPath, bool bHasOBBFiles, bool bDisableVerifyOBBOnStartUp, string UE4Arch, string GPUArch, string CookFlavor, bool bUseExternalFilesDir, string Configuration, int SDKLevelInt)
		{
			// Read the engine version
			string EngineVersion = ReadEngineVersion(EngineDirectory);

			int StoreVersion = GetStoreVersion();

			string Arch = GetNDKArch(UE4Arch);
			int NDKLevelInt = ToolChain.GetNdkApiLevelInt();

			// 64-bit targets must be android-21 or higher
			if (NDKLevelInt < 21)
			{
				if (UE4Arch == "-arm64" || UE4Arch == "-x64")
				{
					NDKLevelInt = 21;
				}
			}

			// ini file to get settings from
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			string PackageName = GetPackageName(ProjectName);
			bool bEnableGooglePlaySupport;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableGooglePlaySupport", out bEnableGooglePlaySupport);
			bool bUseGetAccounts;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bUseGetAccounts", out bUseGetAccounts);
			string DepthBufferPreference;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "DepthBufferPreference", out DepthBufferPreference);
			int MinSDKVersion;
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MinSDKVersion", out MinSDKVersion);
			int TargetSDKVersion = MinSDKVersion;
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "TargetSDKVersion", out TargetSDKVersion);
			string VersionDisplayName;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "VersionDisplayName", out VersionDisplayName);
			float MaxAspectRatioValue;
			if (!Ini.TryGetValue("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MaxAspectRatio", out MaxAspectRatioValue))
			{
				MaxAspectRatioValue = 2.1f;
            }
			string Orientation;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "Orientation", out Orientation);
			bool EnableFullScreen;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bFullScreen", out EnableFullScreen);
			List<string> ExtraManifestNodeTags;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraManifestNodeTags", out ExtraManifestNodeTags);
			List<string> ExtraApplicationNodeTags;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraApplicationNodeTags", out ExtraApplicationNodeTags);
			List<string> ExtraActivityNodeTags;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraActivityNodeTags", out ExtraActivityNodeTags);
			string ExtraActivitySettings;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraActivitySettings", out ExtraActivitySettings);
			string ExtraApplicationSettings;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraApplicationSettings", out ExtraApplicationSettings);
			List<string> ExtraPermissions;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraPermissions", out ExtraPermissions);
			bool bPackageForGearVR = IsPackagingForGearVR(Ini);
			bool bEnableIAP = false;
			Ini.GetBool("OnlineSubsystemGooglePlay.Store", "bSupportsInAppPurchasing", out bEnableIAP);
			bool bShowLaunchImage = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bShowLaunchImage", out bShowLaunchImage);
			string AndroidGraphicsDebugger;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "AndroidGraphicsDebugger", out AndroidGraphicsDebugger);
			bool bSupportAdMob = true;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSupportAdMob", out bSupportAdMob);

			string InstallLocation;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "InstallLocation", out InstallLocation);
			switch (InstallLocation.ToLower())
			{
				case "preferexternal":
					InstallLocation = "preferExternal";
					break;
				case "auto":
					InstallLocation = "auto";
					break;
				default:
					InstallLocation = "internalOnly";
					break;
			}

			// fix up the MinSdkVersion
			if (NDKLevelInt > 19)
			{
				if (MinSDKVersion < 21)
				{
					MinSDKVersion = 21;
					Log.TraceInformation("Fixing minSdkVersion; NDK level above 19 requires minSdkVersion of 21 (arch={0})", UE4Arch.Substring(1));
				}
			}

			if (bGradleEnabled && MinSDKVersion < MinimumSDKLevelForGradle)
			{
				MinSDKVersion = MinimumSDKLevelForGradle;
				Log.TraceInformation("Fixing minSdkVersion; requires minSdkVersion of {0} with Gradle based on active plugins", MinimumSDKLevelForGradle);
			}

			if (TargetSDKVersion < MinSDKVersion)
			{
				TargetSDKVersion = MinSDKVersion;
			}

			// only apply density to configChanges if using android-24 or higher and minimum sdk is 17
			bool bAddDensity = (SDKLevelInt >= 24) && (MinSDKVersion >= 17);

			// disable GearVR if not supported platform (in this case only armv7 for now)
			if (UE4Arch != "-armv7")
			{
				if (bPackageForGearVR)
				{
					Log.TraceInformation("Disabling Package For GearVR for unsupported architecture {0}", UE4Arch);
					bPackageForGearVR = false;
				}
			}

			// disable splash screen for GearVR (for now)
			if (bPackageForGearVR)
			{
				if (bShowLaunchImage)
				{
					Log.TraceInformation("Disabling Show Launch Image for GearVR enabled application");
					bShowLaunchImage = false;
				}
			}

			bool bPackageForDaydream = IsPackagingForDaydream(Ini);
			// disable splash screen for daydream
			if (bPackageForDaydream)
			{
				if (bShowLaunchImage)
				{
					Log.TraceInformation("Disabling Show Launch Image for Daydream enabled application");
					bShowLaunchImage = false;
				}
			}

			//figure out which texture compressions are supported
			bool bETC1Enabled, bETC2Enabled, bDXTEnabled, bATCEnabled, bPVRTCEnabled, bASTCEnabled;
			bETC1Enabled = bETC2Enabled = bDXTEnabled = bATCEnabled = bPVRTCEnabled = bASTCEnabled = false;
			if (CookFlavor.Length < 1)
			{
				//All values supproted
				bETC1Enabled = bETC2Enabled = bDXTEnabled = bATCEnabled = bPVRTCEnabled = bASTCEnabled = true;
			}
			else
			{
				switch (CookFlavor)
				{
					case "_Multi":
						//need to check ini to determine which are supported
						Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bMultiTargetFormat_ETC1", out bETC1Enabled);
						Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bMultiTargetFormat_ETC2", out bETC2Enabled);
						Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bMultiTargetFormat_DXT", out bDXTEnabled);
						Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bMultiTargetFormat_ATC", out bATCEnabled);
						Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bMultiTargetFormat_PVRTC", out bPVRTCEnabled);
						Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bMultiTargetFormat_ASTC", out bASTCEnabled);
						break;
					case "_ETC1":
						bETC1Enabled = true;
						break;
					case "_ETC2":
						bETC2Enabled = true;
						break;
					case "_DXT":
						bDXTEnabled = true;
						break;
					case "_ATC":
						bATCEnabled = true;
						break;
					case "_PVRTC":
						bPVRTCEnabled = true;
						break;
					case "_ASTC":
						bASTCEnabled = true;
						break;
					default:
						Log.TraceWarning("Invalid or unknown CookFlavor used in GenerateManifest: {0}", CookFlavor);
						break;
				}
			}
			bool bSupportingAllTextureFormats = bETC1Enabled && bETC2Enabled && bDXTEnabled && bATCEnabled && bPVRTCEnabled && bASTCEnabled;

			// If it is only ETC2 we need to skip adding the texture format filtering and instead use ES 3.0 as minimum version (it requires ETC2)
			bool bOnlyETC2Enabled = (bETC2Enabled && !(bETC1Enabled || bDXTEnabled || bATCEnabled || bPVRTCEnabled || bASTCEnabled));

			StringBuilder Text = new StringBuilder();
			Text.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
			Text.AppendLine("<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"");
			Text.AppendLine(string.Format("          package=\"{0}\"", PackageName));
			if (ExtraManifestNodeTags != null)
			{
				foreach (string Line in ExtraManifestNodeTags)
				{
					Text.AppendLine("          " + Line);
				}
			}
			Text.AppendLine(string.Format("          android:installLocation=\"{0}\"", InstallLocation));
			Text.AppendLine(string.Format("          android:versionCode=\"{0}\"", StoreVersion));
			Text.AppendLine(string.Format("          android:versionName=\"{0}\">", VersionDisplayName));

			Text.AppendLine("");

			Text.AppendLine("\t<!-- Application Definition -->");
			Text.AppendLine("\t<application android:label=\"@string/app_name\"");
			Text.AppendLine("\t             android:icon=\"@drawable/icon\"");
			if (ExtraApplicationNodeTags != null)
			{
				foreach (string Line in ExtraApplicationNodeTags)
				{
					Text.AppendLine("\t             " + Line);
				}
			}
			Text.AppendLine("\t             android:hardwareAccelerated=\"true\"");
			Text.AppendLine("\t             android:hasCode=\"true\">");
			if (bShowLaunchImage)
			{
				// normal application settings
				Text.AppendLine("\t\t<activity android:name=\"com.epicgames.ue4.SplashActivity\"");
				Text.AppendLine("\t\t          android:label=\"@string/app_name\"");
				Text.AppendLine("\t\t          android:theme=\"@style/UE4SplashTheme\"");
				Text.AppendLine("\t\t          android:launchMode=\"singleTask\"");
				Text.AppendLine(string.Format("\t\t          android:screenOrientation=\"{0}\"", ConvertOrientationIniValue(Orientation)));
				Text.AppendLine(string.Format("\t\t          android:debuggable=\"{0}\">", bIsForDistribution ? "false" : "true"));
				Text.AppendLine("\t\t\t<intent-filter>");
				Text.AppendLine("\t\t\t\t<action android:name=\"android.intent.action.MAIN\" />");
				Text.AppendLine(string.Format("\t\t\t\t<category android:name=\"android.intent.category.LAUNCHER\" />"));
				Text.AppendLine("\t\t\t</intent-filter>");
				Text.AppendLine("\t\t</activity>");
				Text.AppendLine("\t\t<activity android:name=\"com.epicgames.ue4.GameActivity\"");
				Text.AppendLine("\t\t          android:label=\"@string/app_name\"");
				Text.AppendLine("\t\t          android:theme=\"@style/UE4SplashTheme\"");
				Text.AppendLine(bAddDensity ? "\t\t          android:configChanges=\"density|screenSize|orientation|keyboardHidden|keyboard\""
											: "\t\t          android:configChanges=\"screenSize|orientation|keyboardHidden|keyboard\"");
				}
			else
			{
				Text.AppendLine("\t\t<activity android:name=\"com.epicgames.ue4.GameActivity\"");
				Text.AppendLine("\t\t          android:label=\"@string/app_name\"");
				Text.AppendLine("\t\t          android:theme=\"@android:style/Theme.Black.NoTitleBar.Fullscreen\"");
				Text.AppendLine(bAddDensity ? "\t\t          android:configChanges=\"density|screenSize|orientation|keyboardHidden|keyboard\""
											: "\t\t          android:configChanges=\"screenSize|orientation|keyboardHidden|keyboard\"");
			}
			Text.AppendLine("\t\t          android:launchMode=\"singleTask\"");
			Text.AppendLine(string.Format("\t\t          android:screenOrientation=\"{0}\"", ConvertOrientationIniValue(Orientation)));
			if (ExtraActivityNodeTags != null)
			{
				foreach (string Line in ExtraActivityNodeTags)
				{
					Text.AppendLine("\t\t          " + Line);
				}
			}
			Text.AppendLine(string.Format("\t\t          android:debuggable=\"{0}\">", bIsForDistribution ? "false" : "true"));
			Text.AppendLine("\t\t\t<meta-data android:name=\"android.app.lib_name\" android:value=\"UE4\"/>");
			if (!bShowLaunchImage)
			{
				Text.AppendLine("\t\t\t<intent-filter>");
				Text.AppendLine("\t\t\t\t<action android:name=\"android.intent.action.MAIN\" />");
				Text.AppendLine(string.Format("\t\t\t\t<category android:name=\"android.intent.category.LAUNCHER\" />"));
				Text.AppendLine("\t\t\t</intent-filter>");
			}
			if (!string.IsNullOrEmpty(ExtraActivitySettings))
			{
				ExtraActivitySettings = ExtraActivitySettings.Replace("\\n", "\n");
				foreach (string Line in ExtraActivitySettings.Split("\r\n".ToCharArray()))
				{
					Text.AppendLine("\t\t\t" + Line);
				}
			}
			string ActivityAdditionsFile = Path.Combine(GameBuildFilesPath, "ManifestActivityAdditions.txt");
			if (File.Exists(ActivityAdditionsFile))
			{
				foreach (string Line in File.ReadAllLines(ActivityAdditionsFile))
				{
					Text.AppendLine("\t\t\t" + Line);
				}
			}
			Text.AppendLine("\t\t</activity>");

			// For OBB download support
			if (bShowLaunchImage)
			{
				Text.AppendLine("\t\t<activity android:name=\".DownloaderActivity\"");
				Text.AppendLine(string.Format("\t\t          android:screenOrientation=\"{0}\"", ConvertOrientationIniValue(Orientation)));
				Text.AppendLine(bAddDensity ? "\t\t          android:configChanges=\"density|screenSize|orientation|keyboardHidden|keyboard\""
											: "\t\t          android:configChanges=\"screenSize|orientation|keyboardHidden|keyboard\"");
				Text.AppendLine("\t\t          android:theme=\"@style/UE4SplashTheme\" />");
			}
			else
			{
				Text.AppendLine("\t\t<activity android:name=\".DownloaderActivity\" />");
			}

			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.EngineVersion\" android:value=\"{0}\"/>", EngineVersion));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.DepthBufferPreference\" android:value=\"{0}\"/>", ConvertDepthBufferIniValue(DepthBufferPreference)));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bPackageDataInsideApk\" android:value=\"{0}\"/>", bPackageDataInsideApk ? "true" : "false"));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bVerifyOBBOnStartUp\" android:value=\"{0}\"/>", (bIsForDistribution && !bDisableVerifyOBBOnStartUp) ? "true" : "false"));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bShouldHideUI\" android:value=\"{0}\"/>", EnableFullScreen ? "true" : "false"));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.ProjectName\" android:value=\"{0}\"/>", ProjectName));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bHasOBBFiles\" android:value=\"{0}\"/>", bHasOBBFiles ? "true" : "false"));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.BuildConfiguration\" android:value=\"{0}\"/>", Configuration));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bUseExternalFilesDir\" android:value=\"{0}\"/>", bUseExternalFilesDir ? "true" : "false"));
			if (bPackageForDaydream)
			{
				Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bDaydream\" android:value=\"true\"/>"));
			}
			Text.AppendLine("\t\t<meta-data android:name=\"com.google.android.gms.games.APP_ID\"");
			Text.AppendLine("\t\t           android:value=\"@string/app_id\" />");
			Text.AppendLine("\t\t<meta-data android:name=\"com.google.android.gms.version\"");
			Text.AppendLine("\t\t           android:value=\"@integer/google_play_services_version\" />");
			if (bSupportAdMob)
			{
				Text.AppendLine("\t\t<activity android:name=\"com.google.android.gms.ads.AdActivity\"");
				Text.AppendLine("\t\t          android:configChanges=\"keyboard|keyboardHidden|orientation|screenLayout|uiMode|screenSize|smallestScreenSize\"/>");
			}
			if (!string.IsNullOrEmpty(ExtraApplicationSettings))
			{
				ExtraApplicationSettings = ExtraApplicationSettings.Replace("\\n", "\n");
				foreach (string Line in ExtraApplicationSettings.Split("\r\n".ToCharArray()))
				{
					Text.AppendLine("\t\t" + Line);
				}
			}
			string ApplicationAdditionsFile = Path.Combine(GameBuildFilesPath, "ManifestApplicationAdditions.txt");
			if (File.Exists(ApplicationAdditionsFile))
			{
				foreach (string Line in File.ReadAllLines(ApplicationAdditionsFile))
				{
					Text.AppendLine("\t\t" + Line);
				}
			}

			// Required for OBB download support
			Text.AppendLine("\t\t<service android:name=\"OBBDownloaderService\" />");
			Text.AppendLine("\t\t<receiver android:name=\"AlarmReceiver\" />");

			Text.AppendLine("\t\t<receiver android:name=\"com.epicgames.ue4.LocalNotificationReceiver\" />");

			// Max supported aspect ratio
			string MaxAspectRatioString = MaxAspectRatioValue.ToString("f", System.Globalization.CultureInfo.InvariantCulture);
            Text.AppendLine(string.Format("\t\t<meta-data android:name=\"android.max_aspect\" android:value=\"{0}\" />", MaxAspectRatioString));
					
			Text.AppendLine("\t</application>");

			Text.AppendLine("");
			Text.AppendLine("\t<!-- Requirements -->");

			// check for an override for the requirements section of the manifest
			string RequirementsOverrideFile = Path.Combine(GameBuildFilesPath, "ManifestRequirementsOverride.txt");
			if (File.Exists(RequirementsOverrideFile))
			{
				foreach (string Line in File.ReadAllLines(RequirementsOverrideFile))
				{
					Text.AppendLine("\t" + Line);
				}
			}
			else
			{
				// need just the number part of the sdk
				Text.AppendLine(string.Format("\t<uses-sdk android:minSdkVersion=\"{0}\" android:targetSdkVersion=\"{1}\"/>", MinSDKVersion, TargetSDKVersion));
				Text.AppendLine("\t<uses-feature android:glEsVersion=\"" + AndroidToolChain.GetGLESVersionFromGPUArch(GPUArch, bOnlyETC2Enabled) + "\" android:required=\"true\" />");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.INTERNET\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.WRITE_EXTERNAL_STORAGE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.ACCESS_NETWORK_STATE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.WAKE_LOCK\"/>");
			//	Text.AppendLine("\t<uses-permission android:name=\"android.permission.READ_PHONE_STATE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"com.android.vending.CHECK_LICENSE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.ACCESS_WIFI_STATE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.MODIFY_AUDIO_SETTINGS\"/>");

				if (bEnableGooglePlaySupport && bUseGetAccounts)
				{
					Text.AppendLine("\t<uses-permission android:name=\"android.permission.GET_ACCOUNTS\"/>");
				}				
				
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.VIBRATE\"/>");
				//			Text.AppendLine("\t<uses-permission android:name=\"android.permission.DISABLE_KEYGUARD\"/>");

				if (bEnableIAP)
				{
					Text.AppendLine("\t<uses-permission android:name=\"com.android.vending.BILLING\"/>");
				}
				if (ExtraPermissions != null)
				{
					foreach (string Permission in ExtraPermissions)
					{
						string TrimmedPermission = Permission.Trim(' ');
						if (TrimmedPermission != "")
						{
							string PermissionString = string.Format("\t<uses-permission android:name=\"{0}\"/>", TrimmedPermission);
							if (!Text.ToString().Contains(PermissionString))
							{
								Text.AppendLine(PermissionString);
							}
						}
					}
				}
				string RequirementsAdditionsFile = Path.Combine(GameBuildFilesPath, "ManifestRequirementsAdditions.txt");
				if (File.Exists(RequirementsAdditionsFile))
				{
					foreach (string Line in File.ReadAllLines(RequirementsAdditionsFile))
					{
						Text.AppendLine("\t" + Line);
					}
				}
				if (AndroidGraphicsDebugger.ToLower() == "adreno")
				{
					string PermissionString = "\t<uses-permission android:name=\"com.qti.permission.PROFILER\"/>";
					if (!Text.ToString().Contains(PermissionString))
					{
						Text.AppendLine(PermissionString);
					}
				}

				if (!bSupportingAllTextureFormats)
				{
					Text.AppendLine("\t<!-- Supported texture compression formats (cooked) -->");
					if (bETC1Enabled)
					{
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_OES_compressed_ETC1_RGB8_texture\" />");
					}
					if (bETC2Enabled && !bOnlyETC2Enabled)
					{
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_COMPRESSED_RGB8_ETC2\" />");
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_COMPRESSED_RGBA8_ETC2_EAC\" />");
					}
					if (bATCEnabled)
					{
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_AMD_compressed_ATC_texture\" />");
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_ATI_texture_compression_atitc\" />");
					}
					if (bDXTEnabled)
					{
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_EXT_texture_compression_dxt1\" />");
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_EXT_texture_compression_s3tc\" />");
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_NV_texture_compression_s3tc\" />");
					}
					if (bPVRTCEnabled)
					{
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_IMG_texture_compression_pvrtc\" />");
					}
					if (bASTCEnabled)
					{
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_KHR_texture_compression_astc_ldr\" />");
					}
				}
			}

			Text.AppendLine("</manifest>");

			// allow plugins to modify final manifest HERE
			XDocument XDoc;
			try
			{
				XDoc = XDocument.Parse(Text.ToString());
			}
			catch (Exception e)
			{
				throw new BuildException("AndroidManifest.xml is invalid {0}\n{1}", e, Text.ToString());
			}

			UPL.ProcessPluginNode(Arch, "androidManifestUpdates", "", ref XDoc);
			return XDoc.ToString();
		}

		private string GenerateProguard(string Arch, string EngineSourcePath, string GameBuildFilesPath)
		{
			StringBuilder Text = new StringBuilder();

			string ProguardFile = Path.Combine(EngineSourcePath, "proguard-project.txt");
			if (File.Exists(ProguardFile))
			{
				foreach (string Line in File.ReadAllLines(ProguardFile))
				{
					Text.AppendLine(Line);
				}
			}

			string ProguardAdditionsFile = Path.Combine(GameBuildFilesPath, "ProguardAdditions.txt");
			if (File.Exists(ProguardAdditionsFile))
			{
				foreach (string Line in File.ReadAllLines(ProguardAdditionsFile))
				{
					Text.AppendLine(Line);
				}
			}

			// add plugin additions
			return UPL.ProcessPluginNode(Arch, "proguardAdditions", Text.ToString());
		}

		private void ValidateGooglePlay(string UE4BuildPath)
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			bool bEnableGooglePlaySupport;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableGooglePlaySupport", out bEnableGooglePlaySupport);

			if (!bEnableGooglePlaySupport)
			{
				// do not need to do anything; it is fine
				return;
			}

			string IniAppId;
			bool bInvalidIniAppId = false;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "GamesAppID", out IniAppId);

			//validate the value found in the AndroidRuntimeSettings
			Int64 Value;
			if (IniAppId.Length == 0 || !Int64.TryParse(IniAppId, out Value))
			{
				bInvalidIniAppId = true;
			}

			bool bInvalid = false;
			string ReplacementId = "";
			String Filename = Path.Combine(UE4BuildPath, "res", "values", "GooglePlayAppID.xml");
			if (File.Exists(Filename))
			{
				string[] FileContent = File.ReadAllLines(Filename);
				int LineIndex = -1;
				foreach (string Line in FileContent)
				{
					++LineIndex;

					int StartIndex = Line.IndexOf("\"app_id\">");
					if (StartIndex < 0)
						continue;

					StartIndex += 9;
					int EndIndex = Line.IndexOf("</string>");
					if (EndIndex < 0)
						continue;

					string XmlAppId = Line.Substring(StartIndex, EndIndex - StartIndex);

					//validate that the AppId matches the .ini value for the GooglePlay AppId, assuming it's valid
					if (!bInvalidIniAppId &&  IniAppId.CompareTo(XmlAppId) != 0)
					{
						Log.TraceInformation("Replacing Google Play AppID in GooglePlayAppID.xml with AndroidRuntimeSettings .ini value");

						bInvalid = true;
						ReplacementId = IniAppId;
						
					}					
					else if(XmlAppId.Length == 0 || !Int64.TryParse(XmlAppId, out Value))
					{
						Log.TraceWarning("\nWARNING: GooglePlay Games App ID is invalid! Replacing it with \"1\"");

						//write file with something which will fail but not cause an exception if executed
						bInvalid = true;
						ReplacementId = "1";
					}	

					if(bInvalid)
					{
						// remove any read only flags if invalid so it can be replaced
						FileInfo DestFileInfo = new FileInfo(Filename);
						DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;

						//preserve the rest of the file, just fix up this line
						string NewLine = Line.Replace("\"app_id\">" + XmlAppId + "</string>", "\"app_id\">" + ReplacementId + "</string>");
						FileContent[LineIndex] = NewLine;

						File.WriteAllLines(Filename, FileContent);
					}

					break;
				}
			}
			else
			{
				string NewAppId;
				// if we don't have an appID to use from the config, write file with something which will fail but not cause an exception if executed
				if (bInvalidIniAppId)
				{
					Log.TraceWarning("\nWARNING: Creating GooglePlayAppID.xml using a Google Play AppID of \"1\" because there was no valid AppID in AndroidRuntimeSettings!");
					NewAppId = "1";
				}
				else
				{
					Log.TraceInformation("Creating GooglePlayAppID.xml with AndroidRuntimeSettings .ini value");
					NewAppId = IniAppId;
				}

				File.WriteAllText(Filename, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources>\n\t<string name=\"app_id\">" + NewAppId + "</string>\n</resources>\n");
			}
		}

		private bool FilesAreDifferent(string SourceFilename, string DestFilename)
		{
			// source must exist
			FileInfo SourceInfo = new FileInfo(SourceFilename);
			if (!SourceInfo.Exists)
			{
				throw new BuildException("Can't make an APK without file [{0}]", SourceFilename);
			}

			// different if destination doesn't exist
			FileInfo DestInfo = new FileInfo(DestFilename);
			if (!DestInfo.Exists)
			{
				return true;
			}

			// file lengths differ?
			if (SourceInfo.Length != DestInfo.Length)
			{
				return true;
			}

			// validate timestamps
			TimeSpan Diff = DestInfo.LastWriteTimeUtc - SourceInfo.LastWriteTimeUtc;
			if (Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
			{
				return true;
			}

			// could check actual bytes just to be sure, but good enough
			return false;
		}

        private bool RequiresOBB(bool bDisallowPackageInAPK, string OBBLocation)
        {
            if (bDisallowPackageInAPK)
            {
                Log.TraceInformation("APK contains data.");
                return false;
            }
            else if (!String.IsNullOrEmpty(Environment.GetEnvironmentVariable("uebp_LOCAL_ROOT")))
            {
                Log.TraceInformation("On build machine.");
                return true;
            }
            else
            {
                Log.TraceInformation("Looking for OBB.");
                return File.Exists(OBBLocation);
            }
        }

		private void PatchAntBatIfNeeded()
		{
			// only need to do this for Windows (other platforms are Mono so use that for check)
			if (Utils.IsRunningOnMono)
			{
				return;
			}

			string AntBinPath = Environment.ExpandEnvironmentVariables("%ANT_HOME%/bin");
			string AntBatFilename = Path.Combine(AntBinPath, "ant.bat");
			string AntOrigBatFilename = Path.Combine(AntBinPath, "ant.orig.bat");

			// check for an unused drive letter
			string UnusedDriveLetter = "";
			bool bFound = true;
			DriveInfo[] AllDrives = DriveInfo.GetDrives();
			for (char DriveLetter = 'Z'; DriveLetter >= 'A'; DriveLetter--)
			{
				UnusedDriveLetter = Char.ToString(DriveLetter) + ":";
				bFound = false;
				for (int DriveIndex = AllDrives.Length-1; DriveIndex >= 0; DriveIndex--)
				{
					if (AllDrives[DriveIndex].Name.ToUpper().StartsWith(UnusedDriveLetter))
					{
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					break;
				}
			}

			if (bFound)
			{
				Log.TraceInformation("\nUnable to apply fixed ant.bat (all drive letters in use!)");
				return;
			}

			Log.TraceInformation("\nPatching ant.bat to work around commandline length limit (using unused drive letter {0})", UnusedDriveLetter);

			if (!File.Exists(AntOrigBatFilename))
			{
				// copy the existing ant.bat to ant.orig.bat
				File.Copy(AntBatFilename, AntOrigBatFilename, true);
			}

			// make sure ant.bat isn't read-only
			FileAttributes Attribs = File.GetAttributes(AntBatFilename);
			if (Attribs.HasFlag(FileAttributes.ReadOnly))
			{
				File.SetAttributes(AntBatFilename, Attribs & ~FileAttributes.ReadOnly);
			}

			// generate new ant.bat with an unused drive letter for subst
			string AntBatText =
					"@echo off\n" +
					"setlocal\n" +
					"set ANTPATH=%~dp0\n" +
					"set ANT_CMD_LINE_ARGS=\n" +
					":setupArgs\n" +
					"if \"\"%1\"\"==\"\"\"\" goto doneStart\n" +
					"set ANT_CMD_LINE_ARGS=%ANT_CMD_LINE_ARGS% %1\n" +
					"shift\n" +
					"goto setupArgs\n\n" +
					":doneStart\n" +
					"subst " + UnusedDriveLetter + " \"%CD%\"\n" +
					"pushd " + UnusedDriveLetter + "\n" +
					"call \"%ANTPATH%\\ant.orig.bat\" %ANT_CMD_LINE_ARGS%\n" +
					"set ANTERROR=%ERRORLEVEL%\n" +
					"popd\n" +
					"subst " + UnusedDriveLetter + " /d\n" +
					"exit /b %ANTERROR%\n";

			File.WriteAllText(AntBatFilename, AntBatText);
		}

		private bool CreateRunGradle(string GradlePath)
		{
			string RunGradleBatFilename = Path.Combine(GradlePath, "rungradle.bat");

			// check for an unused drive letter
			string UnusedDriveLetter = "";
			bool bFound = true;
			DriveInfo[] AllDrives = DriveInfo.GetDrives();
			for (char DriveLetter = 'Z'; DriveLetter >= 'A'; DriveLetter--)
			{
				UnusedDriveLetter = Char.ToString(DriveLetter) + ":";
				bFound = false;
				for (int DriveIndex = AllDrives.Length - 1; DriveIndex >= 0; DriveIndex--)
				{
					if (AllDrives[DriveIndex].Name.ToUpper().StartsWith(UnusedDriveLetter))
					{
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					break;
				}
			}

			if (bFound)
			{
				Log.TraceInformation("\nUnable to apply subst, using gradlew.bat directly (all drive letters in use!)");
				return false;
			}

			Log.TraceInformation("\nCreating rungradle.bat to work around commandline length limit (using unused drive letter {0})", UnusedDriveLetter);

			// make sure rungradle.bat isn't read-only
			if (File.Exists(RunGradleBatFilename))
			{
				FileAttributes Attribs = File.GetAttributes(RunGradleBatFilename);
				if (Attribs.HasFlag(FileAttributes.ReadOnly))
				{
					File.SetAttributes(RunGradleBatFilename, Attribs & ~FileAttributes.ReadOnly);
				}
			}

			// generate new ant.bat with an unused drive letter for subst
			string RunGradleBatText =
					"@echo off\n" +
					"setlocal\n" +
					"set GRADLEPATH=%~dp0\n" +
					"set GRADLE_CMD_LINE_ARGS=\n" +
					":setupArgs\n" +
					"if \"\"%1\"\"==\"\"\"\" goto doneStart\n" +
					"set GRADLE_CMD_LINE_ARGS=%GRADLE_CMD_LINE_ARGS% %1\n" +
					"shift\n" +
					"goto setupArgs\n\n" +
					":doneStart\n" +
					"subst " + UnusedDriveLetter + " \"%CD%\"\n" +
					"pushd " + UnusedDriveLetter + "\n" +
					"call \"%GRADLEPATH%\\gradlew.bat\" %GRADLE_CMD_LINE_ARGS%\n" +
					"set GRADLEERROR=%ERRORLEVEL%\n" +
					"popd\n" +
					"subst " + UnusedDriveLetter + " /d\n" +
					"exit /b %GRADLEERROR%\n";

			File.WriteAllText(RunGradleBatFilename, RunGradleBatText);

			return true;
		}

		private bool GradleEnabled()
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			bool bEnableGradle = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableGradle", out bEnableGradle);
			return bEnableGradle;
		}

		private bool IsLicenseAgreementValid()
		{
			string LicensePath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/licenses");

			// directory must exist
			if (!Directory.Exists(LicensePath))
			{
				Log.TraceInformation("Directory doesn't exist {0}", LicensePath);
				return false;
			}

			// license file must exist
			string LicenseFilename = Path.Combine(LicensePath, "android-sdk-license");
			if (!File.Exists(LicenseFilename))
			{
				Log.TraceInformation("File doesn't exist {0}", LicenseFilename);
				return false;
			}

			// ignore contents of hash for now (Gradle will report if it isn't valid)
			return true;
		}

		private void MakeApk(AndroidToolChain ToolChain, string ProjectName, string ProjectDirectory, string OutputPath, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bMakeSeparateApks, bool bIncrementalPackage, bool bDisallowPackagingDataInApk, bool bDisallowExternalFilesDir)
		{
			Log.TraceInformation("\n===={0}====PREPARING TO MAKE APK=================================================================", DateTime.Now.ToString());

			SetMinimumSDKLevelForGradle();

			// check for Gradle enabled for this project
			bGradleEnabled = GradleEnabled();

			if (bGradleEnabled)
			{
				if (!IsLicenseAgreementValid())
				{
					throw new BuildException("Android SDK license file not found.  Please agree to license in Android project settings in the editor.");
				}
			}

			// do this here so we'll stop early if there is a problem with the SDK API level (cached so later calls will return the same)
			string SDKAPILevel = GetSdkApiLevel(ToolChain);
			int SDKLevelInt = GetApiLevelInt(SDKAPILevel);
			string BuildToolsVersion = GetBuildToolsVersion(bGradleEnabled);

			if (!bGradleEnabled)
			{
				PatchAntBatIfNeeded();
			}

			// make sure it is cached
			string EngineVersion = ReadEngineVersion(EngineDirectory);

			// cache some tools paths
			string NDKBuildPath = Environment.ExpandEnvironmentVariables("%NDKROOT%/ndk-build" + (Utils.IsRunningOnMono ? "" : ".cmd"));

			// set up some directory info
			string IntermediateAndroidPath = Path.Combine(ProjectDirectory, "Intermediate/Android/");
			string UE4BuildPath = Path.Combine(IntermediateAndroidPath, "APK");
			string UE4JavaFilePath = Path.Combine(ProjectDirectory, "Build", "Android", GetUE4JavaSrcPath());
			string UE4BuildFilesPath = GetUE4BuildFilePath(EngineDirectory);
			string GameBuildFilesPath = Path.Combine(ProjectDirectory, "Build/Android");

			// Generate Java files
			string PackageName = GetPackageName(ProjectName);
			string TemplateDestinationBase = Path.Combine(ProjectDirectory, "Build", "Android", "src", PackageName.Replace('.', Path.DirectorySeparatorChar));
			MakeDirectoryIfRequired(TemplateDestinationBase);

			// We'll be writing the OBB data into the same location as the download service files
			string UE4OBBDataFileName = GetUE4JavaOBBDataFileName(TemplateDestinationBase);
			string UE4DownloadShimFileName = GetUE4JavaDownloadShimFileName(UE4JavaFilePath);

			// Template generated files           
			string JavaTemplateSourceDir = GetUE4TemplateJavaSourceDir(EngineDirectory);
			var templates = from template in Directory.EnumerateFiles(JavaTemplateSourceDir, "*.template")
							let RealName = Path.GetFileNameWithoutExtension(template)
							select new TemplateFile { SourceFile = template, DestinationFile = GetUE4TemplateJavaDestination(TemplateDestinationBase, RealName) };

			// Generate the OBB and Shim files here
			string ObbFileLocation = ProjectDirectory + "/Saved/StagedBuilds/Android" + CookFlavor + ".obb";

			// This is kind of a small hack to get around a rewrite problem
			// We need to make sure the file is there but if the OBB file doesn't exist then we don't want to replace it
			if (File.Exists(ObbFileLocation) || !File.Exists(UE4OBBDataFileName))
			{
				WriteJavaOBBDataFile(UE4OBBDataFileName, PackageName, new List<string> { ObbFileLocation });
			}

			// Make sure any existing proguard file in project is NOT used (back it up)
			string ProjectBuildProguardFile = Path.Combine(GameBuildFilesPath, "proguard-project.txt");
			if (File.Exists(ProjectBuildProguardFile))
			{
				string ProjectBackupProguardFile = Path.Combine(GameBuildFilesPath, "proguard-project.backup");
				File.Move(ProjectBuildProguardFile, ProjectBackupProguardFile);
			}

			WriteJavaDownloadSupportFiles(UE4DownloadShimFileName, templates, new Dictionary<string, string>{
                { "$$GameName$$", ProjectName },
                { "$$PublicKey$$", GetPublicKey() }, 
                { "$$PackageName$$",PackageName }
            });

			// Sometimes old files get left behind if things change, so we'll do a clean up pass
			{
				string CleanUpBaseDir = Path.Combine(ProjectDirectory, "Build", "Android", "src");
				string ImmediateBaseDir = Path.Combine(UE4BuildPath, "src");
				var files = Directory.EnumerateFiles(CleanUpBaseDir, "*.java", SearchOption.AllDirectories);

				Log.TraceInformation("Cleaning up files based on template dir {0}", TemplateDestinationBase);

				// Make a set of files that are okay to clean up
				var cleanFiles = new HashSet<string>();
				cleanFiles.Add("OBBData.java");
				foreach (var template in templates)
				{
					cleanFiles.Add(Path.GetFileName(template.DestinationFile));
				}

				foreach (var filename in files)
				{
					if (filename == UE4DownloadShimFileName)  // we always need the shim, and it'll get rewritten if needed anyway
						continue;

					string filePath = Path.GetDirectoryName(filename);  // grab the file's path
					if (filePath != TemplateDestinationBase)             // and check to make sure it isn't the same as the Template directory we calculated earlier
					{
						// Only delete the files in the cleanup set
						if (!cleanFiles.Contains(Path.GetFileName(filename)))
							continue;

						Log.TraceInformation("Cleaning up file {0}", filename);
						FileAttributes Attribs = File.GetAttributes(filename);
						File.SetAttributes(filename, Attribs & ~FileAttributes.ReadOnly);
						File.Delete(filename);

						// Check to see if this file also exists in our target destination, and if so nuke it too
						string DestFilename = Path.Combine(ImmediateBaseDir, Utils.MakePathRelativeTo(filename, CleanUpBaseDir));
						if (File.Exists(DestFilename))
						{
							Log.TraceInformation("Cleaning up file {0}", DestFilename);
							Attribs = File.GetAttributes(DestFilename);
							File.SetAttributes(DestFilename, Attribs & ~FileAttributes.ReadOnly);
							File.Delete(DestFilename);
						}
					}
				}

				// Directory clean up code (Build/Android/src)
				try
				{
					var BaseDirectories = Directory.EnumerateDirectories(CleanUpBaseDir, "*", SearchOption.AllDirectories).OrderByDescending(x => x);
					foreach (var directory in BaseDirectories)
					{
						if (Directory.Exists(directory) && Directory.GetFiles(directory, "*.*", SearchOption.AllDirectories).Count() == 0)
						{
							Log.TraceInformation("Cleaning Directory {0} as empty.", directory);
							Directory.Delete(directory, true);
						}
					}
				}
				catch (Exception)
				{
					// likely System.IO.DirectoryNotFoundException, ignore it
				}

				// Directory clean up code (Intermediate/APK/src)
				try
				{
					var ImmediateDirectories = Directory.EnumerateDirectories(ImmediateBaseDir, "*", SearchOption.AllDirectories).OrderByDescending(x => x);
					foreach (var directory in ImmediateDirectories)
					{
						if (Directory.Exists(directory) && Directory.GetFiles(directory, "*.*", SearchOption.AllDirectories).Count() == 0)
						{
							Log.TraceInformation("Cleaning Directory {0} as empty.", directory);
							Directory.Delete(directory, true);
						}
					}
				}
				catch (Exception)
				{
					// likely System.IO.DirectoryNotFoundException, ignore it
				}
			}


			// cache if we want data in the Apk
			bool bPackageDataInsideApk = PackageDataInsideApk(bDisallowPackagingDataInApk);
			bool bDisableVerifyOBBOnStartUp = DisableVerifyOBBOnStartUp();
			bool bUseExternalFilesDir = UseExternalFilesDir(bDisallowExternalFilesDir);

			// check to see if any "meta information" is newer than last time we build
			string CurrentBuildSettings = GetAllBuildSettings(ToolChain, UE4BuildPath, bForDistribution, bMakeSeparateApks, bPackageDataInsideApk, bDisableVerifyOBBOnStartUp, bUseExternalFilesDir);
			string BuildSettingsCacheFile = Path.Combine(UE4BuildPath, "UEBuildSettings.txt");

			// do we match previous build settings?
			bool bBuildSettingsMatch = true;

			// get application name and whether it changed, needing to force repackage
			string ApplicationDisplayName;
			if (CheckApplicationName(UE4BuildPath, ProjectName, out ApplicationDisplayName))
			{
				bBuildSettingsMatch = false;
				Log.TraceInformation("Application display name is different than last build, forcing repackage.");
			}

			// if the manifest matches, look at other settings stored in a file
			if (bBuildSettingsMatch)
			{
				if (File.Exists(BuildSettingsCacheFile))
				{
					string PreviousBuildSettings = File.ReadAllText(BuildSettingsCacheFile);
					if (PreviousBuildSettings != CurrentBuildSettings)
					{
						bBuildSettingsMatch = false;
						Log.TraceInformation("Previous .apk file(s) were made with different build settings, forcing repackage.");
					}
				}
			}

			// only check input dependencies if the build settings already match
			if (bBuildSettingsMatch)
			{
				// check if so's are up to date against various inputs
				var JavaFiles = new List<string>{
                                                    UE4OBBDataFileName,
                                                    UE4DownloadShimFileName
                                                };
				// Add the generated files too
				JavaFiles.AddRange(from t in templates select t.SourceFile);
				JavaFiles.AddRange(from t in templates select t.DestinationFile);

				bBuildSettingsMatch = CheckDependencies(ToolChain, ProjectName, ProjectDirectory, UE4BuildFilesPath, GameBuildFilesPath,
					EngineDirectory, JavaFiles, CookFlavor, OutputPath, UE4BuildPath, bMakeSeparateApks, bPackageDataInsideApk);

			}

			var Arches = ToolChain.GetAllArchitectures();
			var GPUArchitectures = ToolChain.GetAllGPUArchitectures();

			// figure out the configuration from output filename
			string Configuration = "Development";
			string OutputConfig = Path.GetFileNameWithoutExtension(OutputPath);
			if (OutputConfig.EndsWith("-Debug"))
			{
				Configuration = "Debug";
			}
			else if (OutputConfig.EndsWith("-Test"))
			{
				Configuration = "Test";
			}
			else if (OutputConfig.EndsWith("-DebugGame"))
			{
				Configuration = "Debug";
			}
			else if (OutputConfig.EndsWith("-Shipping"))
			{
				Configuration = "Shipping";
			}

			// Initialize UPL contexts for each architecture enabled
			List<string> NDKArches = new List<string>();
			foreach (var Arch in Arches)
			{
				string NDKArch = GetNDKArch(Arch);
				if (!NDKArches.Contains(NDKArch))
				{
					NDKArches.Add(NDKArch);
				}
			}
			UPL.Init(NDKArches, bForDistribution, EngineDirectory, UE4BuildPath, ProjectDirectory, Configuration);

			IEnumerable<Tuple<string, string, string>> BuildList = null;

			if (!bBuildSettingsMatch)
			{
				BuildList = from Arch in Arches
							from GPUArch in GPUArchitectures
							let manifest = GenerateManifest(ToolChain, ProjectName, EngineDirectory, bForDistribution, bPackageDataInsideApk, GameBuildFilesPath, RequiresOBB(bDisallowPackagingDataInApk, ObbFileLocation), bDisableVerifyOBBOnStartUp, Arch, GPUArch, CookFlavor, bUseExternalFilesDir, Configuration, SDKLevelInt)
							select Tuple.Create(Arch, GPUArch, manifest);
			}
			else
			{
				BuildList = from Arch in Arches
							from GPUArch in GPUArchitectures
							let manifestFile = Path.Combine(IntermediateAndroidPath, Arch + "_" + GPUArch + "_AndroidManifest.xml")
							let manifest = GenerateManifest(ToolChain, ProjectName, EngineDirectory, bForDistribution, bPackageDataInsideApk, GameBuildFilesPath, RequiresOBB(bDisallowPackagingDataInApk, ObbFileLocation), bDisableVerifyOBBOnStartUp, Arch, GPUArch, CookFlavor, bUseExternalFilesDir, Configuration, SDKLevelInt)
							let OldManifest = File.Exists(manifestFile) ? File.ReadAllText(manifestFile) : ""
							where manifest != OldManifest
							select Tuple.Create(Arch, GPUArch, manifest);
			}

			// Now we have to spin over all the arch/gpu combinations to make sure they all match
			int BuildListComboTotal = BuildList.Count();
			if (BuildListComboTotal == 0)
			{
				Log.TraceInformation("Output .apk file(s) are up to date (dependencies and build settings are up to date)");
				return;
			}


			// Once for all arches code:

			// make up a dictionary of strings to replace in xml files (strings.xml)
			Dictionary<string, string> Replacements = new Dictionary<string, string>();
			Replacements.Add("${EXECUTABLE_NAME}", ApplicationDisplayName);

			if (!bIncrementalPackage)
			{
				// Wipe the Intermediate/Build/APK directory first, except for dexedLibs, because Google Services takes FOREVER to predex, and it almost never changes
				// so allow the ANT checking to win here - if this grows a bit with extra libs, it's fine, it _should_ only pull in dexedLibs it needs
				Log.TraceInformation("Performing complete package - wiping {0}, except for predexedLibs", UE4BuildPath);
				DeleteDirectory(UE4BuildPath, "dexedLibs");
			}

			// If we are packaging for Amazon then we need to copy the  file to the correct location
			Log.TraceInformation("bPackageDataInsideApk = {0}", bPackageDataInsideApk);
			if (bPackageDataInsideApk)
			{
				Console.WriteLine("Obb location {0}", ObbFileLocation);
				string ObbFileDestination = UE4BuildPath + "/assets";
				Console.WriteLine("Obb destination location {0}", ObbFileDestination);
				if (File.Exists(ObbFileLocation))
				{
					Directory.CreateDirectory(UE4BuildPath);
					Directory.CreateDirectory(ObbFileDestination);
					Console.WriteLine("Obb file exists...");
					var DestFileName = Path.Combine(ObbFileDestination, "main.obb.png"); // Need a rename to turn off compression
					var SrcFileName = ObbFileLocation;
					if (!File.Exists(DestFileName) || File.GetLastWriteTimeUtc(DestFileName) < File.GetLastWriteTimeUtc(SrcFileName))
					{
						Console.WriteLine("Copying {0} to {1}", SrcFileName, DestFileName);
						File.Copy(SrcFileName, DestFileName);
					}
				}
			}
			else // try to remove the file it we aren't packaging inside the APK
			{
				string ObbFileDestination = UE4BuildPath + "/assets";
				var DestFileName = Path.Combine(ObbFileDestination, "main.obb.png");
				if (File.Exists(DestFileName))
				{
					File.Delete(DestFileName);
				}
			}

			string AARExtractListFilename = Path.Combine(UE4BuildPath, "JavaLibs", "AARExtractList.txt");
			if (bGradleEnabled)
			{
				//Need to clear out JavaLibs if last run was with Ant
				if (File.Exists(AARExtractListFilename))
				{
					Console.WriteLine("Cleanup up JavaLibs from previous Ant packaging");
					DeleteDirectory(Path.Combine(UE4BuildPath, "JavaLibs"));
				}
			}

			//Copy build files to the intermediate folder in this order (later overrides earlier):
			//	- Shared Engine
			//  - Shared Engine NoRedist (for Epic secret files)
			//  - Game
			//  - Game NoRedist (for Epic secret files)
			CopyFileDirectory(UE4BuildFilesPath, UE4BuildPath, Replacements);
			CopyFileDirectory(UE4BuildFilesPath + "/NotForLicensees", UE4BuildPath, Replacements);
			CopyFileDirectory(UE4BuildFilesPath + "/NoRedist", UE4BuildPath, Replacements);
			CopyFileDirectory(GameBuildFilesPath, UE4BuildPath, Replacements);
			CopyFileDirectory(GameBuildFilesPath + "/NotForLicensees", UE4BuildPath, Replacements);
			CopyFileDirectory(GameBuildFilesPath + "/NoRedist", UE4BuildPath, Replacements);

			// Write Crashlytics data if enabled
			if (CrashlyticsPluginEnabled)
			{
				Trace.TraceInformation("Writing Crashlytics resources");
				WriteCrashlyticsResources(Path.Combine(ProjectDirectory, "Build", "Android"), PackageName, ApplicationDisplayName);
			}

			if (!bGradleEnabled)
			{
				//Extract AAR and Jar files with dependencies if not using Gradle
				ExtractAARAndJARFiles(EngineDirectory, UE4BuildPath, NDKArches, PackageName, AARExtractListFilename);
			}
			else
			{
				//Generate Gradle AAR dependencies
				GenerateGradleAARImports(EngineDirectory, UE4BuildPath, NDKArches);
			}

			//Now validate GooglePlay app_id if enabled
			ValidateGooglePlay(UE4BuildPath);

			//determine which orientation requirements this app has
			bool bNeedLandscape = false;
			bool bNeedPortrait = false;
			DetermineScreenOrientationRequirements(out bNeedPortrait, out bNeedLandscape);

			//Now keep the splash screen images matching orientation requested
			PickSplashScreenOrientation(UE4BuildPath, bNeedPortrait, bNeedLandscape);
			
			//Now package the app based on Daydream packaging settings 
			PackageForDaydream(UE4BuildPath);
			
			//Similarly, keep only the downloader screen image matching the orientation requested
			PickDownloaderScreenOrientation(UE4BuildPath, bNeedPortrait, bNeedLandscape);

			// at this point, we can write out the cached build settings to compare for a next build
			File.WriteAllText(BuildSettingsCacheFile, CurrentBuildSettings);

			// at this point, we can write out the cached build settings to compare for a next build
			File.WriteAllText(BuildSettingsCacheFile, CurrentBuildSettings);

			///////////////
			// in case the game had an AndroidManifest.xml file, we overwrite it now with the generated one
			//File.WriteAllText(ManifestFile, NewManifest);
			///////////////

			Log.TraceInformation("\n===={0}====PREPARING NATIVE CODE=================================================================", DateTime.Now.ToString());
			bool HasNDKPath = File.Exists(NDKBuildPath);

			// get Ant verbosity level
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			string AntVerbosity;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "AntVerbosity", out AntVerbosity);

			foreach (var build in BuildList)
			{
				string Arch = build.Item1;
				string GPUArchitecture = build.Item2;
				string Manifest = build.Item3;
				string NDKArch = GetNDKArch(Arch);

				string SourceSOName = AndroidToolChain.InlineArchName(OutputPath, Arch, GPUArchitecture);
				// if the source binary was UE4Game, replace it with the new project name, when re-packaging a binary only build
				string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UE4Game", ProjectName);
				string DestApkName = Path.Combine(ProjectDirectory, "Binaries/Android/") + ApkFilename + ".apk";

				// As we are always making seperate APKs we need to put the architecture into the name
				DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch, GPUArchitecture);

				// Write the manifest to the correct locations (cache and real)
				String ManifestFile = Path.Combine(IntermediateAndroidPath, Arch + "_" + GPUArchitecture + "_AndroidManifest.xml");
				File.WriteAllText(ManifestFile, Manifest);
				ManifestFile = Path.Combine(UE4BuildPath, "AndroidManifest.xml");
				File.WriteAllText(ManifestFile, Manifest);

				// copy prebuild plugin files
				UPL.ProcessPluginNode(NDKArch, "prebuildCopies", "");

				if (!bGradleEnabled)
				{
					// update metadata files (like project.properties, build.xml) if we are missing a build.xml or if we just overwrote project.properties with a bad version in it (from game/engine dir)
					UpdateProjectProperties(ToolChain, UE4BuildPath, ProjectName);

					// modify the generated build.xml before the final include
					UpdateBuildXML(Arch, NDKArch, EngineDirectory, UE4BuildPath);
				}

				// update GameActivity.java if out of date
				UpdateGameActivity(Arch, NDKArch, EngineDirectory, UE4BuildPath);

				// Copy the generated .so file from the binaries directory to the jni folder
				if (!File.Exists(SourceSOName))
				{
					throw new BuildException("Can't make an APK without the compiled .so [{0}]", SourceSOName);
				}
				if (!Directory.Exists(UE4BuildPath + "/jni"))
				{
					throw new BuildException("Can't make an APK without the jni directory [{0}/jni]", UE4BuildFilesPath);
				}

				String FinalSOName;

				if (HasNDKPath)
				{
					string LibDir = UE4BuildPath + "/jni/" + NDKArch;
					FinalSOName = LibDir + "/libUE4.so";

					// check to see if libUE4.so needs to be copied
					if (BuildListComboTotal > 1 || FilesAreDifferent(SourceSOName, FinalSOName))
					{
						Log.TraceInformation("\nCopying new .so {0} file to jni folder...", SourceSOName);
						Directory.CreateDirectory(LibDir);
						// copy the binary to the standard .so location
						File.Copy(SourceSOName, FinalSOName, true);
					}
				}
				else
				{
					// if no NDK, we don't need any of the debugger stuff, so we just copy the .so to where it will end up
					FinalSOName = UE4BuildPath + "/libs/" + NDKArch + "/libUE4.so";

					// check to see if libUE4.so needs to be copied
					if (BuildListComboTotal > 1 || FilesAreDifferent(SourceSOName, FinalSOName))
					{
						Log.TraceInformation("\nCopying .so {0} file to jni folder...", SourceSOName);
						Directory.CreateDirectory(Path.GetDirectoryName(FinalSOName));
						File.Copy(SourceSOName, FinalSOName, true);
					}
				}

				// remove any read only flags
				FileInfo DestFileInfo = new FileInfo(FinalSOName);
				DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
				File.SetLastWriteTimeUtc(FinalSOName, File.GetLastWriteTimeUtc(SourceSOName));

				// if we need to run ndk-build, do it now
				if (HasNDKPath)
				{
					string LibSOName = UE4BuildPath + "/libs/" + NDKArch + "/libUE4.so";
					// always delete libs up to this point so fat binaries and incremental builds work together (otherwise we might end up with multiple
					// so files in an apk that doesn't want them)
					// note that we don't want to delete all libs, just the ones we copied
					TimeSpan Diff = File.GetLastWriteTimeUtc(LibSOName) - File.GetLastWriteTimeUtc(FinalSOName);
					if (!File.Exists(LibSOName) || Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
					{
						foreach (string Lib in Directory.EnumerateFiles(UE4BuildPath + "/libs", "libUE4*.so", SearchOption.AllDirectories))
						{
							File.Delete(Lib);
						}

						string CommandLine = "APP_ABI=\"" + NDKArch + " " + "\"";
						if (!bForDistribution)
						{
							CommandLine += " NDK_DEBUG=1";
						}
						RunCommandLineProgramWithException(UE4BuildPath, NDKBuildPath, CommandLine, "Preparing native code for debugging...", true);

						File.SetLastWriteTimeUtc(LibSOName, File.GetLastWriteTimeUtc(FinalSOName));
					}
				}

				// after ndk-build is called, we can now copy in the stl .so (ndk-build deletes old files)
				// copy libgnustl_shared.so to library (use 4.8 if possible, otherwise 4.6)
				CopySTL(ToolChain, UE4BuildPath, Arch, NDKArch, bForDistribution);
				CopyGfxDebugger(UE4BuildPath, Arch, NDKArch);
				CopyVulkanValidationLayers(UE4BuildPath, Arch, NDKArch, Configuration);

				// copy postbuild plugin files
				UPL.ProcessPluginNode(NDKArch, "resourceCopies", "");

				Log.TraceInformation("\n===={0}====PERFORMING FINAL APK PACKAGE OPERATION================================================", DateTime.Now.ToString());

				if (!bGradleEnabled)
				{
					// use Ant for compile/package
					string AntBuildType = "debug";
					string AntOutputSuffix = "-debug";
					if (bForDistribution)
					{
						// Generate the Proguard file contents and write it
						string ProguardContents = GenerateProguard(NDKArch, UE4BuildFilesPath, GameBuildFilesPath);
						string ProguardFilename = Path.Combine(UE4BuildPath, "proguard-project.txt");
						if (File.Exists(ProguardFilename))
						{
							File.Delete(ProguardFilename);
						}
						File.WriteAllText(ProguardFilename, ProguardContents);

						// this will write out ant.properties with info needed to sign a distribution build
						PrepareToSignApk(UE4BuildPath);
						AntBuildType = "release";
						AntOutputSuffix = "-release";
					}

					// Use ant to build the .apk file
					string AntOptions = AntBuildType + " -Djava.source=1.7 -Djava.target=1.7";
					string ShellExecutable = Utils.IsRunningOnMono ? "/bin/sh" : "cmd.exe";
					string ShellParametersBegin = Utils.IsRunningOnMono ? "-c '" : "/c ";
					string ShellParametersEnd = Utils.IsRunningOnMono ? "'" : "";
					switch (AntVerbosity.ToLower())
					{
						default:
						case "quiet":
							if (RunCommandLineProgramAndReturnResult(UE4BuildPath, ShellExecutable, ShellParametersBegin + "\"" + GetAntPath() + "\" -quiet " + AntOptions + ShellParametersEnd, "Making .apk with Ant... (note: it's safe to ignore javac obsolete warnings)") != 0)
							{
								RunCommandLineProgramWithException(UE4BuildPath, ShellExecutable, ShellParametersBegin + "\"" + GetAntPath() + "\" " + AntOptions + ShellParametersEnd, "Making .apk with Ant again to show errors");
							}
							break;

						case "normal":
							RunCommandLineProgramWithException(UE4BuildPath, ShellExecutable, ShellParametersBegin + "\"" + GetAntPath() + "\" " + AntOptions + ShellParametersEnd, "Making .apk with Ant again to show errors");
							break;

						case "verbose":
							RunCommandLineProgramWithException(UE4BuildPath, ShellExecutable, ShellParametersBegin + "\"" + GetAntPath() + "\" -verbose " + AntOptions + ShellParametersEnd, "Making .apk with Ant again to show errors");
							break;
					}

					// upload Crashlytics symbols if plugin enabled and using build machine
					if (CrashlyticsPluginEnabled && Environment.GetEnvironmentVariable("IsBuildMachine") == "1")
					{
						AntOptions = "crashlytics-upload-symbols";
						RunCommandLineProgramWithException(UE4BuildPath, ShellExecutable, ShellParametersBegin + "\"" + GetAntPath() + "\" " + AntOptions + ShellParametersEnd, "Uploading Crashlytics symbols");
					}

					// make sure destination exists
					Directory.CreateDirectory(Path.GetDirectoryName(DestApkName));

					// now copy to the final location
					File.Copy(UE4BuildPath + "/bin/" + ProjectName + AntOutputSuffix + ".apk", DestApkName, true);
				}
				else
				{
					// use Gradle for compile/package
					// ini file to get settings from
					string UE4BuildGradlePath = Path.Combine(UE4BuildPath, "gradle");
					string UE4BuildGradleAppPath = Path.Combine(UE4BuildGradlePath, "app");
					string UE4BuildGradleMainPath = Path.Combine(UE4BuildGradleAppPath, "src", "main");

					string CompileSDKVersion = SDKAPILevel.Replace("android-", "");

					// stage files into gradle app directory
					string GradleManifest = Path.Combine(UE4BuildGradleMainPath, "AndroidManifest.xml");
					MakeDirectoryIfRequired(GradleManifest);
					File.Copy(Path.Combine(UE4BuildPath, "AndroidManifest.xml"), GradleManifest, true);

					CleanCopyDirectory(Path.Combine(UE4BuildPath, "assets"), Path.Combine(UE4BuildGradleMainPath, "assets"));
					CleanCopyDirectory(Path.Combine(UE4BuildPath, "jni"), Path.Combine(UE4BuildGradleMainPath, "jni"));     // has debug symbols
					CleanCopyDirectory(Path.Combine(UE4BuildPath, "obj"), Path.Combine(UE4BuildGradleMainPath, "obj"));     // has debug symbols
					CleanCopyDirectory(Path.Combine(UE4BuildPath, "libs"), Path.Combine(UE4BuildGradleMainPath, "libs"));
					CleanCopyDirectory(Path.Combine(UE4BuildPath, "res"), Path.Combine(UE4BuildGradleMainPath, "res"));
					CleanCopyDirectory(Path.Combine(UE4BuildPath, "src"), Path.Combine(UE4BuildGradleMainPath, "java"));

					// move JavaLibs into subprojects
					string JavaLibsDir = Path.Combine(UE4BuildPath, "JavaLibs");
					PrepareJavaLibsForGradle(JavaLibsDir, UE4BuildGradlePath, CompileSDKVersion, BuildToolsVersion);

					// Create gradle.properties
					string GradlePropertiesFilename = Path.Combine(UE4BuildGradlePath, "gradle.properties");
					StringBuilder GradleProperties = new StringBuilder();

					int StoreVersion = GetStoreVersion();
					string VersionDisplayName;
					Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "VersionDisplayName", out VersionDisplayName);
					int MinSDKVersion;
					Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MinSDKVersion", out MinSDKVersion);
					int TargetSDKVersion = MinSDKVersion;
					Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "TargetSDKVersion", out TargetSDKVersion);

					// Make sure minSdkVersion is at least 13 (need this for appcompat-v13 used by AndroidPermissions)
					// this may be changed by active plugins (Google Play Services 11.0.4 needs 14 for example)
					if (MinSDKVersion < MinimumSDKLevelForGradle)
					{
						MinSDKVersion = MinimumSDKLevelForGradle;
					}
					if (TargetSDKVersion < MinSDKVersion)
					{
						TargetSDKVersion = MinSDKVersion;
					}

					// 64-bit targets must be android-21 or higher
					int NDKLevelInt = ToolChain.GetNdkApiLevelInt();
					if (NDKLevelInt < 21)
					{
						if (Arch == "-arm64" || Arch == "-x64")
						{
							NDKLevelInt = 21;
						}
					}

					// fix up the MinSdkVersion
					if (NDKLevelInt > 19)
					{
						if (MinSDKVersion < 21)
						{
							MinSDKVersion = 21;
							Log.TraceInformation("Fixing minSdkVersion; NDK level above 19 requires minSdkVersion of 21 (arch={0})", Arch.Substring(1));
						}
					}

					GradleProperties.AppendLine(string.Format("COMPILE_SDK_VERSION={0}", CompileSDKVersion));
					GradleProperties.AppendLine(string.Format("BUILD_TOOLS_VERSION={0}", BuildToolsVersion));
					GradleProperties.AppendLine(string.Format("PACKAGE_NAME={0}", PackageName));
					GradleProperties.AppendLine(string.Format("MIN_SDK_VERSION={0}", MinSDKVersion.ToString()));
					GradleProperties.AppendLine(string.Format("TARGET_SDK_VERSION={0}", TargetSDKVersion.ToString()));
					GradleProperties.AppendLine(string.Format("STORE_VERSION={0}", StoreVersion.ToString()));
					GradleProperties.AppendLine(string.Format("VERSION_DISPLAY_NAME={0}", VersionDisplayName));
					GradleProperties.AppendLine(string.Format("OUTPUT_PATH={0}", DestApkName.Replace("\\", "/")));

					File.WriteAllText(GradlePropertiesFilename, GradleProperties.ToString());

					StringBuilder GradleBuildAdditionsContent = new StringBuilder();
					GradleBuildAdditionsContent.AppendLine("apply from: 'aar-imports.gradle'");
					GradleBuildAdditionsContent.AppendLine("apply from: 'projects.gradle'");

					GradleBuildAdditionsContent.AppendLine("android {");
					GradleBuildAdditionsContent.AppendLine("\tdefaultConfig {");
					GradleBuildAdditionsContent.AppendLine("\t\tndk {");
					GradleBuildAdditionsContent.AppendLine(string.Format("\t\t\tabiFilter \"{0}\"", NDKArch));
					GradleBuildAdditionsContent.AppendLine("\t\t}");
					GradleBuildAdditionsContent.AppendLine("\t}");

					string GradleBuildType = ":app:assembleDebug";
					if (bForDistribution)
					{
						bool bDisableV2Signing = false;
						GradleBuildType = ":app:assembleRelease";

						if (IsPackagingForGearVR())
						{
							bDisableV2Signing = true;
							Log.TraceInformation("Disabling v2Signing for Gear VR APK");
						}

						string KeyAlias, KeyStore, KeyStorePassword, KeyPassword;
						Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStore", out KeyStore);
						Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyAlias", out KeyAlias);
						Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStorePassword", out KeyStorePassword);
						Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyPassword", out KeyPassword);

						if (string.IsNullOrEmpty(KeyStore) || string.IsNullOrEmpty(KeyAlias) || string.IsNullOrEmpty(KeyStorePassword))
						{
							throw new BuildException("DistributionSigning settings are not all set. Check the DistributionSettings section in the Android tab of Project Settings");
						}

						if (string.IsNullOrEmpty(KeyPassword) || KeyPassword == "_sameaskeystore_")
						{
							KeyPassword = KeyStorePassword;
						}

						// Make sure the keystore file exists
						string KeyStoreFilename = Path.Combine(UE4BuildPath, KeyStore);
						if (!File.Exists(KeyStoreFilename))
						{
							throw new BuildException("Keystore file is missing. Check the DistributionSettings section in the Android tab of Project Settings");
						}

						GradleBuildAdditionsContent.AppendLine("\tsigningConfigs {");
						GradleBuildAdditionsContent.AppendLine("\t\trelease {");
						GradleBuildAdditionsContent.AppendLine(string.Format("\t\t\tstoreFile file('{0}')", KeyStoreFilename.Replace("\\", "/")));
						GradleBuildAdditionsContent.AppendLine(string.Format("\t\t\tstorePassword '{0}'", KeyStorePassword));
						GradleBuildAdditionsContent.AppendLine(string.Format("\t\t\tkeyAlias '{0}'", KeyAlias));
						GradleBuildAdditionsContent.AppendLine(string.Format("\t\t\tkeyPassword '{0}'", KeyPassword));
						if (bDisableV2Signing)
						{
							GradleBuildAdditionsContent.AppendLine("\t\t\tv2SigningEnabled false");
						}
						GradleBuildAdditionsContent.AppendLine("\t\t}");
						GradleBuildAdditionsContent.AppendLine("\t}");

						// Generate the Proguard file contents and write it
						string ProguardContents = GenerateProguard(NDKArch, UE4BuildFilesPath, GameBuildFilesPath);
						string ProguardFilename = Path.Combine(UE4BuildGradleAppPath, "proguard-rules.pro");
						if (File.Exists(ProguardFilename))
						{
							File.Delete(ProguardFilename);
						}
						File.WriteAllText(ProguardFilename, ProguardContents);
					}
					else
					{
						// empty just for Gradle not to complain
						GradleBuildAdditionsContent.AppendLine("\tsigningConfigs {");
						GradleBuildAdditionsContent.AppendLine("\t\trelease {");
						GradleBuildAdditionsContent.AppendLine("\t\t}");
						GradleBuildAdditionsContent.AppendLine("\t}");
					}
					GradleBuildAdditionsContent.AppendLine("}");

					// Add any UPL app buildGradleAdditions
					GradleBuildAdditionsContent.Append(UPL.ProcessPluginNode(NDKArch, "buildGradleAdditions", ""));

					string GradleBuildAdditionsFilename = Path.Combine(UE4BuildGradleAppPath, "buildAdditions.gradle");
					File.WriteAllText(GradleBuildAdditionsFilename, GradleBuildAdditionsContent.ToString());

					// Create baseBuildAdditions.gradle from plugins baseBuildGradleAdditions
					string GradleBaseBuildAdditionsFilename = Path.Combine(UE4BuildGradlePath, "baseBuildAdditions.gradle");
					File.WriteAllText(GradleBaseBuildAdditionsFilename, UPL.ProcessPluginNode(NDKArch, "baseBuildGradleAdditions", ""));

					string GradleScriptPath = Path.Combine(UE4BuildGradlePath, "gradlew");
					if (Utils.IsRunningOnMono)
					{
						// fix permissions for Mac/Linux
						RunCommandLineProgramWithException(UE4BuildGradlePath, "/bin/sh", string.Format("-c 'chmod 0755 \"{0}\"'", GradleScriptPath.Replace("'", "'\"'\"'")), "Fix gradlew permissions");
					}
					else
					{
						if (CreateRunGradle(UE4BuildGradlePath))
						{
							GradleScriptPath = Path.Combine(UE4BuildGradlePath, "rungradle.bat");
						}
						else
						{
							GradleScriptPath = Path.Combine(UE4BuildGradlePath, "gradlew.bat");
						}
					}

					// Use gradle to build the .apk file
					string GradleOptions = GradleBuildType;
					string ShellExecutable = Utils.IsRunningOnMono ? "/bin/sh" : "cmd.exe";
					string ShellParametersBegin = Utils.IsRunningOnMono ? "-c '" : "/c ";
					string ShellParametersEnd = Utils.IsRunningOnMono ? "'" : "";
					RunCommandLineProgramWithException(UE4BuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, "Making .apk with Gradle...");
					
					// For build machine run a clean afterward to clean up intermediate files (does not remove final APK)
					if (Environment.GetEnvironmentVariable("IsBuildMachine") == "1")
					{
						GradleOptions = "clean";
						RunCommandLineProgramWithException(UE4BuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, "Cleaning Gradle intermediates...");
					}
				}

				bool bBuildWithHiddenSymbolVisibility = false;
				Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildWithHiddenSymbolVisibility", out bBuildWithHiddenSymbolVisibility);
				if ( Configuration == "Shipping" && bBuildWithHiddenSymbolVisibility)
				{
					// Copy .so with symbols to 
					int StoreVersion = GetStoreVersion();
					string SymbolSODirectory = Path.Combine(Path.GetDirectoryName(DestApkName), ProjectName + "_Symbols_v" + StoreVersion + "/" + ProjectName + Arch + GPUArchitecture);
					string SymbolifiedSOPath = Path.Combine(SymbolSODirectory, Path.GetFileName(FinalSOName));
					MakeDirectoryIfRequired(SymbolifiedSOPath);

					File.Copy(FinalSOName, SymbolifiedSOPath, true);
				}
			}

		}

		private void PrepareToSignApk(string BuildPath)
		{
			// ini file to get settings from
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			string KeyAlias, KeyStore, KeyStorePassword, KeyPassword;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyAlias", out KeyAlias);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStore", out KeyStore);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStorePassword", out KeyStorePassword);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyPassword", out KeyPassword);

			if (string.IsNullOrEmpty(KeyAlias) || string.IsNullOrEmpty(KeyStore) || string.IsNullOrEmpty(KeyStorePassword))
			{
				throw new BuildException("DistributionSigning settings are not all set. Check the DistributionSettings section in the Android tab of Project Settings");
			}

			string[] AntPropertiesLines = new string[4];
			AntPropertiesLines[0] = "key.store=" + KeyStore;
			AntPropertiesLines[1] = "key.alias=" + KeyAlias;
			AntPropertiesLines[2] = "key.store.password=" + KeyStorePassword;
			AntPropertiesLines[3] = "key.alias.password=" + ((string.IsNullOrEmpty(KeyPassword) || KeyPassword == "_sameaskeystore_") ? KeyStorePassword : KeyPassword);

			// now write out the properties
			File.WriteAllLines(Path.Combine(BuildPath, "ant.properties"), AntPropertiesLines);
		}

		private List<string> CollectPluginDataPaths(TargetReceipt Receipt)
		{
			List<string> PluginExtras = new List<string>();
			if (Receipt == null)
			{
				Log.TraceInformation("Receipt is NULL");
				return PluginExtras;
			}

			// collect plugin extra data paths from target receipt
			var Results = Receipt.AdditionalProperties.Where(x => x.Name == "AndroidPlugin");
			foreach (var Property in Results)
			{
				// Keep only unique paths
				string PluginPath = Property.Value;
				if (PluginExtras.FirstOrDefault(x => x == PluginPath) == null)
				{
					PluginExtras.Add(PluginPath);
					Log.TraceInformation("AndroidPlugin: {0}", PluginPath);
				}
			}
			return PluginExtras;
		}

		public override bool PrepTargetForDeployment(UEBuildDeployTarget InTarget)
		{
			//Log.TraceInformation("$$$$$$$$$$$$$$ PrepTargetForDeployment $$$$$$$$$$$$$$$$$ {0}", InTarget.TargetName);
			AndroidToolChain ToolChain = new AndroidToolChain(InTarget.ProjectFile, false, InTarget.AndroidArchitectures, InTarget.AndroidGPUArchitectures); 

			// we need to strip architecture from any of the output paths
			string BaseSoName = ToolChain.RemoveArchName(InTarget.OutputPaths[0].FullName);

			// get the receipt
			UnrealTargetPlatform Platform = InTarget.Platform;
			UnrealTargetConfiguration Configuration = InTarget.Configuration;
			string ProjectBaseName = Path.GetFileName(BaseSoName).Replace("-" + Platform, "").Replace("-" + Configuration, "").Replace(".so", "");
			FileReference ReceiptFilename = TargetReceipt.GetDefaultPath(InTarget.ProjectDirectory, ProjectBaseName, Platform, Configuration, "");
			Log.TraceInformation("Receipt Filename: {0}", ReceiptFilename);
			SetAndroidPluginData(ToolChain.GetAllArchitectures(), CollectPluginDataPaths(TargetReceipt.Read(ReceiptFilename, UnrealBuildTool.EngineDirectory, InTarget.ProjectDirectory)));

			// make an apk at the end of compiling, so that we can run without packaging (debugger, cook on the fly, etc)
			string RelativeEnginePath = UnrealBuildTool.EngineDirectory.MakeRelativeTo(DirectoryReference.GetCurrentDirectory());
			MakeApk(ToolChain, InTarget.TargetName, InTarget.ProjectDirectory.FullName, BaseSoName, RelativeEnginePath, bForDistribution: false, CookFlavor: "",
				bMakeSeparateApks: ShouldMakeSeparateApks(), bIncrementalPackage: true, bDisallowPackagingDataInApk: false, bDisallowExternalFilesDir: true);

			// if we made any non-standard .apk files, the generated debugger settings may be wrong
			if (ShouldMakeSeparateApks() && (InTarget.OutputPaths.Count > 1 || !InTarget.OutputPaths[0].FullName.Contains("-armv7-es2")))
			{
				Console.WriteLine("================================================================================================================================");
				Console.WriteLine("Non-default apk(s) have been made: If you are debugging, you will need to manually select one to run in the debugger properties!");
				Console.WriteLine("================================================================================================================================");
			}
			return true;
		}

		public static bool ShouldMakeSeparateApks()
		{
			// @todo android fat binary: Currently, there isn't much utility in merging multiple .so's into a single .apk except for debugging,
			// but we can't properly handle multiple GPU architectures in a single .apk, so we are disabling the feature for now
			// The user will need to manually select the apk to run in their Visual Studio debugger settings (see Override APK in TADP, for instance)
			// If we change this, pay attention to <OverrideAPKPath> in AndroidProjectGenerator
			return true;

			// check to see if the project wants separate apks
			// 			ConfigCacheIni Ini = nGetConfigCacheIni("Engine");
			// 			bool bSeparateApks = false;
			// 			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSplitIntoSeparateApks", out bSeparateApks);
			// 
			// 			return bSeparateApks;
		}

		public bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, DirectoryReference ProjectDirectory, string ExecutablePath, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bIsDataDeploy)
		{
			//Log.TraceInformation("$$$$$$$$$$$$$$ PrepForUATPackageOrDeploy $$$$$$$$$$$$$$$$$");

			// note that we cannot allow the data packaged into the APK if we are doing something like Launch On that will not make an obb
			// file and instead pushes files directly via deploy
			AndroidToolChain ToolChain = new AndroidToolChain(ProjectFile, false, null, null);
			MakeApk(ToolChain, ProjectName, ProjectDirectory.FullName, ExecutablePath, EngineDirectory, bForDistribution: bForDistribution, CookFlavor: CookFlavor,
				bMakeSeparateApks: ShouldMakeSeparateApks(), bIncrementalPackage: false, bDisallowPackagingDataInApk: bIsDataDeploy, bDisallowExternalFilesDir: !bForDistribution || bIsDataDeploy );
			return true;
		}

		public static void OutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null))
			{
				Log.TraceInformation(Line.Data);
			}
		}

		private void UpdateBuildXML(string UE4Arch, string NDKArch, string EngineDir, string UE4BuildPath)
		{
			string SourceFilename = Path.Combine(UE4BuildPath, "build.xml");
			string DestFilename = SourceFilename;

			Dictionary<string, string> Replacements = new Dictionary<string, string>{
			  { "<import file=\"${sdk.dir}/tools/ant/build.xml\" />", UPL.ProcessPluginNode(NDKArch, "buildXmlPropertyAdditions", "")}
			};

			string[] TemplateSrc = File.ReadAllLines(SourceFilename);
			string[] TemplateDest = File.Exists(DestFilename) ? File.ReadAllLines(DestFilename) : null;

			for (int LineIndex = 0; LineIndex < TemplateSrc.Length; ++LineIndex)
			{
				string SrcLine = TemplateSrc[LineIndex];
				bool Changed = false;
				foreach (var KVP in Replacements)
				{
					if (SrcLine.Contains(KVP.Key))
					{
						// insert replacement before the <import>
						SrcLine = SrcLine.Replace(KVP.Key, KVP.Value);
						// then add the <import>
						SrcLine += KVP.Key;
						Changed = true;
					}
				}
				if (Changed)
				{
					TemplateSrc[LineIndex] = SrcLine;
				}
			}

			if (TemplateDest == null || TemplateSrc.Length != TemplateDest.Length || !TemplateSrc.SequenceEqual(TemplateDest))
			{
				Log.TraceInformation("\n==== Writing new build.xml file to {0} ====", DestFilename);
				File.WriteAllLines(DestFilename, TemplateSrc);
			}
		}

		private void UpdateGameActivity(string UE4Arch, string NDKArch, string EngineDir, string UE4BuildPath)
		{
			string SourceFilename = Path.Combine(EngineDir, "Build", "Android", "Java", "src", "com", "epicgames", "ue4", "GameActivity.java");
			string DestFilename = Path.Combine(UE4BuildPath, "src", "com", "epicgames", "ue4", "GameActivity.java");

			string LoadLibraryDefaults = "";

			string AndroidGraphicsDebugger;
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "AndroidGraphicsDebugger", out AndroidGraphicsDebugger);

			switch (AndroidGraphicsDebugger.ToLower())
			{
				case "mali":
					LoadLibraryDefaults += "\t\ttry\n" +
											"\t\t{\n" +
											"\t\t\tSystem.loadLibrary(\"MGD\");\n" +
											"\t\t}\n" +
											"\t\tcatch (java.lang.UnsatisfiedLinkError e)\n" +
											"\t\t{\n" +
											"\t\t\tLog.debug(\"libMGD.so not loaded.\");\n" +
											"\t\t}\n";
					break;
				case "renderdoc":
					LoadLibraryDefaults += "\t\ttry\n" +
											"\t\t{\n" +
											"\t\t\tSystem.loadLibrary(\"VkLayer_GLES_RenderDoc\");\n" +
											"\t\t}\n" +
											"\t\tcatch (java.lang.UnsatisfiedLinkError e)\n" +
											"\t\t{\n" +
											"\t\t\tLog.debug(\"libVkLayer_GLES_RenderDoc.so not loaded.\");\n" +
											"\t\t}\n";
					break;
			}
			
			Dictionary<string, string> Replacements = new Dictionary<string, string>{
				{ "//$${gameActivityImportAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityImportAdditions", "")},
				{ "//$${gameActivityPostImportAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityPostImportAdditions", "")},
				{ "//$${gameActivityClassAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityClassAdditions", "")},
				{ "//$${gameActivityReadMetadataAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityReadMetadataAdditions", "")},
				{ "//$${gameActivityOnCreateAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnCreateAdditions", "")},
				{ "//$${gameActivityOnDestroyAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnDestroyAdditions", "")},
				{ "//$${gameActivityOnStartAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnStartAdditions", "")},
				{ "//$${gameActivityOnStopAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnStopAdditions", "")},
				{ "//$${gameActivityOnPauseAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnPauseAdditions", "")},
				{ "//$${gameActivityOnResumeAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnResumeAdditions", "")},
				{ "//$${gameActivityOnNewIntentAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnNewIntentAdditions", "")},
  				{ "//$${gameActivityOnActivityResultAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnActivityResultAdditions", "")},
				{ "//$${soLoadLibrary}$$", UPL.ProcessPluginNode(NDKArch, "soLoadLibrary", LoadLibraryDefaults)}
			};

			string[] TemplateSrc = File.ReadAllLines(SourceFilename);
			string[] TemplateDest = File.Exists(DestFilename) ? File.ReadAllLines(DestFilename) : null;

			for (int LineIndex = 0; LineIndex < TemplateSrc.Length; ++LineIndex)
			{
				string SrcLine = TemplateSrc[LineIndex];
				bool Changed = false;
				foreach (var KVP in Replacements)
				{
					if(SrcLine.Contains(KVP.Key))
					{
						SrcLine = SrcLine.Replace(KVP.Key, KVP.Value);
						Changed = true;
					}
				}
				if (Changed)
				{
					TemplateSrc[LineIndex] = SrcLine;
				}
			}

			if (TemplateDest == null || TemplateSrc.Length != TemplateDest.Length || !TemplateSrc.SequenceEqual(TemplateDest))
			{
                Log.TraceInformation("\n==== Writing new GameActivity.java file to {0} ====", DestFilename);
				File.WriteAllLines(DestFilename, TemplateSrc);
			}
		}

		private AndroidAARHandler CreateAARHandler(string EngineDir, string UE4BuildPath, List<string> NDKArches, bool HandleDependencies=true)
		{
			AndroidAARHandler AARHandler = new AndroidAARHandler();
			string ImportList = "";

			// Get some common paths
			string AndroidHome = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%").TrimEnd('/', '\\');
			EngineDir = EngineDir.TrimEnd('/', '\\');

			// Add the AARs from the default aar-imports.txt
			// format: Package,Name,Version
			string ImportsFile = Path.Combine(UE4BuildPath, "aar-imports.txt");
			if (File.Exists(ImportsFile))
			{
				ImportList = File.ReadAllText(ImportsFile);
			}

			// Run the UPL imports section for each architecture and add any new imports (duplicates will be removed)
			foreach (string NDKArch in NDKArches)
			{
				ImportList = UPL.ProcessPluginNode(NDKArch, "AARImports", ImportList);
			}

			// Add the final list of imports and get dependencies
			foreach (string Line in ImportList.Split('\n'))
			{
				string Trimmed = Line.Trim(' ', '\r');

				if (Trimmed.StartsWith("repository "))
				{
					string DirectoryPath = Trimmed.Substring(11).Trim(' ').TrimEnd('/', '\\');
					DirectoryPath = DirectoryPath.Replace("$(ENGINEDIR)", EngineDir);
					DirectoryPath = DirectoryPath.Replace("$(ANDROID_HOME)", AndroidHome);
					DirectoryPath = DirectoryPath.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
					AARHandler.AddRepository(DirectoryPath);
				}
				else if (Trimmed.StartsWith("repositories "))
				{
					string DirectoryPath = Trimmed.Substring(13).Trim(' ').TrimEnd('/', '\\');
					DirectoryPath = DirectoryPath.Replace("$(ENGINEDIR)", EngineDir);
					DirectoryPath = DirectoryPath.Replace("$(ANDROID_HOME)", AndroidHome);
					DirectoryPath = DirectoryPath.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
					AARHandler.AddRepositories(DirectoryPath, "m2repository");
				}
				else
				{
					string[] Sections = Trimmed.Split(',');
					if (Sections.Length == 3)
					{
						string PackageName = Sections[0].Trim(' ');
						string BaseName = Sections[1].Trim(' ');
						string Version = Sections[2].Trim(' ');
						Log.TraceInformation("AARImports: {0}, {1}, {2}", PackageName, BaseName, Version);
						AARHandler.AddNewAAR(PackageName, BaseName, Version, HandleDependencies);
					}
				}
			}

			return AARHandler;
		}

		private void PrepareJavaLibsForGradle(string JavaLibsDir, string UE4BuildGradlePath, string CompileSDKVersion, string BuildToolsVersion)
		{
			StringBuilder SettingsGradleContent = new StringBuilder();
			StringBuilder ProjectDependencyContent = new StringBuilder();

			SettingsGradleContent.AppendLine("include ':app'");
			ProjectDependencyContent.AppendLine("dependencies {");

			string[] LibDirs = Directory.GetDirectories(JavaLibsDir);
			foreach (string LibDir in LibDirs)
			{
				string RelativePath = Path.GetFileName(LibDir);

				SettingsGradleContent.AppendLine(string.Format("include ':{0}'", RelativePath));
				ProjectDependencyContent.AppendLine(string.Format("\tcompile project(':{0}')", RelativePath));

				string GradleProjectPath = Path.Combine(UE4BuildGradlePath, RelativePath);
				string GradleProjectMainPath = Path.Combine(GradleProjectPath, "src", "main");

				string GradleManifest = Path.Combine(GradleProjectMainPath, "AndroidManifest.xml");
				MakeDirectoryIfRequired(GradleManifest);
				File.Copy(Path.Combine(LibDir, "AndroidManifest.xml"), GradleManifest, true);

				// Copy parts were they need to be
				CleanCopyDirectory(Path.Combine(LibDir, "assets"), Path.Combine(GradleProjectPath, "assets"));
				CleanCopyDirectory(Path.Combine(LibDir, "libs"), Path.Combine(GradleProjectPath, "libs"));
				CleanCopyDirectory(Path.Combine(LibDir, "res"), Path.Combine(GradleProjectMainPath, "res"));
				CleanCopyDirectory(Path.Combine(LibDir, "src"), Path.Combine(GradleProjectMainPath, "java"));

				// Now generate a build.gradle from the manifest
				StringBuilder BuildGradleContent = new StringBuilder();
				BuildGradleContent.AppendLine("apply plugin: 'com.android.library'");
				BuildGradleContent.AppendLine("android {");
				BuildGradleContent.AppendLine(string.Format("\tcompileSdkVersion {0}", CompileSDKVersion));
				BuildGradleContent.AppendLine(string.Format("\tbuildToolsVersion \"{0}\"", BuildToolsVersion));
				BuildGradleContent.AppendLine("\tdefaultConfig {");

				// Try to get the SDK target from the AndroidManifest.xml
				string MinSDK = "9";
				string TargetSDK = "9";
				string VersionCode = "";
				string VersionName = "";
				string ManifestFilename = Path.Combine(LibDir, "AndroidManifest.xml");
				XDocument ManifestXML;
				if (File.Exists(ManifestFilename))
				{
					try
					{
						ManifestXML = XDocument.Load(ManifestFilename);

						XAttribute VersionCodeAttr = ManifestXML.Root.Attribute(XName.Get("versionCode", "http://schemas.android.com/apk/res/android"));
						if (VersionCodeAttr != null)
						{
							VersionCode = VersionCodeAttr.Value;
						}

						XAttribute VersionNameAttr = ManifestXML.Root.Attribute(XName.Get("versionName", "http://schemas.android.com/apk/res/android"));
						if (VersionNameAttr != null)
						{
							VersionName = VersionNameAttr.Value;
						}

						XElement UsesSdk = ManifestXML.Root.Element(XName.Get("uses-sdk", ManifestXML.Root.Name.NamespaceName));
						if (UsesSdk != null)
						{
							XAttribute MinSDKAttr = UsesSdk.Attribute(XName.Get("minSdkVersion", "http://schemas.android.com/apk/res/android"));
							if (MinSDKAttr != null)
							{
								MinSDK = MinSDKAttr.Value;
							}

							XAttribute TargetSDKAttr = UsesSdk.Attribute(XName.Get("targetSdkVersion", "http://schemas.android.com/apk/res/android"));
							if (TargetSDKAttr != null)
							{
								TargetSDK = TargetSDKAttr.Value;
							}
						}
					}
					catch (Exception e)
					{
						Log.TraceError("AAR Manifest file {0} parsing error! {1}", ManifestFilename, e);
					}
				}

				BuildGradleContent.AppendLine(string.Format("\t\tminSdkVersion {0}", MinSDK));
				BuildGradleContent.AppendLine(string.Format("\t\ttargetSdkVersion {0}", TargetSDK));
				if (VersionCode != "")
				{
					BuildGradleContent.AppendLine(string.Format("\t\tversionCode {0}", VersionCode));
				}
				if (VersionName != "")
				{
					BuildGradleContent.AppendLine(string.Format("\t\tversionName \"{0}\"", VersionName));
				}
				BuildGradleContent.AppendLine("\t}");
				BuildGradleContent.AppendLine("}");

				string AdditionsGradleFilename = Path.Combine(LibDir, "additions.gradle");
				if (File.Exists(AdditionsGradleFilename))
				{
					string[] AdditionsLines = File.ReadAllLines(AdditionsGradleFilename);
					foreach (string LineContents in AdditionsLines)
					{
						BuildGradleContent.AppendLine(LineContents);
					}
				}

				string BuildGradleFilename = Path.Combine(GradleProjectPath, "build.gradle");
				File.WriteAllText(BuildGradleFilename, BuildGradleContent.ToString());
			}
			ProjectDependencyContent.AppendLine("}");

			string SettingsGradleFilename = Path.Combine(UE4BuildGradlePath, "settings.gradle");
			File.WriteAllText(SettingsGradleFilename, SettingsGradleContent.ToString());

			string ProjectsGradleFilename = Path.Combine(UE4BuildGradlePath, "app", "projects.gradle");
			File.WriteAllText(ProjectsGradleFilename, ProjectDependencyContent.ToString());
		}

		private void GenerateGradleAARImports(string EngineDir, string UE4BuildPath, List<string> NDKArches)
		{
			AndroidAARHandler AARHandler = CreateAARHandler(EngineDir, UE4BuildPath, NDKArches, false);
			StringBuilder AARImportsContent = new StringBuilder();

			// Add repositories
			AARImportsContent.AppendLine("repositories {");
			foreach (string Repository in AARHandler.Repositories)
			{
				string RepositoryPath = Path.GetFullPath(Repository).Replace('\\', '/');
				AARImportsContent.AppendLine("\tmaven { url uri('" + RepositoryPath + "') }");
			}
			AARImportsContent.AppendLine("}");

			// Add dependencies
			AARImportsContent.AppendLine("dependencies {");
			foreach (AndroidAARHandler.AndroidAAREntry Dependency in AARHandler.AARList)
			{
				AARImportsContent.AppendLine(string.Format("\tcompile '{0}:{1}:{2}'", Dependency.Filename, Dependency.BaseName, Dependency.Version));
			}
			AARImportsContent.AppendLine("}");

			string AARImportsFilename = Path.Combine(UE4BuildPath, "gradle", "app", "aar-imports.gradle");
			File.WriteAllText(AARImportsFilename, AARImportsContent.ToString());
		}

		private void ExtractAARAndJARFiles(string EngineDir, string UE4BuildPath, List<string> NDKArches, string AppPackageName, string AARExtractListFilename)
		{
			AndroidAARHandler AARHandler = CreateAARHandler(EngineDir, UE4BuildPath, NDKArches, true);

			// Finally, extract the AARs and copy the JARs
			AARHandler.ExtractAARs(UE4BuildPath, AppPackageName);
			AARHandler.CopyJARs(UE4BuildPath);

			// Write list of AAR files extracted
			StringBuilder AARListContents = new StringBuilder();
			foreach (AndroidAARHandler.AndroidAAREntry Dependency in AARHandler.AARList)
			{
				AARListContents.AppendLine(Dependency.Filename);
			}
			File.WriteAllText(AARExtractListFilename, AARListContents.ToString());
		}
    }
}
