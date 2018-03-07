// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"
#include "Templates/TypeHash.h"
#include "UObject/PropertyPortFlags.h"


/* FDateTime constants
 *****************************************************************************/

const int32 FDateTime::DaysPerMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
const int32 FDateTime::DaysToMonth[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };


/* FDateTime structors
 *****************************************************************************/

FDateTime::FDateTime(int32 Year, int32 Month, int32 Day, int32 Hour, int32 Minute, int32 Second, int32 Millisecond)
{
	check(Validate(Year, Month, Day, Hour, Minute, Second, Millisecond));

	int32 TotalDays = 0;

	if ((Month > 2) && IsLeapYear(Year))
	{
		++TotalDays;
	}

	--Year;											// the current year is not a full year yet
	--Month;										// the current month is not a full month yet

	TotalDays += Year * 365;
	TotalDays += Year / 4;							// leap year day every four years...
	TotalDays -= Year / 100;						// ...except every 100 years...
	TotalDays += Year / 400;						// ...but also every 400 years
	TotalDays += DaysToMonth[Month];				// days in this year up to last month
	TotalDays += Day - 1;							// days in this month minus today

	Ticks = TotalDays * ETimespan::TicksPerDay
		+ Hour * ETimespan::TicksPerHour
		+ Minute * ETimespan::TicksPerMinute
		+ Second * ETimespan::TicksPerSecond
		+ Millisecond * ETimespan::TicksPerMillisecond;
}


/* FDateTime interface
 *****************************************************************************/

bool FDateTime::ExportTextItem(FString& ValueStr, FDateTime const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
	{
		ValueStr += FString::Printf(TEXT("FDateTime(0x%016X)"), Ticks);
		return true;
	}

	ValueStr += ToString();

	return true;
}


void FDateTime::GetDate(int32& OutYear, int32& OutMonth, int32& OutDay) const
{
	// Based on FORTRAN code in:
	// Fliegel, H. F. and van Flandern, T. C.,
	// Communications of the ACM, Vol. 11, No. 10 (October 1968).

	int32 i, j, k, l, n;

	l = FMath::FloorToInt(GetJulianDay() + 0.5) + 68569;
	n = 4 * l / 146097;
	l = l - (146097 * n + 3) / 4;
	i = 4000 * (l + 1) / 1461001;
	l = l - 1461 * i / 4 + 31;
	j = 80 * l / 2447;
	k = l - 2447 * j / 80;
	l = j / 11;
	j = j + 2 - 12 * l;
	i = 100 * (n - 49) + i + l;

	OutYear = i;
	OutMonth = j;
	OutDay = k;
}


int32 FDateTime::GetDay() const
{
	int32 Year, Month, Day;
	GetDate(Year, Month, Day);

	return Day;
}


EDayOfWeek FDateTime::GetDayOfWeek() const
{
	// January 1, 0001 was a Monday
	return static_cast<EDayOfWeek>((Ticks / ETimespan::TicksPerDay) % 7);
}


int32 FDateTime::GetDayOfYear() const
{
	int32 Year, Month, Day;
	GetDate(Year, Month, Day);

	for (int32 CurrentMonth = 1; CurrentMonth < Month; ++CurrentMonth)
	{
		Day += DaysInMonth(Year, CurrentMonth);
	}

	return Day;
}


int32 FDateTime::GetHour12() const
{
	int32 Hour = GetHour();

	if (Hour < 1)
	{
		return 12;
	}

	if (Hour > 12)
	{
		return (Hour - 12);
	}

	return Hour;
}


int32 FDateTime::GetMonth() const
{
	int32 Year, Month, Day;
	GetDate(Year, Month, Day);

	return Month;
}


int32 FDateTime::GetYear() const
{
	int32 Year, Month, Day;
	GetDate(Year, Month, Day);

	return Year;
}


