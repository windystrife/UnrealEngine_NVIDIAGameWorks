// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PerforceConnection.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ISourceControlModule.h"
#include "HAL/PlatformTime.h"
#include "PerforceSourceControlModule.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "PerforceConnection"

#define FROM_TCHAR(InText, bIsUnicodeServer) (bIsUnicodeServer ? TCHAR_TO_UTF8(InText) : TCHAR_TO_ANSI(InText))
#define TO_TCHAR(InText, bIsUnicodeServer) (bIsUnicodeServer ? UTF8_TO_TCHAR(InText) : ANSI_TO_TCHAR(InText))

/** Custom ClientUser class for handling results and errors from Perforce commands */
class FP4ClientUser : public ClientUser
{
public:

	// Constructor
	FP4ClientUser(FP4RecordSet& InRecords, bool bInIsUnicodeServer, TArray<FText>& InOutErrorMessages)
		:	ClientUser()
		,	bIsUnicodeServer(bInIsUnicodeServer)
		,	Records(InRecords)
		,	OutErrorMessages(InOutErrorMessages)
	{
	}

	// Called by P4API when the results from running a command are ready
	void OutputStat(StrDict* VarList)
	{
#if USE_P4_API
		FP4Record Record;
		StrRef Var, Value;

		// Iterate over each variable and add to records
		for (int32 Index = 0; VarList->GetVar(Index, Var, Value); Index++)
		{
			Record.Add(TO_TCHAR(Var.Text(), bIsUnicodeServer), TO_TCHAR(Value.Text(), bIsUnicodeServer));
		}

		Records.Add(Record);
#endif
	}

	void HandleError(Error *InError)
	{
#if USE_P4_API
		StrBuf ErrorMessage;
		InError->Fmt(&ErrorMessage);
		OutErrorMessages.Add(FText::FromString(FString(TO_TCHAR(ErrorMessage.Text(), bIsUnicodeServer))));
#endif
	}

	bool bIsUnicodeServer;
	FP4RecordSet& Records;
	TArray<FText>& OutErrorMessages;
};

/** A class used instead of FP4ClientUser for handling changelist create command */
class FP4CreateChangelistClientUser : public FP4ClientUser
{
public:

	// Constructor
	FP4CreateChangelistClientUser(FP4RecordSet& InRecords, bool bInIsUnicodeServer, TArray<FText>& InOutErrorMessages, const FText& InDescription, ClientApi &InP4Client)
		:	FP4ClientUser(InRecords, bInIsUnicodeServer, InOutErrorMessages)
		,	Description(InDescription)
		,	ChangelistNumber(0)
		,	P4Client(InP4Client)
	{
	}

	// Called by P4API when the changelist is created.
	void OutputInfo(ANSICHAR Level, const ANSICHAR *Data)
	{
		const int32 ChangeTextLen = FCString::Strlen(TEXT("Change "));
		if (FString(TO_TCHAR(Data, bIsUnicodeServer)).StartsWith(TEXT("Change ")))
		{
			ChangelistNumber = FCString::Atoi(TO_TCHAR(Data + ChangeTextLen, bIsUnicodeServer));
		}
	}

	// Called by P4API on "change -i" command. OutBuffer is filled with changelist specification text
	void InputData(StrBuf* OutBuffer, Error* OutError)
	{
#if USE_P4_API
		FString OutputDesc;
		OutputDesc += TEXT("Change:\tnew\n\n");
		OutputDesc += TEXT("Client:\t");
		OutputDesc += TO_TCHAR(P4Client.GetClient().Text(), bIsUnicodeServer);
		OutputDesc += TEXT("\n\n");
		OutputDesc += TEXT("User:\t");
		OutputDesc += TO_TCHAR(P4Client.GetUser().Text(), bIsUnicodeServer);
		OutputDesc += TEXT("\n\n");
		OutputDesc += TEXT("Status:\tnew\n\n");
		OutputDesc += TEXT("Description:\n");
		{
			TArray<FString> DescLines;
			Description.ToString().ParseIntoArray(DescLines, TEXT("\n"), false);
			for (const FString& DescLine : DescLines)
			{
				OutputDesc += TEXT("\t");
				OutputDesc += DescLine;
				OutputDesc += TEXT("\n");
			}
		}
		OutputDesc += TEXT("\n");
		OutputDesc += TEXT("Files:\n\n");

		OutBuffer->Append(FROM_TCHAR(*OutputDesc, bIsUnicodeServer));
#endif
	}

