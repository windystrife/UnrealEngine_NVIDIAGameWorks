// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PackageUtilityWorkers.cpp: Declarations for structs and classes used by package commandlets.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

/**
 * These bit flag values represent the different types of information that can be reported about a package
 */
enum EPackageInfoFlags
{
	PKGINFO_None			=0x00,
	PKGINFO_Names			=0x01,
	PKGINFO_Imports			=0x02,
	PKGINFO_Exports			=0x04,
	PKGINFO_Compact			=0x08,
	PKGINFO_Depends			=0x20,
	PKGINFO_Paths			=0x40,
	PKGINFO_Thumbs			=0x80,
	PKGINFO_Lazy			=0x100,
	PKGINFO_AssetRegistry	=0x200,
	PKGINFO_Text			=0x400,

	PKGINFO_All			= PKGINFO_Names|PKGINFO_Imports|PKGINFO_Exports|PKGINFO_Depends|PKGINFO_Paths|PKGINFO_Thumbs|PKGINFO_Lazy|PKGINFO_AssetRegistry|PKGINFO_Text,
};

/**
 * Base for classes which generate output for the PkgInfo commandlet
 */
struct FPkgInfoReporter
{
	/** Constructors */
	FPkgInfoReporter() 
	: InfoFlags(PKGINFO_None), bHideOffsets(false), Linker(NULL), PackageCount(0)
	{}
	FPkgInfoReporter( uint32 InInfoFlags, bool bInHideOffsets, FLinkerLoad* InLinker=NULL )
	: InfoFlags(InInfoFlags), bHideOffsets(bInHideOffsets), Linker(InLinker), PackageCount(0)
	{}
	FPkgInfoReporter( const FPkgInfoReporter& Other )
	: InfoFlags(Other.InfoFlags), bHideOffsets(Other.bHideOffsets), Linker(Other.Linker), PackageCount(Other.PackageCount)
	{}

	/** Destructor */
	virtual ~FPkgInfoReporter() {}

	/**
	 * Performs the actual work - generates a report containing information about the linker.
	 *
	 * @param	InLinker	if specified, changes this reporter's Linker before generating the report.
	 */
	virtual void GeneratePackageReport( class FLinkerLoad* InLinker=NULL )=0;

	/**
	 * Changes the target linker for this reporter.  Useful when generating reports for multiple packages.
	 */
	void SetLinker( class FLinkerLoad* NewLinker )
	{
		Linker = NewLinker;
	}

protected:
	/**
	 * A bitmask of PKGINFO_ flags that determine the information that is included in the report.
	 */
	uint32 InfoFlags;

	/**
	 * Determines whether FObjectExport::SerialOffset will be included in the output; useful when generating
	 * a report for comparison against another version of the same package.
	 */
	bool bHideOffsets;

	/**
	 * The linker of the package to generate the report for
	 */
	class FLinkerLoad* Linker;

	/**
	 * The number of packages evaluated by this reporter so far.  Must be incremented by child classes.
	 */
	int32 PackageCount;
};

struct FPkgInfoReporter_Log : public FPkgInfoReporter
{
	FPkgInfoReporter_Log( uint32 InInfoFlags, bool bInHideOffsets, FLinkerLoad* InLinker=NULL )
	: FPkgInfoReporter(InInfoFlags, bInHideOffsets, InLinker)
	{}
	FPkgInfoReporter_Log( const FPkgInfoReporter_Log& Other )
	: FPkgInfoReporter( Other )
	{}
	/**
	 * Writes information about the linker to the log.
	 *
	 * @param	InLinker	if specified, changes this reporter's Linker before generating the report.
	 */
	virtual void GeneratePackageReport( class FLinkerLoad* InLinker=NULL );
};
