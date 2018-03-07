// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/UnrealNames.h"
#include "Misc/AssertionMacros.h"
#include "Misc/MessageDialog.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Misc/CString.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "Misc/ByteSwap.h"
#include "UObject/ObjectVersion.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/ScopeLock.h"
#include "Containers/Set.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemTracker.h"


DEFINE_LOG_CATEGORY_STATIC(LogUnrealNames, Log, All);

/*-----------------------------------------------------------------------------
	FName helpers. 
-----------------------------------------------------------------------------*/
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	void CallNameCreationHook();
#else
	FORCEINLINE void CallNameCreationHook()
	{
	}
#endif

// Get the size of a FNameEntry without the union buffer included
static const int32 NameEntryWithoutUnionSize = sizeof(FNameEntry) - (NAME_SIZE * sizeof(TCHAR));

template<typename TCharType>
FNameEntry* AllocateNameEntry( const TCharType* Name, NAME_INDEX Index);

/**
* Helper function that can be used inside the debuggers watch window. E.g. "DebugFName(Class->Name.Index)". 
*
* @param	Index	Name index to look up string for
* @return			Associated name
*/
const TCHAR* DebugFName(int32 Index)
{
	// Hardcoded static array. This function is only used inside the debugger so it should be fine to return it.
	static TCHAR TempName[256];
	FCString::Strcpy(TempName, *FName::SafeString(Index));
	return TempName;
}

/**
* Helper function that can be used inside the debuggers watch window. E.g. "DebugFName(Class->Name.Index, Class->Name.Number)". 
*
* @param	Index	Name index to look up string for
* @param	Number	Internal instance number of the FName to print (which is 1 more than the printed number)
* @return			Associated name
*/
const TCHAR* DebugFName(int32 Index, int32 Number)
{
	// Hardcoded static array. This function is only used inside the debugger so it should be fine to return it.
	static TCHAR TempName[256];
	FCString::Strcpy(TempName, *FName::SafeString(Index, Number));
	return TempName;
}

/**
 * Helper function that can be used inside the debuggers watch window. E.g. "DebugFName(Class->Name)". 
 *
 * @param	Name	Name to look up string for
 * @return			Associated name
 */
const TCHAR* DebugFName(FName& Name)
{
	// Hardcoded static array. This function is only used inside the debugger so it should be fine to return it.
	static TCHAR TempName[256];
	FCString::Strcpy(TempName, *FName::SafeString(Name.GetDisplayIndex(), Name.GetNumber()));
	return TempName;
}

template <typename TCharType>
static uint16 GetRawCasePreservingHash(const TCharType* Source)
{
	return FCrc::StrCrc32(Source) & 0xFFFF;

}
template <typename TCharType>
static uint16 GetRawNonCasePreservingHash(const TCharType* Source)
{
	return FCrc::Strihash_DEPRECATED(Source) & 0xFFFF;
}

/*-----------------------------------------------------------------------------
	FNameEntry
-----------------------------------------------------------------------------*/


/**
 * @return FString of name portion minus number.
 */
FString FNameEntry::GetPlainNameString() const
{
	if( IsWide() )
	{
		return FString(WideName);
	}
	else
	{
		return FString(AnsiName);
	}
}

/**
 * Appends this name entry to the passed in string.
 *
 * @param	String	String to append this name to
 */
void FNameEntry::AppendNameToString( FString& String ) const
{
	if( IsWide() )
	{
		String += WideName;
	}
	else
	{
		String += AnsiName;
	}
}

void FNameEntry::AppendNameToPathString(FString& String) const
{
	if (IsWide())
	{
		String /= WideName;
	}
	else
	{
		String /= AnsiName;
	}
}

/**
 * @return length of name
 */
int32 FNameEntry::GetNameLength() const
{
	if( IsWide() )
	{
		return FCStringWide::Strlen( WideName );
	}
	else
	{
		return FCStringAnsi::Strlen( AnsiName );
	}
}

/**
 * Compares name using the compare method provided.
 *
 * @param	InName	Name to compare to
 * @return	true if equal, false otherwise
 */
bool FNameEntry::IsEqual( const ANSICHAR* InName, const ENameCase CompareMethod ) const
{
	if( IsWide() )
	{
		// Matching wide-ness means strings are not equal.
		return false;
	}
	else
	{
		return ( (CompareMethod == ENameCase::CaseSensitive) ? FCStringAnsi::Strcmp( AnsiName, InName ) : FCStringAnsi::Stricmp( AnsiName, InName ) ) == 0;
	}
}

/**
 * Compares name using the compare method provided.
 *
 * @param	InName	Name to compare to
 * @return	true if equal, false otherwise
 */
bool FNameEntry::IsEqual( const WIDECHAR* InName, const ENameCase CompareMethod ) const
{
	if( !IsWide() )
	{
		// Matching wide-ness means strings are not equal.
		return false;
	}
	else
	{
		return ( (CompareMethod == ENameCase::CaseSensitive) ? FCStringWide::Strcmp( WideName, InName ) : FCStringWide::Stricmp( WideName, InName ) ) == 0;
	}
}

/**
 * Returns the size in bytes for FNameEntry structure. This is != sizeof(FNameEntry) as we only allocated as needed.
 *
 * @param	Name	Name to determine size for
 * @return	required size of FNameEntry structure to hold this string (might be wide or ansi)
 */
int32 FNameEntry::GetSize( const TCHAR* Name )
{
	return FNameEntry::GetSize( FCString::Strlen( Name ), FCString::IsPureAnsi( Name ) );
}

/**
 * Returns the size in bytes for FNameEntry structure. This is != sizeof(FNameEntry) as we only allocated as needed.
 *
 * @param	Length			Length of name
 * @param	bIsPureAnsi		Whether name is pure ANSI or not
 * @return	required size of FNameEntry structure to hold this string (might be wide or ansi)
 */
int32 FNameEntry::GetSize( int32 Length, bool bIsPureAnsi )
{
	// Add size required for string to the base size used by the FNameEntry
	int32 Size = NameEntryWithoutUnionSize + (Length + 1) * (bIsPureAnsi ? sizeof(ANSICHAR) : sizeof(TCHAR));
	return Size;
}

FNameEntrySerialized::FNameEntrySerialized(const FNameEntry& NameEntry)
{
	if (NameEntry.IsWide())
	{
		PreSetIsWideForSerialization(true);
		FCString::Strcpy(WideName, NAME_SIZE, NameEntry.GetWideName());
		NonCasePreservingHash = GetRawNonCasePreservingHash(NameEntry.GetWideName());
		CasePreservingHash = GetRawCasePreservingHash(NameEntry.GetWideName());
	}
	else
	{
		PreSetIsWideForSerialization(false);
		FCStringAnsi::Strcpy(AnsiName, NAME_SIZE, NameEntry.GetAnsiName());
		NonCasePreservingHash = GetRawNonCasePreservingHash(NameEntry.GetAnsiName());
		CasePreservingHash = GetRawCasePreservingHash(NameEntry.GetAnsiName());
	}
}

