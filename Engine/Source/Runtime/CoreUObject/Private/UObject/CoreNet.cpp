// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnCoreNet.cpp: Core networking support.
=============================================================================*/

#include "UObject/CoreNet.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCoreNet, Log, All);

DEFINE_STAT(STAT_NetSerializeFastArray);
DEFINE_STAT(STAT_NetSerializeFastArray_BuildMap);

/*-----------------------------------------------------------------------------
	FClassNetCache implementation.
-----------------------------------------------------------------------------*/

FClassNetCache::FClassNetCache()
{
}

FClassNetCache::FClassNetCache( const UClass* InClass ) : Class( InClass )
{
}

void FClassNetCacheMgr::SortProperties( TArray< UProperty* >& Properties ) const
{
	// Sort NetProperties so that their ClassReps are sorted by memory offset
	struct FCompareUFieldOffsets
	{
		FORCEINLINE bool operator()( UProperty & A, UProperty & B ) const
		{
			// Ensure stable sort
			if ( A.GetOffset_ForGC() == B.GetOffset_ForGC() )
			{
				return A.GetName() < B.GetName();
			}

			return A.GetOffset_ForGC() < B.GetOffset_ForGC();
		}
	};

	Sort( Properties.GetData(), Properties.Num(), FCompareUFieldOffsets() );
}

uint32 FClassNetCacheMgr::SortedStructFieldsChecksum( const UStruct* Struct, uint32 Checksum ) const
{
	// Generate a list that we can sort, to make sure we process these deterministically
	TArray< UProperty * > Fields;

	for ( TFieldIterator< UProperty > It( Struct ); It; ++It )
	{
		if ( It->PropertyFlags & CPF_RepSkip )
		{
			continue;
		}

		Fields.Add( *It );
	}

	// Sort them
	SortProperties( Fields );

	// Evolve the checksum on the sorted list
	for ( auto Field : Fields )
	{
		Checksum = GetPropertyChecksum( Field, Checksum, true );
	}

	return Checksum;
}

uint32 FClassNetCacheMgr::GetPropertyChecksum( const UProperty* Property, uint32 Checksum, const bool bIncludeChildren ) const
{
	if ( bDebugChecksum )
	{
		UE_LOG( LogCoreNet, Warning, TEXT( "%s%s [%s] [%u] [%u]" ), FCString::Spc( 2 * DebugChecksumIndent ), *Property->GetName().ToLower(), *Property->GetClass()->GetName().ToLower(), Property->ArrayDim, Checksum );
	}

	Checksum = FCrc::StrCrc32( *Property->GetName().ToLower(), Checksum );							// Evolve checksum on name
	Checksum = FCrc::StrCrc32( *Property->GetCPPType( nullptr, 0 ).ToLower(), Checksum );			// Evolve by property type
	Checksum = FCrc::StrCrc32( *FString::Printf( TEXT( "%u" ), Property->ArrayDim ), Checksum );	// Evolve checksum on array dim (to detect when static arrays change size)

	if ( bIncludeChildren )
	{
		const UArrayProperty* ArrayProperty = Cast< UArrayProperty >( Property );

		// Evolve checksum on array inner
		if ( ArrayProperty != NULL )
		{
			return GetPropertyChecksum( ArrayProperty->Inner, Checksum, bIncludeChildren );
		}

		const UStructProperty* StructProperty = Cast< UStructProperty >( Property );

		// Evolve checksum on property struct fields
		if ( StructProperty != NULL )
		{
			if ( bDebugChecksum )
			{
				UE_LOG( LogCoreNet, Warning, TEXT( "%s [%s] [%u]" ), FCString::Spc( 2 * DebugChecksumIndent ), *StructProperty->Struct->GetName().ToLower(), Checksum );
			}

			// Evolve checksum on struct name
			Checksum = FCrc::StrCrc32( *StructProperty->Struct->GetName().ToLower(), Checksum );

			const_cast< FClassNetCacheMgr* >( this )->DebugChecksumIndent++;

			Checksum = SortedStructFieldsChecksum( StructProperty->Struct, Checksum );

			const_cast< FClassNetCacheMgr* >( this )->DebugChecksumIndent--;
		}
	}

	return Checksum;
}

