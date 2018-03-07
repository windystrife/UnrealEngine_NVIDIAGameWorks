// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"

#define IS_STANDALONE_APPLICATION 1

#import "IPhoneHome.h"
#import <Foundation/NSURL.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <SystemConfiguration/SCNetworkReachability.h>
#import <UIKit/UIDevice.h>
#import "IPhoneAsyncTask.h"
#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <TargetConditionals.h>

#include <mach/mach.h>
#include <mach/mach_time.h>
#include "IPhoneObjCWrapper.h"

#include <time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdlib.h>

extern uint64_t GStartupFreeMem;
extern uint64_t GStartupUsedMem;

static Boolean doFlagsIndicateWiFi(SCNetworkReachabilityFlags flags);

// Increment if you make non-compatible changes to the phone home xml (not including the game specific block)
static const int GPhoneHomeXMLVer = 2;

// URL that phone home stats are sent to.
// Internally, we send it to "et.epicgames.com". If you set up your own service, define it's URL here.
#if !defined(PHONE_HOME_URL)
#define PHONE_HOME_URL L"tempuri.org"
#endif


static Boolean GIsPhoneHomeInFlight = FALSE;

static UBOOL GotMOTDCallback(id UserData);
static UBOOL MOTDFailedCallback(id UserData);
static UBOOL CollectStatsCallback(id UserData);

extern int main(int argc, char *argv[]);

/**
 * Creates a string of hex values for the given array
 * The input 0xAB 0xCD 0xEF would return "abcdef"
 *
 * @param BytesToConvert Data to convert to a string
 * @param Length Number of bytes in the BytesToConvert array
 *
 * @return A string representing hex values as a string
 */
NSString* CArrayToHexNSString(BYTE* BytesToConvert, INT Length)
{	
	// allocate space for 2 characters per byte
	ANSICHAR* StringBuffer = (ANSICHAR*)appMalloc(sizeof(ANSICHAR) * (Length * 2 + 1));
	StringBuffer[Length * 2] = '\0';

	// convert into StringBuffer
	static const ANSICHAR ALPHA_CHARS[] = "0123456789abcdef";
	for (INT ByteIndex = 0; ByteIndex < Length; ByteIndex++)
	{
		StringBuffer[2 * ByteIndex + 0] = ALPHA_CHARS[(BytesToConvert[ByteIndex] >> 4) & 0x0F];
		StringBuffer[2 * ByteIndex + 1] = ALPHA_CHARS[BytesToConvert[ByteIndex] & 0x0F];
	}

	// make an NSString from the buffer (autoreleased)
	NSString* DataString = [NSString stringWithCString:StringBuffer encoding:NSASCIIStringEncoding];
	
	// clean up 
	appFree(StringBuffer);
	return DataString;
}

/** Checks for the encryption info command in the MACH-O header.
    Returns 0 if the segment is found and mode is non-zero
	Returns 1 if the segment is found and mode is zero (no encryption; either a dev version or a pirated version)
	Returns 2 if the segment wasn't found (something went wrong)
  */
inline int CheckHeader()
{
#if !TARGET_IPHONE_SIMULATOR
	mach_header* ExecutableHeader;
	Dl_info Info;

	if ((dladdr((const void*)main, &Info) == 0) || (Info.dli_fbase == NULL))
	{
		// Problem finding entry point
	}
	else
	{
		ExecutableHeader = (mach_header*)Info.dli_fbase;

		// Run through the load commands looking for an encryption info command
		uint8_t* CommandPtr = (uint8_t*)(ExecutableHeader + 1);
		for (int i = 0; i < ExecutableHeader->ncmds; ++i)
		{
			// Check for an encryption info command
			load_command* LC = (struct load_command*)CommandPtr;
			if (LC->cmd == LC_ENCRYPTION_INFO)
			{
				// If the encryption mode is 0, it means no encryption.  This isn't expected from an app store app
				// (it is 0 during development, but we catch that via other means).
				encryption_info_command* EIC = (encryption_info_command*)LC;
				return (EIC->cryptid == 0) ? 1 : 0;
			}

			// Advance to the next load command
			CommandPtr += LC->cmdsize;
		}
	}
#endif

	// Unable to find the load command (or run on the simulator)
	return 2;
}