bool FDateTime::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	const int32 ExportDateTimeLen = 19;

	if (FPlatformString::Strlen(Buffer) < ExportDateTimeLen)
	{
		return false;
	}

	if (!Parse(FString(Buffer).Left(ExportDateTimeLen), *this))
	{
		return false;
	}

	Buffer += ExportDateTimeLen;

	return true;
}


bool FDateTime::Serialize(FArchive& Ar)
{
	Ar << *this;

	return true;
}


FString FDateTime::ToHttpDate() const
{
	FString DayStr;
	FString MonthStr;

	switch (GetDayOfWeek())
	{
		case EDayOfWeek::Monday:	DayStr = TEXT("Mon");	break;
		case EDayOfWeek::Tuesday:	DayStr = TEXT("Tue");	break;
		case EDayOfWeek::Wednesday:	DayStr = TEXT("Wed");	break;
		case EDayOfWeek::Thursday:	DayStr = TEXT("Thu");	break;
		case EDayOfWeek::Friday:	DayStr = TEXT("Fri");	break;
		case EDayOfWeek::Saturday:	DayStr = TEXT("Sat");	break;
		case EDayOfWeek::Sunday:	DayStr = TEXT("Sun");	break;
	}

	switch (GetMonthOfYear())
	{
		case EMonthOfYear::January:		MonthStr = TEXT("Jan");	break;
		case EMonthOfYear::February:	MonthStr = TEXT("Feb");	break;
		case EMonthOfYear::March:		MonthStr = TEXT("Mar");	break;
		case EMonthOfYear::April:		MonthStr = TEXT("Apr");	break;
		case EMonthOfYear::May:			MonthStr = TEXT("May");	break;
		case EMonthOfYear::June:		MonthStr = TEXT("Jun");	break;
		case EMonthOfYear::July:		MonthStr = TEXT("Jul");	break;
		case EMonthOfYear::August:		MonthStr = TEXT("Aug");	break;
		case EMonthOfYear::September:	MonthStr = TEXT("Sep");	break;
		case EMonthOfYear::October:		MonthStr = TEXT("Oct");	break;
		case EMonthOfYear::November:	MonthStr = TEXT("Nov");	break;
		case EMonthOfYear::December:	MonthStr = TEXT("Dec");	break;
	}

	FString Time = FString::Printf(TEXT("%02i:%02i:%02i"), GetHour(), GetMinute(), GetSecond());

	return FString::Printf(TEXT("%s, %02d %s %d %s GMT"), *DayStr, GetDay(), *MonthStr, GetYear(), *Time);
}


FString FDateTime::ToIso8601() const
{
	return ToString(TEXT("%Y-%m-%dT%H:%M:%S.%sZ"));
}


FString FDateTime::ToString() const
{
	return ToString(TEXT("%Y.%m.%d-%H.%M.%S"));
}


FString FDateTime::ToString(const TCHAR* Format) const
{
	FString Result;

	if (Format != nullptr)
	{
		while (*Format != TCHAR('\0'))
		{
			if ((*Format == TCHAR('%')) && (*(++Format) != TCHAR('\0')))
			{
				switch (*Format)
				{
				case TCHAR('a'): Result += IsMorning() ? TEXT("am") : TEXT("pm"); break;
				case TCHAR('A'): Result += IsMorning() ? TEXT("AM") : TEXT("PM"); break;
				case TCHAR('d'): Result += FString::Printf(TEXT("%02i"), GetDay()); break;
				case TCHAR('D'): Result += FString::Printf(TEXT("%03i"), GetDayOfYear()); break;
				case TCHAR('m'): Result += FString::Printf(TEXT("%02i"), GetMonth()); break;
				case TCHAR('y'): Result += FString::Printf(TEXT("%02i"), GetYear() % 100); break;
				case TCHAR('Y'): Result += FString::Printf(TEXT("%04i"), GetYear()); break;
				case TCHAR('h'): Result += FString::Printf(TEXT("%02i"), GetHour12()); break;
				case TCHAR('H'): Result += FString::Printf(TEXT("%02i"), GetHour()); break;
				case TCHAR('M'): Result += FString::Printf(TEXT("%02i"), GetMinute()); break;
				case TCHAR('S'): Result += FString::Printf(TEXT("%02i"), GetSecond()); break;
				case TCHAR('s'): Result += FString::Printf(TEXT("%03i"), GetMillisecond()); break;
				default:		 Result += *Format;
				}
			}
			else
			{
				Result += *Format;
			}

			// move to the next one
			Format++;
		}
	}

	return Result;
}


