// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialInstanceConstant.cpp: MaterialInstanceConstant implementation.
=============================================================================*/

#include "Materials/MaterialInstanceConstant.h"


UMaterialInstanceConstant::UMaterialInstanceConstant(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMaterialInstanceConstant::PostLoad()
{
	LLM_SCOPE(ELLMTag::Materials);
	Super::PostLoad();
}

#if WITH_EDITOR
void UMaterialInstanceConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ParameterStateId = FGuid::NewGuid();
}

void UMaterialInstanceConstant::SetParentEditorOnly(UMaterialInterface* NewParent)
{
	check(GIsEditor);
	SetParentInternal(NewParent, true);
}

void UMaterialInstanceConstant::SetVectorParameterValueEditorOnly(FName ParameterName, FLinearColor Value)
{
	check(GIsEditor);
	SetVectorParameterValueInternal(ParameterName,Value);
}

void UMaterialInstanceConstant::SetScalarParameterValueEditorOnly(FName ParameterName, float Value)
{
	check(GIsEditor);
	SetScalarParameterValueInternal(ParameterName,Value);
}

void UMaterialInstanceConstant::SetTextureParameterValueEditorOnly(FName ParameterName, UTexture* Value)
{
	check(GIsEditor);
	SetTextureParameterValueInternal(ParameterName,Value);
}

void UMaterialInstanceConstant::SetFontParameterValueEditorOnly(FName ParameterName,class UFont* FontValue,int32 FontPage)
{
	check(GIsEditor);
	SetFontParameterValueInternal(ParameterName,FontValue,FontPage);
}

void UMaterialInstanceConstant::ClearParameterValuesEditorOnly()
{
	check(GIsEditor);
	ClearParameterValuesInternal();
}
#endif // #if WITH_EDITOR
