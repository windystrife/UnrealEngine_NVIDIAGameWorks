// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

//=============================================================================================================================================================
// This file defines constants used for versioning packages, modules, and various subsystems in UE4. In general, it should not be necessary to include this
// file and access these values directly - they are all wrapped behind the higher-level abstractions in FEngineVersion and the FApp class.
//
// The following concepts are used for versioning in UE4:
//
//  -  The *engine version* defines the explicit major/minor/patch version of the engine, plus the changelist and branch name that it was built from. The 
//     changelist is assumed to be a monotonically increasing number in the current branch, and is used both as a unique identifier and to infer that one engine
//     was later than another. Tagged property serialization in UE4 is tolerant to properties being added or removed, so we always want to prevent an older 
//     build of the engine loading assets created with a newer build, discarding properties which have recently been added, and silently losing data
//     when the asset is saved out. The changelist allows ordering versions in such cases. The engine version is encapsulated by the FEngineVersion class, of 
//     which there are two commonly referenced instances:
//
//         FEngineVersion::Current()        normally uses ENGINE_CURRENT_CL_VERSION for the changelist component, and indicates the code the engine was built 
//                                          from. This is typically only used for diagnostic and display purposes.
//
//         FEngineVersion::CompatibleWith() normally uses ENGINE_COMPATIBLE_CL_VERSION for the changelist component and '0' for the patch component, and 
//                                          indicates the baseline version of the engine that this build maintains strict binary compatibility with. 
//                                          By default, this compatibility extends to assets, executable modules, and any network data transmitted between 
//                                          two builds, and is used when creating patches and hotfixes that can be used interchangably with another build.
//                                          This should be used for versioning in the majority of cases in the engine.
//
//     Both the ENGINE_CURRENT_CL_VERSION and ENGINE_COMPATIBLE_CL_VERSION macros can be updated systemically by build systems using the UpdateLocalVersion 
//     AutomationTool command (as well as the ENGINE_IS_LICENSEE_VERSION and BRANCH_NAME macros).
//
//  -  The *object version* (aka serialization version) is as a monotonically incrementing (but manually updated) integer, and is used to write one-way 
//     upgrade code in custom UObject serialization functions. It is set by the enum in ObjectVersion.h, and is global to the whole engine. This version number 
//     is saved as a raw integer value in package headers, so it cannot be safely reordered or merged between branches. It should ONLY be updated 
//     by Epic, otherwise future engine merges may corrupt content.
//
//  -  The *licensee object version* is provided for licensees to create their own one-way upgrade paths akin to the regular object version. Epic will never
//     add entries to this enumeration. It is defined by the enum in ObjectVersion.h
//
//  -  Any number of *custom object version* objects may be registered to create orthoganal incrementing version numbers similar to the object version and 
//     licensee version enums (see FCustomVersion). Each one is registered with a GUID, ensuring uniqueness and allowing the FArchive to quickly store and 
//     retrieve them without any context of what they represent. Custom versions may be created for individual projects, subsystems, or branches.
//
//  -  The *build version* is an opaque string specific to the product being built, and should be used for identifying the current application 
//     (as opposed to distinct applications built with the same engine version). It is set by the BUILD_VERSION macro, which can be updated using the 
//     UpdateLocalVersion AutomationTool command.
//
//  -  The *network version* and *replay version* are used for versioning the network and replay subsystems, and default to the compatible engine version.
//
//  -  The *engine association* in a .uproject file often takes the appearance of a version number for launcher-installed binary UE4 releases, but may be 
//     other identifiers as well. See ProjectDescriptor.h for a description of how this technique works.
//
// Constants in this file are updated by AutomationTool and UnrealGameSync. Be careful when changing formatting for the submitted version of this file that 
// these tools can still parse it.
//=============================================================================================================================================================

// These numbers define the banner UE4 version, and are the most significant numbers when ordering two engine versions (that is, a 4.12.* version is always 
// newer than a 4.11.* version, regardless of the changelist that it was built with)
#define ENGINE_MAJOR_VERSION	4
#define ENGINE_MINOR_VERSION	18
#define ENGINE_PATCH_VERSION	3

// If set to 1, indicates that this is a licensee build of the engine. For the same major/minor/patch release of the engine, licensee changelists are always 
// considered newer than Epic changelists for engine versions. This follows the assumption that content is developed by Epic leading up to a release, and which 
// point we lock compatibility, and any subsequent licensee modifications to the engine will have a superset of its functionality even if the changelist 
// numbers are lower.
#define ENGINE_IS_LICENSEE_VERSION 0

// The Perforce changelist being compiled. Use this value advisedly; it does not take into account out-of-order commits to engine release branches over 
// development branches, licensee versions, or whether the engine version has been locked to maintain compatibility with a previous engine release. Prefer
// BUILD_VERSION where a unique, product-specific identifier is required, or FEngineVersion::CompatibleWith() where relational comparisons between two 
// versions is required.
#define BUILT_FROM_CHANGELIST 0

// Whether this build is "promoted"; that is, compiled by a build machine (rather than locally) and distributed in binary form. This disables certain features in
// the engine relating to building locally (because they require intermediate files to be available), such as the hot-reload functionality in the editor.
// UnrealGameSync explicitly sets this to zero for local builds.
#define ENGINE_IS_PROMOTED_BUILD  (BUILT_FROM_CHANGELIST > 0)

