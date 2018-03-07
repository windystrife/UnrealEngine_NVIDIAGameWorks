// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/ITextLayoutMarshaller.h"

/**
 * Base class implementing some common functionality for all text marshallers
 */
class SLATE_API FBaseTextLayoutMarshaller : public ITextLayoutMarshaller
{
public:

	//~ Begin ITextLayoutMarshaller Interface
	virtual bool RequiresLiveUpdate() const override
	{
		return false;
	}

	virtual void MakeDirty() override
	{
		bIsDirty = true;
	}

	virtual void ClearDirty() override
	{
		bIsDirty = false;
	}

	virtual bool IsDirty() const override
	{
		return bIsDirty;
	}
	//~ End ITextLayoutMarshaller Interface

protected:

	FBaseTextLayoutMarshaller()
		: bIsDirty(false)
	{
	}

	virtual ~FBaseTextLayoutMarshaller()
	{
	}

private:

	bool bIsDirty;

};
