// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IAutomationDriver.h"
#include "WaitUntil.h"
#include "Misc/Timespan.h"
#include "InputCoreTypes.h"

class FAutomatedApplication;
class IElementLocator;
class IAsyncDriverSequence;
class IAsyncDriverElement;
class IAsyncDriverElementCollection;
class FDriverConfiguration;
class FModifierKeysState;

class FAsyncAutomationDriverFactory;

class FAsyncAutomationDriver
	: public IAsyncAutomationDriver
	, public TSharedFromThis<FAsyncAutomationDriver, ESPMode::ThreadSafe>
{
public:

	virtual ~FAsyncAutomationDriver();

	virtual TAsyncResult<bool> Wait(FTimespan Timespan) override;
	virtual TAsyncResult<bool> Wait(const FDriverWaitDelegate& Delegate) override;

	virtual TSharedRef<IAsyncDriverSequence, ESPMode::ThreadSafe> CreateSequence() override;

	virtual TAsyncResult<FVector2D> GetCursorPosition() const override;

	virtual TAsyncResult<FModifierKeysState> GetModifierKeys() const override;

	virtual TSharedRef<IAsyncDriverElement, ESPMode::ThreadSafe> FindElement(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator) override;

	virtual TSharedRef<IAsyncDriverElementCollection, ESPMode::ThreadSafe> FindElements(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator) override;

	virtual TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe> GetConfiguration() const override;

public:

	void TrackPress(int32 KeyCode, int32 CharCode);
	void TrackPress(EMouseButtons::Type Button);

	void TrackRelease(int32 KeyCode, int32 CharCode);
	void TrackRelease(EMouseButtons::Type Button);

	bool IsPressed(int32 KeyCode, int32 CharCode) const;
	bool IsPressed(EMouseButtons::Type Button) const;

	int32 ProcessCharacterForControlCodes(int32 CharCode) const;

private:

	FAsyncAutomationDriver(
		FAutomatedApplication* const InApplication,
		const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe>& InConfiguration)
		: Application(InApplication)
		, Configuration(InConfiguration)
	{ }

	void Initialize();

private:

	FAutomatedApplication* const Application;
	const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe> Configuration;

	TSet<FKey> PressedModifiers;
	TSet<int32> PressedKeys;
	TSet<int32> PressedChars;
	TSet<EMouseButtons::Type> PressedButtons;
	TMap<int32, int32> CharactersToControlCodes;

	friend FAsyncAutomationDriverFactory;
};

class FAsyncAutomationDriverFactory
{
public:

	static TSharedRef<FAsyncAutomationDriver, ESPMode::ThreadSafe> Create(
		FAutomatedApplication* const AutomatedApplication);

	static TSharedRef<FAsyncAutomationDriver, ESPMode::ThreadSafe> Create(
		FAutomatedApplication* const AutomatedApplication,
		const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe>& Configuration);
};


class FAutomationDriverFactory;

class FAutomationDriver
	: public IAutomationDriver
	, public TSharedFromThis<FAutomationDriver, ESPMode::ThreadSafe>
{
public:

	virtual ~FAutomationDriver();

	virtual bool Wait(FTimespan Timespan) override;
	virtual bool Wait(const FDriverWaitDelegate& Delegate) override;

	virtual TSharedRef<IDriverSequence, ESPMode::ThreadSafe> CreateSequence() override;

	virtual FVector2D GetCursorPosition() const override;

	virtual FModifierKeysState GetModifierKeys() const override;

	virtual TSharedRef<IDriverElement, ESPMode::ThreadSafe> FindElement(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator) override;

	virtual TSharedRef<IDriverElementCollection, ESPMode::ThreadSafe> FindElements(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator) override;

	virtual TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe> GetConfiguration() const override;

public:

	void TrackPress(int32 KeyCode, int32 CharCode);
	void TrackPress(EMouseButtons::Type Button);

	void TrackRelease(int32 KeyCode, int32 CharCode);
	void TrackRelease(EMouseButtons::Type Button);

	bool IsPressed(int32 KeyCode, int32 CharCode) const;
	bool IsPressed(EMouseButtons::Type Button) const;

private:

	FAutomationDriver(
		FAutomatedApplication* const InApplication,
		const TSharedRef<FAsyncAutomationDriver, ESPMode::ThreadSafe>& InAsyncDriver)
		: Application(InApplication)
		, AsyncDriver(InAsyncDriver)
	{ }

private:

	FAutomatedApplication* const Application;
	const TSharedRef<FAsyncAutomationDriver, ESPMode::ThreadSafe> AsyncDriver;

	friend FAutomationDriverFactory;
};

class FAutomationDriverFactory
{
public:

	static TSharedRef<FAutomationDriver, ESPMode::ThreadSafe> Create(
		FAutomatedApplication* const AutomatedApplication);

	static TSharedRef<FAutomationDriver, ESPMode::ThreadSafe> Create(
		FAutomatedApplication* const AutomatedApplication,
		const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe>& Configuration);
};
