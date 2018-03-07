// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "NiagaraCommon.h"
#include "NiagaraScriptSourceBase.generated.h"

struct EditorExposedVectorConstant
{
	FName ConstName;
	FVector4 Value;
};

struct EditorExposedVectorCurveConstant
{
	FName ConstName;
	class UCurveVector *Value;
};

/** Runtime data for a Niagara system */
UCLASS(MinimalAPI)
class UNiagaraScriptSourceBase : public UObject
{
	GENERATED_UCLASS_BODY()

	TArray<TSharedPtr<EditorExposedVectorConstant> > ExposedVectorConstants;
	TArray<TSharedPtr<EditorExposedVectorCurveConstant> > ExposedVectorCurveConstants;

	/** Determines if the input change id is equal to the current source graph's change id.*/
	virtual bool IsSynchronized(const FGuid& InChangeId) { return true; }

	virtual UNiagaraScriptSourceBase* MakeRecursiveDeepCopy(UObject* DestOuter, TMap<const UObject*, UObject*>& ExistingConversions) const { return nullptr; }

	/** Determine if there are any external dependencies wrt to scripts and ensure that those dependencies are sucked into the existing package.*/
	virtual void SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions) {}

	/** Enforce that the source graph is now out of sync with the script.*/
	virtual void MarkNotSynchronized() {}

	virtual FGuid GetChangeID() { return FGuid(); };

	/** Have we called PreCompile on this source previously?*/
	virtual bool IsPreCompiled() const { return false; }
	
	/** Cause the source to build up any internal variables that will be useful in the compilation process.*/
	virtual void PreCompile(UNiagaraEmitter* Emitter, bool bClearErrors = true) {}
	
	/** If the user previously pre-compiled the source, dig through the data to find any variables defined. */
	virtual bool GatherPreCompiledVariables(const FString& InNamespaceFilter, TArray<FNiagaraVariable>& OutVars) { return false; }

	/** Implements compilation of a Niagara script.*/
	virtual ENiagaraScriptCompileStatus Compile(UNiagaraScript* ScriptOwner, FString& OutGraphLevelErrorMessages) { return ENiagaraScriptCompileStatus::NCS_Unknown; }

	/** Do any cleanup to return this data source to its original state before precompilation. Note that this must be called after the "compile" operation.*/
	virtual void PostCompile() {}
};
