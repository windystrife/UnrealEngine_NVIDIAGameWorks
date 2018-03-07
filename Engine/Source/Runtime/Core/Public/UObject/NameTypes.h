// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "Containers/StringConv.h"
#include "UObject/UnrealNames.h"

/*----------------------------------------------------------------------------
	Definitions.
----------------------------------------------------------------------------*/

/** 
 * Do we want to support case-variants for FName?
 * This will add an extra NAME_INDEX variable to FName, but means that ToString() will return you the exact same 
 * string that FName::Init was called with (which is useful if your FNames are shown to the end user)
 * Currently this is enabled for the Editor and any Programs (such as UHT), but not the Runtime
 */
#ifndef WITH_CASE_PRESERVING_NAME
	#define WITH_CASE_PRESERVING_NAME WITH_EDITORONLY_DATA
#endif

class FText;

/** Maximum size of name. */
enum {NAME_SIZE	= 1024};

/** Name index. */
typedef int32 NAME_INDEX;

#define checkName checkSlow

/** Externally, the instance number to represent no instance number is NAME_NO_NUMBER, 
    but internally, we add 1 to indices, so we use this #define internally for 
	zero'd memory initialization will still make NAME_None as expected */
#define NAME_NO_NUMBER_INTERNAL	0

/** Conversion routines between external representations and internal */
#define NAME_INTERNAL_TO_EXTERNAL(x) (x - 1)
#define NAME_EXTERNAL_TO_INTERNAL(x) (x + 1)

/** Special value for an FName with no number */
#define NAME_NO_NUMBER NAME_INTERNAL_TO_EXTERNAL(NAME_NO_NUMBER_INTERNAL)


/** this is the character used to separate a subobject root from its subobjects in a path name. */
#define SUBOBJECT_DELIMITER				TEXT(":")

/** this is the character used to separate a subobject root from its subobjects in a path name, as a char */
#define SUBOBJECT_DELIMITER_CHAR		':'

/** These are the characters that cannot be used in general FNames */
#define INVALID_NAME_CHARACTERS			TEXT("\"' ,\n\r\t")

/** These characters cannot be used in object names */
#define INVALID_OBJECTNAME_CHARACTERS	TEXT("\"' ,/.:|&!~\n\r\t@#(){}[]=;^%$`")

/** These characters cannot be used in ObjectPaths, which includes both the package path and part after the first . */
#define INVALID_OBJECTPATH_CHARACTERS	TEXT("\"' ,|&!~\n\r\t@#(){}[]=;^%$`")

/** These characters cannot be used in long package names */
#define INVALID_LONGPACKAGE_CHARACTERS	TEXT("\\:*?\"<>|' ,.&!~\n\r\t@#")

/** These characters can be used in relative directory names (lowercase versions as well) */
#define VALID_SAVEDDIRSUFFIX_CHARACTERS	TEXT("_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")

enum class ENameCase : uint8
{
	CaseSensitive,
	IgnoreCase,
};

namespace FNameDefs
{
#if !WITH_EDITORONLY_DATA
	// Use a modest bucket count on consoles
	static const uint32 NameHashBucketCount = 65536;
#else
	// On PC platform we use a large number of name hash buckets to accommodate the editor's
	// use of FNames to store asset path and content tags
	static const uint32 NameHashBucketCount = 65536;
#endif
}

enum ELinkerNameTableConstructor    {ENAME_LinkerConstructor};

/** Enumeration for finding name. */
enum EFindName
{
	/** Find a name; return 0 if it doesn't exist. */
	FNAME_Find,

	/** Find a name or add it if it doesn't exist. */
	FNAME_Add,

	/** Finds a name and replaces it. Adds it if missing. This is only used by UHT and is generally not safe for threading. 
	 * All this really is used for is correcting the case of names. In MT conditions you might get a half-changed name.
	 */
	FNAME_Replace_Not_Safe_For_Threading,
};

/*----------------------------------------------------------------------------
	FNameEntry.
----------------------------------------------------------------------------*/

/** 
 * Mask for index bit used to determine whether string is encoded as TCHAR or ANSICHAR. We don't
 * add an extra bool in order to keep the name size to a minimum and 2 billion names is impractical
 * so there are a few bits left in the index.
 */
#define NAME_WIDE_MASK 0x1
#define NAME_INDEX_SHIFT 1

/**
 * A global name, as stored in the global name table.
 */
struct FNameEntry
{
private:
	/** Index of name in hash. */
	NAME_INDEX		Index;

public:
	/** Pointer to the next entry in this hash bin's linked list. */
	FNameEntry*		HashNext;

protected:
	/** Name, variable-sized - note that AllocateNameEntry only allocates memory as needed. */
	union
	{
		ANSICHAR	AnsiName[NAME_SIZE];
		WIDECHAR	WideName[NAME_SIZE];
	};
	// DO NOT ADD VARIABLES BELOW!

	/** 
	 * Constructor called from the linker name table serialization function. Initializes the index
	 * to a value that indicates widechar as that's what the linker is going to serialize.
	 *
	 * Only callable from the serialization version of this class
	 */
	FNameEntry( enum ELinkerNameTableConstructor )
	{
		Index = NAME_WIDE_MASK;
	}

public:
	/** Default constructor doesn't do anything. AllocateNameEntry is responsible for work. */
	FNameEntry()
	{}

