// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/Timespan.h"
#include "Misc/AutomationTest.h"


#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimespanTest, "System.Core.Misc.Timespan", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)


bool FTimespanTest::RunTest(const FString& Parameters)
{
	// constructors must create equal objects
	{
		FTimespan ts1_1 = FTimespan(3, 2, 1);
		FTimespan ts1_2 = FTimespan(0, 3, 2, 1);
		FTimespan ts1_3 = FTimespan(0, 3, 2, 1, 0);

		TestEqual(TEXT("Constructors must create equal objects (Hours/Minutes/Seconds vs. Days/Hours/Minutes/Seconds)"), ts1_1, ts1_2);
		TestEqual(TEXT("Constructors must create equal objects (Hours/Minutes/Seconds vs. Days/Hours/Minutes/Seconds/FractionNano)"), ts1_1, ts1_3);
	}

	// component getters must return correct values
	{
		FTimespan ts2_1 = FTimespan(1, 2, 3, 4, 123456789);

		TestEqual(TEXT("Component getters must return correct values (Days)"), ts2_1.GetDays(), 1);
		TestEqual(TEXT("Component getters must return correct values (Hours)"), ts2_1.GetHours(), 2);
		TestEqual(TEXT("Component getters must return correct values (Minutes)"), ts2_1.GetMinutes(), 3);
		TestEqual(TEXT("Component getters must return correct values (Seconds)"), ts2_1.GetSeconds(), 4);
		TestEqual(TEXT("Component getters must return correct values (FractionMilli)"), ts2_1.GetFractionMilli(), 123);
		TestEqual(TEXT("Component getters must return correct values (FractionMicro)"), ts2_1.GetFractionMicro(), 123456);
		TestEqual(TEXT("Component getters must return correct values (FractionNano)"), ts2_1.GetFractionNano(), 123456700);
	}

	// durations of positive and negative time spans must match
	{
		FTimespan ts3_1 = FTimespan(1, 2, 3, 4, 123456789);
		FTimespan ts3_2 = FTimespan(-1, -2, -3, -4, -123456789);

		TestEqual(TEXT("Durations of positive and negative time spans must match"), ts3_1.GetDuration(), ts3_2.GetDuration());
	}

	// static constructors must create correct values
	{
		TestEqual(TEXT("Static constructors must create correct values (FromDays)"), FTimespan::FromDays(123).GetTotalDays(), 123.0);
		TestEqual(TEXT("Static constructors must create correct values (FromHours)"), FTimespan::FromHours(123).GetTotalHours(), 123.0);
		TestEqual(TEXT("Static constructors must create correct values (FromMinutes)"), FTimespan::FromMinutes(123).GetTotalMinutes(), 123.0);
		TestEqual(TEXT("Static constructors must create correct values (FromSeconds)"), FTimespan::FromSeconds(123).GetTotalSeconds(), 123.0);
		TestEqual(TEXT("Static constructors must create correct values (FromMilliseconds)"), FTimespan::FromMilliseconds(123).GetTotalMilliseconds(), 123.0);
		TestEqual(TEXT("Static constructors must create correct values (FromMicroseconds)"), FTimespan::FromMicroseconds(123).GetTotalMicroseconds(), 123.0);
	}

	// string conversions must return correct strings
	{
		FTimespan ts5_1 = FTimespan(1, 2, 3, 4, 123456789);

		TestEqual<FString>(TEXT("String conversion (Default)"), ts5_1.ToString(), TEXT("+1.02:03:04.123"));
		TestEqual<FString>(TEXT("String conversion (%d.%h:%m:%s.%f)"), ts5_1.ToString(TEXT("%d.%h:%m:%s.%f")), TEXT("+1.02:03:04.123"));
		TestEqual<FString>(TEXT("String conversion (%d.%h:%m:%s.%u)"), ts5_1.ToString(TEXT("%d.%h:%m:%s.%u")), TEXT("+1.02:03:04.123456"));
		TestEqual<FString>(TEXT("String conversion (%D.%h:%m:%s.%n)"), ts5_1.ToString(TEXT("%D.%h:%m:%s.%n")), TEXT("+00000001.02:03:04.123456700"));
	}

	// parsing valid strings must succeed
	{
		FTimespan ts6_t;

		FTimespan ts6_1 = FTimespan(1, 2, 3, 4, 123000000);
		FTimespan ts6_2 = FTimespan(1, 2, 3, 4, 123456000);
		FTimespan ts6_3 = FTimespan(1, 2, 3, 4, 123456700);

		TestTrue(TEXT("Parsing valid strings must succeed (+1.02:03:04.123)"), FTimespan::Parse(TEXT("+1.02:03:04.123"), ts6_t));
		TestEqual(TEXT("Parsing valid strings must result in correct values (+1.02:03:04.123)"), ts6_t, ts6_1);

		TestTrue(TEXT("Parsing valid strings must succeed (+1.02:03:04.123456)"), FTimespan::Parse(TEXT("+1.02:03:04.123456"), ts6_t));
		TestEqual(TEXT("Parsing valid strings must result in correct values (+1.02:03:04.123456)"), ts6_t, ts6_2);

		TestTrue(TEXT("Parsing valid strings must succeed (+1.02:03:04.123456789)"), FTimespan::Parse(TEXT("+1.02:03:04.123456789"), ts6_t));
		TestEqual(TEXT("Parsing valid strings must result in correct values (+1.02:03:04.123456789)"), ts6_t, ts6_3);

		FTimespan ts6_4 = FTimespan(-1, -2, -3, -4, -123000000);
		FTimespan ts6_5 = FTimespan(-1, -2, -3, -4, -123456000);
		FTimespan ts6_6 = FTimespan(-1, -2, -3, -4, -123456700);

		TestTrue(TEXT("Parsing valid strings must succeed (-1.02:03:04.123)"), FTimespan::Parse(TEXT("-1.02:03:04.123"), ts6_t));
		TestEqual(TEXT("Parsing valid strings must result in correct values (-1.02:03:04.123)"), ts6_t, ts6_4);

		TestTrue(TEXT("Parsing valid strings must succeed (-1.02:03:04.123456)"), FTimespan::Parse(TEXT("-1.02:03:04.123456"), ts6_t));
		TestEqual(TEXT("Parsing valid strings must result in correct values (-1.02:03:04.123456)"), ts6_t, ts6_5);

		TestTrue(TEXT("Parsing valid strings must succeed (-1.02:03:04.123456789)"), FTimespan::Parse(TEXT("-1.02:03:04.123456789"), ts6_t));
		TestEqual(TEXT("Parsing valid strings must result in correct values (-1.02:03:04.123456789)"), ts6_t, ts6_6);
	}

	// parsing invalid strings must fail
	{
		FTimespan ts7_1;

		//TestFalse(TEXT("Parsing invalid strings must fail (1,02:03:04.005)"), FTimespan::Parse(TEXT("1,02:03:04.005"), ts7_1));
		//TestFalse(TEXT("Parsing invalid strings must fail (1.1.02:03:04:005)"), FTimespan::Parse(TEXT("1.1.02:03:04:005"), ts7_1));
		//TestFalse(TEXT("Parsing invalid strings must fail (04:005)"), FTimespan::Parse(TEXT("04:005"), ts7_1));
	}

	return true;
}


#endif //WITH_DEV_AUTOMATION_TESTS
