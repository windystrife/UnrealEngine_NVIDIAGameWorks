// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/ObjectWriter.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPtr.h"

///////////////////////////////////////////////////////
// FObjectWriter

FArchive& FObjectWriter::operator<<( class FName& N )
{
	NAME_INDEX ComparisonIndex = N.GetComparisonIndex();
	NAME_INDEX DisplayIndex = N.GetDisplayIndex();
	int32 Number = N.GetNumber();
	ByteOrderSerialize(&ComparisonIndex, sizeof(ComparisonIndex));
	ByteOrderSerialize(&DisplayIndex, sizeof(DisplayIndex));
	ByteOrderSerialize(&Number, sizeof(Number));
	return *this;
}

FArchive& FObjectWriter::operator<<( class UObject*& Res )
{
	ByteOrderSerialize(&Res, sizeof(Res));
	return *this;
}

FArchive& FObjectWriter::operator<<(FLazyObjectPtr& Value)
{
	FUniqueObjectGuid ID = Value.GetUniqueID();
	return *this << ID;
}

FArchive& FObjectWriter::operator<<(FSoftObjectPtr& Value)
{
	Value.ResetWeakPtr();
	return *this << Value.GetUniqueID();
}

FArchive& FObjectWriter::operator<<(FSoftObjectPath& Value)
{
	Value.SerializePath(*this);
	return *this;
}

FArchive& FObjectWriter::operator<<(FWeakObjectPtr& Value)
{
	Value.Serialize(*this);
	return *this;
}

FString FObjectWriter::GetArchiveName() const
{
	return TEXT("FObjectWriter");
}
