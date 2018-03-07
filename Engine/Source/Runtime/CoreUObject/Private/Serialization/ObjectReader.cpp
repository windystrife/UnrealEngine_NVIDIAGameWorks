// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/ObjectReader.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPtr.h"

///////////////////////////////////////////////////////
// FObjectReader

FArchive& FObjectReader::operator<<(FName& N)
{
	NAME_INDEX ComparisonIndex;
	NAME_INDEX DisplayIndex;
	int32 Number;
	ByteOrderSerialize(&ComparisonIndex, sizeof(ComparisonIndex));
	ByteOrderSerialize(&DisplayIndex, sizeof(DisplayIndex));
	ByteOrderSerialize(&Number, sizeof(Number));
	// copy over the name with a name made from the name index and number
	N = FName(ComparisonIndex, DisplayIndex, Number);
	return *this;
}

FArchive& FObjectReader::operator<<(UObject*& Res)
{
	ByteOrderSerialize(&Res, sizeof(Res));
	return *this;
}

FArchive& FObjectReader::operator<<(FLazyObjectPtr& Value)
{
	FArchive& Ar = *this;
	FUniqueObjectGuid ID;
	Ar << ID;

	Value = ID;
	return Ar;
}

FArchive& FObjectReader::operator<<(FSoftObjectPtr& Value)
{
	Value.ResetWeakPtr();
	return *this << Value.GetUniqueID();
}

FArchive& FObjectReader::operator<<(FSoftObjectPath& Value)
{
	Value.SerializePath(*this);
	return *this;
}

FArchive& FObjectReader::operator<<(FWeakObjectPtr& Value)
{
	Value.Serialize(*this);
	return *this;
}

FString FObjectReader::GetArchiveName() const
{
	return TEXT("FObjectReader");
}
