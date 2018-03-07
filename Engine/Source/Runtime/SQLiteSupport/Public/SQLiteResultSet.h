// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Database.h"
//Forward declarations
struct sqlite3_stmt;

/**
* Result set for SQLite database queries
*/
class FSQLiteResultSet : public FDataBaseRecordSet
{
	//FDatabaseRecordSet implementation
protected:
	virtual void MoveToFirst() override;
	virtual void MoveToNext() override;
	virtual bool IsAtEnd() const override;
public:

	virtual int32 GetRecordCount() const override;
	virtual FString GetString(const TCHAR* Column) const override;
	virtual int32 GetInt(const TCHAR* Column) const override;
	virtual float GetFloat(const TCHAR* Column) const override;
	virtual int64 GetBigInt(const TCHAR* Column) const override;
	virtual TArray<FDatabaseColumnInfo> GetColumnNames() const override;
	//FDatabaseRecordSet 

	FSQLiteResultSet(sqlite3_stmt*& InStatement);
	virtual ~FSQLiteResultSet();
	
private:
	TArray<FDatabaseColumnInfo> ColumnNames;
	sqlite3_stmt* PreparedQuery = NULL;
	int32 NumberOfRecords = 0;
	int32 StepStatus = 0;
};
