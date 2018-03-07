// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NetworkFileServerHttp.h"
#include "NetworkFileServerConnection.h"
#include "NetworkFileSystemLog.h"
#include "NetworkMessage.h"
#include "HAL/RunnableThread.h"
#include "SocketSubsystem.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "IPAddress.h"
#include "Serialization/MemoryReader.h"

#if ENABLE_HTTP_FOR_NFS

class FNetworkFileServerClientConnectionHTTP : public FNetworkFileServerClientConnection
{

public:
	FNetworkFileServerClientConnectionHTTP(const FNetworkFileDelegateContainer* NetworkFileDelegates, const TArray<ITargetPlatform*>& InActiveTargetPlatforms )
		:  FNetworkFileServerClientConnection( NetworkFileDelegates, InActiveTargetPlatforms)
	{
	}

	bool SendPayload( TArray<uint8> &Out )
	{
		// Make Boundaries between payloads, add a visual marker for easier debugging.

//		uint32 Marker = 0xDeadBeef;
//		uint32 Size = Out.Num();
//
//		OutBuffer.Append((uint8*)&Marker,sizeof(uint32));
//		OutBuffer.Append((uint8*)&Size,sizeof(uint32));
		OutBuffer.Append(Out);

		return true;
	}

private:

	TArray<uint8>& GetOutBuffer() { return OutBuffer; }
	void ResetBuffer() { OutBuffer.Reset(); }

	TArray<uint8> OutBuffer;

	friend class FNetworkFileServerHttp;
};


//////////////////////////////////////////////////////////////////////////
// LibWebsockets specific structs.

// a object of this type is associated by libwebsocket to every http session.
struct PerSessionData
{
	// data being received.
	TArray<uint8> In;
	// data being sent out.
	TArray<uint8> Out;
};


// protocol array.
static struct lws_protocols Protocols[] = {
	/* first protocol must always be HTTP handler */
	{
			"http-only",                           // name
			FNetworkFileServerHttp::CallBack_HTTP, // callback
			sizeof(PerSessionData),                // per_session_data_size
			15 * 1024,                             // rx_buffer_size
			0,                                     // id
			NULL
	},
	{
		NULL, NULL, 0, 0, 0, NULL   /* End of list */
	}
};
//////////////////////////////////////////////////////////////////////////


FNetworkFileServerHttp::FNetworkFileServerHttp(
	int32 InPort,
	FNetworkFileDelegateContainer InNetworkFileDelegateContainer,
	const TArray<ITargetPlatform*>& InActiveTargetPlatforms
	)
	:ActiveTargetPlatforms(InActiveTargetPlatforms)
	,Port(InPort)
{
	if (Port < 0 )
	{
		Port = DEFAULT_HTTP_FILE_SERVING_PORT;
	}

	UE_LOG(LogFileServer, Warning, TEXT("Unreal Network Http File Server starting up..."));

	NetworkFileDelegates = InNetworkFileDelegateContainer;

	StopRequested.Reset();
	Ready.Reset();

	// spin up the worker thread, this will block till Init has executed on the freshly spinned up thread, Ready will have appropriate value
	// set by the end of this function.
	WorkerThread = FRunnableThread::Create(this,TEXT("FNetworkFileServerHttp"), 8 * 1024, TPri_AboveNormal);
}


bool FNetworkFileServerHttp::IsItReadyToAcceptConnections(void) const
{
	return (Ready.GetValue() != 0);
}

#if UE_BUILD_DEBUG
inline void lws_debugLog(int level, const char *line)
{
	UE_LOG(LogFileServer, Warning, TEXT(" LibWebsocket: %s"), ANSI_TO_TCHAR(line));
}
#endif

bool FNetworkFileServerHttp::Init()
{
	// setup log level.
#if UE_BUILD_DEBUG
	lws_set_log_level( LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_DEBUG , lws_debugLog);
#endif

	struct lws_context_creation_info Info;
	memset(&Info,0,sizeof(lws_context_creation_info));

	// look up libwebsockets.h for details.
	Info.port = Port;
	// we listen on all available interfaces.
	Info.iface = NULL;
	// serve only the http protocols.
	Info.protocols = Protocols;
	// no extensions
	Info.extensions = NULL;
	Info.gid = -1;
	Info.uid = -1;
	Info.options = 0;
	// tack on this object.
	Info.user = this;

	Context = lws_create_context(&Info);

	Port = Info.port;

	if (Context == NULL) {
		UE_LOG(LogFileServer, Error, TEXT(" Could not create a libwebsocket content.\n Port : %d is already in use.\n Exiting...\n"), Port);
		return false;
	}

	Ready.Set(true);
	return true;
}



