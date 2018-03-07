// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

class FDragDropEvent;

/** @return the content being dragged if it matched the 'OperationType'; invalid Ptr otherwise. */
template<typename OperationType>
TSharedPtr<OperationType> FDragDropEvent::GetOperationAs() const
{
	if (Content.IsValid() && Content->IsOfType<OperationType>())
	{
		return StaticCastSharedPtr<OperationType>(Content);
	}
	else
	{
		return TSharedPtr<OperationType>();
	}
}
