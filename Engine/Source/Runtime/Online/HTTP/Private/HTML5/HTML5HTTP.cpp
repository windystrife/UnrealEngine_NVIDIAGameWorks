// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5HTTP.h"
#include "EngineVersion.h"
#include "Http.h"
#include "HttpManager.h"
#include "Misc/App.h"

#include "HTML5JavaScriptFx.h"

/****************************************************************************
* FHTML5HttpRequest implementation
***************************************************************************/

FHTML5HttpRequest::FHTML5HttpRequest()
	:	bCanceled(false)
	,	bCompleted(false)
	,	BytesSent(0)
	,	CompletionStatus(EHttpRequestStatus::NotStarted)
	,	ElapsedTime(0.0f)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::FHTML5HttpRequest()"));
}


FHTML5HttpRequest::~FHTML5HttpRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::~FHTML5HttpRequest()"));
}


FString FHTML5HttpRequest::GetURL()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::GetURL() - %s"), *URL);
	return URL;
}


void FHTML5HttpRequest::SetURL(const FString& InURL)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::SetURL() - %s"), *InURL);
	URL = InURL;
}


FString FHTML5HttpRequest::GetURLParameter(const FString& ParameterName)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::GetURLParameter() - %s"), *ParameterName);

	TArray<FString> StringElements;

	int32 NumElems = URL.ParseIntoArray(StringElements, TEXT("&"), true);
	check(NumElems == StringElements.Num());

	FString ParamValDelimiter(TEXT("="));
	for (int Idx = 0; Idx < NumElems; ++Idx )
	{
		FString Param, Value;
		if (StringElements[Idx].Split(ParamValDelimiter, &Param, &Value) && Param == ParameterName)
		{
			return Value;
		}
	}

	return FString();
}


FString FHTML5HttpRequest::GetHeader(const FString& HeaderName)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::GetHeader() - %s"), *HeaderName);

	FString Result;

	FString* Header = Headers.Find(HeaderName);

	if (Header != NULL)
	{
		Result = *Header;
	}

	return Result;
}


void FHTML5HttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::SetHeader() - %s / %s"), *HeaderName, *HeaderValue);
	Headers.Add(HeaderName, HeaderValue);
}


TArray<FString> FHTML5HttpRequest::GetAllHeaders()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::GetAllHeaders()"));

	TArray<FString> Result;
	for (TMap<FString, FString>::TConstIterator It(Headers); It; ++It)
	{
		Result.Add(It.Key() + TEXT(": ") + It.Value());
	}
	return Result;
}


const TArray<uint8>& FHTML5HttpRequest::GetContent()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::GetContent()"));

	return RequestPayload;
}


void FHTML5HttpRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::SetContent()"));

	RequestPayload = ContentPayload;
}


FString FHTML5HttpRequest::GetContentType()
{
	return GetHeader(TEXT( "Content-Type" ));
}


int32 FHTML5HttpRequest::GetContentLength()
{
	int Len = RequestPayload.Num();
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::GetContentLength() - %i"), Len);

	return Len;
}


void FHTML5HttpRequest::SetContentAsString(const FString& ContentString)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::SetContentAsString() - %s"), *ContentString);

	FTCHARToUTF8 Converter(*ContentString);
	RequestPayload.SetNum(Converter.Length());
	FMemory::Memcpy(RequestPayload.GetData(), (uint8*)(ANSICHAR*)Converter.Get(), RequestPayload.Num());
}


void FHTML5HttpRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
	if (!HeaderName.IsEmpty() && !AdditionalHeaderValue.IsEmpty())
	{
		FString* PreviousValue = Headers.Find(HeaderName);
		FString NewValue;
		if (PreviousValue != nullptr && !PreviousValue->IsEmpty())
		{
			NewValue = (*PreviousValue) + TEXT(", ");
		}
		NewValue += AdditionalHeaderValue;

		SetHeader(HeaderName, NewValue);
	}
}

FString FHTML5HttpRequest::GetVerb()
{
	return Verb;
}


