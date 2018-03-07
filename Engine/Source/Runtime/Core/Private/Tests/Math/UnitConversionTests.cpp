// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"
#include "Math/UnitConversion.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitUnitTests, "System.Core.Math.Unit Conversion", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool IsRoughlyEqual(double One, double Two, float Epsilon)
{
	return FMath::Abs(One-Two) <= Epsilon;
}

bool FUnitUnitTests::RunTest(const FString& Parameters)
{
	struct FTestStruct
	{
		double Source;
		double ExpectedResult;
		double AccuracyEpsilon;

		EUnit FromUnit, ToUnit;
	};
	static FTestStruct Tests[] = {
		{ 0.025,	2.3651e11,	1e7, 	EUnit::Lightyears, 			EUnit::Kilometers	 		},
		{ 0.5,		80467.2,	0.1,	EUnit::Miles, 				EUnit::Centimeters	 		},
		{ 0.2,		182.88,		0.01,	EUnit::Yards, 				EUnit::Millimeters	 		},
		{ 0.2,		60960,		0.1,	EUnit::Feet, 				EUnit::Micrometers	 		},
		{ 10,		0.254,		0.001,	EUnit::Inches, 				EUnit::Meters		 		},
		{ 0.75,		2460.6299,	1e-4,	EUnit::Kilometers, 			EUnit::Feet					},
		{ 1,		39.37,		0.01,	EUnit::Meters, 				EUnit::Inches				},
		{ 2750,		27.5,		1e-6,	EUnit::Centimeters, 		EUnit::Meters				},
		{ 1000,		1.0936,		1e-4,	EUnit::Millimeters, 		EUnit::Yards		 		},
		{ 2000,		0.0787,		1e-4,	EUnit::Micrometers, 		EUnit::Inches		 		},

		{ 90,		PI/2,		1e-3,	EUnit::Degrees, 			EUnit::Radians		 		},
		{ PI,		180,		1e-3,	EUnit::Radians, 			EUnit::Degrees		 		},

		{ 12,		43.2,		0.1,	EUnit::MetersPerSecond,		EUnit::KilometersPerHour	},
		{ 1,		0.6214,		1e-4,	EUnit::KilometersPerHour,	EUnit::MilesPerHour			},
		{ 15,		6.7056,		1e-4,	EUnit::MilesPerHour,		EUnit::MetersPerSecond		},

		{ 100, 		212,		0.1,	EUnit::Celsius,				EUnit::Farenheit			},
		{ 400, 		477.594,	1e-3,	EUnit::Farenheit,			EUnit::Kelvin				},
		{ 72, 		-201.15,	0.01,	EUnit::Kelvin,				EUnit::Celsius				},

		{ 1000, 	3.5274e-5,	1e-6,	EUnit::Micrograms,			EUnit::Ounces,				},
		{ 1000,		1,			0.1,	EUnit::Milligrams,			EUnit::Grams,				},
		{ 200,		0.4409,		1e-4,	EUnit::Grams,				EUnit::Pounds,				},
		{ 0.15,		150000,		0.1,	EUnit::Kilograms,			EUnit::Milligrams,			},
		{ 1,		157.473,	1e-3,	EUnit::MetricTons,			EUnit::Stones,				},
		{ 0.001,	28349.5,	0.1,	EUnit::Ounces,				EUnit::Micrograms,			},
		{ 500,		226.796,	1e-3,	EUnit::Pounds,				EUnit::Kilograms,			},
		{ 100,		0.6350,		1e-4,	EUnit::Stones,				EUnit::MetricTons,			},

		{ 100,		10.1972,	1e-4,	EUnit::Newtons,				EUnit::KilogramsForce,		},
		{ 15,		66.7233,	1e-4,	EUnit::PoundsForce,			EUnit::Newtons,				},
		{ 2,		4.4092,		1e-4,	EUnit::KilogramsForce,		EUnit::PoundsForce,			},
		
		{ 1000,		1,			0.1,	EUnit::Hertz,				EUnit::Kilohertz,			},
		{ 0.25,		250*60,		1e-3,	EUnit::Kilohertz,			EUnit::RevolutionsPerMinute,},
		{ 1000,		1,			1e-3,	EUnit::Megahertz,			EUnit::Gigahertz,			},
		{ 0.001,	1000000,	1e-3,	EUnit::Gigahertz,			EUnit::Hertz,				},
		{ 100,		100.0/60,	1e-3,	EUnit::RevolutionsPerMinute,EUnit::Hertz,				},
		
		{ 1024,		1,			1e-3,	EUnit::Bytes,				EUnit::Kilobytes,			},
		{ 1.5,		1536,		1e-3,	EUnit::Kilobytes,			EUnit::Bytes,				},
		{ 1000,		9.5367e-4,	1e-5,	EUnit::Megabytes,			EUnit::Terabytes,			},
		{ 0.5,		512,		1e-3,	EUnit::Gigabytes,			EUnit::Megabytes,			},
		{ 0.25,		256,		1e-3,	EUnit::Terabytes,			EUnit::Gigabytes,			},
		
		{ 10000,	0.166667,	1e-6,	EUnit::Milliseconds,		EUnit::Minutes,				},
		{ 0.5,		500,		1e-6,	EUnit::Seconds,				EUnit::Milliseconds,		},
		{ 30,		60*30,		1e-6,	EUnit::Minutes,				EUnit::Seconds,				},
		{ 5,		5.0/24,		1e-6,	EUnit::Hours,				EUnit::Days,				},
		{ 0.75,		18,			1e-6,	EUnit::Days,				EUnit::Hours,				},
		{ 3,		0.25,		1e-6,	EUnit::Months,				EUnit::Years,				},
		{ 0.5,		6,			1e-6,	EUnit::Years,				EUnit::Months,				},

	};

	bool bSuccess = true;
	for (auto& Test : Tests)
	{
		const double ActualResult = FUnitConversion::Convert(Test.Source, Test.FromUnit, Test.ToUnit);
		if (!IsRoughlyEqual(ActualResult, Test.ExpectedResult, Test.AccuracyEpsilon))
		{
			bSuccess = false;

			const TCHAR* FromUnitString	= FUnitConversion::GetUnitDisplayString(Test.FromUnit);
			const TCHAR* ToUnitString 	= FUnitConversion::GetUnitDisplayString(Test.ToUnit);

			AddError(FString::Printf(TEXT("Conversion from %s to %s was incorrect. Converting %.10f%s to %s resulted in %.15f%s, expected %.15f%s (threshold = %.15f)"),
				FromUnitString, ToUnitString,
				Test.Source, FromUnitString, ToUnitString,
				ActualResult, ToUnitString,
				Test.ExpectedResult, ToUnitString,
				Test.AccuracyEpsilon)
			);
		}
	}
	return bSuccess;
}

#endif //WITH_DEV_AUTOMATION_TESTS
