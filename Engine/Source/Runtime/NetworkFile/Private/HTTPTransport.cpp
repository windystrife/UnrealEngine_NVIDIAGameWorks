// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTTPTransport.h"

#if ENABLE_HTTP_FOR_NF

#include "Serialization/BufferArchive.h"
#include "NetworkMessage.h"

#if PLATFORM_HTML5
#include "HTML5JavaScriptFx.h"
#include <emscripten/emscripten.h>
#endif


FHTTPTransport::FHTTPTransport()
	:Guid(FGuid::NewGuid())
{
}

bool FHTTPTransport::Initialize(const TCHAR* InHostIp)
{
	// parse out the format
	FString HostIp = InHostIp;

	// make sure that we have the correct protcol
	ensure( HostIp.RemoveFromStart("http://") );

	// strip webserver port
	if ( HostIp.Contains(":") )
	{
		HostIp = HostIp.Left(HostIp.Find(":"));
	}
	// append file server port
	HostIp = FString::Printf(TEXT("%s:%d"), *HostIp, (int)(DEFAULT_HTTP_FILE_SERVING_PORT) );

	// make sure that our string is again correctly formated
	HostIp = FString::Printf(TEXT("http://%s"),*HostIp);

	FCString::Sprintf(Url, *HostIp);

#if !PLATFORM_HTML5
	HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
#else
	emscripten_log(EM_LOG_CONSOLE , "Unreal File Server URL : %s ", TCHAR_TO_ANSI(Url));
#endif

	TArray<uint8> In,Out;
	bool RetResult = SendPayloadAndReceiveResponse(In,Out);
	return RetResult;
}

bool FHTTPTransport::SendPayloadAndReceiveResponse(TArray<uint8>& In, TArray<uint8>& Out)
{
	RecieveBuffer.Empty();
	ReadPtr = 0;

#if !PLATFORM_HTML5

	if (GIsRequestingExit) // We have already lost HTTP Module.
		return false;

	class HTTPRequestHandler
	{

	public:
		HTTPRequestHandler(TArray<uint8>& InOut)
			:Out(InOut)
		{}
		void HttpRequestComplete(	FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			if (HttpResponse.IsValid())
				Out.Append(HttpResponse->GetContent());
		}
	private:
		TArray<uint8>& Out;
	};

	HTTPRequestHandler Handler(RecieveBuffer);

	HttpRequest->OnProcessRequestComplete().BindRaw(&Handler,&HTTPRequestHandler::HttpRequestComplete );
	if ( In.Num() )
	{
		HttpRequest->SetVerb("POST");

		FBufferArchive Ar;
		Ar << Guid;
		Ar.Append(In);

		HttpRequest->SetContent(Ar);
	}
	else
	{
		HttpRequest->SetVerb("GET");
	}

	HttpRequest->ProcessRequest();

	FDateTime StartTime;
	FTimespan Span = FDateTime::UtcNow() - StartTime;

	while( HttpRequest->GetStatus() <= EHttpRequestStatus::Processing &&  Span.GetSeconds() < 10 )
	{
		HttpRequest->Tick(0);
		Span = FDateTime::UtcNow() - StartTime;
	}


	if (HttpRequest->GetStatus() == EHttpRequestStatus::Succeeded)
		return true;

	HttpRequest->CancelRequest();

	return false;
#else  // PLATFORM_HTML5
	FBufferArchive Ar;
	if ( In.Num() )
	{
		Ar << Guid;
		Ar.Append(In);
	}

	unsigned char *OutData = NULL;
	unsigned int OutSize= 0;

	bool RetVal = true;

	UE_SendAndRecievePayLoad(TCHAR_TO_ANSI(Url),(char*)Ar.GetData(),Ar.Num(),(char**)&OutData,(int*)&OutSize);

//	if (!Ar.Num())
	{
		uint32 Size = OutSize;
		uint32 Marker = 0xDeadBeef;
		RecieveBuffer.Append((uint8*)&Marker,sizeof(uint32));
		RecieveBuffer.Append((uint8*)&Size,sizeof(uint32));
	}

	if (OutSize)
	{
		RecieveBuffer.Append(OutData,OutSize);

		// don't go through the Unreal Memory system.
		::free(OutData);
	}

	return RetVal & ReceiveResponse(Out);
#endif
}

bool FHTTPTransport::ReceiveResponse(TArray<uint8> &Out)
{
	// Read one Packet from Receive Buffer.
	// read the size.

	uint32 Marker = *(uint32*)(RecieveBuffer.GetData() + ReadPtr);
	uint32 Size = *(uint32*)(RecieveBuffer.GetData() + ReadPtr + sizeof(uint32));

	// make sure we have the right amount of data available in the buffer.
	check( (ReadPtr + Size + 2*sizeof(uint32)) <= RecieveBuffer.Num());

	Out.Append(RecieveBuffer.GetData() + ReadPtr + 2*sizeof(uint32),Size);

	ReadPtr += 2*sizeof(uint32) + Size;

	return true;
}

#endif // ENABLE_HTTP_FOR_NF