uint32 FClassNetCacheMgr::GetFunctionChecksum( const UFunction* Function, uint32 Checksum ) const
{
	// Evolve checksum on function name
	Checksum = FCrc::StrCrc32( *Function->GetName().ToLower(), Checksum );

	// Evolve the checksum on function flags
	Checksum = FCrc::StrCrc32( *FString::Printf( TEXT( "%u" ), (uint32)Function->FunctionFlags ), Checksum );

#if 0	// This is disabled now that we have backwards compatibility for RPC parameters working in replays 
	TArray< UProperty * > Parms;

	for ( TFieldIterator< UProperty > It( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
	{
		Parms.Add( *It );
	}

	// Sort parameters by offset/name
	SortProperties( Parms );

	// Evolve checksum on sorted function parameters
	for ( UProperty* Parm : Parms )
	{
		Checksum = GetPropertyChecksum( Parm, Checksum, true );
	}
#endif

	return Checksum;
}

uint32 FClassNetCacheMgr::GetFieldChecksum( const UField* Field, uint32 Checksum ) const
{
	if ( Cast< UProperty >( Field ) != NULL )
	{
		return GetPropertyChecksum( (UProperty*)Field, Checksum, false );
	}
	else if ( Cast< UFunction >( Field ) != NULL )
	{
		return GetFunctionChecksum( (UFunction*)Field, Checksum );
	}

	UE_LOG( LogCoreNet, Warning, TEXT( "GetFieldChecksum: Unknown field: %s" ), *Field->GetName() );

	return Checksum;
}

const FClassNetCache* FClassNetCacheMgr::GetClassNetCache( const UClass* Class )
{
	FClassNetCache* Result = ClassFieldIndices.FindRef( Class );

	if ( !Result )
	{
		Result					= ClassFieldIndices.Add( Class, new FClassNetCache( Class ) );
		Result->Super			= NULL;
		Result->FieldsBase		= 0;
		Result->ClassChecksum	= 0;

		if ( Class->GetSuperClass() )
		{
			Result->Super			= GetClassNetCache( Class->GetSuperClass() );
			Result->FieldsBase		= Result->Super->GetMaxIndex();
			Result->ClassChecksum	= Result->Super->ClassChecksum;
		}

		Result->Fields.Empty( Class->NetFields.Num() );

		TArray< UProperty* > Properties;
		Properties.Empty( Class->NetFields.Num() );

		for( int32 i = 0; i < Class->NetFields.Num(); i++ )
		{
			// Add each net field to cache, and assign index/checksum
			UField* Field = Class->NetFields[i];

			UProperty* Property = Cast< UProperty >( Field );
			
			if ( Property != NULL )
			{
				Properties.Add( Property );
			}

			// Get individual checksum
			const uint32 Checksum = GetFieldChecksum( Field, 0 );

			// Get index
			const int32 ThisIndex = Result->GetMaxIndex();

			// Add to cached fields on this class
			Result->Fields.Add( FFieldNetCache( Field, ThisIndex, Checksum ) );
		}

		Result->Fields.Shrink();

		// Add fields to the appropriate hash maps
		for ( TArray< FFieldNetCache >::TIterator It( Result->Fields ); It; ++It )
		{
			Result->FieldMap.Add( It->Field, &*It );

			if ( Result->FieldChecksumMap.Contains( It->FieldChecksum ) )
			{
				UE_LOG( LogCoreNet, Error, TEXT ( "Duplicate checksum: %s, %u" ), *It->Field->GetName(), It->FieldChecksum );
			}

			Result->FieldChecksumMap.Add( It->FieldChecksum, &*It );
		}

		// Initialize class checksum (just use properties for this)
		SortProperties( Properties );

		for ( auto Property : Properties )
		{
			Result->ClassChecksum = GetPropertyChecksum( Property, Result->ClassChecksum, true );
		}
	}
	return Result;
}

void FClassNetCacheMgr::ClearClassNetCache()
{
	for ( auto It = ClassFieldIndices.CreateIterator(); It; ++It)
	{
		delete It.Value();
	}

	ClassFieldIndices.Empty();
}

/*-----------------------------------------------------------------------------
	UPackageMap implementation.
-----------------------------------------------------------------------------*/

bool UPackageMap::SerializeName(FArchive& Ar, FName& InName)
{
	if (Ar.IsLoading())
	{
		uint8 bHardcoded = 0;
		Ar.SerializeBits(&bHardcoded, 1);
		if (bHardcoded)
		{
			// replicated by hardcoded index
			uint32 NameIndex;
			Ar.SerializeInt(NameIndex, MAX_NETWORKED_HARDCODED_NAME + 1);
			InName = EName(NameIndex);
			// hardcoded names never have a Number
		}
		else
		{
			// replicated by string
			FString InString;
			int32 InNumber;
			Ar << InString << InNumber;
			InName = FName(*InString, InNumber);
		}
	}
	else if (Ar.IsSaving())
	{
		uint8 bHardcoded = InName.GetComparisonIndex() <= MAX_NETWORKED_HARDCODED_NAME;
		Ar.SerializeBits(&bHardcoded, 1);
		if (bHardcoded)
		{
			// send by hardcoded index
			checkSlow(InName.GetNumber() <= 0); // hardcoded names should never have a Number
			uint32 NameIndex = uint32(InName.GetComparisonIndex());
			Ar.SerializeInt(NameIndex, MAX_NETWORKED_HARDCODED_NAME + 1);
		}
		else
		{
			// send by string
			FString OutString = InName.GetPlainNameString();
			int32 OutNumber = InName.GetNumber();
			Ar << OutString << OutNumber;
		}
	}
	return true;
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UPackageMap, UObject,
	{
	}
);

// ----------------------------------------------------------------

void SerializeChecksum(FArchive &Ar, uint32 x, bool ErrorOK)
{
	if (Ar.IsLoading() )
	{
		uint32 Magic = 0;
		Ar << Magic;
		if((!ErrorOK || !Ar.IsError()) && !ensure(Magic==x))
		{
			UE_LOG(LogCoreNet, Warning, TEXT("%d == %d"), Magic, x );
		}
		
	}
	else
	{
		uint32 Magic = x;
		Ar << Magic;
	}
}

// ----------------------------------------------------------------
//	FNetBitWriter
// ----------------------------------------------------------------
FNetBitWriter::FNetBitWriter()
:	FBitWriter(0)
{}

FNetBitWriter::FNetBitWriter( int64 InMaxBits )
:	FBitWriter (InMaxBits, true)
,	PackageMap( NULL )
{

}

FNetBitWriter::FNetBitWriter( UPackageMap * InPackageMap, int64 InMaxBits )
:	FBitWriter (InMaxBits, true)
,	PackageMap( InPackageMap )
{

}

FArchive& FNetBitWriter::operator<<( class FName& N )
{
	PackageMap->SerializeName( *this, N );
	return *this;
}

FArchive& FNetBitWriter::operator<<( UObject*& Object )
{
	PackageMap->SerializeObject( *this, UObject::StaticClass(), Object );
	return *this;
}

FArchive& FNetBitWriter::operator<<(FSoftObjectPath& Value)
{
	FString Path = Value.ToString();

	*this << Path;

	if (IsLoading())
	{
		Value.SetPath(MoveTemp(Path));
	}

	return *this;
}

FArchive& FNetBitWriter::operator<<(struct FWeakObjectPtr& WeakObjectPtr)
{
	WeakObjectPtr.Serialize(*this);
	return *this;
}

// ----------------------------------------------------------------
//	FNetBitReader
// ----------------------------------------------------------------
FNetBitReader::FNetBitReader( UPackageMap *InPackageMap, uint8* Src, int64 CountBits )
:	FBitReader	(Src, CountBits)
,   PackageMap  ( InPackageMap )
{
}

FArchive& FNetBitReader::operator<<( UObject*& Object )
{
	PackageMap->SerializeObject( *this, UObject::StaticClass(), Object );
	return *this;
}

FArchive& FNetBitReader::operator<<( class FName& N )
{
	PackageMap->SerializeName( *this, N );
	return *this;
}

FArchive& FNetBitReader::operator<<(FSoftObjectPath& Value)
{
	FString Path = Value.ToString();

	*this << Path;

	if (IsLoading())
	{
		Value.SetPath(MoveTemp(Path));
	}

	return *this;
}

FArchive& FNetBitReader::operator<<(struct FWeakObjectPtr& WeakObjectPtr)
{
	WeakObjectPtr.Serialize(*this);
	return *this;
}

static const TCHAR* GLastRPCFailedReason = NULL;

void RPC_ResetLastFailedReason()
{
	GLastRPCFailedReason = NULL;
}
void RPC_ValidateFailed( const TCHAR* Reason )
{
	GLastRPCFailedReason = Reason;
}

const TCHAR* RPC_GetLastFailedReason()
{
	return GLastRPCFailedReason;
}
