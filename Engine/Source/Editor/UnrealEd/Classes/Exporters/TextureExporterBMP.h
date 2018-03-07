// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// TextureExporterBMP
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Exporters/Exporter.h"
#include "TextureExporterBMP.generated.h"

UCLASS()
class UTextureExporterBMP : public UExporter
{
	GENERATED_UCLASS_BODY()

	virtual bool SupportsObject(UObject* Object) const override;
	virtual bool ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags=0 ) override;
};



