// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DocumentationEditorTarget : TargetRules
{
	public DocumentationEditorTarget(TargetInfo Target)
	: base(Target)
	{
		Type = TargetType.Editor;
		ExtraModuleNames.Add("UE4Game");
		ExtraModuleNames.Add("OnlineSubsystemAmazon");
		ExtraModuleNames.Add("OnlineSubsystemFacebook");
		ExtraModuleNames.Add("OnlineSubsystemNull");
		ExtraModuleNames.Add("OnlineSubsystemSteam");
	}
}
