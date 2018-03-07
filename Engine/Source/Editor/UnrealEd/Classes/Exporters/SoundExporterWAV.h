// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Exporters/Exporter.h"
#include "SoundExporterWAV.generated.h"

UCLASS()
class USoundExporterWAV : public UExporter
{
public:
	GENERATED_BODY()

public:
	USoundExporterWAV(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	//~ Begin UExporter Interface
	virtual bool ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags=0 ) override;
	virtual bool SupportsObject(UObject* Object) const override;
	//~ End UExporter Interface
};


