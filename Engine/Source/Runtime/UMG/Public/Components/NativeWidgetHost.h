// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "NativeWidgetHost.generated.h"

/**
 * A NativeWidgetHost is a container widget that can contain one child slate widget.  This should
 * be used when all you need is to nest a native widget inside a UMG widget.
 */
UCLASS()
class UMG_API UNativeWidgetHost : public UWidget
{
	GENERATED_UCLASS_BODY()

	void SetContent(TSharedRef<SWidget> InContent);
	TSharedPtr< SWidget > GetContent() const { return NativeWidget; }

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	TSharedRef<SWidget> GetDefaultContent();

protected:
	TSharedPtr<SWidget> NativeWidget;
};
