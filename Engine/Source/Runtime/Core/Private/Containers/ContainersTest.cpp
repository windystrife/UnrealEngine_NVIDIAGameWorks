// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/SortedMap.h"
#include "Containers/ArrayView.h"
#include "Misc/AutomationTest.h"
#include "Stats/StatsMisc.h"
#include "Math/RandomStream.h"

#define MAX_TEST_OBJECTS      65
#define MAX_TEST_OBJECTS_STEP 1
#define RANDOM_SEED 12345

namespace
{
	namespace EContainerTestType
	{
		enum Type
		{
			MovedFrom,
			Key,
			Value
		};
	}

	struct FContainerTestStats
	{
		int32  NextId;
		int32  ConstructedIDs[MAX_TEST_OBJECTS];
		int32* End;

		FContainerTestStats()
		{
			Reset();
		}

		void Reset()
		{
			NextId = 1;
			End    = ConstructedIDs;
		}

		int32 Num() const
		{
			return End - ConstructedIDs;
		}

		int32 Add()
		{
			// Ensure we're not constructing too many objects
			check(Num() < MAX_TEST_OBJECTS);

			// Store ID in array
			return *End++ = NextId++;
		}

		void Remove(int32 ObjId)
		{
			for (int32* It = ConstructedIDs; It != End; ++It)
			{
				if (*It != ObjId)
					continue;

				// Remove this from the list
				--End;
				for (; It != End; ++It)
				{
					*It = *(It + 1);
				}
				return;
			}

			// We didn't find an entry for this - an invalid destructor call?
			check(false);
		}
	} ContainerTestStats;

	struct FContainerTestType
	{
		FContainerTestType(const TCHAR* InStr, EContainerTestType::Type InType)
			: Str (InStr)
			, Type(InType)
			, Id  (ContainerTestStats.Add())
		{
		}

		FContainerTestType(const FContainerTestType& Other)
			: Str (Other.Str)
			, Type(Other.Type)
			, Id  (ContainerTestStats.Add())
		{
		}

		FContainerTestType(FContainerTestType&& Other)
			: Str (Other.Str)
			, Type(Other.Type)
			, Id  (ContainerTestStats.Add())
		{
			Other.Str  = NULL;
			Other.Type = EContainerTestType::MovedFrom;
		}

		FContainerTestType& operator=(FContainerTestType&& Other)
		{
			Str  = Other.Str;
			Type = Other.Type;

			Other.Str  = NULL;
			Other.Type = EContainerTestType::MovedFrom;
			
			return *( static_cast<FContainerTestType*>( this ) );
		}

		FContainerTestType& operator=(const FContainerTestType& Other)
		{
			Str  = Other.Str;
			Type = Other.Type;
			
			return *( static_cast<FContainerTestType*>( this ) );
		}

		~FContainerTestType()
		{
			ContainerTestStats.Remove(Id);
		}

		friend bool operator==(const FContainerTestType& Lhs, const FContainerTestType& Rhs)
		{
			return Lhs.Type == Rhs.Type &&
			       Lhs.Id   == Rhs.Id   &&
			       !FCString::Strcmp(Lhs.Str, Rhs.Str);
		}

		friend bool operator!=(const FContainerTestType& Lhs, const FContainerTestType& Rhs)
		{
			return !(Lhs == Rhs);
		}

		const TCHAR*             Str;
		EContainerTestType::Type Type;
		int32                    Id;
	};

	struct FContainerTestKeyType : FContainerTestType
	{
		FContainerTestKeyType()
			: FContainerTestType(TEXT("<default key>"), EContainerTestType::Key)
		{
		}

		explicit FContainerTestKeyType(const TCHAR* InStr)
			: FContainerTestType(InStr, EContainerTestType::Key)
		{
		}

		FContainerTestKeyType(FContainerTestKeyType&& Other)
			: FContainerTestType(MoveTemp(Other))
		{
		}

		FContainerTestKeyType(const FContainerTestKeyType& Other)
			: FContainerTestType(Other)
		{
		}

