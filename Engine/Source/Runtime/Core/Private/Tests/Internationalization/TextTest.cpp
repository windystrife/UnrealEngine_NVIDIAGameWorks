// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "CoreGlobals.h"
#include "Internationalization/Text.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/AutomationTest.h"
#include "Internationalization/ICUUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

// Disable optimization for TextTest as it compiles very slowly in development builds
PRAGMA_DISABLE_OPTIMIZATION

#define LOCTEXT_NAMESPACE "Core.Tests.TextFormatTest"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextTest, "System.Core.Misc.Text", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FText FormatWithoutArguments(const FText& Pattern)
	{
		FFormatOrderedArguments Arguments;
		return FText::Format(Pattern, Arguments);
	}

	void ArrayToString(const TArray<FString>& Array, FString& String)
	{
		const int32 Count = Array.Num();
		for(int32 i = 0; i < Count; ++i)
		{
			if(i > 0)
			{
				String += TEXT(", ");
			}
			String += Array[i];
		}
	}

	void TestPatternParameterEnumeration(FTextTest& Test, const FText& Pattern, TArray<FString>& ActualParameters, const TArray<FString>& ExpectedParameters)
	{
		ActualParameters.Empty(ExpectedParameters.Num());
		FText::GetFormatPatternParameters(Pattern, ActualParameters);
		if(ActualParameters != ExpectedParameters)
		{
			FString ActualParametersString;
			ArrayToString(ActualParameters, ActualParametersString);
			FString ExpectedParametersString;
			ArrayToString(ExpectedParameters, ExpectedParametersString);
			Test.AddError( FString::Printf( TEXT("\"%s\" contains parameters (%s) but expected parameters (%s)."), *(Pattern.ToString()), *(ActualParametersString), *(ExpectedParametersString) ) );
		}
	}
}


bool FTextTest::RunTest (const FString& Parameters)
{
	FInternationalization& I18N = FInternationalization::Get();
	
	FInternationalization::FCultureStateSnapshot OriginalCultureState;
	I18N.BackupCultureState(OriginalCultureState);

	FText ArgText0 = FText::FromString(TEXT("Arg0"));
	FText ArgText1 = FText::FromString(TEXT("Arg1"));
	FText ArgText2 = FText::FromString(TEXT("Arg2"));
	FText ArgText3 = FText::FromString(TEXT("Arg3"));

#define INVTEXT(x) FText::FromString(TEXT(x))

#define TEST( Desc, A, B ) if( !A.EqualTo(B) ) AddError(FString::Printf(TEXT("%s - A=%s B=%s"),*Desc,*A.ToString(),*B.ToString()))
	
	FText TestText;

	TestText = INVTEXT("Format with single apostrophes quotes: '{0}'");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Format with single apostrophes quotes: 'Arg0'"));
	TestText = INVTEXT("Format with double apostrophes quotes: ''{0}''");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Format with double apostrophes quotes: ''Arg0''"));
	TestText = INVTEXT("Print with single graves: `{0}`");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Print with single graves: {0}`"));
	TestText = INVTEXT("Format with double graves: ``{0}``");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Format with double graves: `Arg0`"));

	TestText = INVTEXT("Testing `escapes` here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing `escapes` here."));
	TestText = INVTEXT("Testing ``escapes` here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing `escapes` here."));
	TestText = INVTEXT("Testing ``escapes`` here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing `escapes` here."));

	TestText = INVTEXT("Testing `}escapes{ here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing }escapes{ here."));
	TestText = INVTEXT("Testing `}escapes{ here.`");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing }escapes{ here.`"));
	TestText = INVTEXT("Testing `}escapes{` here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing }escapes{` here."));
	TestText = INVTEXT("Testing }escapes`{ here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing }escapes{ here."));
	TestText = INVTEXT("`Testing }escapes`{ here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("`Testing }escapes{ here."));

	TestText = INVTEXT("Testing `{escapes} here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing {escapes} here."));
	TestText = INVTEXT("Testing `{escapes} here.`");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing {escapes} here.`"));
	TestText = INVTEXT("Testing `{escapes}` here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing {escapes}` here."));

	TestText = INVTEXT("Starting text: {0} {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg0 Arg1"));
	TestText = INVTEXT("{0} {1} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 Arg1 - Ending Text."));
	TestText = INVTEXT("Starting text: {0} {1} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg0 Arg1 - Ending Text."));
	TestText = INVTEXT("{0} {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 Arg1"));
	TestText = INVTEXT("{1} {0}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg1 Arg0"));
	TestText = INVTEXT("{0}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Arg0"));
	TestText = INVTEXT("{0} - {1} - {2} - {3}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0,ArgText1,ArgText2,ArgText3), INVTEXT("Arg0 - Arg1 - Arg2 - Arg3"));
	TestText = INVTEXT("{0} - {0} - {0} - {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 - Arg0 - Arg0 - Arg1"));

	TestText = INVTEXT("Starting text: {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg1"));
	TestText = INVTEXT("{0} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 - Ending Text."));
	TestText = INVTEXT("Starting text: {0} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg0 - Ending Text."));

	TestText = INVTEXT("{0} {2}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1, ArgText2), INVTEXT("Arg0 Arg2"));
	TestText = INVTEXT("{1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1, ArgText2), INVTEXT("Arg1"));

	TestText = INVTEXT("Starting text: {0} {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg0 Arg1"));
	TestText = INVTEXT("{0} {1} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 Arg1 - Ending Text."));
	TestText = INVTEXT("Starting text: {0} {1} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg0 Arg1 - Ending Text."));
	TestText = INVTEXT("{0} {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 Arg1"));
	TestText = INVTEXT("{1} {0}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg1 Arg0"));
	TestText = INVTEXT("{0}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Arg0"));
	TestText = INVTEXT("{0} - {1} - {2} - {3}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0,ArgText1,ArgText2,ArgText3), INVTEXT("Arg0 - Arg1 - Arg2 - Arg3"));
	TestText = INVTEXT("{0} - {0} - {0} - {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 - Arg0 - Arg0 - Arg1"));

	{
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("Age"), FText::FromString( TEXT("23") ) );
		Arguments.Add( TEXT("Height"), FText::FromString( TEXT("68") ) );
		Arguments.Add( TEXT("Gender"), FText::FromString( TEXT("male") ) );
		Arguments.Add( TEXT("Name"), FText::FromString( TEXT("Saul") ) );

		// Not using all the arguments is okay.
		TestText = INVTEXT("My name is {Name}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My name is Saul.") );
		TestText = INVTEXT("My age is {Age}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My age is 23.") );
		TestText = INVTEXT("My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My gender is male.") );
		TestText = INVTEXT("My height is {Height}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My height is 68.") );

		// Using arguments out of order is okay.
		TestText = INVTEXT("My name is {Name}. My age is {Age}. My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My name is Saul. My age is 23. My gender is male.") );
		TestText = INVTEXT("My age is {Age}. My gender is {Gender}. My name is {Name}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My age is 23. My gender is male. My name is Saul.") );
		TestText = INVTEXT("My gender is {Gender}. My name is {Name}. My age is {Age}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My gender is male. My name is Saul. My age is 23.") );
		TestText = INVTEXT("My gender is {Gender}. My age is {Age}. My name is {Name}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My gender is male. My age is 23. My name is Saul.") );
		TestText = INVTEXT("My age is {Age}. My name is {Name}. My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My age is 23. My name is Saul. My gender is male.") );
		TestText = INVTEXT("My name is {Name}. My gender is {Gender}. My age is {Age}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My name is Saul. My gender is male. My age is 23.") );

		// Reusing arguments is okay.
		TestText = INVTEXT("If my age is {Age}, I have been alive for {Age} year(s).");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("If my age is 23, I have been alive for 23 year(s).") );

		// Not providing an argument leaves the parameter as text.
		TestText = INVTEXT("What... is the air-speed velocity of an unladen swallow? {AirSpeedOfAnUnladenSwallow}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("What... is the air-speed velocity of an unladen swallow? {AirSpeedOfAnUnladenSwallow}.") );
	}

	{
		FFormatNamedArguments ArgumentList;
		ArgumentList.Emplace(TEXT("Age"), FText::FromString(TEXT("23")));
		ArgumentList.Emplace(TEXT("Height"), FText::FromString(TEXT("68")));
		ArgumentList.Emplace(TEXT("Gender"), FText::FromString(TEXT("male")));
		ArgumentList.Emplace(TEXT("Name"), FText::FromString(TEXT("Saul")));

		// Not using all the arguments is okay.
		TestText = INVTEXT("My name is {Name}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My name is Saul.") );
		TestText = INVTEXT("My age is {Age}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My age is 23.") );
		TestText = INVTEXT("My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My gender is male.") );
		TestText = INVTEXT("My height is {Height}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My height is 68.") );

		// Using arguments out of order is okay.
		TestText = INVTEXT("My name is {Name}. My age is {Age}. My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My name is Saul. My age is 23. My gender is male.") );
		TestText = INVTEXT("My age is {Age}. My gender is {Gender}. My name is {Name}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My age is 23. My gender is male. My name is Saul.") );
		TestText = INVTEXT("My gender is {Gender}. My name is {Name}. My age is {Age}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My gender is male. My name is Saul. My age is 23.") );
		TestText = INVTEXT("My gender is {Gender}. My age is {Age}. My name is {Name}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My gender is male. My age is 23. My name is Saul.") );
		TestText = INVTEXT("My age is {Age}. My name is {Name}. My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My age is 23. My name is Saul. My gender is male.") );
		TestText = INVTEXT("My name is {Name}. My gender is {Gender}. My age is {Age}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My name is Saul. My gender is male. My age is 23.") );

		// Reusing arguments is okay.
		TestText = INVTEXT("If my age is {Age}, I have been alive for {Age} year(s).");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("If my age is 23, I have been alive for 23 year(s).") );

		// Not providing an argument leaves the parameter as text.
		TestText = INVTEXT("What... is the air-speed velocity of an unladen swallow? {AirSpeedOfAnUnladenSwallow}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("What... is the air-speed velocity of an unladen swallow? {AirSpeedOfAnUnladenSwallow}.") );
	}

