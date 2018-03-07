// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ISlateMetaData.h"

class FDriverIdMetaData
	: public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FDriverIdMetaData, ISlateMetaData)

	FDriverIdMetaData(FName InId)
		: Id(MoveTemp(InId))
	{ }

	FName Id;
};