		FContainerTestKeyType& operator=(FContainerTestKeyType&& Other)
		{
			(FContainerTestType&)*this = MoveTemp(Other);
			
			return *( static_cast<FContainerTestKeyType*>( this ) );
		}

		FContainerTestKeyType& operator=(const FContainerTestKeyType& Other)
		{
			(FContainerTestType&)*this = Other;
			
			return *( static_cast<FContainerTestKeyType*>( this ) );
		}
	};

	struct FContainerTestValueType : FContainerTestType
	{
		FContainerTestValueType()
			: FContainerTestType(TEXT("<default value>"), EContainerTestType::Value)
		{
		}

		explicit FContainerTestValueType(const TCHAR* InStr)
			: FContainerTestType(InStr, EContainerTestType::Value)
		{
		}

		FContainerTestValueType(FContainerTestValueType&& Other)
			: FContainerTestType(MoveTemp(Other))
		{
		}

		FContainerTestValueType(const FContainerTestValueType& Other)
			: FContainerTestType(Other)
		{
		}

		FContainerTestValueType& operator=(FContainerTestValueType&& Other)
		{
			(FContainerTestType&)*this = MoveTemp(Other);
			
			return *( static_cast<FContainerTestValueType*>( this ) );
		}

		FContainerTestValueType& operator=(const FContainerTestValueType& Other)
		{
			(FContainerTestType&)*this = Other;
			
			return *( static_cast<FContainerTestValueType*>( this ) );
		}
	};

	template <typename Container>
	void CheckContainerElements(Container& Cont)
	{
		auto It  = Cont.CreateIterator();
		auto CIt = Cont.CreateConstIterator();
		for (auto& E : Cont)
		{
			check(*It  == E);
			check(*CIt == E);

			++It;
			++CIt;
		}
	}

	template <typename Container>
	void CheckContainerNum(Container& Cont)
	{
		int32 Count = 0;
		for (auto It = Cont.CreateIterator(); It; ++It)
		{
			++Count;
		}

		int32 CCount = 0;
		for (auto It = Cont.CreateConstIterator(); It; ++It)
		{
			++CCount;
		}

		int32 RCount = 0;
		for (auto& It : Cont)
		{
			++RCount;
		}

		check(Count  == Cont.Num());
		check(CCount == Cont.Num());
		check(RCount == Cont.Num());
	}

	template <typename Container>
	void CheckContainerEnds(Container& Cont)
	{
		auto Iter  = Cont.CreateIterator();
		auto CIter = Cont.CreateConstIterator();

		for (int32 Num = Cont.Num(); Num; --Num)
		{
			++Iter;
			++CIter;
		}

		check(!Iter);
		check(!CIter);
	}

	template <typename KeyType>
	KeyType GenerateTestKey(int32 Input)
	{
		return (KeyType)Input;
	}

	template <>
	FName GenerateTestKey<FName>(int32 Input)
	{
		// Don't use _foo as we want to test the slower compare path
		return FName(*FString::Printf(TEXT("TestName%d"), Input));
	}

	template <>
	FString GenerateTestKey<FString>(int32 Input)
	{
		return FString::Printf(TEXT("TestString%d"), Input);
	}

	template <typename ContainerType, typename KeyType>
	void RunContainerTests()
	{
		ContainerType Cont;

		ContainerTestStats.Reset();
		// Subtract one to account for temporaries that will be created during an Add
		for (int32 Count = 0; Count < MAX_TEST_OBJECTS - 1; Count += MAX_TEST_OBJECTS_STEP)
		{
			for (int32 N = 0; N != Count; ++N)
			{
				Cont.Add(GenerateTestKey<KeyType>(N), FContainerTestValueType(TEXT("New Value")));
				CheckContainerNum(Cont);
				CheckContainerEnds(Cont);
				CheckContainerElements(Cont);
			}

			for (int32 N = 0; N != Count; ++N)
			{
				Cont.Remove(GenerateTestKey<KeyType>(N));
				CheckContainerNum(Cont);
				CheckContainerEnds(Cont);
				CheckContainerElements(Cont);
			}

			for (int32 N = 0; N != Count; ++N)
			{
				Cont.Add(GenerateTestKey<KeyType>((Count - 1) - N), FContainerTestValueType(TEXT("New Value")));
				CheckContainerNum(Cont);
				CheckContainerEnds(Cont);
				CheckContainerElements(Cont);
			}

			for (int32 N = 0; N != Count; ++N)
			{
				Cont.Remove(GenerateTestKey<KeyType>(N));
				CheckContainerNum(Cont);
				CheckContainerEnds(Cont);
				CheckContainerElements(Cont);
			}
		}
	}

