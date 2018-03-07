// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class WaveworksTesterEditorTarget : TargetRules
{
	public WaveworksTesterEditorTarget(TargetInfo Target) : base(Target)
    {
		Type = TargetType.Editor;

        ExtraModuleNames.Add("WaveworksTester");
    }
}