/*-----------------------------------------------------------------------------
	FName statics.
-----------------------------------------------------------------------------*/

TNameEntryArray& FName::GetNames()
{
	// NOTE: The reason we initialize to NULL here is due to an issue with static initialization of variables with
	// constructors/destructors across DLL boundaries, where a function called from a statically constructed object
	// calls a function in another module (such as this function) that creates a static variable.  A crash can occur
	// because the static initialization of this DLL has not yet happened, and the CRT's list of static destructors
	// cannot be written to because it has not yet been initialized fully.	(@todo UE4 DLL)
	static TNameEntryArray*	Names = NULL;
	if( Names == NULL )
	{
		check(IsInGameThread());
		Names = new TNameEntryArray();
	}
	return *Names;
}

FNameEntry*** FName::GetNameTableForDebuggerVisualizers_MT()
{
	return GetNames().GetRootBlockForDebuggerVisualizers();
}


FCriticalSection* FName::GetCriticalSection()
{
	static FCriticalSection*	CriticalSection = NULL;
	if( CriticalSection == NULL )
	{
		check(IsInGameThread());
		CriticalSection = new FCriticalSection();
	}
	return CriticalSection;
}

FString FName::NameToDisplayString( const FString& InDisplayName, const bool bIsBool )
{
	// Copy the characters out so that we can modify the string in place
	const TArray< TCHAR >& Chars = InDisplayName.GetCharArray();

	// This is used to indicate that we are in a run of uppercase letter and/or digits.  The code attempts to keep
	// these characters together as breaking them up often looks silly (i.e. "Draw Scale 3 D" as opposed to "Draw Scale 3D"
	bool bInARun = false;
	bool bWasSpace = false;
	bool bWasOpenParen = false;
	FString OutDisplayName;
	OutDisplayName.GetCharArray().Reserve(Chars.Num());
	for( int32 CharIndex = 0 ; CharIndex < Chars.Num() ; ++CharIndex )
	{
		TCHAR ch = Chars[CharIndex];

		bool bLowerCase = FChar::IsLower( ch );
		bool bUpperCase = FChar::IsUpper( ch );
		bool bIsDigit = FChar::IsDigit( ch );
		bool bIsUnderscore = FChar::IsUnderscore( ch );

		// Skip the first character if the property is a bool (they should all start with a lowercase 'b', which we don't want to keep)
		if( CharIndex == 0 && bIsBool && ch == 'b' )
		{
			// Check if next character is uppercase as it may be a user created string that doesn't follow the rules of Unreal variables
			if (Chars.Num() > 1 && FChar::IsUpper(Chars[1]))
			{
				continue;
			}
		}

		// If the current character is upper case or a digit, and the previous character wasn't, then we need to insert a space if there wasn't one previously
		if( (bUpperCase || bIsDigit) && !bInARun && !bWasOpenParen)
		{
			if( !bWasSpace && OutDisplayName.Len() > 0 )
			{
				OutDisplayName += TEXT( ' ' );
				bWasSpace = true;
			}
			bInARun = true;
		}

		// A lower case character will break a run of upper case letters and/or digits
		if( bLowerCase )
		{
			bInARun = false;
		}

		// An underscore denotes a space, so replace it and continue the run
		if( bIsUnderscore )
		{
			ch = TEXT( ' ' );
			bInARun = true;
		}

		// If this is the first character in the string, then it will always be upper-case
		if( OutDisplayName.Len() == 0 )
		{
			ch = FChar::ToUpper( ch );
		}
		else if( bWasSpace || bWasOpenParen)	// If this is first character after a space, then make sure it is case-correct
		{
			// Some words are always forced lowercase
			const TCHAR* Articles[] =
			{
				TEXT( "In" ),
				TEXT( "As" ),
				TEXT( "To" ),
				TEXT( "Or" ),
				TEXT( "At" ),
				TEXT( "On" ),
				TEXT( "If" ),
				TEXT( "Be" ),
				TEXT( "By" ),
				TEXT( "The" ),
				TEXT( "For" ),
				TEXT( "And" ),
				TEXT( "With" ),
				TEXT( "When" ),
				TEXT( "From" ),
			};

			// Search for a word that needs case repaired
			bool bIsArticle = false;
			for( int32 CurArticleIndex = 0; CurArticleIndex < ARRAY_COUNT( Articles ); ++CurArticleIndex )
			{
				// Make sure the character following the string we're testing is not lowercase (we don't want to match "in" with "instance")
				const int32 ArticleLength = FCString::Strlen( Articles[ CurArticleIndex ] );
				if( ( Chars.Num() - CharIndex ) > ArticleLength && !FChar::IsLower( Chars[ CharIndex + ArticleLength ] ) && Chars[ CharIndex + ArticleLength ] != '\0' )
				{
					// Does this match the current article?
					if( FCString::Strncmp( &Chars[ CharIndex ], Articles[ CurArticleIndex ], ArticleLength ) == 0 )
					{
						bIsArticle = true;
						break;
					}
				}
			}

			// Start of a keyword, force to lowercase
			if( bIsArticle )
			{
				ch = FChar::ToLower( ch );				
			}
			else	// First character after a space that's not a reserved keyword, make sure it's uppercase
			{
				ch = FChar::ToUpper( ch );
			}
		}

		bWasSpace = ( ch == TEXT( ' ' ) ? true : false );
		bWasOpenParen = ( ch == TEXT( '(' ) ? true : false );

		OutDisplayName += ch;
	}

	return OutDisplayName;
}


// Static variables.
FNameEntry*	FName::NameHashHead[FNameDefs::NameHashBucketCount];
FNameEntry*	FName::NameHashTail[FNameDefs::NameHashBucketCount];
int32		FName::NameEntryMemorySize;
int32		FName::NumAnsiNames;
int32		FName::NumWideNames;


/*-----------------------------------------------------------------------------
	FName implementation.
-----------------------------------------------------------------------------*/

/**
 * Create an FName. If FindType is FNAME_Find, and the string part of the name 
 * doesn't already exist, then the name will be NAME_None
 *
 * @param Name Value for the string portion of the name
 * @param FindType Action to take (see EFindName)
 */
FName::FName(const WIDECHAR* Name, EFindName FindType)
{
	if (Name)
	{
		Init(Name, NAME_NO_NUMBER_INTERNAL, FindType);
	}
	else
	{
		*this = FName(NAME_None);
	}
}

FName::FName(const ANSICHAR* Name, EFindName FindType)
{
	if (Name)
	{
		Init(Name, NAME_NO_NUMBER_INTERNAL, FindType);
	}
	else
	{
		*this = FName(NAME_None);
	}
}

/**
 * Create an FName. If FindType is FNAME_Find, and the string part of the name 
 * doesn't already exist, then the name will be NAME_None
 *
 * @param Name Value for the string portion of the name
 * @param Number Value for the number portion of the name
 * @param FindType Action to take (see EFindName)
 */
