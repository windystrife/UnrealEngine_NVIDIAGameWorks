// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SubversionSourceControlState.h"
#include "ISubversionSourceControlWorker.h"
#include "SubversionSourceControlUtils.h"

class FSubversionConnectWorker : public ISubversionSourceControlWorker
{
public:
	virtual ~FSubversionConnectWorker() {}
	// ISubversionSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FSubversionSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

private:
	/** The root of our working copy */
	FString WorkingCopyRoot;

	/** The root of our repository */
	FString RepositoryRoot;
};

class FSubversionCheckOutWorker : public ISubversionSourceControlWorker
{
public:
	virtual ~FSubversionCheckOutWorker() {}
	// ISubversionSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FSubversionSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FSubversionSourceControlState> OutStates;
};

class FSubversionCheckInWorker : public ISubversionSourceControlWorker
{
public:
	virtual ~FSubversionCheckInWorker() {}
	// ISubversionSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FSubversionSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FSubversionSourceControlState> OutStates;
};

class FSubversionMarkForAddWorker : public ISubversionSourceControlWorker
{
public:
	virtual ~FSubversionMarkForAddWorker() {}
	// ISubversionSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FSubversionSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FSubversionSourceControlState> OutStates;
};

class FSubversionDeleteWorker : public ISubversionSourceControlWorker
{
public:
	virtual ~FSubversionDeleteWorker() {}
	// ISubversionSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FSubversionSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Map of filenames to Subversion state */
	TArray<FSubversionSourceControlState> OutStates;
};

class FSubversionRevertWorker : public ISubversionSourceControlWorker
{
public:
	virtual ~FSubversionRevertWorker() {}
	// ISubversionSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FSubversionSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Map of filenames to Subversion state */
	TArray<FSubversionSourceControlState> OutStates;
};

class FSubversionSyncWorker : public ISubversionSourceControlWorker
{
public:
	virtual ~FSubversionSyncWorker() {}
	// ISubversionSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FSubversionSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Map of filenames to Subversion state */
	TArray<FSubversionSourceControlState> OutStates;
};

class FSubversionUpdateStatusWorker : public ISubversionSourceControlWorker
{
public:
	virtual ~FSubversionUpdateStatusWorker() {}
	// ISubversionSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FSubversionSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FSubversionSourceControlState> OutStates;

	/** Map of filenames to history */
	SubversionSourceControlUtils::FHistoryOutput OutHistory;
};

class FSubversionCopyWorker : public ISubversionSourceControlWorker
{
public:
	virtual ~FSubversionCopyWorker() {}
	// ISubversionSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FSubversionSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Map of filenames to Subversion state */
	TArray<FSubversionSourceControlState> OutStates;
};

class FSubversionResolveWorker : public ISubversionSourceControlWorker
{
public:
	virtual ~FSubversionResolveWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute( class FSubversionSourceControlCommand& InCommand ) override;
	virtual bool UpdateStates() const override;
	
private:
	/** Map of filenames to Subversion state */
	TArray<FSubversionSourceControlState> OutStates;
};