	FText Description;
	int32 ChangelistNumber;
	ClientApi& P4Client;
};

/** Custom ClientUser class for handling login commands */
class FP4LoginClientUser : public FP4ClientUser
{
public:

	// Constructor
	FP4LoginClientUser(const FString& InPassword, FP4RecordSet& InRecords, bool bInIsUnicodeServer, TArray<FText>& InOutErrorMessages)
		:	FP4ClientUser(InRecords, bInIsUnicodeServer, InOutErrorMessages)
		,	Password(InPassword)
	{
	}

	void Prompt( const StrPtr& InMessage, StrBuf& OutPrompt, int NoEcho, Error* InError )
	{
#if USE_P4_API
		OutPrompt.Set(FROM_TCHAR(*Password, bIsUnicodeServer));
#endif
	}

	/** Password to use when logging in */
	FString Password;
};


class FP4KeepAlive : public KeepAlive
{
public:
	FP4KeepAlive(FOnIsCancelled InIsCancelled)
		: IsCancelled(InIsCancelled)
	{
	}

#if USE_P4_API
	virtual int IsAlive() override
	{
		if(IsCancelled.IsBound() && IsCancelled.Execute())
		{
			return 0;
		}

		return 1;
	}
#endif

	FOnIsCancelled IsCancelled;
};

/**
 * Runs "client" command to test if the connection is actually OK. ClientApi::Init() only checks
 * if it can connect to server, doesn't verify user name nor workspace name.
 */
static bool TestConnection(ClientApi& P4Client, const FString& ClientSpecName, bool bIsUnicodeServer, TArray<FText>& OutErrorMessages)
{
	FP4RecordSet Records;
	FP4ClientUser User(Records, bIsUnicodeServer, OutErrorMessages);

	UTF8CHAR* ClientSpecUTF8Name = nullptr;
	if(bIsUnicodeServer)
	{
		FTCHARToUTF8 UTF8String(*ClientSpecName);
		ClientSpecUTF8Name = new UTF8CHAR[UTF8String.Length() + 1];
		FMemory::Memcpy(ClientSpecUTF8Name, UTF8String.Get(), UTF8String.Length() + 1);
	}
	else
	{		
		ClientSpecUTF8Name = new UTF8CHAR[ClientSpecName.Len() + 1];
		FMemory::Memcpy(ClientSpecUTF8Name, TCHAR_TO_ANSI(*ClientSpecName), ClientSpecName.Len() + 1);
	}

	const char *ArgV[] = { "-o", (char*)ClientSpecUTF8Name };
#if USE_P4_API
	P4Client.SetArgv(2, const_cast<char*const*>(ArgV));

	P4Client.Run("client", &User);
#endif

	// clean up args
	delete [] ClientSpecUTF8Name;

	// If there are error messages, user name is most likely invalid. Otherwise, make sure workspace actually
	// exists on server by checking if we have it's update date.
	bool bConnectionOK = OutErrorMessages.Num() == 0 && Records.Num() > 0 && Records[0].Contains(TEXT("Update"));
	if (!bConnectionOK && OutErrorMessages.Num() == 0)
	{
		OutErrorMessages.Add(LOCTEXT("InvalidWorkspace", "Invalid Workspace"));
	}

	return bConnectionOK;
}

static bool CheckUnicodeStatus(ClientApi& P4Client, bool& bIsUnicodeServer, TArray<FText>& OutErrorMessages)
{
#if USE_P4_API
	FP4RecordSet Records;
	FP4ClientUser User(Records, false, OutErrorMessages);

	P4Client.Run("info", &User);

	if(Records.Num() > 0)
	{
		bIsUnicodeServer = Records[0].Find(TEXT("unicode")) != NULL;
	}
	else
	{
		bIsUnicodeServer = false;
	}
#endif

	return OutErrorMessages.Num() == 0;
}