FName::FName( const TCHAR* Name, int32 InNumber, EFindName FindType )
{
	Init(Name, InNumber, FindType);
}

FName::FName(const FNameEntrySerialized& LoadedEntry)
{
	if (LoadedEntry.bWereHashesLoaded)
	{
		// Since the name table can change sizes we need to mask the raw hash to the current size so we don't access out of bounds
		const uint16 NonCasePreservingHash = LoadedEntry.NonCasePreservingHash & (FNameDefs::NameHashBucketCount - 1);
		const uint16 CasePreservingHash = LoadedEntry.CasePreservingHash & (FNameDefs::NameHashBucketCount - 1);
		if (LoadedEntry.IsWide())
		{
			Init(LoadedEntry.GetWideName(), NAME_NO_NUMBER_INTERNAL, FNAME_Add, NonCasePreservingHash, CasePreservingHash);
		}
		else
		{
			Init(LoadedEntry.GetAnsiName(), NAME_NO_NUMBER_INTERNAL, FNAME_Add, NonCasePreservingHash, CasePreservingHash);
		}
	}
	else
	{
		if (LoadedEntry.IsWide())
		{
			Init(LoadedEntry.GetWideName(), NAME_NO_NUMBER_INTERNAL, FNAME_Add, false);
		}
		else
		{
			Init(LoadedEntry.GetAnsiName(), NAME_NO_NUMBER_INTERNAL, FNAME_Add, false);
		}
	}
}

FName::FName( EName HardcodedIndex, const TCHAR* Name )
{
	check(HardcodedIndex >= 0);
	Init(Name, NAME_NO_NUMBER_INTERNAL, FNAME_Add, false, HardcodedIndex);
}

/**
 * Compares name to passed in one. Sort is alphabetical ascending.
 *
 * @param	Other	Name to compare this against
 * @return	< 0 is this < Other, 0 if this == Other, > 0 if this > Other
 */
int32 FName::Compare( const FName& Other ) const
{
	// Names match, check whether numbers match.
	if( GetComparisonIndexFast() == Other.GetComparisonIndexFast() )
	{
		return GetNumber() - Other.GetNumber();
	}
	// Names don't match. This means we don't even need to check numbers.
	else
	{
		TNameEntryArray& Names = GetNames();
		const FNameEntry* const ThisEntry = GetComparisonNameEntry();
		const FNameEntry* const OtherEntry = Other.GetComparisonNameEntry();

		// Ansi/Wide mismatch, convert to wide
		if( ThisEntry->IsWide() != OtherEntry->IsWide() )
		{
			return FCStringWide::Stricmp(	ThisEntry->IsWide() ? ThisEntry->GetWideName() : StringCast<WIDECHAR>(ThisEntry->GetAnsiName()).Get(),
								OtherEntry->IsWide() ? OtherEntry->GetWideName() : StringCast<WIDECHAR>(OtherEntry->GetAnsiName()).Get() );
		}
		// Both are wide.
		else if( ThisEntry->IsWide() )
		{
			return FCStringWide::Stricmp( ThisEntry->GetWideName(), OtherEntry->GetWideName() );
		}
		// Both are ansi.
		else
		{
			return FCStringAnsi::Stricmp( ThisEntry->GetAnsiName(), OtherEntry->GetAnsiName() );
		}		
	}
}

template <typename TCharType>
uint16 FName::GetCasePreservingHash(const TCharType* Source)
{
	return GetRawCasePreservingHash(Source) & (FNameDefs::NameHashBucketCount - 1);
}

template <typename TCharType>
uint16 FName::GetNonCasePreservingHash(const TCharType* Source)
{
	return GetRawNonCasePreservingHash(Source) & (FNameDefs::NameHashBucketCount - 1);
}

void FName::Init(const WIDECHAR* InName, int32 InNumber, EFindName FindType, bool bSplitName, int32 HardcodeIndex)
{
	LLM_SCOPE(ELLMTag::FName);

	const bool bIsPureAnsi = TCString<WIDECHAR>::IsPureAnsi(InName);
	// Switch to ANSI if possible to save memory
	if (bIsPureAnsi)
	{
		InitInternal_HashSplit<ANSICHAR>(StringCast<ANSICHAR>(InName).Get(), InNumber, FindType, bSplitName, HardcodeIndex);
	}
	else
	{
		InitInternal_HashSplit<WIDECHAR>(InName, InNumber, FindType, bSplitName, HardcodeIndex);
	}
}

void FName::Init(const WIDECHAR* InName, int32 InNumber, EFindName FindType, const uint16 NonCasePreservingHash, const uint16 CasePreservingHash)
{
	// Since this comes from the FLinkerLoad we know that it is not pure ansi
	InitInternal<WIDECHAR>(InName, InNumber, FindType, -1, NonCasePreservingHash, CasePreservingHash);
}

void FName::Init(const ANSICHAR* InName, int32 InNumber, EFindName FindType, bool bSplitName, int32 HardcodeIndex)
{
	InitInternal_HashSplit<ANSICHAR>(InName, InNumber, FindType, bSplitName, HardcodeIndex);
}

void FName::Init(const ANSICHAR* InName, int32 InNumber, EFindName FindType, const uint16 NonCasePreservingHash, const uint16 CasePreservingHash)
{
	InitInternal<ANSICHAR>(InName, InNumber, FindType, -1, NonCasePreservingHash, CasePreservingHash);
}

template <typename TCharType>
void FName::InitInternal_HashSplit(const TCharType* InName, int32 InNumber, const EFindName FindType, bool bSplitName, const int32 HardcodeIndex)
{
	TCharType TempBuffer[NAME_SIZE];
	int32 TempNumber;
	// if we were passed in a number, we can't split again, other wise, a_1_2_3_4 would change every time
	// it was loaded in
	if (InNumber == NAME_NO_NUMBER_INTERNAL && bSplitName == true )
	{
		if (SplitNameWithCheckImpl<TCharType>(InName, TempBuffer, ARRAY_COUNT(TempBuffer), TempNumber))
		{
			InName = TempBuffer;
			InNumber = NAME_EXTERNAL_TO_INTERNAL(TempNumber);
		}
	}
	// Hash value of string after splitting
	const uint16 NonCasePreservingHash = GetNonCasePreservingHash(InName);
#if WITH_CASE_PRESERVING_NAME
	const uint16 CasePreservingHash = GetCasePreservingHash(InName);
#else
	const uint16 CasePreservingHash = 0;
#endif
	InitInternal<TCharType>(InName, InNumber, FindType, HardcodeIndex, NonCasePreservingHash, CasePreservingHash);
}

