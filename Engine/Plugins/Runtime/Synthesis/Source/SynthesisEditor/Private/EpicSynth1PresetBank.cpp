// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EpicSynth1PresetBank.h"
#include "SynthComponents/EpicSynth1Component.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

UClass* FAssetTypeActions_ModularSynthPresetBank::GetSupportedClass() const
{
	return UModularSynthPresetBank::StaticClass();
}

UModularSynthPresetBankFactory::UModularSynthPresetBankFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UModularSynthPresetBank::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UModularSynthPresetBankFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UModularSynthPresetBank* NewPresetBank = NewObject<UModularSynthPresetBank>(InParent, InName, Flags);

	return NewPresetBank;
}