// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Components/PrimitiveComponent.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "AI/Navigation/NavLinkHostInterface.h"
#include "NavLinkComponent.generated.h"

class FPrimitiveSceneProxy;
struct FNavigationRelevantData;

UCLASS(ClassGroup = (Navigation), meta = (BlueprintSpawnableComponent), hidecategories = (Activation))
class ENGINE_API UNavLinkComponent : public UPrimitiveComponent, public INavLinkHostInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Navigation)
	TArray<FNavigationLink> Links;

	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual bool IsNavigationRelevant() const override;

	virtual bool GetNavigationLinksClasses(TArray<TSubclassOf<class UNavLinkDefinition> >& OutClasses) const override { return false; }
	virtual bool GetNavigationLinksArray(TArray<FNavigationLink>& OutLink, TArray<FNavigationSegmentLink>& OutSegments) const override;

	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override { return true; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
#endif // WITH_EDITOR
};
