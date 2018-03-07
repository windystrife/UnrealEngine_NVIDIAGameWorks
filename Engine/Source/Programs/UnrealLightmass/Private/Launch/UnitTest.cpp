// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnitTest.h"
#include "LMkDOP.h"
#include "Math/LMOctree.h"
#include "UnrealLightmass.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Guid.h"
#include "LightmassScene.h"

namespace Lightmass
{

/** Defines how the light is stored in the scene's light octree. */
struct FTestOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };
	enum { LoosenessDenominator = 16 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static FBoxCenterAndExtent GetBoundingBox(const float& Element)
	{
		return FBoxCenterAndExtent(FVector4(0), FVector4(Element));
	}

	FORCEINLINE static bool AreElementsEqual(const float& A,const float& B)
	{
		return A == B;
	}
};


class FTestCollisionDataProvider
{
	TkDOPTree<FTestCollisionDataProvider, uint16>& kDOP;
	FVector4 Vertex;
public:

	FTestCollisionDataProvider(TkDOPTree<FTestCollisionDataProvider, uint16>& InkDOP)
		: kDOP(InkDOP)
		, Vertex(0)
	{
	}

	/**
	 * Given an index, returns the position of the vertex
	 *
	 * @param Index the index into the vertices array
	 */
	FORCEINLINE const FVector4& GetVertex(uint16 Index) const
	{
		return Vertex;
	}

	/** Returns additional information. */
	FORCEINLINE int32 GetItemIndex(uint16 MaterialIndex) const
	{
		return 0;
	}

	/**
	 * Returns the kDOPTree for this mesh
	 */
	FORCEINLINE const TkDOPTree<FTestCollisionDataProvider,uint16>& GetkDOPTree(void) const
	{
		return kDOP;
	}
	/**
	 * Returns the local to world for the component
	 */
	FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	{
		return FMatrix::Identity;
	}

	/**
	 * Returns the world to local for the component
	 */
	FORCEINLINE const FMatrix GetWorldToLocal(void) const
	{
		return FMatrix::Identity;
	}

	/**
	 * Returns the local to world transpose adjoint for the component
	 */
	FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	{
		return FMatrix::Identity;
	}

	/**
	 * Returns the determinant for the component
	 */
	FORCEINLINE float GetDeterminant(void) const
	{
		return 0.0f;
	}
};

class FTestRunnable : public FRunnable
{
	bool bStop;
public:
	virtual bool Init(void) 
	{
		bStop = false;
		return true;
	}

	virtual uint32 Run(void)
	{
		for (int32 i = 0; i < 10 && !bStop; i++)
		{
			UE_LOG(LogLightmass, Display, TEXT("Thread counter %d"), i);
			FPlatformProcess::Sleep(1.0f);
		}
		UE_LOG(LogLightmass, Display, TEXT("Thread done!!"));

		return 0;
	}

	virtual void Stop(void)
	{
		bStop = true;
	}

	virtual void Exit(void)
	{
	}

};

