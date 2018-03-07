// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"
#include "Widgets/Layout/SScaleBox.h"
#include "ScaleBox.generated.h"

/**
 * Allows you to place content with a desired size and have it scale to meet the constraints placed on this box's alloted area.  If
 * you needed to have a background image scale to fill an area but not become distorted with different aspect ratios, or if you need
 * to auto fit some text to an area, this is the control for you.
 *
 * * Single Child
 * * Aspect Ratio
 */
UCLASS(config=Engine)
class UMG_API UScaleBox : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** The stretching rule to apply when content is stretched */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stretching")
	TEnumAsByte<EStretch::Type> Stretch;

	/** Controls in what direction content can be scaled */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stretching")
	TEnumAsByte<EStretchDirection::Type> StretchDirection;

	/** Optional scale that can be specified by the User. Used only for UserSpecified stretching. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stretching")
	float UserSpecifiedScale;

	/** Optional bool to ignore the inherited scale. Applies inverse scaling to counteract parents before applying the local scale operation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stretching")
	bool IgnoreInheritedScale;

	/**
	 * Only perform a single layout pass, if you do this, it can save a considerable
	 * amount of time, however, some things like text may not look correct.  You may also
	 * see the UI judder between frames.  This generally is caused by not explicitly
	 * sizing the widget, and instead allowing it to layout based on desired size along
	 * which won't work in Single Layout Pass mode.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Performance")
	bool bSingleLayoutPass;

public:
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetStretch(EStretch::Type InStretch);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetStretchDirection(EStretchDirection::Type InStretchDirection);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetUserSpecifiedScale(float InUserSpecifiedScale);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetIgnoreInheritedScale(bool bInIgnoreInheritedScale);
public:

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

	virtual void Serialize(FArchive& Ar) override;

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	TSharedPtr<SScaleBox> MyScaleBox;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};
