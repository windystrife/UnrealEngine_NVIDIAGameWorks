// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImgMediaSourceFactory.h"
#include "Misc/Paths.h"
#include "ImgMediaSource.h"


/* UExrFileMediaSourceFactory structors
 *****************************************************************************/

UImgMediaSourceFactory::UImgMediaSourceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("exr;EXR Image Sequence"));

	SupportedClass = UImgMediaSource::StaticClass();
	bEditorImport = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UImgMediaSourceFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UImgMediaSource* MediaSource = NewObject<UImgMediaSource>(InParent, InClass, InName, Flags);
	MediaSource->SetSequencePath(FPaths::GetPath(CurrentFilename));

	return MediaSource;
}
