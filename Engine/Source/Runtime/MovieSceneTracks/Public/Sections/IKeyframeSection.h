// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EMovieSceneKeyInterpolation : uint8;

template<typename KeyDataType>
class IKeyframeSection
{
public:

	/** Returns true if the supplied key data is different than the section value at the supplied time, otherwise false. */
	virtual bool NewKeyIsNewData( float Time, const KeyDataType& KeyData ) const = 0;

	/** Returns true if the section has any keys which correspond to the supplied key data, otherwise false. */
	virtual bool HasKeys( const KeyDataType& KeyData ) const = 0;

	/** Adds a key to the section at the supplied time, with the supplied data. */
	virtual void AddKey( float Time, const KeyDataType& KeyData, EMovieSceneKeyInterpolation KeyInterpolation ) = 0;

	/** Sets the default value for the section which corresponds to the supplied key data. */
	virtual void SetDefault( const KeyDataType& KeyData ) = 0;

	/** Removes all default data from this section. */
	virtual void ClearDefaults() = 0;

};
class IKeyframeSectionEnum
{
public:

	/** Returns true if the supplied key data is different than the section value at the supplied time, otherwise false. */
	virtual bool NewKeyIsNewData( float Time, const int64& KeyData ) const = 0;

	/** Returns true if the section has any keys which correspond to the supplied key data, otherwise false. */
	virtual bool HasKeys( const int64& KeyData ) const = 0;

	/** Adds a key to the section at the supplied time, with the supplied data. */
	virtual void AddKey( float Time, const int64& KeyData, EMovieSceneKeyInterpolation KeyInterpolation ) = 0;

	/** Sets the default value for the section which corresponds to the supplied key data. */
	virtual void SetDefault( const int64& KeyData ) = 0;

	/** Removes all default data from this section. */
	virtual void ClearDefaults() = 0;

};