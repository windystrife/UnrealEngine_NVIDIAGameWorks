// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class Error;

/**
 * Enumerates serialization tokens.
 */
enum class EStructDeserializerBackendTokens
{
	/** End of an array. */
	ArrayEnd,

	/** Beginning of an array. */
	ArrayStart,

	/** A comment. */
	Comment,

	/** An error occurred when reading the token. */
	Error,

	/** No token available. */
	None,

	/** A scalar property. */
	Property,

	/** End of a data structure. */
	StructureEnd,

	/** Beginning of a data structure. */
	StructureStart,
};


/**
 * Interface for UStruct serializer backends.
 */
class IStructDeserializerBackend
{
public:

	/**
	 * Gets the identifier of the current field.
	 *
	 * @return The field's name.
	 */
	virtual const FString& GetCurrentPropertyName() const = 0;

	/**
	 * Gets a debug string for the reader's current state.
	 *
	 * The returned string contains debug information that is relevant to the reader's
	 * serialization format. For example, it could be a line and column number for text
	 * based formats, or a byte offset for binary serialization formats.
	 *
	 * @return The debug string.
	 */
	virtual FString GetDebugString() const = 0;

	/**
	 * Gets the last error message.
	 *
	 * @return Error message.
	 */
	virtual const FString& GetLastErrorMessage() const = 0;

	/**
	 * Reads the next token from the stream.
	 *
	 * @param OutToken Will contain the read token, if any.
	 * @return true if a token was read, false otherwise.
	 */
	virtual bool GetNextToken( EStructDeserializerBackendTokens& OutToken ) = 0;

	/**
	 * Reads the specified property from the stream.
	 *
	 * @param Property The property to read into.
	 * @param Outer The outer property holding the property to read (in case of arrays).
	 * @param Data The buffer that will hold the read data.
	 * @param ArrayIndex An index into the property array (for static arrays).
	 * @return true on success, false otherwise.
	 */
	virtual bool ReadProperty( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex ) = 0;

	/** Skips the array that is currently being read from the stream. */
	virtual void SkipArray() = 0;

	/** Skips the object that is currently being read from the stream. */
	virtual void SkipStructure() = 0;

public:

	/** Virtual destructor. */
	virtual ~IStructDeserializerBackend() { }
};
