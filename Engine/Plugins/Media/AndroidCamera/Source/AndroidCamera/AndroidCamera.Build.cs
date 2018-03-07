// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class AndroidCamera : ModuleRules
	{
		public AndroidCamera(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AndroidCameraFactory",
					"Core",
					"Engine",
					"Launch",
					"MediaUtils",
					"RenderCore",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"AndroidCamera/Private",
					"AndroidCamera/Private/Player",
					"AndroidCamera/Private/Shared",
				});

			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "AndroidCamera_UPL.xml")));
		}
	}
}
