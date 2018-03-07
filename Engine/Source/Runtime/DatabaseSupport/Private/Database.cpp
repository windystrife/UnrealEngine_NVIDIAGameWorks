// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Precompiled header include. Can't add anything above this line.
#include "Database.h"
#include "Misc/CommandLine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataBase, Log, All);
#if USE_REMOTE_INTEGRATION

/**
 * Sends a command to the database proxy.
 *
 * @param	Cmd		The command to be sent.
 */
bool ExecuteDBProxyCommand(FSocket *Socket, const TCHAR* Cmd)
{
	check(Socket);
	check(Cmd);

	int32 CmdStrLength = FCString::Strlen(Cmd);
	int32 BytesSent = 0;

	// add 1 so we also send NULL
	++CmdStrLength;

	TCHAR *SendBuf = (TCHAR*)FMemory::Malloc(CmdStrLength * sizeof(TCHAR));

	// convert to network byte ordering. This is important for running on the ps3 and xenon
	for(int32 BufIndex = 0; BufIndex < CmdStrLength; ++BufIndex)
	{
		SendBuf[BufIndex] = htons(Cmd[BufIndex]);
	}

	bool bRet = Socket->Send((uint8*)SendBuf, CmdStrLength * sizeof(TCHAR), BytesSent);

	FMemory::Free(SendBuf);

	return bRet;
}

////////////////////////////////////////////// FRemoteDatabaseConnection ///////////////////////////////////////////////////////
/**
 * Constructor.
 */
FRemoteDatabaseConnection::FRemoteDatabaseConnection()
: Socket(NULL)
{
	check(GSocketSubsystem);
	// The socket won't work if secure connections are enabled, so don't try
	if (GSocketSubsystem->RequiresEncryptedPackets() == false)
	{
		Socket = GSocketSubsystem->CreateSocket(NAME_Stream, TEXT("remote database connection"));
	}
}

/**
 * Destructor.
 */
FRemoteDatabaseConnection::~FRemoteDatabaseConnection()
{
	check(GSocketSubsystem);

	if ( Socket )
	{
		GSocketSubsystem->DestroySocket(Socket);
		Socket = NULL;
	}
}

/**
 * Opens a connection to the database.
 *
 * @param	ConnectionString	Connection string passed to database layer
 * @param   RemoteConnectionIP  The IP address which the RemoteConnection should connect to
 * @param   RemoteConnectionStringOverride  The connection string which the RemoteConnection is going to utilize
 *
 * @return	true if connection was successfully established, false otherwise
 */
bool FRemoteDatabaseConnection::Open( const TCHAR* ConnectionString, const TCHAR* RemoteConnectionIP, const TCHAR* RemoteConnectionStringOverride )
{
	bool bIsValid = false;
	if ( Socket )
	{
		FInternetAddr Address;
		Address.SetIp(RemoteConnectionIP, bIsValid);
		Address.SetPort(10500);

		if(bIsValid)
		{
			bIsValid = Socket->Connect(Address);

			if(bIsValid && RemoteConnectionStringOverride)
			{
				SetConnectionString(RemoteConnectionStringOverride);
			}
		}
	}
	return bIsValid;
}

/**
 * Closes connection to database.
 */
void FRemoteDatabaseConnection::Close()
{
	if ( Socket )
	{
		Socket->Close();
	}
}

/**
 * Executes the passed in command on the database.
 *
 * @param CommandString		Command to execute
 *
 * @return true if execution was successful, false otherwise
 */
bool FRemoteDatabaseConnection::Execute(const TCHAR* CommandString)
{
	FString Cmd = FString::Printf(TEXT("<command results=\"false\">%s</command>"), CommandString);
	return ExecuteDBProxyCommand(Socket, *Cmd);
}

/**
 * Executes the passed in command on the database. The caller is responsible for deleting
 * the created RecordSet.
 *
 * @param CommandString		Command to execute
 * @param RecordSet			Reference to recordset pointer that is going to hold result
 *
 * @return true if execution was successful, false otherwise
 */
