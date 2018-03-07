// @third party code - BEGIN HairWorks
#include "Engine/HairWorksAsset.h"
#include <Nv/Common/NvCoMemoryReadStream.h>
#include "EditorFramework/AssetImportData.h"
#include "UObjectIterator.h"
#include "HairWorksSDK.h"
#include "Engine/HairWorksMaterial.h"
#include "Components/HairWorksComponent.h"

UHairWorksAsset::UHairWorksAsset(const class FObjectInitializer& ObjectInitializer):
	Super(ObjectInitializer),
	AssetId(NvHair::ASSET_ID_NULL)
{
}

UHairWorksAsset::~UHairWorksAsset()
{
	if(AssetId != NvHair::ASSET_ID_NULL)
		::HairWorks::GetSDK()->freeAsset(AssetId);
}

void UHairWorksAsset::Serialize(FArchive & Ar)
{
	Super::Serialize(Ar);
}

void UHairWorksAsset::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif

	if(!HasAnyFlags(RF_NeedLoad))
		HairMaterial = NewObject<UHairWorksMaterial>(this, NAME_None, GetMaskedFlags(RF_PropagateToSubObjects));

	Super::PostInitProperties();
}

void UHairWorksAsset::PostLoad()
{
	Super::PostLoad();

	// Preload asset
	if(::HairWorks::GetSDK() == nullptr)
		return;

	auto& HairSdk = *::HairWorks::GetSDK();

	check(AssetId == NvHair::ASSET_ID_NULL);
	NvCo::MemoryReadStream ReadStream(AssetData.GetData(), AssetData.Num());
	HairSdk.loadAsset(&ReadStream, AssetId, nullptr, &::HairWorks::GetAssetConversionSettings());

	// Initialize pins
	if(AssetId != NvHair::ASSET_ID_NULL && HairMaterial->Pins.Num() == 0)
		InitPins();

	// Setup bone lookup table
	InitBoneLookupTable();
}

#if WITH_EDITOR
void UHairWorksAsset::PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent)
{
	// Let HairWorks components to update their rendering data
	for(TObjectIterator<UHairWorksComponent> It; It; ++It)
	{
		if(It->HairInstance.Hair == this)
			It->MarkRenderDynamicDataDirty();
	}

	// Call parent
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UHairWorksAsset::InitPins() const
{
	// Empty engine pins
	if (::HairWorks::GetSDK() == nullptr || AssetId == NvHair::ASSET_ID_NULL)
		return;

	HairMaterial->Pins.Empty();

	// Get pins
	auto& HairSdk = *::HairWorks::GetSDK();
	TArray<NvHair::Pin> Pins;
	Pins.AddDefaulted(HairSdk.getNumPins(AssetId));
	if(Pins.Num() == 0)
		return;

	HairSdk.getPins(AssetId, 0, Pins.Num(), Pins.GetData());

	// Add pin to engine pins
	for(const auto& Pin : Pins)
	{
		FHairWorksPin EnginePin;
		EnginePin.Bone = BoneNames[Pin.m_boneIndex];
		EnginePin.bDynamicPin = Pin.m_useDynamicPin;
		EnginePin.bTetherPin = Pin.m_doLra;
		EnginePin.Stiffness = Pin.m_pinStiffness;
		EnginePin.InfluenceFallOff = Pin.m_influenceFallOff;
		EnginePin.InfluenceFallOffCurve = reinterpret_cast<const FVector4&>(Pin.m_influenceFallOffCurve);

		HairMaterial->Pins.Add(EnginePin);
	}
}
void UHairWorksAsset::InitBoneLookupTable()
{
	BoneNameToIdx.Empty(BoneNames.Num());
	for(auto Idx = 0; Idx < BoneNames.Num(); ++Idx)
	{
		BoneNameToIdx.Add(BoneNames[Idx], Idx);
	}
}
// @third party code - END HairWorks