/**
  This function scans the root directory and hashes all filenames / directory names found.
  It is used as part of the piracy checks, and has nothing to do with DLC.
  It is only named that because symbols in MM files aren't always stripped

  @return A string containing the hex representation of the SHA-1 hash of the sorted list of filenames in the main app directory (peer to Info.plist)
*/
inline NSString* ScanForDLC()
{
	// use the API to retrieve where the application is stored
	NSString* SearchDir = [[NSBundle mainBundle] resourcePath];
	
	// Get the list of files in the main directory of the application bundle and sort them
	NSArray* DirectoryContents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:SearchDir error:nil];
	NSArray* SortedContents = [DirectoryContents sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

	// Run through the list of files, hashing each filename / directory name
	FSHA1 ShaHasher;
	for (NSString* Filename in SortedContents)
	{
		NSString* LastPartOfFilename = [Filename lastPathComponent];
		int NumChars = [LastPartOfFilename length];

		char AssertWCharSizeIs4[(sizeof(wchar_t) == 4) ? 0 : -1];
		(void)AssertWCharSizeIs4;

		const void* FilenameChars = [LastPartOfFilename cStringUsingEncoding:NSUTF32StringEncoding];

		// Hash this filename
		ShaHasher.Update((const BYTE *)(FilenameChars), NumChars * sizeof(wchar_t));
	}

	//@TODO: Open the plist and hash it's contents (as a replacement for the SigningIdenitity check, which is pretty useless anyways)

	// Add any additional anti-tamper file detections here (take no action on them, just check and modify the hash)

	// Finalize the hash and convert it to a string
	BYTE FilenameListHash[20];
	ShaHasher.Final();
	ShaHasher.GetHash(FilenameListHash);

	NSString* HashOfRootFiles = CArrayToHexNSString(FilenameListHash, 20);
	return HashOfRootFiles;
}

@implementation IPhoneHome

- (void)didSucceed:(NSString*)motd
{
	// if, somehow, this gets called after we got our data, ignore it
	if (Completed)
	{
		return;
	}
	Completed = YES;

	// reset our "since last upload" stats
	IPhoneSaveUserSettingU64("IPhoneHome::NumSurveyFailsSinceLast", 0);

	// record last upload time
	IPhoneSaveUserSettingU64("IPhoneHome::LastSuccess", (uint64_t)time(NULL));

	// reset our "since last submission stats"
	IPhoneSaveUserSettingU64("IPhoneHome::NumInvocations", 0);
	IPhoneSaveUserSettingU64("IPhoneHome::NumCrashes", 0);
	IPhoneSaveUserSettingU64("IPhoneHome::NumMemoryWarnings", 0);
	IPhoneSaveUserSettingU64("IPhoneHome::AppPlaytimeSecs", 0);

	// notify the UPhoneHome object
	// NOTE: needs to be called in the game thread.
	IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
	AsyncTask.UserData = motd;
	AsyncTask.GameThreadCallbackFn = GotMOTDCallback;
	[AsyncTask FinishedTask];
}

- (void)didFail
{
	// if, somehow, this gets called after we got our data, ignore it
	if (Completed)
	{
		return;
	}
	Completed = YES;

	// increment failure count
	IPhoneIncrementUserSettingU64("IPhoneHome::TotalSurveyFails");
	IPhoneIncrementUserSettingU64("IPhoneHome::NumSurveyFailsSinceLast");

	// record last failure time so we don't retry to soon
	IPhoneSaveUserSettingU64("IPhoneHome::LastFailure", (uint64_t)time(NULL));

	// for now, just log
	debugf(TEXT("MOTD GET failed."));

	// notify the UPhoneHome object
	// NOTE: needs to be called in the game thread.
	IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
	AsyncTask.GameThreadCallbackFn = MOTDFailedCallback;
	[AsyncTask FinishedTask];
}

