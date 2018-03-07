// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#define AES_BLOCK_SIZE 16


struct CORE_API FAES
{
	static const uint32 AESBlockSize = 16;

	/**
	 * Encrypts a chunk of data using a specific key
	 *
	 * @param Contents the buffer to encrypt
	 * @param NumBytes the size of the buffer
	 * @param Key a null terminated string that is a 32 byte multiple length
	 */
	static void EncryptData(uint8* Contents, uint32 NumBytes, ANSICHAR* Key);

	/**
	* Encrypts a chunk of data using a specific key
	*
	* @param Contents the buffer to encrypt
	* @param NumBytes the size of the buffer
	* @param Key a byte array that is a 32 byte multiple length
	*/
	static void EncryptData(uint8* Contents, uint32 NumBytes, const uint8* KeyBytes, uint32 NumKeyBytes);

	/**
	 * Decrypts a chunk of data using a specific key
	 *
	 * @param Contents the buffer to encrypt
	 * @param NumBytes the size of the buffer
	 * @param Key a null terminated string that is a 32 byte multiple length
	 */
	static void DecryptData(uint8* Contents, uint32 NumBytes, const ANSICHAR* Key);

	/**
	* Decrypts a chunk of data using a specific key
	*
	* @param Contents the buffer to encrypt
	* @param NumBytes the size of the buffer
	* @param Key a byte array that is a 32 byte multiple length
	*/
	static void DecryptData(uint8* Contents, uint32 NumBytes, const uint8* KeyBytes, uint32 NumKeyBytes);
};