	/** 
	 * Sets whether or not the NameEntry will have a wide string, or an ansi string
	 *
	 * @param bIsWide true if we are going to serialize a wide string
	 */
	FORCEINLINE void PreSetIsWideForSerialization(bool bIsWide)
	{
		Index = bIsWide ? NAME_WIDE_MASK : 0;
	}

	/**
	 * Returns index of name in hash passed to FNameEntry via AllocateNameEntry. The lower bits
	 * are used for internal state, which is why we need to shift.
	 *
	 * @return Index of name in hash
	 */
	FORCEINLINE int32 GetIndex() const
	{
		return Index >> NAME_INDEX_SHIFT;
	}

	/**
	 * Returns whether this name entry is represented via TCHAR or ANSICHAR
	 */
	FORCEINLINE bool IsWide() const
	{
		return (Index & NAME_WIDE_MASK);
	}

	/**
	 * @return FString of name portion minus number.
	 */
	CORE_API FString GetPlainNameString() const;	

	/**
	 * Appends this name entry to the passed in string.
	 *
	 * @param	String	String to append this name to
	 */
	CORE_API void AppendNameToString( FString& String ) const;

	/**
	* Appends this name entry to the passed in string, adding path separator between strings (with FString operator/).
	*
	* @param	String	String to append this name to
	*/
	CORE_API void AppendNameToPathString( FString& String ) const;

	/**
	 * @return length of name
	 */
	CORE_API int32 GetNameLength() const;

	/**
	 * Compares name using the compare method provided.
	 *
	 * @param	InName	Name to compare to
	 * @return	true if equal, false otherwise
	 */
	bool IsEqual( const ANSICHAR* InName, const ENameCase CompareMethod ) const;

	/**
	 * Compares name using the compare method provided.
	 *
	 * @param	InName	Name to compare to
	 * @return	true if equal, false otherwise
	 */
	bool IsEqual( const WIDECHAR* InName, const ENameCase CompareMethod ) const;

	/**
	 * @return direct access to ANSI name if stored in ANSI
	 */
	inline ANSICHAR const* GetAnsiName() const
	{
		check(!IsWide());
		return AnsiName;
	}

	/**
	 * @return direct access to wide name if stored in widechars
	 */
	inline WIDECHAR const* GetWideName() const
	{
		check(IsWide());
		return WideName;
	}

	static CORE_API int32 GetSize( const TCHAR* Name );

	/**
	 * Returns the size in bytes for FNameEntry structure. This is != sizeof(FNameEntry) as we only allocated as needed.
	 *
	 * @param	Length			Length of name
	 * @param	bIsPureAnsi		Whether name is pure ANSI or not
	 * @return	required size of FNameEntry structure to hold this string (might be wide or ansi)
	 */
	static int32 GetSize( int32 Length, bool bIsPureAnsi );

	// Functions.
	friend CORE_API FArchive& operator<<( FArchive& Ar, FNameEntry& E );
	friend CORE_API FArchive& operator<<( FArchive& Ar, FNameEntry* E )
	{
		return Ar << *E;
	}

	// Friend for access to Flags.
	template<typename TCharType>
	friend FNameEntry* AllocateNameEntry(const TCharType* Name, NAME_INDEX Index);
};

/**
 *  This struct is only used during loading/saving and is not part of the runtime costs
 */
struct FNameEntrySerialized :
	public FNameEntry
{
	uint16 NonCasePreservingHash;
	uint16 CasePreservingHash;
	bool bWereHashesLoaded;

	FNameEntrySerialized(const FNameEntry& NameEntry);
	FNameEntrySerialized(enum ELinkerNameTableConstructor) :
		FNameEntry(ENAME_LinkerConstructor),
		NonCasePreservingHash(0),
		CasePreservingHash(0),
		bWereHashesLoaded(false)
	{
	}

	friend CORE_API FArchive& operator<<(FArchive& Ar, FNameEntrySerialized& E);
	friend CORE_API FArchive& operator<<(FArchive& Ar, FNameEntrySerialized* E)
	{
		return Ar << *E;
	}
};

/**
 * Simple array type that can be expanded without invalidating existing entries.
 * This is critical to thread safe FNames.
 * @param ElementType Type of the pointer we are storing in the array
 * @param MaxTotalElements absolute maximum number of elements this array can ever hold
 * @param ElementsPerChunk how many elements to allocate in a chunk
 **/
 template<typename ElementType, int32 MaxTotalElements, int32 ElementsPerChunk>
class TStaticIndirectArrayThreadSafeRead
{
	enum
	{
		// figure out how many elements we need in the master table
		ChunkTableSize = (MaxTotalElements + ElementsPerChunk - 1) / ElementsPerChunk
	};
	/** Static master table to chunks of pointers **/
	ElementType** Chunks[ChunkTableSize];
	/** Number of elements we currently have **/
	int32 NumElements;
	/** Number of chunks we currently have **/
	int32 NumChunks;