void FHTML5HttpRequest::SetVerb(const FString& InVerb)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::SetVerb() - %s"), *InVerb);

	Verb = InVerb.ToUpper();
}

bool IsURLEncoded(const TArray<uint8> & Payload)
{
	static char AllowedChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
	static bool bTableFilled = false;
	static bool AllowedTable[256] = { false };

	if (!bTableFilled)
	{
		for (int32 Idx = 0; Idx < ARRAY_COUNT(AllowedChars) - 1; ++Idx)	// -1 to avoid trailing 0
		{
			uint8 AllowedCharIdx = static_cast<uint8>(AllowedChars[Idx]);
			check(AllowedCharIdx < ARRAY_COUNT(AllowedTable));
			AllowedTable[AllowedCharIdx] = true;
		}

		bTableFilled = true;
	}

	const int32 Num = Payload.Num();
	for (int32 Idx = 0; Idx < Num; ++Idx)
	{
		if (!AllowedTable[Payload[Idx]])
			return false;
	}

	return true;
}

void FHTML5HttpRequest::StaticReceiveCallback(void *arg, void *buffer, uint32 size, void* httpHeaders)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::StaticReceiveDataCallback()"));

	FHTML5HttpRequest* Request = reinterpret_cast<FHTML5HttpRequest*>(arg);

	return Request->ReceiveCallback(arg, buffer, size, httpHeaders);
}

void FHTML5HttpRequest::ReceiveCallback(void *arg, void *buffer, uint32 size, void* httpHeaders)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::ReceiveDataCallback()"));
	UE_LOG(LogHttp, Verbose, TEXT("Response size: %d"), size);

	check(Response.IsValid());

	if (Response.IsValid())
	{
		Response->Payload.AddUninitialized(size);

		// headers
		FString Header(ANSI_TO_TCHAR(httpHeaders));
		Header = Header.Replace(TEXT("\r"), TEXT(""));
		FString HeaderChunk, HeaderLeftovers;
		while (Header.Split(TEXT("\n"), &HeaderChunk, &HeaderLeftovers))
		{
			if (!HeaderChunk.IsEmpty())
			{
				UE_LOG(LogHttp, Verbose, TEXT("%p: Received response header '%s'."), this, *HeaderChunk);

				FString HeaderKey, HeaderValue;
				if (HeaderChunk.Split(TEXT(":"), &HeaderKey, &HeaderValue))
				{
					FString* PreviousValue = Response->Headers.Find(HeaderKey);
					FString NewValue;
					if (PreviousValue != nullptr && !PreviousValue->IsEmpty())
					{
						NewValue = (*PreviousValue) + TEXT(", ");
					}
					HeaderValue.TrimStartInline();
					NewValue += HeaderValue;
					Response->Headers.Add(HeaderKey, NewValue);
				}
			}
			if (!HeaderLeftovers.IsEmpty())
			{
				Header = HeaderLeftovers;
			}
			else
			{
				break;
			}
		}

		// save
		UE_LOG(LogHttp, Verbose, TEXT("Saving payload..."));

		FMemory::Memcpy(static_cast<uint8*>(Response->Payload.GetData()), buffer, size);
		Response->TotalBytesRead = size;
		Response->HttpCode = 200;

		UE_LOG(LogHttp, Verbose, TEXT("Payload length: %d"), Response->Payload.Num());

		MarkAsCompleted();
	}
}

void FHTML5HttpRequest::StaticErrorCallback(void* arg, int httpStatusCode, const char* httpStatusText)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::StaticErrorDataCallback()"));

	FHTML5HttpRequest* Request = reinterpret_cast<FHTML5HttpRequest*>(arg);
	return Request->ErrorCallback(arg, httpStatusCode, httpStatusText);
}

void FHTML5HttpRequest::ErrorCallback(void* arg, int httpStatusCode, const char* httpStatusText)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::ErrorDataCallback() HttpStatusCode: %d"), httpStatusCode);

	check(Response.IsValid());

	if (Response.IsValid())
	{
		Response->Payload.Empty();
		Response->TotalBytesRead = 0;
		Response->HttpCode = httpStatusCode;
		MarkAsCompleted();
	}

}

