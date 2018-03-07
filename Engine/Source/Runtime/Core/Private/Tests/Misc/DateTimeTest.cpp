// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "Misc/AutomationTest.h"


#if WITH_DEV_AUTOMATION_TESTS

#define TestUnixEquivalent(Desc, A, B) if ((A).ToUnixTimestamp() != (B)) AddError(FString::Printf(TEXT("%s - A=%d B=%d"), Desc, (A).ToUnixTimestamp(), (B)));
#define TestYear(Desc, A, B) if ((A.GetYear()) != (B)) AddError(FString::Printf(TEXT("%s - A=%d B=%d"), Desc, (A.GetYear()), (B)));
#define TestMonth(Desc, A, B) if ((A.GetMonth()) != (B)) AddError(FString::Printf(TEXT("%s - A=%d B=%d"), Desc, (A.GetMonth()), B));
#define TestMonthOfYear(Desc, A, B) if ((A.GetMonthOfYear()) != (B)) AddError(FString::Printf(TEXT("%s - A=%d B=%d"), Desc ,(static_cast<int32>(A.GetMonthOfYear())), static_cast<int32>(B)));
#define TestDay(Desc, A, B) if ((A.GetDay()) != (B)) AddError(FString::Printf(TEXT("%s - A=%d B=%d"), Desc, (A.GetDay()), (B)));
#define TestHour(Desc, A, B) if ((A.GetHour()) != (B)) AddError(FString::Printf(TEXT("%s - A=%d B=%d"), Desc, (A.GetHour()), (B)));
#define TestMinute(Desc, A, B) if ((A.GetMinute()) != (B)) AddError(FString::Printf(TEXT("%s - A=%d B=%d"), Desc, (A.GetMinute()), (B)));
#define TestSecond(Desc, A, B) if ((A.GetSecond()) != (B)) AddError(FString::Printf(TEXT("%s - A=%d B=%d"), Desc, (A.GetSecond()), (B)));
#define TestMillisecond(Desc, A, B) if ((A.GetMillisecond()) != (B)) AddError(FString::Printf(TEXT("%s - A=%d B=%d"), Desc, (A.GetMillisecond()) ,(B)));


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDateTimeTest, "System.Core.Misc.DateTime", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)