void TestLightmass()
{
	UE_LOG(LogLightmass, Display, TEXT("\n\n"));
	UE_LOG(LogLightmass, Display, TEXT("==============================================================================================="));
	UE_LOG(LogLightmass, Display, TEXT("Running \"unit test\". This will take several seconds, and will end with an assertion."));
	UE_LOG(LogLightmass, Display, TEXT("This is on purpose, as it's testing the callstack gathering..."));
	UE_LOG(LogLightmass, Display, TEXT("==============================================================================================="));
	UE_LOG(LogLightmass, Display, TEXT("\n\n"));

	void* Buf = FMemory::Malloc(1024);

	TArray<int32> TestArray;

	TestArray.Add(5);

	TArray<int32> ArrayCopy = TestArray;
	
	FVector4 TestVectorA(1, 0, 0, 1);
	FVector4 TestVectorB(1, 1, 1, 1);
	FVector4 TestVector = TestVectorA + TestVectorB;

	FString TestString = FString::Printf(TEXT("Copy has %d, Vector is [%.2f, %.2f, %.2f, %.2f]\n"), ArrayCopy[0], TestVector.X, TestVector.Y, TestVector.Z, TestVector.W);

	wprintf(TEXT("%s"), *TestString);

	FMemory::Free(Buf);

	struct FAlignTester
	{
		uint8 A;
		FMatrix M1;
		uint8 B;
		FMatrix M2;
		uint8 C;
		FVector4 V;
	};
	FAlignTester AlignTest;
	checkf(((PTRINT)(&FMatrix::Identity) & 15) == 0, TEXT("Identity matrix unaligned"));
	checkf(((PTRINT)(&AlignTest.M1) & 15) == 0, TEXT("First matrix unaligned"));
	checkf(((PTRINT)(&AlignTest.M2) & 15) == 0, TEXT("Second matrix unaligned"));
	checkf(((PTRINT)(&AlignTest.V) & 15) == 0, TEXT("Vector unaligned"));

	FGuid Guid(1, 2, 3, 4);
	UE_LOG(LogLightmass, Display, TEXT("Guid is %s"), *Guid.ToString());

	TMap<FString, int32> TestMap;
	TestMap.Add(FString(TEXT("Five")), 5);
	TestMap.Add(TEXT("Ten"), 10);

	UE_LOG(LogLightmass, Display, TEXT("Map[Five] = %d, Map[Ten] = %d"), TestMap.FindRef(TEXT("Five")), TestMap.FindRef(FString(TEXT("Ten"))));

	FMatrix TestMatrix(FVector(0, 0, 0.1f), FVector(0, 1.0f, 0), FVector(0.9f, 0, 0), FVector(0, 0, 0));

	UE_LOG(LogLightmass, Display, TEXT("Mat=\n  [%0.2f, %0.2f, %0.2f, %0.2f]\n  [%0.2f, %0.2f, %0.2f, %0.2f]\n  [%0.2f, %0.2f, %0.2f, %0.2f]\n  [%0.2f, %0.2f, %0.2f, %0.2f]"), 
		TestMatrix.M[0][0], TestMatrix.M[0][1], TestMatrix.M[0][2], TestMatrix.M[0][3], 
		TestMatrix.M[1][0], TestMatrix.M[1][1], TestMatrix.M[1][2], TestMatrix.M[1][3], 
		TestMatrix.M[2][0], TestMatrix.M[2][1], TestMatrix.M[2][2], TestMatrix.M[2][3], 
		TestMatrix.M[3][0], TestMatrix.M[3][1], TestMatrix.M[3][2], TestMatrix.M[3][3]
		);

	TestMatrix = TestMatrix.GetTransposed();

	UE_LOG(LogLightmass, Display, TEXT("Transposed Mat=\n  [%0.2f, %0.2f, %0.2f, %0.2f]\n  [%0.2f, %0.2f, %0.2f, %0.2f]\n  [%0.2f, %0.2f, %0.2f, %0.2f]\n  [%0.2f, %0.2f, %0.2f, %0.2f]"), 
		TestMatrix.M[0][0], TestMatrix.M[0][1], TestMatrix.M[0][2], TestMatrix.M[0][3], 
		TestMatrix.M[1][0], TestMatrix.M[1][1], TestMatrix.M[1][2], TestMatrix.M[1][3], 
		TestMatrix.M[2][0], TestMatrix.M[2][1], TestMatrix.M[2][2], TestMatrix.M[2][3], 
		TestMatrix.M[3][0], TestMatrix.M[3][1], TestMatrix.M[3][2], TestMatrix.M[3][3]
		);

	TestMatrix = TestMatrix.GetTransposed().InverseFast();

	UE_LOG(LogLightmass, Display, TEXT("Inverted Mat=\n  [%0.2f, %0.2f, %0.2f, %0.2f]\n  [%0.2f, %0.2f, %0.2f, %0.2f]\n  [%0.2f, %0.2f, %0.2f, %0.2f]\n  [%0.2f, %0.2f, %0.2f, %0.2f]"), 
		TestMatrix.M[0][0], TestMatrix.M[0][1], TestMatrix.M[0][2], TestMatrix.M[0][3], 
		TestMatrix.M[1][0], TestMatrix.M[1][1], TestMatrix.M[1][2], TestMatrix.M[1][3], 
		TestMatrix.M[2][0], TestMatrix.M[2][1], TestMatrix.M[2][2], TestMatrix.M[2][3], 
		TestMatrix.M[3][0], TestMatrix.M[3][1], TestMatrix.M[3][2], TestMatrix.M[3][3]
		);

	UE_LOG(LogLightmass, Display, TEXT("sizeof FDirectionalLight = %d, FLight = %d, FDirectionalLightData = %d"), sizeof(FDirectionalLight), sizeof(FLight), sizeof(FDirectionalLightData));

	TOctree<float, FTestOctreeSemantics> TestOctree(FVector4(0), 10.0f);
	TestOctree.AddElement(5);


	// kDOP test
	TkDOPTree<FTestCollisionDataProvider, uint16> TestkDOP;
	FTestCollisionDataProvider TestDataProvider(TestkDOP);
	FHitResult TestResult;
	
	FkDOPBuildCollisionTriangle<uint16> TestTri(0, FVector4(0,0,0,0), FVector4(1,1,1,0), FVector4(2,2,2,0), INDEX_NONE, INDEX_NONE, INDEX_NONE, false, true);
	TArray<FkDOPBuildCollisionTriangle<uint16> > TestTriangles;
	TestTriangles.Add(TestTri);

	TestkDOP.Build(TestTriangles);
	
	UE_LOG(LogLightmass, Display, TEXT("\nStarting a thread"));
	FTestRunnable* TestRunnable = new FTestRunnable;
	// create a thread with the test runnable, and let it auto-delete itself
	FRunnableThread* TestThread = FRunnableThread::Create(TestRunnable, TEXT("TestRunnable"));


	double Start = FPlatformTime::Seconds();
	UE_LOG(LogLightmass, Display, TEXT("\nWaiting 4 seconds"), Start);
	FPlatformProcess::Sleep(4.0f);
	UE_LOG(LogLightmass, Display, TEXT("%.2f seconds have passed, killing thread"), FPlatformTime::Seconds() - Start);
	
	// wait for thread to end
	double KillStart = FPlatformTime::Seconds();
	TestRunnable->Stop();
	TestThread->WaitForCompletion();
		
	delete TestThread;
	delete TestRunnable;
	
	UE_LOG(LogLightmass, Display, TEXT("It took %.2f seconds to kill the thread [should be < 1 second]"), FPlatformTime::Seconds() - KillStart);


	UE_LOG(LogLightmass, Display, TEXT("\n\n"));

	checkf(5 == 2, TEXT("And boom goes the dynamite\n"));
}

} // namespace
