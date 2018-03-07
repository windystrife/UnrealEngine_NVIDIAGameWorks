// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IPhoneHome.h: IPhone class handling ET communication.
=============================================================================*/

#import <Foundation/NSString.h>
#import <Foundation/NSData.h>

@interface IPhoneHome : NSObject
{
@private
	id Delegate;
	NSMutableData* ReplyBuffer;
	Boolean WiFiConn;
	Boolean Completed;
	NSMutableString* AppPayload;
	Boolean AppPayloadDone;
	NSMutableString* GamePayload;
	Boolean GamePayloadDone;
	uint64_t CollectStart;
	NSString* UniqueId;
	NSString* GameName;
}

+ (void)queueRequest;

@end



/**
 * Creates a string of hex values for the given data blob, potentially
 * using the SHA1 hash of the data as the string. 
 * The input 0xAB 0xCD 0xEF would return "abcdef"
 *
 * @param InData Data to convert to a string
 * @param bUseDataHash If TRUE, the returned string will be the 20 byte hash as a string
 *
 * @return A string representing hex values as a string
 */
NSString* DataToString(NSData* InData, BOOL bUseDataHash);