FPerforceConnection::FPerforceConnection(const FPerforceConnectionInfo& InConnectionInfo)
:	bEstablishedConnection(false)
,	bIsUnicode(false)
{
	EstablishConnection(InConnectionInfo);
}

FPerforceConnection::~FPerforceConnection()
{
	Disconnect();
}

bool FPerforceConnection::AutoDetectWorkspace(const FPerforceConnectionInfo& InConnectionInfo, FString& OutWorkspaceName)
{
	bool Result = false;
	FMessageLog SourceControlLog("SourceControl");

	//before even trying to summon the window, try to "smart" connect with the default server/username
	TArray<FText> ErrorMessages;
	FPerforceConnection Connection(InConnectionInfo);
	TArray<FString> ClientSpecList;
	Connection.GetWorkspaceList(InConnectionInfo, FOnIsCancelled(), ClientSpecList, ErrorMessages);

	//if only one client spec matched (and default connection info was correct)
	if (ClientSpecList.Num() == 1)
	{
		OutWorkspaceName = ClientSpecList[0];
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("WorkspaceName"), FText::FromString(OutWorkspaceName) );
		SourceControlLog.Info(FText::Format(LOCTEXT("ClientSpecAutoDetect", "Auto-detected Perforce client spec: '{WorkspaceName}'"), Arguments));
		Result = true;
	}
	else if (ClientSpecList.Num() > 0)
	{
		SourceControlLog.Warning(LOCTEXT("AmbiguousClientSpecLine1", "Source Control unable to auto-login due to ambiguous client specs"));
		SourceControlLog.Warning(LOCTEXT("AmbiguousClientSpecLine2", "  Please select a client spec in the Perforce settings dialog"));
		SourceControlLog.Warning(LOCTEXT("AmbiguousClientSpecLine3", "  If you are unable to work with source control, consider checking out the files by hand temporarily"));

		// List out the clientspecs that were found to be ambiguous
		SourceControlLog.Info(LOCTEXT("AmbiguousClientSpecListTitle", "Ambiguous client specs..."));
		for (int32 Index = 0; Index < ClientSpecList.Num(); Index++)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add( TEXT("ClientSpecName"), FText::FromString(ClientSpecList[Index]) );
			SourceControlLog.Info(FText::Format(LOCTEXT("AmbiguousClientSpecListItem", "...{ClientSpecName}"), Arguments));
		}
	}

	return Result;
}

bool FPerforceConnection::Login(const FPerforceConnectionInfo& InConnectionInfo)
{
	TArray<FText> ErrorMessages;
#if USE_P4_API
	FP4RecordSet Records;
	FP4LoginClientUser User(InConnectionInfo.Password, Records, false, ErrorMessages);

	const char *ArgV[] = { "-a" };
	P4Client.SetArgv(1, const_cast<char*const*>(ArgV));
	P4Client.Run("login", &User);

	if(ErrorMessages.Num())
	{
		UE_LOG(LogSourceControl, Error, TEXT("Login failed"));
		for(auto ErrorMessage : ErrorMessages)
		{
			UE_LOG(LogSourceControl, Error, TEXT("%s"), *ErrorMessage.ToString());
		}
	}
#endif

	return ErrorMessages.Num() == 0;
}

