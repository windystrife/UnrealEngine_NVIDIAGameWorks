// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace BuildPatchServices
{
	/**
	 * Namespace to declares the progress type enum
	 */
	enum class EBuildPatchState : uint32
	{
		// The patch process is waiting for other installs
		Queued = 0,

		// The patch process is initializing
		Initializing,

		// The patch process is enumerating existing staged data
		Resuming,

		// The patch process is downloading patch data
		Downloading,

		// The patch process is installing files
		Installing,

		// The patch process is moving staged files to the install
		MovingToInstall,

		// The patch process is setting up attributes on the build
		SettingAttributes,

		// The patch process is verifying the build
		BuildVerification,

		// The patch process is cleaning temp files
		CleanUp,

		// The patch process is installing prerequisites
		PrerequisitesInstall,

		// A state to catch the UI when progress is 100% but UI still being displayed
		Completed,

		// The process has been set paused
		Paused,

		// Holds the number of states, for array sizes
		NUM_PROGRESS_STATES,
	};

	/**
	 * Returns the string representation of the EBuildPatchState value. Used for analytics and logging only.
	 * @param State - The value.
	 * @return The enum's string value.
	 */
	BUILDPATCHSERVICES_API const FString& StateToString(const EBuildPatchState& State);

	/**
	 * Returns the FText representation of the specified EBuildPatchState value. Used for displaying to the user.
	 * @param State - The error type value.
	 * @return The display text associated with the progress step.
	 */
	BUILDPATCHSERVICES_API const FText& StateToText(const EBuildPatchState& State);
}