#undef TEST
#define TEST( Pattern, Actual, Expected ) TestPatternParameterEnumeration(*this, Pattern, Actual, Expected)

	TArray<FString> ActualArguments;
	TArray<FString> ExpectedArguments;

	TestText = INVTEXT("My name is {Name}.");
	ExpectedArguments.Empty(1);
	ExpectedArguments.Add(TEXT("Name"));
	TEST(TestText, ActualArguments, ExpectedArguments);

	TestText = INVTEXT("My age is {Age}.");
	ExpectedArguments.Empty(1);
	ExpectedArguments.Add(TEXT("Age"));
	TEST(TestText, ActualArguments, ExpectedArguments);

	TestText = INVTEXT("If my age is {Age}, I have been alive for {Age} year(s).");
	ExpectedArguments.Empty(1);
	ExpectedArguments.Add(TEXT("Age"));
	TEST(TestText, ActualArguments, ExpectedArguments);

	TestText = INVTEXT("{0} - {1} - {2} - {3}");
	ExpectedArguments.Empty(4);
	ExpectedArguments.Add(TEXT("0"));
	ExpectedArguments.Add(TEXT("1"));
	ExpectedArguments.Add(TEXT("2"));
	ExpectedArguments.Add(TEXT("3"));
	TEST(TestText, ActualArguments, ExpectedArguments);

	TestText = INVTEXT("My name is {Name}. My age is {Age}. My gender is {Gender}.");
	ExpectedArguments.Empty(3);
	ExpectedArguments.Add(TEXT("Name"));
	ExpectedArguments.Add(TEXT("Age"));
	ExpectedArguments.Add(TEXT("Gender"));
	TEST(TestText, ActualArguments, ExpectedArguments);

#undef TEST

#undef INVTEXT

#if UE_ENABLE_ICU
	if (I18N.SetCurrentCulture("en-US"))
	{
#define TEST( A, B, ComparisonLevel ) if( !(FText::FromString(A)).EqualTo(FText::FromString(B), (ComparisonLevel)) ) AddError(FString::Printf(TEXT("Testing comparison of equivalent characters with comparison level (%s). - A=%s B=%s"),TEXT(#ComparisonLevel),(A),(B)))

		// Basic sanity checks
		TEST( TEXT("a"), TEXT("A"), ETextComparisonLevel::Primary ); // Basic sanity check
		TEST( TEXT("a"), TEXT("a"), ETextComparisonLevel::Tertiary ); // Basic sanity check
		TEST( TEXT("A"), TEXT("A"), ETextComparisonLevel::Tertiary ); // Basic sanity check

		// Test equivalence
		TEST( TEXT("ss"), TEXT("\x00DF"), ETextComparisonLevel::Primary ); // Lowercase Sharp s
		TEST( TEXT("SS"), TEXT("\x1E9E"), ETextComparisonLevel::Primary ); // Uppercase Sharp S
		TEST( TEXT("ae"), TEXT("\x00E6"), ETextComparisonLevel::Primary ); // Lowercase ae
		TEST( TEXT("AE"), TEXT("\x00C6"), ETextComparisonLevel::Primary ); // Uppercase AE

		// Test accentuation
		TEST( TEXT("u") , TEXT("\x00FC"), ETextComparisonLevel::Primary ); // Lowercase u with dieresis
		TEST( TEXT("U") , TEXT("\x00DC"), ETextComparisonLevel::Primary ); // Uppercase U with dieresis

#undef TEST
	}
	else
	{
		AddWarning(FString::Printf(TEXT("Internationalization data for %s missing - test is partially disabled."), TEXT("en-US")));
	}
#else
	AddWarning("ICU is disabled thus locale-aware string comparison is disabled.");
#endif

