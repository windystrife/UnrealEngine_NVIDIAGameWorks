// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class Error;
class IStructSerializerBackend;

/**
 * Enumerates policies for serializing null values.
 */
enum class EStructSerializerNullValuePolicies
{
	/** Do not serialize NULL values. */
	Ignore,
		
	/** Serialize NULL values. */
	Serialize
};


/**
 * Enumerates policies for serializing object reference loops.
 */
enum class EStructSerializerReferenceLoopPolicies
{
	/** Circular references generate an error. */
	Error,
		
	/** Ignore circular references. */
	Ignore,
		
	/** Serialize circular references. */
	Serialize
};


/**
 * Structure for UStruct serialization policies.
 */
struct FStructSerializerPolicies
{
	/** Holds the policy for null values. */
	EStructSerializerNullValuePolicies NullValues;

	/** Holds the policy for reference loops. */
	EStructSerializerReferenceLoopPolicies ReferenceLoops;

	/** Predicate for performing advanced filtering of struct properties. 
		If set, the predicate should return true for all properties it wishes to include in the output.
	 */
	TFunction<bool (const UProperty* /*CurrentProp*/, const UProperty* /*ParentProp*/)> PropertyFilter;

	/** Default constructor. */
	FStructSerializerPolicies()
		: NullValues(EStructSerializerNullValuePolicies::Serialize)
		, ReferenceLoops(EStructSerializerReferenceLoopPolicies::Ignore)
		, PropertyFilter()
	{ }
};


/**
 * Implements a static class that can serialize UStruct based types.
 *
 * This class implements the basic functionality for the serialization of UStructs, such as
 * iterating a structure's properties and writing property values. The actual writing of
 * serialized output data is performed by serialization backends, which allows this class
 * to remain serialization format agnostic.
 *
 * The serializer's behavior can be customized with serialization policies. This allows for
 * control over how to handle null values, circular references and other edge cases.
 */
class FStructSerializer
{
public:

	/**
	 * Serializes a given data structure of the specified type using the specified policy.
	 *
	 * @param Struct The data structure to serialize.
	 * @param TypeInfo The structure's type information.
	 * @param Backend The serialization backend to use.
	 * @param Policies The serialization policies to use.
	 * @see Deserialize
	 */
	SERIALIZATION_API static void Serialize( const void* Struct, UStruct& TypeInfo, IStructSerializerBackend& Backend, const FStructSerializerPolicies& Policies );

	/**
	 * Serializes a given data structure of the specified type using the default policy.
	 *
	 * @param Struct The data structure to serialize.
	 * @param TypeInfo The structure's type information.
	 * @param Backend The serialization backend to use.
	 * @see Deserialize
	 */
	static void Serialize( const void* Struct, UStruct& TypeInfo, IStructSerializerBackend& Backend )
	{
		Serialize(Struct, TypeInfo, Backend, FStructSerializerPolicies());
	}

public:

	/**
	 * Serializes a given USTRUCT to a string using the default policy.
	 *
	 * @param StructType The type of the struct to serialize.
	 * @param Struct The struct to serialize.
	 * @param Backend The serialization backend to use.
	 * @return A string holding the serialized object.
	 */
	template<typename StructType>
	static void Serialize( const StructType& Struct, IStructSerializerBackend& Backend )
	{
		Serialize(&Struct, *Struct.StaticStruct(), Backend, FStructSerializerPolicies());
	}

	/**
	 * Serializes a given USTRUCT to a string using the specified policy.
	 *
	 * @param StructType The type of the struct to serialize.
	 * @param Struct The struct to serialize.
	 * @param Backend The serialization backend to use.
	 * @param Policies The serialization policies to use.
	 * @return A string holding the serialized object.
	 */
	template<typename StructType>
	static void Serialize( const StructType& Struct, IStructSerializerBackend& Backend, const FStructSerializerPolicies& Policies )
	{
		Serialize(&Struct, *Struct.StaticStruct(), Backend, Policies);
	}
};