	template <typename ContainerType, typename KeyType>
	void RunPerformanceTest(const FString& Description, int32 NumObjects, int32 NumOperations)
	{
		ContainerTestStats.Reset();
		
		ContainerType Cont;
		FRandomStream RandomStream(RANDOM_SEED);

		// Prep keys, not part of performance test
		TArray<KeyType> KeyArray;
		KeyArray.Reserve(NumObjects);

		for (int32 i = 0; i < NumObjects; i++)
		{
			KeyArray.Add(GenerateTestKey<KeyType>(i));
		}

		for (int32 i = 0; i < NumObjects; i++)
		{
			int32 SwapIndex = RandomStream.RandRange(0, NumObjects - 1);
			if (i != SwapIndex)
			{
				KeyArray.Swap(i, SwapIndex);
			}
		}

		FScopeLogTime LogTimePtr(*FString::Printf(TEXT("%s objects=%d count=%d"), *Description, NumObjects, NumOperations), nullptr, FScopeLogTime::ScopeLog_Milliseconds);

		// Add elements in stably randomized order
		for (int32 i = 0; i < NumObjects; i++)
		{
			Cont.Add(KeyArray[i], FString(TEXT("New Value")));
		}
		
		// Now do searches
		for (int32 i = 0; i < NumOperations; i++)
		{
			KeyType& Key = KeyArray[RandomStream.RandRange(0, NumObjects - 1)];

			FString* FoundValue = Cont.Find(Key);
			check(FoundValue);
		}
	}

	template <typename ContainerType, typename KeyType>
	void RunSetPerformanceTest(const FString& Description, int32 NumObjects, int32 NumOperations)
	{
		ContainerTestStats.Reset();

		ContainerType Cont;
		FRandomStream RandomStream(RANDOM_SEED);

		// Prep keys, not part of performance test
		TArray<KeyType> KeyArray;
		KeyArray.Reserve(NumObjects);

		for (int32 i = 0; i < NumObjects; i++)
		{
			KeyArray.Add(GenerateTestKey<KeyType>(i));
		}

		for (int32 i = 0; i < NumObjects; i++)
		{
			int32 SwapIndex = RandomStream.RandRange(0, NumObjects - 1);
			if (i != SwapIndex)
			{
				KeyArray.Swap(i, SwapIndex);
			}
		}

		FScopeLogTime LogTimePtr(*FString::Printf(TEXT("%s objects=%d count=%d"), *Description, NumObjects, NumOperations), nullptr, FScopeLogTime::ScopeLog_Milliseconds);

		// Add elements in stably randomized order
		for (int32 i = 0; i < NumObjects; i++)
		{
			Cont.Add(KeyArray[i]);
		}

		// Now do searches
		for (int32 i = 0; i < NumOperations; i++)
		{
			KeyType& Key = KeyArray[RandomStream.RandRange(0, NumObjects - 1)];

			bool FoundValue = Cont.Contains(Key);
			check(FoundValue);
		}
	}

