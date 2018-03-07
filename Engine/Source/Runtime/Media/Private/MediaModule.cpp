// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IMediaModule.h"

#include "CoreMinimal.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

#include "IMediaCaptureSupport.h"
#include "IMediaPlayerFactory.h"
#include "IMediaTimeSource.h"
#include "MediaClock.h"
#include "MediaTicker.h"


/**
 * Implements the Media module.
 */
class FMediaModule
	: public IMediaModule
{
public:

	//~ IMediaModule interface

	virtual const TArray<IMediaCaptureSupport*>& GetCaptureSupports() const override
	{
		return CaptureSupports;
	}

	virtual IMediaClock& GetClock() override
	{
		return Clock;
	}

	virtual const TArray<IMediaPlayerFactory*>& GetPlayerFactories() const override
	{
		return PlayerFactories;
	}

	virtual IMediaPlayerFactory* GetPlayerFactory(const FName& FactoryName) const override
	{
		for (IMediaPlayerFactory* Factory : PlayerFactories)
		{
			if (Factory->GetPlayerName() == FactoryName)
			{
				return Factory;
			}
		}

		return nullptr;
	}

	virtual IMediaTicker& GetTicker() override
	{
		return Ticker;
	}

	virtual void LockToTimecode(bool Locked) override
	{
		TimecodeLocked = Locked;
	}

	virtual void RegisterCaptureSupport(IMediaCaptureSupport& Support) override
	{
		CaptureSupports.AddUnique(&Support);
	}

	virtual void RegisterPlayerFactory(IMediaPlayerFactory& Factory) override
	{
		PlayerFactories.AddUnique(&Factory);
	}

	virtual void SetTimeSource(const TSharedPtr<IMediaTimeSource, ESPMode::ThreadSafe>& NewTimeSource) override
	{
		TimeSource = NewTimeSource;
	}

	virtual void TickPostEngine() override
	{
		Clock.TickFetch();
	}

	virtual void TickPostRender() override
	{
		Clock.TickOutput();
	}

	virtual void TickPreEngine() override
	{
		if (TimeSource.IsValid())
		{
			Clock.UpdateTimecode(TimeSource->GetTimecode(), TimecodeLocked);
		}

		Clock.TickInput();
	}

	virtual void TickPreSlate() override
	{
		Clock.TickRender();
	}

	virtual void UnregisterCaptureSupport(IMediaCaptureSupport& Support) override
	{
		CaptureSupports.Remove(&Support);
	}

	virtual void UnregisterPlayerFactory(IMediaPlayerFactory& Factory) override
	{
		PlayerFactories.Remove(&Factory);
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		TickerThread = FRunnableThread::Create(&Ticker, TEXT("FMediaTicker"));
	}

	virtual void ShutdownModule() override
	{
		if (TickerThread != nullptr)
		{
			TickerThread->Kill(true);
			delete TickerThread;
			TickerThread = nullptr;
		}

		CaptureSupports.Reset();
		PlayerFactories.Reset();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

private:

	/** The registered capture device support objects. */
	TArray<IMediaCaptureSupport*> CaptureSupports;

	/** The media clock. */
	FMediaClock Clock;

	/** Time code of the current frame. */
	FTimespan CurrentTimecode;

	/** The registered video player factories. */
	TArray<IMediaPlayerFactory*> PlayerFactories;

	/** High-frequency ticker runnable. */
	FMediaTicker Ticker;

	/** High-frequency ticker thread. */
	FRunnableThread* TickerThread;

	/** Whether media objects should lock to the media clock's time code. */
	bool TimecodeLocked;

	/** The media clock's time source. */
	TSharedPtr<IMediaTimeSource, ESPMode::ThreadSafe> TimeSource;
};


IMPLEMENT_MODULE(FMediaModule, Media);