template <typename TCharType>
void FName::InitInternal(const TCharType* InName, int32 InNumber, const EFindName FindType, const int32 HardcodeIndex, const uint16 NonCasePreservingHash, const uint16 CasePreservingHash)
{
	check(TCString<TCharType>::Strlen(InName)<=NAME_SIZE);

	// initialize the name subsystem if necessary
	if (!GetIsInitialized())
	{
		StaticInit();
	}

	check(InName);

	// If empty or invalid name was specified, return NAME_None.
	if( !InName[0] )
	{
		check(HardcodeIndex < 1); // if this is hardcoded, it better be zero 
		ComparisonIndex = NAME_None;
#if WITH_CASE_PRESERVING_NAME
		DisplayIndex = NAME_None;
#endif
		Number = NAME_NO_NUMBER_INTERNAL;
		return;
	}


	//!!!! Caution, since these are set by static initializers from multiple threads, we must use local variables for this stuff until just before we return.

	bool bWasFoundOrAdded = true;
	int32 OutComparisonIndex = HardcodeIndex;
	int32 OutDisplayIndex = HardcodeIndex;

	const bool bIsPureAnsi = TCString<TCharType>::IsPureAnsi(InName);
	if(bIsPureAnsi)
	{
		bWasFoundOrAdded = InitInternal_FindOrAdd<ANSICHAR>(StringCast<ANSICHAR>(InName).Get(), FindType, HardcodeIndex, NonCasePreservingHash, CasePreservingHash, OutComparisonIndex, OutDisplayIndex);
	}
	else
	{
		bWasFoundOrAdded = InitInternal_FindOrAdd<WIDECHAR>(StringCast<WIDECHAR>(InName).Get(), FindType, HardcodeIndex, NonCasePreservingHash, CasePreservingHash, OutComparisonIndex, OutDisplayIndex);
	}

	if(bWasFoundOrAdded)
	{
		ComparisonIndex = OutComparisonIndex;
#if WITH_CASE_PRESERVING_NAME
		DisplayIndex = OutDisplayIndex;
#endif
		Number = InNumber;
	}
	else
	{
		ComparisonIndex = NAME_None;
#if WITH_CASE_PRESERVING_NAME
		DisplayIndex = NAME_None;
#endif
		Number = NAME_NO_NUMBER_INTERNAL;
	}
}

template <typename TCharType> void IncrementNameCount();
template <> void IncrementNameCount<ANSICHAR>()
{
	FName::NumAnsiNames++;
}
template <> void IncrementNameCount<WIDECHAR>()
{
	FName::NumWideNames++;
}

template <typename TCharType>
struct FNameInitHelper
{
};

template <>
struct FNameInitHelper<ANSICHAR>
{
	static const bool IsAnsi = true;

	static const ANSICHAR* GetNameString(const FNameEntry* const NameEntry)
	{
		return NameEntry->GetAnsiName();
	}

	static void SetNameString(FNameEntry* const DestNameEntry, const ANSICHAR* SrcName)
	{
		// Can't rely on the template override for static arrays since the safe crt version of strcpy will fill in
		// the remainder of the array of NAME_SIZE with 0xfd.  So, we have to pass in the length of the dynamically allocated array instead.
		FCStringAnsi::Strcpy(const_cast<ANSICHAR*>(DestNameEntry->GetAnsiName()), DestNameEntry->GetNameLength()+1, SrcName);
	}

	static void SetNameString(FNameEntry* const DestNameEntry, const ANSICHAR* SrcName, const int32 NameLength)
	{
		// Can't rely on the template override for static arrays since the safe crt version of strcpy will fill in
		// the remainder of the array of NAME_SIZE with 0xfd.  So, we have to pass in the length of the dynamically allocated array instead.
		FCStringAnsi::Strcpy(const_cast<ANSICHAR*>(DestNameEntry->GetAnsiName()), NameLength + 1, SrcName);
	}

	static int32 GetIndexShiftValue()
	{
		return 0;
	}

	static int32 GetSize(int32 Length)
	{
		// Add size required for string to the base size used by the FNameEntry
		int32 Size = NameEntryWithoutUnionSize + (Length + 1) * sizeof(ANSICHAR);
		return Size;
	}
};

template <>
struct FNameInitHelper<WIDECHAR>
{
	static const bool IsAnsi = false;

	static const WIDECHAR* GetNameString(const FNameEntry* const NameEntry)
	{
		return NameEntry->GetWideName();
	}

	static void SetNameString(FNameEntry* const DestNameEntry, const WIDECHAR* SrcName)
	{
		// Can't rely on the template override for static arrays since the safe crt version of strcpy will fill in
		// the remainder of the array of NAME_SIZE with 0xfd.  So, we have to pass in the length of the dynamically allocated array instead.
		FCStringWide::Strcpy(const_cast<WIDECHAR*>(DestNameEntry->GetWideName()), DestNameEntry->GetNameLength()+1, SrcName);
	}

	static void SetNameString(FNameEntry* const DestNameEntry, const WIDECHAR* SrcName, const int32 NameLength)
	{
		// Can't rely on the template override for static arrays since the safe crt version of strcpy will fill in
		// the remainder of the array of NAME_SIZE with 0xfd.  So, we have to pass in the length of the dynamically allocated array instead.
		FCStringWide::Strcpy(const_cast<WIDECHAR*>(DestNameEntry->GetWideName()), NameLength + 1, SrcName);
	}

	static int32 GetIndexShiftValue()
	{
		return 1;
	}

	static int32 GetSize(int32 Length)
	{
		// Add size required for string to the base size used by the FNameEntry
		int32 Size = NameEntryWithoutUnionSize + (Length + 1) * sizeof(TCHAR);
		return Size;
	}
};

template <typename TCharType>
bool FName::InitInternal_FindOrAdd(const TCharType* InName, const EFindName FindType, const int32 HardcodeIndex, const uint16 NonCasePreservingHash, const uint16 CasePreservingHash, int32& OutComparisonIndex, int32& OutDisplayIndex)
{
	const bool bWasFoundOrAdded = InitInternal_FindOrAddNameEntry<TCharType>(InName, FindType, ENameCase::IgnoreCase, NonCasePreservingHash, OutComparisonIndex);
	
#if WITH_CASE_PRESERVING_NAME
	if(bWasFoundOrAdded && HardcodeIndex < 0)
	{
		TNameEntryArray& Names = GetNames();
		const FNameEntry* const NameEntry = Names[OutComparisonIndex];

		// If the string we got back doesn't match the case of the string we provided, also add a case variant version for display purposes
		if(TCString<TCharType>::Strcmp(InName, FNameInitHelper<TCharType>::GetNameString(NameEntry)) != 0)
		{
			if(!InitInternal_FindOrAddNameEntry<TCharType>(InName, FindType, ENameCase::CaseSensitive, CasePreservingHash, OutDisplayIndex))
			{
				// We don't consider failing to find/add the case variant a full failure
				OutDisplayIndex = OutComparisonIndex;
			}
		}
		else
		{
			OutDisplayIndex = OutComparisonIndex;
		}
	}
	else
#endif
	{
		OutDisplayIndex = OutComparisonIndex;
	}

	return bWasFoundOrAdded;
}