	/**
	 * Expands the array so that Element[Index] is allocated. New pointers are all zero.
	 * @param Index The Index of an element we want to be sure is allocated
	 **/
	void ExpandChunksToIndex(int32 Index)
	{
		check(Index >= 0 && Index < MaxTotalElements);
		int32 ChunkIndex = Index / ElementsPerChunk;
		while (1)
		{
			if (ChunkIndex < NumChunks)
			{
				break;
			}
			// add a chunk, and make sure nobody else tries
			ElementType*** Chunk = &Chunks[ChunkIndex];
			ElementType** NewChunk = (ElementType**)FMemory::Malloc(sizeof(ElementType*) * ElementsPerChunk);
			FMemory::Memzero(NewChunk, sizeof(ElementType*) * ElementsPerChunk);
			if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)Chunk, NewChunk, nullptr))
			{
				// someone else beat us to the add, we don't support multiple concurrent adds
				check(0)
			}
			else
			{
				NumChunks++;
			}
		}
		check(ChunkIndex < NumChunks && Chunks[ChunkIndex]); // should have a valid pointer now
	}

	/**
	 * Return a pointer to the pointer to a given element
	 * @param Index The Index of an element we want to retrieve the pointer-to-pointer for
	 **/
	FORCEINLINE_DEBUGGABLE ElementType const* const* GetItemPtr(int32 Index) const
	{
		int32 ChunkIndex = Index / ElementsPerChunk;
		int32 WithinChunkIndex = Index % ElementsPerChunk;
		checkf(IsValidIndex(Index), TEXT("IsValidIndex(%d)"), Index);
		checkf(ChunkIndex < NumChunks, TEXT("ChunkIndex (%d) < NumChunks (%d)"), ChunkIndex, NumChunks);
		checkf(Index < MaxTotalElements, TEXT("Index (%d) < MaxTotalElements (%d)"), Index, MaxTotalElements);
		ElementType** Chunk = Chunks[ChunkIndex];
		check(Chunk);
		return Chunk + WithinChunkIndex;
	}

public:
	/** Constructor : Probably not thread safe **/
	TStaticIndirectArrayThreadSafeRead()
		: NumElements(0)
		, NumChunks(0)
	{
		FMemory::Memzero(Chunks);
	}
	/** 
	 * Return the number of elements in the array 
	 * Thread safe, but you know, someone might have added more elements before this even returns
	 * @return	the number of elements in the array
	**/
	FORCEINLINE int32 Num() const
	{
		return NumElements;
	}
	/** 
	 * Return if this index is valid
	 * Thread safe, if it is valid now, it is valid forever. Other threads might be adding during this call.
	 * @param	Index	Index to test
	 * @return	true, if this is a valid
	**/
	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Index < Num() && Index >= 0;
	}
	/** 
	 * Return a reference to an element
	 * @param	Index	Index to return
	 * @return	a reference to the pointer to the element
	 * Thread safe, if it is valid now, it is valid forever. This might return nullptr, but by then, some other thread might have made it non-nullptr.
	**/
	FORCEINLINE ElementType const* const& operator[](int32 Index) const
	{
		ElementType const* const* ItemPtr = GetItemPtr(Index);
		check(ItemPtr);
		return *ItemPtr;
	}
	/** 
	 * Add more elements to the array
	 * @param	NumToAdd	Number of elements to add
	 * @return	the number of elements in the container before we did the add. In other words, the add index.
	 * Not thread safe. This should only be called by one thread, but the other methods can be called while this is going on.
	**/
	int32 AddZeroed(int32 NumToAdd)
	{
		int32 Result = NumElements;
		check(NumElements + NumToAdd <= MaxTotalElements);
		ExpandChunksToIndex(NumElements + NumToAdd - 1);
		check(Result == NumElements);
		NumElements += NumToAdd;
		FPlatformMisc::MemoryBarrier();
		return Result;
	}
	/** 
	 * Return a naked pointer to the fundamental data structure for debug visualizers.
	**/
	ElementType*** GetRootBlockForDebuggerVisualizers()
	{
		return Chunks;
	}
	/**
	* Make sure chunks are allocated to hold the specified capacity of items. This is NOT thread safe.
	**/
	void Reserve(int32 Capacity)
	{
		check(Capacity >= 0 && Capacity <= MaxTotalElements);
		if (Capacity > NumElements)
		{			
			int32 MaxChunks = (Capacity + ElementsPerChunk - 1) / ElementsPerChunk;
			check(MaxChunks >= NumChunks);
			for (int32 ChunkIndex = 0; ChunkIndex < MaxChunks; ++ChunkIndex)
			{
				if (!Chunks[ChunkIndex])
				{
					ElementType** NewChunk = (ElementType**)FMemory::Malloc(sizeof(ElementType*) * ElementsPerChunk);
					FMemory::Memzero(NewChunk, sizeof(ElementType*) * ElementsPerChunk);
					Chunks[ChunkIndex] = NewChunk;
				}
			}
			NumChunks = MaxChunks;
		}
	}
};

// Typedef for the threadsafe master name table. 
// CAUTION: If you change those constants, you probably need to update the debug visualizers.
typedef TStaticIndirectArrayThreadSafeRead<FNameEntry, 2 * 1024 * 1024 /* 2M unique FNames */, 16384 /* allocated in 64K/128K chunks */ > TNameEntryArray;

/**
 * The minimum amount of data required to reconstruct a name
 * This is smaller than FName, but you lose the case-preserving behavior
 */
struct CORE_API FMinimalName
{
	FORCEINLINE FMinimalName()
		: Index(0)
		, Number(NAME_NO_NUMBER_INTERNAL)
	{
	}

	FORCEINLINE FMinimalName(const EName N)
		: Index(N)
		, Number(NAME_NO_NUMBER_INTERNAL)
	{
	}

	FORCEINLINE FMinimalName(const NAME_INDEX InIndex, const int32 InNumber)
		: Index(InIndex)
		, Number(InNumber)
	{
	}

