// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Builds an inner layout for a section
 */
class ISectionLayoutBuilder
{
public:
	virtual ~ISectionLayoutBuilder() { }

	/** 
	 * Pushes a new category onto the layout.  If there is a current category, this category will appear as a child of the current category
	 *
	 * @param CategoryName	The name of the category
	 * @param DisplayLabel	The localized display label for the category
	 */
	virtual void PushCategory( FName CategoryName, const FText& DisplayLabel ) = 0;
	
	/**
	 * Sets the section as a key area itself
	 * @param KeyArea		Interface for accessing and drawing keys
	 */
	virtual void SetSectionAsKeyArea( TSharedRef<class IKeyArea> KeyArea ) = 0;

	/**
	 * Adds a key area onto the layout.  If a category is pushed, the key area will appear as a child of the current category
	 *
	 * @param KeyAreaName	The name of the key area
	 * @param DisplayLabel	The localized display label for the key area
	 * @param KeyArea		Interface for accessing and drawing keys
	 */
	virtual void AddKeyArea( FName KeyAreaName, const FText& DisplayLabel, TSharedRef<class IKeyArea> KeyArea ) = 0;

	/**
	 * Pops a category off the stack
	 */
	virtual void PopCategory() = 0;


};

