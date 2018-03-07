// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintCompiler.h"
#include "ControlRig.h"
#include "KismetCompiler.h"

bool FControlRigBlueprintCompiler::CanCompile(const UBlueprint* Blueprint)
{
	if (Blueprint && Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UControlRig::StaticClass()))
	{
		return true;
	}

	return false;
}

void FControlRigBlueprintCompiler::Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TArray<UObject*>* ObjLoaded)
{
	FControlRigBlueprintCompilerContext Compiler(Blueprint, Results, CompileOptions, ObjLoaded);
	Compiler.Compile();
}

int32 FControlRigBlueprintCompilerContext::GetNewControlRigAllocationIndex()
{
	return CurrentControlRigAllocationIndex++;
}