	/** Index into the Names array (used to find String portion of the string/number pair) */
	NAME_INDEX		Index;
	/** Number portion of the string/number pair (stored internally as 1 more than actual, so zero'd memory will be the default, no-instance case) */
	int32			Number;
};

/**
 * The full amount of data required to reconstruct a case-preserving name
 * This will be the same size as FName when WITH_CASE_PRESERVING_NAME is 1, and is used to store an FName in cases where 
 * the size of FName must be constant between build configurations (eg, blueprint bytecode)
 */
struct CORE_API FScriptName
{
	FORCEINLINE FScriptName()
		: ComparisonIndex(0)
		, DisplayIndex(0)
		, Number(NAME_NO_NUMBER_INTERNAL)
	{
	}

	FORCEINLINE FScriptName(const EName N)
		: ComparisonIndex(N)
		, DisplayIndex(N)
		, Number(NAME_NO_NUMBER_INTERNAL)
	{
	}

	FORCEINLINE FScriptName(const NAME_INDEX InComparisonIndex, const NAME_INDEX InDisplayIndex, const int32 InNumber)
		: ComparisonIndex(InComparisonIndex)
		, DisplayIndex(InDisplayIndex)
		, Number(InNumber)
	{
	}

	/** Index into the Names array (used to find String portion of the string/number pair used for comparison) */
	NAME_INDEX		ComparisonIndex;
	/** Index into the Names array (used to find String portion of the string/number pair used for display) */
	NAME_INDEX		DisplayIndex;
	/** Number portion of the string/number pair (stored internally as 1 more than actual, so zero'd memory will be the default, no-instance case) */
	uint32			Number;
};

/**
 * Public name, available to the world.  Names are stored as a combination of
 * an index into a table of unique strings and an instance number.
 * Names are case-insensitive, but case-preserving (when WITH_CASE_PRESERVING_NAME is 1)
 */
class CORE_API FName
{
public:

	FORCEINLINE NAME_INDEX GetComparisonIndex() const
	{
		const NAME_INDEX Index = GetComparisonIndexFast();
		checkName(Index >= 0 && Index < GetNames().Num());
		checkName(GetNames()[Index]);
		return Index;
	}

	FORCEINLINE NAME_INDEX GetDisplayIndex() const
	{
		const NAME_INDEX Index = GetDisplayIndexFast();
		checkName(Index >= 0 && Index < GetNames().Num());
		checkName(GetNames()[Index]);
		return Index;
	}

	FORCEINLINE int32 GetNumber() const
	{
		return Number;
	}

	FORCEINLINE void SetNumber(const int32 NewNumber)
	{
		Number = NewNumber;
	}
	
	/** Returns the pure name string without any trailing numbers */
	FString GetPlainNameString() const
	{
		return GetDisplayNameEntry()->GetPlainNameString();
	}

	/**
	 * Returns the underlying ANSI string pointer.  No allocations.  Will fail if this is actually a wide name.
	 */
	FORCEINLINE ANSICHAR const* GetPlainANSIString() const
	{
		return GetDisplayNameEntry()->GetAnsiName();
	}

	/**
	 * Returns the underlying WIDE string pointer.  No allocations.  Will fail if this is actually an ANSI name.
	 */
	FORCEINLINE WIDECHAR const* GetPlainWIDEString() const
	{
		return GetDisplayNameEntry()->GetWideName();
	}

	const FNameEntry* GetComparisonNameEntry() const;
	const FNameEntry* GetDisplayNameEntry() const;

	/**
	 * Converts an FName to a readable format
	 *
	 * @return String representation of the name
	 */
	FString ToString() const;

	/**
	 * Converts an FName to a readable format, in place
	 * 
	 * @param Out String to fill with the string representation of the name
	 */
	void ToString(FString& Out) const;

	/**
	 * Converts an FName to a readable format, in place, appending to an existing string (ala GetFullName)
	 * 
	 * @param Out String to append with the string representation of the name
	 */
	void AppendString(FString& Out) const;

	/**
	 * Check to see if this FName matches the other FName, potentially also checking for any case variations
	 */
	FORCEINLINE bool IsEqual(const FName& Other, const ENameCase CompareMethod = ENameCase::IgnoreCase, const bool bCompareNumber = true ) const
	{
		return ((CompareMethod == ENameCase::IgnoreCase) ? GetComparisonIndexFast() == Other.GetComparisonIndexFast() : GetDisplayIndexFast() == Other.GetDisplayIndexFast())
			&& (!bCompareNumber || GetNumber() == Other.GetNumber());
	}

	FORCEINLINE bool operator==(const FName& Other) const
	{
		#if WITH_CASE_PRESERVING_NAME
			return GetComparisonIndexFast() == Other.GetComparisonIndexFast() && GetNumber() == Other.GetNumber();
		#else
			static_assert(sizeof(CompositeComparisonValue) == sizeof(*this), "ComparisonValue does not cover the entire FName state");
			return CompositeComparisonValue == Other.CompositeComparisonValue;
		#endif
	}
	FORCEINLINE bool operator!=(const FName& Other) const
	{
		return !(*this == Other);
	}

	/**
	 * Comparison operator used for sorting alphabetically.
	 */
	FORCEINLINE bool operator<( const FName& Other ) const
	{
		return Compare(Other) < 0;
	}

