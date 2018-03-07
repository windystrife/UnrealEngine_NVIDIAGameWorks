// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandList.cpp: Metal command buffer list wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandList.h"
#include "MetalCommandQueue.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"

#pragma mark - Public C++ Boilerplate -

FMetalCommandList::FMetalCommandList(FMetalCommandQueue& InCommandQueue, bool const bInImmediate)
: CommandQueue(InCommandQueue)
, SubmittedBuffers(!bInImmediate ? [NSMutableArray<id<MTLCommandBuffer>> new] : nil)
, bImmediate(bInImmediate)
{
}

FMetalCommandList::~FMetalCommandList(void)
{
	[SubmittedBuffers release];
	SubmittedBuffers = nil;
}
	
#pragma mark - Public Command List Mutators -

static void ReportMetalCommandBufferFailure(id <MTLCommandBuffer> CompletedBuffer, TCHAR const* ErrorType, bool bDoCheck=true)
{
	NSString* Label = CompletedBuffer.label;
	int32 Code = CompletedBuffer.error.code;
	NSString* Domain = CompletedBuffer.error.domain;
	NSString* ErrorDesc = CompletedBuffer.error.localizedDescription;
	NSString* FailureDesc = CompletedBuffer.error.localizedFailureReason;
	NSString* RecoveryDesc = CompletedBuffer.error.localizedRecoverySuggestion;
	
	FString LabelString = Label ? FString(Label) : FString(TEXT("Unknown"));
	FString DomainString = Domain ? FString(Domain) : FString(TEXT("Unknown"));
	FString ErrorString = ErrorDesc ? FString(ErrorDesc) : FString(TEXT("Unknown"));
	FString FailureString = FailureDesc ? FString(FailureDesc) : FString(TEXT("Unknown"));
	FString RecoveryString = RecoveryDesc ? FString(RecoveryDesc) : FString(TEXT("Unknown"));
	
	if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
	{
		NSMutableString* DescString = [NSMutableString new];
		[DescString appendFormat:@"Command Buffer %p %@:", CompletedBuffer, Label ? Label : @"Unknown"];

		for (NSString* String in ((NSObject<MTLCommandBuffer>*)CompletedBuffer).debugGroups)
		{
			[DescString appendFormat:@"\n\tDebugGroup: %@", String];
		}
		
		UE_LOG(LogMetal, Warning, TEXT("Command Buffer %p %s:%s"), CompletedBuffer, *LabelString, *FString(DescString));
	}
	else
	{
		NSString* Desc = CompletedBuffer.debugDescription;
		UE_LOG(LogMetal, Warning, TEXT("%s"), *FString(Desc));
	}
	
    if (bDoCheck)
    {
		UE_LOG(LogMetal, Fatal, TEXT("Command Buffer %s Failed with %s Error! Error Domain: %s Code: %d Description %s %s %s"), *LabelString, ErrorType, *DomainString, Code, *ErrorString, *FailureString, *RecoveryString);
    }
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureInternal(id <MTLCommandBuffer> CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Internal"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureTimeout(id <MTLCommandBuffer> CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Timeout"), PLATFORM_IOS);
}

