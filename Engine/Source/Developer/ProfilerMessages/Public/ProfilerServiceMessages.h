// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "ProfilerServiceMessages.generated.h"


/** Profiler Service authorization message. */
USTRUCT()
struct FProfilerServiceAuthorize
{
	GENERATED_USTRUCT_BODY()

	/** Session ID. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid SessionId;

	/** Instance ID. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid InstanceId;

	/** Default constructor. */
	FProfilerServiceAuthorize() { }

	/** Creates and initializes a new instance. */
	FProfilerServiceAuthorize(const FGuid& InSessionId, const FGuid& InInstanceId)
		: SessionId(InSessionId)
		, InstanceId(InInstanceId)
	{}

};


/** Profiler Service data. */
USTRUCT()
struct FProfilerServiceData2
{
	GENERATED_USTRUCT_BODY()

	/** Instance ID. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid InstanceId;

	/** Stats frame. */
	UPROPERTY(EditAnywhere, Category="Message")
	int64 Frame;

	/** Size of the compressed data. */
	UPROPERTY(EditAnywhere, Category="Message")
	int32 CompressedSize;
	
	/** Size of the uncompressed data. */
	UPROPERTY(EditAnywhere, Category="Message")
	int32 UncompressedSize;

	/** Profiler data encoded as string of hexes, cannot use TArray<uint8> because of the Message Bus limitation. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HexData;

	/** Default constructor. */
	FProfilerServiceData2() { }

	/** Creates and initializes a new instance. */
	FProfilerServiceData2(const FGuid& InInstance, int64 InFrame, const FString& InHexData, int32 InCompressedSize, int32 InUncompressedSize)
		: InstanceId(InInstance)
		, Frame(InFrame)
		, CompressedSize(InCompressedSize)
		, UncompressedSize(InUncompressedSize)
		, HexData(InHexData)
	{ }
};


/**
 */
USTRUCT()
struct FProfilerServicePreviewAck
{
	GENERATED_USTRUCT_BODY()

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid InstanceId;

	/** Default constructor. */
	FProfilerServicePreviewAck() { }

	/** Creates and initializes a new instance. */
	FProfilerServicePreviewAck(const FGuid& InInstance)
		: InstanceId(InInstance)
	{ }
};


/**
 * Implements a message for copying a file through the network, as well as for synchronization.
 * Unfortunately assumes that InstanceId and Filename are transfered without errors.
 */
USTRUCT()
struct FProfilerServiceFileChunk
{
	GENERATED_USTRUCT_BODY()

	/** The ID of the instance where this message should be sent. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid InstanceId;

	/** The file containing this file chunk. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Filename;

	/** Data to be sent through message bus. Message bug doesn't support TArray<>, so we encode the data as HexString. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HexData;

	/** FProfilerFileChunkHeader stored in the array. */
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<uint8> Header;

	/** Hash of this data and header. */
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<uint8> ChunkHash;

	/** Default constructor. */
	FProfilerServiceFileChunk() { }

	/** Constructor for the new file chunk. */
	FProfilerServiceFileChunk
	( 
		const FGuid& InInstanceID, 
		const FString& InFilename, 
		const TArray<uint8>& InHeader
	)
		: InstanceId(InInstanceID)
		, Filename(InFilename)
		, Header(InHeader)
	{ }

	struct FNullTag
	{
	};

	/** Copy constructor, copies all properties, but not data. */
	FProfilerServiceFileChunk(const FProfilerServiceFileChunk& ProfilerServiceFileChunk, FNullTag)
		: InstanceId(ProfilerServiceFileChunk.InstanceId)
		, Filename(ProfilerServiceFileChunk.Filename)
		, Header(ProfilerServiceFileChunk.Header)
	{ }
};


/**
 */
USTRUCT()
struct FProfilerServicePing
{
	GENERATED_USTRUCT_BODY()
};


/**
 */
USTRUCT()
struct FProfilerServicePong
{
	GENERATED_USTRUCT_BODY()

};


/**
 */
USTRUCT()
struct FProfilerServiceSubscribe
{
	GENERATED_USTRUCT_BODY()

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid SessionId;

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid InstanceId;

	/** Default constructor. */
	FProfilerServiceSubscribe() { }

	/** Creates and initializes a new instance. */
	FProfilerServiceSubscribe(const FGuid& InSessionId, const FGuid& InInstanceId)
		: SessionId(InSessionId)
		, InstanceId(InInstanceId)
	{ }
};


/**
 */
USTRUCT()
struct FProfilerServiceUnsubscribe
{
	GENERATED_USTRUCT_BODY()

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid SessionId;

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid InstanceId;

	/** Default constructor. */
	FProfilerServiceUnsubscribe() { }

	/** Creates and initializes a new instance. */
	FProfilerServiceUnsubscribe(const FGuid& InSessionId, const FGuid& InInstanceId)
		: SessionId(InSessionId)
		, InstanceId(InInstanceId)
	{ }
};


/**
 */
USTRUCT()
struct FProfilerServiceCapture
{
	GENERATED_USTRUCT_BODY()

	/** The data capture state that should be set. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool bRequestedCaptureState;

	/** Default constructor. */
	FProfilerServiceCapture() { }

	/** Creates and initializes a new instance. */
	FProfilerServiceCapture(const bool bInRequestedCaptureState)
		: bRequestedCaptureState(bInRequestedCaptureState)
	{ }
};


/**
 */
USTRUCT()
struct FProfilerServicePreview
{
	GENERATED_USTRUCT_BODY()

	/**
	 * The data preview state that should be set.
	 */
	UPROPERTY(EditAnywhere, Category="Message")
	bool bRequestedPreviewState;

	/** Default constructor. */
	FProfilerServicePreview() { }

	/** Creates and initializes a new instance. */
	FProfilerServicePreview(const bool bInRequestedPreviewState)
		: bRequestedPreviewState(bInRequestedPreviewState)
	{ }
};


/**
 */
USTRUCT()
struct FProfilerServiceRequest
{
	GENERATED_USTRUCT_BODY()

	/** Request @see EProfilerRequestType. */
	UPROPERTY(EditAnywhere, Category="Message")
	uint32 Request;

	/** Default constructor. */
	FProfilerServiceRequest() { }

	/** Creates and initializes a new instance. */
	FProfilerServiceRequest(uint32 InRequest)
		: Request(InRequest)
	{ }
};
