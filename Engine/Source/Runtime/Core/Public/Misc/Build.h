// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


/*--------------------------------------------------------------------------------
	Build configuration coming from UBT, do not modify
--------------------------------------------------------------------------------*/

// Set any configuration not defined by UBT to zero
#ifndef UE_BUILD_DEBUG
	#define UE_BUILD_DEBUG				0
#endif
#ifndef UE_BUILD_DEVELOPMENT
	#define UE_BUILD_DEVELOPMENT		0
#endif
#ifndef UE_BUILD_TEST
	#define UE_BUILD_TEST				0
#endif
#ifndef UE_BUILD_SHIPPING
	#define UE_BUILD_SHIPPING			0
#endif
#ifndef UE_GAME
	#define UE_GAME						0
#endif
#ifndef UE_EDITOR
	#define UE_EDITOR					0
#endif
#ifndef UE_BUILD_SHIPPING_WITH_EDITOR
	#define UE_BUILD_SHIPPING_WITH_EDITOR 0
#endif
#ifndef UE_BUILD_DOCS
	#define UE_BUILD_DOCS				0
#endif

/** 
 *   Whether compiling for dedicated server or not.
 */
#ifndef UE_SERVER
	#define UE_SERVER					0
#endif

// Ensure that we have one, and only one build config coming from UBT
#if UE_BUILD_DEBUG + UE_BUILD_DEVELOPMENT + UE_BUILD_TEST + UE_BUILD_SHIPPING != 1
	#error Exactly one of [UE_BUILD_DEBUG UE_BUILD_DEVELOPMENT UE_BUILD_TEST UE_BUILD_SHIPPING] should be defined to be 1
#endif


/*--------------------------------------------------------------------------------
	Legacy defined we want to make sure don't compile if they came in a merge.
--------------------------------------------------------------------------------*/

#define FINAL_RELEASE_DEBUGCONSOLE	(#)
#define FINAL_RELEASE				(#)
#define SHIPPING_PC_GAME			(#)
#define UE_BUILD_FINAL_RELEASE (#)

/*----------------------------------------------------------------------------
	Mandatory bridge options coming from UBT, do not modify directly!
----------------------------------------------------------------------------*/

/**
 * Whether we are compiling with the editor; must be defined by UBT
 */
#ifndef WITH_EDITOR
	#define WITH_EDITOR	0 // for auto-complete
	#error UBT should always define WITH_EDITOR to be 0 or 1
#endif

/**
 * Whether we are compiling with the engine; must be defined by UBT
 */
#ifndef WITH_ENGINE
	#define WITH_ENGINE	0 // for auto-complete
	#error UBT should always define WITH_ENGINE to be 0 or 1
#endif

/**
 *	Whether we are compiling with developer tools; must be defined by UBT
 */
#ifndef WITH_UNREAL_DEVELOPER_TOOLS
	#define WITH_UNREAL_DEVELOPER_TOOLS		0	// for auto-complete
	#error UBT should always define WITH_UNREAL_DEVELOPER_TOOLS to be 0 or 1
#endif

/**
 *	Whether we are compiling with plugin support; must be defined by UBT
 */
#ifndef WITH_PLUGIN_SUPPORT
	#define WITH_PLUGIN_SUPPORT		0	// for auto-complete
	#error UBT should always define WITH_PLUGIN_SUPPORT to be 0 or 1
#endif

 /** Enable perf counters */
#ifndef WITH_PERFCOUNTERS
	#define WITH_PERFCOUNTERS		0
#endif

/**
 * Unreal Header Tool requires extra data stored in the structure of a few core files. This enables some ifdef hacks to make this work. 
 * Set via UBT, do not modify directly
 */
#ifndef HACK_HEADER_GENERATOR
	#define HACK_HEADER_GENERATOR 0
#endif

/** Whether we are compiling with automation worker functionality.  Note that automation worker defaults to enabled in
    UE_BUILD_TEST configuration, so that it can be used for performance testing on devices */
#ifndef WITH_AUTOMATION_WORKER
	#define WITH_AUTOMATION_WORKER !(UE_BUILD_SHIPPING || HACK_HEADER_GENERATOR)
#endif

/**
 * Whether we want the slimmest possible build of UE4 or not. Don't modify directly but rather change UEBuildConfiguration.cs in UBT.
 */
#ifndef UE_BUILD_MINIMAL
	#define UE_BUILD_MINIMAL 0 // for auto-complete
	#error UBT should always define UE_BUILD_MINIMAL to be 0 or 1
#endif

/**
* Whether we want a monolithic build (no DLLs); must be defined by UBT
*/
#ifndef IS_MONOLITHIC
	#define IS_MONOLITHIC 0 // for auto-complete
	#error UBT should always define IS_MONOLITHIC to be 0 or 1
#endif

/**
* Whether we want a program (shadercompilerworker, fileserver) or a game; must be defined by UBT
*/
#ifndef IS_PROGRAM
	#define IS_PROGRAM 0 // for autocomplete
	#error UBT should always define IS_PROGRAM to be 0 or 1
#endif

/**
* Whether we support hot-reload. Currently requires a non-monolithic build and non-shipping configuration.
*/
#ifndef WITH_HOT_RELOAD
	#define WITH_HOT_RELOAD (!IS_MONOLITHIC && !UE_BUILD_SHIPPING && !UE_BUILD_TEST && !UE_GAME && !UE_SERVER)
#endif

/*----------------------------------------------------------------------------
	Optional bridge options coming from UBT, do not modify directly!
	If UBT doesn't set the value, it is assumed to be 0, and we set that here.
----------------------------------------------------------------------------*/

/**
 * Checks to see if pure virtual has actually been implemented, this is normally run as a CIS process and is set (indirectly) by UBT
 *
 * @see Core.h
 * @see ObjectMacros.h
 **/