#if UE_ENABLE_ICU
	// Sort Testing
	// French
	if (I18N.SetCurrentCulture("fr"))
	{
		TArray<FText> CorrectlySortedValues;
		CorrectlySortedValues.Add( FText::FromString( TEXT("cote") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("cot\u00e9") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("c\u00f4te") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("c\u00f4t\u00e9") ) );

		{
			// Make unsorted.
			TArray<FText> Values;
			Values.Reserve(CorrectlySortedValues.Num());

			Values.Add(CorrectlySortedValues[1]);
			Values.Add(CorrectlySortedValues[3]);
			Values.Add(CorrectlySortedValues[2]);
			Values.Add(CorrectlySortedValues[0]);

			// Execute sort.
			Values.Sort(FText::FSortPredicate());

			// Test if sorted.
			bool Identical = true;
			for(int32 j = 0; j < Values.Num(); ++j)
			{
				Identical = Values[j].EqualTo(CorrectlySortedValues[j]);
				if(!Identical)
				{
					break;
				}
			}
			if( !Identical )
			{
				//currently failing AddError(FString::Printf(TEXT("Sort order is wrong for culture (%s)."), *FInternationalization::Get().GetCurrentCulture()->GetEnglishName()));
			}
		}
	}
	else
	{
		AddWarning(FString::Printf(TEXT("Internationalization data for %s missing - test is partially disabled."), TEXT("fr")));
	}

	// French Canadian
	if (I18N.SetCurrentCulture("fr-CA"))
	{
		TArray<FText> CorrectlySortedValues;
		CorrectlySortedValues.Add( FText::FromString( TEXT("cote") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("côte") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("coté") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("côté") ) );

		{
			// Make unsorted.
			TArray<FText> Values;
			Values.Reserve(CorrectlySortedValues.Num());

			Values.Add(CorrectlySortedValues[1]);
			Values.Add(CorrectlySortedValues[3]);
			Values.Add(CorrectlySortedValues[2]);
			Values.Add(CorrectlySortedValues[0]);

			// Execute sort.
			Values.Sort(FText::FSortPredicate());

			// Test if sorted.
			bool Identical = true;
			for(int32 j = 0; j < Values.Num(); ++j)
			{
				Identical = Values[j].EqualTo(CorrectlySortedValues[j]);
				if(!Identical) break;
			}
			if( !Identical )
			{
				//currently failing AddError(FString::Printf(TEXT("Sort order is wrong for culture (%s)."), *FInternationalization::Get().GetCurrentCulture()->GetEnglishName()));
			}
		}
	}
	else
	{
		AddWarning(FString::Printf(TEXT("Internationalization data for %s missing - test is partially disabled."), TEXT("fr-CA")));
	}
#else
	AddWarning("ICU is disabled thus locale-aware string collation is disabled.");
#endif

#if UE_ENABLE_ICU
	{
		I18N.RestoreCultureState(OriginalCultureState);

		TArray<uint8> FormattedHistoryAsEnglish;
		TArray<uint8> FormattedHistoryAsFrenchCanadian;
		TArray<uint8> InvariantFTextData;

		FString InvariantString = TEXT("This is a culture invariant string.");
		FString FormattedTestLayer2_OriginalLanguageSourceString;
		FText FormattedTestLayer2;

		// Scoping to allow all locals to leave scope after we serialize at the end
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("String1"), LOCTEXT("RebuildFTextTest1_Lorem", "Lorem"));
			Args.Add(TEXT("String2"), LOCTEXT("RebuildFTextTest1_Ipsum", "Ipsum"));
			FText FormattedTest1 = FText::Format(LOCTEXT("RebuildNamedText1", "{String1} \"Lorem Ipsum\" {String2}"), Args);

			FFormatOrderedArguments ArgsOrdered;
			ArgsOrdered.Add(LOCTEXT("RebuildFTextTest1_Lorem", "Lorem"));
			ArgsOrdered.Add(LOCTEXT("RebuildFTextTest1_Ipsum", "Ipsum"));
			FText FormattedTestOrdered1 = FText::Format(LOCTEXT("RebuildOrderedText1", "{0} \"Lorem Ipsum\" {1}"), ArgsOrdered);

			// Will change to 5.542 due to default settings for numbers
			FText AsNumberTest1 = FText::AsNumber(5.5421);

			FText AsPercentTest1 = FText::AsPercent(0.925);
			FText AsCurrencyTest1 = FText::AsCurrencyBase(10025, TEXT("USD"));

			FDateTime DateTimeInfo(2080, 8, 20, 9, 33, 22);
			FText AsDateTimeTest1 = FText::AsDateTime(DateTimeInfo, EDateTimeStyle::Default, EDateTimeStyle::Default, TEXT("UTC"));

			// FormattedTestLayer2 must be updated when adding or removing from this block. Further, below, 
			// verifying the LEET translated string must be changed to reflect what the new string looks like.
			FFormatNamedArguments ArgsLayer2;
			ArgsLayer2.Add("NamedLayer1", FormattedTest1);
			ArgsLayer2.Add("OrderedLayer1", FormattedTestOrdered1);
			ArgsLayer2.Add("FTextNumber", AsNumberTest1);
			ArgsLayer2.Add("Number", 5010.89221);
			ArgsLayer2.Add("DateTime", AsDateTimeTest1);
			ArgsLayer2.Add("Percent", AsPercentTest1);
			ArgsLayer2.Add("Currency", AsCurrencyTest1);
			FormattedTestLayer2 = FText::Format(LOCTEXT("RebuildTextLayer2", "{NamedLayer1} | {OrderedLayer1} | {FTextNumber} | {Number} | {DateTime} | {Percent} | {Currency}"), ArgsLayer2);

			{
				// Serialize the full, bulky FText that is a composite of most of the other FTextHistories.
				FMemoryWriter Ar(FormattedHistoryAsEnglish);
				Ar << FormattedTestLayer2;
				Ar.Close();
			}

			// The original string in the native language.
			FormattedTestLayer2_OriginalLanguageSourceString = FormattedTestLayer2.BuildSourceString();

			{
				// Swap to "LEET" culture to check if rebuilding works (verify the whole)
				I18N.SetCurrentCulture("LEET");

				// When changes are made to FormattedTestLayer2, please pull out the newly translated LEET string and update the below if-statement to keep the test passing!
				FString LEETTranslatedString = FormattedTestLayer2.ToString();

				FString DesiredOutput = FString(TEXT("\x2021") TEXT("\xAB") TEXT("\x2021") TEXT("\xAB") TEXT("\x2021") TEXT("L0r3m") TEXT("\x2021") TEXT("\xBB") TEXT(" \"L0r3m 1p$um\" ") TEXT("\xAB") TEXT("\x2021") TEXT("1p$um") TEXT("\x2021") TEXT("\xBB") TEXT("\x2021") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("\x2021") TEXT("\xAB") TEXT("\x2021") TEXT("L0r3m") TEXT("\x2021") TEXT("\xBB") TEXT(" \"L0r3m 1p$um\" ") TEXT("\xAB") TEXT("\x2021") TEXT("1p$um") TEXT("\x2021") TEXT("\xBB") TEXT("\x2021") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("5.5421") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("5010.89221") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("Aug 20, 2080, 9:33:22 AM") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("92%") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("$") TEXT("\xA0") TEXT("100.25") TEXT("\xBB") TEXT("\x2021"));
				// Convert the baked string into an FText, which will be leetified, then compare it to the rebuilt FText
				if(LEETTranslatedString != DesiredOutput)
				{
					AddError( TEXT("FormattedTestLayer2 did not rebuild to correctly in LEET!") );
					AddError( TEXT("Formatted Output=") + LEETTranslatedString );
					AddError( TEXT("Desired Output=") + DesiredOutput );
				}
			}

			// Swap to French-Canadian to check if rebuilding works (verify each numerical component)
			{
				I18N.SetCurrentCulture("fr-CA");

				// Need the FText to be rebuilt in fr-CA.
				FormattedTestLayer2.ToString();

				if(AsNumberTest1.CompareTo(FText::AsNumber(5.5421)) != 0)
				{
					AddError( TEXT("AsNumberTest1 did not rebuild correctly in French-Canadian") );
					AddError( TEXT("Number Output=") + AsNumberTest1.ToString() );
				}

				if(AsPercentTest1.CompareTo(FText::AsPercent(0.925)) != 0)
				{
					AddError( TEXT("AsPercentTest1 did not rebuild correctly in French-Canadian") );
					AddError( TEXT("Percent Output=") + AsPercentTest1.ToString() );
				}

				if(AsCurrencyTest1.CompareTo(FText::AsCurrencyBase(10025, TEXT("USD"))) != 0)
				{
					AddError( TEXT("AsCurrencyTest1 did not rebuild correctly in French-Canadian") );
					AddError( TEXT("Currency Output=") + AsCurrencyTest1.ToString() );
				}

				if(AsDateTimeTest1.CompareTo(FText::AsDateTime(DateTimeInfo, EDateTimeStyle::Default, EDateTimeStyle::Default, TEXT("UTC"))) != 0)
				{
					AddError( TEXT("AsDateTimeTest1 did not rebuild correctly in French-Canadian") );
					AddError( TEXT("DateTime Output=") + AsDateTimeTest1.ToString() );
				}

				{
					// Serialize the full, bulky FText that is a composite of most of the other FTextHistories.
					// We don't care how this may be translated, we will be serializing this in as LEET.
					FMemoryWriter Ar(FormattedHistoryAsFrenchCanadian);
					Ar << FormattedTestLayer2;
					Ar.Close();
				}

				{
					FText InvariantFText = FText::FromString(InvariantString);

					// Serialize an invariant FText
					FMemoryWriter Ar(InvariantFTextData);
					Ar << InvariantFText;
					Ar.Close();
				}
			}
		}

		{
			I18N.SetCurrentCulture("LEET");

			FText FormattedEnglishTextHistoryAsLeet;
			FText FormattedFrenchCanadianTextHistoryAsLeet;

			{
				FMemoryReader Ar(FormattedHistoryAsEnglish);
				Ar << FormattedEnglishTextHistoryAsLeet;
				Ar.Close();
			}
			{
				FMemoryReader Ar(FormattedHistoryAsFrenchCanadian);
				Ar << FormattedFrenchCanadianTextHistoryAsLeet;
				Ar.Close();
			}

			// Confirm the two FText's serialize in and get translated into the current (LEET) translation. One originated in English, the other in French-Canadian locales.
			if(FormattedEnglishTextHistoryAsLeet.CompareTo(FormattedFrenchCanadianTextHistoryAsLeet) != 0)
			{
				AddError( TEXT("Serialization of text histories from source English and source French-Canadian to LEET did not produce the same results!") );
				AddError( TEXT("English Output=") + FormattedEnglishTextHistoryAsLeet.ToString() );
				AddError( TEXT("French-Canadian Output=") + FormattedFrenchCanadianTextHistoryAsLeet.ToString() );
			}

			// Confirm the two FText's source strings for the serialized FTexts are the same.
			if(FormattedEnglishTextHistoryAsLeet.BuildSourceString() != FormattedFrenchCanadianTextHistoryAsLeet.BuildSourceString())
			{
				AddError( TEXT("Serialization of text histories from source English and source French-Canadian to LEET did not produce the same source results!") );
				AddError( TEXT("English Output=") + FormattedEnglishTextHistoryAsLeet.BuildSourceString() );
				AddError( TEXT("French-Canadian Output=") + FormattedFrenchCanadianTextHistoryAsLeet.BuildSourceString() );
			}

			// Rebuild in LEET so that when we build the source string the DisplayString is still in LEET. 
			FormattedTestLayer2.ToString();

			{
				I18N.RestoreCultureState(OriginalCultureState);

				FText InvariantFText;

				FMemoryReader Ar(InvariantFTextData);
				Ar << InvariantFText;
				Ar.Close();

				if(InvariantFText.ToString() != InvariantString)
				{
					AddError( TEXT("Invariant FText did not match the original FString after serialization!") );
					AddError( TEXT("Invariant Output=") + InvariantFText.ToString() );
				}


				FString FormattedTestLayer2_SourceString = FormattedTestLayer2.BuildSourceString();

				// Compare the source string of the LEETified version of FormattedTestLayer2 to ensure it is correct.
				if(FormattedTestLayer2_OriginalLanguageSourceString != FormattedTestLayer2_SourceString)
				{
					AddError( TEXT("FormattedTestLayer2's source string was incorrect!") );
					AddError( TEXT("Output=") + FormattedTestLayer2_SourceString );
					AddError( TEXT("Desired Output=") + FormattedTestLayer2_OriginalLanguageSourceString );
				}
			}
		}
	}