FString FNetworkFileServerHttp::GetSupportedProtocol() const
{
	return FString("http");
}


bool FNetworkFileServerHttp::GetAddressList(TArray<TSharedPtr<FInternetAddr> >& OutAddresses) const
{
	// if Init failed, its already too late.
	ensure( Context != nullptr);

	// we are listening to all local interfaces.
	ISocketSubsystem::Get()->GetLocalAdapterAddresses(OutAddresses);
	// Fix up ports.
	for (int32 AddressIndex = 0; AddressIndex < OutAddresses.Num(); ++AddressIndex)
	{
		OutAddresses[AddressIndex]->SetPort(Port);
	}

	return true;
}

int32 FNetworkFileServerHttp::NumConnections() const
{
	return	RequestHandlers.Num();
}

void FNetworkFileServerHttp::Shutdown()
{
	// Allow multiple calls to this function.
	if ( WorkerThread )
	{
		WorkerThread->Kill(true); // Kill Nicely. Wait for everything to shutdown.
		delete WorkerThread;
		WorkerThread = NULL;
	}
}

uint32 FNetworkFileServerHttp::Run()
{
	UE_LOG(LogFileServer, Display, TEXT("Unreal Network File Http Server is ready for client connections on port %d"), Port);

	// start servicing.

	// service libwebsocket context.
	while(!StopRequested.GetValue())
	{
		// service libwebsocket, have a slight delay so it doesn't spin on zero load.
		lws_service(Context, 10);
		lws_callback_on_writable_all_protocol(Context, &Protocols[0]);
	}

	UE_LOG(LogFileServer, Display, TEXT("Unreal Network File Http Server is now Shutting down "));
	return true;
}

// Called internally by FRunnableThread::Kill.
void FNetworkFileServerHttp::Stop()
{
	StopRequested.Set(true);
}

void FNetworkFileServerHttp::Exit()
{
	// let's start shutting down.
	// fires a LWS_CALLBACK_PROTOCOL_DESTROY callback, we clean up after ourselves there.
	lws_context_destroy(Context);
	Context = NULL;
}

FNetworkFileServerHttp::~FNetworkFileServerHttp()
{
	Shutdown();
	// delete our request handlers.
	for ( auto& Element : RequestHandlers)
	{
		delete Element.Value;
	}
	// make sure context has been already cleaned up.
	check( Context == NULL );
}

FNetworkFileServerClientConnectionHTTP* FNetworkFileServerHttp::CreateNewConnection()
{
	return new FNetworkFileServerClientConnectionHTTP(&NetworkFileDelegates,ActiveTargetPlatforms);
}

// Have a similar process function for the normal tcp connection.
void FNetworkFileServerHttp::Process(FArchive& In, TArray<uint8>&Out, FNetworkFileServerHttp* Server)
{
	int loops = 0;
	while(!In.AtEnd())
	{
		UE_LOG(LogFileServer, Log, TEXT("In %d "), loops++);
		// Every Request has a Guid attached to it - similar to Web session IDs.
		FGuid ClientGuid;
		In << ClientGuid;

		UE_LOG(LogFileServer, Log, TEXT("Recieved GUID %s"), *ClientGuid.ToString());

		FNetworkFileServerClientConnectionHTTP* Connection = NULL;
		if (Server->RequestHandlers.Contains(ClientGuid))
		{
			UE_LOG(LogFileServer, Log, TEXT("Picking up an existing handler" ));
			Connection = Server->RequestHandlers[ClientGuid];
		}
		else
		{
			UE_LOG(LogFileServer, Log, TEXT("Creating a handler" ));
			Connection = Server->CreateNewConnection();
			Server->RequestHandlers.Add(ClientGuid,Connection);
		}

		Connection->ProcessPayload(In);
		Out.Append(Connection->GetOutBuffer());
		Connection->ResetBuffer();
	}
}