template <typename TCharType>
bool FName::InitInternal_FindOrAddNameEntry(const TCharType* InName, const EFindName FindType, const ENameCase ComparisonMode, const uint16 iHash, int32& OutIndex)
{
	CallNameCreationHook();
	if (OutIndex < 0)
	{
		// Try to find the name in the hash.
		for( FNameEntry* Hash=NameHashHead[iHash]; Hash; Hash=Hash->HashNext )
		{
			FPlatformMisc::Prefetch( Hash->HashNext );
			// Compare the passed in string
			if( Hash->IsEqual( InName, ComparisonMode ) )
			{
				// Found it in the hash.
				OutIndex = Hash->GetIndex();

				// Check to see if the caller wants to replace the contents of the
				// FName with the specified value. This is useful for compiling
				// script classes where the file name is lower case but the class
				// was intended to be uppercase.
				if (FindType == FNAME_Replace_Not_Safe_For_Threading)
				{
					check(IsInGameThread());

					// This *must* be true, or we'll overwrite memory when the
					// copy happens if it is longer
					check(TCString<TCharType>::Strlen(InName) == Hash->GetNameLength());

					FNameInitHelper<TCharType>::SetNameString(Hash, InName);
				}
				check(OutIndex >= 0);
				return true;
			}
		}

		// Didn't find name.
		if( FindType==FNAME_Find )
		{
			// Not found.
			return false;
		}
	}
	// acquire the lock
	FScopeLock ScopeLock(GetCriticalSection());
	if (OutIndex < 0)
	{
		// Try to find the name in the hash. AGAIN...we might have been adding from a different thread and we just missed it
		for( FNameEntry* Hash=NameHashHead[iHash]; Hash; Hash=Hash->HashNext )
		{
			// Compare the passed in string
			if( Hash->IsEqual( InName, ComparisonMode ) )
			{
				// Found it in the hash.
				OutIndex = Hash->GetIndex();
				check(FindType == FNAME_Add);  // if this was a replace, well it isn't safe for threading. Find should have already been handled
				return true;
			}
		}
	}
	FNameEntry* OldHashHead=NameHashHead[iHash];
	FNameEntry* OldHashTail=NameHashTail[iHash];
	TNameEntryArray& Names = GetNames();
	if (OutIndex < 0)
	{
		OutIndex = Names.AddZeroed(1);
	}
	else
	{
		check(OutIndex < Names.Num());
	}
	FNameEntry* NewEntry = AllocateNameEntry(InName, OutIndex);
	if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Names[OutIndex], NewEntry, nullptr) != nullptr) // we use an atomic operation to check for unexpected concurrency, verify alignment, etc
	{
		UE_LOG(LogUnrealNames, Fatal, TEXT("Hardcoded name '%s' at index %i was duplicated (or unexpected concurrency). Existing entry is '%s'."), *NewEntry->GetPlainNameString(), NewEntry->GetIndex(), *Names[OutIndex]->GetPlainNameString() );
	}
	if (!OldHashHead)
	{
		checkSlow(!OldHashTail);

		// atomically assign the new head as other threads may be reading it
		if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&NameHashHead[iHash], NewEntry, OldHashHead) != OldHashHead) // we use an atomic operation to check for unexpected concurrency, verify alignment, etc
		{
			check(0); // someone changed this while we were changing it
		}
		NameHashTail[iHash] = NewEntry; // We can non-atomically assign the tail since it's only ever read while locked
	}
	else
	{
		checkSlow(OldHashTail);

		// atomically update the linked list as other threads may be reading it
		if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&OldHashTail->HashNext, NewEntry, nullptr) != nullptr) // we use an atomic operation to check for unexpected concurrency, verify alignment, etc
		{
			check(0); // someone changed this while we were changing it
		}
		NameHashTail[iHash] = NewEntry; // We can non-atomically assign the tail since it's only ever read while locked
	}
	check(OutIndex >= 0);
	return true;
}

const FNameEntry* FName::GetComparisonNameEntry() const
{
	TNameEntryArray& Names = GetNames();
	const NAME_INDEX Index = GetComparisonIndex();
	return Names[Index];
}

const FNameEntry* FName::GetDisplayNameEntry() const
{
	TNameEntryArray& Names = GetNames();
	const NAME_INDEX Index = GetDisplayIndex();
	return Names[Index];
}

FString FName::ToString() const
{
	if (GetNumber() == NAME_NO_NUMBER_INTERNAL)
	{
		// Avoids some extra allocations in non-number case
		return GetDisplayNameEntry()->GetPlainNameString();
	}
	
	FString Out;	
	ToString(Out);
	return Out;
}

void FName::ToString(FString& Out) const
{
	// A version of ToString that saves at least one string copy
	const FNameEntry* const NameEntry = GetDisplayNameEntry();
	if (GetNumber() == NAME_NO_NUMBER_INTERNAL)
	{
		Out.Empty(NameEntry->GetNameLength());
		NameEntry->AppendNameToString(Out);
	}	
	else
	{
		Out.Empty(NameEntry->GetNameLength() + 6);
		NameEntry->AppendNameToString(Out);

		Out += TEXT("_");
		Out.AppendInt(NAME_INTERNAL_TO_EXTERNAL(GetNumber()));
	}
}

void FName::AppendString(FString& Out) const
{
	const FNameEntry* const NameEntry = GetDisplayNameEntry();
	NameEntry->AppendNameToString( Out );
	if (GetNumber() != NAME_NO_NUMBER_INTERNAL)
	{
		Out += TEXT("_");
		Out.AppendInt(NAME_INTERNAL_TO_EXTERNAL(GetNumber()));
	}
}


/*-----------------------------------------------------------------------------
	FName subsystem.
-----------------------------------------------------------------------------*/

