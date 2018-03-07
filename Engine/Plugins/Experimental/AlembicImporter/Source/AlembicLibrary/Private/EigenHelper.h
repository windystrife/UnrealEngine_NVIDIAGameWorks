// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Just to be sure, also added this in Eigen.Build.cs
#ifndef EIGEN_MPL2_ONLY
#define EIGEN_MPL2_ONLY
#endif

#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(push)
	#pragma warning(disable:6326) // Potential comparison of a constant with another constant.
#endif
	#include <Eigen/Dense>
	#include <Eigen/SVD>
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(pop)
#endif

#define USE_MEMCPY_FOR_EIGEN_CONVERSION 1

namespace EigenHelpers
{
	/** Sets the number of thread used by Eigen */
	static void SetNumEigenThreads(const int32 InNumThreads)
	{
		const int32 CurrentNumThreads = Eigen::nbThreads();
		if (CurrentNumThreads != InNumThreads)
		{
			Eigen::setNbThreads(InNumThreads);
		}
	}

	/** Converts a float array into an Eigen Matrix */
	static void ConvertArrayToEigenMatrix(const TArray<float>& InArray, const int32 InRows, const int32 InColumns, Eigen::MatrixXf& OutMatrix)
	{
		OutMatrix.resize(InRows, InColumns);

#if USE_MEMCPY_FOR_EIGEN_CONVERSION
		FMemory::Memcpy(OutMatrix.data(), InArray.GetData(), InRows * InColumns * sizeof(float));
#else
		// Copy matrix data
		for (int32 ColumnIndex = 0; ColumnIndex < InColumns; ++ColumnIndex)
		{
			const int32 ColumnOffset = ColumnIndex * InRows;
			for (int32 RowIndex = 0; RowIndex < InRows; ++RowIndex)
			{
				OutMatrix(RowIndex, ColumnIndex) = InArray[ColumnOffset + RowIndex];
			}
		}
#endif // USE_MEMCPY_FOR_EIGEN_CONVERSION
	}

	/** Converts an Eigen Matrix into a float array */
	static void ConvertEigenMatrixToArray(const Eigen::MatrixXf& InMatrix, TArray<float>& OutArray, uint32& OutColumns, uint32& OutRows )
	{
		OutColumns = (int32)InMatrix.cols();
		OutRows = (int32)InMatrix.rows();
		const uint32 TotalSize = OutRows * OutColumns;
		OutArray.Empty(TotalSize);
		OutArray.AddZeroed(TotalSize);

#if USE_MEMCPY_FOR_EIGEN_CONVERSION
		FMemory::Memcpy(OutArray.GetData(), InMatrix.data(), TotalSize * sizeof(float));
#else
		for (int32 ColumnIndex = 0; ColumnIndex < OutColumns; ++ColumnIndex)
		{
			const int32 ColumnOffset = ColumnIndex * OutRows;
			for (int32 RowIndex = 0; RowIndex < OutRows; ++RowIndex)
			{
				OutArray[ColumnOffset + RowIndex] = InMatrix(RowIndex, ColumnIndex);
			}
		}
#endif // USE_MEMCPY_FOR_EIGEN_CONVERSION
	}
	
	/** Performs Singular value decomposition on the given matrix and returns respective the calculated U, V and S Matrix */
	static void PerformSVD(const Eigen::MatrixXf& InMatrix, Eigen::MatrixXf& OutU, Eigen::MatrixXf& OutV, Eigen::MatrixXf& OutS)
	{
		Eigen::JacobiSVD<Eigen::MatrixXf> SVD(InMatrix, Eigen::ComputeThinU | Eigen::ComputeThinV);
		OutU = SVD.matrixU();
		OutV = SVD.matrixV().transpose();
		OutS = SVD.singularValues();
	}

	/** Performs Singular value decomposition on the given matrix and returns respective the calculated U, V and S Matrix */
	static void PerformSVD(const TArray<float>& InMatrix, const int32 InRows, const int32 InColumns, TArray<float>& OutU, TArray<float>& OutV, TArray<float>& OutS)
	{
		Eigen::MatrixXf EigenMatrix;
		ConvertArrayToEigenMatrix(InMatrix, InRows, InColumns, EigenMatrix);
		
		Eigen::MatrixXf EigenOutU;
		Eigen::MatrixXf EigenOutV;
		Eigen::MatrixXf EigenOutS;
		PerformSVD(EigenMatrix, EigenOutU, EigenOutV, EigenOutS);

		uint32 NumColumns, NumRows;
		ConvertEigenMatrixToArray(EigenOutU, OutU, NumColumns, NumRows);
		ConvertEigenMatrixToArray(EigenOutV, OutV, NumColumns, NumRows);
		ConvertEigenMatrixToArray(EigenOutS, OutS, NumColumns, NumRows);
	}
};
