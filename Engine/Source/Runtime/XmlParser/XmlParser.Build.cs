// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XmlParser : ModuleRules
{
	public XmlParser( ReadOnlyTargetRules Target ) : base(Target)
	{
		PublicIncludePaths.AddRange(new string[] { "Editor/XmlParser/Public" });
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"CoreUObject",
			}
			);
	}
}
