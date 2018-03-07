// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemPackage.h"

namespace EOnlineKeyValuePairDataType
{
	enum Type
	{
		/** Means the data in the OnlineData value fields should be ignored */
		Empty,
		/** 32 bit integer */
		Int32,
		/** 32 bit unsigned integer */
		UInt32,
		/** 64 bit integer */
		Int64,
		/** 64 bit unsigned integer */
		UInt64,
		/** Double (8 byte) */
		Double,
		/** Unicode string */
		String,
		/** Float (4 byte) */
		Float,
		/** Binary data */
		Blob,
		/** bool data (1 byte) */
		Bool,
		MAX
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineKeyValuePairDataType::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Empty:
			return TEXT("Empty");
		case Int32:
			return TEXT("Int32");
		case UInt32:
			return TEXT("UInt32");
		case Int64:
			return TEXT("Int64");
		case UInt64:
			return TEXT("UInt64");
		case Double:
			return TEXT("Double");
		case String:
			return TEXT("String");
		case Float:
			return TEXT("Float");
		case Blob:
			return TEXT("Blob");
		case Bool:
			return TEXT("Bool");
		default:
			return TEXT("");
		}		
	}
}

/**
 *	Associative container for key value pairs
 */
template<class KeyType, class ValueType>
class FOnlineKeyValuePairs : public TMap<KeyType,ValueType>
{
	typedef TMap<KeyType, ValueType> Super;

public:

	FORCEINLINE FOnlineKeyValuePairs() {}
	FORCEINLINE FOnlineKeyValuePairs(FOnlineKeyValuePairs&& Other) : Super(MoveTemp(Other)) {}
	FORCEINLINE FOnlineKeyValuePairs(const FOnlineKeyValuePairs&  Other) : Super(Other) {}
	FORCEINLINE FOnlineKeyValuePairs& operator=(FOnlineKeyValuePairs&& Other) { Super::operator=(MoveTemp(Other)); return *this; }
	FORCEINLINE FOnlineKeyValuePairs& operator=(const FOnlineKeyValuePairs&  Other) { Super::operator=(Other); return *this; }

};

/**
 *	Container for storing data of variable type
 */
class ONLINESUBSYSTEM_API FVariantData
{

private:

	/** Current data type */
	EOnlineKeyValuePairDataType::Type Type;
	/** Union of all possible types that can be stored */
	union ValueUnion
	{
		bool AsBool;
		int32 AsInt;
		uint32 AsUInt;
		float AsFloat;
		int64 AsInt64;
		uint64 AsUInt64;
		double AsDouble;
		TCHAR* AsTCHAR;
		struct 
		{ 
			uint8* BlobData;
			uint32 BlobSize;
		} AsBlob;

		ValueUnion() { FMemory::Memset( this, 0, sizeof( ValueUnion ) ); }
	} Value;

public:

	/** Constructor */
	FVariantData() :
		Type(EOnlineKeyValuePairDataType::Empty)
	{
	}

	/** Constructor starting with an initialized value/type */
	template<typename ValueType>
	FVariantData(const ValueType& InData)
		: Type(EOnlineKeyValuePairDataType::Empty)
	{
		SetValue(InData);
	}

	/**
	 * Copy constructor. Copies the other into this object
	 *
	 * @param Other the other structure to copy
	 */
	FVariantData(const FVariantData& Other);

	/**
	 * Move constructor. Moves the other into this object
	 *
	 * @param Other the other structure to move
	 */
	FVariantData(FVariantData&& Other);

	/**
	 * Assignment operator. Copies the other into this object
	 *
	 * @param Other the other structure to copy
	 */
	FVariantData& operator=(const FVariantData& Other);

	/**
	 * Move Assignment operator. Moves the other into this object
	 *
	 * @param Other the other structure to move
	 */
	FVariantData& operator=(FVariantData&& Other);

	/**
	 * Cleans up the data to prevent leaks
	 */
	~FVariantData()
	{
		Empty();
	}

	/**
	 * Cleans up the existing data and sets the type to ODT_Empty
	 */
	void Empty();