- (void)doSendPayload
{
	if (!AppPayloadDone || !GamePayloadDone)
	{
		// wait for the last one to finish
		return;
	}

	// check for an encryption segment with encryption mode = decrypted, or no encryption segment at all
	int DevResult = CheckHeader(); // 0 is valid, 1 is pirated, 2 is missing / unknown LC

	// combine the payloads
	NSMutableString* Payload = [NSMutableString stringWithString:@"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"];
	[Payload appendFormat:@"<phone-home ver=\"%d\" dev=\"%d\">\n", GPhoneHomeXMLVer, DevResult];
	[Payload appendString:AppPayload];
	[Payload appendString:GamePayload];

	// end timing and record
	mach_timebase_info_data_t TimeBase;
	mach_timebase_info(&TimeBase);
	double dts = 1e-9 * (double)(TimeBase.numer * (mach_absolute_time() - CollectStart)) / (double)TimeBase.denom;
	[Payload appendFormat:@"<meta collect-time-secs=\"%.3lf\"/>\n", dts];

	// terminate and convert to data
	[Payload appendString:@"\n</phone-home>"];
	NSData* Data = [Payload dataUsingEncoding:NSUTF8StringEncoding];

	// convert it to a URL (with OS4 weak reference workaround to the class)
	NSString* UrlStr = [NSString stringWithFormat:@"http://%s/PhoneHome-1?uid=%@&game=%@%s", 
		PHONE_HOME_URL,//TCHAR_TO_ANSI(PHONE_HOME_URL), 
		UniqueId, 
		[GameName stringByAddingPercentEscapesUsingEncoding:NSASCIIStringEncoding], 
		WiFiConn ? "" : "&wwan=1"];
	Class NSURLClass = NSClassFromString(@"NSURL");
	NSURL* Url = [NSURLClass URLWithString:UrlStr];
	
	// encode Payload and create a POST HTTP request to send it
	NSMutableURLRequest * Request = [NSMutableURLRequest requestWithURL:Url];
	[Request setHTTPMethod:@"POST"];
	[Request setHTTPBody:Data];
	[Request setValue:@"application/xml" forHTTPHeaderField:@"Content-Type"];
	[Request setValue:[NSString stringWithFormat:@"%lu", (unsigned long)[Data length]] forHTTPHeaderField:@"Content-Length"];

	// assume it's a failure and record the time now so we don't retry to soon (but don't increment failure count)
	// it's possible we'll quit before the delegate tells us we've sent but it will still go through
	IPhoneSaveUserSettingU64("IPhoneHome::LastFailure", (uint64_t)time(NULL));
	
	// send the request asynchronously
	NSURLConnection* Conn = [NSURLConnection connectionWithRequest:Request delegate:self];
	if (Conn == nil)
	{
		// record that we failed
		[self didFail];
		
		// autorelease ourselves
		[self autorelease];
		return;
	}
}

-(void)appThreadGameStatsDone
{
	// call doSendPayload
	GamePayloadDone = YES;
	[self doSendPayload];
}

/**
 * This method is called in the game thread, it is safe to invoke UPhoneHome from here. 
 * We schedule ourselves back into the app thread when the payload gathering is complete.
 */
