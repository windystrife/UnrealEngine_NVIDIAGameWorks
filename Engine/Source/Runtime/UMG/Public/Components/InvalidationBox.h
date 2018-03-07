// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"
#include "InvalidationBox.generated.h"

class SInvalidationPanel;

/**
 * Invalidate
 * * Single Child
 * * Caching / Performance
 */
UCLASS()
class UMG_API UInvalidationBox : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category="Invalidation Box")
	void InvalidateCache();

	/**
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category="Invalidation Box")
	bool GetCanCache() const;

	/**
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category="Invalidation Box")
	void SetCanCache(bool CanCache);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	// UPanelWidget
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

protected:
	/**
	 * Should the invalidation panel cache the widgets?  Making this false makes it so the invalidation
	 * panel stops acting like an invalidation panel, just becomes a simple container widget.
	 */
	UPROPERTY(EditAnywhere, Category="Caching")
	bool bCanCache;

	/**
	 * Caches the locations for child draw elements relative to the invalidation box,
	 * this adds extra overhead to drawing them every frame.  However, in cases where
	 * the position of the invalidation boxes changes every frame this can be a big savings.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caching")
	bool CacheRelativeTransforms;

protected:
	TSharedPtr<class SInvalidationPanel> MyInvalidationPanel;
};
