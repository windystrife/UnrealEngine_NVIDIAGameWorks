// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class GoogleCloudMessaging : ModuleRules
	{
		public GoogleCloudMessaging(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(new string[]
				{
					"GoogleCloudMessaging/Private",
				});

			PrivateDependencyModuleNames.AddRange(new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Launch"
				});

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "GoogleCloudMessaging_UPL.xml")));
			}

			PublicIncludePathModuleNames.Add("Launch");
		}
	}
}
