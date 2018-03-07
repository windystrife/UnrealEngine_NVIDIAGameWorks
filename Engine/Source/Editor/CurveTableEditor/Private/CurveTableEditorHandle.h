// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveOwnerInterface.h"
#include "UObject/WeakObjectPtr.h"
#include "Engine/CurveTable.h"

/**
 * Handle to a particular row in a table, used for editing individual curves
 */
struct FCurveTableEditorHandle : public FCurveOwnerInterface
{
	FCurveTableEditorHandle()
		: CurveTable(nullptr)
		, RowName(NAME_None)
	{ }

	FCurveTableEditorHandle(const UCurveTable* InCurveTable, FName InRowName)
		: CurveTable(InCurveTable)
		, RowName(InRowName)
	{ }

	/** Pointer to table we want a row from */
	TWeakObjectPtr<const UCurveTable> CurveTable;

	/** Name of row in the table that we want */
	FName RowName;

	//~ Begin FCurveOwnerInterface Interface.
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	virtual void ModifyOwner() override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;
	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override;
	virtual TArray<const UObject*> GetOwners() const override
	{ 
		//Note this is read only so we return nothing
		return TArray<const UObject*>(); 
	}

	//~ End FCurveOwnerInterface Interface.

	/** Returns true if the curve is valid */
	bool IsValid() const
	{
		return (GetCurve() != nullptr);
	}

	/** Returns true if this handle is specifically pointing to nothing */
	bool IsNull() const
	{
		return CurveTable == nullptr && RowName == NAME_None;
	}

	/** Get the curve straight from the row handle */
	FRichCurve* GetCurve() const;
};