bool FPerforceConnection::EnsureValidConnection(FString& InOutServerName, FString& InOutUserName, FString& InOutWorkspaceName, const FPerforceConnectionInfo& InConnectionInfo)
{
	bool bIsUnicodeServer = false;
	bool bConnectionOK = false;
#if USE_P4_API
	FMessageLog SourceControlLog("SourceControl");

	FString NewServerName = InOutServerName;
	FString NewUserName = InOutUserName;
	FString NewClientSpecName = InOutWorkspaceName;

	ClientApi TestP4;
	TestP4.SetProtocol("tag", "");
	TestP4.SetProtocol("enableStreams", "");

	if (NewServerName.Len() && NewUserName.Len() && NewClientSpecName.Len())
	{
		//attempt connection with given settings
		TestP4.SetPort(TCHAR_TO_ANSI(*NewServerName));

		if(InConnectionInfo.Password.Len() > 0)
		{
			TestP4.SetPassword(TCHAR_TO_ANSI(*InConnectionInfo.Password));
		}

		if(InConnectionInfo.HostOverride.Len() > 0)
		{
			TestP4.SetHost(TCHAR_TO_ANSI(*InConnectionInfo.HostOverride));
		}
	}

	Error P4Error;
	TestP4.Init(&P4Error);

	bConnectionOK = !P4Error.Test();
	if (!bConnectionOK)
	{
		//Connection FAILED
		StrBuf ErrorMessage;
		P4Error.Fmt(&ErrorMessage);
		SourceControlLog.Error(LOCTEXT("P4ErrorConnection", "P4ERROR: Failed to connect to source control provider."));
		SourceControlLog.Error(FText::FromString(ANSI_TO_TCHAR(ErrorMessage.Text())));
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("PortName"), FText::FromString(NewServerName) );
		Arguments.Add( TEXT("UserName"), FText::FromString(NewUserName) );
		Arguments.Add( TEXT("ClientSpecName"), FText::FromString(NewClientSpecName) );
		Arguments.Add( TEXT("Ticket"), FText::FromString(InConnectionInfo.Ticket) );
		SourceControlLog.Error(FText::Format(LOCTEXT("P4ErrorConnection_Details", "Port={PortName}, User={UserName}, ClientSpec={ClientSpecName}, Ticket={Ticket}"), Arguments));
	}

	// run an info command to determine unicode status
	if(bConnectionOK)
	{
		TArray<FText> ErrorMessages;

		bConnectionOK = CheckUnicodeStatus(TestP4, bIsUnicodeServer, ErrorMessages);
		if(!bConnectionOK)
		{
			SourceControlLog.Error(LOCTEXT("P4ErrorConnection", "P4ERROR: Could not determine server unicode status."));
			SourceControlLog.Error(ErrorMessages.Num() > 0 ? ErrorMessages[0] : LOCTEXT("P4ErrorConnection_Unknown error", "Unknown error"));
			FFormatNamedArguments Arguments;
			Arguments.Add( TEXT("PortName"), FText::FromString(NewServerName) );
			Arguments.Add( TEXT("UserName"), FText::FromString(NewUserName) );
			Arguments.Add( TEXT("ClientSpecName"), FText::FromString(NewClientSpecName) );
			Arguments.Add( TEXT("Ticket"), FText::FromString(InConnectionInfo.Ticket) );
			SourceControlLog.Error(FText::Format(LOCTEXT("P4ErrorConnection_Details", "Port={PortName}, User={UserName}, ClientSpec={ClientSpecName}, Ticket={Ticket}"), Arguments));
		}
		else
		{
			if(bIsUnicodeServer)
			{
				// set translation mode. From here onwards we need to use UTF8 when using text args
				TestP4.SetTrans(CharSetApi::UTF_8);
			}

			// now we have determined unicode status, we can set the values that can be specified in non-ansi characters
			TestP4.SetCwd(FROM_TCHAR(*FPaths::RootDir(), bIsUnicodeServer));
			TestP4.SetUser(FROM_TCHAR(*NewUserName, bIsUnicodeServer));
			TestP4.SetClient(FROM_TCHAR(*NewClientSpecName, bIsUnicodeServer));

			if(InConnectionInfo.Ticket.Len())
			{
				TestP4.SetPassword(FROM_TCHAR(*InConnectionInfo.Ticket, bIsUnicodeServer));
			}
		}
	}

	if (bConnectionOK)
	{
		// If a client spec was not specified, attempt to auto-detect it here. If the detection is not successful, neither is this connection since we need a client spec.
		if ( NewClientSpecName.IsEmpty() )
		{
			FPerforceConnectionInfo AutoCredentials = InConnectionInfo;
			AutoCredentials.Port = TO_TCHAR(TestP4.GetPort().Text(), bIsUnicodeServer);
			AutoCredentials.UserName = TO_TCHAR(TestP4.GetUser().Text(), bIsUnicodeServer);

		 	bConnectionOK = FPerforceConnection::AutoDetectWorkspace(AutoCredentials, NewClientSpecName);
			if ( bConnectionOK )
			{
				TestP4.SetClient(FROM_TCHAR(*NewClientSpecName, bIsUnicodeServer));
			}
		}
	}

	if (bConnectionOK)
	{
		TArray<FText> ErrorMessages;

		bConnectionOK = TestConnection(TestP4, NewClientSpecName, bIsUnicodeServer, ErrorMessages);
		if (!bConnectionOK)
		{
			//Login FAILED
			SourceControlLog.Error(LOCTEXT("P4ErrorConnection", "P4ERROR: Failed to connect to source control provider."));
			SourceControlLog.Error(ErrorMessages.Num() > 0 ? ErrorMessages[0] : LOCTEXT("P4ErrorConnection_InvalidWorkspace", "Invalid workspace"));
			FFormatNamedArguments Arguments;
			Arguments.Add( TEXT("PortName"), FText::FromString(NewServerName) );
			Arguments.Add( TEXT("UserName"), FText::FromString(NewUserName) );
			Arguments.Add( TEXT("ClientSpecName"), FText::FromString(NewClientSpecName) );
			Arguments.Add( TEXT("Ticket"), FText::FromString(InConnectionInfo.Ticket) );
			SourceControlLog.Error(FText::Format(LOCTEXT("P4ErrorConnection_Details", "Port={PortName}, User={UserName}, ClientSpec={ClientSpecName}, Ticket={Ticket}"), Arguments));
		}
	}

	//whether successful or not, disconnect to clean up
	TestP4.Final(&P4Error);
	if (bConnectionOK && P4Error.Test())
	{
		//Disconnect FAILED
		bConnectionOK = false;
		StrBuf ErrorMessage;
		P4Error.Fmt(&ErrorMessage);
		SourceControlLog.Error(LOCTEXT("P4ErrorFailedDisconnect", "P4ERROR: Failed to disconnect from Server."));
		SourceControlLog.Error(FText::FromString(TO_TCHAR(ErrorMessage.Text(), bIsUnicodeServer)));
	}

	//if never specified, take the default connection values
	if (NewServerName.Len() == 0)
	{
		NewServerName = TO_TCHAR(TestP4.GetPort().Text(), bIsUnicodeServer);
	}
	if (NewUserName.Len() == 0)
	{
		NewUserName = TO_TCHAR(TestP4.GetUser().Text(), bIsUnicodeServer);
	}
	if (NewClientSpecName.Len() == 0)
	{
		NewClientSpecName = TO_TCHAR(TestP4.GetClient().Text(), bIsUnicodeServer);
		if (NewClientSpecName == TO_TCHAR(TestP4.GetHost().Text(), bIsUnicodeServer))
		{
			// If the client spec name is the same as host name, assume P4 couldn't get the actual
			// spec name for this host and let GetPerforceLogin() try to find a proper one.
			bConnectionOK = false;
		}
	}

	if (bConnectionOK)
	{
		InOutServerName = NewServerName;
		InOutUserName = NewUserName;
		InOutWorkspaceName = NewClientSpecName;
	}
