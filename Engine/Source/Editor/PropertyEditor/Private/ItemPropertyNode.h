// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyNode.h"

class FItemPropertyNode : public FPropertyNode
{
public:

	FItemPropertyNode();
	virtual ~FItemPropertyNode();

	/**
	 * Calculates the memory address for the data associated with this item's property.  This is typically the value of a UProperty or a UObject address.
	 *
	 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
	 *
	 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to an FArray*)
	 */
	virtual uint8* GetValueBaseAddress( uint8* Base ) override;
	/**
	 * Calculates the memory address for the data associated with this item's value.  For most properties, identical to GetValueBaseAddress.  For items corresponding
	 * to dynamic array elements, the pointer returned will be the location for that element's data. 
	 *
	 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to whatever type is the Inner for the dynamic array)
	 */
	virtual uint8* GetValueAddress( uint8* Base ) override;

	/**
	 * Overridden function to get the derived object node
	 */
	virtual FItemPropertyNode* AsItemPropertyNode() override { return this; }

	/** Display name override to use instead of the property name */
	void SetDisplayNameOverride( const FText& InDisplayNameOverride ) override;

	/**
	* @return true if the property is mark as a favorite
	*/
	virtual void SetFavorite(bool FavoriteValue) override;

	/**
	* @return true if the property is mark as a favorite
	*/
	virtual bool IsFavorite() const override;

	/**
	* Set the permission to display the favorite icon
	*/
	virtual void SetCanDisplayFavorite(bool CanDisplayFavoriteIcon) override;

	/**
	* Set the permission to display the favorite icon
	*/
	virtual bool CanDisplayFavorite() const override;

	/**
	 * @return The formatted display name for the property in this node                                                              
	 */
	virtual FText GetDisplayName() const override;

	/**
	 * Sets the tooltip override to use instead of the property tooltip
	 */
	virtual void SetToolTipOverride( const FText& InToolTipOverride ) override;

	/**
	 * @return The tooltip for the property in this node                                                              
	 */
	virtual FText GetToolTipText() const override;

protected:
	/**
	 * Overridden function for special setup
	 */
	virtual void InitExpansionFlags() override;

	/**
	 * Overridden function for Creating Child Nodes
	 */
	virtual void InitChildNodes() override;

private:
	/** Display name override to use instead of the property name */
	FText DisplayNameOverride;
	/** Tooltip override to use instead of the property tooltip */
	FText ToolTipOverride;

	/**
	* Option to know if we can display the favorite icon in the property editor
	*/
	bool bCanDisplayFavorite;
};
