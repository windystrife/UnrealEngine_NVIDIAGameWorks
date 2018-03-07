// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// TextureCubeExporterHDR
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Exporters/Exporter.h"
#include "TextureCubeExporterHDR.generated.h"

UCLASS()
class UNREALED_API UTextureCubeExporterHDR : public UExporter
{
	GENERATED_UCLASS_BODY()

	//~ Begin UExporter Interface
	virtual bool ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags = 0) override;
	//~ End UExporter Interface
};



