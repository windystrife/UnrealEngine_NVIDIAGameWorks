//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "PhononMaterial.h"
#include "Components/ActorComponent.h"
#include "PhononMaterialComponent.generated.h"

/**
 * Phonon Material components are used to customize an actor's acoustic properties. Only valid on actors that also
 * have a Phonon Geometry component.
 */
UCLASS(ClassGroup = (Audio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class UPhononMaterialComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPhononMaterialComponent();

	IPLMaterial GetMaterialPreset() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

	UPROPERTY()
	int32 MaterialIndex;

	// Choose from a variety of preset physical materials, or choose Custom to specify values manually.
	UPROPERTY(EditAnywhere, Category = Settings)
	EPhononMaterial MaterialPreset;

	// How much this material absorbs low frequency sound.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LowFreqAbsorption;

	// How much this material absorbs mid frequency sound.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MidFreqAbsorption;

	// How much this material absorbs high frequency sound.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float HighFreqAbsorption;

	// How much this material transmits low frequency sound.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LowFreqTransmission;

	// How much this material transmits mid frequency sound.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MidFreqTransmission;

	// How much this material transmits high frequency sound.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float HighFreqTransmission;

	// Specifies how "rough" the surface is. Surfaces with a high scattering value randomly reflect sound in all directions;
	// surfaces with a low scattering value reflect sound in a mirror-like manner.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Scattering;
};