- (void)gameThreadCollectStats
{
	GameName = [[[NSBundle mainBundle] infoDictionary] objectForKey: @"CFBundleIdentifier"];
	if (GameName == nil)
	{
		GameName = @"unknown";
	}

	NSString* PackagingMode = [[[NSBundle mainBundle] infoDictionary] objectForKey: @"EpicPackagingMode"];
	if (PackagingMode == nil)
	{
		PackagingMode = @"unknown";
	}

	NSString* AppVersionString = [[[NSBundle mainBundle] infoDictionary] objectForKey: @"EpicAppVersion"];
	if (AppVersionString == nil)
	{
		AppVersionString = @"unknown";
	}

	// Do another piracy check (file hash)
	NSString* DLCHash = ScanForDLC();

	INT EngineVersion = GEngineVersion;

	// report the game name and engine version
	[GamePayload appendFormat:@"<game name=\"%@\" engver=\"%d\" pkgmode=\"%@\" appver=\"%@\" dlc=\"%@\">\n", 
		[[GameName stringByReplacingOccurrencesOfString: @"&" withString: @"&amp;"] 
			stringByReplacingOccurrencesOfString: @"\"" withString: @"&quot;"], 
		EngineVersion,
		[[PackagingMode stringByReplacingOccurrencesOfString: @"&" withString: @"&amp;"] 
			stringByReplacingOccurrencesOfString: @"\"" withString: @"&quot;"],
		[[AppVersionString stringByReplacingOccurrencesOfString: @"&" withString: @"&amp;"] 
			stringByReplacingOccurrencesOfString: @"\"" withString: @"&quot;"],
		DLCHash
		];

	// implement game-specifc stats here (UPhoneHome)

	// close the game tag
	[GamePayload appendString:@"</game>\n"];

	// queue appThreadGameStatsDone on the app thread
	[self performSelectorOnMainThread:@selector(appThreadGameStatsDone) withObject:nil waitUntilDone:NO];
}

- (void)getUUID
{
	// get the UUID
	UniqueId = [[NSUUID UUID] UUIDString];
	[UniqueId retain];
}

- (void)appThreadReachabilityDone
{	
	// add stats about upload connectivity
	uint64_t TotalSurveys = IPhoneLoadUserSettingU64("IPhoneHome::TotalSurveys");
	uint64_t TotalSurveyFails = IPhoneLoadUserSettingU64("IPhoneHome::TotalSurveyFails");
	uint64_t TotalSurveyFailsSinceLast = IPhoneLoadUserSettingU64("IPhoneHome::NumSurveyFailsSinceLast");
	[AppPayload appendFormat:@"<upload total-attempts=\"%llu\" total-failures=\"%llu\" recent-failures=\"%llu\"/>\n", TotalSurveys, TotalSurveyFails, TotalSurveyFailsSinceLast];

	// get the device hardware type
	size_t hwtype_size;
	sysctlbyname("hw.machine", NULL, &hwtype_size, NULL, 0);
	char* hwtype = (char*)malloc(hwtype_size+1);
	hwtype[hwtype_size] = '\0';
	sysctlbyname("hw.machine", hwtype, &hwtype_size, NULL, 0);

	// get and hash the unique ID
	[self getUUID];

	// get other device info (os, device type, etc)
	UIDevice* dev = [UIDevice currentDevice];
	[AppPayload appendFormat:@"<device type=\"%@\" model=\"%s\" os-ver=\"%@\"/>\n", [dev model], hwtype, [dev systemVersion]];
	free(hwtype); // release hardware string

	// get locale (language settings) info
	NSLocale* loc = [NSLocale currentLocale];
	NSString* vc = [loc objectForKey:NSLocaleVariantCode];
	[AppPayload appendFormat:@"<locale country=\"%@\" language=\"%@\" variant=\"%@\"/>\n", [loc objectForKey:NSLocaleCountryCode], [loc objectForKey:NSLocaleLanguageCode], vc == NULL ? @"" : vc];

	// report the memory we had to work with when we started
	[AppPayload appendFormat:@"<startmem free-bytes=\"%llu\" used-bytes=\"%llu\"/>\n", GStartupFreeMem, GStartupUsedMem];

	// report application stats
	[AppPayload appendFormat:@"<app invokes=\"%llu\" crashes=\"%llu\" memwarns=\"%llu\" playtime-secs=\"%llu\"/>\n", 
		IPhoneLoadUserSettingU64("IPhoneHome::NumInvocations"),
		IPhoneLoadUserSettingU64("IPhoneHome::NumCrashes"),
		IPhoneLoadUserSettingU64("IPhoneHome::NumMemoryWarnings"),
		IPhoneLoadUserSettingU64("IPhoneHome::AppPlaytimeSecs")];

	// send off the payload (asynchronously of course)
	AppPayloadDone = YES;
	[self doSendPayload];
}