/* FDateTime static interface
 *****************************************************************************/

int32 FDateTime::DaysInMonth(int32 Year, int32 Month)
{
	check((Month >= 1) && (Month <= 12));

	if ((Month == 2) && IsLeapYear(Year))
	{
		return 29;
	}

	return DaysPerMonth[Month];
}


int32 FDateTime::DaysInYear(int32 Year)
{
	if (IsLeapYear(Year))
	{
		return 366;
	}

	return 365;
}


bool FDateTime::IsLeapYear(int32 Year)
{
	if ((Year % 4) == 0)
	{
		return (((Year % 100) != 0) || ((Year % 400) == 0));
	}

	return false;
}


FDateTime FDateTime::Now()
{
	int32 Year, Month, Day, DayOfWeek;
	int32 Hour, Minute, Second, Millisecond;

	FPlatformTime::SystemTime(Year, Month, DayOfWeek, Day, Hour, Minute, Second, Millisecond);

	return FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);
}


bool FDateTime::Parse(const FString& DateTimeString, FDateTime& OutDateTime)
{
	// first replace -, : and . with space
	FString FixedString = DateTimeString.Replace(TEXT("-"), TEXT(" "));
	FixedString.ReplaceInline(TEXT(":"), TEXT(" "), ESearchCase::CaseSensitive);
	FixedString.ReplaceInline(TEXT("."), TEXT(" "), ESearchCase::CaseSensitive);

	// split up on a single delimiter
	TArray<FString> Tokens;
	FixedString.ParseIntoArray(Tokens, TEXT(" "), true);

	// make sure it parsed it properly (within reason)
	if ((Tokens.Num() < 6) || (Tokens.Num() > 7))
	{
		return false;
	}

	const int32 Year = FCString::Atoi(*Tokens[0]);
	const int32 Month = FCString::Atoi(*Tokens[1]);
	const int32 Day = FCString::Atoi(*Tokens[2]);
	const int32 Hour = FCString::Atoi(*Tokens[3]);
	const int32 Minute = FCString::Atoi(*Tokens[4]);
	const int32 Second = FCString::Atoi(*Tokens[5]);
	const int32 Millisecond = Tokens.Num() > 6 ? FCString::Atoi(*Tokens[6]) : 0;

	if (!Validate(Year, Month, Day, Hour, Minute, Second, Millisecond))
	{
		return false;
	}

	// convert the tokens to numbers
	OutDateTime.Ticks = FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond).Ticks;

	return true;
}


