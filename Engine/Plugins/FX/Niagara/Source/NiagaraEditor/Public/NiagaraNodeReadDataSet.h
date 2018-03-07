// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraCommon.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraNodeDataSetBase.h"
#include "NiagaraNodeReadDataSet.generated.h"

UCLASS(MinimalAPI)
class UNiagaraNodeReadDataSet : public UNiagaraNodeDataSetBase
{
	GENERATED_UCLASS_BODY()

public:

	//~ Begin EdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End EdGraphNode Interface

	//~Begin UNiagaraGraph Interface
	virtual bool CanAddToGraph(UNiagaraGraph* TargetGraph, FString& OutErrorMsg) const override;

	//~End UNiagaraGraph Interface
	virtual void Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)override;

};