// The changelist version of the engine. By including a monotonically increasing number in the engine version (and saving it into packages as FEngineVersion), 
// we can prevent newer packages with the same major/minor engine version from being loaded with an older revision of the engine.
#define ENGINE_CURRENT_CL_VERSION  BUILT_FROM_CHANGELIST

// The compatible changelist version of the engine. This number identifies a particular API revision, and is used to determine module and package backwards 
// compatibility. Hotfixes should retain the compatible version of the original release. This define is parsed by the build tools, and should be a number or 
// BUILT_FROM_CHANGELIST, defined in this particular order for each alternative.
#if ENGINE_CURRENT_CL_VERSION > 0
	#if ENGINE_IS_LICENSEE_VERSION
		#define ENGINE_COMPATIBLE_CL_VERSION ENGINE_CURRENT_CL_VERSION
	#else
		#define ENGINE_COMPATIBLE_CL_VERSION 3709383 /* Or hotfix compatibility changelist */
	#endif
#else
	#define ENGINE_COMPATIBLE_CL_VERSION 0
#endif
#if ENGINE_IS_LICENSEE_VERSION == 0 && ENGINE_PATCH_VERSION > 0 && ENGINE_CURRENT_CL_VERSION > 0 && ENGINE_COMPATIBLE_CL_VERSION == ENGINE_CURRENT_CL_VERSION
	#error ENGINE_COMPATIBLE_CL_VERSION must be manually defined for hotfix builds
#endif

// The version number used for determining network compatibility
#define ENGINE_NET_VERSION  ENGINE_COMPATIBLE_CL_VERSION

// The version number used for determining replay compatibility
#define ENGINE_REPLAY_VERSION  ENGINE_NET_VERSION

// The branch that this engine is being built from. When set by UAT, this has the form of a Perforce depot path with forward slashes escaped by plus characters 
// (eg. //UE4/Main -> ++UE4+Main)
#define BRANCH_NAME "++UE4+Release-4.18"

// Macros for encoding strings
#define VERSION_TEXT_2(x) L ## x
#define VERSION_TEXT(x) VERSION_TEXT_2(x)
#define VERSION_STRINGIFY_2(x) VERSION_TEXT(#x)
#define VERSION_STRINGIFY(x) VERSION_STRINGIFY_2(x)

// The BUILD_VERSION string defines an opaque string representing this particular build, and can be customized for a product without modifying the internal engine version.
// This string should be used to uniquely identify a build of the current product, as opposed to something built with this version of the engine.
#define BUILD_VERSION VERSION_TEXT(BRANCH_NAME) VERSION_TEXT("-CL-") VERSION_STRINGIFY(BUILT_FROM_CHANGELIST) 

// Various strings used for engine resources
#define EPIC_COMPANY_NAME  "Epic Games, Inc."
#define EPIC_COPYRIGHT_STRING "Copyright 1998-2017 Epic Games, Inc. All Rights Reserved."
#define EPIC_PRODUCT_NAME "Unreal Engine"
#define EPIC_PRODUCT_IDENTIFIER "UnrealEngine"

// Various strings used for project resources
#ifdef PROJECT_COMPANY_NAME
	#define BUILD_PROJECT_COMPANY_NAME PREPROCESSOR_TO_STRING(PROJECT_COMPANY_NAME)
#else
	#define BUILD_PROJECT_COMPANY_NAME EPIC_COMPANY_NAME
#endif // !PROJECT_COMPANY_NAME
#ifdef PROJECT_COPYRIGHT_STRING
	#define BUILD_PROJECT_COPYRIGHT_STRING PREPROCESSOR_TO_STRING(PROJECT_COPYRIGHT_STRING)
#else
	#define BUILD_PROJECT_COPYRIGHT_STRING EPIC_COPYRIGHT_STRING
#endif // !PROJECT_COPYRIGHT_STRING
#ifdef PROJECT_PRODUCT_NAME
	#define BUILD_PROJECT_PRODUCT_NAME PREPROCESSOR_TO_STRING(PROJECT_PRODUCT_NAME)
#else
	#define BUILD_PROJECT_PRODUCT_NAME EPIC_PRODUCT_NAME
#endif // !PROJECT_PRODUCT_NAME
#ifdef PROJECT_PRODUCT_IDENTIFIER
	#define BUILD_PROJECT_PRODUCT_IDENTIFIER PREPROCESSOR_TO_STRING(PROJECT_PRODUCT_IDENTIFIER)
#else
	#define BUILD_PROJECT_PRODUCT_IDENTIFIER EPIC_PRODUCT_IDENTIFIER
#endif // !PROJECT_PRODUCT_IDENTIFIER

#define ENGINE_VERSION_STRING \
	VERSION_STRINGIFY(ENGINE_MAJOR_VERSION) \
	VERSION_TEXT(".") \
	VERSION_STRINGIFY(ENGINE_MINOR_VERSION) \
	VERSION_TEXT(".") \
	VERSION_STRINGIFY(ENGINE_PATCH_VERSION) \
	VERSION_TEXT("-") \
	VERSION_STRINGIFY(BUILT_FROM_CHANGELIST) \
	VERSION_TEXT("+") \
	VERSION_TEXT(BRANCH_NAME)
