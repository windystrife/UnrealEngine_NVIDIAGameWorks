// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Framework/SlateDelegates.h"
#include "PropertyHandle.h"

class FDetailWidgetRow;

DECLARE_DELEGATE_RetVal_OneParam(bool, FIsResetToDefaultVisible, TSharedPtr<IPropertyHandle> /* PropertyHandle */);
DECLARE_DELEGATE_OneParam(FResetToDefaultHandler, TSharedPtr<IPropertyHandle> /* PropertyHandle*/);

/**
 * Structure describing the delegates needed to override the behavior of reset to default in detail properties
 */
class FResetToDefaultOverride
{
public:
	/** Creates a FResetToDefaultOverride in which the the reset to default is always visible */
	static FResetToDefaultOverride Create(FResetToDefaultHandler InResetToDefaultClicked, const bool InPropagateToChildren = false)
	{
		FResetToDefaultOverride ResetToDefault;
		ResetToDefault.bForceShow = true;
		ResetToDefault.OnResetToDefaultClickedDelegate = InResetToDefaultClicked;
		ResetToDefault.bPropagateToChildren = InPropagateToChildren;
		ResetToDefault.bForceHide = false;
		return ResetToDefault;
	}

	/** Creates a FResetToDefaultOverride from visibility and click handler callback delegates */
	static FResetToDefaultOverride Create(FIsResetToDefaultVisible InIsResetToDefaultVisible, FResetToDefaultHandler InResetToDefaultClicked, const bool InPropagateToChildren = false)
	{
		FResetToDefaultOverride ResetToDefault;
		ResetToDefault.bForceShow = false;
		ResetToDefault.IsResetToDefaultVisibleDelegate = InIsResetToDefaultVisible;
		ResetToDefault.OnResetToDefaultClickedDelegate = InResetToDefaultClicked;
		ResetToDefault.bPropagateToChildren = InPropagateToChildren;
		ResetToDefault.bForceHide = false;
		return ResetToDefault;
	}

	/** Creates a FResetToDefaultOverride in which reset to default is never visible */
	static FResetToDefaultOverride Hide(const bool InPropagateToChildren = false)
	{
		FResetToDefaultOverride HideResetToDefault;
		HideResetToDefault.bForceShow = false;
		HideResetToDefault.bForceHide = true;
		HideResetToDefault.bPropagateToChildren = InPropagateToChildren;
		return HideResetToDefault;
	}

	/** Called by the UI to show/hide the reset widgets */
	bool IsResetToDefaultVisible(TSharedPtr<IPropertyHandle> Property) const
	{
		if (bForceShow)
		{
			return true;
		}
		if (!bForceHide && IsResetToDefaultVisibleDelegate.IsBound())
		{
			return IsResetToDefaultVisibleDelegate.Execute(Property);
		}
		return false;
	}

	/** Called by the property editor to actually reset the property to default */
	FResetToDefaultHandler OnResetToDefaultClicked() const
	{
		return OnResetToDefaultClickedDelegate;
	}

	/** Called by properties to determine whether this override should set on their children */
	bool PropagatesToChildren() const
	{
		return bPropagateToChildren;
	}

private:
	/** Callback to indicate whether or not reset to default is visible */
	FIsResetToDefaultVisible IsResetToDefaultVisibleDelegate;

	/** Delegate called when reset to default is clicked */
	FResetToDefaultHandler OnResetToDefaultClickedDelegate;

	/** Should properties pass this on to their children? */
	bool bPropagateToChildren;

	/** Ignore the visibility delegate and always show the reset to default widgets? */
	bool bForceShow;

	/** Ignore the visibility delegate and never show the reset to default widgets? */
	bool bForceHide;
};

/**
 * A single row for a property in a details panel                                                              
 */
class IDetailPropertyRow
{
public:
	virtual ~IDetailPropertyRow(){}

	/** @return the property handle for the property on this row */
	virtual TSharedPtr<IPropertyHandle> GetPropertyHandle() = 0;

	/**
	 * Sets the localized display name of the property
	 *
	 * @param InDisplayName	The name of the property
	 */
	virtual IDetailPropertyRow& DisplayName( const FText& InDisplayName ) = 0;

	/**
	 * Sets the localized tooltip of the property
	 *
	 * @param InToolTip	The tooltip of the property
	 */
	virtual IDetailPropertyRow& ToolTip( const FText& InToolTip ) = 0;

	/**
	 * Sets whether or not we show the default property editing buttons for this property
	 *
	 * @param bShowPropertyButtons	if true property buttons for this property will be shown.  
	 */
	virtual IDetailPropertyRow& ShowPropertyButtons( bool bShowPropertyButtons ) = 0;

	/**
	 * Sets an edit condition for this property.  If the edit condition fails, the property is not editable
	 * This will add a checkbox before the name of the property that users can click to toggle the edit condition
	 * Properties with built in edit conditions will override this automatically.  If the 
	 *
	 * @param EditConditionValue			Attribute for the value of the edit condition (true indicates that the edit condition passes)
	 * @param OnEditConditionValueChanged	Delegate called when the edit condition value changes
	 */
	virtual IDetailPropertyRow& EditCondition( TAttribute<bool> EditConditionValue, FOnBooleanValueChanged OnEditConditionValueChanged ) = 0;

	/**
	 * Sets whether or not this property is enabled
	 *
	 * @param InIsEnabled	Attribute for the enabled state of the property (true to enable the property)
	 */
	virtual IDetailPropertyRow& IsEnabled( TAttribute<bool> InIsEnabled ) = 0;
	

	/**
	 * Sets whether or not this property should auto-expand
	 *
	 * @param bForceExpansion	true to force the property to be expanded
	 */
	virtual IDetailPropertyRow& ShouldAutoExpand(bool bForceExpansion = true) = 0;

	/**
	 * Sets the visibility of this property
	 *
	 * @param Visibility	Attribute for the visibility of this property
	 */
	virtual IDetailPropertyRow& Visibility( TAttribute<EVisibility> Visibility ) = 0;

	/**
	 * Overrides the behavior of reset to default
	 *
	 * @param ResetToDefault	Contains the delegates needed to override the behavior of reset to default
	 */
	virtual IDetailPropertyRow& OverrideResetToDefault(const FResetToDefaultOverride& ResetToDefault) = 0;

	/**
	 * Returns the name and value widget of this property row.  You can use this widget to apply further customization to existing widgets (by using this  with CustomWidget)
	 *
	 * @param OutNameWidget		The default name widget
	 * @param OutValueWidget	The default value widget
	 */
	virtual void GetDefaultWidgets( TSharedPtr<SWidget>& OutNameWidget, TSharedPtr<SWidget>& OutValueWidget ) = 0;

	/**
	 * Returns the name and value widget of this property row.  You can use this widget to apply further customization to existing widgets (by using this  with CustomWidget)
	 *
	 * @param OutNameWidget		The default name widget
	 * @param OutValueWidget	The default value widget
	 * @param OutCustomRow		The default widget row
	 */
	virtual void GetDefaultWidgets( TSharedPtr<SWidget>& OutNameWidget, TSharedPtr<SWidget>& OutValueWidget, FDetailWidgetRow& Row ) = 0;

	/**
	 * Overrides the property widget
	 *
	 * @param bShowChildren	Whether or not to still show any children of this property
	 * @return a row for the property that custom widgets can be added to
	 */
	virtual FDetailWidgetRow& CustomWidget( bool bShowChildren = false ) = 0;
};
