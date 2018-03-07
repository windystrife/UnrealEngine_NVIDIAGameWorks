// Copyright (C) 2009 Nine Realms, Inc

#include "Quadric.h"

bool CalcGradient( double grad[4], const FVector& p0, const FVector& p1, const FVector& p2, const FVector& n, float a0, float a1, float a2 )
{
	// solve for gd
	// [ p0, 1 ][ g0 ] = [ a0 ]
	// [ p1, 1 ][ g1 ] = [ a1 ]
	// [ p2, 1 ][ g2 ] = [ a2 ]
	// [ n,  0 ][ d  ] = [ 0  ]

	double det, invDet;

	// 2x2 sub-determinants required to calculate 4x4 determinant
	double det2_01_01 = p0[0] * p1[1] - p0[1] * p1[0];
	double det2_01_02 = p0[0] * p1[2] - p0[2] * p1[0];
	double det2_01_03 = p0[0] - p1[0];
	double det2_01_12 = p0[1] * p1[2] - p0[2] * p1[1];
	double det2_01_13 = p0[1] - p1[1];
	double det2_01_23 = p0[2] - p1[2];

	// 3x3 sub-determinants required to calculate 4x4 determinant
	double det3_201_013 = p2[0] * det2_01_13 - p2[1] * det2_01_03 + det2_01_01;
	double det3_201_023 = p2[0] * det2_01_23 - p2[2] * det2_01_03 + det2_01_02;
	double det3_201_123 = p2[1] * det2_01_23 - p2[2] * det2_01_13 + det2_01_12;

	det = - det3_201_123 * n[0] + det3_201_023 * n[1] - det3_201_013 * n[2];

	if( FMath::Abs( det ) < 1e-8 )
	{
		return false;
	}

	invDet = 1.0 / det;

	// remaining 2x2 sub-determinants
	double det2_03_01 = p0[0] * n[1] - p0[1] * n[0];
	double det2_03_02 = p0[0] * n[2] - p0[2] * n[0];
	double det2_03_12 = p0[1] * n[2] - p0[2] * n[1];
	double det2_03_03 = -n[0];
	double det2_03_13 = -n[1];
	double det2_03_23 = -n[2];

	double det2_13_01 = p1[0] * n[1] - p1[1] * n[0];
	double det2_13_02 = p1[0] * n[2] - p1[2] * n[0];
	double det2_13_12 = p1[1] * n[2] - p1[2] * n[1];
	double det2_13_03 = -n[0];
	double det2_13_13 = -n[1];
	double det2_13_23 = -n[2];

	// remaining 3x3 sub-determinants
	double det3_203_012 = p2[0] * det2_03_12 - p2[1] * det2_03_02 + p2[2] * det2_03_01;
	double det3_203_013 = p2[0] * det2_03_13 - p2[1] * det2_03_03 + det2_03_01;
	double det3_203_023 = p2[0] * det2_03_23 - p2[2] * det2_03_03 + det2_03_02;
	double det3_203_123 = p2[1] * det2_03_23 - p2[2] * det2_03_13 + det2_03_12;

	double det3_213_012 = p2[0] * det2_13_12 - p2[1] * det2_13_02 + p2[2] * det2_13_01;
	double det3_213_013 = p2[0] * det2_13_13 - p2[1] * det2_13_03 + det2_13_01;
	double det3_213_023 = p2[0] * det2_13_23 - p2[2] * det2_13_03 + det2_13_02;
	double det3_213_123 = p2[1] * det2_13_23 - p2[2] * det2_13_13 + det2_13_12;

	double det3_301_012 = n[0] * det2_01_12 - n[1] * det2_01_02 + n[2] * det2_01_01;
	double det3_301_013 = n[0] * det2_01_13 - n[1] * det2_01_03;
	double det3_301_023 = n[0] * det2_01_23 - n[2] * det2_01_03;
	double det3_301_123 = n[1] * det2_01_23 - n[2] * det2_01_13;

	grad[0] = - det3_213_123 * invDet * a0 + det3_203_123 * invDet * a1 + det3_301_123 * invDet * a2;
	grad[1] = + det3_213_023 * invDet * a0 - det3_203_023 * invDet * a1 - det3_301_023 * invDet * a2;
	grad[2] = - det3_213_013 * invDet * a0 + det3_203_013 * invDet * a1 + det3_301_013 * invDet * a2;
	grad[3] = + det3_213_012 * invDet * a0 - det3_203_012 * invDet * a1 - det3_301_012 * invDet * a2;

	return true;
}
bool CalcGradientMatrix( double* __restrict GradMatrix, const FVector& p0, const FVector& p1, const FVector& p2, const FVector& n )
{
	// solve for gd
	// [ p0, 1 ][ g0 ] = [ a0 ]
	// [ p1, 1 ][ g1 ] = [ a1 ]
	// [ p2, 1 ][ g2 ] = [ a2 ]
	// [ n,  0 ][ d  ] = [ 0  ]

	double det, invDet;

	// 2x2 sub-determinants required to calculate 4x4 determinant
	double det2_01_01 = p0[0] * p1[1] - p0[1] * p1[0];
	double det2_01_02 = p0[0] * p1[2] - p0[2] * p1[0];
	double det2_01_03 = p0[0] - p1[0];
	double det2_01_12 = p0[1] * p1[2] - p0[2] * p1[1];
	double det2_01_13 = p0[1] - p1[1];
	double det2_01_23 = p0[2] - p1[2];

	// 3x3 sub-determinants required to calculate 4x4 determinant
	double det3_201_013 = p2[0] * det2_01_13 - p2[1] * det2_01_03 + det2_01_01;
	double det3_201_023 = p2[0] * det2_01_23 - p2[2] * det2_01_03 + det2_01_02;
	double det3_201_123 = p2[1] * det2_01_23 - p2[2] * det2_01_13 + det2_01_12;

	det = - det3_201_123 * n[0] + det3_201_023 * n[1] - det3_201_013 * n[2];

	if( FMath::Abs( det ) < 1e-8 )
	{
		return false;
	}

	invDet = 1.0 / det;

	// remaining 2x2 sub-determinants
	double det2_03_01 = p0[0] * n[1] - p0[1] * n[0];
	double det2_03_02 = p0[0] * n[2] - p0[2] * n[0];
	double det2_03_12 = p0[1] * n[2] - p0[2] * n[1];
	double det2_03_03 = -n[0];
	double det2_03_13 = -n[1];
	double det2_03_23 = -n[2];

	double det2_13_01 = p1[0] * n[1] - p1[1] * n[0];
	double det2_13_02 = p1[0] * n[2] - p1[2] * n[0];
	double det2_13_12 = p1[1] * n[2] - p1[2] * n[1];
	double det2_13_03 = -n[0];
	double det2_13_13 = -n[1];
	double det2_13_23 = -n[2];

	// remaining 3x3 sub-determinants
	double det3_203_012 = p2[0] * det2_03_12 - p2[1] * det2_03_02 + p2[2] * det2_03_01;
	double det3_203_013 = p2[0] * det2_03_13 - p2[1] * det2_03_03 + det2_03_01;
	double det3_203_023 = p2[0] * det2_03_23 - p2[2] * det2_03_03 + det2_03_02;
	double det3_203_123 = p2[1] * det2_03_23 - p2[2] * det2_03_13 + det2_03_12;

	double det3_213_012 = p2[0] * det2_13_12 - p2[1] * det2_13_02 + p2[2] * det2_13_01;
	double det3_213_013 = p2[0] * det2_13_13 - p2[1] * det2_13_03 + det2_13_01;
	double det3_213_023 = p2[0] * det2_13_23 - p2[2] * det2_13_03 + det2_13_02;
	double det3_213_123 = p2[1] * det2_13_23 - p2[2] * det2_13_13 + det2_13_12;

	double det3_301_012 = n[0] * det2_01_12 - n[1] * det2_01_02 + n[2] * det2_01_01;
	double det3_301_013 = n[0] * det2_01_13 - n[1] * det2_01_03;
	double det3_301_023 = n[0] * det2_01_23 - n[2] * det2_01_03;
	double det3_301_123 = n[1] * det2_01_23 - n[2] * det2_01_13;

	GradMatrix[ 0] = det3_213_123 * invDet;
	GradMatrix[ 1] = det3_213_023 * invDet;
	GradMatrix[ 2] = det3_213_013 * invDet;
	GradMatrix[ 3] = det3_213_012 * invDet;

	GradMatrix[ 4] = det3_203_123 * invDet;
	GradMatrix[ 5] = det3_203_023 * invDet;
	GradMatrix[ 6] = det3_203_013 * invDet;
	GradMatrix[ 7] = det3_203_012 * invDet;

	GradMatrix[ 8] = det3_301_123 * invDet;
	GradMatrix[ 9] = det3_301_023 * invDet;
	GradMatrix[10] = det3_301_013 * invDet;
	GradMatrix[11] = det3_301_012 * invDet;

	return true;
}
