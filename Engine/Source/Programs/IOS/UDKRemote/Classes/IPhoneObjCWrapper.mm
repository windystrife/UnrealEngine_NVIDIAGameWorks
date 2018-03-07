// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 IPhoneObjCWrapper.mm: iPhone wrapper for making ObjC calls from C++ code
 =============================================================================*/

#include "IPhoneObjCWrapper.h"
#include <unistd.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSPathUtilities.h>
#import <mach/mach_host.h>
#import <mach/task.h>


/**
 * Get the path to the .app where file loading occurs
 * 
 * @param AppDir [out] Return path for the application directory that is the root of file loading
 * @param MaxLen Size of AppDir buffer
 */
void IPhoneGetApplicationDirectory( char *AppDir, int MaxLen ) 
{
	// use the API to retrieve where the application is stored
	NSString *dir = [NSSearchPathForDirectoriesInDomains(NSAllApplicationsDirectory, NSUserDomainMask, YES) objectAtIndex: 0];
	[dir getCString: AppDir maxLength: MaxLen encoding: NSASCIIStringEncoding];
}

/**
 * Get the path to the user document directory where file saving occurs
 * 
 * @param DocDir [out] Return path for the application directory that is the root of file saving
 * @param MaxLen Size of DocDir buffer
 */
void IPhoneGetDocumentDirectory( char *DocDir, int MaxLen ) 
{
	// use the API to retrieve where the application is stored
	NSString *dir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex: 0];
	[dir getCString: DocDir maxLength: MaxLen encoding: NSASCIIStringEncoding];
}

/**
 * Creates a directory (must be in the Documents directory
 *
 * @param Directory Path to create
 * @param bMakeTree If true, it will create intermediate directory
 *
 * @return true if successful)
 */
bool IPhoneCreateDirectory(char* Directory, bool bMakeTree)
{
	NSAutoreleasePool *autoreleasepool = [[NSAutoreleasePool alloc] init];
	
	// convert to iPhone string
	NSFileManager* FileManager = [NSFileManager defaultManager];
	NSString* NSPath = [FileManager stringWithFileSystemRepresentation:Directory length:strlen(Directory)];

	// create the directory (with optional tree)
	BOOL Result = [FileManager createDirectoryAtPath:NSPath withIntermediateDirectories:bMakeTree attributes:nil error:nil];
	
	[autoreleasepool release];
	
	return Result;
}

/* These are defined in mach/shared_memory_server.h, which does not exist when
   compiling for device */
#ifndef SHARED_TEXT_REGION_SIZE
#define SHARED_TEXT_REGION_SIZE  0x08000000
#endif

#ifndef SHARED_DATA_REGION_SIZE
#define SHARED_DATA_REGION_SIZE 0x08000000
#endif

/**
 * Retrieve current memory information (for just this task)
 *
 * @param FreeMemory Amount of free memory in bytes
 * @param UsedMemory Amount of used memory in bytes
 */
void IPhoneGetTaskMemoryInfo(uint64_t& ResidentSize, uint64_t& VirtualSize)
{
	// Get stats about the current task
	task_basic_info Stats;
	mach_msg_type_number_t Size = TASK_BASIC_INFO_COUNT;

	task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&Stats, &Size);

	// Return the most interesting stuff
	ResidentSize = Stats.resident_size;
	VirtualSize = Stats.virtual_size - SHARED_TEXT_REGION_SIZE - SHARED_DATA_REGION_SIZE;
}


 
/**
 * Retrieve current memory information (for the entire device, not limited to our process)
 *
 * @param FreeMemory Amount of free memory in bytes
 * @param UsedMemory Amount of used memory in bytes
 */
void IPhoneGetPhysicalMemoryInfo( uint64_t & FreeMemory, uint64_t & UsedMemory )
{
	// get stats about the host (mem is in pages)
	vm_statistics Stats;
	mach_msg_type_number_t Size = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &Size);
	
	// get the page size
	vm_size_t PageSize;
	host_page_size(mach_host_self(), &PageSize);
	
	// combine to get free memory!
	FreeMemory = Stats.free_count * PageSize;
	// and used memory
	UsedMemory = ( Stats.active_count + Stats.inactive_count + Stats.wire_count ) * PageSize;
}


/**
 * Launch a URL for the given Tag
 *
 * @param Tag String describing what URL to launch
 */
void IPhoneLaunchURL(const char* Tag)
{
	Class NSURLClass = NSClassFromString(@"NSURL");

	// create the URL
	NSString* URLString = [NSString stringWithFormat:@"http://www.epicgames.com/%s/", Tag];
	NSURL* URL = [NSURLClass URLWithString:URLString];
	
	// launch it!
	[[UIApplication sharedApplication] openURL:URL];
}


