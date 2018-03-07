// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "LandscapeProxy.h"


#include "Landscape.generated.h"

class ULandscapeComponent;

UENUM()
enum ELandscapeSetupErrors
{
	LSE_None,
	/** No Landscape Info available. */
	LSE_NoLandscapeInfo,
	/** There was already component with same X,Y. */
	LSE_CollsionXY,
	/** No Layer Info, need to add proper layers. */
	LSE_NoLayerInfo,
	LSE_MAX,
};

UCLASS(MinimalAPI, showcategories=(Display, Movement, Collision, Lighting, LOD, Input))
class ALandscape : public ALandscapeProxy
{
	GENERATED_BODY()

public:
	ALandscape(const FObjectInitializer& ObjectInitializer);

	//~ Begin ALandscapeProxy Interface
	LANDSCAPE_API virtual ALandscape* GetLandscapeActor() override;
#if WITH_EDITOR
	//~ End ALandscapeProxy Interface

	LANDSCAPE_API bool HasAllComponent(); // determine all component is in this actor
	
	// Include Components with overlapped vertices
	// X2/Y2 Coordinates are "inclusive" max values
	LANDSCAPE_API static void CalcComponentIndicesOverlap(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const int32 ComponentSizeQuads, 
		int32& ComponentIndexX1, int32& ComponentIndexY1, int32& ComponentIndexX2, int32& ComponentIndexY2);

	// Exclude Components with overlapped vertices
	// X2/Y2 Coordinates are "inclusive" max values
	LANDSCAPE_API static void CalcComponentIndicesNoOverlap(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const int32 ComponentSizeQuads,
		int32& ComponentIndexX1, int32& ComponentIndexY1, int32& ComponentIndexX2, int32& ComponentIndexY2);

	static void SplitHeightmap(ULandscapeComponent* Comp, bool bMoveToCurrentLevel = false);
	
	//~ Begin UObject Interface.
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual bool ShouldImport(FString* ActorPropString, bool IsMovingLevel) override;
	virtual void PostEditImport() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif
	virtual void PostLoad() override;
	//~ End UObject Interface
};
