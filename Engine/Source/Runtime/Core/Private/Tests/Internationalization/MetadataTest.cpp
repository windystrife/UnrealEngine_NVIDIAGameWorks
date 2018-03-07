// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"
#include "Internationalization/InternationalizationMetadata.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetadataTest, "System.Core.Misc.Internationalization Metadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

BEGIN_FUNCTION_BUILD_OPTIMIZATION

bool FMetadataTest::RunTest( const FString& Parameters )
{
	// BooleanValue metadata
	bool BoolFalse = false;
	bool BoolTrue = true;
	TSharedPtr<FLocMetadataValue> MetadataValueBoolFalse = MakeShareable( new FLocMetadataValueBoolean( BoolFalse ) );
	TSharedPtr<FLocMetadataValue> MetadataValueBoolTrue = MakeShareable( new FLocMetadataValueBoolean( BoolTrue ) );

	// StringValue metadata
	FString StringA = TEXT("A");
	FString StringB = TEXT("B");
	TSharedPtr<FLocMetadataValue> MetadataValueStringA = MakeShareable( new FLocMetadataValueString( StringA ) );
	TSharedPtr<FLocMetadataValue> MetadataValueStringB = MakeShareable( new FLocMetadataValueString( StringB ) );

	// ArrayValue metadata
	TArray< TSharedPtr< FLocMetadataValue > > ArrayA;
	TArray< TSharedPtr< FLocMetadataValue > > ArrayB;

	ArrayA.Add( MetadataValueBoolFalse );
	ArrayA.Add( MetadataValueStringA );

	ArrayB.Add( MetadataValueBoolTrue );
	ArrayB.Add( MetadataValueStringB );

	TSharedPtr<FLocMetadataValue> MetadataValueArrayA = MakeShareable( new FLocMetadataValueArray( ArrayA ) );
	TSharedPtr<FLocMetadataValue> MetadataValueArrayB = MakeShareable( new FLocMetadataValueArray( ArrayB ) );

	// Object metadata
	TSharedPtr< FLocMetadataObject > MetadataObjectA = MakeShareable( new FLocMetadataObject );
	TSharedPtr< FLocMetadataObject > MetadataObjectB = MakeShareable( new FLocMetadataObject );

	// Setup object A
	MetadataObjectA->SetField( TEXT("MetadataBoolFalse"), MetadataValueBoolFalse );
	MetadataObjectA->SetField( TEXT("MetadataStringA"), MetadataValueStringA );
	MetadataObjectA->SetField( TEXT("MetadataArrayA"), MetadataValueArrayA );
	MetadataObjectA->SetField( TEXT("*MetadataCompareModifier"), MetadataValueStringA);// Note: The * name prefix modifies the way entries in the object are compared.

	// Setup object B
	MetadataObjectB->SetField( TEXT("MetadataBoolFalse"), MetadataValueBoolTrue );
	MetadataObjectB->SetField( TEXT("MetadataStringB"), MetadataValueStringB );
	MetadataObjectB->SetField( TEXT("MetadataArrayB"), MetadataValueArrayB );
	MetadataObjectA->SetBoolField( TEXT("*MetadataCompareModifier"), true);// Note: Different type/value. The * name prefix modifies the way entries in the object are compared.

	// ObjectValue metadata
	TSharedPtr< FLocMetadataValue > MetadataValueObjectA = MakeShareable( new FLocMetadataValueObject( MetadataObjectA ) );
	TSharedPtr< FLocMetadataValue > MetadataValueObjectB = MakeShareable( new FLocMetadataValueObject( MetadataObjectB ) );

	// Testing the bool meta data value type
	{
		TestFalse( TEXT("MetadataValueBoolFalse == MetadataValueBoolTrue"), *MetadataValueBoolFalse == *MetadataValueBoolTrue );
		TestTrue( TEXT("MetadataValueBoolFalse < MetadataValueBoolTrue"), *MetadataValueBoolFalse < *MetadataValueBoolTrue );
		TestFalse( TEXT("MetadataValueBoolTrue < MetadataValueBoolFalse"), *MetadataValueBoolTrue < *MetadataValueBoolFalse);

		TestTrue( TEXT("MetadataValueBoolFalse < MetadataValueString"), *MetadataValueBoolFalse < *MetadataValueStringA );
		TestTrue( TEXT("MetadataValueBoolTrue < MetadataValueString"), *MetadataValueBoolTrue < *MetadataValueStringA);

		TestTrue( TEXT("MetadataValueBoolFalse < MetadataValueArray"), *MetadataValueBoolFalse < *MetadataValueArrayA );
		TestTrue( TEXT("MetadataValueBoolTrue < MetadataValueArray"), *MetadataValueBoolTrue < *MetadataValueArrayA);

		TestTrue( TEXT("MetadataValueBoolFalse < MetadataValueObject"), *MetadataValueBoolFalse < *MetadataValueObjectA );
		TestTrue( TEXT("MetadataValueBoolTrue < MetadataValueObject"), *MetadataValueBoolTrue < *MetadataValueObjectA);

		TSharedPtr<FLocMetadataValue> MetadataValueBoolFalseClone = MetadataValueBoolFalse->Clone();
		TSharedPtr<FLocMetadataValue> MetadataValueBoolTrueClone = MetadataValueBoolTrue->Clone();

		if( MetadataValueBoolFalse == MetadataValueBoolFalseClone )
		{
			AddError(TEXT("MetadataValueBool and its Clone are not unique objects."));
		}
		
		TestTrue( TEXT("MetadataValueBoolFalseClone == MetadataValueBoolFalse"), *MetadataValueBoolFalseClone == *MetadataValueBoolFalse );
		TestFalse( TEXT("MetadataValueBoolFalseClone < MetadataValueBoolFalse"), *MetadataValueBoolFalseClone < *MetadataValueBoolFalse );

		TestTrue( TEXT("MetadataValueBoolTrueClone == MetadataValueBoolTrue"), *MetadataValueBoolTrueClone == *MetadataValueBoolTrue );
		TestFalse( TEXT("MetadataValueBoolTrueClone < MetadataValueBoolTrue"), *MetadataValueBoolTrueClone < *MetadataValueBoolTrue );

		// Test the bool metadata when it is part of an object
		TSharedPtr< FLocMetadataObject > MetadataObjectFalse = MakeShareable( new FLocMetadataObject );
		MetadataObjectFalse->SetField(TEXT("MetadataValueBool"), MetadataValueBoolFalse);

		TSharedPtr< FLocMetadataObject > MetadataObjectTrue = MakeShareable( new FLocMetadataObject );
		MetadataObjectTrue->SetField(TEXT("MetadataValueBool"), MetadataValueBoolTrue);

		TestFalse( TEXT("GetBoolField(MetadataValueBool)"), MetadataObjectFalse->GetBoolField( TEXT("MetadataValueBool") ) );
		TestTrue( TEXT("GetBoolField(MetadataValueBool)"), MetadataObjectTrue->GetBoolField( TEXT("MetadataValueBool") ) );

		TestFalse( TEXT("MetadataObjectFalse == MetadataObjectTrue"), *MetadataObjectFalse == *MetadataObjectTrue );
		TestTrue( TEXT("MetadataObjectFalse < MetadataObjectTrue"), *MetadataObjectFalse < *MetadataObjectTrue );
	}

	// Testing string metadata value type
	{
		TestFalse( TEXT("MetadataValueStringA == MetadataValueStringB"), *MetadataValueStringA == *MetadataValueStringB );
		TestTrue( TEXT("MetadataValueStringA < MetadataValueStringB"), *MetadataValueStringA < *MetadataValueStringB );
		TestFalse( TEXT("MetadataValueStringB < MetadataValueStringA"), *MetadataValueStringB < *MetadataValueStringA);

		TestTrue( TEXT("MetadataValueString < MetadataValueArray"), *MetadataValueStringA < *MetadataValueArrayA );

		TestTrue( TEXT("MetadataValueStringA < MetadataValueObject"), *MetadataValueStringA < *MetadataValueObjectA );


		TSharedPtr<FLocMetadataValue> MetadataValueStringAClone = MetadataValueStringA->Clone();

		if( MetadataValueStringA == MetadataValueStringAClone )
		{
			AddError(TEXT("MetadataValueString and its Clone are not unique objects."));
		}

		TestTrue( TEXT("MetadataValueStringAClone == MetadataValueStringA"), *MetadataValueStringAClone == *MetadataValueStringA );
		TestFalse( TEXT("MetadataValueStringAClone < MetadataValueStringA"), *MetadataValueStringAClone < *MetadataValueStringA );
		TestTrue( TEXT("MetadataValueStringAClone < MetadataValueStringB"), *MetadataValueStringAClone < *MetadataValueStringB );

		// Test the string metadata when it is part of an object
		TSharedPtr< FLocMetadataObject > TestMetadataObjectA = MakeShareable( new FLocMetadataObject );
		TestMetadataObjectA->SetField(TEXT("MetadataValueString"), MetadataValueStringA);

		TSharedPtr< FLocMetadataObject > TestMetadataObjectB = MakeShareable( new FLocMetadataObject );
		TestMetadataObjectB->SetField(TEXT("MetadataValueString"), MetadataValueStringB);

		TestTrue( TEXT("GetStringField(MetadataValueString) == StringA"), TestMetadataObjectA->GetStringField( TEXT("MetadataValueString") ) == StringA );
	
		TestFalse( TEXT("TestMetadataObjectA == TestMetadataObjectB"), *TestMetadataObjectA == *TestMetadataObjectB );
		TestTrue( TEXT("TestMetadataObjectA < TestMetadataObjectB"), *TestMetadataObjectA < *TestMetadataObjectB );
	}

	// Testing array metadata value type
	{
		TestFalse( TEXT("MetadataValueArrayA == MetadataValueArrayB"), *MetadataValueArrayA == *MetadataValueArrayB );
		TestTrue( TEXT("MetadataValueArrayA < MetadataValueArrayB"), *MetadataValueArrayA < *MetadataValueArrayB );
		TestFalse( TEXT("MetadataValueArrayB < MetadataValueArrayA"), *MetadataValueArrayB < *MetadataValueArrayA);

		TestTrue( TEXT("MetadataValueArrayA < MetadataValueObject"), *MetadataValueArrayA < *MetadataValueObjectA );

		TSharedPtr<FLocMetadataValue> MetadataValueArrayAClone = MetadataValueArrayA->Clone();

		if( MetadataValueArrayA == MetadataValueArrayAClone )
		{
			AddError(TEXT("MetadataValueString and its Clone are not unique objects."));
		}

		TestTrue( TEXT("MetadataValueArrayAClone == MetadataValueArrayA"), *MetadataValueArrayAClone == *MetadataValueArrayA );
		TestFalse( TEXT("MetadataValueArrayAClone < MetadataValueArrayA"), *MetadataValueArrayAClone < *MetadataValueArrayA );
		TestTrue( TEXT("MetadataValueArrayAClone < MetadataValueArrayB"), *MetadataValueArrayAClone < *MetadataValueArrayB );

		// Test Less than and equality checks.  Metadata arrays are equivalent if they contain equivalent contents in any order.  
		//  To calculate if a metadata array is less than another, we sort both arrays and check each entry index against its counterpart.
		//  If we encounter an entry that is less than another we stop looking.
		TArray< TSharedPtr< FLocMetadataValue > > ArrayC;
		ArrayC.Add( MetadataValueBoolFalse );
		ArrayC.Add( MetadataValueBoolFalse->Clone() );
		TSharedPtr<FLocMetadataValue> MetadataValueArrayC = MakeShareable( new FLocMetadataValueArray( ArrayC ) );
	
		TestFalse( TEXT("MetadataValueArrayA == MetadataValueArrayC"), *MetadataValueArrayA == *MetadataValueArrayC );
		TestTrue( TEXT("MetadataValueArrayC < MetadataValueArrayA"), *MetadataValueArrayC < *MetadataValueArrayA );
		TestTrue( TEXT("MetadataValueArrayC < MetadataValueArrayB"), *MetadataValueArrayC < *MetadataValueArrayB );

		TArray< TSharedPtr< FLocMetadataValue > > ArrayD;
		ArrayD.Add( MetadataValueBoolFalse );
		ArrayD.Add( MetadataValueBoolFalse->Clone() );
		ArrayD.Add( MetadataValueBoolFalse->Clone() );
		TSharedPtr<FLocMetadataValue> MetadataValueArrayD = MakeShareable( new FLocMetadataValueArray( ArrayD ) );

		TestFalse( TEXT("MetadataValueArrayA == MetadataValueArrayD"), *MetadataValueArrayA == *MetadataValueArrayD );
		TestFalse( TEXT("MetadataValueArrayC == MetadataValueArrayD"), *MetadataValueArrayC == *MetadataValueArrayD );
		TestTrue( TEXT("MetadataValueArrayC < MetadataValueArrayD"), *MetadataValueArrayC < *MetadataValueArrayD );
		TestTrue( TEXT("MetadataValueArrayD < MetadataValueArrayA"), *MetadataValueArrayD < *MetadataValueArrayA );

		// Test the array metadata when it is part of an object
		TSharedPtr< FLocMetadataObject > TestMetadataObjectA = MakeShareable( new FLocMetadataObject );
		TestMetadataObjectA->SetField(TEXT("MetadataValueArray"), MetadataValueArrayA);

		TSharedPtr< FLocMetadataObject > TestMetadataObjectB = MakeShareable( new FLocMetadataObject );
		TestMetadataObjectB->SetField(TEXT("MetadataValueArray"), MetadataValueArrayB);

		TestTrue( TEXT("GetArrayField(MetadataValueArray) == ArrayA"), TestMetadataObjectA->GetArrayField( TEXT("MetadataValueArray") ) == ArrayA );

		TestFalse( TEXT("TestMetadataObjectA == TestMetadataObjectB"), *TestMetadataObjectA == *TestMetadataObjectB );
		TestTrue( TEXT("TestMetadataObjectA < TestMetadataObjectB"), *TestMetadataObjectA < *TestMetadataObjectB );
	}

	// Testing object metadata value type
	{
		TestFalse( TEXT("MetadataValueObjectA == MetadataValueObjectB"), *MetadataValueObjectA == *MetadataValueObjectB );
		TestTrue( TEXT("MetadataValueObjectA < MetadataValueObjectB"), *MetadataValueObjectA < *MetadataValueObjectB );
		TestFalse( TEXT("MetadataValueObjectB < MetadataValueObjectA"), *MetadataValueObjectB < *MetadataValueObjectA);

		TSharedPtr<FLocMetadataValue> MetadataValueObjectAClone = MetadataValueObjectA->Clone();

		if( MetadataValueObjectA == MetadataValueObjectAClone )
		{
			AddError(TEXT("MetadataValueString and its Clone are not unique objects."));
		}

		TestTrue( TEXT("MetadataValueObjectAClone == MetadataValueObjectA"), *MetadataValueObjectAClone == *MetadataValueObjectA );
		TestFalse( TEXT("MetadataValueObjectAClone < MetadataValueObjectA"), *MetadataValueObjectAClone < *MetadataValueObjectA );
		TestTrue( TEXT("MetadataValueObjectAClone < MetadataValueObjectB"), *MetadataValueObjectAClone < *MetadataValueObjectB );


		// Test the object metadata when it is part of another object
		TSharedPtr< FLocMetadataObject > TestMetadataObjectA = MakeShareable( new FLocMetadataObject );
		TestMetadataObjectA->SetField(TEXT("MetadataValueObject"), MetadataValueObjectA);

		TSharedPtr< FLocMetadataObject > TestMetadataObjectB = MakeShareable( new FLocMetadataObject );
		TestMetadataObjectB->SetField(TEXT("MetadataValueObject"), MetadataValueObjectB);

		TestTrue( TEXT("GetObjectField(MetadataValueObject) == MetadataObjectA"), *TestMetadataObjectA->GetObjectField( TEXT("MetadataValueObject") ) == *MetadataObjectA );

		TestFalse( TEXT("TestMetadataObjectA == TestMetadataObjectB"), *TestMetadataObjectA == *TestMetadataObjectB );
		TestTrue( TEXT("TestMetadataObjectA < TestMetadataObjectB"), *TestMetadataObjectA < *TestMetadataObjectB );
	}

	// Testing Loc Metadata Object
	{
		TestFalse( TEXT("MetadataObjectA == MetadataObjectB"), *MetadataObjectA == *MetadataObjectB );
		TestTrue( TEXT("MetadataObjectA < MetadataObjectB"), *MetadataObjectA < *MetadataObjectB );
		TestFalse( TEXT("MetadataObjectB < MetadataObjectA"), *MetadataObjectB < *MetadataObjectA);

		// Test copy ctor
		{
			FLocMetadataObject MetadataObjectAClone = *MetadataObjectA;
			TestTrue( TEXT("MetadataObjectAClone == MetadataObjectA"), MetadataObjectAClone == *MetadataObjectA );
		}
		
		// Test assignment
		{
			FLocMetadataObject MetadataObjectAClone = *MetadataObjectB;
			// PVS-Studio complains about double initialization, but that's something we're testing
			// so we disable the warning
			MetadataObjectAClone = *MetadataObjectA; //-V519
			TestTrue( TEXT("MetadataObjectAClone == MetadataObjectA"), MetadataObjectAClone == *MetadataObjectA );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectB"), MetadataObjectAClone == *MetadataObjectB );
		}
		
		// Test comparison operator  
		{
			FLocMetadataObject MetadataObjectAClone = *MetadataObjectA;
			
			
			// Adding standard entry
			MetadataObjectAClone.SetStringField( TEXT("NewEntry"), TEXT("NewEntryValue") );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectA"), MetadataObjectAClone == *MetadataObjectA );
			
			// Adding non-standard entry.  Note metadata with * prefix in the name will ignore value and type when performing comparisons
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("*NewEntry"), TEXT("*NewEntryValue") );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectA"), MetadataObjectAClone == *MetadataObjectA );

			// Value mismatch on entry with * prefix with same type
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("*NoCompare"), TEXT("NoCompare") );
			FLocMetadataObject MetadataObjectAClone2 = *MetadataObjectA;
			MetadataObjectAClone2.SetStringField( TEXT("*NoCompare"), TEXT("NoCompare2") );
			TestTrue( TEXT("MetadataObjectAClone == MetadataObjectAClone2"), MetadataObjectAClone == MetadataObjectAClone2 );

			// Value and type mismatch on entry with * prefix
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("*NoCompare"), TEXT("NoCompare") );
			MetadataObjectAClone2 = *MetadataObjectA;
			MetadataObjectAClone2.SetBoolField( TEXT("*NoCompare"), true );
			TestTrue( TEXT("MetadataObjectAClone == MetadataObjectAClone2"), MetadataObjectAClone == MetadataObjectAClone2 );
		
			// Value mismatch on standard entry
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("DoCompare"), TEXT("DoCompare") );
			MetadataObjectAClone2 = *MetadataObjectA;
			MetadataObjectAClone2.SetStringField( TEXT("DoCompare"), TEXT("DoCompare2") );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectAClone2"), MetadataObjectAClone == MetadataObjectAClone2 );

			// Value and type mismatch on standard entry
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("DoCompare"), TEXT("DoCompare") );
			MetadataObjectAClone2 = *MetadataObjectA;
			MetadataObjectAClone2.SetBoolField( TEXT("DoCompare"), true );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectAClone2"), MetadataObjectAClone == MetadataObjectAClone2 );
		}		
		
		// Test IsExactMatch function.  Note: Differs from the == operator which takes into account the COMPARISON_MODIFIER_PREFIX
		{
			// This function behaves similar to the comparison operator except that it ensures all metadata entries match exactly.  In other
			//  words, it will also perform exact match checks on * prefixed metadata items.
			FLocMetadataObject MetadataObjectAClone = *MetadataObjectA;

			TestTrue( TEXT("MetadataObjectAClone == MetadataObjectA"), MetadataObjectAClone.IsExactMatch( *MetadataObjectA) );

			// Adding standard entry
			MetadataObjectAClone.SetStringField( TEXT("NewEntry"), TEXT("NewEntryValue") );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectA"), MetadataObjectAClone.IsExactMatch( *MetadataObjectA) );

			// Adding non-standard entry.  Note metadata with * prefix in the name will ignore value and type when performing comparisons
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("*NewEntry"), TEXT("*NewEntryValue") );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectA"),MetadataObjectAClone.IsExactMatch( *MetadataObjectA)  );

			// Value mismatch on entry with * prefix with same type
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("*NoCompare"), TEXT("NoCompare") );
			FLocMetadataObject MetadataObjectAClone2 = *MetadataObjectA;
			MetadataObjectAClone2.SetStringField( TEXT("*NoCompare"), TEXT("NoCompare2") );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectAClone2"), MetadataObjectAClone.IsExactMatch( MetadataObjectAClone2) );

			// Value and type mismatch on entry with * prefix
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("*NoCompare"), TEXT("NoCompare") );
			MetadataObjectAClone2 = *MetadataObjectA;
			MetadataObjectAClone2.SetBoolField( TEXT("*NoCompare"), true );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectAClone2"),  MetadataObjectAClone.IsExactMatch( MetadataObjectAClone2) );

			// Value mismatch on standard entry
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("DoCompare"), TEXT("DoCompare") );
			MetadataObjectAClone2 = *MetadataObjectA;
			MetadataObjectAClone2.SetStringField( TEXT("DoCompare"), TEXT("DoCompare2") );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectAClone2"),  MetadataObjectAClone.IsExactMatch( MetadataObjectAClone2) );

			// Value and type mismatch on standard entry
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("DoCompare"), TEXT("DoCompare") );
			MetadataObjectAClone2 = *MetadataObjectA;
			MetadataObjectAClone2.SetBoolField( TEXT("DoCompare"), true );
			TestFalse( TEXT("MetadataObjectAClone == MetadataObjectAClone2"),  MetadataObjectAClone.IsExactMatch( MetadataObjectAClone2) );
		}

		// Test less than operator
		{
			
			// Adding standard entry that would appear before other entries
			FLocMetadataObject MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("ANewEntry"), TEXT("NewEntryValue") );
			TestTrue( TEXT("MetadataObjectAClone < MetadataObjectA"), MetadataObjectAClone < *MetadataObjectA );
			
			// Adding standard entry that would appear after other entries
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("ZNewEntry"), TEXT("NewEntryValue") );
			TestTrue( TEXT("MetadataObjectA < MetadataObjectAClone"), *MetadataObjectA < MetadataObjectAClone );

			// Adding non-standard entry that would appear before other entries.  Note metadata with * prefix has no special treatment in less than operator
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("*NewEntry"), TEXT("NewEntryValue") );
			TestTrue( TEXT("MetadataObjectAClone < MetadataObjectA"), MetadataObjectAClone < *MetadataObjectA );

			// Value mismatch on entry with * prefix with same type. Note metadata with * prefix has no special treatment in less than operator
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetStringField( TEXT("*NoCompare"), TEXT("NoCompare") );
			FLocMetadataObject MetadataObjectAClone2 = *MetadataObjectA;
			MetadataObjectAClone2.SetStringField( TEXT("*NoCompare"), TEXT("NoCompare2") );
			TestTrue( TEXT("MetadataObjectAClone < MetadataObjectAClone2"), MetadataObjectAClone < MetadataObjectAClone2 );

			// Value and type mismatch on entry with * prefix.  Note metadata with * prefix has no special treatment in less than operator
			MetadataObjectAClone = *MetadataObjectA;
			MetadataObjectAClone.SetBoolField( TEXT("*NoCompare"), true );
			MetadataObjectAClone2 = *MetadataObjectA;
			MetadataObjectAClone2.SetStringField( TEXT("*NoCompare"), TEXT("NoCompare") );
			TestTrue( TEXT("MetadataObjectAClone < MetadataObjectAClone2"), MetadataObjectAClone < MetadataObjectAClone2 );
		}
	}

	return true;
}

END_FUNCTION_BUILD_OPTIMIZATION

#endif //WITH_DEV_AUTOMATION_TESTS