	/**
	 * Comparison operator used for sorting alphabetically.
	 */
	FORCEINLINE bool operator>(const FName& Other) const
	{
		return Compare(Other) > 0;
	}

	FORCEINLINE bool IsNone() const
	{
		return GetComparisonIndexFast() == 0 && GetNumber() == 0;
	}

	FORCEINLINE bool IsValid() const
	{
		TNameEntryArray& Names = GetNames();
		return GetComparisonIndexFast() >= 0 && GetComparisonIndexFast() < Names.Num() && Names[GetComparisonIndexFast()] != nullptr
			&& GetDisplayIndexFast() >= 0 && GetDisplayIndexFast() < Names.Num() && Names[GetDisplayIndexFast()] != nullptr;
	}

	/**
	 * Helper function to check if the index is valid. Does not check if the name itself is valid.
	 */
	FORCEINLINE bool IsValidIndexFast() const
	{
		return GetComparisonIndexFast() >= 0 && GetComparisonIndexFast() < GetNames().Num()
			&& GetDisplayIndexFast() >= 0 && GetDisplayIndexFast() < GetNames().Num();
	}

	/**
	 * Checks to see that a given name-like string follows the rules that Unreal requires.
	 *
	 * @param	InName			String containing the name to test.
	 * @param	InInvalidChars	The set of invalid characters that the name cannot contain.
	 * @param	OutReason		If the check fails, this string is filled in with the reason why.
	 * @param	InErrorCtx		Error context information to show in the error message (default is "Name").
	 *
	 * @return	true if the name is valid
	 */
	static bool IsValidXName( const FString& InName, const FString& InInvalidChars, FText* OutReason = nullptr, const FText* InErrorCtx = nullptr );

	/**
	 * Checks to see that a FName follows the rules that Unreal requires.
	 *
	 * @param	InInvalidChars	The set of invalid characters that the name cannot contain
	 * @param	OutReason		If the check fails, this string is filled in with the reason why.
	 * @param	InErrorCtx		Error context information to show in the error message (default is "Name").
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidXName( const FString& InInvalidChars = INVALID_NAME_CHARACTERS, FText* OutReason = nullptr, const FText* InErrorCtx = nullptr ) const
	{
		return IsValidXName(ToString(), InInvalidChars, OutReason, InErrorCtx);
	}

	/**
	 * Takes an FName and checks to see that it follows the rules that Unreal requires.
	 *
	 * @param	OutReason		If the check fails, this string is filled in with the reason why.
	 * @param	InInvalidChars	The set of invalid characters that the name cannot contain
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidXName( FText& OutReason, const FString& InInvalidChars = INVALID_NAME_CHARACTERS ) const
	{
		return IsValidXName(ToString(), InInvalidChars, &OutReason);
	}

	/**
	 * Takes an FName and checks to see that it follows the rules that Unreal requires for object names.
	 *
	 * @param	OutReason		If the check fails, this string is filled in with the reason why.
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidObjectName( FText& OutReason ) const
	{
		return IsValidXName(ToString(), INVALID_OBJECTNAME_CHARACTERS, &OutReason);
	}

	/**
	 * Takes an FName and checks to see that it follows the rules that Unreal requires for package or group names.
	 *
	 * @param	OutReason		If the check fails, this string is filled in with the reason why.
	 * @param	bIsGroupName	if true, check legality for a group name, else check legality for a package name
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidGroupName( FText& OutReason, bool bIsGroupName=false ) const
	{
		return IsValidXName(ToString(), INVALID_LONGPACKAGE_CHARACTERS, &OutReason);
	}

#ifdef IMPLEMENT_ASSIGNMENT_OPERATOR_MANUALLY
	// Assignment operator
	FORCEINLINE FName& operator=(const FName& Other)
	{
		this->ComparisonIndex = Other.ComparisonIndex;
#if WITH_CASE_PRESERVING_NAME
		this->DisplayIndex = Other.DisplayIndex;
#endif
		this->Number = Other.Number;

		return *this;
	}
#endif

	/**
	 * Compares name to passed in one. Sort is alphabetical ascending.
	 *
	 * @param	Other	Name to compare this against
	 * @return	< 0 is this < Other, 0 if this == Other, > 0 if this > Other
	 */
	int32 Compare( const FName& Other ) const;

	/**
	 * Fast compares name to passed in one using indexes. Sort is allocation order ascending.
	 *
	 * @param	Other	Name to compare this against
	 * @return	< 0 is this < Other, 0 if this == Other, > 0 if this > Other
	 */
	FORCEINLINE int32 CompareIndexes(const FName& Other) const
	{
		int32 ComparisonDiff = GetComparisonIndexFast() - Other.GetComparisonIndexFast();

		if (ComparisonDiff == 0)
		{
			return GetNumber() - Other.GetNumber();
		}
		return ComparisonDiff;
	}

	/**
	 * Create an FName with a hardcoded string index.
	 *
	 * @param N The hardcoded value the string portion of the name will have. The number portion will be NAME_NO_NUMBER
	 */
	FORCEINLINE FName( EName N )
		: ComparisonIndex( N )
#if WITH_CASE_PRESERVING_NAME
		, DisplayIndex( N )
#endif
		, Number( NAME_NO_NUMBER_INTERNAL )
	{
		check(N < NAME_MaxHardcodedNameIndex);
	}

