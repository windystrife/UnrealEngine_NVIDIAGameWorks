// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class Error;

/**
 * Json (JavaScript Object Notation) is a lightweight data-interchange format.
 * Information on how it works can be found here: http://www.json.org/.
 * This code was written from scratch with only the Json spec as a guide.
 *
 * In order to use Json effectively, you need to be familiar with the Object/Value
 * hierarchy, and you should use the FJsonObject class and FJsonValue subclasses.
 */

/**
 * Represents all the types a Json Value can be.
 */
enum class EJson
{
	None,
	Null,
	String,
	Number,
	Boolean,
	Array,
	Object
};


enum class EJsonToken
{
	None,
	Comma,
	CurlyOpen,
	CurlyClose,
	SquareOpen,
	SquareClose,
	Colon,
	String,

	// short values
	Number,
	True,
	False,
	Null,

	Identifier
};

FORCEINLINE bool EJsonToken_IsShortValue(EJsonToken Token)
{
	return Token >= EJsonToken::Number && Token <= EJsonToken::Null;
}

enum class EJsonNotation
{
	ObjectStart,
	ObjectEnd,
	ArrayStart,
	ArrayEnd,
	Boolean,
	String,
	Number,
	Null,
	Error
};
