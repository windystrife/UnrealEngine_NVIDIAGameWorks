// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomationTest.h"

BEGIN_DEFINE_SPEC(FAutomationExpectedErrorTest, "System.Automation.ExpectedError", EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ApplicationContextMask)
END_DEFINE_SPEC(FAutomationExpectedErrorTest)
void FAutomationExpectedErrorTest::Define()
{
	Describe("A defined expected error in a test", [this]()
	{

		It("will not add an error with a number of occurrences less than zero", [this]()
		{
			// Suppress error logged when adding entry with invalid occurrence count
			AddExpectedError(TEXT("number of expected occurrences must be >= 0"), EAutomationExpectedErrorFlags::Contains, 1);

			AddExpectedError(TEXT("The two values are not equal"), EAutomationExpectedErrorFlags::Contains, -1);

			TArray<FAutomationExpectedError> Errors;
			GetExpectedErrors(Errors);

			// Only the first expected error should exist in the list.
			TestEqual("Expected Errors Count", Errors.Num(), 1);
		});

		It("will add an error with a number of occurrences equal to zero", [this]()
		{
			AddExpectedError(TEXT("Expected Error"), EAutomationExpectedErrorFlags::Contains, 0);

			TArray<FAutomationExpectedError> Errors;
			GetExpectedErrors(Errors);
			TestEqual("Expected Errors Count", Errors.Num(), 1);

			// Add the expected error to ensure all test conditions pass
			AddError(TEXT("Expected Error"));
		});

		It("will not duplicate an existing expected error using the same matcher", [this]()
		{
			// Suppress warning logged when adding duplicate value
			AddExpectedError(TEXT("cannot add duplicate entries"), EAutomationExpectedErrorFlags::Contains, 1);
			
			AddExpectedError(TEXT("Expected Error"), EAutomationExpectedErrorFlags::Contains, 1);
			AddExpectedError(TEXT("Expected Error"), EAutomationExpectedErrorFlags::Contains, 1);

			TArray<FAutomationExpectedError> Errors;
			GetExpectedErrors(Errors);
			TestEqual("Expected Errors Count", Errors.Num(), 2);

			// Add the expected error to ensure all test conditions pass
			AddError(TEXT("Expected Error"));
		});

		It("will not duplicate an expected error using a different matcher", [this]()
		{
			// Suppress warning logged when adding duplicate value
			AddExpectedError(TEXT("cannot add duplicate entries"), EAutomationExpectedErrorFlags::Contains, 2);

			AddExpectedError(TEXT("Expected Exact Error"), EAutomationExpectedErrorFlags::Exact, 1);
			AddExpectedError(TEXT("Expected Exact Error"), EAutomationExpectedErrorFlags::Contains, 1);

			AddExpectedError(TEXT("Expected Contains Error"), EAutomationExpectedErrorFlags::Contains, 1);
			AddExpectedError(TEXT("Expected Contains Error"), EAutomationExpectedErrorFlags::Exact, 1);

			TArray<FAutomationExpectedError> Errors;
			GetExpectedErrors(Errors);
			TestEqual("Expected Errors Count", Errors.Num(), 3);

			// Add the expected errors to ensure all test conditions pass
			AddError(TEXT("Expected Exact Error"));
			AddError(TEXT("Expected Contains Error"));
		});

		// Disabled until fix for UE-44340 (crash creating invalid regex) is merged
		xIt("will not add an error with an invalid regex pattern", [this]()
		{
			AddExpectedError(TEXT("invalid regex }])([{"), EAutomationExpectedErrorFlags::Contains, 0);

			TArray<FAutomationExpectedError> Errors;
			GetExpectedErrors(Errors);

			TestEqual("Expected Errors Count", Errors.Num(), 0);
		});

		It("will match both Error and Warning messages", [this]()
		{
			AddExpectedError(TEXT("Expected Message"), EAutomationExpectedErrorFlags::Contains, 0);
			AddError(TEXT("Expected Message"));
			AddWarning(TEXT("Expected Message"));
		});

		It("will not fail or warn if encountered", [this]()
		{
			AddExpectedError(TEXT("Expected Error"), EAutomationExpectedErrorFlags::Contains, 1);
			AddExpectedError(TEXT("Expected Warning"), EAutomationExpectedErrorFlags::Contains, 1);
			AddError(TEXT("Expected Error"));
			AddWarning(TEXT("Expected Warning"));
		});

		It("will not match multiple occurrences in the same message when using Contains matcher", [this]()
		{
			AddExpectedError(TEXT("Expected"), EAutomationExpectedErrorFlags::Contains, 1);
			AddError(TEXT("ExpectedExpectedExpected ExpectedExpectedExpected"));
		});

		It("will match different messages that fit the regex pattern", [this]()
		{
			AddExpectedError(TEXT("Response \\(-?\\d+\\)"), EAutomationExpectedErrorFlags::Contains, 4);
			AddError(TEXT("Response (0)"));
			AddError(TEXT("Response (1)"));
			AddError(FString::Printf(TEXT("Response (%d)"), MIN_int64));
			AddError(FString::Printf(TEXT("Response (%d)"), MAX_uint64));
		});

	});

}

// Tests for cases where expected errors will fail a test.
// IMPORTANT: The pass condition for these tests is that they FAIL. To prevent
// the expected failures from interfering with regular test runs, these tests
// must be run manually.
BEGIN_DEFINE_SPEC(FAutomationExpectedErrorFailureTest, "System.Automation.ExpectedError", EAutomationTestFlags::NegativeFilter | EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::RequiresUser)
END_DEFINE_SPEC(FAutomationExpectedErrorFailureTest)

void FAutomationExpectedErrorFailureTest::Define()
{
	Describe("An expected error failure", [this]()
	{
		It("will occur if expected a specific number of times and NOT encountered.", [this]()
		{
			AddExpectedError(TEXT("Expected Error"), EAutomationExpectedErrorFlags::Exact, 1);
		});

		It("will occur if expected a specific number of times and is encountered too few times.", [this]()
		{
			AddExpectedError(TEXT("Expected Error"), EAutomationExpectedErrorFlags::Exact, 2);
			AddError(TEXT("Expected Error"));
		});

		It("will occur if expected a specific number of times and is encountered too many times.", [this]()
		{
			AddExpectedError(TEXT("Expected Error"), EAutomationExpectedErrorFlags::Exact, 1);
			AddError(TEXT("Expected Error"));
			AddError(TEXT("Expected Error"));
		});

		It("will occur if expected any number of times and is not encountered.", [this]()
		{
			AddExpectedError(TEXT("Expected Error"), EAutomationExpectedErrorFlags::Exact, 0);
		});

		It("will occur if multiple expected errors are NOT encountered.", [this]()
		{
			AddExpectedError(TEXT("Expected Error 1"), EAutomationExpectedErrorFlags::Exact, 1);
			AddExpectedError(TEXT("Expected Error 2"), EAutomationExpectedErrorFlags::Contains, 1);
		});

		It("will occur if not all expected errors are encountered.", [this]()
		{
			AddExpectedError(TEXT("Expected error 1"), EAutomationExpectedErrorFlags::Exact, 1);
			AddExpectedError(TEXT("Expected error 2"), EAutomationExpectedErrorFlags::Contains, 1);
			AddError(TEXT("Expected error 1"));
		});

		It("will occur if only partial matches are encountered when using Exact matcher", [this]()
		{
			AddExpectedError(TEXT("Expected"), EAutomationExpectedErrorFlags::Exact, 1);
			AddError(TEXT("Expected error"));
		});
	});
}