	/**
	 *	Get the key for this key value pair
	 */
	const EOnlineKeyValuePairDataType::Type GetType() const
	{
		return Type;
	}

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(const FString& InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(const TCHAR* InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(int32 InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(uint32 InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(bool InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(double InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(float InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(const TArray<uint8>& InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param Size the length of the buffer to copy
	 * @param InData the new data to assign
	 */
	void SetValue(uint32 Size, const uint8* InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(int64 InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(uint64 InData);

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(FString& OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(int32& OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(uint32& OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(bool& OutData) const;

	/**
	  Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(int64& OutData) const;

	/**
	  Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(uint64& OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(float& OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(TArray<uint8>& OutData) const;

	/**
	 * Copies the data after verifying the type.
	 * NOTE: Performs a deep copy so you are responsible for freeing the data
	 *
	 * @param OutSize out value that receives the size of the copied data
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(uint32& OutSize,uint8** OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(double& OutData) const;

	/**
	 * Increments the value by the specified amount
	 * 
	 * @param IncBy the amount to increment by
	 */
	template<typename TYPE, EOnlineKeyValuePairDataType::Type ENUM_TYPE>
	FORCEINLINE void Increment(TYPE IncBy)
	{
		checkSlow(Type == EOnlineKeyValuePairDataType::Int32 || Type == EOnlineKeyValuePairDataType::Int64 ||
				  Type == EOnlineKeyValuePairDataType::UInt32 || Type == EOnlineKeyValuePairDataType::UInt64 ||
				  Type == EOnlineKeyValuePairDataType::Float || Type == EOnlineKeyValuePairDataType::Double);
		if (Type == ENUM_TYPE)
		{
			*(TYPE*)&Value += IncBy;
		}
	}

	/**
	 * Decrements the value by the specified amount
	 *
	 * @param DecBy the amount to decrement by
	 */
	template<typename TYPE, EOnlineKeyValuePairDataType::Type ENUM_TYPE>
	FORCEINLINE void Decrement(TYPE DecBy)
	{
		checkSlow(Type == EOnlineKeyValuePairDataType::Int32 || Type == EOnlineKeyValuePairDataType::Int64 ||
				  Type == EOnlineKeyValuePairDataType::UInt32 || Type == EOnlineKeyValuePairDataType::UInt64 ||
				  Type == EOnlineKeyValuePairDataType::Float || Type == EOnlineKeyValuePairDataType::Double);
		if (Type == ENUM_TYPE)
		{
			*(TYPE*)&Value -= DecBy;
		}
	}

	/**
	 * Converts the data into a string representation
	 */
	FString ToString() const;

	/**
	 * Converts the string to the specified type of data for this setting
	 *
	 * @param NewValue the string value to convert
	 *
	 * @return true if it was converted, false otherwise
	 */
	bool FromString(const FString& NewValue);

	/** @return The type as a string */
	const TCHAR* GetTypeString() const
	{
		return EOnlineKeyValuePairDataType::ToString(Type);
	}

	/**
	 * Convert variant data to json object with "type,value" fields
	 *
	 * @return json object representation
	 */
	TSharedRef<class FJsonObject> ToJson() const;

	/**
	 * Convert json object to variant data from "type,value" fields
	 *
	 * @param JsonObject json to convert from
	 * @return true if conversion was successful
	 */
	bool FromJson(const TSharedRef<class FJsonObject>& JsonObject);

	/**
	 * Comparison of two settings data classes
	 *
	 * @param Other the other settings data to compare against
	 *
	 * @return true if they are equal, false otherwise
	 */
	bool operator==(const FVariantData& Other) const;
	bool operator!=(const FVariantData& Other) const;
};

/**
 * Helper class for converting from UStruct to FVariantData and back
 * only very basic flat UStructs with POD types are supported
 */
class ONLINESUBSYSTEM_API FVariantDataConverter
{
public:
	/**
	 * Convert a UStruct into a variant mapping table
	 *
	 * @param StructDefinition layout of the UStruct
	 * @param Struct actual UStruct data
	 * @param OutVariantMap container for outgoing data
	 * @param CheckFlags property must have this flag to be serialized
	 * @param SkipFlags property cannot have this flag to be serialized
	 * 
	 * @return true if it was successful, false otherwise
	 */
	static bool UStructToVariantMap(const UStruct* StructDefinition, const void* Struct, FOnlineKeyValuePairs<FString, FVariantData>& OutVariantMap, int64 CheckFlags, int64 SkipFlags);

	/**
	 * Convert a single UProperty to an FVariantData
	 *
	 * @param Property definition of the property
	 * @param Value actual property data
	 * @param CheckFlags property must have this flag to be serialized
	 * @param SkipFlags property cannot have this flag to be serialized
	 * @param OutVariantData container for outgoing data
	 * 
	 * @return true if it was successful, false otherwise
	 */
	static bool UPropertyToVariantData(UProperty* Property, const void* Value, int64 CheckFlags, int64 SkipFlags, FVariantData& OutVariantData);

public:

	/**
	 * Convert a map of FVariantData elements to a UStruct
	 *
	 * @param VariantMap Input variant data
	 * @param StructDefinition layout of the UStruct
	 * @param OutStruct output container for UStruct data
	 * @param CheckFlags property must have this flag to be serialized
	 * @param SkipFlags property cannot have this flag to be serialized
	 * 
	 * @return true if it was successful, false otherwise
	 */
	static bool VariantMapToUStruct(const FOnlineKeyValuePairs<FString, FVariantData>& VariantMap, const UStruct* StructDefinition, void* OutStruct, int64 CheckFlags, int64 SkipFlags);

	/**
	 * Convert an FVariantData to a UProperty
	 *
	 * @param Variant Input variant data
	 * @param Property definition of the property
	 * @param OutValue outgoing property data container
	 * @param CheckFlags property must have this flag to be serialized
	 * @param SkipFlags property cannot have this flag to be serialized
	 * 
	 * @return true if it was successful, false otherwise
	 */
	static bool VariantDataToUProperty(const FVariantData* Variant, UProperty* Property, void* OutValue, int64 CheckFlags, int64 SkipFlags);
	
private:

	/**
	 * Convert a single UProperty to an FVariantData
	 *
	 * @param Property definition of the property
	 * @param Value actual property data
	 * @param OutVariantData container for outgoing data
	 * @param CheckFlags property must have this flag to be serialized
	 * @param SkipFlags property cannot have this flag to be serialized
	 * 
	 * @return true if it was successful, false otherwise
	 */
	static bool ConvertScalarUPropertyToVariant(UProperty* Property, const void* Value, FVariantData& OutVariantData, int64 CheckFlags, int64 SkipFlags);

	/**
	 * Convert an FVariantData to a UProperty
	 *
	 * @param Variant Input variant data
	 * @param Property definition of the property
	 * @param OutValue outgoing property data container
	 * @param CheckFlags property must have this flag to be serialized
	 * @param SkipFlags property cannot have this flag to be serialized
	 * 
	 * @return true if it was successful, false otherwise
	 */
	static bool ConvertScalarVariantToUProperty(const FVariantData* Variant, UProperty* Property, void* OutValue, int64 CheckFlags, int64 SkipFlags);
};

