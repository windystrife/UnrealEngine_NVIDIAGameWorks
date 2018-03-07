// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterHandle.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScript.h"
#include "NiagaraTypes.h"

const FString FNiagaraParameterHandle::UserNamespace(TEXT("User"));
const FString FNiagaraParameterHandle::EngineNamespace(TEXT("Engine"));
const FString FNiagaraParameterHandle::SystemNamespace(TEXT("System"));
const FString FNiagaraParameterHandle::EmitterNamespace(TEXT("Emitter"));
const FString FNiagaraParameterHandle::ParticleAttributeNamespace(TEXT("Particles"));
const FString FNiagaraParameterHandle::ModuleNamespace(TEXT("Module"));
const FString FNiagaraParameterHandle::InitialPrefix(TEXT("Initial"));

FNiagaraParameterHandle::FNiagaraParameterHandle() 
{
}

FNiagaraParameterHandle::FNiagaraParameterHandle(const FString& InParameterHandleString)
{
	ParameterHandleString = InParameterHandleString;
	int32 DotIndex;
	if (ParameterHandleString.FindChar(TEXT('.'), DotIndex))
	{
		Name = ParameterHandleString.RightChop(DotIndex + 1);
		Namespace = ParameterHandleString.Left(DotIndex);
	}
	else
	{
		Name = ParameterHandleString;
	}
}

FNiagaraParameterHandle::FNiagaraParameterHandle(const FString& InNamespace, const FString& InName)
{
	Namespace = InNamespace;
	Name = InName;
	ParameterHandleString = Namespace + "." + Name;
}

bool FNiagaraParameterHandle::operator==(const FNiagaraParameterHandle& Other) const
{
	return ParameterHandleString == Other.ParameterHandleString;
}

FNiagaraParameterHandle FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(const FNiagaraParameterHandle& ModuleParameterHandle, UNiagaraNodeFunctionCall* ModuleNode)
{
	return ModuleParameterHandle.IsModuleHandle()
		? FNiagaraParameterHandle(ModuleNode->GetFunctionName(), ModuleParameterHandle.GetName())
		: ModuleParameterHandle;
}

FNiagaraParameterHandle FNiagaraParameterHandle::CreateEngineParameterHandle(const FNiagaraVariable& SystemVariable)
{
	return FNiagaraParameterHandle(SystemVariable.GetName().ToString());
}

FNiagaraParameterHandle FNiagaraParameterHandle::CreateEmitterParameterHandle(const FNiagaraVariable& EmitterVariable)
{

	return FNiagaraParameterHandle(EmitterNamespace, EmitterVariable.GetName().ToString());
}

FNiagaraParameterHandle FNiagaraParameterHandle::CreateParticleAttributeParameterHandle(const FString& InName)
{
	return FNiagaraParameterHandle(ParticleAttributeNamespace, InName);
}

FNiagaraParameterHandle FNiagaraParameterHandle::CreateModuleParameterHandle(const FString& InName)
{
	return FNiagaraParameterHandle(ModuleNamespace, InName);
}

FNiagaraParameterHandle FNiagaraParameterHandle::CreateInitialParameterHandle(const FNiagaraParameterHandle& Handle)
{
	return FNiagaraParameterHandle(Handle.GetNamespace(), InitialPrefix + TEXT(".") + Handle.GetName());
}

bool FNiagaraParameterHandle::IsValid() const 
{
	return ParameterHandleString.IsEmpty() == false;
}

const FString& FNiagaraParameterHandle::GetParameterHandleString() const 
{
	return ParameterHandleString;
}

const FString& FNiagaraParameterHandle::GetName() const 
{
	return Name;
}

const FString& FNiagaraParameterHandle::GetNamespace() const
{
	return Namespace;
}

bool FNiagaraParameterHandle::IsEngineHandle() const
{
	return Namespace == EngineNamespace;
}

bool FNiagaraParameterHandle::IsSystemHandle() const
{
	return Namespace == SystemNamespace;
}

bool FNiagaraParameterHandle::IsEmitterHandle() const
{
	return Namespace == EmitterNamespace;
}

bool FNiagaraParameterHandle::IsParticleAttributeHandle() const
{
	return Namespace == ParticleAttributeNamespace;
}

bool FNiagaraParameterHandle::IsModuleHandle() const
{
	return Namespace == ModuleNamespace;
}