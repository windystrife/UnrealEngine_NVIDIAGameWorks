// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SQLiteSupport.h"
#include "SQLiteResultSet.h"

/**
* SQLite database file
*/

//Forward declarations

struct sqlite3;

class FSQLiteDatabase : FDataBaseConnection
{
public: 
	
	/** Closes the database handle and unlocks the file */
	SQLITESUPPORT_API virtual void Close();

	/** Execute a command on the database without storing the result set (if any) */
	SQLITESUPPORT_API virtual bool Execute(const TCHAR* CommandString) override;

	/** Executes the command string on the currently opened database, returning a FSQLiteResultSet in RecordSet. Caller is responsible for freeing RecordSet */
	SQLITESUPPORT_API bool Execute(const TCHAR* CommandString, FSQLiteResultSet*& RecordSet);

	/** Open a SQLite file 
	* @param	ConnectionString	Path to the file that should be opened
	* @param	RemoteConnectionIP	Unused with this implementation
	* @param	ConnectionString	Unused with this implementation
	*/
	SQLITESUPPORT_API virtual bool Open(const TCHAR* ConnectionString, const TCHAR* RemoteConnectionIP, const TCHAR* RemoteConnectionStringOverride);

	
	//Overriding to address warning C4264 - SQLite databases should call Execute(const TCHAR* CommandString, FSQLiteResultSet*& RecordSet) instead
	SQLITESUPPORT_API virtual bool Execute(const TCHAR* CommandString, FDataBaseRecordSet*& RecordSet) override;

	SQLITESUPPORT_API FString GetLastError();
protected:
	sqlite3* DbHandle = NULL;
};