	/**
	 * Create an FName with a hardcoded string index and (instance).
	 *
	 * @param N The hardcoded value the string portion of the name will have
	 * @param InNumber The hardcoded value for the number portion of the name
	 */
	FORCEINLINE FName( EName N, int32 InNumber )
		: ComparisonIndex( N )
#if WITH_CASE_PRESERVING_NAME
		, DisplayIndex( N )
#endif
		, Number( InNumber )
	{
		// If this fires the enum was out of bounds - did you pass an index instead?
		// If you want to clone an FName with a new number, FName( const FName& Other, int32 InNumber ) is the thing you want
		check(N < NAME_MaxHardcodedNameIndex);
		check(InNumber >= 0 && InNumber <= 0xffffff);
	}

	/**
	 * Create an FName from an existing string, but with a different instance.
	 *
	 * @param Other The FName to take the string values from
	 * @param InNumber The hardcoded value for the number portion of the name
	 */
	FORCEINLINE FName( const FName& Other, int32 InNumber )
		: ComparisonIndex( Other.ComparisonIndex )
#if WITH_CASE_PRESERVING_NAME
		, DisplayIndex( Other.DisplayIndex )
#endif
		, Number( InNumber )
	{
	}

	/**
	 * Create an FName from its component parts
	 * Only call this if you *really* know what you're doing
	 */
	FORCEINLINE FName( const NAME_INDEX InComparisonIndex, const NAME_INDEX InDisplayIndex, const int32 InNumber )
		: ComparisonIndex( InComparisonIndex )
#if WITH_CASE_PRESERVING_NAME
		, DisplayIndex( InDisplayIndex )
#endif
		, Number( InNumber )
	{
	}

	/**
	 * Default constructor, initialized to None
	 */
	FORCEINLINE FName()
		: ComparisonIndex( 0 )
#if WITH_CASE_PRESERVING_NAME
		, DisplayIndex( 0 )
#endif
		, Number( NAME_NO_NUMBER_INTERNAL )
	{
	}

	/**
	 * Scary no init constructor, used for something obscure in UObjectBase
	 */
	explicit FName(ENoInit)
	{
	}

	/**
	 * Create an FName. If FindType is FNAME_Find, and the string part of the name 
	 * doesn't already exist, then the name will be NAME_None
	 *
	 * @param Name			Value for the string portion of the name
	 * @param FindType		Action to take (see EFindName)
	 */
	FName(const WIDECHAR* Name, EFindName FindType=FNAME_Add);
	FName(const ANSICHAR* Name, EFindName FindType=FNAME_Add);

	// Deprecated bUnused
	DEPRECATED(4.12, "Removed bUnused from FName") FName(const WIDECHAR* Name, EFindName FindType, bool bUnused) : FName(Name, FindType) { }
	DEPRECATED(4.12, "Removed bUnused from FName") FName(const ANSICHAR* Name, EFindName FindType, bool bUnused) : FName(Name, FindType) { }

	/**
	 * Create an FName. If FindType is FNAME_Find, and the string part of the name 
	 * doesn't already exist, then the name will be NAME_None
	 *
	 * @param Name Value for the string portion of the name
	 * @param Number Value for the number portion of the name
	 * @param FindType Action to take (see EFindName)
	 */
	FName( const TCHAR* Name, int32 InNumber, EFindName FindType=FNAME_Add );

	/**
	 * Constructor used by FLinkerLoad when loading its name table; Creates an FName with an instance
	 * number of 0 that does not attempt to split the FName into string and number portions. Also,
	 * this version skips calculating the hashes of the names if possible
	 */
	FName(const FNameEntrySerialized& LoadedEntry);

	/**
	 * Create an FName with a hardcoded string index.
	 *
	 * @param HardcodedIndex	The hardcoded value the string portion of the name will have. 
	 * @param Name				The hardcoded name to initialize
	 */
	explicit FName( EName HardcodedIndex, const TCHAR* Name );

	/**
	 * Equality operator.
	 *
	 * @param	Other	String to compare this name to
	 * @return true if name matches the string, false otherwise
	 */
	template <typename CharType>
	bool operator==(const CharType* Other) const
	{
		// Find name entry associated with this FName.
		check(Other);
		const FNameEntry* const Entry = GetComparisonNameEntry();

		// Temporary buffer to hold split name in case passed in name is of Name_Number format.
		WIDECHAR TempBuffer[NAME_SIZE];
		int32 InNumber = NAME_NO_NUMBER_INTERNAL;
		int32 TempNumber = NAME_NO_NUMBER_INTERNAL;

		// Check whether we need to split the passed in string into name and number portion.
		auto WideOther = StringCast<WIDECHAR>(Other);
		const WIDECHAR* WideOtherPtr = WideOther.Get();
		if (SplitNameWithCheck(WideOtherPtr, TempBuffer, ARRAY_COUNT(TempBuffer), TempNumber))
		{
			WideOtherPtr = TempBuffer;
			InNumber = NAME_EXTERNAL_TO_INTERNAL(TempNumber);
		}

		// Report a match if both the number and string portion match.
		bool bAreNamesMatching = false;
		if (InNumber == GetNumber())
		{
			if (Entry->IsWide())
			{
				bAreNamesMatching = !FPlatformString::Stricmp(WideOtherPtr, Entry->GetWideName());
			}
			else
			{
				bAreNamesMatching = !FPlatformString::Stricmp(WideOtherPtr, Entry->GetAnsiName());
			}
		}

		return bAreNamesMatching;
	}

