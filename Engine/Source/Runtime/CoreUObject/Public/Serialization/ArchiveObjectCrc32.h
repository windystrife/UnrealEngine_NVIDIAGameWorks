// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/MemoryWriter.h"
#include "Containers/Queue.h"

/*----------------------------------------------------------------------------
FArchiveObjectCrc32
----------------------------------------------------------------------------*/

/**
* Calculates a checksum on an object's serialized data stream.
*/
class COREUOBJECT_API FArchiveObjectCrc32 : public FArchiveUObject
{
public:
	/**
	* Default constructor.
	*/
	FArchiveObjectCrc32();

	//~ Begin FArchive Interface
	virtual void Serialize(void* Data, int64 Length);
	virtual FArchive& operator<<(class FName& Name);
	virtual FArchive& operator<<(class UObject*& Object);
	virtual FString GetArchiveName() const { return TEXT("FArchiveObjectCrc32"); }
	//~ End FArchive Interface

	/**
	* Serialize the given object, calculate and return its checksum.
	*/
	uint32 Crc32(UObject* Object, uint32 CRC = 0);

protected:
	/** Return if object was already serialized */
	virtual bool CustomSerialize(class UObject* Object) { return false; }

	/** Internal byte array used for serialization */
	TArray<uint8> SerializedObjectData;
	/** Internal archive used for serialization */
	FMemoryWriter MemoryWriter;
	/** Internal queue of object references awaiting serialization */
	TQueue<UObject*> ObjectsToSerialize;
	/** Internal currently serialized object */
	const UObject* ObjectBeingSerialized;
	/** Internal root object */
	const UObject* RootObject;
};
