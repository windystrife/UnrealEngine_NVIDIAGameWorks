// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/Timespan.h"

/**
 * Class for encoding/decoding Base64 data (RFC 4648)
 */
class CORE_API FBase64
{
	/**
	 * Base64 supports encoding any 3 uint8 chunk of binary data into a 4 uint8 set of characters from the alphabet below
	 */
	static uint8 EncodingAlphabet[64];
	/**
	 * Used to do a reverse look up from the encoded alphabet to the original 6 bit value
	 */
	static uint8 DecodingAlphabet[256];
	
	/** Ctor hidden on purpose, use static methods only */
	FBase64()
	{
	}

public:

	/**
	 * Encodes the source into a Base64 string
	 *
	 * @param Source the binary payload to stringify
	 * @param Length the length of the payload that needs encoding
	 *
	 * @return the stringified form of the binary data
	 */
	static FString Encode(uint8* Source, uint32 Length);

	/**
	 * Decodes a Base64 string into an array of bytes
	 *
	 * @param Source the stringified data to convert
	 * @param Length the length of the buffer being converted
	 * @param Dest the out buffer that will be filled with the decoded data
	 * @param PadCount the out count of the padding on the orignal buffer (0 to 2)
	 *
	 * @return TRUE if the buffer was decoded, FALSE if it failed to decode
	 */
	static bool Decode(const ANSICHAR* Source, uint32 Length, uint8* Dest, uint32& PadCount);

	/**
	 * Encodes a binary uint8 array into a Base64 string
	 *
	 * @param Source the binary data to convert
	 *
	 * @return a string that encodes the binary data in a way that can be safely transmitted via various Internet protocols
	 */
	static FString Encode(const TArray<uint8>& Source);

	/**
	 * Decodes a Base64 string into an array of bytes
	 *
	 * @param Source the stringified data to convert
	 * @param Dest the out buffer that will be filled with the decoded data
	 */
	static bool Decode(const FString& Source, TArray<uint8>& Dest);

	/**
	 * Encodes a FString into a Base64 string
	 *
	 * @param Source the string data to convert
	 *
	 * @return a string that encodes the binary data in a way that can be safely transmitted via various Internet protocols
	 */
	static FString Encode(const FString& Source);

	/**
	 * Decodes a Base64 string into a FString
	 *
	 * @param Source the stringified data to convert
	 * @param Dest the out buffer that will be filled with the decoded data
	 */
	static bool Decode(const FString& Source, FString& Dest);

	/**
	* Determine the decoded data size for the incoming base64 encoded string
	*
	* @param Source the stringified data to test
	*
	* @return The size in bytes of the decoded data
	*/
	static uint32 GetDecodedDataSize(const FString& Source);
};
