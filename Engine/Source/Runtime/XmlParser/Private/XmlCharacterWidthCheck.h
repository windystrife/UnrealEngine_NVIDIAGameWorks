// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Helper class to determine the character width used by an XML file
 */
class CharacterWidthCheck
{
public:
	/**
	 * Set buffer containing XML aata
	 * @note Buffer must be a least 4 bytes big
	 * @parameter Buffer to check for XML
	 **/
	CharacterWidthCheck(const void* Buffer)
		: TextStart(Buffer)
		, CharacterWidth(1)
		, EntireBuffer(Buffer)
	{
	}

	/**
	 * Check start of buffer and store determined character width
	 * @return Whether the buffer appears to contain XML
	 */
	bool FindCharacterWidth()
	{
		// http://en.wikipedia.org/wiki/Byte_order_mark
		auto BufferBytes = static_cast<const uint8*>(EntireBuffer);
		if (BufferBytes[0] == 0xef && BufferBytes[1] == 0xbb && BufferBytes[2] == 0xbf)
		{
			// Accept UTF-8: we don't attempt to decode it, just pass it through unscathed
			TextStart = &BufferBytes[3];
			return true;
		}

		if (CheckBOMOrOpenTag<uint16>() || CheckBOMOrOpenTag<uint32>())
		{
			return true;
		}

		if ((BufferBytes[0] == '<') && (BufferBytes[1] != 0))
		{
			// Plain 8 bit characters
			TextStart = BufferBytes;
			CharacterWidth = 1;
			return true;
		}

		return false;
	}

	/** Start of actual XML, skipping BOM if present */
	const void* TextStart;

	/** Number of bytes each character in the XML data takes up */
	int CharacterWidth;

private:
	/**
	 * Check for 16 or 32-bit XML data
	 * @return Whether the buffer appears to contain XML
	 */
	template <typename CharType>
	bool CheckBOMOrOpenTag()
	{
		auto FirstWideChar = static_cast<const CharType*>(EntireBuffer);
		if (*FirstWideChar != UNICODE_BOM && *FirstWideChar != '<')
		{
			return false;
		}

		if (*FirstWideChar == UNICODE_BOM)
		{
			TextStart = FirstWideChar + 1;
		}
		CharacterWidth = sizeof(CharType);
		return true;
	}

	/** Provided buffer that may or may not contain XML */
	const void* EntireBuffer;
};

