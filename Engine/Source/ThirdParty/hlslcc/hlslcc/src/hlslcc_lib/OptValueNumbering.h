// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#ifndef OPTVALUENUMBERING_H
#define OPTVALUENUMBERING_H

// Replaces equivalent texture fetches into one variable; return true if any change was made
bool LocalValueNumbering(exec_list* Instructions, _mesa_glsl_parse_state* ParseState);

// Converts complex expressions to simpler ones with more temp variables:
//	x = a * b + c - d * (1 - e * f);
//	t0 = e * f; t1 = 1 - t0; t2 = d * t1; t3 = c - t2; t4 = a * b; x = t4 + t3;
bool ExpandSubexpressions(exec_list* Instructions, _mesa_glsl_parse_state* ParseState);

#endif	// OPTVALUENUMBERING_H
