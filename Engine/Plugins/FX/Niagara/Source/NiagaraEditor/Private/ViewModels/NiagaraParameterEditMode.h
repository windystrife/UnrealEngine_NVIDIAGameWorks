// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Defines edit modes for parameters in the view model. */
enum class ENiagaraParameterEditMode
{
	/** User can edit all aspects include name, type, and value, and can also add and remove. */
	EditAll,
	/** The user can only edit the value of the parameters. */
	EditValueOnly
};