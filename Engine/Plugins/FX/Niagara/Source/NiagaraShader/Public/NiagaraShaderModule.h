// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
* Niagara shader module interface
*/
class INiagaraShaderModule : public IModuleInterface
{
public:
	DECLARE_DELEGATE_RetVal(void, FOnProcessQueue);
	FDelegateHandle NIAGARASHADER_API SetOnProcessShaderCompilationQueue(FOnProcessQueue InOnProcessQueue);
	void NIAGARASHADER_API ResetOnProcessShaderCompilationQueue(FDelegateHandle DelegateHandle);
	void ProcessShaderCompilationQueue();

private:
	FOnProcessQueue OnProcessQueue;
};