#endif

	return bConnectionOK;
}

bool FPerforceConnection::GetWorkspaceList(const FPerforceConnectionInfo& InConnectionInfo, FOnIsCancelled InOnIsCanceled, TArray<FString>& OutWorkspaceList, TArray<FText>& OutErrorMessages)
{
	if(bEstablishedConnection)
	{
		TArray<FString> Params;
		bool bAllowWildHosts = !GIsBuildMachine;
		Params.Add(TEXT("-u"));
		Params.Add(InConnectionInfo.UserName);

		FP4RecordSet Records;
		bool bConnectionDropped = false;
		bool bCommandOK = RunCommand(TEXT("clients"), Params, Records, OutErrorMessages, InOnIsCanceled, bConnectionDropped);

		if (bCommandOK)
		{
			FString ApplicationPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectDir()).ToLower();

			FString LocalHostName = InConnectionInfo.HostOverride;
			if(LocalHostName.Len() == 0)
			{
				// No host override, check environment variable
				TCHAR P4HostEnv[256];
				FPlatformMisc::GetEnvironmentVariable(TEXT("P4HOST"), P4HostEnv, ARRAY_COUNT(P4HostEnv));
				LocalHostName = P4HostEnv;
			}

			if (LocalHostName.Len() == 0)
			{
				// no host name, use local machine name
				LocalHostName = FString(FPlatformProcess::ComputerName()).ToLower();
			}
			else
			{
				LocalHostName = LocalHostName.ToLower();
			}

			for (int32 Index = 0; Index < Records.Num(); ++Index)
			{
				const FP4Record& ClientRecord = Records[Index];
				FString ClientName = ClientRecord("client");
				FString HostName = ClientRecord("Host");
				FString ClientRootPath = ClientRecord("Root").ToLower();

				//this clientspec has to be meant for this machine ( "" hostnames mean any host can use ths clientspec in p4 land)
				bool bHostNameMatches = (LocalHostName == HostName.ToLower());
				bool bHostNameWild = (HostName.Len() == 0);

				if( bHostNameMatches || (bHostNameWild && bAllowWildHosts) )
				{
					// A workspace root could be "null" which allows the user to map depot locations to different drives.
					// Allow these workspaces since we already allow workspaces mapped to drive letters.
					const bool bIsNullClientRootPath = (ClientRootPath == TEXT("null"));

					//make sure all slashes point the same way
					ClientRootPath = ClientRootPath.Replace(TEXT("\\"), TEXT("/"));
					ApplicationPath = ApplicationPath.Replace(TEXT("\\"), TEXT("/"));

					if (!ClientRootPath.EndsWith(TEXT("/")))
					{
						ClientRootPath += TEXT("/");
					}

					// Only allow paths that are ACTUALLY legit for this application
					if (bIsNullClientRootPath || ApplicationPath.Contains(ClientRootPath) )
					{
						OutWorkspaceList.Add(ClientName);
					}
					else
					{
						UE_LOG(LogSourceControl, Error, TEXT(" %s client specs rejected due to root directory mismatch (%s)"), *ClientName, *ClientRootPath);
					}

					//Other useful fields: Description, Owner, Host

				}
				else
				{
					UE_LOG(LogSourceControl, Error, TEXT(" %s client specs rejected due to host name mismatch (%s)"), *ClientName, *HostName);
				}
			}
		}

		return bCommandOK;
	}

	return false;
}

