// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/SoundSourceEffectFactory.h"
#include "Sound/SoundSubmix.h"
#include "AudioDeviceManager.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "SoundFactoryUtility.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Modules/ModuleManager.h"
#include "Classes/Sound/AudioSettings.h"

#define LOCTEXT_NAMESPACE "AudioEditorFactories"

USoundSourceEffectFactory::USoundSourceEffectFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundEffectSourcePreset::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
	SoundEffectSourcepresetClass = nullptr;
}

bool USoundSourceEffectFactory::ConfigureProperties()
{
	SoundEffectSourcepresetClass = nullptr;

	// Load the classviewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	TSharedPtr<FAssetClassParentFilter> Filter = MakeShareable(new FAssetClassParentFilter);
	Options.ClassFilter = Filter;

	Filter->DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists;
	Filter->AllowedChildrenOfClasses.Add(USoundEffectSourcePreset::StaticClass());

	const FText TitleText = LOCTEXT("CreateSoundSourceEffectOptions", "Pick Source Effect Class");
	UClass* ChosenClass = nullptr;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, USoundEffectSourcePreset::StaticClass());

	if (bPressedOk)
	{
		SoundEffectSourcepresetClass = ChosenClass;
	}

	return bPressedOk;
}

UObject* USoundSourceEffectFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	USoundEffectSourcePreset* NewSoundEffectSourcePreset = nullptr;
	if (SoundEffectSourcepresetClass != nullptr)
	{
		NewSoundEffectSourcePreset = NewObject<USoundEffectSourcePreset>(InParent, SoundEffectSourcepresetClass, InName, Flags);
	}
	return NewSoundEffectSourcePreset;
}

bool USoundSourceEffectFactory::CanCreateNew() const
{
	return GetDefault<UAudioSettings>()->IsAudioMixerEnabled();
}

USoundSourceEffectChainFactory::USoundSourceEffectChainFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundEffectSourcePresetChain::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundSourceEffectChainFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<USoundEffectSourcePresetChain>(InParent, InName, Flags);
}

bool USoundSourceEffectChainFactory::CanCreateNew() const
{
	return GetDefault<UAudioSettings>()->IsAudioMixerEnabled();
}

#undef LOCTEXT_NAMESPACE