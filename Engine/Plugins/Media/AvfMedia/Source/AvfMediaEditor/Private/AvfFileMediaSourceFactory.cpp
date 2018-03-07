// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AvfFileMediaSourceFactory.h"

#include "Misc/Paths.h"
#include "FileMediaSource.h"
#include "Containers/UnrealString.h"
#include "UObject/UObjectGlobals.h"


/* UAvfFileMediaSourceFactory structors
 *****************************************************************************/

UAvfFileMediaSourceFactory::UAvfFileMediaSourceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("3g2;3G2 Multimedia Stream"));
	Formats.Add(TEXT("3gp;3GP Video Stream"));
	Formats.Add(TEXT("3gp2;3GPP2 Multimedia File"));
	Formats.Add(TEXT("3gpp;3GPP Multimedia File"));
	Formats.Add(TEXT("ac3;AC-3 Audio File"));
	Formats.Add(TEXT("aif;Audio Interchange File"));
	Formats.Add(TEXT("aiff;Audio Interchange File"));
	Formats.Add(TEXT("amr;Adaptive Multi-Rate Audio"));
	Formats.Add(TEXT("au;Audio File"));
	Formats.Add(TEXT("bwf;Broadcast Wave Audio"));
	Formats.Add(TEXT("caf;Core Audio"));
	Formats.Add(TEXT("cdda;Compact Disc Digital Audio"));
	Formats.Add(TEXT("m4a;Apple MPEG-4 Audio"));
	Formats.Add(TEXT("m4v;Apple MPEG-4 Video"));
	Formats.Add(TEXT("mov;Apple QuickTime Movie"));
	Formats.Add(TEXT("mp3;MPEG-2 Audio"));
	Formats.Add(TEXT("mp4;MPEG-4 Movie"));
	Formats.Add(TEXT("sdv;Samsung Digital Video"));
	Formats.Add(TEXT("snd;Sound File"));
	Formats.Add(TEXT("wav;Wave Audio File"));
	Formats.Add(TEXT("wave;Wave Audio File"));

	SupportedClass = UFileMediaSource::StaticClass();
	bEditorImport = true;
}


/* UFactory overrides
 *****************************************************************************/

bool UAvfFileMediaSourceFactory::FactoryCanImport(const FString& Filename)
{
	// @hack: disable file extensions that are used in other factories
	// @todo gmp: add support for multiple factories per file extension
	const FString FileExtension = FPaths::GetExtension(Filename);

	return (FileExtension.ToUpper() != FString("WAV"));
}


UObject* UAvfFileMediaSourceFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UFileMediaSource* MediaSource = NewObject<UFileMediaSource>(InParent, InClass, InName, Flags);
	MediaSource->SetFilePath(CurrentFilename);

	return MediaSource;
}