bool FPerforceConnection::IsValidConnection()
{
#if USE_P4_API
	return bEstablishedConnection && !P4Client.Dropped();
#else
	return false;
#endif
}

void FPerforceConnection::Disconnect()
{
#if USE_P4_API
	Error P4Error;

	P4Client.Final(&P4Error);

	if (P4Error.Test())
	{
		StrBuf ErrorMessage;
		P4Error.Fmt(&ErrorMessage);
		UE_LOG(LogSourceControl, Error, TEXT("P4ERROR: Failed to disconnect from Server."));
		UE_LOG(LogSourceControl, Error, TEXT("%s"), TO_TCHAR(ErrorMessage.Text(), bIsUnicode));
	}
#endif
}

bool FPerforceConnection::RunCommand(const FString& InCommand, const TArray<FString>& InParameters, FP4RecordSet& OutRecordSet, TArray<FText>& OutErrorMessage, FOnIsCancelled InIsCancelled, bool& OutConnectionDropped, const bool bInStandardDebugOutput, const bool bInAllowRetry)
{
#if USE_P4_API
	if (!bEstablishedConnection)
	{
		return false;
	}

	FString FullCommand = InCommand;

	// Prepare arguments
	int32 ArgC = InParameters.Num();
	UTF8CHAR** ArgV = new UTF8CHAR*[ArgC];
	for (int32 Index = 0; Index < ArgC; Index++)
	{
		if(bIsUnicode)
		{
			FTCHARToUTF8 UTF8String(*InParameters[Index]);
			ArgV[Index] = new UTF8CHAR[UTF8String.Length() + 1];
			FMemory::Memcpy(ArgV[Index], UTF8String.Get(), UTF8String.Length() + 1);
		}
		else
		{
			ArgV[Index] = new UTF8CHAR[InParameters[Index].Len() + 1];
			FMemory::Memcpy(ArgV[Index], TCHAR_TO_ANSI(*InParameters[Index]), InParameters[Index].Len() + 1);
		}
		
		if (bInStandardDebugOutput)
		{
			FullCommand += TEXT(" ");
			FullCommand += InParameters[Index];
		}
	}

	if (bInStandardDebugOutput)
	{
		UE_LOG( LogSourceControl, Log, TEXT("Attempting 'p4 %s'"), *FullCommand );
	}

	double SCCStartTime = FPlatformTime::Seconds();

	P4Client.SetArgv(ArgC, (char**)ArgV);

	FP4KeepAlive KeepAlive(InIsCancelled);
	P4Client.SetBreak(&KeepAlive);

	OutRecordSet.Reset();
	FP4ClientUser User(OutRecordSet, bIsUnicode, OutErrorMessage);
	P4Client.Run(FROM_TCHAR(*InCommand, bIsUnicode), &User);
	if ( P4Client.Dropped() )
	{
		OutConnectionDropped = true;
	}

	P4Client.SetBreak(NULL);

	// Free arguments
	for (int32 Index = 0; Index < ArgC; Index++)
	{
		delete [] ArgV[Index];
	}
	delete [] ArgV;

	if (bInStandardDebugOutput)
	{
		UE_LOG( LogSourceControl, VeryVerbose, TEXT("P4 execution time: %0.4f seconds. Command: %s"), FPlatformTime::Seconds() - SCCStartTime, *FullCommand );
	}
#endif

	return OutRecordSet.Num() > 0;
}