- (void)collectPayload
{
	// begin timing
	CollectStart = mach_absolute_time(); 

	// increment total surveys attempted.
	IPhoneIncrementUserSettingU64("IPhoneHome::TotalSurveys");

	// queue some stats collection in the game thread
	IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
	AsyncTask.UserData = self;
	AsyncTask.GameThreadCallbackFn = CollectStatsCallback;
	[AsyncTask FinishedTask];

	// meanwhile check for WiFi connection
	WiFiConn = NO;
	SCNetworkReachabilityRef Ref = SCNetworkReachabilityCreateWithName(NULL, PHONE_HOME_URL);//TCHAR_TO_ANSI(PHONE_HOME_URL));
	if (Ref != NULL)
	{
		SCNetworkReachabilityFlags Flags;
		if (SCNetworkReachabilityGetFlags(Ref, &Flags))
		{
			WiFiConn = doFlagsIndicateWiFi(Flags);
		}
		CFRelease(Ref);
	}

	// TODO: do the WiFi check asynchronously and then call back to this fn
	[self appThreadReachabilityDone];
}

- (id)init
{
	if (self = [super init])
	{
		Completed = NO;
		AppPayload = [[NSMutableString alloc] init];
		AppPayloadDone = NO;
		GamePayload = [[NSMutableString alloc] init];
		GamePayloadDone = NO;
		ReplyBuffer = [[NSMutableData alloc] init];
		UniqueId = nil;
		GameName = nil;
	}
	
	return self;
}

+ (Boolean)shouldPhoneHome
{
#if !IS_STANDALONE_APPLICATION
	// check the command line for overrides
	if (ParseParam(appCmdLine(), TEXT("nophonehome")))
	{
		return NO;
	}
	if (ParseParam(appCmdLine(), TEXT("forcephonehome")))
	{
		return YES;
	}
#endif

	// get last success and last failure times
	time_t Now = time(NULL);
	time_t LastSuccess = (time_t)IPhoneLoadUserSettingU64("IPhoneHome::LastSuccess");
	time_t LastFailure = (time_t)IPhoneLoadUserSettingU64("IPhoneHome::LastFailure");

	// make sure at least 1 day has passed since last_success
	if (LastSuccess != 0 && Now - LastSuccess < 60*60*24)
	{
		return NO;
	}

	// make sure at least 5 minutes have passed since last_failure
	if (LastFailure != 0 && Now - LastFailure < 60*5)
	{
		return NO;
	}
	
	return YES;
}

+ (void)queueRequest
{
	// if there's one in flight, never phone home
	// NOTE: this isn't intended to be MT-safe as queueRequest makes no such guarantee either
	if (GIsPhoneHomeInFlight)
	{
		return;
	}

	// enforce throttling
	if (![IPhoneHome shouldPhoneHome])
	{
		return;
	}

	// mark that a phone home is in flight because we may get called again while async callbacks are processing
	GIsPhoneHomeInFlight = TRUE;

	// NOTE: intentionally keeping the refcount at 1 here since IPhoneHome releases itself
	// if you need to keep a copy, please still call retain as usual.
	IPhoneHome* Iph = [[IPhoneHome alloc] init];

	// send the payload
	// NOTE: this completes in several asynchronous callback steps
	[Iph collectPayload];
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
	// accumulate the response data
	[ReplyBuffer appendData:data];
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSHTTPURLResponse *)response
{
	if ([response statusCode] == 200)
	{
		// convert MOTD to a string
		NSString* motd = [[NSString alloc] initWithData:ReplyBuffer encoding:NSUTF8StringEncoding];

		// record that we succeeded
		[self didSucceed: motd];
	}

	// reset the ReplyBuffer (in case we get redirects)
	[ReplyBuffer setLength:0];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
	// record that we failed
	// IF we have already reported success, this will be ignored
	[self didFail];
	
	// release ourselves
	[self autorelease];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	// record that we failed
	[self didFail];
	
	// release ourselves
	[self autorelease];
}

