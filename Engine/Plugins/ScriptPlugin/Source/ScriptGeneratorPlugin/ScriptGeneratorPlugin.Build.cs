// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class ScriptGeneratorPlugin : ModuleRules
	{
		public ScriptGeneratorPlugin(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {					
					"Programs/UnrealHeaderTool/Public",
					// ... add other public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/ScriptGeneratorPlugin/Private",
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"Projects",
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);

			// This checks only for UHT target platform, not the target platform of the game we're building so it's important
			// to make sure Lua is compiled for all supported platforms
			var LuaLibDirectory = Path.Combine("..", "Plugins", "ScriptPlugin", "Source", "Lua", "Lib", Target.Platform.ToString(), "Release");
			var LuaLibPath = Path.Combine(LuaLibDirectory, "Lua.lib");
			if (File.Exists(LuaLibPath))
			{
				Log.TraceVerbose("ScriptGenerator LUA Integration enabled");
				Definitions.Add("WITH_LUA=1");
			}
			else
			{
				Log.TraceVerbose("ScriptGenerator LUA Integration NOT enabled");
				Definitions.Add("WITH_LUA=0");
			}

			Definitions.Add("HACK_HEADER_GENERATOR=1");
		}
	}
}