bool FRemoteDatabaseConnection::Execute(const TCHAR* CommandString, FDataBaseRecordSet*& RecordSet)
{
	RecordSet = NULL;
	FString Cmd = FString::Printf(TEXT("<command results=\"true\">%s</command>"), CommandString);
	bool bRetVal = ExecuteDBProxyCommand(Socket, *Cmd);
	int32 ResultID = 0;
	int32 BytesRead;

	if(bRetVal)
	{
		Socket->Recv((uint8*)&ResultID, sizeof(int32), BytesRead);
		bRetVal = BytesRead == sizeof(int32);

		if(bRetVal)
		{
			RecordSet = new FRemoteDataBaseRecordSet(ResultID, Socket);
		}
	}

	return bRetVal;
}

/**
 * Sets the connection string to be used for this connection in the DB proxy.
 *
 * @param	ConnectionString	The new connection string to use.
 */
bool FRemoteDatabaseConnection::SetConnectionString(const TCHAR* ConnectionString)
{
	FString Cmd = FString::Printf(TEXT("<connectionString>%s</connectionString>"), ConnectionString);
	return ExecuteDBProxyCommand(Socket, *Cmd);
}

////////////////////////////////////////////// FRemoteDataBaseRecordSet ///////////////////////////////////////////////////////

/** Moves to the first record in the set. */
void FRemoteDataBaseRecordSet::MoveToFirst()
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<movetofirst resultset=\"%s\"/>"), *ID));
}

/** Moves to the next record in the set. */
void FRemoteDataBaseRecordSet::MoveToNext()
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<movetonext resultset=\"%s\"/>"), *ID));
}

/**
 * Returns whether we are at the end.
 *
 * @return true if at the end, false otherwise
 */
bool FRemoteDataBaseRecordSet::IsAtEnd() const
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<isatend resultset=\"%s\"/>"), *ID));

	int32 BytesRead;
	bool bResult;
	Socket->Recv((uint8*)&bResult, sizeof(bool), BytesRead);

	if(BytesRead != sizeof(bool))
	{
		bResult = false;
	}
	else
	{
		bResult = ntohl(bResult);
	}

	return bResult;
}

/**
 * Returns a string associated with the passed in field/ column for the current row.
 *
 * @param	Column	Name of column to retrieve data for in current row
 */
FString FRemoteDataBaseRecordSet::GetString( const TCHAR* Column ) const
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<getstring resultset=\"%s\">%s</getstring>"), *ID, Column));

	const int32 BUFSIZE = 2048;
	int32 BytesRead;
	int32 StrLength;
	TCHAR Buf[BUFSIZE];

	Socket->Recv((uint8*)&StrLength, sizeof(int32), BytesRead);

	StrLength = ntohl(StrLength);

	if(BytesRead != sizeof(int32) || StrLength <= 0)
	{
		return TEXT("");
	}

	if(StrLength > BUFSIZE - 1)
	{
		StrLength = BUFSIZE - 1;
	}

	Socket->Recv((uint8*)Buf, StrLength * sizeof(TCHAR), BytesRead);

	// TCHAR is assumed to be wchar_t so if we recv an odd # of bytes something messed up occured. Round down to the nearest wchar_t and then convert to number of TCHAR's.
	BytesRead -= BytesRead & 1; // rounding down
	BytesRead >>= 1; // conversion

	// convert from network to host byte order
	for(int i = 0; i < BytesRead; ++i)
	{
		Buf[i] = ntohs(Buf[i]);
	}

	Buf[BytesRead] = 0;

	return FString(Buf);
}

/**
* Returns an integer associated with the passed in field/ column for the current row.
*
* @param	Column	Name of column to retrieve data for in current row
*/
int32 FRemoteDataBaseRecordSet::GetInt( const TCHAR* Column ) const
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<getint resultset=\"%s\">%s</getint>"), *ID, Column));

	int32 BytesRead;
	int32 Value;

	Socket->Recv((uint8*)&Value, sizeof(int32), BytesRead);

	return ntohl(Value);
}

/**
* Returns a float associated with the passed in field/ column for the current row.
*
* @param	Column	Name of column to retrieve data for in current row
*/
float FRemoteDataBaseRecordSet::GetFloat( const TCHAR* Column ) const
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<getfloat resultset=\"%s\">%s</getfloat>"), *ID, Column));

	int32 BytesRead;
	int32 Temp;

	Socket->Recv((uint8*)&Temp, sizeof(int32), BytesRead);

	Temp = ntohl(Temp);

	float Value = *((float*)&Temp);

	return Value;
}

