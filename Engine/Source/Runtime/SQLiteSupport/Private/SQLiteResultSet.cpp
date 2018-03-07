// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SQLiteSupport.h"
#include "SQLiteResultSet.h"
#include "sqlite3.h"
FSQLiteResultSet::~FSQLiteResultSet()
{
	if (PreparedQuery)
	{
		sqlite3_finalize(PreparedQuery);
	}
}

FSQLiteResultSet::FSQLiteResultSet(sqlite3_stmt*& InStatement) 
	: PreparedQuery(InStatement)
{
	
	StepStatus = sqlite3_step(PreparedQuery);
	if (StepStatus == SQLITE_ROW)
	{
		for (int32 i = 0; i < sqlite3_column_count(PreparedQuery); i++)
		{
			FDatabaseColumnInfo ColumnInfo;
			switch (sqlite3_column_type(PreparedQuery, i))
			{
			case SQLITE_INTEGER:
				ColumnInfo.DataType = DBT_INT;
				break;

			case SQLITE_FLOAT:
				ColumnInfo.DataType = DBT_FLOAT;
				break;

			case SQLITE_TEXT:
				ColumnInfo.DataType = DBT_STRING;
				break;

			case SQLITE_BLOB:
				ColumnInfo.DataType = DBT_UNKOWN;
				break;
			case SQLITE_NULL:
				ColumnInfo.DataType = DBT_UNKOWN;

			default:
				break;
			}
			ColumnInfo.ColumnName = sqlite3_column_name(PreparedQuery, i);
			ColumnNames.Add(ColumnInfo);
		}
		NumberOfRecords++;
	}
	while (sqlite3_step(PreparedQuery) == SQLITE_ROW)
	{
		NumberOfRecords++;
	}
	sqlite3_reset(PreparedQuery);
}


TArray<FDatabaseColumnInfo> FSQLiteResultSet::GetColumnNames() const
{
	return ColumnNames;
}

int64 FSQLiteResultSet::GetBigInt(const TCHAR* Column) const
{
	int32 ColumnIndex = ColumnNames.IndexOfByPredicate([&](FDatabaseColumnInfo ColumnInfo){return ColumnInfo.ColumnName == Column; });
	if (ColumnIndex == INDEX_NONE)
	{
		//UE_LOG(LogDataBase, Log, TEXT("SQLITE: Failure retrieving big int value for column [%s]"), Column);
		return 0;
	}
	else
	{
		return sqlite3_column_int64(PreparedQuery, ColumnIndex);
	}
	
}

float FSQLiteResultSet::GetFloat(const TCHAR* Column) const
{
	int32 ColumnIndex = ColumnNames.IndexOfByPredicate([&](FDatabaseColumnInfo ColumnInfo){return ColumnInfo.ColumnName == Column; });
	if (ColumnIndex == INDEX_NONE)
	{
		//UE_LOG(LogDataBase, Log, TEXT("SQLITE: Failure retrieving float value for column [%s]"), Column);
		return 0;
	}
	else
	{
		return FCString::Atof(UTF8_TO_TCHAR(sqlite3_column_text(PreparedQuery, ColumnIndex)));
	}


}

int32 FSQLiteResultSet::GetInt(const TCHAR* Column) const
{
	int32 ColumnIndex = ColumnNames.IndexOfByPredicate([&](FDatabaseColumnInfo ColumnInfo){return ColumnInfo.ColumnName == Column; });
	if (ColumnIndex == INDEX_NONE)
	{
		//UE_LOG(LogDataBase, Log, TEXT("SQLITE: Failure retrieving int value for column [%s]"), Column);
		return 0;
	}
	else
	{
		return FCString::Atoi(UTF8_TO_TCHAR(sqlite3_column_text(PreparedQuery, ColumnIndex)));
	}


}

FString FSQLiteResultSet::GetString(const TCHAR* Column) const
{
	int32 ColumnIndex = ColumnNames.IndexOfByPredicate([&](FDatabaseColumnInfo ColumnInfo){return ColumnInfo.ColumnName == Column; });
	if (ColumnIndex == INDEX_NONE)
	{
		//UE_LOG(LogDataBase, Log, TEXT("SQLITE: Failure retrieving string value for column [%s]"), Column);
		return FString();
	}
	else
	{
		return FString(UTF8_TO_TCHAR(sqlite3_column_text(PreparedQuery, ColumnIndex)));
	}

}

int32 FSQLiteResultSet::GetRecordCount() const
{
	return NumberOfRecords;
}

bool FSQLiteResultSet::IsAtEnd() const
{
	return StepStatus == SQLITE_DONE;
}

void FSQLiteResultSet::MoveToNext()
{
	StepStatus = sqlite3_step(PreparedQuery);
}

void FSQLiteResultSet::MoveToFirst()
{
	sqlite3_reset(PreparedQuery);
	StepStatus = sqlite3_step(PreparedQuery);
}

