#include "FlowGridAssetFactory.h"
#include "NvFlowEditorCommon.h"

// NvFlow begin
/*-----------------------------------------------------------------------------
UFlowGridAssetFactory.
-----------------------------------------------------------------------------*/
UFlowGridAssetFactory::UFlowGridAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SupportedClass = UFlowGridAsset::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UFlowGridAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UFlowGridAsset* FlowGridAsset = NewObject<UFlowGridAsset>(InParent, InName, Flags);

	return FlowGridAsset;
}
// NvFlow end