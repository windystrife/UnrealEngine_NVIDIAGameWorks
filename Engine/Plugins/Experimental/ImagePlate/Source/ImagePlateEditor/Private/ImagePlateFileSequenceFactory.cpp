// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ImagePlateFileSequenceFactory.h"
#include "ImagePlateFileSequence.h"

#define LOCTEXT_NAMESPACE "ImagePlateFileSequenceFactory"

UImagePlateFileSequenceFactory::UImagePlateFileSequenceFactory(const FObjectInitializer& ObjectInitializer)
 : Super(ObjectInitializer)
{
	bCreateNew = true;
	SupportedClass = UImagePlateFileSequence::StaticClass();
}

FText UImagePlateFileSequenceFactory::GetDisplayName() const
{
	return LOCTEXT("ImagePlateFileSequenceFactoryDescription", "Image Plate File Sequence");
}

UObject* UImagePlateFileSequenceFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	return NewObject<UImagePlateFileSequence>(InParent, Name, Flags);
}

#undef LOCTEXT_NAMESPACE
