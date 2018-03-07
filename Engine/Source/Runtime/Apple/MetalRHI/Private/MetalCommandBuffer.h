// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>
#include "MetalResources.h"
#include "MetalShaderResources.h"

/**
 * EMetalDebugCommandType: Types of command recorded in our debug command-buffer wrapper.
 */
enum EMetalDebugCommandType
{
	EMetalDebugCommandTypeRenderEncoder,
	EMetalDebugCommandTypeComputeEncoder,
	EMetalDebugCommandTypeBlitEncoder,
	EMetalDebugCommandTypeEndEncoder,
    EMetalDebugCommandTypePipeline,
	EMetalDebugCommandTypeDraw,
	EMetalDebugCommandTypeDispatch,
	EMetalDebugCommandTypeBlit,
	EMetalDebugCommandTypeSignpost,
	EMetalDebugCommandTypePushGroup,
	EMetalDebugCommandTypePopGroup,
	EMetalDebugCommandTypeInvalid
};

/**
 * EMetalDebugLevel: Level of Metal debug features to be enabled.
 */
enum EMetalDebugLevel
{
	EMetalDebugLevelOff,
	EMetalDebugLevelLogDebugGroups,
	EMetalDebugLevelFastValidation,
	EMetalDebugLevelTrackResources,
	EMetalDebugLevelResetOnBind,
	EMetalDebugLevelValidation,
	EMetalDebugLevelLogOperations,
	EMetalDebugLevelConditionalSubmit,
	EMetalDebugLevelWaitForComplete
};

NS_ASSUME_NONNULL_BEGIN
/**
 * FMetalDebugCommand: The data recorded for each command in the debug command-buffer wrapper.
 */
struct FMetalDebugCommand
{
	NSString* Label;
	EMetalDebugCommandType Type;
	MTLRenderPassDescriptor* PassDesc;
};

/**
 * Simpler NSObject extension that provides for an associated object to track debug groups in a command-buffer.
 * This doesn't interfere with objc_msgSend invocation so doesn't cost as much on the CPU.
 */
@interface NSObject (IMetalDebugGroupAssociation)
@property (nonatomic, strong) NSMutableArray<NSString*>* debugGroups;
@end

@protocol IMetalCommandBufferExtensions
@property (readonly) CFTimeInterval kernelStartTime;
@property (readonly) CFTimeInterval kernelEndTime;
@property (readonly) CFTimeInterval GPUStartTime;
@property (readonly) CFTimeInterval GPUEndTime;
@end

/**
 * FMetalDebugCommandBuffer: Wrapper around id<MTLCommandBuffer> that records information about commands.
 * This allows reporting of substantially more information in debug modes which can be especially helpful 
 * when debugging GPU command-buffer failures.
 */
@interface FMetalDebugCommandBuffer : FApplePlatformObject<MTLCommandBuffer>
{
	NSMutableArray<NSString*>* DebugGroup;
	NSString* ActiveEncoder;
	TSet<id<MTLResource>> Resources;
	TSet<id> States;
	@public
	TArray<FMetalDebugCommand*> DebugCommands;
    EMetalDebugLevel DebugLevel;
	id<MTLBuffer> DebugInfoBuffer;
};

/** The wrapped native command-buffer for which we collect debug information. */
@property (readonly) id<MTLCommandBuffer> InnerBuffer;

/** Initialise the wrapper with the provided command-buffer. */
-(id)initWithCommandBuffer:(id<MTLCommandBuffer>)Buffer;

/** Add the resource to be tracked in this command-buffer so we can validate lifetime on failure. */
-(void) trackResource:(id<MTLResource>)Resource;
/** Add the state to be tracked in this command-buffer so we can validate lifetime on failure. */
-(void) trackState:(id)State;

/** Record a bgein render encoder command. */
-(void) beginRenderCommandEncoder:(NSString*)Label withDescriptor:(MTLRenderPassDescriptor*)Desc;
/** Record a begin compute encoder command. */
-(void) beginComputeCommandEncoder:(NSString*)Label;
/** Record a begin blit encoder command. */
-(void) beginBlitCommandEncoder:(NSString*)Label;
/** Record an end encoder command. */
-(void) endCommandEncoder;

/** Record a pipeline state set. */
-(void) setPipeline:(NSString*)Desc;
/** Record a draw command. */
-(void) draw:(NSString*)Desc;
/** Record a dispatch command. */
-(void) dispatch:(NSString*)Desc;
/** Record a blit command. */
-(void) blit:(NSString*)Desc;

/** Record an signpost command. */
-(void) insertDebugSignpost:(NSString*)Label;
/** Record a push debug group command. */
-(void) pushDebugGroup:(NSString*)Group;
/** Record a pop debug group command. */
-(void) popDebugGroup;

@end
NS_ASSUME_NONNULL_END