	/**
	 * Inequality operator.
	 *
	 * @param	Other	String to compare this name to
	 * @return true if name does not match the string, false otherwise
	 */
	template <typename CharType>
	bool operator!=(const CharType* Other) const
	{
		return !operator==(Other);
	}

	template <typename TCharType>
	static uint16 GetCasePreservingHash(const TCharType* Source);
	template <typename TCharType>
	static uint16 GetNonCasePreservingHash(const TCharType* Source);

	static void StaticInit();
	static void DisplayHash( class FOutputDevice& Ar );
	static FString SafeString( int32 InDisplayIndex, int32 InstanceNumber=NAME_NO_NUMBER_INTERNAL )
	{
		TNameEntryArray& Names = GetNames();
		return GetIsInitialized()
			? (Names.IsValidIndex(InDisplayIndex) && Names[InDisplayIndex])
				? FName(InDisplayIndex, InDisplayIndex, InstanceNumber).ToString()
				: FString(TEXT("*INVALID*"))
			: FString(TEXT("*UNINITIALIZED*"));
	}
	static int32 GetMaxNames()
	{
		return GetNames().Num();
	}
	/**
	 * @return Size of all name entries.
	 */
	static int32 GetNameEntryMemorySize()
	{
		return NameEntryMemorySize;
	}
	/**
	* @return Size of Name Table object as a whole
	*/
	static int32 GetNameTableMemorySize()
	{
		return GetNameEntryMemorySize() + (GetMaxNames() * sizeof(FNameEntry*)) + sizeof(NameHashHead) + sizeof(NameHashTail);
	}

	/**
	 * @return number of ansi names in name table
	 */
	static int32 GetNumAnsiNames()
	{
		return NumAnsiNames;
	}
	/**
	 * @return number of wide names in name table
	 */
	static int32 GetNumWideNames()
	{
		return NumWideNames;
	}
	static FNameEntry const* GetEntry( int i )
	{
		return GetNames()[i];
	}
	//@}

	/**
	 * Helper function to split an old-style name (Class_Number, ie Rocket_17) into
	 * the component parts usable by new-style FNames. Only use results if this function
	 * returns true.
	 *
	 * @param OldName		Old-style name
	 * @param NewName		Output string portion of the name/number pair
	 * @param NewNameLen	Size of NewName buffer (in TCHAR units)
	 * @param NewNumber		Number portion of the name/number pair
	 *
	 * @return true if the name was split, only then will NewName/NewNumber have valid values
	 */
	static bool SplitNameWithCheck(const WIDECHAR* OldName, WIDECHAR* NewName, int32 NewNameLen, int32& NewNumber);

	/** Singleton to retrieve a table of all names (multithreaded) for debug visualizers. */
	static FNameEntry*** GetNameTableForDebuggerVisualizers_MT();
	/** Run autotest on FNames. */
	static void AutoTest();
	
	/**
	 * Takes a string and breaks it down into a human readable string.
	 * For example - "bCreateSomeStuff" becomes "Create Some Stuff?" and "DrawScale3D" becomes "Draw Scale 3D".
	 * 
	 * @param	InDisplayName	[In, Out] The name to sanitize
	 * @param	bIsBool				True if the name is a bool
	 *
	 * @return	the sanitized version of the display name
	 */
	static FString NameToDisplayString( const FString& InDisplayName, const bool bIsBool );

private:

	union
	{
		struct
		{
			/** Index into the Names array (used to find String portion of the string/number pair used for comparison) */
			NAME_INDEX		ComparisonIndex;
		#if WITH_CASE_PRESERVING_NAME
			/** Index into the Names array (used to find String portion of the string/number pair used for display) */
			NAME_INDEX		DisplayIndex;
		#endif
			/** Number portion of the string/number pair (stored internally as 1 more than actual, so zero'd memory will be the default, no-instance case) */
			uint32			Number;
		};

		// Used to perform a single comparison in FName::operator==
		#if !WITH_CASE_PRESERVING_NAME
			uint64 CompositeComparisonValue;
		#endif
	};

	/** Name hash head - used to iterate the single-linked list.		*/
	static FNameEntry*						NameHashHead[FNameDefs::NameHashBucketCount];
	/** Name hash tail - insert new entries after this - NON ATOMIC!	*/
	static FNameEntry*						NameHashTail[FNameDefs::NameHashBucketCount];
	/** Size of all name entries.								*/
	static int32							NameEntryMemorySize;	
	/** Number of ANSI names in name table.						*/
	static int32							NumAnsiNames;			
	/** Number of wide names in name table.					*/
	static int32							NumWideNames;

	/** Singleton to retrieve a table of all names. */
	static TNameEntryArray& GetNames();
	/**
	 * Return the static initialized flag. Must be in a function like this so we don't have problems with 
	 * different initialization order of static variables across the codebase. Use this function to get or set the variable.
	 */
	static bool& GetIsInitialized();

	friend const TCHAR* DebugFName(int32);
	friend const TCHAR* DebugFName(int32, int32);
	friend const TCHAR* DebugFName(FName&);
	template<typename TCharType>
	friend FNameEntry* AllocateNameEntry(const TCharType* Name, NAME_INDEX Index);
	/** Used to increment the correct counter based upon TCharType */
	template <typename TCharType> friend void IncrementNameCount();

