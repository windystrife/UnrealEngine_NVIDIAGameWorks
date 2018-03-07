// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Styling/SlateColor.h"

class IDetailCategoryBuilder;
class IPropertyHandle;
enum class ECheckBoxState : uint8;

// Helper class to create a Mobility customization for the specified Property in the specified CategoryBuilder.
class DETAILCUSTOMIZATIONS_API FMobilityCustomization : public TSharedFromThis<FMobilityCustomization>
{
public:
	enum
	{
		StaticMobilityBitMask = (1u << EComponentMobility::Static),
		StationaryMobilityBitMask = (1u << EComponentMobility::Stationary),
		MovableMobilityBitMask = (1u << EComponentMobility::Movable),
	};

	FMobilityCustomization()
	{}

	void CreateMobilityCustomization(IDetailCategoryBuilder& InCategoryBuilder, TSharedPtr<IPropertyHandle> InMobilityHandle, uint8 InRestrictedMobilityBits, bool bForLight);

private:

	ECheckBoxState IsMobilityActive(EComponentMobility::Type InMobility) const;
	FSlateColor GetMobilityTextColor(EComponentMobility::Type InMobility) const;
	void OnMobilityChanged(ECheckBoxState InCheckedState, EComponentMobility::Type InMobility);
	FText GetMobilityToolTip() const;
	
	TSharedPtr<IPropertyHandle> MobilityHandle;
};
