// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"

/**
 * Structure for optional floating point sizes.
 */
struct FOptionalSize 
{
	/**
	 * Creates an unspecified size.
	 */
	FOptionalSize( )
		: Size(Unspecified)
	{ }

	/**
	 * Creates a size with the specified value.
	 *
	 * @param SpecifiedSize The size to set.
	 */
	FOptionalSize( const float SpecifiedSize )
		: Size(SpecifiedSize)
	{ }

public:

	/**
	 * Checks whether the size is set.
	 *
	 * @return true if the size is set, false if it is unespecified.
	 * @see Get
	 */
	bool IsSet( ) const
	{
		return Size != Unspecified;
	}

	/**
	 * Gets the value of the size.
	 *
	 * Before calling this method, check with IsSet() whether the size is actually specified.
	 * Unspecified sizes a value of -1.0f will be returned.
	 *
	 * @see IsSet
	 */
	float Get( ) const
	{
		return Size;
	}

private:

	// constant for unspecified sizes.
	SLATECORE_API static const float Unspecified;

	// Holds the size, if specified.
	float Size;
};


/**
 * Base structure for size parameters.
 *
 * Describes a way in which a parent widget allocates available space to its child widgets.
 *
 * When SizeRule is SizeRule_Auto, the widget's DesiredSize will be used as the space required.
 * When SizeRule is SizeRule_Stretch, the available space will be distributed proportionately between
 * peer Widgets depending on the Value property. Available space is space remaining after all the
 * peers' SizeRule_Auto requirements have been satisfied.
 *
 * FSizeParam cannot be constructed directly - see FStretch, FAuto, and FAspectRatio
 */
struct FSizeParam
{
	enum ESizeRule
	{
		SizeRule_Auto,
		SizeRule_Stretch
	};
	
	/** The sizing rule to use. */
	ESizeRule SizeRule;

	/**
	 * The actual value this size parameter stores.
	 *
	 * This value can be driven by a delegate. It is only used for the Stretch mode.
	 */
	TAttribute<float> Value;
	
protected:

	/**
	 * Hidden constructor.
	 *
	 * Use FAspectRatio, FAuto, FStretch to instantiate size parameters.
	 *
	 * @see FAspectRatio, FAuto, FStretch
	 */
	FSizeParam( ESizeRule InTypeOfSize, const TAttribute<float>& InValue )
		: SizeRule(InTypeOfSize)
		, Value(InValue)
	{ }
};


/**
 * Structure for size parameters with SizeRule = SizeRule_Stretch.
 *
 * @see FAspectRatio
 * @see FAuto
 */
struct FStretch
	: public FSizeParam
{
	FStretch( const TAttribute<float>& StretchAmount )
		: FSizeParam(SizeRule_Stretch, StretchAmount)
	{ }

	FStretch( )
		: FSizeParam(SizeRule_Stretch, 1.0f)
	{ }
};


/**
 * Structure for size parameters with SizeRule = SizeRule_Auto.
 *
 * @see FAspectRatio, FStretch
 */
struct FAuto
	: public FSizeParam
{
	FAuto()
		: FSizeParam(SizeRule_Auto, 0.0f)
	{ }
};
