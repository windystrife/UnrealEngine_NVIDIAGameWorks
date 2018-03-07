#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "BlastMaterial.generated.h"


/**
Blast material. Describes destructible properties or basically how much of a damage is applied.
*/
USTRUCT()
struct BLAST_API FBlastMaterial
{
	GENERATED_USTRUCT_BODY()

	FBlastMaterial() : Health(100.f), MinDamageThreshold(0.0f), MaxDamageThreshold(1.0f), bGenerateHitEventsForLeafActors(false) {}

	// Bonds and chunks health value. Applying damage decreases health. When it decreased to zero bond will be broken.
	UPROPERTY(EditAnywhere, Category = "Blast")
	float				Health;

	// Min damage fraction threshold to be applied. Range [0, 1]. For example 0.1 filters all damage below 10% of health.
	UPROPERTY(EditAnywhere, Category = "Blast", meta = (UIMin = 0, ClampMin = 0, ClampMax = 1, UIMax = 1))
	float				MinDamageThreshold;

	// Max damage fraction threshold to be applied. Range [0, 1]. For example 0.8 won't allow more then 80% of health damage to be applied.
	UPROPERTY(EditAnywhere, Category = "Blast", meta = (UIMin = 0, ClampMin = 0, ClampMax = 1, UIMax = 1))
	float				MaxDamageThreshold;

	UPROPERTY(EditAnywhere, Category = "Blast")
	bool				bGenerateHitEventsForLeafActors;

	/**
	Helper to normalize damage.

	Pass damage defined in health, damage in range [0, 1] is returned, where 0 basically
	indicates that the threshold wasn't reached and there is no point in applying it.

	\param[in]		damageInHealth			Damage defined in terms of health amount to be reduced.

	\return normalized damage
	*/
	float GetNormalizedDamage(float damageInHealth) const
	{
		const float damage = Health > 0.f ? damageInHealth / Health : 1.0f;
		return damage > MinDamageThreshold ? (damage < MaxDamageThreshold ? damage : MaxDamageThreshold) : 0.f;
	}
};
