// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"
#include "Containers/Algo/Copy.h"
#include "Containers/Algo/Heapify.h"
#include "Containers/Algo/HeapSort.h"
#include "Containers/Algo/IntroSort.h"
#include "Containers/Algo/IsHeap.h"
#include "Containers/Algo/IsSorted.h"
#include "Containers/Algo/Sort.h"
#include "Containers/Algo/Transform.h"
#include "Templates/Greater.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlgosTest, "System.Core.Misc.Algos", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

#define NUM_TEST_OBJECTS      32



bool FAlgosTest::RunTest(const FString& Parameters)
{
	struct FTestData
	{
		FTestData(FString&& InName, int32 InAge, bool bInRetired = false)
			: Name(MoveTemp(InName))
			, Age(InAge)
			, bRetired(bInRetired)
		{
		}

		bool IsTeenager() const
		{
			return Age >= 13 && Age <= 19;
		}

		FString GetName() const
		{
			return Name;
		}

		FString Name;
		int32 Age;
		bool bRetired;
	};

	// setup
	TArray<int> TestData;
	for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
	{
		TestData.Add(i);
	}
	TArray<int> TestData2;
	for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
	{
		TestData2.Add(FMath::Rand());
	}

	// copy
	{
		TArray<int> TestArray;
		// empty array
		Algo::Copy(TestData, TestArray);
		check(TestArray == TestData);
		// existing data
		Algo::Copy(TestData2, TestArray);
		check(TestArray.Num() == NUM_TEST_OBJECTS * 2);
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			check(TestArray[i] == TestData[i]);
		}
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			check(TestArray[i + NUM_TEST_OBJECTS] == TestData2[i]);
		}
	}

	// copy if
	{
		TArray<int> TestArray;
		// empty array
		Algo::CopyIf(TestData, TestArray, [](int i) { return (i % 2) == 0; });
		int j = 0;
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			if (TestData[i] % 2 == 0)
			{
				check(TestArray[j] == TestData[i]);
				++j;
			}
		}
		// existing data
		Algo::CopyIf(TestData2, TestArray, [](int i) { return (i % 2) == 0; });
		j = 0;
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			if (TestData[i] % 2 == 0)
			{
				check(TestArray[j] == TestData[i]);
				++j;
			}
		}
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			if (TestData2[i] % 2 == 0)
			{
				check(TestArray[j] == TestData2[i]);
				++j;
			}
		}
		check(j == TestArray.Num())
	}

	// transform
	{
		TArray<float> TestArray;

		// empty array
		{
			Algo::Transform(TestData, TestArray, [](int i) { return FMath::DegreesToRadians(i); });
			check(TestArray.Num() == NUM_TEST_OBJECTS);
			for (int i = 0; i < TestArray.Num(); ++i)
			{
				check(TestArray[i] == FMath::DegreesToRadians(TestData[i]));
			}
		}

		// existing data
		{
			Algo::Transform(TestData2, TestArray, [](int i) { return FMath::DegreesToRadians(i); });
			check(TestArray.Num() == NUM_TEST_OBJECTS * 2);
			for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
			{
				check(TestArray[i] == FMath::DegreesToRadians(TestData[i]));
			}
			for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
			{
				check(TestArray[i + NUM_TEST_OBJECTS] == FMath::DegreesToRadians(TestData2[i]));
			}
		}

		// projection via member function pointer
		{
			TArray<FString> Strings = {
				TEXT("Hello"),
				TEXT("this"),
				TEXT("is"),
				TEXT("a"),
				TEXT("projection"),
				TEXT("test")
			};

			TArray<int32> Lengths;
			Algo::Transform(Strings, Lengths, &FString::Len);
			check(Lengths == TArray<int32>({ 5, 4, 2, 1, 10, 4 }));
		}

		// projection via data member pointer
		{
			TArray<FTestData> Data = {
				FTestData(TEXT("Alice"), 31),
				FTestData(TEXT("Bob"), 25),
				FTestData(TEXT("Charles"), 19),
				FTestData(TEXT("Donna"), 13)
			};

			TArray<int32> Ages;
			Algo::Transform(Data, Ages, &FTestData::Age);

			check(Ages == TArray<int32>({ 31, 25, 19, 13 }));
		}

		// projection across smart pointers
		{
			TArray<TUniquePtr<FTestData>> Data;
			Data.Add(MakeUnique<FTestData>(TEXT("Elsa"), 61));
			Data.Add(MakeUnique<FTestData>(TEXT("Fred"), 11));
			Data.Add(MakeUnique<FTestData>(TEXT("Georgina"), 34));
			Data.Add(MakeUnique<FTestData>(TEXT("Henry"), 54));
			Data.Add(MakeUnique<FTestData>(TEXT("Ichabod"), 87));

			TArray<FString> Names;
			Algo::Transform(Data, Names, &FTestData::Name);

			TArray<FString> ExpectedNames = { TEXT("Elsa"), TEXT("Fred"), TEXT("Georgina"), TEXT("Henry"), TEXT("Ichabod") };
			check(Names == ExpectedNames);
		}
	}

	// transform if
	{
		TArray<float> TestArray;

		// empty array
		{
			Algo::TransformIf(TestData, TestArray, [](int i) { return (i % 2) == 0; }, [](int i) { return FMath::DegreesToRadians(i); });
			int j = 0;
			for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
			{
				if (TestData[i] % 2 == 0)
				{
					check(TestArray[j] == FMath::DegreesToRadians(TestData[i]));
					++j;
				}
			}
		}

		// existing data
		{
			Algo::TransformIf(TestData2, TestArray, [](int i) { return (i % 2) == 0; }, [](int i) { return FMath::DegreesToRadians(i); });
			int j = 0;
			for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
			{
				if (TestData[i] % 2 == 0)
				{
					check(TestArray[j] == FMath::DegreesToRadians(TestData[i]));
					++j;
				}
			}
			for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
			{
				if (TestData2[i] % 2 == 0)
				{
					check(TestArray[j] == FMath::DegreesToRadians(TestData2[i]));
					++j;
				}
			}
			check(j == TestArray.Num());
		}

		TArray<TUniquePtr<FTestData>> Data;
		Data.Add(MakeUnique<FTestData>(TEXT("Jeff"), 15, false));
		Data.Add(MakeUnique<FTestData>(TEXT("Katrina"), 77, true));
		Data.Add(MakeUnique<FTestData>(TEXT("Lenny"), 29, false));
		Data.Add(MakeUnique<FTestData>(TEXT("Michelle"), 13, false));
		Data.Add(MakeUnique<FTestData>(TEXT("Nico"), 65, true));

		// projection and transform via data member pointer
		{
			TArray<FString> NamesOfRetired;
			Algo::TransformIf(Data, NamesOfRetired, &FTestData::bRetired, &FTestData::Name);
			TArray<FString> ExpectedNamesOfRetired = { TEXT("Katrina"), TEXT("Nico") };
			check(NamesOfRetired == ExpectedNamesOfRetired);
		}

		// projection and transform via member function pointer
		{
			TArray<FString> NamesOfTeenagers;
			Algo::TransformIf(Data, NamesOfTeenagers, &FTestData::IsTeenager, &FTestData::GetName);
			TArray<FString> ExpectedNamesOfTeenagers = { TEXT("Jeff"), TEXT("Michelle") };
			check(NamesOfTeenagers == ExpectedNamesOfTeenagers);
		}
	}

	// binary search
	{
		// Verify static array case
		int StaticArray[] = { 2,4,6,6,6,8 };

		check(Algo::BinarySearch(StaticArray, 6) == 2);
		check(Algo::BinarySearch(StaticArray, 5) == INDEX_NONE);

		check(Algo::LowerBound(StaticArray, 6) == 2);
		check(Algo::LowerBound(StaticArray, 5) == 2);
		check(Algo::UpperBound(StaticArray, 6) == 5);
		check(Algo::LowerBound(StaticArray, 7) == 5);
		check(Algo::LowerBound(StaticArray, 9) == 6);

		// Dynamic array case
		TArray<int32> IntArray = { 2,2,4,4,6,6,6,8,8 };

		check(Algo::BinarySearch(IntArray, 6) == 4);
		check(Algo::BinarySearch(IntArray, 5) == INDEX_NONE);

		check(Algo::LowerBound(IntArray, 2) == 0);
		check(Algo::UpperBound(IntArray, 2) == 2);
		check(Algo::LowerBound(IntArray, 6) == 4);
		check(Algo::UpperBound(IntArray, 6) == 7);
		check(Algo::LowerBound(IntArray, 5) == 4);
		check(Algo::UpperBound(IntArray, 5) == 4);
		check(Algo::LowerBound(IntArray, 7) == 7);
		check(Algo::LowerBound(IntArray, 9) == 9);
	}

	// heapify
	{
		TArray<int> TestArray = TestData2;
		Algo::Heapify(TestArray);

		check(Algo::IsHeap(TestArray));
	}

	// heap sort
	{
		TArray<int> TestArray = TestData2;
		Algo::HeapSort(TestArray);

		check(Algo::IsHeap(TestArray));

		check(Algo::IsSorted(TestArray));
	}

	// intro sort
	{
		TArray<int> TestArray = TestData2;
		Algo::IntroSort(TestArray);

		check(Algo::IsSorted(TestArray));
	}

	// sort
	{
		// regular Sort
		TArray<int> TestArray = TestData2;
		Algo::Sort(TestArray);

		check(Algo::IsSorted(TestArray));

		// Sort with predicate
		TestArray = TestData2;

		TGreater<> Predicate;
		Algo::Sort(TestArray, Predicate);

		check(Algo::IsSorted(TestArray, Predicate));

		// SortBy
		TestArray = TestData2;

		auto Projection = [](int Val) -> int
		{
			return Val % 1000; // will sort using the last 3 digits only
		};

		Algo::SortBy(TestArray, Projection);

		check(Algo::IsSortedBy(TestArray, Projection));

		// SortBy with predicate
		TestArray = TestData2;

		Algo::SortBy(TestArray, Projection, Predicate);

		check(Algo::IsSortedBy(TestArray, Projection, Predicate));
	}

	return true;
}
