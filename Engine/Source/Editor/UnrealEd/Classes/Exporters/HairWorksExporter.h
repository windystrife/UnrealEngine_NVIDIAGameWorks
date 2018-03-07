// @third party code - BEGIN HairWorks
//~=============================================================================
// HairWorksExporter
//~=============================================================================

#pragma once

#include "Exporters/Exporter.h"
#include "HairWorksExporter.generated.h"

UCLASS()
class UHairWorksExporter : public UExporter
{
	GENERATED_UCLASS_BODY()


	//~ Begin UExporter Interface
	virtual bool ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags=0 ) override;
	//~ End UExporter Interface
};
// @third party code - END HairWorks