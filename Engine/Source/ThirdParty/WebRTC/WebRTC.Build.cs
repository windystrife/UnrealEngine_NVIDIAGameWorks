// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WebRTC : ModuleRules
{
	public WebRTC(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bShouldUseWebRTC = false;
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			bShouldUseWebRTC = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			bShouldUseWebRTC = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64"))
		{
			bShouldUseWebRTC = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			bShouldUseWebRTC = true;
		}

		if (bShouldUseWebRTC)
		{
			string VS2013Friendly_WebRtcSdkPath = Target.UEThirdPartySourceDirectory + "WebRTC/VS2013_friendly";
			string LinuxTrunk_WebRtcSdkPath = Target.UEThirdPartySourceDirectory + "WebRTC/sdk_trunk_linux";

			string PlatformSubdir = Target.Platform.ToString();
			string ConfigPath = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";


			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				Definitions.Add("WEBRTC_WIN=1");

				string VisualStudioVersionFolder = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

				string IncludePath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "include", PlatformSubdir, VisualStudioVersionFolder);
				PublicSystemIncludePaths.Add(IncludePath);

				string LibraryPath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "lib", PlatformSubdir, VisualStudioVersionFolder, ConfigPath);

				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_base.lib"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_base_approved.lib"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_xmllite.lib"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_xmpp.lib"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "expat.lib"));
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				Definitions.Add("WEBRTC_MAC=1");
				Definitions.Add("WEBRTC_POSIX=1");

				string IncludePath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "include", PlatformSubdir);
				PublicSystemIncludePaths.Add(IncludePath);

				string LibraryPath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "lib", PlatformSubdir, ConfigPath);

				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_base.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_base_approved.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_xmllite.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_xmpp.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "libexpat.a"));
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				Definitions.Add("WEBRTC_LINUX=1");
				Definitions.Add("WEBRTC_POSIX=1");

				string IncludePath = Path.Combine(LinuxTrunk_WebRtcSdkPath, "include");
				PublicSystemIncludePaths.Add(IncludePath);

				// This is slightly different than the other platforms
				string LibraryPath = Path.Combine(LinuxTrunk_WebRtcSdkPath, "lib", Target.Architecture, ConfigPath);

				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_base.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_base_approved.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_xmllite.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_xmpp.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "libexpat.a"));
			}
			else if (Target.Platform == UnrealTargetPlatform.PS4)
			{
				Definitions.Add("WEBRTC_ORBIS");
				Definitions.Add("FEATURE_ENABLE_SSL");
				Definitions.Add("SSL_USE_OPENSSL");
				Definitions.Add("EXPAT_RELATIVE_PATH");

				string IncludePath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "include", PlatformSubdir);
				PublicSystemIncludePaths.Add(IncludePath);

				string LibraryPath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "lib", PlatformSubdir, ConfigPath);

				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_base.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_base_approved.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_xmllite.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_xmpp.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "expat.a"));
			}

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		}

	}
}