bool FDateTime::ParseHttpDate(const FString& HttpDate, FDateTime& OutDateTime)
{
	auto ParseTime = [](const FString& Time, int32& Hour, int32& Minute, int32& Second) -> bool
	{
		// 2DIGIT ":" 2DIGIT ":" 2DIGIT
		// ; 00:00 : 00 - 23 : 59 : 59
		TArray<FString> Tokens;

		// split up on a single delimiter
		int32 NumTokens = Time.ParseIntoArray(Tokens, TEXT(":"), true);

		if (NumTokens == 3)
		{
			Hour = FCString::Atoi(*Tokens[0]);
			Minute = FCString::Atoi(*Tokens[1]);
			Second = FCString::Atoi(*Tokens[2]);

			return (Hour >= 0 && Hour < 24) && (Minute >= 0 && Minute <= 59) && (Second >= 0 && Second <= 59);
		}

		return false;
	};

	auto ParseWkday = [](const FString& WkDay) -> int32
	{
		const int32 NumChars = WkDay.Len();

		if (NumChars == 3)
		{
			if (WkDay.Equals(TEXT("Mon")))
			{
				return 1;
			}
			else if (WkDay.Equals(TEXT("Tue")))
			{
				return 2;
			}
			else if (WkDay.Equals(TEXT("Wed")))
			{
				return 3;
			}
			else if (WkDay.Equals(TEXT("Thu")))
			{
				return 4;
			}
			else if (WkDay.Equals(TEXT("Fri")))
			{
				return 5;
			}
			else if (WkDay.Equals(TEXT("Sat")))
			{
				return 6;
			}
			else if (WkDay.Equals(TEXT("Sun")))
			{
				return 7;
			}
		}

		return -1;
	};

	auto ParseWeekday = [](const FString& WeekDay) -> int32
	{
		const int32 NumChars = WeekDay.Len();

		if (NumChars >= 6 && NumChars <= 9)
		{
			if (WeekDay.Equals(TEXT("Monday")))
			{
				return 1;
			}
			else if (WeekDay.Equals(TEXT("Tueday")))
			{
				return 2;
			}
			else if (WeekDay.Equals(TEXT("Wednesday")))
			{
				return 3;
			}
			else if (WeekDay.Equals(TEXT("Thursday")))
			{
				return 4;
			}
			else if (WeekDay.Equals(TEXT("Friday")))
			{
				return 5;
			}
			else if (WeekDay.Equals(TEXT("Saturday")))
			{
				return 6;
			}
			else if (WeekDay.Equals(TEXT("Sunday")))
			{
				return 7;
			}
		}

		return -1;
	};

	auto ParseMonth = [](const FString& Month) -> int32
	{
		const int32 NumChars = Month.Len();

		if (NumChars == 3)
		{
			if (Month.Equals(TEXT("Jan")))
			{
				return 1;
			}
			else if (Month.Equals(TEXT("Feb")))
			{
				return 2;
			}
			else if (Month.Equals(TEXT("Mar")))
			{
				return 3;
			}
			else if (Month.Equals(TEXT("Apr")))
			{
				return 4;
			}
			else if (Month.Equals(TEXT("May")))
			{
				return 5;
			}
			else if (Month.Equals(TEXT("Jun")))
			{
				return 6;
			}
			else if (Month.Equals(TEXT("Jul")))
			{
				return 7;
			}
			else if (Month.Equals(TEXT("Aug")))
			{
				return 8;
			}
			else if (Month.Equals(TEXT("Sep")))
			{
				return 9;
			}
			else if (Month.Equals(TEXT("Oct")))
			{
				return 10;
			}
			else if (Month.Equals(TEXT("Nov")))
			{
				return 11;
			}
			else if (Month.Equals(TEXT("Dec")))
			{
				return 12;
			}
		}

		return -1;
	};

	auto ParseDate1 = [ParseMonth](const FString& DayStr, const FString& MonStr, const FString& YearStr, int32& Month, int32& Day, int32& Year) -> bool
	{
		// date1 = 2DIGIT SP month SP 4DIGIT
		// ; day month year(e.g., 02 Jun 1982)

		Day = FCString::Atoi(*DayStr);
		Month = ParseMonth(MonStr);
		Year = (YearStr.Len() == 4) ? FCString::Atoi(*YearStr) : -1;

		return (Day > 0 && Day <= 31) && (Month > 0 && Month <= 12) && (Year > 0 && Year <= 9999);
	};

	auto ParseDate2 = [ParseMonth](const FString& Date2, int32& Month, int32& Day, int32& Year) -> bool
	{
		// date2 = 2DIGIT "-" month "-" 2DIGIT
		// ; day - month - year(e.g., 02 - Jun - 82)
		TArray<FString> Tokens;

		// split up on a single delimiter
		int32 NumTokens = Date2.ParseIntoArray(Tokens, TEXT("-"), true);
		if (NumTokens == 3)
		{
			Day = FCString::Atoi(*Tokens[0]);
			Month = ParseMonth(Tokens[1]);
			Year = FCString::Atoi(*Tokens[2]);
			
			// Horrible assumption here, but this is a deprecated part of the spec
			Year += 1900;
		}

		return (Day > 0 && Day <= 31) && (Month > 0 && Month <= 12) && (Year > 0 && Year <= 9999);
	};

	auto ParseDate3 = [ParseMonth](const FString& MonStr, const FString& DayStr, int32& Month, int32& Day) -> bool
	{
		// date3 = month SP(2DIGIT | (SP 1DIGIT))
		// ; month day(e.g., Jun  2)
		const int32 NumDigits = DayStr.Len();
		Day = (NumDigits > 0 && NumDigits <= 2) ? FCString::Atoi(*DayStr) : -1;
		Month = ParseMonth(MonStr);

		return (Day > 0 && Day <= 31) && (Month > 0 && Month <= 12);
	};

	if (!HttpDate.IsEmpty())
	{
		TArray<FString> Tokens;

		// split up on a single delimiter
		int32 NumTokens = HttpDate.ParseIntoArray(Tokens, TEXT(" "), true);

		if (NumTokens > 0 && ensure(Tokens.Num() == NumTokens))
		{
			int32 Month = 0;
			int32 Day = 0;
			int32 Year = 0;
			int32 Hour = 0;
			int32 Minute = 0;
			int32 Second = 0;
			int32 Millisecond = 0;

			if (Tokens[0].EndsWith(TEXT(",")))
			{
				Tokens[0].RemoveAt(Tokens[0].Len() - 1, 1);
			}

			if (Tokens[Tokens.Num() - 1].Equals(TEXT("GMT")))
			{
				// rfc1123 - date | rfc850 - date 
				if (Tokens.Num() == 6)
				{
					int32 WkDay = ParseWkday(Tokens[0]);

					if (WkDay > 0)
					{
						// rfc1123 - date = wkday "," SP date1 SP time SP "GMT"
						if (ParseDate1(Tokens[1], Tokens[2], Tokens[3], Month, Day, Year))
						{
							ParseTime(Tokens[4], Hour, Minute, Second);
						}
					}
				}
				else if (Tokens.Num() == 4)
				{
					// rfc850 - date = weekday "," SP date2 SP time SP "GMT"
					int32 WeekDay = ParseWeekday(Tokens[0]);

					if (WeekDay > 0)
					{
						if (ParseDate2(Tokens[1], Month, Day, Year))
						{
							ParseTime(Tokens[2], Hour, Minute, Second);
						}
					}
				}

			}
			else if (Tokens.Num() == 5)
			{
				// asctime - date = wkday SP date3 SP time SP 4DIGIT
				int32 WkDay = ParseWkday(Tokens[0]);

				if (WkDay > 0)
				{
					if (ParseDate3(Tokens[1], Tokens[2], Month, Day))
					{
						if (ParseTime(Tokens[3], Hour, Minute, Second))
						{
							if (Tokens[4].Len() == 4)
							{
								Year = FCString::Atoi(*Tokens[4]);
							}
						}
					}
				}
			}

			if (Validate(Year, Month, Day, Hour, Minute, Second, Millisecond))
			{
				// convert the tokens to numbers
				OutDateTime = FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);

				return true;
			}
		}
	}

	return false;
}