/** Constructor. */
FRemoteDataBaseRecordSet::FRemoteDataBaseRecordSet(int32 ResultSetID, FSocket *Connection) : Socket(NULL)
{
	check(ResultSetID >= 0);
	check(Connection);

	// NOTE: This socket will be deleted by whatever created it (prob an FRemoteDatabaseConnection), not this class.
	Socket = Connection;
	ID = FString::Printf(TEXT("%i"), ResultSetID);
}

/** Virtual destructor as class has virtual functions. */
FRemoteDataBaseRecordSet::~FRemoteDataBaseRecordSet()
{
	// tell the DB proxy to clean up the resources allocated for the result set.
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<closeresultset resultset=\"%s\"/>"), *ID));
}

#endif

#if USE_ADO_INTEGRATION
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include "AllowWindowsPlatformAtomics.h"

/*-----------------------------------------------------------------------------
	ADO integration for database connectivity
-----------------------------------------------------------------------------*/

// Using import allows making use of smart pointers easily. Please post to the list if a workaround such as
// using %COMMONFILES% works to hide the localization issues and non default program file folders.
//#import "C:\Program files\Common Files\System\Ado\msado15.dll" rename("EOF", "ADOEOF")

#pragma warning(push)
#pragma warning(disable: 4471) // a forward declaration of an unscoped enumeration must have an underlying type (int assumed)
#import "System\ADO\msado15.dll" rename("EOF", "ADOEOF") //lint !e322
#pragma warning(pop)

/*-----------------------------------------------------------------------------
	FADODataBaseRecordSet implementation.
-----------------------------------------------------------------------------*/

/**
 * ADO implementation of database record set.
 */
class FADODataBaseRecordSet : public FDataBaseRecordSet
{
private:
	ADODB::_RecordsetPtr ADORecordSet;

protected:
	/** Moves to the first record in the set. */
	virtual void MoveToFirst()
	{
		if( !ADORecordSet->BOF || !ADORecordSet->ADOEOF )
		{
			ADORecordSet->MoveFirst();
		}
	}
	/** Moves to the next record in the set. */
	virtual void MoveToNext()
	{
		if( !ADORecordSet->ADOEOF )
		{
			ADORecordSet->MoveNext();
		}
	}
	/**
	 * Returns whether we are at the end.
	 *
	 * @return true if at the end, false otherwise
	 */
	virtual bool IsAtEnd() const
	{
		return !!ADORecordSet->ADOEOF;
	}

public:

	/** 
	 *   Returns a count of the number of records in the record set
	 */
	virtual int32 GetRecordCount() const
	{
		return ADORecordSet->RecordCount;
	}