#else
	AddWarning("ICU is disabled thus locale-aware formatting needed in rebuilding source text from history is disabled.");
#endif

	//**********************************
	// FromString Test
	//**********************************
	TestText = FText::FromString( TEXT("Test String") );

	if ( GIsEditor && TestText.IsCultureInvariant() )
	{
		AddError( TEXT("FromString should not produce a Culture Invariant Text when called inside the editor") );
	}
	
	if ( !GIsEditor && !TestText.IsCultureInvariant() )
	{
		AddError( TEXT("FromString should produce a Culture Invariant Text when called outside the editor") );
	}

	if ( TestText.IsTransient() )
	{
		AddError( TEXT("FromString should never produce a Transient Text") );
	}

	I18N.RestoreCultureState(OriginalCultureState);

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextRoundingTest, "System.Core.Misc.TextRounding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTextRoundingTest::RunTest (const FString& Parameters)
{
	static const TCHAR* RoundingModeNames[] = {
		TEXT("HalfToEven"),
		TEXT("HalfFromZero"),
		TEXT("HalfToZero"),
		TEXT("FromZero"),
		TEXT("ToZero"),
		TEXT("ToNegativeInfinity"),
		TEXT("ToPositiveInfinity"),
	};

	static_assert(ERoundingMode::ToPositiveInfinity == ARRAY_COUNT(RoundingModeNames) - 1, "RoundingModeNames array needs updating");

	static const double InputValues[] = {
		1000.1224,
		1000.1225,
		1000.1226,
		1000.1234,
		1000.1235,
		1000.1236,
		
		1000.1244,
		1000.1245,
		1000.1246,
		1000.1254,
		1000.1255,
		1000.1256,

		-1000.1224,
		-1000.1225,
		-1000.1226,
		-1000.1234,
		-1000.1235,
		-1000.1236,
		
		-1000.1244,
		-1000.1245,
		-1000.1246,
		-1000.1254,
		-1000.1255,
		-1000.1256,
	};

	static const TCHAR* OutputValues[][ARRAY_COUNT(RoundingModeNames)] = 
	{
		// HalfToEven        | HalfFromZero      | HalfToZero        | FromZero          | ToZero            | ToNegativeInfinity | ToPositiveInfinity
		{  TEXT("1000.122"),   TEXT("1000.122"),   TEXT("1000.122"),   TEXT("1000.123"),   TEXT("1000.122"),   TEXT("1000.122"),    TEXT("1000.123") },
		{  TEXT("1000.122"),   TEXT("1000.123"),   TEXT("1000.122"),   TEXT("1000.123"),   TEXT("1000.122"),   TEXT("1000.122"),    TEXT("1000.123") },
		{  TEXT("1000.123"),   TEXT("1000.123"),   TEXT("1000.123"),   TEXT("1000.123"),   TEXT("1000.122"),   TEXT("1000.122"),    TEXT("1000.123") },
		{  TEXT("1000.123"),   TEXT("1000.123"),   TEXT("1000.123"),   TEXT("1000.124"),   TEXT("1000.123"),   TEXT("1000.123"),    TEXT("1000.124") },
		{  TEXT("1000.124"),   TEXT("1000.124"),   TEXT("1000.123"),   TEXT("1000.124"),   TEXT("1000.123"),   TEXT("1000.123"),    TEXT("1000.124") },
		{  TEXT("1000.124"),   TEXT("1000.124"),   TEXT("1000.124"),   TEXT("1000.124"),   TEXT("1000.123"),   TEXT("1000.123"),    TEXT("1000.124") },

		{  TEXT("1000.124"),   TEXT("1000.124"),   TEXT("1000.124"),   TEXT("1000.125"),   TEXT("1000.124"),   TEXT("1000.124"),    TEXT("1000.125") },
		{  TEXT("1000.124"),   TEXT("1000.125"),   TEXT("1000.124"),   TEXT("1000.125"),   TEXT("1000.124"),   TEXT("1000.124"),    TEXT("1000.125") },
		{  TEXT("1000.125"),   TEXT("1000.125"),   TEXT("1000.125"),   TEXT("1000.125"),   TEXT("1000.124"),   TEXT("1000.124"),    TEXT("1000.125") },
		{  TEXT("1000.125"),   TEXT("1000.125"),   TEXT("1000.125"),   TEXT("1000.126"),   TEXT("1000.125"),   TEXT("1000.125"),    TEXT("1000.126") },
		{  TEXT("1000.126"),   TEXT("1000.126"),   TEXT("1000.125"),   TEXT("1000.126"),   TEXT("1000.125"),   TEXT("1000.125"),    TEXT("1000.126") },
		{  TEXT("1000.126"),   TEXT("1000.126"),   TEXT("1000.126"),   TEXT("1000.126"),   TEXT("1000.125"),   TEXT("1000.125"),    TEXT("1000.126") },

		{ TEXT("-1000.122"),  TEXT("-1000.122"),  TEXT("-1000.122"),  TEXT("-1000.123"),  TEXT("-1000.122"),  TEXT("-1000.123"),   TEXT("-1000.122") },
		{ TEXT("-1000.122"),  TEXT("-1000.123"),  TEXT("-1000.122"),  TEXT("-1000.123"),  TEXT("-1000.122"),  TEXT("-1000.123"),   TEXT("-1000.122") },
		{ TEXT("-1000.123"),  TEXT("-1000.123"),  TEXT("-1000.123"),  TEXT("-1000.123"),  TEXT("-1000.122"),  TEXT("-1000.123"),   TEXT("-1000.122") },
		{ TEXT("-1000.123"),  TEXT("-1000.123"),  TEXT("-1000.123"),  TEXT("-1000.124"),  TEXT("-1000.123"),  TEXT("-1000.124"),   TEXT("-1000.123") },
		{ TEXT("-1000.124"),  TEXT("-1000.124"),  TEXT("-1000.123"),  TEXT("-1000.124"),  TEXT("-1000.123"),  TEXT("-1000.124"),   TEXT("-1000.123") },
		{ TEXT("-1000.124"),  TEXT("-1000.124"),  TEXT("-1000.124"),  TEXT("-1000.124"),  TEXT("-1000.123"),  TEXT("-1000.124"),   TEXT("-1000.123") },

		{ TEXT("-1000.124"),  TEXT("-1000.124"),  TEXT("-1000.124"),  TEXT("-1000.125"),  TEXT("-1000.124"),  TEXT("-1000.125"),   TEXT("-1000.124") },
		{ TEXT("-1000.124"),  TEXT("-1000.125"),  TEXT("-1000.124"),  TEXT("-1000.125"),  TEXT("-1000.124"),  TEXT("-1000.125"),   TEXT("-1000.124") },
		{ TEXT("-1000.125"),  TEXT("-1000.125"),  TEXT("-1000.125"),  TEXT("-1000.125"),  TEXT("-1000.124"),  TEXT("-1000.125"),   TEXT("-1000.124") },
		{ TEXT("-1000.125"),  TEXT("-1000.125"),  TEXT("-1000.125"),  TEXT("-1000.126"),  TEXT("-1000.125"),  TEXT("-1000.126"),   TEXT("-1000.125") },
		{ TEXT("-1000.126"),  TEXT("-1000.126"),  TEXT("-1000.125"),  TEXT("-1000.126"),  TEXT("-1000.125"),  TEXT("-1000.126"),   TEXT("-1000.125") },
		{ TEXT("-1000.126"),  TEXT("-1000.126"),  TEXT("-1000.126"),  TEXT("-1000.126"),  TEXT("-1000.125"),  TEXT("-1000.126"),   TEXT("-1000.125") },
	};

	static_assert(ARRAY_COUNT(InputValues) == ARRAY_COUNT(OutputValues), "The size of InputValues does not match OutputValues");

	// This test needs to be run using an English culture
	FInternationalization& I18N = FInternationalization::Get();
	const FString OriginalCulture = I18N.GetCurrentCulture()->GetName();
	I18N.SetCurrentCulture(TEXT("en"));

	// Test to make sure that the decimal formatter is rounding fractional numbers correctly (to 3 decimal places)
	FNumberFormattingOptions FormattingOptions = FNumberFormattingOptions()
		.SetUseGrouping(false)
		.SetMaximumFractionalDigits(3);

	auto DoSingleTest = [&](const double InNumber, const FString& InExpectedString, const FString& InDescription)
	{
		const FText ResultText = FText::AsNumber(InNumber, &FormattingOptions);
		if(ResultText.ToString() != InExpectedString)
		{
			AddError(FString::Printf(TEXT("Text rounding failure: source '%f' - expected '%s' - result '%s'. %s."), InNumber, *InExpectedString, *ResultText.ToString(), *InDescription));
		}
	};

	auto DoAllTests = [&](const ERoundingMode InRoundingMode)
	{
		FormattingOptions.SetRoundingMode(InRoundingMode);

		for (int32 TestValueIndex = 0; TestValueIndex < ARRAY_COUNT(InputValues); ++TestValueIndex)
		{
			DoSingleTest(InputValues[TestValueIndex], OutputValues[TestValueIndex][InRoundingMode], RoundingModeNames[InRoundingMode]);
		}
	};

	DoAllTests(ERoundingMode::HalfToEven);
	DoAllTests(ERoundingMode::HalfFromZero);
	DoAllTests(ERoundingMode::HalfToZero);
	DoAllTests(ERoundingMode::FromZero);
	DoAllTests(ERoundingMode::ToZero);
	DoAllTests(ERoundingMode::ToNegativeInfinity);
	DoAllTests(ERoundingMode::ToPositiveInfinity);

	// HalfToEven - Rounds to the nearest place, equidistant ties go to the value which is closest to an even value: 1.5 becomes 2, 0.5 becomes 0
	{
		FormattingOptions.SetRoundingMode(ERoundingMode::HalfToEven);

		DoSingleTest(1000.12459, TEXT("1000.125"), TEXT("HalfToEven"));
		DoSingleTest(1000.124549, TEXT("1000.125"), TEXT("HalfToEven"));
		DoSingleTest(1000.124551, TEXT("1000.125"), TEXT("HalfToEven"));
		DoSingleTest(1000.12451, TEXT("1000.125"), TEXT("HalfToEven"));
		DoSingleTest(1000.1245000001, TEXT("1000.125"), TEXT("HalfToEven"));
		DoSingleTest(1000.12450000000001, TEXT("1000.124"), TEXT("HalfToEven"));

		DoSingleTest(512.9999, TEXT("513"), TEXT("HalfToEven"));
		DoSingleTest(-512.9999, TEXT("-513"), TEXT("HalfToEven"));
	}

	// Restore original culture
	I18N.SetCurrentCulture(OriginalCulture);

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextPaddingTest, "System.Core.Misc.TextPadding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTextPaddingTest::RunTest (const FString& Parameters)
{
	// This test needs to be run using an English culture
	FInternationalization& I18N = FInternationalization::Get();
	const FString OriginalCulture = I18N.GetCurrentCulture()->GetName();
	I18N.SetCurrentCulture(TEXT("en"));

	// Test to make sure that the decimal formatter is padding integral numbers correctly
	FNumberFormattingOptions FormattingOptions;

	auto DoSingleIntTest = [&](const int32 InNumber, const FString& InExpectedString, const FString& InDescription)
	{
		const FText ResultText = FText::AsNumber(InNumber, &FormattingOptions);
		if(ResultText.ToString() != InExpectedString)
		{
			AddError(FString::Printf(TEXT("Text padding failure: source '%d' - expected '%s' - result '%s'. %s."), InNumber, *InExpectedString, *ResultText.ToString(), *InDescription));
		}
	};

	auto DoSingleDoubleTest = [&](const double InNumber, const FString& InExpectedString, const FString& InDescription)
	{
		const FText ResultText = FText::AsNumber(InNumber, &FormattingOptions);
		if(ResultText.ToString() != InExpectedString)
		{
			AddError(FString::Printf(TEXT("Text padding failure: source '%f' - expected '%s' - result '%s'. %s."), InNumber, *InExpectedString, *ResultText.ToString(), *InDescription));
		}
	};

	// Test with a max limit of 3
	{
		FormattingOptions = FNumberFormattingOptions()
			.SetUseGrouping(false)
			.SetMaximumIntegralDigits(3);

		DoSingleIntTest(123456,  TEXT("456"),  TEXT("Truncating '123456' to a max of 3 integral digits"));
		DoSingleIntTest(-123456, TEXT("-456"), TEXT("Truncating '-123456' to a max of 3 integral digits"));
	}

	// Test with a min limit of 6
	{
		FormattingOptions = FNumberFormattingOptions()
			.SetUseGrouping(false)
			.SetMinimumIntegralDigits(6);

		DoSingleIntTest(123,  TEXT("000123"),  TEXT("Padding '123' to a min of 6 integral digits"));
		DoSingleIntTest(-123, TEXT("-000123"), TEXT("Padding '-123' to a min of 6 integral digits"));
	}

	// Test with forced fractional digits
	{
		FormattingOptions = FNumberFormattingOptions()
			.SetUseGrouping(false)
			.SetMinimumFractionalDigits(3);

		DoSingleIntTest(123,  TEXT("123.000"),  TEXT("Padding '123' to a min of 3 fractional digits"));
		DoSingleIntTest(-123, TEXT("-123.000"), TEXT("Padding '-123' to a min of 3 fractional digits"));
	}

	// Testing with leading zeros on a real number
	{
		FormattingOptions = FNumberFormattingOptions()
			.SetUseGrouping(false)
			.SetMaximumFractionalDigits(4);

		DoSingleDoubleTest(0.00123,  TEXT("0.0012"),  TEXT("Padding '0.00123' to a max of 4 fractional digits"));
		DoSingleDoubleTest(-0.00123, TEXT("-0.0012"), TEXT("Padding '-0.00123' to a max of 4 fractional digits"));
	}

	// Testing with leading zeros on a real number
	{
		FormattingOptions = FNumberFormattingOptions()
			.SetUseGrouping(false)
			.SetMaximumFractionalDigits(8);

		DoSingleDoubleTest(0.00123,  TEXT("0.00123"),  TEXT("Padding '0.00123' to a max of 8 fractional digits"));
		DoSingleDoubleTest(-0.00123, TEXT("-0.00123"), TEXT("Padding '-0.00123' to a max of 8 fractional digits"));
	}

	// Test with forced fractional digits on a real number
	{
		FormattingOptions = FNumberFormattingOptions()
			.SetUseGrouping(false)
			.SetMinimumFractionalDigits(8)
			.SetMaximumFractionalDigits(8);

		DoSingleDoubleTest(0.00123,  TEXT("0.00123000"),  TEXT("Padding '0.00123' to a min of 8 fractional digits"));
		DoSingleDoubleTest(-0.00123, TEXT("-0.00123000"), TEXT("Padding '-0.00123' to a min of 8 fractional digits"));
	}

	// Restore original culture
	I18N.SetCurrentCulture(OriginalCulture);

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextFormatArgModifierTest, "System.Core.Misc.TextFormatArgModifiers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTextFormatArgModifierTest::RunTest(const FString& Parameters)
{
	auto EnsureValidResult = [&](const FString& InResult, const FString& InExpected, const FString& InName, const FString& InDescription)
	{
		if (!InResult.Equals(InExpected, ESearchCase::CaseSensitive))
		{
			AddError(FString::Printf(TEXT("%s failure: result '%s' (expected '%s'). %s."), *InName, *InResult, *InExpected, *InDescription));
		}
	};

	{
		const FTextFormat CardinalFormatText = FText::FromString(TEXT("There {NumCats}|plural(one=is,other=are) {NumCats} {NumCats}|plural(one=cat,other=cats)"));
		EnsureValidResult(FText::FormatNamed(CardinalFormatText, TEXT("NumCats"), 0).ToString(), TEXT("There are 0 cats"), TEXT("CardinalResult0"), CardinalFormatText.GetSourceText().ToString());
		EnsureValidResult(FText::FormatNamed(CardinalFormatText, TEXT("NumCats"), 1).ToString(), TEXT("There is 1 cat"), TEXT("CardinalResult1"), CardinalFormatText.GetSourceText().ToString());
		EnsureValidResult(FText::FormatNamed(CardinalFormatText, TEXT("NumCats"), 2).ToString(), TEXT("There are 2 cats"), TEXT("CardinalResult2"), CardinalFormatText.GetSourceText().ToString());
		EnsureValidResult(FText::FormatNamed(CardinalFormatText, TEXT("NumCats"), 3).ToString(), TEXT("There are 3 cats"), TEXT("CardinalResult3"), CardinalFormatText.GetSourceText().ToString());
		EnsureValidResult(FText::FormatNamed(CardinalFormatText, TEXT("NumCats"), 4).ToString(), TEXT("There are 4 cats"), TEXT("CardinalResult4"), CardinalFormatText.GetSourceText().ToString());
	}

	{
		const FTextFormat OrdinalFormatText = FText::FromString(TEXT("You came {Place}{Place}|ordinal(one=st,two=nd,few=rd,other=th)!"));
		EnsureValidResult(FText::FormatNamed(OrdinalFormatText, TEXT("Place"), 0).ToString(), TEXT("You came 0th!"), TEXT("OrdinalResult0"), OrdinalFormatText.GetSourceText().ToString());
		EnsureValidResult(FText::FormatNamed(OrdinalFormatText, TEXT("Place"), 1).ToString(), TEXT("You came 1st!"), TEXT("OrdinalResult1"), OrdinalFormatText.GetSourceText().ToString());
		EnsureValidResult(FText::FormatNamed(OrdinalFormatText, TEXT("Place"), 2).ToString(), TEXT("You came 2nd!"), TEXT("OrdinalResult2"), OrdinalFormatText.GetSourceText().ToString());
		EnsureValidResult(FText::FormatNamed(OrdinalFormatText, TEXT("Place"), 3).ToString(), TEXT("You came 3rd!"), TEXT("OrdinalResult3"), OrdinalFormatText.GetSourceText().ToString());
		EnsureValidResult(FText::FormatNamed(OrdinalFormatText, TEXT("Place"), 4).ToString(), TEXT("You came 4th!"), TEXT("OrdinalResult4"), OrdinalFormatText.GetSourceText().ToString());
	}

	{
		const FTextFormat GenderFormatText = FText::FromString(TEXT("{Gender}|gender(Le,La) {Gender}|gender(guerrier,guerrière) est {Gender}|gender(fort,forte)"));
		EnsureValidResult(FText::FormatNamed(GenderFormatText, TEXT("Gender"), ETextGender::Masculine).ToString(), TEXT("Le guerrier est fort"), TEXT("GenderResultM"), GenderFormatText.GetSourceText().ToString());
		EnsureValidResult(FText::FormatNamed(GenderFormatText, TEXT("Gender"), ETextGender::Feminine).ToString(), TEXT("La guerrière est forte"), TEXT("GenderResultF"), GenderFormatText.GetSourceText().ToString());
	}

	{
		const FTextFormat GenderFormatText = FText::FromString(TEXT("{Gender}|gender(Le guerrier est fort,La guerrière est forte)"));
		EnsureValidResult(FText::FormatNamed(GenderFormatText, TEXT("Gender"), ETextGender::Masculine).ToString(), TEXT("Le guerrier est fort"), TEXT("GenderResultM"), GenderFormatText.GetSourceText().ToString());
		EnsureValidResult(FText::FormatNamed(GenderFormatText, TEXT("Gender"), ETextGender::Feminine).ToString(), TEXT("La guerrière est forte"), TEXT("GenderResultF"), GenderFormatText.GetSourceText().ToString());
	}

	{
		const FText Consonant = FText::FromString(TEXT("\uC0AC\uB78C")/* 사람 */);
		const FText ConsonantRieul = FText::FromString(TEXT("\uC11C\uC6B8")/* 서울 */);
		const FText Vowel = FText::FromString(TEXT("\uC0AC\uC790")/* 사자 */);

		{
			const FTextFormat HppFormatText = FText::FromString(TEXT("{Arg}|hpp(\uC740,\uB294)"));/* 은/는 */
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Consonant).ToString(), TEXT("\uC0AC\uB78C\uC740"), TEXT("HppResultConsonant"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), ConsonantRieul).ToString(), TEXT("\uC11C\uC6B8\uC740"), TEXT("HppResultConsonantRieul"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Vowel).ToString(), TEXT("\uC0AC\uC790\uB294"), TEXT("HppResultVowel"), HppFormatText.GetSourceText().ToString());
		}

		{
			const FTextFormat HppFormatText = FText::FromString(TEXT("{Arg}|hpp(\uC774,\uAC00)"));/* 이/가 */
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Consonant).ToString(), TEXT("\uC0AC\uB78C\uC774"), TEXT("HppResultConsonant"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), ConsonantRieul).ToString(), TEXT("\uC11C\uC6B8\uC774"), TEXT("HppResultConsonantRieul"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Vowel).ToString(), TEXT("\uC0AC\uC790\uAC00"), TEXT("HppResultVowel"), HppFormatText.GetSourceText().ToString());
		}

		{
			const FTextFormat HppFormatText = FText::FromString(TEXT("{Arg}|hpp(\uC744,\uB97C)"));/* 을/를 */
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Consonant).ToString(), TEXT("\uC0AC\uB78C\uC744"), TEXT("HppResultConsonant"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), ConsonantRieul).ToString(), TEXT("\uC11C\uC6B8\uC744"), TEXT("HppResultConsonantRieul"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Vowel).ToString(), TEXT("\uC0AC\uC790\uB97C"), TEXT("HppResultVowel"), HppFormatText.GetSourceText().ToString());
		}

		{
			const FTextFormat HppFormatText = FText::FromString(TEXT("{Arg}|hpp(\uACFC,\uC640)"));/* 과/와 */
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Consonant).ToString(), TEXT("\uC0AC\uB78C\uACFC"), TEXT("HppResultConsonant"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), ConsonantRieul).ToString(), TEXT("\uC11C\uC6B8\uACFC"), TEXT("HppResultConsonantRieul"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Vowel).ToString(), TEXT("\uC0AC\uC790\uC640"), TEXT("HppResultVowel"), HppFormatText.GetSourceText().ToString());
		}

		{
			const FTextFormat HppFormatText = FText::FromString(TEXT("{Arg}|hpp(\uC544,\uC57C)"));/* 아/야 */
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Consonant).ToString(), TEXT("\uC0AC\uB78C\uC544"), TEXT("HppResultConsonant"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), ConsonantRieul).ToString(), TEXT("\uC11C\uC6B8\uC544"), TEXT("HppResultConsonantRieul"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Vowel).ToString(), TEXT("\uC0AC\uC790\uC57C"), TEXT("HppResultVowel"), HppFormatText.GetSourceText().ToString());
		}

		{
			const FTextFormat HppFormatText = FText::FromString(TEXT("{Arg}|hpp(\uC774\uC5B4,\uC5EC)"));/* 이어/여 */
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Consonant).ToString(), TEXT("\uC0AC\uB78C\uC774\uC5B4"), TEXT("HppResultConsonant"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), ConsonantRieul).ToString(), TEXT("\uC11C\uC6B8\uC774\uC5B4"), TEXT("HppResultConsonantRieul"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Vowel).ToString(), TEXT("\uC0AC\uC790\uC5EC"), TEXT("HppResultVowel"), HppFormatText.GetSourceText().ToString());
		}

		{
			const FTextFormat HppFormatText = FText::FromString(TEXT("{Arg}|hpp(\uC774\uC5D0,\uC608)"));/* 이에/예 */
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Consonant).ToString(), TEXT("\uC0AC\uB78C\uC774\uC5D0"), TEXT("HppResultConsonant"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), ConsonantRieul).ToString(), TEXT("\uC11C\uC6B8\uC774\uC5D0"), TEXT("HppResultConsonantRieul"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Vowel).ToString(), TEXT("\uC0AC\uC790\uC608"), TEXT("HppResultVowel"), HppFormatText.GetSourceText().ToString());
		}

		{
			const FTextFormat HppFormatText = FText::FromString(TEXT("{Arg}|hpp(\uC774\uC5C8,​\uC600)"));/* 이었/​였 */
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Consonant).ToString(), TEXT("\uC0AC\uB78C\uC774\uC5C8"), TEXT("HppResultConsonant"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), ConsonantRieul).ToString(), TEXT("\uC11C\uC6B8\uC774\uC5C8"), TEXT("HppResultConsonantRieul"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Vowel).ToString(), TEXT("\uC0AC\uC790​\uC600"), TEXT("HppResultVowel"), HppFormatText.GetSourceText().ToString());
		}

		{
			const FTextFormat HppFormatText = FText::FromString(TEXT("{Arg}|hpp(\uC73C\uB85C,\uB85C)"));/* 으로/로 */
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Consonant).ToString(), TEXT("\uC0AC\uB78C\uC73C\uB85C"), TEXT("HppResultConsonant"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), ConsonantRieul).ToString(), TEXT("\uC11C\uC6B8\uB85C"), TEXT("HppResultConsonantRieul"), HppFormatText.GetSourceText().ToString());
			EnsureValidResult(FText::FormatNamed(HppFormatText, TEXT("Arg"), Vowel).ToString(), TEXT("\uC0AC\uC790\uB85C"), TEXT("HppResultVowel"), HppFormatText.GetSourceText().ToString());
		}
	}

	return true;
}

