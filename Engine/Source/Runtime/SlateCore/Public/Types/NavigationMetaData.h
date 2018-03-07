// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "Input/NavigationReply.h"
#include "Types/ISlateMetaData.h"

class SWidget;

/**
 * Simple tagging metadata
 */
class FNavigationMetaData : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FNavigationMetaData, ISlateMetaData)

	FNavigationMetaData()
	{
		for (SNavData& Rule : Rules)
		{
			Rule.BoundaryRule = EUINavigationRule::Escape;
			Rule.FocusRecipient = nullptr;
		}
	}

	/**
	 * Get the boundary rule  for the provided navigation type
	 * 
	 * @param InNavigation the type of navigation to get the boundary rule for
	 * @return the boundary rule for the provided navigation type
	 */
	EUINavigationRule GetBoundaryRule(EUINavigation InNavigation) const
	{
		return Rules[(uint8)InNavigation].BoundaryRule;
	}

	/**
	 * Get the focus recipient for the provided navigation type
	 * 
	 * @param InNavigation the type of navigation to get the focus recipient for
	 * @return the focus recipient for the provided navigation type
	 */
	const TWeakPtr<SWidget>& GetFocusRecipient(EUINavigation InNavigation) const
	{
		return Rules[(uint8)InNavigation].FocusRecipient;
	}

	/**
	 * Get the focus recipient delegate for the provided navigation type
	 * 
	 * @param InNavigation the type of navigation to get the focus recipient delegate for
	 * @return the focus recipient delegate for the provided navigation type
	 */
	const FNavigationDelegate& GetFocusDelegate(EUINavigation InNavigation) const
	{
		return Rules[(uint8)InNavigation].FocusDelegate;
	}

	/**
	 * Set the navigation behavior for the provided navigation type to be explicit, using a constant widget
	 * 
	 * @param InNavigation the type of navigation to set explicit
	 * @param InFocusRecipient the widget used when the system requests the destination for the explicit navigation
	 */
	void SetNavigationExplicit(EUINavigation InNavigation, TSharedPtr<SWidget> InFocusRecipient)
	{
		Rules[(uint8)InNavigation].BoundaryRule = EUINavigationRule::Explicit;
		Rules[(uint8)InNavigation].FocusRecipient = InFocusRecipient;
	}

	/**
	 * Set the navigation behavior for the provided navigation type to be a custom delegate
	 * 
	 * @param InNavigation the type of navigation to set custom
	 * @param InFocusRecipient the delegate used when the system requests the destination for the custom navigation
	 */
	void SetNavigationCustom(EUINavigation InNavigation, FNavigationDelegate InFocusDelegate)
	{
		Rules[(uint8)InNavigation].BoundaryRule = EUINavigationRule::Custom;
		Rules[(uint8)InNavigation].FocusDelegate = InFocusDelegate;
	}

	/**
	* Set the navigation behavior for the provided navigation type to be wrap
	*
	* @param InNavigation the type of navigation to set wrapping 
	*/
	void SetNavigationWrap(EUINavigation InNavigation)
	{
		Rules[(uint8)InNavigation].BoundaryRule = EUINavigationRule::Wrap;
	}

	/**
	 * An event should return a FNavigationReply::Explicit() to let the system know to stop at the bounds of this widget.
	 */
	void SetNavigationStop(EUINavigation InNavigation)
	{
		Rules[(uint8)InNavigation].BoundaryRule = EUINavigationRule::Stop;
	}

	/**
	 * An event should return a FNavigationReply::Escape() to let the system know that a navigation can escape the bounds of this widget.
	 */
	void SetNavigationEscape(EUINavigation InNavigation)
	{
		Rules[(uint8)InNavigation].BoundaryRule = EUINavigationRule::Escape;
	}
private:

	struct SNavData
	{
		EUINavigationRule BoundaryRule;
		TWeakPtr<SWidget> FocusRecipient;
		FNavigationDelegate FocusDelegate;
	};
	SNavData Rules[(uint8)EUINavigation::Num];

};