#ifndef CHECK_PUREVIRTUALS
	#define CHECK_PUREVIRTUALS 0
#endif

/** Whether to use the null RHI. */
#ifndef USE_NULL_RHI
	#define USE_NULL_RHI 0
#endif

/** If not specified, disable logging in shipping */
#ifndef USE_LOGGING_IN_SHIPPING
	#define USE_LOGGING_IN_SHIPPING 0
#endif

#ifndef USE_CHECKS_IN_SHIPPING
	#define USE_CHECKS_IN_SHIPPING 0
#endif

/*--------------------------------------------------------------------------------
	Basic options that by default depend on the build configuration and platform

	DO_GUARD_SLOW									If true, then checkSlow, checkfSlow and verifySlow are compiled into the executable.
	DO_CHECK										If true, then checkCode, checkf, verify, check, checkNoEntry, checkNoReentry, checkNoRecursion, verifyf, checkf, ensure, ensureAlways, ensureMsgf and ensureAlwaysMsgf are compiled into the executables
	STATS											If true, then the stats system is compiled into the executable.
	ALLOW_DEBUG_FILES								If true, then debug files like screen shots and profiles can be saved from the executable.
	NO_LOGGING										If true, then no logs or text output will be produced

--------------------------------------------------------------------------------*/

#if UE_BUILD_DEBUG
	#define DO_GUARD_SLOW									1
	#define DO_CHECK										1
	#define STATS											(!UE_BUILD_MINIMAL || !WITH_EDITORONLY_DATA || USE_STATS_WITHOUT_ENGINE || USE_MALLOC_PROFILER)
	#define ALLOW_DEBUG_FILES								1
	#define ALLOW_CONSOLE									1
	#define NO_LOGGING										0
#elif UE_BUILD_DEVELOPMENT
	#define DO_GUARD_SLOW									0
	#define DO_CHECK										1
	#define STATS											(!UE_BUILD_MINIMAL || !WITH_EDITORONLY_DATA || USE_STATS_WITHOUT_ENGINE || USE_MALLOC_PROFILER)
	#define ALLOW_DEBUG_FILES								1
	#define ALLOW_CONSOLE									1
	#define NO_LOGGING										0
#elif UE_BUILD_TEST
	#define DO_GUARD_SLOW									0
	#define DO_CHECK										USE_CHECKS_IN_SHIPPING
	#define STATS											(USE_MALLOC_PROFILER)
	#define ALLOW_DEBUG_FILES								1
	#define ALLOW_CONSOLE									1
	#define NO_LOGGING										!USE_LOGGING_IN_SHIPPING
#elif UE_BUILD_SHIPPING
	#if WITH_EDITOR
		#define DO_GUARD_SLOW								0
		#define DO_CHECK									1
		#define STATS										1
		#define ALLOW_DEBUG_FILES							1
		#define ALLOW_CONSOLE								0
		#define NO_LOGGING									0
	#else
		#define DO_GUARD_SLOW								0
		#define DO_CHECK									USE_CHECKS_IN_SHIPPING
		#define STATS										0
		#define ALLOW_DEBUG_FILES							0
		#define ALLOW_CONSOLE								0
		#define NO_LOGGING									!USE_LOGGING_IN_SHIPPING
	#endif
#else
	#error Exactly one of [UE_BUILD_DEBUG UE_BUILD_DEVELOPMENT UE_BUILD_TEST UE_BUILD_SHIPPING] should be defined to be 1
#endif


/**
 * This is a global setting which will turn on logging / checks for things which are
 * considered especially bad for consoles.  Some of the checks are probably useful for PCs also.
 *
 * Throughout the code base there are specific things which dramatically affect performance and/or
 * are good indicators that something is wrong with the content.  These have PERF_ISSUE_FINDER in the
 * comment near the define to turn the individual checks on. 
 *
 * e.g. #if defined(PERF_LOG_DYNAMIC_LOAD_OBJECT) || LOOKING_FOR_PERF_ISSUES
 *
 * If one only cares about DLO, then one can enable the PERF_LOG_DYNAMIC_LOAD_OBJECT define.  Or one can
 * globally turn on all PERF_ISSUE_FINDERS :-)
 *
 **/
#ifndef LOOKING_FOR_PERF_ISSUES
	#define LOOKING_FOR_PERF_ISSUES (0 && !(UE_BUILD_SHIPPING))
#endif

/** Enable the use of the network profiler as long as we are a build that includes stats */
#define USE_NETWORK_PROFILER         STATS

/** Enable UberGraphPersistentFrame feature. It can speed up BP compilation (re-instancing) in editor, but introduce an unnecessary overhead in runtime. */
#define USE_UBER_GRAPH_PERSISTENT_FRAME 1

/** Enable fast calls for event thunks into an event graph that have no parameters  */
#define UE_BLUEPRINT_EVENTGRAPH_FASTCALLS 1

/** Enable perf counters on dedicated servers */
#define USE_SERVER_PERF_COUNTERS ((UE_SERVER || UE_EDITOR) && WITH_PERFCOUNTERS)

#define USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING 1
#define USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS (USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING && 0)

// 0 (default), set this to 1 to get draw events with "TOGGLEDRAWEVENTS" "r.ShowMaterialDrawEvents" and the "ProfileGPU" command working in test
#define ALLOW_PROFILEGPU_IN_TEST 0
// draw events with "TOGGLEDRAWEVENTS" "r.ShowMaterialDrawEvents" (for ProfileGPU, Pix, Razor, RenderDoc, ...) and the "ProfileGPU" command are normally compiled out for TEST and SHIPPING
#define WITH_PROFILEGPU (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || (UE_BUILD_TEST && ALLOW_PROFILEGPU_IN_TEST))
