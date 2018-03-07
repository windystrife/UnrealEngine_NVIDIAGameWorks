// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// SequenceExporterT3D
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Exporters/Exporter.h"
#include "SequenceExporterT3D.generated.h"

class FExportObjectInnerContext;

UCLASS()
class USequenceExporterT3D : public UExporter
{
public:
	GENERATED_BODY()

public:
	USequenceExporterT3D(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	//~ Begin UExporter Interface
	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) override;
	//~ End UExporter Interface
};

