#include "FlowMaterialFactory.h"
#include "NvFlowEditorCommon.h"

// NvFlow begin
/*-----------------------------------------------------------------------------
UFlowMaterialFactory.
-----------------------------------------------------------------------------*/
UFlowMaterialFactory::UFlowMaterialFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UFlowMaterial::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UFlowMaterialFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UFlowMaterial* FlowMaterial = NewObject<UFlowMaterial>(InParent, InName, Flags);

	return FlowMaterial;
}
// NvFlow end
