// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

namespace BuildPatchServices
{
	/**
	 * Defines the interface to be implemented by any controllable class.
	 */
	class IControllable
	{
	public:
		virtual ~IControllable() {}

		/**
		 * Sets whether the class should pause current activities and wait.
		 * @param bIsPaused     True if the class should pause.
		 */
		virtual void SetPaused(bool bIsPaused) = 0;

		/**
		 * Called to instruct the class to cease all activity, and perform any shutdown.
		 */
		virtual void Abort() = 0;
	};
}