// This static function handles all callbacks coming in and when context is services via lws_service
// return value of -1, closes the connection.
int FNetworkFileServerHttp::CallBack_HTTP(
			struct lws *Wsi,
			enum lws_callback_reasons Reason,
			void *User,
			void *In,
			size_t Len)
{
	struct lws_context *Context = lws_get_context(Wsi);
	PerSessionData* BufferInfo = (PerSessionData*)User;
	FNetworkFileServerHttp* Server = (FNetworkFileServerHttp*)lws_context_user(Context);

	switch (Reason)
	{

	case LWS_CALLBACK_HTTP:

		// hang on to socket even if there's no data for atleast 60 secs.
		lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 60);

		/* if it was not legal POST URL, let it continue and accept data */
		if (!lws_hdr_total_length(Wsi, WSI_TOKEN_POST_URI))
		{
			char *requested_uri = (char *) In;

			// client request the base page. e.g  http://unrealfileserver:port/
			// just return a banner, probably add some more information, e,g Version, Config, Game. etc.
			if ( FCString::Strcmp(ANSI_TO_TCHAR(requested_uri), TEXT("/")) == 0 )
			{
				TCHAR Buffer[1024];
				TCHAR ServerBanner[] = TEXT("<HTML>This is Unreal File Server</HTML>");
				int x = FCString::Sprintf(
					Buffer,
					TEXT("HTTP/1.0 200 OK\x0d\x0a")
					TEXT("Server: Unreal File Server\x0d\x0a")
					TEXT("Connection: close\x0d\x0a")
					TEXT("Content-Type: text/html; charset=utf-8\x0d\x0a")
					TEXT("Content-Length: %u\x0d\x0a\x0d\x0a%s"),
					FCString::Strlen(ServerBanner),
					ServerBanner
					);

				// very small data being sent, its fine to just send.
				lws_write(Wsi,(unsigned char*)TCHAR_TO_ANSI(Buffer),FCStringAnsi::Strlen(TCHAR_TO_ANSI(Buffer)), LWS_WRITE_HTTP);
			}
			else
			{
				// client has asked for a file. ( only html/js files are served.)

				// what type is being served.
				FString FilePath = FPaths::ProjectDir() / TEXT("Binaries/HTML5") + FString((ANSICHAR*)In);
				TCHAR Mime[512];


				if ( FilePath.Contains(".js"))
				{
					FCStringWide::Strcpy(Mime,TEXT("application/javascript;charset=UTF-8"));
				}
				else
				{
					FCStringWide::Strcpy(Mime,TEXT("text/html;charset=UTF-8"));
				}

				UE_LOG(LogFileServer, Warning, TEXT("HTTP Serving file %s with mime %s "), *FilePath, (Mime));

				FString AbsoluteFilePath = FPaths::ConvertRelativePathToFull(FilePath);
				AbsoluteFilePath.ReplaceInline(TEXT("/"),TEXT("\\"));

				// we are going to read the complete file in memory and then serve it in batches.
				// rather than reading and sending in batches because Unreal NFS servers are not running in memory
				// constrained env and the added complexity is not worth it.


				TArray<uint8> FileData;
				FFileHelper::LoadFileToArray(FileData, *AbsoluteFilePath, FILEREAD_Silent);

				if (FileData.Num() == 0)
				{
					// umm. we didn't find file, we should tell the client that we couldn't find it.
					// send 404.
					TCHAR Buffer[1024];
					TCHAR ServerBanner[] = TEXT("<HTML>Not Found</HTML>");
					int x = FCString::Sprintf(
						Buffer,
						TEXT("HTTP/1.0 404 Not Found\x0d\x0a")
						TEXT("Server: Unreal File Server\x0d\x0a")
						TEXT("Connection: close\x0d\x0a")
						TEXT("Content-Type: text/html; charset=utf-8\x0d\x0a")
						TEXT("Content-Length: %u\x0d\x0a\x0d\x0a%s"),
						FCString::Strlen(ServerBanner),
						ServerBanner
						);
					lws_write(Wsi,(unsigned char*)TCHAR_TO_ANSI(Buffer),FCStringAnsi::Strlen(TCHAR_TO_ANSI(Buffer)), LWS_WRITE_HTTP);
					// chug along, client will close the connection.
					break;
				}

				// file up the header.
				TCHAR Header[1024];
				int Length = 0;
				if (FilePath.Contains("gz"))
				{
					Length = FCString::Sprintf(Header,
						TEXT("HTTP/1.1 200 OK\x0d\x0a")
						TEXT("Server: Unreal File Server\x0d\x0a")
						TEXT("Connection: close\x0d\x0a")
						TEXT("Content-Type: %s \x0d\x0a")
						TEXT("Content-Encoding: gzip\x0d\x0a")
						TEXT("Content-Length: %u\x0d\x0a\x0d\x0a"),
						Mime, FileData.Num());
				}
				else
				{
					Length = FCString::Sprintf(Header,
						TEXT("HTTP/1.1 200 OK\x0d\x0a")
						TEXT("Server: Unreal File Server\x0d\x0a")
						TEXT("Connection: close\x0d\x0a")
						TEXT("Content-Type: %s \x0d\x0a")
						TEXT("Content-Length: %u\x0d\x0a\x0d\x0a"),
						Mime, FileData.Num());
				}

				// make space for the whole file in our out buffer.
				BufferInfo->Out.Append((uint8*)TCHAR_TO_ANSI(Header),Length);
				BufferInfo->Out.Append(FileData);
				// we need to write back to the client, queue up a write callback.
				lws_callback_on_writable(Wsi);
			}
		}
		else
		{
			// we got a post request!, queue up a write callback.
			lws_callback_on_writable(Wsi);
		}

		break;
	case LWS_CALLBACK_HTTP_BODY:
		{
			// post data is coming in, push it on to our incoming buffer.
			UE_LOG(LogFileServer, Log, TEXT("Incoming HTTP Partial Body Size %d, total size  %d"),Len, Len+ BufferInfo->In.Num());
			BufferInfo->In.Append((uint8*)In,Len);
			// we received some data - update time out.
			lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 60);
		}
		break;
	case LWS_CALLBACK_HTTP_BODY_COMPLETION:
		{
			// we have all the post data from the client.
			// create archives and process them.
			UE_LOG(LogFileServer, Log, TEXT("Incoming HTTP total size  %d"), BufferInfo->In.Num());
			FMemoryReader Reader(BufferInfo->In);
			TArray<uint8> Writer;

			FNetworkFileServerHttp::Process(Reader,Writer,Server);

			// even if we have 0 data to push, tell the client that we don't any data.
			ANSICHAR Header[1024];
			int Length = FCStringAnsi::Sprintf(
				(ANSICHAR*)Header,
				"HTTP/1.1 200 OK\x0d\x0a"
				"Server: Unreal File Server\x0d\x0a"
				"Connection: close\x0d\x0a"
				"Content-Type: application/octet-stream \x0d\x0a"
				"Content-Length: %u\x0d\x0a\x0d\x0a",
				Writer.Num()
				);

			// Add Http Header
			BufferInfo->Out.Append((uint8*)Header,Length);
			// Add Binary Data.
			BufferInfo->Out.Append(Writer);

			// we have enqueued data increase timeout and push a writable callback.
			lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 60);
			lws_callback_on_writable(Wsi);

		}
		break;
	case LWS_CALLBACK_CLOSED_HTTP:

		if ( BufferInfo == NULL )
			break;

		// client went away or
		//clean up.
		BufferInfo->In.Empty();
		BufferInfo->Out.Empty();

		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		// we are going away.

		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:

		// get rid of superfluous write callbacks.
		if ( BufferInfo == NULL )
			break;

		// we have data o send out.
		if (BufferInfo->Out.Num())
		{
			int SentSize = lws_write(Wsi,(unsigned char*)BufferInfo->Out.GetData(),BufferInfo->Out.Num(), LWS_WRITE_HTTP);
			// get rid of the data that has been sent.
			BufferInfo->Out.RemoveAt(0,SentSize);
		}

		break;

	default:
		break;
	}

	return 0;
}

#endif
