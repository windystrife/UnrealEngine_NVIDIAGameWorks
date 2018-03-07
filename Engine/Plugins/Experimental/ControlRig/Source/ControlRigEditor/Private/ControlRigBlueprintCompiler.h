// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetCompilerModule.h"
#include "KismetCompiler.h"

class FControlRigBlueprintCompiler : public IBlueprintCompiler
{
public:
	/** IBlueprintCompiler interface */
	virtual bool CanCompile(const UBlueprint* Blueprint) override;
	virtual void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TArray<UObject*>* ObjLoaded) override;
};

class FControlRigBlueprintCompilerContext : public FKismetCompilerContext
{
public:
	FControlRigBlueprintCompilerContext(UBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions, TArray<UObject*>* InObjLoaded)
		: FKismetCompilerContext(SourceSketch, InMessageLog, InCompilerOptions, InObjLoaded)
		, CurrentControlRigAllocationIndex(0)
	{
	}

	/** Get a unique allocation index for this ControlRig's current compilation */
	int32 GetNewControlRigAllocationIndex();

private:
	/** Current ControlRig allocation index used to preallocate sub-ControlRigs */
	int32 CurrentControlRigAllocationIndex;
};