void FName::StaticInit()
{
	check(IsInGameThread());
	/** Global instance used to initialize the GCRCTable. It used to be initialized in appInit() */
	//@todo: Massive workaround for static init order without needing to use a function call for every use of GCRCTable
	// This ASSUMES that fname::StaticINit is going to be called BEFORE ANY use of GCRCTable
	FCrc::Init();

	check(GetIsInitialized() == false);
	check((FNameDefs::NameHashBucketCount&(FNameDefs::NameHashBucketCount-1)) == 0);
	GetIsInitialized() = 1;

	// Init the name hash.
	for (int32 HashIndex = 0; HashIndex < FNameDefs::NameHashBucketCount; HashIndex++)
	{
		NameHashHead[HashIndex] = nullptr;
		NameHashTail[HashIndex] = nullptr;
	}

	{
		FScopeLock ScopeLock(GetCriticalSection());

		TNameEntryArray& Names = GetNames();
		Names.AddZeroed(NAME_MaxHardcodedNameIndex + 1);
	}

	{
		// Register all hardcoded names.
		#define REGISTER_NAME(num,namestr) FName Temp_##namestr(EName(num), TEXT(#namestr));
		#include "UObject/UnrealNames.inl"
		#undef REGISTER_NAME
	}

#if DO_CHECK
	// Verify no duplicate names.
	for (int32 HashIndex = 0; HashIndex < FNameDefs::NameHashBucketCount; HashIndex++)
	{
		for (FNameEntry* Hash = NameHashHead[HashIndex]; Hash; Hash = Hash->HashNext)
		{
			for (FNameEntry* Other = Hash->HashNext; Other; Other = Other->HashNext)
			{
				if (FCString::Stricmp(*Hash->GetPlainNameString(), *Other->GetPlainNameString()) == 0)
				{
					// we can't print out here because there may be no log yet if this happens before main starts
					if (FPlatformMisc::IsDebuggerPresent())
					{
						FPlatformMisc::DebugBreak();
					}
					else
					{
						FPlatformMisc::PromptForRemoteDebugging(false);
						FMessageDialog::Open(EAppMsgType::Ok, FText::Format( NSLOCTEXT("UnrealEd", "DuplicatedHardcodedName", "Duplicate hardcoded name: {0}"), FText::FromString( Hash->GetPlainNameString() ) ) );
						FPlatformMisc::RequestExit(false);
					}
				}
			}
		}
	}
	// check that the MAX_NETWORKED_HARDCODED_NAME define is correctly set
	if (GetMaxNames() <= MAX_NETWORKED_HARDCODED_NAME)
	{
		if (FPlatformMisc::IsDebuggerPresent())
		{
			FPlatformMisc::DebugBreak();
		}
		else
		{
			FPlatformMisc::PromptForRemoteDebugging(false);
			// can't use normal check()/UE_LOG(LogUnrealNames, Fatal,) here
			FMessageDialog::Open( EAppMsgType::Ok, FText::Format( NSLOCTEXT("UnrealEd", "MAX_NETWORKED_HARDCODED_NAME Incorrect", "MAX_NETWORKED_HARDCODED_NAME is incorrectly set! (Currently {0}, must be no greater than {1}"), FText::AsNumber( MAX_NETWORKED_HARDCODED_NAME ), FText::AsNumber( GetMaxNames() - 1 ) ) );
			FPlatformMisc::RequestExit(false);
		}
	}
#endif
}

bool& FName::GetIsInitialized()
{
	static bool bIsInitialized = false;
	return bIsInitialized;
}

void FName::DisplayHash( FOutputDevice& Ar )
{
	int32 UsedBins=0, NameCount=0, MemUsed = 0;
	for( int32 i=0; i<FNameDefs::NameHashBucketCount; i++ )
	{
		if( NameHashHead[i] != NULL ) UsedBins++;
		for( FNameEntry *Hash = NameHashHead[i]; Hash; Hash=Hash->HashNext )
		{
			NameCount++;
			// Count how much memory this entry is using
			MemUsed += FNameEntry::GetSize( Hash->GetNameLength(), Hash->IsWide() );
		}
	}
	Ar.Logf( TEXT("Hash: %i names, %i/%i hash bins, Mem in bytes %i"), NameCount, UsedBins, FNameDefs::NameHashBucketCount, MemUsed);
}

bool FName::SplitNameWithCheck(const WIDECHAR* OldName, WIDECHAR* NewName, int32 NewNameLen, int32& NewNumber)
{
	return SplitNameWithCheckImpl<WIDECHAR>(OldName, NewName, NewNameLen, NewNumber);
}

template <typename TCharType>
bool FName::SplitNameWithCheckImpl(const TCharType* OldName, TCharType* NewName, int32 NewNameLen, int32& NewNumber)
{
	bool bSucceeded = false;
	const int32 OldNameLength = TCString<TCharType>::Strlen(OldName);

	if(OldNameLength > 0)
	{
		// get string length
		const TCharType* LastChar = OldName + (OldNameLength - 1);
		
		// if the last char is a number, then we will try to split
		const TCharType* Ch = LastChar;
		if (*Ch >= '0' && *Ch <= '9')
		{
			// go backwards, looking an underscore or the start of the string
			// (we don't look at first char because '_9' won't split well)
			while (*Ch >= '0' && *Ch <= '9' && Ch > OldName)
			{
				Ch--;
			}

			// if the first non-number was an underscore (as opposed to a letter),
			// we can split
			if (*Ch == '_')
			{
				// check for the case where there are multiple digits after the _ and the first one
				// is a 0 ("Rocket_04"). Can't split this case. (So, we check if the first char
				// is not 0 or the length of the number is 1 (since ROcket_0 is valid)
				if (Ch[1] != '0' || LastChar - Ch == 1)
				{
					// attempt to convert what's following it to a number
					uint64 TempConvert = TCString<TCharType>::Atoi64(Ch + 1);
					if (TempConvert <= MAX_int32)
					{
						NewNumber = (int32)TempConvert;
						// copy the name portion into the buffer
						TCString<TCharType>::Strncpy(NewName, OldName, FMath::Min<int32>(Ch - OldName + 1, NewNameLen));

						// mark successful
						bSucceeded = true;
					}
				}
			}
		}
	}

	// return success code
	return bSucceeded;
}

bool FName::IsValidXName(const FString& InName, const FString& InInvalidChars, FText* OutReason, const FText* InErrorCtx)
{
	if (InName.IsEmpty() || InInvalidChars.IsEmpty())
	{
		return true;
	}

	// See if the name contains invalid characters.
	FString MatchedInvalidChars;
	TSet<TCHAR> AlreadyMatchedInvalidChars;
	for (const TCHAR InvalidChar : InInvalidChars)
	{
		if (!AlreadyMatchedInvalidChars.Contains(InvalidChar) && InName.GetCharArray().Contains(InvalidChar))
		{
			MatchedInvalidChars.AppendChar(InvalidChar);
			AlreadyMatchedInvalidChars.Add(InvalidChar);
		}
	}

	if (MatchedInvalidChars.Len())
	{
		if (OutReason)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ErrorCtx"), (InErrorCtx) ? *InErrorCtx : NSLOCTEXT("Core", "NameDefaultErrorCtx", "Name"));
			Args.Add(TEXT("IllegalNameCharacters"), FText::FromString(MatchedInvalidChars));
			*OutReason = FText::Format(NSLOCTEXT("Core", "NameContainsInvalidCharacters", "{ErrorCtx} may not contain the following characters: {IllegalNameCharacters}"), Args);
		}
		return false;
	}

	return true;
}

