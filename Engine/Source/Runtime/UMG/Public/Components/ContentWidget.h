// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PanelWidget.h"
#include "ContentWidget.generated.h"

/**  */
UCLASS(Abstract)
class UMG_API UContentWidget : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:
	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	UPanelSlot* GetContentSlot() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	UPanelSlot* SetContent(UWidget* Content);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	UWidget* GetContent() const;

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	// End UPanelWidget
};
