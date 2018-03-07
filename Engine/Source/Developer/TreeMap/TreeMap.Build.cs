// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class TreeMap : ModuleRules
	{
		public TreeMap(ReadOnlyTargetRules Target) : base(Target)
		{
	        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "SlateCore" });
	        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "InputCore", "XmlParser" });
		}
	}
}
