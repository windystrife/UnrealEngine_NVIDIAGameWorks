// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// Metadata used by the control rig system
namespace ControlRigMetadata
{
	// Metadata usable in UPROPERTY for customizing the behavior when displaying the property in a property panel
	enum
	{
		/// [PropertyMetadata] Indicates that the property should be exposed as an input for an animation controller
		AnimationInput,

		/// [PropertyMetadata] Indicates that the property should be exposed as an output for an animation controller
		AnimationOutput,
	};
}