	/**
	 * Returns a string associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual FString GetString( const TCHAR* Column ) const
	{
		FString ReturnString;

		// Retrieve specified column field value for selected row.
		_variant_t Value = ADORecordSet->GetCollect( Column );
		// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
		if( Value.vt != VT_NULL )
		{
			ReturnString = (TCHAR*)_bstr_t(Value);
		}
		// Unknown column.
		else
		{
			ReturnString = TEXT("Unknown Column");
		}

		return ReturnString;
	}

	/**
	 * Returns an integer associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual int32 GetInt( const TCHAR* Column ) const
	{
		int32 ReturnValue = 0;

		// Retrieve specified column field value for selected row.
		_variant_t Value = ADORecordSet->GetCollect( Column );
		// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
		if( Value.vt != VT_NULL )
		{
			ReturnValue = (int32)Value;
		}
		// Unknown column.
		else
		{
			UE_LOG(LogDataBase, Log, TEXT("Failure retrieving int32 value for column [%s]"),Column);
		}

		return ReturnValue;
	}

	/**
	 * Returns a float associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual float GetFloat( const TCHAR* Column ) const
	{
		float ReturnValue = 0;

		// Retrieve specified column field value for selected row.
		_variant_t Value = ADORecordSet->GetCollect( Column );
		// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
		if( Value.vt != VT_NULL )
		{
			ReturnValue = (float)Value;
		}
		// Unknown column.
		else
		{
			UE_LOG(LogDataBase, Log, TEXT("Failure retrieving float value for column [%s]"),Column);
		}

		return ReturnValue;
	}

	/**
	 * Returns an int64 associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual int64 GetBigInt( const TCHAR* Column ) const
	{
		int64 ReturnValue = 0;

		// Retrieve specified column field value for selected row.
		_variant_t Value = ADORecordSet->GetCollect( Column );
		// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
		if( Value.vt != VT_NULL )
		{
			ReturnValue = (int64)Value;
		}
		// Unknown column.
		else
		{
			UE_LOG(LogDataBase, Log, TEXT("Failure retrieving BIGINT value for column [%s]"),Column);
		}

		return ReturnValue;
	}

	/**
	 * Returns the set of column names for this Recordset.  This is useful for determining  
	 * what you can actually ask the record set for without having to hard code those ahead of time.  
	 */  
	virtual TArray<FDatabaseColumnInfo> GetColumnNames() const
	{  
		TArray<FDatabaseColumnInfo> Retval;  

		if( !ADORecordSet->BOF || !ADORecordSet->ADOEOF ) 
		{  
			ADORecordSet->MoveFirst();

			for( int16 i = 0; i < ADORecordSet->Fields->Count; ++i )  
			{  
				_bstr_t bstrName = ADORecordSet->Fields->Item[i]->Name;  
				_variant_t varValue = ADORecordSet->Fields->Item[i]->Value;  
				ADODB::DataTypeEnum DataType = ADORecordSet->Fields->Item[i]->Type;  

				FDatabaseColumnInfo NewInfo;  
				NewInfo.ColumnName = FString((TCHAR*)_bstr_t(bstrName));  

				// from http://www.w3schools.com/ado/prop_field_type.asp#datatypeenum  
				switch( DataType )  
				{  
				case ADODB::adInteger:  
				case ADODB::adBigInt:
					NewInfo.DataType = DBT_INT;  
					break;  
				case ADODB::adSingle:  
				case ADODB::adDouble:  
					NewInfo.DataType = DBT_FLOAT;  
					break;  

				case ADODB::adWChar:
				case ADODB::adVarWChar:
					NewInfo.DataType = DBT_STRING;
					break;

				default:  
					UE_LOG(LogDataBase, Warning, TEXT("Unable to find a EDataBaseUE3Types (%s) from DODB::DataTypeEnum DataType: %d "), *NewInfo.ColumnName, static_cast<int32>(DataType) );  
					NewInfo.DataType = DBT_UNKOWN;  
					break;  
				}  


				Retval.Add( NewInfo );  
			}  
		}  

		// here for debugging as this code is new.
		for( int32 i = 0; i < Retval.Num(); ++i )
		{  
			UE_LOG(LogDataBase, Warning, TEXT( "ColumnName %d: Name: %s  Type: %d"), i, *Retval[i].ColumnName, static_cast<int32>(Retval[i].DataType) );  
		}  

		return Retval;  
	}   


	/**
	 * Constructor, used to associate ADO record set with this class.
	 *
	 * @param InADORecordSet	ADO record set to use
	 */
	FADODataBaseRecordSet( ADODB::_RecordsetPtr InADORecordSet )
	:	ADORecordSet( InADORecordSet )
	{
	}

	/** Destructor, cleaning up ADO record set. */
	virtual ~FADODataBaseRecordSet()
	{
		if(ADORecordSet && (ADORecordSet->State & ADODB::adStateOpen))
		{
			// We're using smart pointers so all we need to do is close and assign NULL.
			ADORecordSet->Close();
		}

		ADORecordSet = NULL;
	}
};


/*-----------------------------------------------------------------------------
	FADODataBaseConnection implementation.
-----------------------------------------------------------------------------*/

/**
 * Data base connection class using ADO C++ interface to communicate with SQL server.
 */
class FADODataBaseConnection : public FDataBaseConnection
{
private:
	/** ADO database connection object. */
	ADODB::_ConnectionPtr DataBaseConnection;

public:
	/** Constructor, initializing all member variables. */
	FADODataBaseConnection()
	{
		DataBaseConnection = NULL;
	}

