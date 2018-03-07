// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FNiagaraVariable;

/** A runtime instance of an System parameter binding. */
class FNiagaraParameterBindingInstance
{
public:
	FNiagaraParameterBindingInstance(const FNiagaraVariable& InSource, FNiagaraVariable& InDestination);

	/** Updates the paramter binding each frame. */
	void Tick();

private:
	/** The source variable for the binding. */
	const FNiagaraVariable& Source;

	/** The destination variable for the binding. */
	FNiagaraVariable& Destination;
};