/**
 * Save a key/value string pair to the user's settings
 *
 * @param Key Name of the setting
 * @param Value Value to save to the setting
 */
void IPhoneSaveUserSetting(const char* Key, const char* Value)
{
	// get the user settings object
    NSUserDefaults *Defaults = [NSUserDefaults standardUserDefaults];

	// convert input strings to NSStrings
	NSString* KeyString = [NSString stringWithUTF8String:Key];
	NSString* ValueString = [NSString stringWithUTF8String:Value];

	// set the setting
	[Defaults setObject:ValueString forKey:KeyString];
	[Defaults synchronize];
}


/**
 * Load a value from the user's settings for the given Key
 *
 * @param Key Name of the setting
 * @param Value [out] String to put the loaded value into
 * @param MaxValueLen Size of the OutValue string
 */
void IPhoneLoadUserSetting(const char* Key, char* OutValue, int MaxValueLen)
{
	// get the user settings object
    NSUserDefaults *Defaults = [NSUserDefaults standardUserDefaults];

	// convert input string to NSStrings
	NSString* KeyString = [NSString stringWithUTF8String:Key];

	// load the value
	NSString* ValueString = [Defaults stringForKey:KeyString];

	// make sure we have a non-null string
	if (ValueString == nil)
	{
		ValueString = @"";
	}

	// convert it to a C string to pass back
	[ValueString getCString: OutValue maxLength: MaxValueLen encoding: NSASCIIStringEncoding];
}


/**
 * Convenience wrapper around IPhoneLoadUserSetting for integers
 * NOTE: strtoull returns 0 if it can't parse the int (this will be the default when we first load)
 * 
 * @param Name Name of the setting
 * @return Setting value as uint64_t
 */
uint64_t IPhoneLoadUserSettingU64(const char* Name)
{
	char Buf[32];
	IPhoneLoadUserSetting(Name, Buf, sizeof(Buf)-1); 
	return strtoull(Buf, NULL, 0);
}


/**
 * Convenience wrapper around IPhoneSaveUserSetting for integers
 * 
 * @param Name The name of the setting
 * @param Value The new setting value
 */
void IPhoneSaveUserSettingU64(const char* Name, uint64_t Value)
{
	char Buf[32];
	snprintf(Buf, sizeof(Buf)-1, "%llu", Value);
	IPhoneSaveUserSetting(Name, Buf); 
}


/**
 * Convenience wrapper around IPhoneLoadUserSettingU64 and IPhoneSaveUserSettingU64
 *
 * @param Name The name of the setting
 */
void IPhoneIncrementUserSettingU64(const char* Name, uint64_t By)
{
	uint64_t Value = IPhoneLoadUserSettingU64(Name);
	IPhoneSaveUserSettingU64(Name, Value + By);
}

/**
 * @return the type of device we are currently running on
 */
EIOSDevice IPhoneGetDeviceType()
{
	// easy way to check for IPad
	if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
	{
		return IOS_IPad;
	}

	// check for OS 4
	if([[UIScreen mainScreen] respondsToSelector:@selector(scale)]) 
	{
		if ([[UIScreen mainScreen] scale] > 1.0f)
		{
			if( [[[UIDevice currentDevice] model] isEqualToString:@"iPhone"] )
			{
				return IOS_IPhone4;
			}
			else
			{
				return IOS_IPodTouch4;
			}
		}
	}

	// anything left over is IPhone3GS
	return IOS_IPhone3GS;
}


/**
 * Gets the language the user has selected
 *
 * @param Language String to receive the Language into
 * @param MaxLen Size of the Language string
 */
void IPhoneGetUserLanguage(char* Language, int MaxLen)
{
	// get the set of languages
	NSArray* Languages = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleLanguages"];

	// get the language the user would like (first element is selected)
	NSString* PreferredLanguage = [Languages objectAtIndex:0];

	// convert to C string to pass back
	[PreferredLanguage getCString:Language maxLength:MaxLen encoding:NSASCIIStringEncoding];
}

/**
 * Retrieves the string value for the given key in the application's bundle (ie Info.plist)
 *
 * @param Key The key to look up
 * @param Value A buffer to fill in with the value
 * @param MaxLen Size of the Value string
 */
bool IPhoneGetBundleStringValue(const char* Key, char* Value, int MaxLen)
{
	// look up the key in the main bundle
	id BundleValue = [[[NSBundle mainBundle] infoDictionary] objectForKey:[NSString stringWithUTF8String:Key]];

	// make sure it's a string object
	if (BundleValue && [BundleValue isKindOfClass:NSString.class])
	{
		// if it is, convert it to a c string
		[BundleValue getCString:Value maxLength:MaxLen encoding:NSASCIIStringEncoding];
		
		return true;
	}

	return false;
}