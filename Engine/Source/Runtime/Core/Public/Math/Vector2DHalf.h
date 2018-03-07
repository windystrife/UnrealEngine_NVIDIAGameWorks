// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Math/Vector2D.h"
#include "Math/Float16.h"

/**
 * Structure for two dimensional vectors with half floating point precision.
 */
struct FVector2DHalf 
{
	/** Holds the vector's X-component. */
	FFloat16 X;

	/** Holds the vector's Y-component. */
	FFloat16 Y;

public:

	/** Default Constructor (no initialization). */
	FORCEINLINE FVector2DHalf() { }

	/**
	 * Constructor.
	 *
	 * InX half float X value
	 * Iny half float Y value
	 */
 	FORCEINLINE FVector2DHalf( const FFloat16& InX,const FFloat16& InY );

	/** Constructor 
	 *
	 * InX float X value
	 * Iny float Y value
	 */
	FORCEINLINE FVector2DHalf( float InX,float InY );

	/** Constructor 
	 *
	 * Vector2D float vector
	 */
	FORCEINLINE FVector2DHalf( const FVector2D& Vector2D );

public:

	/**
	 * Assignment operator.
	 *
	 * @param Vector2D The value to assign.
	 */
 	FVector2DHalf& operator=( const FVector2D& Vector2D );

	/** Implicit conversion operator for conversion to FVector2D. */
	operator FVector2D() const;

public:

	/**
	 * Get a textual representation of the vector.
	 *
	 * @return Text describing the vector.
	 */
	FString ToString() const;

public:

	/**
	 * Serializes the FVector2DHalf.
	 *
	 * @param Ar Reference to the serialization archive.
	 * @param V Reference to the FVector2DHalf being serialized.
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<( FArchive& Ar, FVector2DHalf& V )
	{
		return Ar << V.X << V.Y;
	}
};


/* FVector2DHalf inline functions
 *****************************************************************************/

FORCEINLINE FVector2DHalf::FVector2DHalf( const FFloat16& InX, const FFloat16& InY )
 	:	X(InX), Y(InY)
{ }


FORCEINLINE FVector2DHalf::FVector2DHalf( float InX, float InY )
	:	X(InX), Y(InY)
{ }


FORCEINLINE FVector2DHalf::FVector2DHalf( const FVector2D& Vector2D )
	:	X(Vector2D.X), Y(Vector2D.Y)
{ }


FORCEINLINE FVector2DHalf& FVector2DHalf::operator=( const FVector2D& Vector2D )
{
 	X = FFloat16(Vector2D.X);
 	Y = FFloat16(Vector2D.Y);

	return *this;
}


FORCEINLINE FString FVector2DHalf::ToString() const
{
	return FString::Printf(TEXT("X=%3.3f Y=%3.3f"), (float)X, (float)Y );
}


FORCEINLINE FVector2DHalf::operator FVector2D() const
{
	return FVector2D((float)X,(float)Y);
}