#if UE_ENABLE_ICU

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FICUSanitizationTest, "System.Core.Misc.ICUSanitization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FICUSanitizationTest::RunTest(const FString& Parameters)
{
	// Validate culture code sanitization
	{
		auto TestCultureCodeSanitization = [this](const FString& InCode, const FString& InExpectedCode)
		{
			const FString SanitizedCode = ICUUtilities::SanitizeCultureCode(InCode);
			if (SanitizedCode != InExpectedCode)
			{
				AddError(FString::Printf(TEXT("SanitizeCultureCode did not produce the expected result (got '%s', expected '%s')"), *SanitizedCode, *InExpectedCode));
			}
		};

		TestCultureCodeSanitization(TEXT("en-US"), TEXT("en-US"));
		TestCultureCodeSanitization(TEXT("en_US_POSIX"), TEXT("en_US_POSIX"));
		TestCultureCodeSanitization(TEXT("en-US{}%"), TEXT("en-US"));
		TestCultureCodeSanitization(TEXT("en{}%-US"), TEXT("en-US"));
	}

	// Validate timezone code sanitization
	{
		auto TestTimezoneCodeSanitization = [this](const FString& InCode, const FString& InExpectedCode)
		{
			const FString SanitizedCode = ICUUtilities::SanitizeTimezoneCode(InCode);
			if (SanitizedCode != InExpectedCode)
			{
				AddError(FString::Printf(TEXT("SanitizeTimezoneCode did not produce the expected result (got '%s', expected '%s')"), *SanitizedCode, *InExpectedCode));
			}
		};

		TestTimezoneCodeSanitization(TEXT("Etc/Unknown"), TEXT("Etc/Unknown"));
		TestTimezoneCodeSanitization(TEXT("America/Sao_Paulo"), TEXT("America/Sao_Paulo"));
		TestTimezoneCodeSanitization(TEXT("America/Sao_Paulo{}%"), TEXT("America/Sao_Paulo"));
		TestTimezoneCodeSanitization(TEXT("America/Sao{}%_Paulo"), TEXT("America/Sao_Paulo"));
		TestTimezoneCodeSanitization(TEXT("Antarctica/DumontDUrville"), TEXT("Antarctica/DumontDUrville"));
		TestTimezoneCodeSanitization(TEXT("Antarctica/DumontDUrville{}%"), TEXT("Antarctica/DumontDUrville"));
		TestTimezoneCodeSanitization(TEXT("Antarctica/Dumont{}%DUrville"), TEXT("Antarctica/DumontDUrville"));
		TestTimezoneCodeSanitization(TEXT("Antarctica/DumontD'Urville"), TEXT("Antarctica/DumontDUrville"));
		TestTimezoneCodeSanitization(TEXT("Antarctica/DumontDUrville_Dumont"), TEXT("Antarctica/DumontDUrville"));
		TestTimezoneCodeSanitization(TEXT("GMT-8:00"), TEXT("GMT-8:00"));
		TestTimezoneCodeSanitization(TEXT("GMT-8:00{}%"), TEXT("GMT-8:00"));
		TestTimezoneCodeSanitization(TEXT("GMT-{}%8:00"), TEXT("GMT-8:00"));
	}

	// Validate currency code sanitization
	{
		auto TestCurrencyCodeSanitization = [this](const FString& InCode, const FString& InExpectedCode)
		{
			const FString SanitizedCode = ICUUtilities::SanitizeCurrencyCode(InCode);
			if (SanitizedCode != InExpectedCode)
			{
				AddError(FString::Printf(TEXT("SanitizeCurrencyCode did not produce the expected result (got '%s', expected '%s')"), *SanitizedCode, *InExpectedCode));
			}
		};

		TestCurrencyCodeSanitization(TEXT("USD"), TEXT("USD"));
		TestCurrencyCodeSanitization(TEXT("USD{}%"), TEXT("USD"));
		TestCurrencyCodeSanitization(TEXT("U{}%SD"), TEXT("USD"));
		TestCurrencyCodeSanitization(TEXT("USDUSD"), TEXT("USD"));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FICUTextTest, "System.Core.Misc.ICUText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FICUTextTest::RunTest (const FString& Parameters)
{
	// Test to make sure that ICUUtilities converts strings correctly

	const FString SourceString(TEXT("This is a test"));
	const FString SourceString2(TEXT("This is another test"));
	icu::UnicodeString ICUString;
	FString ConversionBackStr;

	// ---------------------------------------------------------------------

	ICUUtilities::ConvertString(SourceString, ICUString);
	if (SourceString.Len() != ICUString.countChar32())
	{
		AddError(FString::Printf(TEXT("icu::UnicodeString is the incorrect length (%d; expected %d)."), ICUString.countChar32(), SourceString.Len()));
	}

	ICUUtilities::ConvertString(ICUString, ConversionBackStr);
	if (ICUString.length() != ConversionBackStr.Len())
	{
		AddError(FString::Printf(TEXT("FString is the incorrect length (%d; expected %d)."), ConversionBackStr.Len(), ICUString.countChar32()));
	}
	if (SourceString != ConversionBackStr)
	{
		AddError(FString::Printf(TEXT("FString is has the incorrect converted value ('%s'; expected '%s')."), *ConversionBackStr, *SourceString));
	}

	// ---------------------------------------------------------------------

	ICUUtilities::ConvertString(SourceString2, ICUString);
	if (SourceString2.Len() != ICUString.countChar32())
	{
		AddError(FString::Printf(TEXT("icu::UnicodeString is the incorrect length (%d; expected %d)."), ICUString.countChar32(), SourceString2.Len()));
	}

	ICUUtilities::ConvertString(ICUString, ConversionBackStr);
	if (ICUString.length() != ConversionBackStr.Len())
	{
		AddError(FString::Printf(TEXT("FString is the incorrect length (%d; expected %d)."), ConversionBackStr.Len(), ICUString.countChar32()));
	}
	if (SourceString2 != ConversionBackStr)
	{
		AddError(FString::Printf(TEXT("FString is has the incorrect converted value ('%s'; expected '%s')."), *ConversionBackStr, *SourceString2));
	}

	// ---------------------------------------------------------------------

	ICUUtilities::ConvertString(SourceString, ICUString);
	if (SourceString.Len() != ICUString.countChar32())
	{
		AddError(FString::Printf(TEXT("icu::UnicodeString is the incorrect length (%d; expected %d)."), ICUString.countChar32(), SourceString.Len()));
	}

	ICUUtilities::ConvertString(ICUString, ConversionBackStr);
	if (ICUString.length() != ConversionBackStr.Len())
	{
		AddError(FString::Printf(TEXT("FString is the incorrect length (%d; expected %d)."), ConversionBackStr.Len(), ICUString.countChar32()));
	}
	if (SourceString != ConversionBackStr)
	{
		AddError(FString::Printf(TEXT("FString is has the incorrect converted value ('%s'; expected '%s')."), *ConversionBackStr, *SourceString));
	}

	return true;
}

#endif // #if UE_ENABLE_ICU

#undef LOCTEXT_NAMESPACE 


PRAGMA_ENABLE_OPTIMIZATION

#endif //WITH_DEV_AUTOMATION_TESTS