void FHTML5HttpRequest::StaticProgressCallback(void* arg, int Loaded, int Total)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::StaticProgressCallback()"));

	FHTML5HttpRequest* Request = reinterpret_cast<FHTML5HttpRequest*>(arg);

	return Request->ProgressCallback(arg, Loaded, Total);
}

void FHTML5HttpRequest::ProgressCallback(void* arg, int Loaded, int Total)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::ProgressCallback()"));

	if (GetVerb() == TEXT("GET"))
	{
		if (Response.IsValid())
		{
			Response->TotalBytesRead = Loaded;
			OnRequestProgress().ExecuteIfBound(SharedThis(this), 0, Response->TotalBytesRead);
		}
	}
	else
	{
		BytesSent = Loaded;
		OnRequestProgress().ExecuteIfBound(SharedThis(this), BytesSent, 0);
	}

	UE_LOG(LogHttp, Verbose, TEXT("Loaded: %d, Total: %d"), Loaded, Total);
}

extern "C" void Register_OnBeforeUnload(void *ctx, void(*callback)(void*))
{
	UE_Register_OnBeforeUnload(ctx,callback);
}

extern "C" void UnRegister_OnBeforeUnload(void *ctx, void(*callback)(void*))
{
	UE_UnRegister_OnBeforeUnload(ctx,callback);
}

bool FHTML5HttpRequest::StartRequest()
{
	if( UE_LOG_ACTIVE(LogHttp, Verbose) )
	{
		const TCHAR* zurl = *URL;
		const TCHAR* zverb = *Verb;
		EM_ASM_({
			console.log( "FHTML5HttpRequest::StartRequest()" + $0);

			console.log( "- URL='" + $1 + "'");
			console.log( "- Verb='" + $2 + "'");
			console.log( "- Custom headers are " + $3 );
			console.log( "- Payload size=" + $4 );
		}, this, zurl, zverb, Headers.Num() ? TEXT("present") : TEXT("NOT present"), RequestPayload.Num());
	}

	if (!FHttpModule::Get().IsHttpEnabled())
	{
		UE_LOG(LogHttp, Verbose, TEXT("Http disabled. Skipping request. url=%s"), *GetURL());
		return false;
	}// Prevent overlapped requests using the same instance
	else if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
		return false;
	}
	// Nothing to do without a valid URL
	else if (URL.IsEmpty())
	{
		UE_LOG(LogHttp, Log, TEXT("Cannot process HTTP request: URL is empty"));
		return false;
	}


	//"User-Agent" && "Content-Length" are automatically set by the browser xhr request. We can't do much.

	// make a fake header, so server has some idea this is UE
	SetHeader(TEXT("X-UnrealEngine-Agent"), FString::Printf(TEXT("game=%s, engine=UE4, version=%s"), FApp::GetProjectName(), *FEngineVersion::Current().ToString()));

	// Add "Pragma: no-cache" to mimic WinInet behavior
	if (GetHeader("Pragma").IsEmpty())
	{
		SetHeader(TEXT("Pragma"), TEXT("no-cache"));
	}

	TArray<FString> AllHeaders = GetAllHeaders();

	// Create a String which emscripten can understand.
	FString RequestHeaders = FString::Join(AllHeaders, TEXT("%"));

	// set up verb (note that Verb is expected to be uppercase only)
	if (Verb == TEXT("POST"))
	{
		// If we don't pass any other Content-Type, RequestPayload is assumed to be URL-encoded by this time
		// (if we pass, don't check here and trust the request)
		check(!GetHeader("Content-Type").IsEmpty() || IsURLEncoded(RequestPayload));

		UE_MakeHTTPDataRequest(this, TCHAR_TO_ANSI(*URL), "POST", (char*)RequestPayload.GetData(), RequestPayload.Num(),TCHAR_TO_ANSI(*RequestHeaders), 1, 0, StaticReceiveCallback, StaticErrorCallback, StaticProgressCallback);
	}
	else if (Verb == TEXT("PUT"))
	{
		UE_LOG(LogHttp, Log, TEXT("TODO: PUT"));

		//TODO: PUT

		// reset the counter
		BytesSent = 0;
		return false;
	}
	else if (Verb == TEXT("GET"))
	{
		UE_MakeHTTPDataRequest(this, TCHAR_TO_ANSI(*URL), "GET", NULL, 0,TCHAR_TO_ANSI(*RequestHeaders), 1, 1, StaticReceiveCallback, StaticErrorCallback, StaticProgressCallback);
	}
	else if (Verb == TEXT("HEAD"))
	{
		UE_LOG(LogHttp, Log, TEXT("TODO: HEAD"));

		//TODO: HEAD
		return false;
	}
	else if (Verb == TEXT("DELETE"))
	{

		// If we don't pass any other Content-Type, RequestPayload is assumed to be URL-encoded by this time
		// (if we pass, don't check here and trust the request)
		check(!GetHeader("Content-Type").IsEmpty() || IsURLEncoded(RequestPayload));

		//TODO: DELETE

		UE_LOG(LogHttp, Log, TEXT("TODO: DELETE"));
		return false;
	}
	else
	{
		UE_LOG(LogHttp, Fatal, TEXT("Unsupported verb '%s"), *Verb);
		FPlatformMisc::DebugBreak();
	}

	return true;
}