static __attribute__ ((optnone)) void MetalCommandBufferFailurePageFault(id <MTLCommandBuffer> CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("PageFault"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureBlacklisted(id <MTLCommandBuffer> CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Blacklisted"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureNotPermitted(id <MTLCommandBuffer> CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("NotPermitted"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureOutOfMemory(id <MTLCommandBuffer> CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("OutOfMemory"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureInvalidResource(id <MTLCommandBuffer> CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("InvalidResource"));
}

static void HandleMetalCommandBufferError(id <MTLCommandBuffer> CompletedBuffer)
{
	MTLCommandBufferError Code = (MTLCommandBufferError)CompletedBuffer.error.code;
	switch(Code)
	{
		case MTLCommandBufferErrorInternal:
			MetalCommandBufferFailureInternal(CompletedBuffer);
			break;
		case MTLCommandBufferErrorTimeout:
			MetalCommandBufferFailureTimeout(CompletedBuffer);
			break;
		case MTLCommandBufferErrorPageFault:
			MetalCommandBufferFailurePageFault(CompletedBuffer);
			break;
		case MTLCommandBufferErrorBlacklisted:
			MetalCommandBufferFailureBlacklisted(CompletedBuffer);
			break;
		case MTLCommandBufferErrorNotPermitted:
			MetalCommandBufferFailureNotPermitted(CompletedBuffer);
			break;
		case MTLCommandBufferErrorOutOfMemory:
			MetalCommandBufferFailureOutOfMemory(CompletedBuffer);
			break;
		case MTLCommandBufferErrorInvalidResource:
			MetalCommandBufferFailureInvalidResource(CompletedBuffer);
			break;
		case MTLCommandBufferErrorNone:
			// No error
			break;
		default:
			ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Unknown"));
			break;
	}
}

static __attribute__ ((optnone)) void HandleAMDMetalCommandBufferError(id <MTLCommandBuffer> CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

static __attribute__ ((optnone)) void HandleNVIDIAMetalCommandBufferError(id <MTLCommandBuffer> CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

static void HandleIntelMetalCommandBufferError(id <MTLCommandBuffer> CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

void FMetalCommandList::HandleMetalCommandBufferFailure(id <MTLCommandBuffer> CompletedBuffer)
{
	if (CompletedBuffer.error.domain == MTLCommandBufferErrorDomain || [CompletedBuffer.error.domain isEqualToString:MTLCommandBufferErrorDomain])
	{
		if (GRHIVendorId && IsRHIDeviceAMD())
		{
			HandleAMDMetalCommandBufferError(CompletedBuffer);
		}
		else if (GRHIVendorId && IsRHIDeviceNVIDIA())
		{
			HandleNVIDIAMetalCommandBufferError(CompletedBuffer);
		}
		else if (GRHIVendorId && IsRHIDeviceIntel())
		{
			HandleIntelMetalCommandBufferError(CompletedBuffer);
		}
		else
		{
			HandleMetalCommandBufferError(CompletedBuffer);
		}
	}
	else
	{
		ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Unknown"));
	}
}

void FMetalCommandList::Commit(id<MTLCommandBuffer> Buffer, NSArray<MTLCommandBufferHandler>* CompletionHandlers, bool const bWait)
{
	check(Buffer);
	
	[CompletionHandlers retain];
	[Buffer addCompletedHandler : ^ (id <MTLCommandBuffer> CompletedBuffer)
	{
		if (CompletedBuffer.status == MTLCommandBufferStatusError)
		{
			HandleMetalCommandBufferFailure(Buffer);
		}
		if (CompletionHandlers)
		{
			for (MTLCommandBufferHandler Handler in CompletionHandlers)
			{
				Handler(CompletedBuffer);
			}
			[CompletionHandlers release];
		}
	}];
    
    FMetalGPUProfiler::RecordCommandBuffer(Buffer);
    
	if (bImmediate)
	{
		if (bWait)
		{
			[Buffer retain];
		}
		CommandQueue.CommitCommandBuffer(Buffer);
		if (bWait)
		{
			[Buffer waitUntilCompleted];
			[Buffer release];
		}
	}
	else
	{
		check(SubmittedBuffers);
		check(!bWait);
		[SubmittedBuffers addObject: Buffer];
	}
}

void FMetalCommandList::Submit(uint32 Index, uint32 Count)
{
	// Only deferred contexts should call Submit, the immediate context commits directly to the command-queue.
	check(!bImmediate);

	// Command queue takes ownership of the array
	CommandQueue.SubmitCommandBuffers(SubmittedBuffers, Index, Count);
	SubmittedBuffers = [NSMutableArray new];
}
