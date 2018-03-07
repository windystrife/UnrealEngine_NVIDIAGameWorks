// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImgMediaLoaderWork.h"
#include "ImgMediaPrivate.h"

#include "Misc/ScopeLock.h"

#include "IImgMediaLoader.h"
#include "IImgMediaReader.h"


/* FImgMediaLoaderWork structors
 *****************************************************************************/

FImgMediaLoaderWork::FImgMediaLoaderWork(IImgMediaLoader& InOwner, const TSharedRef<IImgMediaReader, ESPMode::ThreadSafe>& InReader)
	: AutoDelete(false)
	, Done(false)
	, FrameNumber(INDEX_NONE)
	, Owner(InOwner)
	, Reader(InReader)
{ }


/* FImgMediaLoaderWork interface
 *****************************************************************************/

void FImgMediaLoaderWork::Initialize(int32 InFrameNumber, const FString& InImagePath)
{
	check(!AutoDelete);

	FrameNumber = InFrameNumber;
	ImagePath = InImagePath;
	Done = false;
}


void FImgMediaLoaderWork::DeleteWhenDone()
{
	FScopeLock Lock(&CriticalSection);

	if (Done)
	{
		delete this;
	}
	else
	{
		AutoDelete = true;
	}
}


/* IQueuedWork interface
 *****************************************************************************/

void FImgMediaLoaderWork::Abandon()
{
	// not supported
}


void FImgMediaLoaderWork::DoThreadedWork()
{
	FImgMediaFrame* Frame;

	if ((FrameNumber == INDEX_NONE) || ImagePath.IsEmpty())
	{
		Frame = nullptr;
	}
	else
	{
		// read the image frame
		Frame = new FImgMediaFrame();

		if (!Reader->ReadFrame(ImagePath, *Frame))
		{
			delete Frame;
			Frame = nullptr;
		}
	}

	// notify owner, or shut down
	FScopeLock Lock(&CriticalSection);

	if (AutoDelete)
	{
		delete this;
	}
	else
	{
		Done = true;
		Owner.NotifyWorkComplete(*this, FrameNumber, MakeShareable(Frame));
	}
}