// This code is paraphrased from the Apple Reachability sample 
// http://developer.apple.com/iphone/library/samplecode/Reachability/Introduction/Intro.html
Boolean doFlagsIndicateWiFi(SCNetworkReachabilityFlags flags)
{   
	if ((flags & kSCNetworkReachabilityFlagsReachable) == 0)
	{
		// if target host is not reachable
		return NO;
	}

	Boolean isWiFi = NO;
    if ((flags & kSCNetworkReachabilityFlagsConnectionRequired) == 0)
    {
        // if target host is reachable and no connection is required
        //  then we'll assume (for now) that you are on Wi-Fi
		isWiFi = YES;
    }
    
    if ((((flags & kSCNetworkReachabilityFlagsConnectionOnDemand ) != 0) ||
        (flags & kSCNetworkReachabilityFlagsConnectionOnTraffic) != 0))
    {
        // ... and the connection is on-demand (or on-traffic) if the
        //     calling application is using the CFSocketStream or higher APIs

        if ((flags & kSCNetworkReachabilityFlagsInterventionRequired) == 0)
        {
            // ... and no [user] intervention is needed
			isWiFi = YES;
        }
    }
    
    if ((flags & kSCNetworkReachabilityFlagsIsWWAN) == kSCNetworkReachabilityFlagsIsWWAN)
    {
        // ... but WWAN connections are OK if the calling application
        //     is using the CFNetwork (CFSocketStream?) APIs.
        isWiFi = NO;
    }
    return isWiFi;
}

- (void)dealloc
{	
	[ReplyBuffer release];
	[AppPayload release];
	[GamePayload release];
	[UniqueId release];
	
	[super dealloc];
}

@end

static UBOOL GotMOTDCallback(id UserData)
{
	//NSString* motd = (NSString*)UserData;
	
	// log the motd
	//TCHAR* MotdStr = UTF8_TO_TCHAR([motd cStringUsingEncoding:NSUTF8StringEncoding]);
	//debugf(TEXT("MOTD BEGIN\n%s\nMOTD END"), MotdStr);

	// let the game implementation do something with it or write your own C++ implementation here
	//UPhoneHome::GotMOTD(MotdStr);
	
	GIsPhoneHomeInFlight = FALSE;

	return TRUE;
}

static UBOOL MOTDFailedCallback(id UserData)
{
	// let the game implementation know we failed or write your own C++ implementation here
	//UPhoneHome::MOTDFailed();
	
	GIsPhoneHomeInFlight = FALSE;

	return TRUE;
}

static UBOOL CollectStatsCallback(id UserData)
{
	IPhoneHome* iph = (IPhoneHome*)UserData;
	[iph gameThreadCollectStats];

	return TRUE;
}

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
NSString* DataToString(NSData* InData, BOOL bUseDataHash)
{
	BYTE* BytesToConvert;
	INT Length;

	// hash the data to a stack buffer if needed
	BYTE DataHash[20];
	if (bUseDataHash)
	{
		FSHA1::HashBuffer([InData bytes], [InData length], DataHash);
		BytesToConvert = DataHash;
		Length = 20;
	}
	// otherwise, use the bytes in the InData
	else
	{
		BytesToConvert = (BYTE*)[InData bytes];
		Length = [InData length];
	}

	return CArrayToHexNSString(BytesToConvert, Length);
}
