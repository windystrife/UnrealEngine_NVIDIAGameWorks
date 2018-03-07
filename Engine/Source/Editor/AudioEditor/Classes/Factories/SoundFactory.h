// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// SoundFactory
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "SoundFactory.generated.h"

UCLASS(MinimalAPI, hidecategories=Object)
class USoundFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	/** If enabled, a sound cue will automatically be created for the sound */
	UPROPERTY(EditAnywhere, Category=SoundFactory, meta=(ToolTip="If enabled, a sound cue will automatically be created for the sound"))
	uint32 bAutoCreateCue:1;

	/** If enabled, the created sound cue will include a attenuation node */
	UPROPERTY(EditAnywhere, Category=SoundFactory, meta=(ToolTip="If enabled, the created sound cue will include a attenuation node"))
	uint32 bIncludeAttenuationNode:1;

	/** If enabled, the created sound cue will include a looping node */
	UPROPERTY(EditAnywhere, Category=SoundFactory, meta=(ToolTip="If enabled, the created sound cue will include a looping node"))
	uint32 bIncludeLoopingNode:1;

	/** If enabled, the created sound cue will include a modulator node */
	UPROPERTY(EditAnywhere, Category=SoundFactory, meta=(ToolTip="If enabled, the created sound cue will include a modulator node"))
	uint32 bIncludeModulatorNode:1;

	/** The volume of the created sound cue */
	UPROPERTY(EditAnywhere, Category=SoundFactory, meta=(ToolTip="The volume of the created sound cue"))
	float CueVolume;

	/** If not empty, imported waves will be placed in PackageCuePackageSuffix, but only if bAutoCreateCue is true. */
	UPROPERTY(EditAnywhere, Category=SoundFactory, meta=(ToolTip="If not empty, generated SoundCues will be placed in PackageCuePackageSuffix, but only if bAutoCreateCue is true"))
	FString CuePackageSuffix;


	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn ) override;
	//~ End UFactory Interface
	
	/** Suppresses the import overwrite dialog until one iteration of FactoryCreateBinary completes; this is primarily used for reimporting sounds */
	AUDIOEDITOR_API static void SuppressImportOverwriteDialog();

private:
	/** If true, the overwrite dialog should not be shown while importing */
	static bool bSuppressImportOverwriteDialog;
};
