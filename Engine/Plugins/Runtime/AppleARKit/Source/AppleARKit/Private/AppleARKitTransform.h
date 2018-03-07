#pragma once

// UE4
#include "Math/Transform.h"

struct FAppleARKitTransform
{	

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

	/** 
	 * Convert's an ARKit 'Y up' 'right handed' coordinate system transform to Unreal's 'Z up' 
	 * 'left handed' coordinate system.
	 *
	 * Ignores scale.
	 */
	static FORCEINLINE FTransform ToFTransform( const matrix_float4x4& RawYUpMatrix, float WorldToMetersScale = 100.0f )
	{
		// Conversion here is as per SteamVRHMD::ToFMatrix
		FMatrix RawYUpFMatrix(
			FPlane(RawYUpMatrix.columns[0][0], RawYUpMatrix.columns[0][1], RawYUpMatrix.columns[0][2], RawYUpMatrix.columns[0][3]),
			FPlane(RawYUpMatrix.columns[1][0], RawYUpMatrix.columns[1][1], RawYUpMatrix.columns[1][2], RawYUpMatrix.columns[1][3]),
			FPlane(RawYUpMatrix.columns[2][0], RawYUpMatrix.columns[2][1], RawYUpMatrix.columns[2][2], RawYUpMatrix.columns[2][3]),
			FPlane(RawYUpMatrix.columns[3][0], RawYUpMatrix.columns[3][1], RawYUpMatrix.columns[3][2], RawYUpMatrix.columns[3][3]));

		// Extract & convert translation
		FVector Translation = FVector( -RawYUpFMatrix.M[3][2], RawYUpFMatrix.M[3][0], RawYUpFMatrix.M[3][1] ) * WorldToMetersScale;

		// Extract & convert rotation 
		FQuat RawRotation( RawYUpFMatrix );
		FQuat Rotation( -RawRotation.Z, RawRotation.X, RawRotation.Y, -RawRotation.W );

		return FTransform( Rotation, Translation );
	}

	/** 
	 * Convert's an ARKit 'Y up' 'right handed' coordinate system vector to Unreal's 'Z up' 
	 * 'left handed' coordinate system.
	 */
	static FORCEINLINE FVector ToFVector( const vector_float3& RawYUpVector, float WorldToMetersScale = 100.0f )
	{
		return FVector( -RawYUpVector.z, RawYUpVector.x, RawYUpVector.y ) * WorldToMetersScale;
	}

#endif // ARKIT_SUPPORT
};