bool FDateTimeTest::RunTest(const FString& Parameters)
{
	const int64 UnixEpochTimestamp = 0;
	const int64 UnixBillenniumTimestamp = 1000000000;
	const int64 UnixOnesTimestamp = 1111111111;
	const int64 UnixDecimalSequenceTimestamp = 1234567890;

	const FDateTime UnixEpoch = FDateTime::FromUnixTimestamp(UnixEpochTimestamp);
	const FDateTime UnixBillennium = FDateTime::FromUnixTimestamp(UnixBillenniumTimestamp);
	const FDateTime UnixOnes = FDateTime::FromUnixTimestamp(UnixOnesTimestamp);
	const FDateTime UnixDecimalSequence = FDateTime::FromUnixTimestamp(UnixDecimalSequenceTimestamp);

	const FDateTime TestDateTime(2013, 8, 14, 12, 34, 56, 789);
	
	TestUnixEquivalent(TEXT("Testing Unix Epoch Ticks"), UnixEpoch, UnixEpochTimestamp);
	TestUnixEquivalent(TEXT("Testing Unix Billennium Ticks"), UnixBillennium, UnixBillenniumTimestamp);
	TestUnixEquivalent(TEXT("Testing Unix Ones Ticks"), UnixOnes, UnixOnesTimestamp);
	TestUnixEquivalent(TEXT("Testing Unix Decimal Sequence Ticks"), UnixDecimalSequence, UnixDecimalSequenceTimestamp);

	TestYear(TEXT("Testing Unix Epoch Year"), UnixEpoch, 1970);
	TestMonth(TEXT("Testing Unix Epoch Month"), UnixEpoch, 1);
	TestMonthOfYear(TEXT("Testing Unix Epoch MonthOfYear"), UnixEpoch, EMonthOfYear::January);
	TestDay(TEXT("Testing Unix Epoch Day"), UnixEpoch, 1);
	TestHour(TEXT("Testing Unix Epoch Hour"), UnixEpoch, 0);
	TestMinute(TEXT("Testing Unix Epoch Minute"), UnixEpoch, 0);
	TestSecond(TEXT("Testing Unix Epoch Second"), UnixEpoch, 0);
	TestMillisecond(TEXT("Testing Unix Epoch Millisecond"), UnixEpoch, 0);

	TestYear(TEXT("Testing Unix Billennium Year"), UnixBillennium, 2001);
	TestMonth(TEXT("Testing Unix Billennium MonthOfYear"), UnixBillennium, 9);
	TestMonthOfYear(TEXT("Testing Unix Billennium Month"), UnixBillennium, EMonthOfYear::September);
	TestDay(TEXT("Testing Unix Billennium Day"), UnixBillennium, 9);
	TestHour(TEXT("Testing Unix Billennium Hour"), UnixBillennium, 1);
	TestMinute(TEXT("Testing Unix Billennium Minute"), UnixBillennium, 46);
	TestSecond(TEXT("Testing Unix Billennium Second"), UnixBillennium, 40);
	TestMillisecond(TEXT("Testing Unix Billennium Millisecond"), UnixBillennium, 0);

	TestYear(TEXT("Testing Unix Ones Year"), UnixOnes, 2005);
	TestMonth(TEXT("Testing Unix Ones Month"), UnixOnes, 3);
	TestMonthOfYear(TEXT("Testing Unix Ones MonthOfYear"), UnixOnes, EMonthOfYear::March);
	TestDay(TEXT("Testing Unix Ones Day"), UnixOnes, 18);
	TestHour(TEXT("Testing Unix Ones Hour"), UnixOnes, 1);
	TestMinute(TEXT("Testing Unix Ones Minute"), UnixOnes, 58);
	TestSecond(TEXT("Testing Unix Ones Second"), UnixOnes, 31);
	TestMillisecond(TEXT("Testing Unix Ones Millisecond"), UnixOnes, 0);

	TestYear(TEXT("Testing Unix Decimal Sequence Year"), UnixDecimalSequence, 2009);
	TestMonth(TEXT("Testing Unix Decimal Sequence Month"), UnixDecimalSequence, 2);
	TestMonthOfYear(TEXT("Testing Unix Decimal Sequence MonthOfYear"), UnixDecimalSequence, EMonthOfYear::February);
	TestDay(TEXT("Testing Unix Decimal Sequence Day"), UnixDecimalSequence, 13);
	TestHour(TEXT("Testing Unix Decimal Sequence Hour"), UnixDecimalSequence, 23);
	TestMinute(TEXT("Testing Unix Decimal Sequence Minute"), UnixDecimalSequence, 31);
	TestSecond(TEXT("Testing Unix Decimal Sequence Second"), UnixDecimalSequence, 30);
	TestMillisecond(TEXT("Testing Unix Decimal Sequence Millisecond"), UnixDecimalSequence, 0);

	TestYear(TEXT("Testing Test Date Time Year"), TestDateTime, 2013);
	TestMonth(TEXT("Testing Test Date Time Month"), TestDateTime, 8);
	TestMonthOfYear(TEXT("Testing Test Date Time MonthOfYear"), TestDateTime, EMonthOfYear::August);
	TestDay(TEXT("Testing Test Date Time Day"), TestDateTime, 14);
	TestHour(TEXT("Testing Test Date Time Hour"), TestDateTime, 12);
	TestMinute(TEXT("Testing Test Date Time Minute"), TestDateTime, 34);
	TestSecond(TEXT("Testing Test Date Time Second"), TestDateTime, 56);
	TestMillisecond(TEXT("Testing Test Date Time Millisecond"), TestDateTime, 789);

	FDateTime ParsedDateTime;

	TestFalse(TEXT("Parsing an empty ISO string must fail"), FDateTime::ParseIso8601(TEXT(""), ParsedDateTime));

	return true;
}


#undef TestUnixEquivalent
#undef TestYear
#undef TestMonth
#undef TestDay
#undef TestHour
#undef TestMinute
#undef TestSecond
#undef TestMillisecond


#endif //WITH_DEV_AUTOMATION_TESTS
