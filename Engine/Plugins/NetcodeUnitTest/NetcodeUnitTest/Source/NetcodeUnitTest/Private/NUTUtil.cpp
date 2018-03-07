// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NUTUtil.h"


#include "UnitTest.h"
#include "ProcessUnitTest.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/OutputDeviceHelper.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "ClientUnitTest.h"


// Globals

static FAssertHookDevice GAssertHook;


/**
 * FAssertHookDevice
 */

void FAssertHookDevice::AddAssertHook(FString Assert)
{
	// Hook GError when an assert hook is first added
	if (GError != &GAssertHook)
	{
		GAssertHook.HookDevice(GError);

		GError = &GAssertHook;
	}

	GAssertHook.DisabledAsserts.Add(Assert);
}


/**
 * NUTUtil
 */

void NUTUtil::GetUnitTestClassDefList(TArray<UUnitTest*>& OutUnitTestClassDefaults)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		// @todo #JohnBRefactor: Move all 'abstract class' checks to a central location, as you do this in multiple locations
		if (It->IsChildOf(UUnitTest::StaticClass()) && *It != UUnitTest::StaticClass() && *It != UClientUnitTest::StaticClass() &&
			*It != UProcessUnitTest::StaticClass())
		{
			UUnitTest* CurDefault = Cast<UUnitTest>(It->GetDefaultObject());

			if (CurDefault != NULL)
			{
				OutUnitTestClassDefaults.Add(CurDefault);
			}
		}
	}
}

void NUTUtil::SortUnitTestClassDefList(TArray<UUnitTest*>& InUnitTestClassDefaults)
{
	// @todo #JohnBRefactorLambda: Convert these inline sort functions to lambda's now
	struct FUnitTestDateSort
	{
		FORCEINLINE bool operator()(const UUnitTest& A, const UUnitTest& B) const
		{
			return (A.GetUnitTestDate() < B.GetUnitTestDate());
		}
	};

	struct FUnitTestTypeDateSort
	{
		FUnitTestTypeDateSort(TArray<FString>& InTypeOrder)
			: TypeOrder(InTypeOrder)
		{
		}

		FORCEINLINE bool operator()(const UUnitTest& A, const UUnitTest& B) const
		{
			bool bReturnVal = false;

			if (TypeOrder.IndexOfByKey(A.GetUnitTestType()) < TypeOrder.IndexOfByKey(B.GetUnitTestType()))
			{
				bReturnVal = true;
			}
			else if (TypeOrder.IndexOfByKey(A.GetUnitTestType()) == TypeOrder.IndexOfByKey(B.GetUnitTestType()) &&
						A.GetUnitTestDate() < B.GetUnitTestDate())
			{
				bReturnVal = true;
			}

			return bReturnVal;
		}

		/** The order with which the prioritize types */
		TArray<FString> TypeOrder;
	};


	// First reorder the unit test classes by date, then grab the unit test types by date, then group them by type/date
	TArray<FString> ListTypes;

	InUnitTestClassDefaults.Sort(FUnitTestDateSort());

	for (int i=0; i<InUnitTestClassDefaults.Num(); i++)
	{
		ListTypes.AddUnique(InUnitTestClassDefaults[i]->GetUnitTestType());
	}

	// Now sort again, based both on type and date
	InUnitTestClassDefaults.Sort(FUnitTestTypeDateSort(ListTypes));
}

void NUTUtil::SpecialLog(FOutputDeviceFile* Ar, const TCHAR* SpecialCategory, const TCHAR* Data, ELogVerbosity::Type Verbosity,
							const FName& Category)
{
	bool bOldEmitTerminator = Ar->GetAutoEmitLineTerminator();
	bool bOldSuppressEvent = Ar->GetSuppressEventTag();

	Ar->SetAutoEmitLineTerminator(false);

	// Log the timestamp, special category and verbosity/category tag (uses some log system hacks to achieve clean logs)
	FString SerializeStr = SpecialCategory;

	if (!bOldSuppressEvent)
	{
		if (Category != NAME_None)
		{
			SerializeStr += Category.ToString() + TEXT(":");
		}

		if (Verbosity != ELogVerbosity::Log)
		{
			SerializeStr += FOutputDeviceHelper::VerbosityToString(Verbosity);
			SerializeStr += TEXT(": ");
		}
		else if (Category != NAME_None)
		{
			SerializeStr += TEXT(" ");
		}
	}

	Ar->Serialize(*SerializeStr, ELogVerbosity::Log, NAME_None);

	Ar->SetAutoEmitLineTerminator(bOldEmitTerminator);
	Ar->SetSuppressEventTag(true);

	Ar->Serialize(Data, Verbosity, Category);

	Ar->SetSuppressEventTag(bOldSuppressEvent);
}


