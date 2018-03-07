// Copyright 2017 Google Inc.

using System.IO;
using UnrealBuildTool;

public class GoogleInstantPreview : ModuleRules
{
	public GoogleInstantPreview(ReadOnlyTargetRules Target)
		: base (Target)
	{
        Type = ModuleType.External;

		string GoogleInstantPreviewTargetDir = Path.Combine(Target.UEThirdPartyBinariesDirectory, "GoogleInstantPreview");
		string GoogleInstantPreviewSourceDir = Path.Combine(Target.UEThirdPartySourceDirectory, "GoogleInstantPreview");

		PublicIncludePaths.Add(GoogleInstantPreviewSourceDir);

        // Instant preview only supported by Editor
        if (Target.Type == TargetType.Editor)
        {
            if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
            {
                string IpSharedPlatform = Target.Platform == UnrealTargetPlatform.Win64 ? "x64" : "Win32";
                string IpSharedLibSourceDir = Path.Combine(GoogleInstantPreviewSourceDir, IpSharedPlatform, "Release");
                PublicLibraryPaths.Add(IpSharedLibSourceDir);
                PublicAdditionalLibraries.Add("ip_shared.lib");
                string[] dllDeps = {
                    "ip_shared.dll",
                    "libeay32.dll",
                    "ssleay32.dll",
                    "zlib.dll",
                };
                string IpSharedLibTargetDir = Path.Combine(GoogleInstantPreviewTargetDir, IpSharedPlatform, "Release");
                foreach (string dll in dllDeps)
                {
                    PublicDelayLoadDLLs.Add(dll);
                    string dllPath = Path.Combine(IpSharedLibTargetDir, dll);
                    RuntimeDependencies.Add(new RuntimeDependency(dllPath));
                }
            }
            else if (Target.Platform == UnrealTargetPlatform.Mac)
            {
                string[] dylibDeps = {
                        "libip_shared.dylib",
                        "libgrpc++.dylib",
                        "libgpr.dylib",
                        "libgrpc.dylib",
                };
                string IpSharedLibTargetDir = Path.Combine(GoogleInstantPreviewTargetDir, "Mac", "Release");
                foreach (string dylib in dylibDeps)
                {
                    
                    string dylibPath = Path.Combine(IpSharedLibTargetDir, dylib);
                    PublicDelayLoadDLLs.Add(dylibPath);
                    RuntimeDependencies.Add(new RuntimeDependency(dylibPath));
                    
                }
            }
        }
	}
}
