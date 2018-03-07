// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Editor/PropertyEditor/Public/IPropertyTypeCustomization.h"
#include "SGraphPin.h"

class IPropertyHandle;

/** Customization for a primary asset id, shows an asset picker with filters */
class FPrimaryAssetIdCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPrimaryAssetIdCustomization);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}

private:
	bool OnShouldFilterAsset(const struct FAssetData& InAssetData, TArray<FPrimaryAssetType> AllowedTypes) const;
	FString OnGetObjectPath() const;
	void OnSetObject(const FAssetData& AssetData);

	/** Handle to the struct property being customized */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;

	/** Specified type */
	TArray<FPrimaryAssetType> AllowedTypes;
};

/** Graph pin version of UI */
class SPrimaryAssetIdGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SPrimaryAssetIdGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

private:

	void OnIdSelected(FPrimaryAssetId AssetId);
	FText GetDisplayText() const;

	FPrimaryAssetId CurrentId;
};