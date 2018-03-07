// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// StaticMeshExporterOBJ
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Exporters/Exporter.h"
#include "StaticMeshExporterOBJ.generated.h"

class FExportObjectInnerContext;

UCLASS()
class UStaticMeshExporterOBJ : public UExporter
{
	GENERATED_UCLASS_BODY()


	//~ Begin UExporter Interface
	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) override;
	//~ End UExporter Interface
};



