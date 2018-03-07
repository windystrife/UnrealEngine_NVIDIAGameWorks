// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STileView.h"
#include "Components/TableViewBase.h"
#include "TileView.generated.h"

/**
 * A flow panel that presents the contents as a set of tiles all uniformly sized.
 */
UCLASS(Experimental)
class UMG_API UTileView : public UTableViewBase
{
	GENERATED_UCLASS_BODY()

public:

	/**  */
	UPROPERTY(EditAnywhere, Category=Appearance)
	float ItemWidth;

	/**  */
	UPROPERTY(EditAnywhere, Category=Appearance)
	float ItemHeight;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Content)
	TArray<UObject*> Items;

	/**  */
	UPROPERTY(EditAnywhere, Category=Content)
	TEnumAsByte<ESelectionMode::Type> SelectionMode;

	/**  */
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnGenerateRowUObject OnGenerateTileEvent;

public:

	/** Set item width */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetItemWidth(float Width);

	/** Set item height */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetItemHeight(float Height);

	/** Refreshes the list */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	void RequestListRefresh();

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	TSharedRef<ITableRow> HandleOnGenerateTile(UObject* Item, const TSharedRef< STableViewBase >& OwnerTable) const;

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

protected:
	TSharedPtr< STileView< UObject* > > MyTileView;
};
