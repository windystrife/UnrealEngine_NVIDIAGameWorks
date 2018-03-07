// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/Plane.h"
#include "Math/Matrix.h"

/**
 * Realigns the near plane for an existing projection matrix 
 * with an arbitrary clip plane
 * from: http://sourceforge.net/mailarchive/message.php?msg_id=000901c26324%242181ea90%24a1e93942%40firefly
 * Updated for the fact that our FPlane uses Ax+By+Cz=D.
 */
class FClipProjectionMatrix : public FMatrix
{
public:
	/**
	 * Constructor
	 *
	 * @param	SrcProjMat - source projection matrix to premultiply with the clip matrix
	 * @param	Plane - clipping plane used to build the clip matrix (assumed to be in camera space)
	 */
	FClipProjectionMatrix( const FMatrix& SrcProjMat, const FPlane& Plane );

private:
	/** return sign of a number */
	FORCEINLINE float sgn( float a );
};


FORCEINLINE FClipProjectionMatrix::FClipProjectionMatrix( const FMatrix& SrcProjMat, const FPlane& Plane ) :
FMatrix(SrcProjMat)
{
	// Calculate the clip-space corner point opposite the clipping plane
	// as (sgn(clipPlane.x), sgn(clipPlane.y), 1, 1) and
	// transform it into camera space by multiplying it
	// by the inverse of the projection matrix
	FPlane CornerPlane( 
		sgn(Plane.X) / SrcProjMat.M[0][0],
		sgn(Plane.Y) / SrcProjMat.M[1][1],
		1.0f,
		-(1.0f - SrcProjMat.M[2][2]) / SrcProjMat.M[3][2]
	);

	// Calculate the scaled plane vector
	FPlane ProjPlane( Plane * (1.0f / (Plane | CornerPlane)) );

	// use the projected space clip plane in z column 
	// Note: (account for our negated W coefficient)
	M[0][2] = ProjPlane.X;
	M[1][2] = ProjPlane.Y;
	M[2][2] = ProjPlane.Z;
	M[3][2] = -ProjPlane.W;
}


FORCEINLINE float FClipProjectionMatrix::sgn( float a )
{
	if (a > 0.0f) return (1.0f);
	if (a < 0.0f) return (-1.0f);
	return (0.0f);
}
