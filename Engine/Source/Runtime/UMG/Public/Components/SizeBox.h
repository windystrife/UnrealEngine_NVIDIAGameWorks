// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"
#include "SizeBox.generated.h"

class SBox;

/**
 * A widget that allows you to specify the size it reports to have and desire.  Not all widgets report a desired size
 * that you actually desire.  Wrapping them in a SizeBox lets you have the Size Box force them to be a particular size.
 *
 * * Single Child
 * * Fixed Size
 */
UCLASS()
class UMG_API USizeBox : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	/**  */
	UPROPERTY(EditAnywhere, Category="Child Layout", meta=(InlineEditConditionToggle))
	uint32 bOverride_WidthOverride : 1;

	/**  */
	UPROPERTY(EditAnywhere, Category="Child Layout", meta=(InlineEditConditionToggle))
	uint32 bOverride_HeightOverride : 1;

	/**  */
	UPROPERTY(EditAnywhere, Category="Child Layout", meta=(InlineEditConditionToggle))
	uint32 bOverride_MinDesiredWidth : 1;

	/**  */
	UPROPERTY(EditAnywhere, Category="Child Layout", meta=(InlineEditConditionToggle))
	uint32 bOverride_MinDesiredHeight : 1;

	/**  */
	UPROPERTY(EditAnywhere, Category="Child Layout", meta=(InlineEditConditionToggle))
	uint32 bOverride_MaxDesiredWidth : 1;

	/**  */
	UPROPERTY(EditAnywhere, Category="Child Layout", meta=(InlineEditConditionToggle))
	uint32 bOverride_MaxDesiredHeight : 1;

	/**  */
	UPROPERTY(EditAnywhere, Category="Child Layout", meta=(InlineEditConditionToggle))
	uint32 bOverride_MaxAspectRatio : 1;


	/** When specified, ignore the content's desired size and report the WidthOverride as the Box's desired width. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout", meta=( editcondition="bOverride_WidthOverride" ))
	float WidthOverride;

	/** When specified, ignore the content's desired size and report the HeightOverride as the Box's desired height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout", meta=( editcondition="bOverride_HeightOverride" ))
	float HeightOverride;

	/** When specified, will report the MinDesiredWidth if larger than the content's desired width. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout", meta=( editcondition="bOverride_MinDesiredWidth" ))
	float MinDesiredWidth;

	/** When specified, will report the MinDesiredHeight if larger than the content's desired height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout", meta=( editcondition="bOverride_MinDesiredHeight" ))
	float MinDesiredHeight;

	/** When specified, will report the MaxDesiredWidth if smaller than the content's desired width. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout", meta=( editcondition="bOverride_MaxDesiredWidth" ))
	float MaxDesiredWidth;

	/** When specified, will report the MaxDesiredHeight if smaller than the content's desired height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout", meta=( editcondition="bOverride_MaxDesiredHeight" ))
	float MaxDesiredHeight;

	/** */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout", meta=( editcondition="bOverride_MaxAspectRatio" ))
	float MaxAspectRatio;

public:
		
	/** When specified, ignore the content's desired size and report the WidthOverride as the Box's desired width. */
	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void SetWidthOverride(float InWidthOverride);

	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void ClearWidthOverride();

	/** When specified, ignore the content's desired size and report the HeightOverride as the Box's desired height. */
	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void SetHeightOverride(float InHeightOverride);

	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void ClearHeightOverride();

	/** When specified, will report the MinDesiredWidth if larger than the content's desired width. */
	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void SetMinDesiredWidth(float InMinDesiredWidth);

	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void ClearMinDesiredWidth();

	/** When specified, will report the MinDesiredHeight if larger than the content's desired height. */
	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void SetMinDesiredHeight(float InMinDesiredHeight);

	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void ClearMinDesiredHeight();

	/** When specified, will report the MaxDesiredWidth if smaller than the content's desired width. */
	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void SetMaxDesiredWidth(float InMaxDesiredWidth);

	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void ClearMaxDesiredWidth();

	/** When specified, will report the MaxDesiredHeight if smaller than the content's desired height. */
	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void SetMaxDesiredHeight(float InMaxDesiredHeight);

	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void ClearMaxDesiredHeight();

	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void SetMaxAspectRatio(float InMaxAspectRatio);

	UFUNCTION(BlueprintCallable, Category="Layout|Size Box")
	void ClearMaxAspectRatio();

public:

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	TSharedPtr<SBox> MySizeBox;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};
