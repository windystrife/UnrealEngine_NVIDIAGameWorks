// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Brush.h"
#include "Volume.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVolume, Log, All);

/** 
 *	An editable 3D volume placed in a level. Different types of volumes perform different functions
 *	@see https://docs.unrealengine.com/latest/INT/Engine/Actors/Volumes
 */
UCLASS(showcategories=Collision, hidecategories=(Brush, Physics), abstract, ConversionRoot)
class ENGINE_API AVolume : public ABrush
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	/** Delegate used for notifications when a volumes initial shape changes */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnVolumeShapeChanged, AVolume& )

	/** Function to get the 'Volume imported' delegate */
	static FOnVolumeShapeChanged& GetOnVolumeShapeChangedDelegate()
	{
		return OnVolumeShapeChanged;
	}
private:
	/** Called during posteditchange after the volume's initial shape has changed */
	static FOnVolumeShapeChanged OnVolumeShapeChanged;
public:
	//~ Begin AActor Interface
	/**
	* Function that gets called from within Map_Check to allow this actor to check itself
	* for any potential errors and register them with map check dialog.
	*/
	virtual void CheckForErrors() override;
#endif // WITH_EDITOR
	
	virtual bool IsLevelBoundsRelevant() const override;
	//~ End AActor Interface

	//~ Begin Brush Interface
	virtual bool IsStaticBrush() const override;
	virtual bool IsVolumeBrush() const override;
	//~ End Brush Interface

	/** @returns true if a sphere/point (with optional radius CheckRadius) overlaps this volume */
	bool EncompassesPoint(FVector Point, float SphereRadius=0.f, float* OutDistanceToPoint = 0) const;

	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//End UObject Interface


};



