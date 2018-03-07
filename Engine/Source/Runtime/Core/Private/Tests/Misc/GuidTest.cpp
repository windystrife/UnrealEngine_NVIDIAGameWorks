// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuidTest, "System.Core.Misc.Guid", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGuidTest::RunTest( const FString& Parameters )
{
	FGuid g = FGuid(305419896, 2271560481, 305419896, 2271560481);

	// string conversions
	TestEqual(TEXT("String conversion (Default) must return EGuidFormats::Digits string"), g.ToString(), g.ToString(EGuidFormats::Digits));
	TestEqual<FString>(TEXT("String conversion (EGuidFormats::Digits)"), g.ToString(EGuidFormats::Digits), TEXT("12345678876543211234567887654321"));
	TestEqual<FString>(TEXT("String conversion (EGuidFormats::DigitsWithHyphens)"), g.ToString(EGuidFormats::DigitsWithHyphens), TEXT("12345678-8765-4321-1234-567887654321"));
	TestEqual<FString>(TEXT("String conversion (EGuidFormats::DigitsWithHyphensInBraces)"), g.ToString(EGuidFormats::DigitsWithHyphensInBraces), TEXT("{12345678-8765-4321-1234-567887654321}"));
	TestEqual<FString>(TEXT("String conversion (EGuidFormats::DigitsWithHyphensInParentheses)"), g.ToString(EGuidFormats::DigitsWithHyphensInParentheses), TEXT("(12345678-8765-4321-1234-567887654321)"));
	TestEqual<FString>(TEXT("String conversion (EGuidFormats::HexValuesInBraces)"), g.ToString(EGuidFormats::HexValuesInBraces), TEXT("{0x12345678,0x8765,0x4321,{0x12,0x34,0x56,0x78,0x87,0x65,0x43,0x21}}"));
	TestEqual<FString>(TEXT("String conversion (EGuidFormats::UniqueObjectGuid)"), g.ToString(EGuidFormats::UniqueObjectGuid), TEXT("12345678-87654321-12345678-87654321"));

	// parsing valid strings (exact)
	FGuid g2_1;

	TestTrue(TEXT("Parsing valid strings must succeed (EGuidFormats::Digits)"), FGuid::ParseExact(TEXT("12345678876543211234567887654321"), EGuidFormats::Digits, g2_1));
	TestEqual(TEXT("Parsing valid strings must succeed (EGuidFormats::Digits)"), g2_1, g);

	TestTrue(TEXT("Parsing valid strings must succeed (EGuidFormats::DigitsWithHyphens)"), FGuid::ParseExact(TEXT("12345678-8765-4321-1234-567887654321"), EGuidFormats::DigitsWithHyphens, g2_1));
	TestEqual(TEXT("Parsing valid strings must succeed (EGuidFormats::DigitsWithHyphens)"), g2_1, g);

	TestTrue(TEXT("Parsing valid strings must succeed (EGuidFormats::DigitsWithHyphensInBraces)"), FGuid::ParseExact(TEXT("{12345678-8765-4321-1234-567887654321}"), EGuidFormats::DigitsWithHyphensInBraces, g2_1));
	TestEqual(TEXT("Parsing valid strings must succeed (EGuidFormats::DigitsWithHyphensInBraces)"), g2_1, g);

	TestTrue(TEXT("Parsing valid strings must succeed (EGuidFormats::DigitsWithHyphensInParentheses)"), FGuid::ParseExact(TEXT("(12345678-8765-4321-1234-567887654321)"), EGuidFormats::DigitsWithHyphensInParentheses, g2_1));
	TestEqual(TEXT("Parsing valid strings must succeed (EGuidFormats::DigitsWithHyphensInParentheses)"), g2_1, g);

	TestTrue(TEXT("Parsing valid strings must succeed (EGuidFormats::HexValuesInBraces)"), FGuid::ParseExact(TEXT("{0x12345678,0x8765,0x4321,{0x12,0x34,0x56,0x78,0x87,0x65,0x43,0x21}}"), EGuidFormats::HexValuesInBraces, g2_1));
	TestEqual(TEXT("Parsing valid strings must succeed (EGuidFormats::HexValuesInBraces)"), g2_1, g);

	TestTrue(TEXT("Parsing valid strings must succeed (EGuidFormats::UniqueObjectGuid)"), FGuid::ParseExact(TEXT("12345678-87654321-12345678-87654321"), EGuidFormats::UniqueObjectGuid, g2_1));
	TestEqual(TEXT("Parsing valid strings must succeed (EGuidFormats::UniqueObjectGuid)"), g2_1, g);

	// parsing invalid strings (exact)


	// parsing valid strings (automatic)
	FGuid g3_1;

	TestTrue(TEXT("Parsing valid strings must succeed (12345678876543211234567887654321)"), FGuid::Parse(TEXT("12345678876543211234567887654321"), g3_1));
	TestEqual(TEXT("Parsing valid strings must succeed (12345678876543211234567887654321)"), g3_1, g);

	TestTrue(TEXT("Parsing valid strings must succeed (12345678-8765-4321-1234-567887654321)"), FGuid::Parse(TEXT("12345678-8765-4321-1234-567887654321"), g3_1));
	TestEqual(TEXT("Parsing valid strings must succeed (12345678-8765-4321-1234-567887654321)"), g3_1, g);

	TestTrue(TEXT("Parsing valid strings must succeed ({12345678-8765-4321-1234-567887654321})"), FGuid::Parse(TEXT("{12345678-8765-4321-1234-567887654321}"), g3_1));
	TestEqual(TEXT("Parsing valid strings must succeed ({12345678-8765-4321-1234-567887654321})"), g3_1, g);

	TestTrue(TEXT("Parsing valid strings must succeed ((12345678-8765-4321-1234-567887654321))"), FGuid::Parse(TEXT("(12345678-8765-4321-1234-567887654321)"), g3_1));
	TestEqual(TEXT("Parsing valid strings must succeed ((12345678-8765-4321-1234-567887654321))"), g3_1, g);

	TestTrue(TEXT("Parsing valid strings must succeed ({0x12345678,0x8765,0x4321,{0x12,0x34,0x56,0x78,0x87,0x65,0x43,0x21}})"), FGuid::Parse(TEXT("{0x12345678,0x8765,0x4321,{0x12,0x34,0x56,0x78,0x87,0x65,0x43,0x21}}"), g3_1));
	TestEqual(TEXT("Parsing valid strings must succeed ({0x12345678,0x8765,0x4321,{0x12,0x34,0x56,0x78,0x87,0x65,0x43,0x21}})"), g3_1, g);

	TestTrue(TEXT("Parsing valid strings must succeed (12345678-87654321-12345678-87654321)"), FGuid::Parse(TEXT("12345678-87654321-12345678-87654321"), g3_1));
	TestEqual(TEXT("Parsing valid strings must succeed (12345678-87654321-12345678-87654321)"), g3_1, g);

	//parsing invalid strings (automatic)

	// GUID validation
	FGuid g4_1 = FGuid::NewGuid();

	TestTrue(TEXT("New GUIDs must be valid"), g4_1.IsValid());
	
	g4_1.Invalidate();

	TestFalse(TEXT("Invalidated GUIDs must be invalid"), g4_1.IsValid());

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