bool FHTML5HttpRequest::ProcessRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::ProcessRequest()"));

	if (!StartRequest())
	{
		UE_LOG(LogHttp, Warning, TEXT("Processing HTTP request failed. Increase verbosity for additional information."));

		// No response since connection failed
		Response = NULL;
		// Cleanup and call delegate
		FinishedRequest();

		return false;
	}

	// Mark as in-flight to prevent overlapped requests using the same object
	CompletionStatus = EHttpRequestStatus::Processing;
	// Response object to handle data that comes back after starting this request
	Response = MakeShareable(new FHTML5HttpResponse(*this));
	// Add to global list while being processed so that the ref counted request does not get deleted
	FHttpModule::Get().GetHttpManager().AddRequest(SharedThis(this));
	// reset timeout
	ElapsedTime = 0.0f;

	UE_LOG(LogHttp, Verbose, TEXT("Request is waiting for processing"), this );

	return true;
}


FHttpRequestCompleteDelegate& FHTML5HttpRequest::OnProcessRequestComplete()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::OnProcessRequestComplete()"));
	return RequestCompleteDelegate;
}

FHttpRequestProgressDelegate& FHTML5HttpRequest::OnRequestProgress()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::OnRequestProgress()"));
	return RequestProgressDelegate;
}

void FHTML5HttpRequest::FinishedRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::FinishedRequest()"));

	// if completed, get more info
	if (bCompleted)
	{
		if (Response.IsValid())
		{
			Response->bSucceeded = EHttpResponseCodes::IsOk(Response->HttpCode);
			Response->ContentLength = Response->TotalBytesRead;
		}
	}

	// if just finished, mark as stopped async processing
	if (Response.IsValid())
	{
		Response->bIsReady = true;
	}

	// Clean up session/request handles that may have been created
	CleanupRequest();

	if (Response.IsValid() &&
		Response->bSucceeded)
	{
		UE_LOG(LogHttp, Verbose, TEXT("%p: request has been successfully processed. HTTP code: %d, content length: %d, actual payload size: %d"), 
			this, Response->HttpCode, Response->ContentLength, Response->Payload.Num() );

		// Mark last request attempt as completed successfully
		CompletionStatus = EHttpRequestStatus::Succeeded;
		// Call delegate with valid request/response objects
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this),Response,true);
	}
	else
	{
		// Mark last request attempt as completed but failed
		CompletionStatus = EHttpRequestStatus::Failed;
		// No response since connection failed
		Response = NULL;
		// Call delegate with failure
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), NULL, false);
	}
	// Remove from global list since processing is now complete
	FHttpModule::Get().GetHttpManager().RemoveRequest(SharedThis(this));
}