	/** Destructor, tearing down connection. */
	virtual ~FADODataBaseConnection()
	{
		Close();
	}

	/**
	 * Opens a connection to the database.
	 *
	 * @param	ConnectionString	Connection string passed to database layer
	 * @param   RemoteConnectionIP  The IP address which the RemoteConnection should connect to
	 * @param   RemoteConnectionStringOverride  The connection string which the RemoteConnection is going to utilize
	 *
	 * @return	true if connection was successfully established, false otherwise
	 */
	virtual bool Open( const TCHAR* ConnectionString, const TCHAR* RemoteConnectionIP, const TCHAR* RemoteConnectionStringOverride )
	{
		if (!FWindowsPlatformMisc::CoInitialize())
		{
			return false;
		}

		// Create instance of DB connection object.
		HRESULT hr = DataBaseConnection.CreateInstance(__uuidof(ADODB::Connection));
		if (FAILED(hr))
		{
			FWindowsPlatformMisc::CoUninitialize();
			throw _com_error(hr);
		}

		// Open the connection. Operation is synchronous.
		DataBaseConnection->Open( ConnectionString, TEXT(""), TEXT(""), ADODB::adConnectUnspecified );

		return true;
	}

	/**
	 * Closes connection to database.
	 */
	virtual void Close()
	{
		// Close database connection if exists and free smart pointer.
		if( DataBaseConnection && (DataBaseConnection->State & ADODB::adStateOpen))
		{
			DataBaseConnection->Close();

			FWindowsPlatformMisc::CoUninitialize();
		}

		DataBaseConnection = NULL;
	}

	/**
	 * Executes the passed in command on the database.
	 *
	 * @param CommandString		Command to execute
	 *
	 * @return true if execution was successful, false otherwise
	 */
	virtual bool Execute( const TCHAR* CommandString )
	{
		// Execute command, passing in optimization to tell DB to not return records.
		DataBaseConnection->Execute( CommandString, NULL, ADODB::adExecuteNoRecords );

		return true;
	}

	/**
	 * Executes the passed in command on the database. The caller is responsible for deleting
	 * the created RecordSet.
	 *
	 * @param CommandString		Command to execute
	 * @param RecordSet			Reference to recordset pointer that is going to hold result
	 *
	 * @return true if execution was successful, false otherwise
	 */
	virtual bool Execute( const TCHAR* CommandString, FDataBaseRecordSet*& RecordSet )
	{
		// Initialize return value.
		RecordSet = NULL;

		// Create instance of record set.
		ADODB::_RecordsetPtr ADORecordSet = NULL;
		ADORecordSet.CreateInstance(__uuidof(ADODB::Recordset) );
				
		// Execute the passed in command on the record set. The recordset returned will be in open state so you can call Get* on it directly.
		ADORecordSet->Open( CommandString, _variant_t((IDispatch *) DataBaseConnection), ADODB::adOpenStatic, ADODB::adLockReadOnly, ADODB::adCmdText );

		// Create record set from returned data.
		RecordSet = new FADODataBaseRecordSet( ADORecordSet );

		return RecordSet != NULL;
	}
};

#include "HideWindowsPlatformAtomics.h"
#include "HideWindowsPlatformTypes.h"
#endif // USE_ADO_INTEGRATION

/*-----------------------------------------------------------------------------
	FDataBaseConnection implementation.
-----------------------------------------------------------------------------*/

/**
 * Static function creating appropriate database connection object.
 *
 * @return	instance of platform specific database connection object
 */
FDataBaseConnection* FDataBaseConnection::CreateObject()
{
	if( FParse::Param( FCommandLine::Get(), TEXT("NODATABASE") ) )
	{
		UE_LOG(LogDataBase, Log, TEXT("DB usage disabled, please ignore failure messages."));
		return NULL;
	}
#if USE_ADO_INTEGRATION
	return new FADODataBaseConnection();
#elif USE_REMOTE_INTEGRATION
	return new FRemoteDatabaseConnection();
#else
	return new FDataBaseConnection();
#endif
}
