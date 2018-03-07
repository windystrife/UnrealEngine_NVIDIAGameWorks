// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

class UNiagaraNodeFunctionCall;
struct FNiagaraVariable;

class NIAGARAEDITOR_API FNiagaraParameterHandle
{
public:
	FNiagaraParameterHandle();

	FNiagaraParameterHandle(const FString& InParameterHandleString);

	FNiagaraParameterHandle(const FString& InNamespace, const FString& InName);

	bool operator==(const FNiagaraParameterHandle& Other) const;

	static FNiagaraParameterHandle CreateAliasedModuleParameterHandle(const FNiagaraParameterHandle& ModuleParameterHandle, UNiagaraNodeFunctionCall* ModuleNode);

	static FNiagaraParameterHandle CreateEngineParameterHandle(const FNiagaraVariable& SystemVariable);

	static FNiagaraParameterHandle CreateEmitterParameterHandle(const FNiagaraVariable& EmitterVariable);

	static FNiagaraParameterHandle CreateParticleAttributeParameterHandle(const FString& InName);

	static FNiagaraParameterHandle CreateModuleParameterHandle(const FString& InName);

	static FNiagaraParameterHandle CreateInitialParameterHandle(const FNiagaraParameterHandle& Handle);

	bool IsValid() const;

	const FString& GetParameterHandleString() const;

	const FString& GetName() const;

	const FString& GetNamespace() const;

	bool IsEngineHandle() const;

	bool IsSystemHandle() const;

	bool IsEmitterHandle() const;

	bool IsParticleAttributeHandle() const;

	bool IsModuleHandle() const;

public:
	static const FString UserNamespace;
	static const FString EngineNamespace;
	static const FString SystemNamespace;
	static const FString EmitterNamespace;
	static const FString ParticleAttributeNamespace;
	static const FString ModuleNamespace;
	static const FString InitialPrefix;

private:
	FString ParameterHandleString;
	FString Name;
	FString Namespace;
};