	/**
	 * Initialization from a wide string
	 * 
	 * @param InName String name of the name/number pair
	 * @param InNumber Number part of the name/number pair
	 * @param FindType Operation to perform on names
	 * @param bSplitName If true, this function will attempt to split a number off of the string portion (turning Rocket_17 to Rocket and number 17)
	 * @param HardcodeIndex If >= 0, this represents a hardcoded FName and so automatically gets this index
	 */
	void Init(const WIDECHAR* InName, int32 InNumber, EFindName FindType, bool bSplitName=true, int32 HardcodeIndex = -1);
	/**
	 * Version that takes the hashes as part of the constructor (serialized previously)
	 */
	void Init(const WIDECHAR* InName, int32 InNumber, EFindName FindType, const uint16 NonCasePreservingHash, const uint16 CasePreservingHash);

	/**
	 * Initialization from an ANSI string
	 * 
	 * @param InName		String name of the name/number pair
	 * @param InNumber		Number part of the name/number pair
	 * @param FindType		Operation to perform on names
	 * @param bSplitName	If true, this function will attempt to split a number off of the string portion (turning Rocket_17 to Rocket and number 17)
	 * @param HardcodeIndex If >= 0, this represents a hardcoded FName and so automatically gets this index
	 */
	void Init(const ANSICHAR* InName, int32 InNumber, EFindName FindType, bool bSplitName=true, int32 HardcodeIndex = -1);
	/**
	 * Version that takes the hashes as part of the constructor (serialized previously). Skips InitInternal_HashSplit for loading perf reasons
	 */
	void Init(const ANSICHAR* InName, int32 InNumber, EFindName FindType, const uint16 NonCasePreservingHash, const uint16 CasePreservingHash);

	template <typename TCharType>
	void InitInternal(const TCharType* InName, int32 InNumber, const EFindName FindType, const int32 HardcodeIndex, const uint16 NonCasePreservingHash, const uint16 CasePreservingHash);

	/**
	 * Version of InitInternal that calculates the hash after splitting the string. Used by runtime FName construction
	 */
	template <typename TCharType>
	void InitInternal_HashSplit(const TCharType* InName, int32 InNumber, const EFindName FindType, bool bSplitName, const int32 HardcodeIndex);

	template <typename TCharType>
	static bool InitInternal_FindOrAdd(const TCharType* InName, const EFindName FindType, const int32 HardcodeIndex, const uint16 NonCasePreservingHash, const uint16 CasePreservingHash, int32& OutComparisonIndex, int32& OutDisplayIndex);

	template <typename TCharType>
	static bool InitInternal_FindOrAddNameEntry(const TCharType* InName, const EFindName FindType, const ENameCase ComparisonMode, const uint16 iHash, int32& OutIndex);

	template <typename TCharType>
	static bool SplitNameWithCheckImpl(const TCharType* OldName, TCharType* NewName, int32 NewNameLen, int32& NewNumber);

	FORCEINLINE NAME_INDEX GetComparisonIndexFast() const
	{
		return ComparisonIndex;
	}

	FORCEINLINE NAME_INDEX GetDisplayIndexFast() const
	{
#if WITH_CASE_PRESERVING_NAME
		return DisplayIndex;
#else
		return ComparisonIndex;
#endif
	}

	/** Singleton to retrieve the critical section. */
	static FCriticalSection* GetCriticalSection();

};

template<> struct TIsZeroConstructType<class FName> { enum { Value = true }; };
Expose_TNameOf(FName)


inline uint32 GetTypeHash( const FName N )
{
	return N.GetComparisonIndex() + N.GetNumber();
}

namespace Lex
{
	FORCEINLINE FString ToString(const FName& Name)
	{
		return Name.ToString();
	}

	FORCEINLINE void FromString(FName& Name, const TCHAR* Str)
	{
		Name = FName(Str);
	}
}

FORCEINLINE FMinimalName NameToMinimalName(const FName& InName)
{
	return FMinimalName(InName.GetComparisonIndex(), InName.GetNumber());
}

FORCEINLINE FName MinimalNameToName(const FMinimalName& InName)
{
	return FName(InName.Index, InName.Index, InName.Number);
}


FORCEINLINE FScriptName NameToScriptName(const FName& InName)
{
	return FScriptName(InName.GetComparisonIndex(), InName.GetDisplayIndex(), InName.GetNumber());
}

FORCEINLINE FName ScriptNameToName(const FScriptName& InName)
{
	return FName(InName.ComparisonIndex, InName.DisplayIndex, InName.Number);
}


/**
 * Equality operator with CharType* on left hand side and FName on right hand side
 * 
 * @param	LHS		CharType to compare to FName
 * @param	RHS		FName to compare to CharType
 * @return True if strings match, false otherwise.
 */
template <typename CharType>
inline bool operator==(const CharType *LHS, const FName &RHS)
{
	return RHS == LHS;
}

/**
 * Inequality operator with CharType* on left hand side and FName on right hand side
 *
 * @param	LHS		CharType to compare to FName
 * @param	RHS		FName to compare to CharType
 * @return True if strings don't match, false otherwise.
 */
template <typename CharType>
inline bool operator!=(const CharType *LHS, const FName &RHS)
{
	return RHS != LHS;
}

/** FNames act like PODs. */
template <> struct TIsPODType<FName> { enum { Value = true }; };


/** Sort predicate to sort FName by index instead of alphabetically, pass to anything that wants TLess */
struct FNameSortIndexes
{
	FORCEINLINE bool operator()(const FName& A, const FName& B) const
	{
		return A.CompareIndexes(B) < 0;
	}
};