int32 FPerforceConnection::CreatePendingChangelist(const FText &Description, FOnIsCancelled InIsCancelled, TArray<FText>& OutErrorMessages)
{
#if USE_P4_API
	TArray<FString> Params;
	FP4RecordSet Records;

	const char *ArgV[] = { "-i" };
	P4Client.SetArgv(1, const_cast<char*const*>(ArgV));

	FP4KeepAlive KeepAlive(InIsCancelled);
	P4Client.SetBreak(&KeepAlive);

	FP4CreateChangelistClientUser User(Records, bIsUnicode, OutErrorMessages, Description, P4Client);
	P4Client.Run("change", &User);

	P4Client.SetBreak(NULL);

	return User.ChangelistNumber;
#else
	return 0;
#endif
}

void FPerforceConnection::EstablishConnection(const FPerforceConnectionInfo& InConnectionInfo)
{
#if USE_P4_API
	// Verify Input. ServerName and UserName are required
	if ( InConnectionInfo.Port.IsEmpty() || InConnectionInfo.UserName.IsEmpty() )
	{
		return;
	}

	//Connection assumed successful
	bEstablishedConnection = true;

	UE_LOG(LogSourceControl, Verbose, TEXT("Attempting P4 connection: %s/%s"), *InConnectionInfo.Port, *InConnectionInfo.UserName);

	P4Client.SetProtocol("tag", "");
	P4Client.SetProtocol("enableStreams", "");

	//Set configuration based params
	P4Client.SetPort(TCHAR_TO_ANSI(*InConnectionInfo.Port));

	Error P4Error;
	if(InConnectionInfo.Password.Len() > 0)
	{
		UE_LOG(LogSourceControl, Verbose, TEXT(" ... applying password" ));
		P4Client.DefinePassword(TCHAR_TO_ANSI(*InConnectionInfo.Password), &P4Error);
		if(P4Error.Test())
		{
			StrBuf ErrorMessage;
			P4Error.Fmt(&ErrorMessage);
			UE_LOG(LogSourceControl, Error, TEXT("P4ERROR: Could not set password."));
			UE_LOG(LogSourceControl, Error, TEXT("%s"), ANSI_TO_TCHAR(ErrorMessage.Text()));
		}
	}

	if(InConnectionInfo.HostOverride.Len() > 0)
	{
		UE_LOG(LogSourceControl, Verbose, TEXT(" ... overriding host" ));
		P4Client.SetHost(TCHAR_TO_ANSI(*InConnectionInfo.HostOverride));
	}

	UE_LOG(LogSourceControl, Verbose, TEXT(" ... connecting" ));

	//execute the connection to perforce using the above settings
	P4Client.Init(&P4Error);

	//ensure the connection is valid
	UE_LOG(LogSourceControl, Verbose, TEXT(" ... validating connection" ));
	if (P4Error.Test())
	{
		bEstablishedConnection = false;
		StrBuf ErrorMessage;
		P4Error.Fmt(&ErrorMessage);

		UE_LOG(LogSourceControl, Error, TEXT("P4ERROR: Invalid connection to server."));
		UE_LOG(LogSourceControl, Error, TEXT("%s"), ANSI_TO_TCHAR(ErrorMessage.Text()));
	}
	else
	{
		TArray<FString> Params;
		TArray<FText> ErrorMessages;
		FP4RecordSet Records;
		bool bConnectionDropped = false;
		const bool bStandardDebugOutput = false;
		const bool bAllowRetry = true;

		UE_LOG(LogSourceControl, Verbose, TEXT(" ... checking unicode status" ));

		if (RunCommand(TEXT("info"), Params, Records, ErrorMessages, FOnIsCancelled(), bConnectionDropped, bStandardDebugOutput, bAllowRetry))
		{
			// Get character encoding
			bIsUnicode = Records[0].Find(TEXT("unicode")) != NULL;
			if(bIsUnicode)
			{
				P4Client.SetTrans(CharSetApi::UTF_8);
				UE_LOG(LogSourceControl, Verbose, TEXT(" server is unicode" ));
			}

			// Now we know our unicode status we can gather the client root
			P4Client.SetUser(FROM_TCHAR(*InConnectionInfo.UserName, bIsUnicode));

			if(InConnectionInfo.Password.Len() > 0)
			{
				Login(InConnectionInfo);
			}

			if (InConnectionInfo.Ticket.Len())
			{
				P4Client.SetPassword(FROM_TCHAR(*InConnectionInfo.Ticket, bIsUnicode));
			}
			if (InConnectionInfo.Workspace.Len())
			{
				P4Client.SetClient(FROM_TCHAR(*InConnectionInfo.Workspace, bIsUnicode));
			}

			P4Client.SetCwd(FROM_TCHAR(*FPaths::RootDir(), bIsUnicode));

			// Gather the client root
			UE_LOG(LogSourceControl, Verbose, TEXT(" ... getting info" ));
			bConnectionDropped = false;
			if (RunCommand(TEXT("info"), Params, Records, ErrorMessages, FOnIsCancelled(), bConnectionDropped, bStandardDebugOutput, bAllowRetry))
			{
				UE_LOG(LogSourceControl, Verbose, TEXT(" ... getting clientroot" ));
				ClientRoot = Records[0](TEXT("clientRoot"));

				//make sure all slashes point the same way
				ClientRoot = ClientRoot.Replace(TEXT("\\"), TEXT("/"));
			}
		}
	}
#endif
}

