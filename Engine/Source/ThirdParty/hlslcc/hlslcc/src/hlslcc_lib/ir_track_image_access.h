// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Determine access modes on image* variables and update the ir
 * @param ir - IR instructions.
 */
void TrackImageAccess(struct exec_list* ir, _mesa_glsl_parse_state* InParseState);
