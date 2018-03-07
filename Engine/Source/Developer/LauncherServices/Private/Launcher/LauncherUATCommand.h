// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * class for setting up UAT command arguments for various launcher commands
 */
class FLauncherUATCommand
{
public:
	virtual ~FLauncherUATCommand() { }

	/**
	 * Retrieves the name of the command
	 *
	 * @return name of the command
	 */
	virtual FString GetName() const = 0;

	/**
	 * Retrieves the short description of the command
	 *
	 * @return description of the command
	 */
	virtual FString GetDesc() const = 0;

	/**
	 * Retrieves the arguments used by UAT
	 *
	 * @param ChainState - the state of the task chain
	 *
	 * @return string of UAT arguments
	 */
	virtual FString GetArguments(FLauncherTaskChainState& ChainState) const = 0;

	/**
	 * Retrieves any dependency arguments needed by other commands
	 *
	 * @param ChainState - the state of the task chain
	 *
	 * @return string of UAT arguments
	 */
	virtual FString GetDependencyArguments(FLauncherTaskChainState& ChainState) const
	{
		return TEXT("");
	}

	/**
	 * Retrieves any special arguments for dependent task for the addcmdline
	 *
	 * @param ChainState - the state of the task chain
	 *
	 * @return string of UAT arguments
	 */
	virtual FString GetAdditionalArguments(FLauncherTaskChainState& ChainState) const
	{
		return TEXT("");
	}

	/**
	 * Determines if the command is complete, used for commands that need to wait for responses from games/servers
	 *
	 * @return true if the command is done
	 */
	virtual bool IsComplete() const
	{
		return true;
	}

	/**
	 * Executes any commands before the execution of UAT
	 *
	 * @param ChainState - the state of the task chain
	 *
	 * @return true if the execution was successful
	 */
	virtual bool PreExecute(FLauncherTaskChainState& ChainState)
	{
		return true;
	}

	/**
	 * Executes any commands after the execution of UAT
	 *
	 * @param ChainState - the state of the task chain
	 *
	 * @return true if the execution was successful
	 */
	virtual bool PostExecute(FLauncherTaskChainState& ChainState)
	{
		return true;
	}
};
