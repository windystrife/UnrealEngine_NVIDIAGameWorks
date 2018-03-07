
//=============================================================================
// FlowMaterialFactory
//=============================================================================
#pragma once

// NvFlow begin

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"

#include "FlowRenderMaterialFactory.generated.h"

UCLASS(hidecategories=Object)
class UFlowRenderMaterialFactory : public UFactory
{
	GENERATED_UCLASS_BODY()


	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	// Begin UFactory Interface	
};

// NvFlow end