void FName::AutoTest()
{
	const FName AutoTest_1("AutoTest_1");
	const FName autoTest_1("autoTest_1");
	const FName autoTeSt_1("autoTeSt_1");
	const FName AutoTest1Find("autoTEST_1", EFindName::FNAME_Find);
	const FName AutoTest_2(TEXT("AutoTest_2"));
	const FName AutoTestB_2(TEXT("AutoTestB_2"));
	const FName NullName(static_cast<ANSICHAR*>(nullptr));

	check(AutoTest_1 != AutoTest_2);
	check(AutoTest_1 == autoTest_1);
	check(AutoTest_1 == autoTeSt_1);
#if WITH_CASE_PRESERVING_NAME
	check(!FCString::Strcmp(*AutoTest_1.ToString(), TEXT("AutoTest_1")));
	check(!FCString::Strcmp(*autoTest_1.ToString(), TEXT("autoTest_1")));
	check(!FCString::Strcmp(*autoTeSt_1.ToString(), TEXT("autoTeSt_1")));
	check(!FCString::Strcmp(*AutoTestB_2.ToString(), TEXT("AutoTestB_2")));
#endif
	check(autoTest_1.GetComparisonIndex() == AutoTest_2.GetComparisonIndex());
	check(autoTest_1.GetPlainNameString() == AutoTest_1.GetPlainNameString());
	check(autoTest_1.GetPlainNameString() == AutoTest_2.GetPlainNameString());
	check(*AutoTestB_2.GetPlainNameString() != *AutoTest_2.GetPlainNameString());
	check(AutoTestB_2.GetNumber() == AutoTest_2.GetNumber());
	check(autoTest_1.GetNumber() != AutoTest_2.GetNumber());
	check(NullName.IsNone());
}

/*-----------------------------------------------------------------------------
	FNameEntry implementation.
-----------------------------------------------------------------------------*/

