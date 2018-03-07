#include "FlowRenderMaterialFactory.h"
#include "NvFlowEditorCommon.h"

// NvFlow begin
/*-----------------------------------------------------------------------------
UFlowRenderMaterialFactory.
-----------------------------------------------------------------------------*/
UFlowRenderMaterialFactory::UFlowRenderMaterialFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UFlowRenderMaterial::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UFlowRenderMaterialFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UFlowRenderMaterial* FlowRenderMaterial = NewObject<UFlowRenderMaterial>(InParent, InName, Flags);

	return FlowRenderMaterial;
}
// NvFlow end
