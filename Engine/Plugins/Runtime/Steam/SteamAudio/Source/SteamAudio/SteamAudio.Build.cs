//
// Copyright (C) Valve Corporation. All rights reserved.
//

namespace UnrealBuildTool.Rules
{
	public class SteamAudio : ModuleRules
	{
		public SteamAudio(ReadOnlyTargetRules Target) : base(Target)
		{

			OptimizeCode = CodeOptimization.Never;

			PrivateIncludePaths.AddRange(
				new string[] {
					"SteamAudio/Private",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"AudioMixer",
					"InputCore",
					"RenderCore",
					"ShaderCore",
					"RHI"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"TargetPlatform",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"Projects",
					"AudioMixer",
					"XAudio2",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
				PrivateDependencyModuleNames.Add("Landscape");
			}

			AddEngineThirdPartyPrivateStaticDependencies(Target, "libPhonon");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11Audio");
		}
	}
}