FScopedPerforceConnection::FScopedPerforceConnection( FPerforceSourceControlCommand& InCommand )
	: Connection(NULL)
	, Concurrency(InCommand.Concurrency)
{
	Initialize(InCommand.ConnectionInfo);
	InCommand.MarkConnectionAsSuccessful();
}

FScopedPerforceConnection::FScopedPerforceConnection( EConcurrency::Type InConcurrency, const FPerforceConnectionInfo& InConnectionInfo )
	: Connection(NULL)
	, Concurrency(InConcurrency)
{
	Initialize(InConnectionInfo);
}

void FScopedPerforceConnection::Initialize( const FPerforceConnectionInfo& InConnectionInfo )
{
	if(Concurrency == EConcurrency::Synchronous)
	{
		// Synchronous commands reuse the same persistent connection to reduce
		// the number of expensive connection attempts.
		FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::LoadModuleChecked<FPerforceSourceControlModule>("PerforceSourceControl");
		if ( PerforceSourceControl.GetProvider().EstablishPersistentConnection() )
		{
			Connection = PerforceSourceControl.GetProvider().GetPersistentConnection();
		}
	}
	else
	{
		// Async commands form a new connection for each attempt because
		// using the persistent connection is not threadsafe
		FPerforceConnection* NewConnection = new FPerforceConnection(InConnectionInfo);
		if ( NewConnection->IsValidConnection() )
		{
			Connection = NewConnection;
		}
	}
}

FScopedPerforceConnection::~FScopedPerforceConnection()
{
	if(Connection != NULL)
	{
		if(Concurrency == EConcurrency::Asynchronous)
		{
			// Remove this connection as it is only temporary
			Connection->Disconnect();
			delete Connection;
		}
		
		Connection = NULL;
	}
}

#undef LOCTEXT_NAMESPACE
