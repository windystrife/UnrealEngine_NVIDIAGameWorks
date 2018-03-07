// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DecoratedDragDropOp.h"

/** Drag payload type - implement derived types using DRAG_DROP_OPERATOR_TYPE */
class IDragPayload
{
protected:
	/** Friendship required so we can access IsOfType without exposing it */
	friend class FMultipleDataDragOp;

	/** Check if this payload type is the same as the specified type ID */
	virtual bool IsOfTypeImpl(const FString& Type) const { return false; }
};

/** A drag/drop operation that contains multiple types of dragged data (payloads) */
class FMultipleDataDragOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FMultipleDataDragOp, FDecoratedDragDropOp)

	/** Add some more data to this operation's payload */
	void AddPayload(TUniquePtr<IDragPayload> InPayload)
	{
		Payload.Add(MoveTemp(InPayload));
	}

	/** Extract the specified data type from this operation */
	virtual const void* Extract(const FString& Type) const
	{
		for (const auto& Entry : Payload)
		{
			if (Entry->IsOfTypeImpl(T::GetTypeId()))
			{
				return Entry.Get();
			}
		}
		return FDecoratedDragDropOp::Extract(Type);
	}

protected:

	/** The data that we're dragging */
	TArray<TUniquePtr<IDragPayload>> Payload;
};