	template<typename TValueType>
	struct FCaseSensitiveLookupKeyFuncs : BaseKeyFuncs<TValueType, FString>
	{
		static FORCEINLINE const FString& GetSetKey(const TPair<FString, TValueType>& Element)
		{
			return Element.Key;
		}
		static FORCEINLINE bool Matches(const FString& A, const FString& B)
		{
			return A.Equals(B, ESearchCase::CaseSensitive);
		}
		static FORCEINLINE uint32 GetKeyHash(const FString& Key)
		{
			return FCrc::StrCrc32<TCHAR>(*Key);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContainersSmokeTest, "System.Core.Containers.Smoke", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FContainersSmokeTest::RunTest( const FString& Parameters )
{
	RunContainerTests<TMap<int32, FContainerTestValueType>, int32>();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContainersFullTest, "System.Core.Containers.Full", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FContainersFullTest::RunTest(const FString& Parameters)
{
	RunContainerTests<TMap<int32, FContainerTestValueType>, int32>();
	RunContainerTests<TMap<FName, FContainerTestValueType>, FName>();
	RunContainerTests<TMap<FString, FContainerTestValueType>, FString>();
	RunContainerTests<TMap<int32, FContainerTestValueType, TInlineSetAllocator<64>>, int32>();
	RunContainerTests<TMap<FString, FContainerTestValueType, FDefaultSetAllocator, FCaseSensitiveLookupKeyFuncs<FContainerTestValueType>>, FString>();

	RunContainerTests<TSortedMap<int32, FContainerTestValueType>, int32>();
	RunContainerTests<TSortedMap<FName, FContainerTestValueType>, FName>();
	RunContainerTests<TSortedMap<FString, FContainerTestValueType>, FString>();
	RunContainerTests<TSortedMap<FString, FContainerTestValueType, TInlineAllocator<64>>, FString>();

	// Verify use of FName index sorter with SortedMap

	TSortedMap<FName, int32, FDefaultAllocator, FNameSortIndexes> NameMap;
	NameMap.Add(NAME_NameProperty);
	NameMap.Add(NAME_FloatProperty);
	NameMap.Add(NAME_None);
	NameMap.Add(NAME_IntProperty);

	auto It = NameMap.CreateConstIterator();

	check(It->Key == NAME_None); ++It;
	check(It->Key == NAME_IntProperty); ++It;
	check(It->Key == NAME_FloatProperty); ++It;
	check(It->Key == NAME_NameProperty); ++It;
	check(!It);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContainerPerformanceTest, "System.Core.Containers.Performance", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FContainerPerformanceTest::RunTest(const FString& Parameters)
{
	RunPerformanceTest<TMap<int32, FString>, int32>(TEXT("TMap int32"), 1, 1000000);
	RunPerformanceTest<TMap<int32, FString>, int32>(TEXT("TMap int32"), 10, 1000000);
	RunPerformanceTest<TMap<int32, FString>, int32>(TEXT("TMap int32"), 100, 1000000);
	RunPerformanceTest<TMap<int32, FString>, int32>(TEXT("TMap int32"), 1000, 1000000);
	RunPerformanceTest<TMap<int32, FString>, int32>(TEXT("TMap int32"), 10000, 1000000);

	RunPerformanceTest<TMap<FName, FString>, FName>(TEXT("TMap FName"), 1, 1000000);
	RunPerformanceTest<TMap<FName, FString>, FName>(TEXT("TMap FName"), 10, 1000000);
	RunPerformanceTest<TMap<FName, FString>, FName>(TEXT("TMap FName"), 100, 1000000);
	RunPerformanceTest<TMap<FName, FString>, FName>(TEXT("TMap FName"), 1000, 1000000);
	RunPerformanceTest<TMap<FName, FString>, FName>(TEXT("TMap FName"), 10000, 1000000);

	RunPerformanceTest<TMap<FString, FString>, FString>(TEXT("TMap FString"), 1, 1000000);
	RunPerformanceTest<TMap<FString, FString>, FString>(TEXT("TMap FString"), 10, 1000000);
	RunPerformanceTest<TMap<FString, FString>, FString>(TEXT("TMap FString"), 100, 1000000);
	RunPerformanceTest<TMap<FString, FString>, FString>(TEXT("TMap FString"), 1000, 1000000);
	RunPerformanceTest<TMap<FString, FString>, FString>(TEXT("TMap FString"), 10000, 1000000);

	RunPerformanceTest<TSortedMap<int32, FString>, int32>(TEXT("TSortedMap int32"), 1, 1000000);
	RunPerformanceTest<TSortedMap<int32, FString>, int32>(TEXT("TSortedMap int32"), 10, 1000000);
	RunPerformanceTest<TSortedMap<int32, FString>, int32>(TEXT("TSortedMap int32"), 100, 1000000);
	RunPerformanceTest<TSortedMap<int32, FString>, int32>(TEXT("TSortedMap int32"), 1000, 1000000);
	RunPerformanceTest<TSortedMap<int32, FString>, int32>(TEXT("TSortedMap int32"), 10000, 1000000);

	RunPerformanceTest<TSortedMap<FName, FString>, FName>(TEXT("TSortedMap FName"), 1, 1000000);
	RunPerformanceTest<TSortedMap<FName, FString>, FName>(TEXT("TSortedMap FName"), 10, 1000000);
	RunPerformanceTest<TSortedMap<FName, FString>, FName>(TEXT("TSortedMap FName"), 100, 1000000);
	RunPerformanceTest<TSortedMap<FName, FString>, FName>(TEXT("TSortedMap FName"), 1000, 1000000);
	RunPerformanceTest<TSortedMap<FName, FString>, FName>(TEXT("TSortedMap FName"), 10000, 1000000);

	RunPerformanceTest<TSortedMap<FString, FString>, FString>(TEXT("TSortedMap FString"), 1, 1000000);
	RunPerformanceTest<TSortedMap<FString, FString>, FString>(TEXT("TSortedMap FString"), 10, 1000000);
	RunPerformanceTest<TSortedMap<FString, FString>, FString>(TEXT("TSortedMap FString"), 100, 1000000);
	RunPerformanceTest<TSortedMap<FString, FString>, FString>(TEXT("TSortedMap FString"), 1000, 1000000);
	RunPerformanceTest<TSortedMap<FString, FString>, FString>(TEXT("TSortedMap FString"), 10000, 1000000);

	RunSetPerformanceTest<TSet<FName>, FName>(TEXT("TSet FName"), 1, 1000000);
	RunSetPerformanceTest<TSet<FName>, FName>(TEXT("TSet FName"), 10, 1000000);
	RunSetPerformanceTest<TSet<FName>, FName>(TEXT("TSet FName"), 100, 1000000);

	RunSetPerformanceTest<TArray<FName>, FName>(TEXT("TArray FName"), 1, 1000000);
	RunSetPerformanceTest<TArray<FName>, FName>(TEXT("TArray FName"), 10, 1000000);
	RunSetPerformanceTest<TArray<FName>, FName>(TEXT("TArray FName"), 100, 1000000);

	return true;
}

namespace ArrayViewTests
{
	// commented out lines shouldn't compile

	struct Base
	{
		int32 b;
	};

	struct Derived : public Base
	{
		int32 d;
	};

	template<typename T>
	void TestFunction(TArrayView<T>)
	{
	}

	bool RunTest()
	{
		// C array + derived-to-base conversions
		Derived test1[13];
		TestFunction<Derived>(test1);
		//TestFunction<Base>(test1);
		//TestFunction<const Base>(test1);

		// C array of pointers + derived-to-base conversions
		Derived* test2[13];
		TestFunction<const Derived* const>(test2);
		//TestFunction<const Derived*>(test2);
		TestFunction<Derived* const>(test2);
		//TestFunction<const Base* const>(test2);

		// TArray + derived-to-base conversions
		TArray<Derived> test3;
		TestFunction<Derived>(test3);
		//TestFunction<Base>(test3);
		//TestFunction<const Base>(test3);

		// const TArray
		const TArray<Base> test4;
		TestFunction<const Base>(test4);
		//TestFunction<Base>(test4);

		// TArray of const
		TArray<const Base> test5;
		TestFunction<const Base>(test5);
		//TestFunction<Base>(test5);

		// temporary C array
		struct Test6
		{
			Base test[13];
		};
		TestFunction<const Base>(Test6().test);
		//TestFunction<Base>(Test6().test); // shouldn't compile but VS allows it :(

		return true;
	}
};