void FHTML5HttpRequest::CleanupRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::CleanupRequest()"));
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		CancelRequest();
	}
}


void FHTML5HttpRequest::CancelRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::CancelRequest()"));
	bCanceled = true;
}


EHttpRequestStatus::Type FHTML5HttpRequest::GetStatus()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::GetStatus()"));
	return CompletionStatus;
}

const FHttpResponsePtr FHTML5HttpRequest::GetResponse() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpRequest::GetResponse()"));
	return Response;
}


void FHTML5HttpRequest::Tick(float DeltaSeconds)
{
	// check for true completion/cancellation
	if (bCompleted || bCanceled)
	{
		FinishedRequest();
		return;
	}

	// keep track of elapsed seconds
	ElapsedTime += DeltaSeconds;
	const float HttpTimeout = FHttpModule::Get().GetHttpTimeout();
	if (HttpTimeout > 0 && ElapsedTime >= HttpTimeout)
	{
		UE_LOG(LogHttp, Warning, TEXT("Timeout processing Http request. %p"),
			this);

		// finish it off since it is timeout
		FinishedRequest();
	}
}

float FHTML5HttpRequest::GetElapsedTime()
{
	return ElapsedTime;
}


/****************************************************************************
* FHTML5HttpResponse implementation
**************************************************************************/

FHTML5HttpResponse::FHTML5HttpResponse(FHTML5HttpRequest& InRequest)
	:	Request(InRequest)
	,	TotalBytesRead(0)
	,	HttpCode(EHttpResponseCodes::Unknown)
	,	ContentLength(0)
	,	bIsReady(0)
	,	bSucceeded(0)
{
}


FHTML5HttpResponse::~FHTML5HttpResponse()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::~FHTML5HttpResponse()"));
}


FString FHTML5HttpResponse::GetURL()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::GetURL()"));
	return Request.GetURL();
}


FString FHTML5HttpResponse::GetURLParameter(const FString& ParameterName)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::GetURLParameter()"));

	return Request.GetURLParameter(ParameterName);
}


FString FHTML5HttpResponse::GetHeader(const FString& HeaderName)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::GetHeader()"));

	FString Result(TEXT(""));

	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached header [%s]. Response still processing. %p"),
			*HeaderName, &Request);
	}
	else
	{
		FString* Header = Headers.Find(HeaderName);

		if (Header != NULL)
		{
			return *Header;
		}
	}

	return Result;
}


TArray<FString> FHTML5HttpResponse::GetAllHeaders()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::GetAllHeaders()"));

	TArray<FString> Result;

	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached headers. Response still processing. %p"),&Request);
	}
	else
	{
		for (TMap<FString, FString>::TConstIterator It(Headers); It; ++It)
		{
			Result.Add(It.Key() + TEXT(": ") + It.Value());
		}
	}

	return Result;
}


FString FHTML5HttpResponse::GetContentType()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::GetContentType()"));

	return GetHeader(TEXT("Content-Type"));
}


int32 FHTML5HttpResponse::GetContentLength()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::GetContentLength()"));

	return ContentLength;
}


const TArray<uint8>& FHTML5HttpResponse::GetContent()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::GetContent()"));

	if (!IsReady())
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"), &Request);
	}

	return Payload;
}


FString FHTML5HttpResponse::GetContentAsString()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::GetContentAsString()"));

	// Fill in our data.
	GetContent();

	TArray<uint8> ZeroTerminatedPayload;
	ZeroTerminatedPayload.AddZeroed(Payload.Num() + 1);
	FMemory::Memcpy(ZeroTerminatedPayload.GetData(), Payload.GetData(), Payload.Num());

	return UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData());
}


int32 FHTML5HttpResponse::GetResponseCode()
{
	UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::GetResponseCode()"));

	return HttpCode;
}

bool FHTML5HttpResponse::IsReady()
{
	if (bIsReady)
	{
		UE_LOG(LogHttp, Verbose, TEXT("FHTML5HttpResponse::IsReady()"));
	}

	return bIsReady;
}