FArchive& operator<<( FArchive& Ar, FNameEntry& E )
{
	if( Ar.IsLoading() )
	{
		// for optimization reasons, we want to keep pure Ansi strings as Ansi for initializing the name entry
		// (and later the FName) to stop copying in and out of TCHARs
		int32 StringLen;
		Ar << StringLen;

		// negative stringlen means it's a wide string
		if (StringLen < 0)
		{
			StringLen = -StringLen;

			// mark the name will be wide
			E.PreSetIsWideForSerialization(true);

			// get the pointer to the wide array 
			WIDECHAR* WideName = const_cast<WIDECHAR*>(E.GetWideName());

			// read in the UCS2CHAR string and byteswap it, etc
			auto Sink = StringMemoryPassthru<UCS2CHAR>(WideName, StringLen, StringLen);
			Ar.Serialize(Sink.Get(), StringLen * sizeof(UCS2CHAR));
			Sink.Apply();

			INTEL_ORDER_TCHARARRAY(WideName)
		}
		else
		{
			// mark the name will be ansi
			E.PreSetIsWideForSerialization(false);

			// ansi strings can go right into the AnsiBuffer
			ANSICHAR* AnsiName = const_cast<ANSICHAR*>(E.GetAnsiName());
			Ar.Serialize(AnsiName, StringLen);
		}
	}
	else
	{
		// Convert to our serialized type
		FNameEntrySerialized EntrySerialized(E);
		Ar << EntrySerialized;
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FNameEntrySerialized& E)
{
	if (Ar.IsLoading())
	{
		// for optimization reasons, we want to keep pure Ansi strings as Ansi for initializing the name entry
		// (and later the FName) to stop copying in and out of TCHARs
		int32 StringLen;
		Ar << StringLen;

		// negative stringlen means it's a wide string
		if (StringLen < 0)
		{
			StringLen = -StringLen;

			// mark the name will be wide
			E.PreSetIsWideForSerialization(true);

			// get the pointer to the wide array 
			WIDECHAR* WideName = const_cast<WIDECHAR*>(E.GetWideName());

			// read in the UCS2CHAR string and byteswap it, etc
			auto Sink = StringMemoryPassthru<UCS2CHAR>(WideName, StringLen, StringLen);
			Ar.Serialize(Sink.Get(), StringLen * sizeof(UCS2CHAR));
			Sink.Apply();

			INTEL_ORDER_TCHARARRAY(WideName)
		}
		else
		{
			// mark the name will be ansi
			E.PreSetIsWideForSerialization(false);

			// ansi strings can go right into the AnsiBuffer
			ANSICHAR* AnsiName = const_cast<ANSICHAR*>(E.GetAnsiName());
			Ar.Serialize(AnsiName, StringLen);
		}
		if (Ar.UE4Ver() >= VER_UE4_NAME_HASHES_SERIALIZED)
		{
			// Read the save time calculated hashes to save load time perf
			Ar << E.NonCasePreservingHash;
			Ar << E.CasePreservingHash;
			E.bWereHashesLoaded = true;
		}
	}
	else
	{
		FString Str = E.GetPlainNameString();
		Ar << Str;
		Ar << E.NonCasePreservingHash;
		Ar << E.CasePreservingHash;
	}

	return Ar;
}

/**
 * Pooled allocator for FNameEntry structures. Doesn't have to worry about freeing memory as those
 * never go away. It simply uses 64K chunks and allocates new ones as space runs out. This reduces
 * allocation overhead significantly (only minor waste on 64k boundaries) and also greatly helps
 * with fragmentation as 50-100k allocations turn into tens of allocations.
 */
class FNameEntryPoolAllocator
{
public:
	/** Initializes all member variables. */
	FNameEntryPoolAllocator()
	{
		TotalAllocatedPages	= 0;
		CurrentPoolStart	= NULL;
		CurrentPoolEnd		= NULL;
		check(ThreadGuard.GetValue() == 0);
	}

	/**
	 * Allocates the requested amount of bytes and casts them to a FNameEntry pointer.
	 *
	 * @param   Size  Size in bytes to allocate
	 * @return  Allocation of passed in size cast to a FNameEntry pointer.
	 */
	FNameEntry* Allocate( int32 Size )
	{
		check(ThreadGuard.Increment() == 1);
		// Some platforms need all of the name entries to be aligned to 4 bytes, so by
		// aligning the size here the next allocation will be aligned to 4
		Size = Align( Size, alignof(FNameEntry) );

		// Allocate a new pool if current one is exhausted. We don't worry about a little bit
		// of waste at the end given the relative size of pool to average and max allocation.
		if( CurrentPoolEnd - CurrentPoolStart < Size )
		{
			AllocateNewPool();
		}
		check( CurrentPoolEnd - CurrentPoolStart >= Size );
		// Return current pool start as allocation and increment by size.
		FNameEntry* NameEntry = (FNameEntry*) CurrentPoolStart;
		CurrentPoolStart += Size;
		check(ThreadGuard.Decrement() == 0);
		return NameEntry;
	}

	/**
	 * Returns the amount of memory to allocate for each page pool.
	 *
	 * @return  Page pool size.
	 */
	FORCEINLINE int32 PoolSize()
	{
		// Allocate in 64k chunks as it's ideal for page size.
		return 256 * 1024;
	}

	/**
	 * Returns the number of pages that have been allocated so far for names.
	 *
	 * @return  The number of pages allocated.
	 */
	FORCEINLINE int32 PageCount()
	{
		return TotalAllocatedPages;
	}

private:
	/** Allocates a new pool. */
	void AllocateNewPool()
	{
		TotalAllocatedPages++;
		CurrentPoolStart = (uint8*) FMemory::Malloc(PoolSize());
		CurrentPoolEnd = CurrentPoolStart + PoolSize();
	}

	/** Beginning of pool. Allocated by AllocateNewPool, incremented by Allocate.	*/
	uint8* CurrentPoolStart;
	/** End of current pool. Set by AllocateNewPool and checked by Allocate.		*/
	uint8* CurrentPoolEnd;
	/** Total number of pages that have been allocated.								*/
	int32 TotalAllocatedPages;
	/** Threadsafe counter to test for unwanted concurrency							*/
	FThreadSafeCounter ThreadGuard;
};

/** Global allocator for name entries. */
FNameEntryPoolAllocator GNameEntryPoolAllocator;

template<typename TCharType>
FNameEntry* AllocateNameEntry(const TCharType* Name, NAME_INDEX Index)
{
	LLM_SCOPE(ELLMTag::FName);

	const SIZE_T NameLen  = TCString<TCharType>::Strlen(Name);
	int32 NameEntrySize	  = FNameInitHelper<TCharType>::GetSize( NameLen );
	FNameEntry* NameEntry = GNameEntryPoolAllocator.Allocate( NameEntrySize );
	FName::NameEntryMemorySize += NameEntrySize;
	FPlatformAtomics::InterlockedExchange(&NameEntry->Index, (Index << NAME_INDEX_SHIFT) | (FNameInitHelper<TCharType>::GetIndexShiftValue()));
	FPlatformAtomics::InterlockedExchangePtr((void**)&NameEntry->HashNext, nullptr);
	FNameInitHelper<TCharType>::SetNameString(NameEntry, Name, NameLen);
	IncrementNameCount<TCharType>();
	return NameEntry;
}


#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "Containers/StackTracker.h"
static TAutoConsoleVariable<int32> CVarLogGameThreadFNameChurn(
	TEXT("LogGameThreadFNameChurn.Enable"),
	0,
	TEXT("If > 0, then collect sample game thread fname create, periodically print a report of the worst offenders."));

static TAutoConsoleVariable<int32> CVarLogGameThreadFNameChurn_PrintFrequency(
	TEXT("LogGameThreadFNameChurn.PrintFrequency"),
	300,
	TEXT("Number of frames between churn reports."));

static TAutoConsoleVariable<int32> CVarLogGameThreadFNameChurn_Threshhold(
	TEXT("LogGameThreadFNameChurn.Threshhold"),
	10,
	TEXT("Minimum average number of fname creations per frame to include in the report."));

static TAutoConsoleVariable<int32> CVarLogGameThreadFNameChurn_SampleFrequency(
	TEXT("LogGameThreadFNameChurn.SampleFrequency"),
	1,
	TEXT("Number of fname creates per sample. This is used to prevent churn sampling from slowing the game down too much."));

static TAutoConsoleVariable<int32> CVarLogGameThreadFNameChurn_StackIgnore(
	TEXT("LogGameThreadFNameChurn.StackIgnore"),
	4,
	TEXT("Number of items to discard from the top of a stack frame."));

static TAutoConsoleVariable<int32> CVarLogGameThreadFNameChurn_RemoveAliases(
	TEXT("LogGameThreadFNameChurn.RemoveAliases"),
	1,
	TEXT("If > 0 then remove aliases from the counting process. This essentialy merges addresses that have the same human readable string. It is slower."));

static TAutoConsoleVariable<int32> CVarLogGameThreadFNameChurn_StackLen(
	TEXT("LogGameThreadFNameChurn.StackLen"),
	3,
	TEXT("Maximum number of stack frame items to keep. This improves aggregation because calls that originate from multiple places but end up in the same place will be accounted together."));


struct FSampleFNameChurn
{
	FStackTracker GGameThreadFNameChurnTracker;
	bool bEnabled;
	int32 CountDown;
	uint64 DumpFrame;

	FSampleFNameChurn()
		: bEnabled(false)
		, CountDown(MAX_int32)
		, DumpFrame(0)
	{
	}

	void NameCreationHook()
	{
		bool bNewEnabled = CVarLogGameThreadFNameChurn.GetValueOnGameThread() > 0;
		if (bNewEnabled != bEnabled)
		{
			check(IsInGameThread());
			bEnabled = bNewEnabled;
			if (bEnabled)
			{
				CountDown = CVarLogGameThreadFNameChurn_SampleFrequency.GetValueOnGameThread();
				DumpFrame = GFrameCounter + CVarLogGameThreadFNameChurn_PrintFrequency.GetValueOnGameThread();
				GGameThreadFNameChurnTracker.ResetTracking();
				GGameThreadFNameChurnTracker.ToggleTracking(true, true);
			}
			else
			{
				GGameThreadFNameChurnTracker.ToggleTracking(false, true);
				DumpFrame = 0;
				GGameThreadFNameChurnTracker.ResetTracking();
			}
		}
		else if (bEnabled)
		{
			check(IsInGameThread());
			check(DumpFrame);
			if (--CountDown <= 0)
			{
				CountDown = CVarLogGameThreadFNameChurn_SampleFrequency.GetValueOnGameThread();
				CollectSample();
				if (GFrameCounter > DumpFrame)
				{
					PrintResultsAndReset();
				}
			}
		}
	}

	void CollectSample()
	{
		check(IsInGameThread());
		GGameThreadFNameChurnTracker.CaptureStackTrace(CVarLogGameThreadFNameChurn_StackIgnore.GetValueOnGameThread(), nullptr, CVarLogGameThreadFNameChurn_StackLen.GetValueOnGameThread(), CVarLogGameThreadFNameChurn_RemoveAliases.GetValueOnGameThread() > 0);
	}
	void PrintResultsAndReset()
	{
		DumpFrame = GFrameCounter + CVarLogGameThreadFNameChurn_PrintFrequency.GetValueOnGameThread();
		FOutputDeviceRedirector* Log = FOutputDeviceRedirector::Get();
		float SampleAndFrameCorrection = float(CVarLogGameThreadFNameChurn_SampleFrequency.GetValueOnGameThread()) / float(CVarLogGameThreadFNameChurn_PrintFrequency.GetValueOnGameThread());
		GGameThreadFNameChurnTracker.DumpStackTraces(CVarLogGameThreadFNameChurn_Threshhold.GetValueOnGameThread(), *Log, SampleAndFrameCorrection);
		GGameThreadFNameChurnTracker.ResetTracking();
	}
};

FSampleFNameChurn GGameThreadFNameChurnTracker;

void CallNameCreationHook()
{
	if (GIsRunning && IsInGameThread())
	{
		GGameThreadFNameChurnTracker.NameCreationHook();
	}
}

#endif

