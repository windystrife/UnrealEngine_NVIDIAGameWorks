// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SQLiteSupport.h"
#include "SQLiteDatabaseConnection.h"
#include "sqlite3.h"

bool FSQLiteDatabase::Execute(const TCHAR* CommandString, FSQLiteResultSet*& RecordSet)
{
	
	//Compile the statement/query
	sqlite3_stmt* PreparedStatement;
	int32 PrepareStatus = sqlite3_prepare_v2(DbHandle, TCHAR_TO_UTF8(CommandString), -1, &PreparedStatement, NULL);
	if (PrepareStatus == SQLITE_OK)
	{
		//Initialize records from compiled query
        RecordSet = new FSQLiteResultSet(PreparedStatement);
		return true;
	}
	else
	{
		RecordSet = NULL;
		return false;
	}
}


bool FSQLiteDatabase::Execute(const TCHAR* CommandString)
{
	//Compile the statement/query
	sqlite3_stmt* PreparedStatement;
	int32 PrepareStatus = sqlite3_prepare_v2(DbHandle, TCHAR_TO_UTF8(CommandString), -1, &PreparedStatement, NULL);
	if (PrepareStatus == SQLITE_OK)
	{
		int32 StepStatus = SQLITE_ERROR;
		//Execute the statement until it is done or we get an error
		do
		{
			StepStatus = sqlite3_step(PreparedStatement);
		} while ((StepStatus != SQLITE_ERROR) && (StepStatus != SQLITE_CONSTRAINT) && (StepStatus != SQLITE_DONE));
		
		//Did we make it all the way through the query without an error?
		if (StepStatus == SQLITE_DONE)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool FSQLiteDatabase::Execute(const TCHAR* CommandString, FDataBaseRecordSet*& RecordSet)
{
	return Execute(CommandString, (FSQLiteResultSet*&)RecordSet);
}

void FSQLiteDatabase::Close()
{
	if (DbHandle)
	{
		sqlite3_close(DbHandle);
		DbHandle = nullptr;
	}
}



bool FSQLiteDatabase::Open(const TCHAR* ConnectionString, const TCHAR* RemoteConnectionIP, const TCHAR* RemoteConnectionStringOverride)
{
	if (DbHandle)
	{
		return false;
	}

	int32 Result = sqlite3_open(TCHAR_TO_UTF8(ConnectionString), &DbHandle);
	return Result == SQLITE_OK;
}

FString FSQLiteDatabase::GetLastError()
{
	TCHAR* ErrorString = NULL;
	ErrorString = (TCHAR*) UTF8_TO_TCHAR(sqlite3_errmsg(DbHandle));
	if (ErrorString)
	{
		return FString(ErrorString);
	}
	else
	{
		return FString();
	}
}
