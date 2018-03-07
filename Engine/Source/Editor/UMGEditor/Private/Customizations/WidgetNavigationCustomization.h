// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "WidgetBlueprintEditor.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"

class IDetailChildrenBuilder;

class FWidgetNavigationCustomization : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IPropertyTypeCustomization> MakeInstance(TSharedRef<FWidgetBlueprintEditor> InEditor)
	{
		return MakeShareable(new FWidgetNavigationCustomization(InEditor));
	}

	FWidgetNavigationCustomization(TSharedRef<FWidgetBlueprintEditor> InEditor)
		: Editor(InEditor)
	{
	}
	
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	
private:
	
	EUINavigationRule GetNavigationRule(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav) const;
	FText GetNavigationText(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav) const;

	void MakeNavRow(TWeakPtr<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, EUINavigation Nav, FText NavName);

	TSharedRef<class SWidget> MakeNavMenu(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav);

	void HandleNavMenuEntryClicked(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav, EUINavigationRule Rule);

	FText GetExplictWidget(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav) const;
	void OnCommitExplictWidgetText(const FText& ItemFText, ETextCommit::Type CommitInfo, TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav);
	EVisibility GetExplictWidgetFieldVisibility(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav) const;

	void SetNav(UWidget* Widget, EUINavigation Nav, TOptional<EUINavigationRule> Rule, TOptional<FName> WidgetToFocus);

private:
	TWeakPtr<FWidgetBlueprintEditor> Editor;
};
