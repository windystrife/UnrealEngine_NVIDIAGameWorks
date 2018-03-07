// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "CircularThrobber.generated.h"

class USlateBrushAsset;

/**
 * A throbber widget that orients images in a spinning circle.
 * 
 * * No Children
 * * Spinner Progress
 */
UCLASS()
class UMG_API UCircularThrobber : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** How many pieces there are */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( ClampMin="1", ClampMax="25", UIMin="1", UIMax="25" ))
	int32 NumberOfPieces;

	/** The amount of time for a full circle (in seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( ClampMin="0", UIMin="0" ))
	float Period;

	/** The radius of the circle. If the throbber is a child of Canvas Panel, the 'Size to Content' option must be enabled in order to set Radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=(EditCondition="bEnableRadius"))
	float Radius;

	/** Image to use for each segment of the throbber */
	UPROPERTY()
	USlateBrushAsset* PieceImage_DEPRECATED;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush Image;

public:

	/** Sets how many pieces there are. */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetNumberOfPieces(int32 InNumberOfPieces);

	/** Sets the amount of time for a full circle (in seconds). */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetPeriod(float InPeriod);

	/** Sets the radius of the circle. */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetRadius(float InRadius);

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

private:

	UPROPERTY(Transient, EditAnywhere, Category="Appearance", meta=(InlineEditConditionToggle))
	bool bEnableRadius;

	/** The CircularThrobber widget managed by this object. */
	TSharedPtr<SCircularThrobber> MyCircularThrobber;
};
