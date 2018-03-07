// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterBindingInstance.h"
#include "NiagaraTypes.h"

FNiagaraParameterBindingInstance::FNiagaraParameterBindingInstance(const FNiagaraVariable& InSource, FNiagaraVariable& InDestination)
	: Source(InSource)
	, Destination(InDestination)
{
}

void FNiagaraParameterBindingInstance::Tick()
{
	Source.CopyTo(Destination.GetData());
}