bool FDateTime::ParseIso8601(const TCHAR* DateTimeString, FDateTime& OutDateTime)
{
	// DateOnly: YYYY-MM-DD
	// DateTime: YYYY-mm-ddTHH:MM:SS(.ssss)(Z|+th:tm|-th:tm)

	const TCHAR* Ptr = DateTimeString;
	TCHAR* Next = nullptr;

	int32 Year = 0, Month = 0, Day = 0;
	int32 Hour = 0, Minute = 0, Second = 0, Millisecond = 0;
	int32 TzHour = 0, TzMinute = 0;

	// get date
	Year = FCString::Strtoi(Ptr, &Next, 10);

	if ((Next <= Ptr) || (*Next == TCHAR('\0')))
	{
		return false;
	}

	Ptr = Next + 1; // skip separator
	Month = FCString::Strtoi(Ptr, &Next, 10);

	if ((Next <= Ptr) || (*Next == TCHAR('\0')))
	{
		return false;
	}

	Ptr = Next + 1; // skip separator
	Day = FCString::Strtoi(Ptr, &Next, 10);

	if (Next <= Ptr)
	{
		return false;
	}

	// check whether this is date and time
	if (*Next == TCHAR('T'))
	{
		Ptr = Next + 1;

		// parse time
		Hour = FCString::Strtoi(Ptr, &Next, 10);

		if ((Next <= Ptr) || (*Next == TCHAR('\0')))
		{
			return false;
		}

		Ptr = Next + 1; // skip separator
		Minute = FCString::Strtoi(Ptr, &Next, 10);

		if ((Next <= Ptr) || (*Next == TCHAR('\0')))
		{
			return false;
		}

		Ptr = Next + 1; // skip separator
		Second = FCString::Strtoi(Ptr, &Next, 10);

		if (Next <= Ptr)
		{
			return false;
		}

		// check for milliseconds
		if (*Next == TCHAR('.'))
		{
			Ptr = Next + 1;
			Millisecond = FCString::Strtoi(Ptr, &Next, 10);

			// should be no more than 3 digits
			if ((Next <= Ptr) || (Next > Ptr + 3))
			{
				return false;
			}

			for (int32 Digits = Next - Ptr; Digits < 3; ++Digits)
			{
				Millisecond *= 10;
			}
		}

		// see if the timezone offset is included
		if (*Next == TCHAR('+') || *Next == TCHAR('-'))
		{
			// include the separator since it's + or -
			Ptr = Next;

			// parse the timezone offset
			TzHour = FCString::Strtoi(Ptr, &Next, 10);

			if ((Next <= Ptr) || (*Next == TCHAR('\0')))
			{
				return false;
			}

			Ptr = Next + 1; // skip separator
			TzMinute = FCString::Strtoi(Ptr, &Next, 10);

			if (Next <= Ptr)
			{
				return false;
			}
		}
		else if ((*Next != TCHAR('\0')) && (*Next != TCHAR('Z')))
		{
			return false;
		}
	}
	else if (*Next != TCHAR('\0'))
	{
		return false;
	}

	if (!Validate(Year, Month, Day, Hour, Minute, Second, Millisecond))
	{
		return false;
	}

	FDateTime Final(Year, Month, Day, Hour, Minute, Second, Millisecond);

	// adjust for the timezone (bringing the DateTime into UTC)
	int32 TzOffsetMinutes = (TzHour < 0) ? TzHour * 60 - TzMinute : TzHour * 60 + TzMinute;
	Final -= FTimespan::FromMinutes(TzOffsetMinutes);
	OutDateTime = Final;

	return true;
}


FDateTime FDateTime::UtcNow()
{
	int32 Year, Month, Day, DayOfWeek;
	int32 Hour, Minute, Second, Millisecond;

	FPlatformTime::UtcTime(Year, Month, DayOfWeek, Day, Hour, Minute, Second, Millisecond);

	return FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);
}


bool FDateTime::Validate(int32 Year, int32 Month, int32 Day, int32 Hour, int32 Minute, int32 Second, int32 Millisecond)
{
	return (Year >= 1) && (Year <= 9999) &&
		(Month >= 1) && (Month <= 12) &&
		(Day >= 1) && (Day <= DaysInMonth(Year, Month)) &&
		(Hour >= 0) && (Hour <= 23) &&
		(Minute >= 0) && (Minute <= 59) &&
		(Second >= 0) && (Second <= 59) &&
		(Millisecond >= 0) && (Millisecond <= 999);
}

