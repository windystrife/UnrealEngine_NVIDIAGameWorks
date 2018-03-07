#include "ActorFactoryFlowGridActor.h"
#include "NvFlowEditorCommon.h"

#include "AssetTypeActions_FlowGridAsset.h"

#define LOCTEXT_NAMESPACE "ActorFactory"

// NvFlow begin
/*-----------------------------------------------------------------------------
UActorFactoryFlowGridActor
-----------------------------------------------------------------------------*/
UActorFactoryFlowGridActor::UActorFactoryFlowGridActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("FlowGridActorDisplayName", "Flow Grid Actor");
	NewActorClass = AFlowGridActor::StaticClass();
}

bool UActorFactoryFlowGridActor::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UFlowGridAsset::StaticClass()))
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoFlowGridAsset", "No Flow Grid Asset was specified.");
		return false;
	}

	return true;
}

void UActorFactoryFlowGridActor::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	UFlowGridAsset* FlowGridAsset = CastChecked<UFlowGridAsset>(Asset);
	AFlowGridActor* FlowGridActor = CastChecked<AFlowGridActor>(NewActor);

	if (FlowGridActor && FlowGridActor->FlowGridComponent)
	{
		FlowGridActor->FlowGridComponent->FlowGridAsset = FlowGridAsset;
		FlowGridActor->PostEditChange();
	}
}
// NvFlow end

#undef LOCTEXT_NAMESPACE