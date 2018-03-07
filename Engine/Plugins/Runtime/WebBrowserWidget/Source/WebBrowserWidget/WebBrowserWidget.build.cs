// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WebBrowserWidget : ModuleRules
	{
		public WebBrowserWidget(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
					"WebBrowser",
                    "Slate",
                    "SlateCore",
					"UMG"
				}
			);
		}
	}
}
