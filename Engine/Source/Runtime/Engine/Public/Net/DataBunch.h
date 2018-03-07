// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DataBunch.h: Unreal bunch class.
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "Misc/NetworkGuid.h"
#include "UObject/CoreNet.h"
#include "EngineLogs.h"

class UChannel;
class UNetConnection;

//
// A bunch of data to send.
//
class FOutBunch : public FNetBitWriter
{
public:
	// Variables.
	FOutBunch *				Next;
	UChannel *				Channel;
	double					Time;
	bool					ReceivedAck;
	int32					ChIndex;
	int32					ChType;
	int32					ChSequence;
	int32					PacketId;
	uint8					bOpen;
	uint8					bClose;
	uint8					bDormant;
	uint8					bIsReplicationPaused;   // Replication on this channel is being paused by the server
	uint8					bReliable;
	uint8					bPartial;				// Not a complete bunch
	uint8					bPartialInitial;		// The first bunch of a partial bunch
	uint8					bPartialFinal;			// The final bunch of a partial bunch
	uint8					bHasPackageMapExports;	// This bunch has networkGUID name/id pairs
	uint8					bHasMustBeMappedGUIDs;	// This bunch has guids that must be mapped before we can process this bunch

	TArray< FNetworkGUID >	ExportNetGUIDs;			// List of GUIDs that went out on this bunch
	TArray< uint64 >		NetFieldExports;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FString			DebugString;
	void	SetDebugString(FString DebugStr)
	{
			DebugString = DebugStr;
	}
	FString	GetDebugString()
	{
		return DebugString;
	}
#else
	FORCEINLINE void SetDebugString(FString DebugStr)
	{

	}
	FORCEINLINE FString	GetDebugString()
	{
		return FString();
	}
#endif

	// Functions.
	ENGINE_API FOutBunch();
	ENGINE_API FOutBunch( class UChannel* InChannel, bool bClose );
	ENGINE_API FOutBunch( UPackageMap * PackageMap, int64 InMaxBits = 1024 );


	FString	ToString()
	{
		// String cating like this is super slow! Only enable in non shipping builds
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FString Str(TEXT("FOutBunch: "));
		Str += FString::Printf(TEXT("Channel[%d] "), ChIndex);
		Str += FString::Printf(TEXT("ChSequence: %d "), ChSequence);
		Str += FString::Printf(TEXT("NumBits: %lld "), GetNumBits());
		Str += FString::Printf(TEXT("PacketId: %d "), PacketId);
		Str += FString::Printf(TEXT("bOpen: %d "), bOpen);
		Str += FString::Printf(TEXT("bClose: %d "), bClose);
		Str += FString::Printf(TEXT("bDormant: %d "), bDormant);
		Str += FString::Printf(TEXT("bIsReplicationPaused: %d "), bIsReplicationPaused);
		Str += FString::Printf(TEXT("bReliable: %d "), bReliable);
		Str += FString::Printf(TEXT("bPartial: %d//%d//%d "), bPartial, bPartialInitial, bPartialFinal);
		Str += FString::Printf( TEXT( "bHasPackageMapExports: %d " ), bHasPackageMapExports );
		Str += GetDebugString();
#else
		FString Str = FString::Printf(TEXT("Channel[%d]. Seq %d. PacketId: %d"), ChIndex, ChSequence, PacketId);
#endif
		return Str;
	}
};

//
// A bunch of data received from a channel.
//
class FInBunch : public FNetBitReader
{
public:
	// Variables.
	int32				PacketId;	// Note this must stay as first member variable in FInBunch for FInBunch(FInBunch, bool) to work
	FInBunch *			Next;
	UNetConnection *	Connection;
	int32				ChIndex;
	int32				ChType;
	int32				ChSequence;
	uint8				bOpen;
	uint8				bClose;
	uint8				bDormant;				// Close, but go dormant
	uint8				bIsReplicationPaused;	// Replication on this channel is being paused by the server
	uint8				bReliable;
	uint8				bPartial;				// Not a complete bunch
	uint8				bPartialInitial;		// The first bunch of a partial bunch
	uint8				bPartialFinal;			// The final bunch of a partial bunch
	uint8				bHasPackageMapExports;	// This bunch has networkGUID name/id pairs
	uint8				bHasMustBeMappedGUIDs;	// This bunch has guids that must be mapped before we can process this bunch

	FString	ToString()
	{
		// String cating like this is super slow! Only enable in non shipping builds
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FString Str(TEXT("FInBunch: "));
		Str += FString::Printf(TEXT("Channel[%d] "), ChIndex);
		Str += FString::Printf(TEXT("ChSequence: %d "), ChSequence);
		Str += FString::Printf(TEXT("NumBits: %lld "), GetNumBits());
		Str += FString::Printf(TEXT("PacketId: %d "), PacketId);
		Str += FString::Printf(TEXT("bOpen: %d "), bOpen);
		Str += FString::Printf(TEXT("bClose: %d "), bClose);
		Str += FString::Printf(TEXT("bDormant: %d "), bDormant);
		Str += FString::Printf(TEXT("bIsReplicationPaused: %d "), bIsReplicationPaused);
		Str += FString::Printf(TEXT("bReliable: %d "), bReliable);
		Str += FString::Printf(TEXT("bPartial: %d//%d//%d "), bPartial, bPartialInitial, bPartialFinal);
		Str += FString::Printf( TEXT( "bHasPackageMapExports: %d " ), bHasPackageMapExports );
#else
		FString Str = FString::Printf(TEXT("Channel[%d]. Seq %d. PacketId: %d"), ChIndex, ChSequence, PacketId);
#endif
		return Str;
	}
 
	// Functions.
	ENGINE_API FInBunch( UNetConnection* InConnection, uint8* Src=NULL, int64 CountBits=0 );
	ENGINE_API FInBunch( FInBunch &InBunch, bool CopyBuffer );
};

/** out bunch for the control channel (special restrictions) */
struct FControlChannelOutBunch : public FOutBunch
{
	ENGINE_API FControlChannelOutBunch(class UChannel* InChannel, bool bClose);

	FArchive& operator<<(FName& Name)
	{
		UE_LOG(LogNet, Fatal,TEXT("Cannot send Names on the control channel"));
		ArIsError = true;
		return *this;
	}
	FArchive& operator<<(UObject*& Object)
	{
		UE_LOG(LogNet, Fatal,TEXT("Cannot send Objects on the control channel"));
		ArIsError = true;
		return *this;
	}
};
