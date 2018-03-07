// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 IPhoneObjCWrapper.h: iPhone wrapper for making ObjC calls from C++ code
 =============================================================================*/


#ifndef __IPHONE_OBJC_WRAPPER_H__
#define __IPHONE_OBJC_WRAPPER_H__

#import <stdint.h>

#define IPHONE_PATH_MAX 1024

/**
 * Possible iOS devices
 */
enum EIOSDevice
{
	IOS_IPhone3GS,
	IOS_IPhone4,
	IOS_IPad,
	IOS_IPodTouch4,
	IOS_Unknown,
};

/**
 * Get the path to the .app where file loading occurs
 * 
 * @param AppDir [out] Return path for the application directory that is the root of file loading
 * @param MaxLen Size of AppDir buffer
 */
void IPhoneGetApplicationDirectory( char *AppDir, int MaxLen );

/**
 * Get the path to the user document directory where file saving occurs
 * 
 * @param DocDir [out] Return path for the application directory that is the root of file saving
 * @param MaxLen Size of DocDir buffer
 */
void IPhoneGetDocumentDirectory( char *DocDir, int MaxLen );

/**
 * Creates a directory (must be in the Documents directory
 *
 * @param Directory Path to create
 * @param bMakeTree If true, it will create intermediate directory
 *
 * @return true if successful)
 */
bool IPhoneCreateDirectory(char* Directory, bool bMakeTree);

/**
 * Retrieve current memory information (for just this task)
 *
 * @param FreeMemory Amount of free memory in bytes
 * @param UsedMemory Amount of used memory in bytes
 */
void IPhoneGetTaskMemoryInfo(uint64_t& ResidentSize, uint64_t& VirtualSize);

/**
 * Retrieve current memory information (for the entire device, not limited to our process)
 *
 * @param FreeMemory Amount of free memory in bytes
 * @param UsedMemory Amount of used memory in bytes
 */
void IPhoneGetPhysicalMemoryInfo( uint64_t & FreeMemory, uint64_t & UsedMemory );

/**
 * Enables or disables the view autorotation when the user rotates the view
 */
void IPhoneSetRotationEnabled(int bEnabled);

/**
 * Launch a URL for the given Tag
 *
 * @param Tag String describing what URL to launch
 */
void IPhoneLaunchURL(const char* Tag);


/**
 * Save a key/value string pair to the user's settings
 *
 * @param Key Name of the setting
 * @param Value Value to save to the setting
 */
void IPhoneSaveUserSetting(const char* Key, const char* Value);

/**
 * Load a value from the user's settings for the given Key
 *
 * @param Key Name of the setting
 * @param Value [out] String to put the loaded value into
 * @param MaxValueLen Size of the OutValue string
 */
void IPhoneLoadUserSetting(const char* Key, char* OutValue, int MaxValueLen);

/**
 * Convenience wrapper around IPhoneLoadUserSetting for integers
 * NOTE: strtoull returns 0 if it can't parse the int (this will be the default when we first load)
 * 
 * @param Name Name of the setting
 * @return Setting value as uint64_t
 */
uint64_t IPhoneLoadUserSettingU64(const char* Name);

/**
 * Convenience wrapper around IPhoneSaveUserSetting for integers
 * 
 * @param Name The name of the setting
 * @param Value The new setting value
 */
void IPhoneSaveUserSettingU64(const char* Name, uint64_t Value);

/**
 * Convenience wrapper around IPhoneLoadUserSettingU64 and IPhoneSaveUserSettingU64
 *
 * @param Name The name of the setting
 * @param By How much to increment the setting by
 */
void IPhoneIncrementUserSettingU64(const char* Name, uint64_t By = 1);

/**
 * @return the type of device we are currently running on
 */
EIOSDevice IPhoneGetDeviceType();

/**
 * Gets the language the user has selected
 *
 * @param Language String to receive the Language into
 * @param MaxLen Size of the Language string
 */
void IPhoneGetUserLanguage(char* Language, int MaxLen);

/**
 * Retrieves the string value for the given key in the application's bundle (ie Info.plist)
 *
 * @param Key The key to look up
 * @param Value A buffer to fill in with the value
 * @param MaxLen Size of the Value string
 *
 * @return true if Key was found in the bundle, and it was had a string value to return
 */
bool IPhoneGetBundleStringValue(const char* Key, char* Value, int MaxLen);

#endif