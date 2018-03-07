// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	ApplePlatformString.h: Apple platform string classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once

#include "StandardPlatformString.h"
#include "AssertionMacros.h"
#include "OutputDevice.h"
#include "CoreTypes.h"

#if PLATFORM_MAC
#include "Mac/MacSystemIncludes.h"
#elif PLATFORM_IOS
#include "IOS/IOSSystemIncludes.h"
#endif

#ifdef __OBJC__
#import <Foundation/NSString.h>

class FString;

@interface NSString (FString_Extensions)

/**
 * Converts an TCHAR string to an NSString
 */
+ (NSString*) stringWithTCHARString:(const TCHAR*)MyTCHARString;

/**
 * Converts an FString to an NSString
 */
+ (NSString*) stringWithFString:(const FString&)MyFString;

@end
#endif


/**
* Mac string implementation
**/
struct FApplePlatformString : public FStandardPlatformString
{
	// These should be replaced with equivalent Convert and ConvertedLength functions when we properly support variable-length encodings.
/*	static void WideCharToMultiByte(const TCHAR *Source, uint32 LengthWM1, ANSICHAR *Dest, uint32 LengthA)
	{
		CFStringRef CFSource = CFStringCreateWithBytes( kCFAllocatorDefault, ( const uint8 *)Source, LengthWM1 * sizeof( TCHAR ), kCFStringEncodingUTF32LE, false );
		check(CFSource);
		const SIZE_T CFSourceLength = CFStringGetLength( CFSource );
		LengthWM1 = LengthWM1 < CFSourceLength ? LengthWM1 : CFSourceLength;
		CFRange Range = CFRangeMake( 0, LengthWM1 );
		CFIndex ActualLength = 0;
		CFIndex Converted = CFStringGetBytes( CFSource, Range, kCFStringEncodingUTF8, '?', false, NULL, LengthA, &ActualLength );
		check(Converted == LengthWM1 && ActualLength <= LengthA);
		CFStringGetBytes( CFSource, Range, kCFStringEncodingUTF8, '?', false, ( uint8 *)Dest, LengthA, &ActualLength );
		Dest[ActualLength] = 0;
		CFRelease( CFSource );
	}
	static void MultiByteToWideChar(const ANSICHAR *Source, TCHAR *Dest, uint32 LengthM1)
	{
		CFStringRef CFSource = CFStringCreateWithBytes( kCFAllocatorDefault, ( const uint8 *)Source, LengthM1, kCFStringEncodingUTF8, false );
		check(CFSource);
		const SIZE_T CFSourceLength = CFStringGetLength( CFSource );
		LengthM1 = LengthM1 < CFSourceLength ? LengthM1 : CFSourceLength;
		CFRange Range = CFRangeMake( 0, LengthM1 );
		CFIndex ActualLength = 0;
		CFStringGetBytes( CFSource, Range, kCFStringEncodingUTF32LE, '?', false, NULL, LengthM1 * sizeof( TCHAR ), &ActualLength );
		check(ActualLength <= (LengthM1 * sizeof( TCHAR )));
		CFStringGetBytes( CFSource, Range, kCFStringEncodingUTF32LE, '?', false, ( uint8 *)Dest, LengthM1 * sizeof( TCHAR ), &ActualLength );
		uint8* Ptr = (uint8*)Dest;
		Ptr[ActualLength] = Ptr[ActualLength + 1] = Ptr[ActualLength + 2] = Ptr[ActualLength + 3] = 0;
		CFRelease( CFSource );
	}*/
	static void CFStringToTCHAR( CFStringRef CFStr, TCHAR *TChar )
	{
		const SIZE_T Length = CFStringGetLength( CFStr );
		CFRange Range = CFRangeMake( 0, Length );
		CFStringGetBytes( CFStr, Range, kCFStringEncodingUTF32LE, '?', false, ( uint8 *)TChar, Length * sizeof( TCHAR ) + 1, NULL );
		TChar[Length] = 0;
	}
	static CFStringRef TCHARToCFString( const TCHAR *TChar )
	{
		const SIZE_T Length = wcslen( TChar );
		CFStringRef String = CFStringCreateWithBytes( kCFAllocatorDefault, ( const uint8 *)TChar, Length * sizeof( TCHAR ), kCFStringEncodingUTF32LE, false );
		check(String);
		return String;
	}

	static const ANSICHAR* GetEncodingName()
	{
		return "UTF-32LE";
	}

	static const bool IsUnicodeEncoded = true;
};

typedef FApplePlatformString FPlatformString;
