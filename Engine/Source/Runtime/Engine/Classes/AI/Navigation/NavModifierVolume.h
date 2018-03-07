// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "GameFramework/Volume.h"
#include "NavModifierVolume.generated.h"

struct FNavigationRelevantData;

/** 
 *	Allows applying selected AreaClass to navmesh, using Volume's shape
 */
UCLASS(hidecategories=(Navigation))
class ENGINE_API ANavModifierVolume : public AVolume, public INavRelevantInterface
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Default)
	TSubclassOf<UNavArea> AreaClass;

public:
	ANavModifierVolume(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void SetAreaClass(TSubclassOf<UNavArea> NewAreaClass = nullptr);

	TSubclassOf<UNavArea> GetAreaClass() const { return AreaClass; }

	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual FBox GetNavigationBounds() const override;
	virtual void RebuildNavigationData() override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
