// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class FNiagaraScriptViewModel;
class INiagaraParameterCollectionViewModel;

class FNiagaraScriptDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance(TWeakPtr<FNiagaraScriptViewModel> ScriptViewModel);
	
	FNiagaraScriptDetails(TSharedPtr<FNiagaraScriptViewModel> InScriptViewModel);

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder);
private:
	TSharedPtr<FNiagaraScriptViewModel> ScriptViewModel;

};

