#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "FixConflictingLocalizationKeys.generated.h"

/**
 * Internal commandlet to fix any conflicting localization keys that are found in a manifest.
 * @note Hard-coded to work with the "Game" localization target.
 */
UCLASS()
class UFixConflictingLocalizationKeysCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	virtual int32 Main(const FString& Params);
};
