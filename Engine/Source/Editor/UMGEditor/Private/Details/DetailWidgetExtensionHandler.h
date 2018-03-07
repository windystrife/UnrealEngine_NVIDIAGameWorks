// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "WidgetBlueprintEditor.h"
#include "Editor/PropertyEditor/Public/IDetailPropertyExtensionHandler.h"

class IPropertyHandle;

class FDetailWidgetExtensionHandler : public IDetailPropertyExtensionHandler
{
public:
	FDetailWidgetExtensionHandler(TSharedPtr<class FWidgetBlueprintEditor> InBlueprintEditor);

	virtual bool IsPropertyExtendable(const UClass* InObjectClass, const class IPropertyHandle& PropertyHandle) const override;

	virtual TSharedRef<SWidget> GenerateExtensionWidget(const UClass* InObjectClass, TSharedPtr<IPropertyHandle> PropertyHandle) override;

private:
	TWeakPtr<class FWidgetBlueprintEditor> BlueprintEditor;
};
