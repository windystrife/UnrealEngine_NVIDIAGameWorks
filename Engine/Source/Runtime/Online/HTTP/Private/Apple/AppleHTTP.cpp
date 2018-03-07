// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "AppleHTTP.h"
#include "EngineVersion.h"
#include "Security/Security.h"
#include "Misc/App.h"
#include "HAL/PlatformTime.h"
#include "Http.h"
#include "HttpModule.h"

/****************************************************************************
 * FAppleHttpRequest implementation
 ***************************************************************************/


FAppleHttpRequest::FAppleHttpRequest()
:	Connection(nullptr)
,	CompletionStatus(EHttpRequestStatus::NotStarted)
,	ProgressBytesSent(0)
,	StartRequestTime(0.0)
,	ElapsedTime(0.0f)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::FAppleHttpRequest()"));
	Request = [[NSMutableURLRequest alloc] init];
	Request.timeoutInterval = FHttpModule::Get().GetHttpTimeout();

	// Disable cache to mimic WinInet behavior
	Request.cachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
}


FAppleHttpRequest::~FAppleHttpRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::~FAppleHttpRequest()"));
	check(Connection == nullptr);
	[Request release];
}


FString FAppleHttpRequest::GetURL()
{
	SCOPED_AUTORELEASE_POOL;
	NSURL* URL = Request.URL;
	if (URL != nullptr)
	{
		FString ConvertedURL(URL.absoluteString);
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetURL() - %s"), *ConvertedURL);
		return ConvertedURL;
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetURL() - NULL"));
		return FString();
	}
}


void FAppleHttpRequest::SetURL(const FString& URL)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::SetURL() - %s"), *URL);
	Request.URL = [NSURL URLWithString: URL.GetNSString()];
}


FString FAppleHttpRequest::GetURLParameter(const FString& ParameterName)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetURLParameter() - %s"), *ParameterName);

	NSString* ParameterNameStr = ParameterName.GetNSString();
	NSArray* Parameters = [Request.URL.query componentsSeparatedByString:@"&"];
	for (NSString* Parameter in Parameters)
	{
		NSArray* KeyValue = [Parameter componentsSeparatedByString:@"="];
		NSString* Key = KeyValue[0];
		if ([Key compare:ParameterNameStr] == NSOrderedSame)
		{
			return FString(KeyValue[1]);
		}
	}
	return FString();
}


FString FAppleHttpRequest::GetHeader(const FString& HeaderName)
{
	SCOPED_AUTORELEASE_POOL;
	FString Header([Request valueForHTTPHeaderField:HeaderName.GetNSString()]);
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetHeader() - %s"), *Header);
	return Header;
}


void FAppleHttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::SetHeader() - %s / %s"), *HeaderName, *HeaderValue );
	[Request setValue: HeaderValue.GetNSString() forHTTPHeaderField: HeaderName.GetNSString()];
}

void FAppleHttpRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
    if (!HeaderName.IsEmpty() && !AdditionalHeaderValue.IsEmpty())
    {
        NSDictionary* Headers = [Request allHTTPHeaderFields];
        NSString* PreviousHeaderValuePtr = [Headers objectForKey: HeaderName.GetNSString()];
        FString PreviousValue(PreviousHeaderValuePtr);
		FString NewValue;
		if (PreviousValue != nullptr && !PreviousValue.IsEmpty())
		{
			NewValue = PreviousValue + TEXT(", ");
		}
		NewValue += AdditionalHeaderValue;

        SetHeader(HeaderName, NewValue);
	}
}

TArray<FString> FAppleHttpRequest::GetAllHeaders()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetAllHeaders()"));
	NSDictionary* Headers = Request.allHTTPHeaderFields;
	TArray<FString> Result;
	Result.Reserve(Headers.count);
	for (NSString* Key in Headers.allKeys)
	{
		FString ConvertedValue(Headers[Key]);
		FString ConvertedKey(Key);
		UE_LOG(LogHttp, Verbose, TEXT("Header= %s, Key= %s"), *ConvertedValue, *ConvertedKey);

		Result.Add( FString::Printf( TEXT("%s: %s"), *ConvertedKey, *ConvertedValue ) );
	}
	return Result;
}


const TArray<uint8>& FAppleHttpRequest::GetContent()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetContent()"));
	NSData* Body = Request.HTTPBody; // accessing HTTPBody will call copy on the value, increasing its retain count
	RequestPayload.Empty();
	RequestPayload.Append((const uint8*)Body.bytes, Body.length);
	return RequestPayload;
}


void FAppleHttpRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::SetContent()"));
	Request.HTTPBody = [NSData dataWithBytes:ContentPayload.GetData() length:ContentPayload.Num()];
}


FString FAppleHttpRequest::GetContentType()
{
	FString ContentType = GetHeader(TEXT("Content-Type"));
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetContentType() - %s"), *ContentType);
	return ContentType;
}


int32 FAppleHttpRequest::GetContentLength()
{
	SCOPED_AUTORELEASE_POOL;
	NSData* Body = Request.HTTPBody;
	int Len = Body.length;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetContentLength() - %i"), Len);
	return Len;
}


void FAppleHttpRequest::SetContentAsString(const FString& ContentString)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::SetContentAsString() - %s"), *ContentString);
	FTCHARToUTF8 Converter(*ContentString);
	
	// The extra length computation here is unfortunate, but it's technically not safe to assume the length is the same.
	Request.HTTPBody = [NSData dataWithBytes:(ANSICHAR*)Converter.Get() length:Converter.Length()];
}


FString FAppleHttpRequest::GetVerb()
{
	FString ConvertedVerb(Request.HTTPMethod);
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetVerb() - %s"), *ConvertedVerb);
	return ConvertedVerb;
}


void FAppleHttpRequest::SetVerb(const FString& Verb)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::SetVerb() - %s"), *Verb);
	Request.HTTPMethod = Verb.GetNSString();
}


bool FAppleHttpRequest::ProcessRequest()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::ProcessRequest()"));
	bool bStarted = false;

	FString Scheme(Request.URL.scheme);
	Scheme = Scheme.ToLower();

	// Prevent overlapped requests using the same instance
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
	}
	else if(GetURL().Len() == 0)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. No URL was specified."));
	}
	else if( Scheme != TEXT("http") && Scheme != TEXT("https"))
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. URL '%s' is not a valid HTTP request. %p"), *GetURL(), this);
	}
	else
	{
		bStarted = StartRequest();
	}

	if( !bStarted )
	{
		FinishedRequest();
	}

	return bStarted;
}


FHttpRequestCompleteDelegate& FAppleHttpRequest::OnProcessRequestComplete()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::OnProcessRequestComplete()"));
	return RequestCompleteDelegate;
}

FHttpRequestProgressDelegate& FAppleHttpRequest::OnRequestProgress() 
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FAppleHttpRequest::OnRequestProgress()"));
	return RequestProgressDelegate;
}


bool FAppleHttpRequest::StartRequest()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::StartRequest()"));
	bool bStarted = false;

	// set the content-length and user-agent
	if(GetContentLength() > 0)
	{
		[Request setValue:[NSString stringWithFormat:@"%d", GetContentLength()] forHTTPHeaderField:@"Content-Length"];
	}

	const FString UserAgent = GetHeader("User-Agent");
	if(UserAgent.IsEmpty())
	{
		NSString* Tag = FPlatformHttp::GetDefaultUserAgent().GetNSString();
		[Request addValue:Tag forHTTPHeaderField:@"User-Agent"];
	}
	else
	{
		[Request addValue:UserAgent.GetNSString() forHTTPHeaderField:@"User-Agent"];
	}

	Response = MakeShareable( new FAppleHttpResponse( *this ) );

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	// Create the connection, tell it to run in the main run loop, and kick it off.
	Connection = [[NSURLConnection alloc] initWithRequest:Request delegate:Response->ResponseWrapper startImmediately:NO];
#pragma clang diagnostic pop
	if (Connection != nullptr && Response->ResponseWrapper != nullptr)
	{
		CompletionStatus = EHttpRequestStatus::Processing;
		[Connection scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
		[Connection start];
		UE_LOG(LogHttp, Verbose, TEXT("[Connection start]"));

		bStarted = true;
		// Add to global list while being processed so that the ref counted request does not get deleted
		FHttpModule::Get().GetHttpManager().AddRequest(SharedThis(this));
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Could not initialize Internet connection."));
		CompletionStatus = EHttpRequestStatus::Failed_ConnectionError;
	}
	StartRequestTime = FPlatformTime::Seconds();
	// reset the elapsed time.
	ElapsedTime = 0.0f;

	return bStarted;
}

void FAppleHttpRequest::FinishedRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::FinishedRequest()"));
	ElapsedTime = (float)(FPlatformTime::Seconds() - StartRequestTime);
	if( Response.IsValid() && Response->IsReady() && !Response->HadError())
	{
		UE_LOG(LogHttp, Verbose, TEXT("Request succeeded"));
		CompletionStatus = EHttpRequestStatus::Succeeded;

		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), Response, true);
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("Request failed"));
		FString URL([[Request URL] absoluteString]);
		CompletionStatus = EHttpRequestStatus::Failed;

		Response = nullptr;
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), nullptr, false);
	}

	// Clean up session/request handles that may have been created
	CleanupRequest();

	// Remove from global list since processing is now complete
	if (FHttpModule::Get().GetHttpManager().IsValidRequest(this))
	{
		FHttpModule::Get().GetHttpManager().RemoveRequest(SharedThis(this));
	}
}


void FAppleHttpRequest::CleanupRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::CleanupRequest()"));

	if(CompletionStatus == EHttpRequestStatus::Processing)
	{
		CancelRequest();
	}

	if(Connection != nullptr)
	{
		[Connection release];
		Connection = nullptr;
	}
}


void FAppleHttpRequest::CancelRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::CancelRequest()"));
	if(Connection != nullptr)
	{
		[Connection cancel];
	}
	FinishedRequest();
}


EHttpRequestStatus::Type FAppleHttpRequest::GetStatus()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetStatus()"));
	return CompletionStatus;
}


const FHttpResponsePtr FAppleHttpRequest::GetResponse() const
{
	return Response;
}


void FAppleHttpRequest::Tick(float DeltaSeconds)
{
	if( CompletionStatus == EHttpRequestStatus::Processing || Response->HadError() )
	{
		if (OnRequestProgress().IsBound())
		{
			const int32 BytesWritten = Response->GetNumBytesWritten();
			const int32 BytesRead = Response->GetNumBytesReceived();
			if (BytesWritten > 0 || BytesRead > 0)
			{
				OnRequestProgress().Execute(SharedThis(this), BytesWritten, BytesRead);
			}
		}
		if( Response->IsReady() )
		{
			FinishedRequest();
		}
	}
}

float FAppleHttpRequest::GetElapsedTime()
{
	return ElapsedTime;
}


/****************************************************************************
 * FHttpResponseAppleWrapper implementation
 ***************************************************************************/

@implementation FHttpResponseAppleWrapper
@synthesize Response;
@synthesize bIsReady;
@synthesize bHadError;
@synthesize BytesWritten;


-(FHttpResponseAppleWrapper*) init
{
	UE_LOG(LogHttp, Verbose, TEXT("-(FHttpResponseAppleWrapper*) init"));
	self = [super init];
	bIsReady = false;
	
	return self;
}

- (void)dealloc
{
	[Response release];
	[super dealloc];
}

-(void) connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
	UE_LOG(LogHttp, Verbose, TEXT("didSendBodyData:(NSInteger)bytesWritten totalBytes:Written:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite"));
	self.BytesWritten = totalBytesWritten;
	UE_LOG(LogHttp, Verbose, TEXT("didSendBodyData: totalBytesWritten = %d, totalBytesExpectedToWrite = %d: %p"), totalBytesWritten, totalBytesExpectedToWrite, self);
}

-(void) connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveResponse:(NSURLResponse *)response"));
	self.Response = (NSHTTPURLResponse*)response;
	
	// presize the payload container if possible
	Payload.Empty();
	Payload.Reserve([response expectedContentLength] != NSURLResponseUnknownLength ? [response expectedContentLength] : 0);
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveResponse: expectedContentLength = %d. Length = %d: %p"), [response expectedContentLength], Payload.Max(), self);
}


-(void) connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
	Payload.Append((const uint8*)[data bytes], [data length]);
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveData with %d bytes. After Append, Payload Length = %d: %p"), [data length], Payload.Num(), self);
}


-(void) connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	self.bIsReady = YES;
	self.bHadError = YES;
	UE_LOG(LogHttp, Warning, TEXT("didFailWithError. Http request failed - %s %s: %p"), 
		*FString([error localizedDescription]),
		*FString([[error userInfo] objectForKey:NSURLErrorFailingURLStringErrorKey]),
		self);
	// Log more details if verbose logging is enabled and this is an SSL error
	if (UE_LOG_ACTIVE(LogHttp, Verbose))
	{
		SecTrustRef PeerTrustInfo = reinterpret_cast<SecTrustRef>([[error userInfo] objectForKey:NSURLErrorFailingURLPeerTrustErrorKey]);
		if (PeerTrustInfo != nullptr)
		{
			SecTrustResultType TrustResult = kSecTrustResultInvalid;
			SecTrustGetTrustResult(PeerTrustInfo, &TrustResult);

			FString TrustResultString;
			switch (TrustResult)
			{
#define MAP_TO_RESULTSTRING(Constant) case Constant: TrustResultString = TEXT(#Constant); break;
			MAP_TO_RESULTSTRING(kSecTrustResultInvalid)
			MAP_TO_RESULTSTRING(kSecTrustResultProceed)
			MAP_TO_RESULTSTRING(kSecTrustResultDeny)
			MAP_TO_RESULTSTRING(kSecTrustResultUnspecified)
			MAP_TO_RESULTSTRING(kSecTrustResultRecoverableTrustFailure)
			MAP_TO_RESULTSTRING(kSecTrustResultFatalTrustFailure)
			MAP_TO_RESULTSTRING(kSecTrustResultOtherError)
#undef MAP_TO_RESULTSTRING
			default:
				TrustResultString = TEXT("unknown");
				break;
			}
			UE_LOG(LogHttp, Verbose, TEXT("didFailWithError. SSL trust result: %s (%d)"), *TrustResultString, TrustResult);
		}
	}
}


-(void) connectionDidFinishLoading:(NSURLConnection *)connection
{
	UE_LOG(LogHttp, Verbose, TEXT("connectionDidFinishLoading: %p"), self);
	self.bIsReady = YES;
}

- (TArray<uint8>&)getPayload
{
	return Payload;
}

-(int32)getBytesWritten
{
	return self.BytesWritten;
}

@end




/****************************************************************************
 * FAppleHTTPResponse implementation
 **************************************************************************/

FAppleHttpResponse::FAppleHttpResponse(const FAppleHttpRequest& InRequest)
	: Request( InRequest )
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::FAppleHttpResponse()"));
	ResponseWrapper = [[FHttpResponseAppleWrapper alloc] init];
}


FAppleHttpResponse::~FAppleHttpResponse()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::~FAppleHttpResponse()"));
	[ResponseWrapper getPayload].Empty();

	[ResponseWrapper release];
	ResponseWrapper = nil;
}


FString FAppleHttpResponse::GetURL()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetURL()"));
	return FString(Request.Request.URL.query);
}


FString FAppleHttpResponse::GetURLParameter(const FString& ParameterName)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetURLParameter()"));

	NSString* ParameterNameStr = ParameterName.GetNSString();
	NSArray* Parameters = [[[Request.Request URL] query] componentsSeparatedByString:@"&"];
	for (NSString* Parameter in Parameters)
	{
		NSArray* KeyValue = [Parameter componentsSeparatedByString:@"="];
		NSString* Key = [KeyValue objectAtIndex:0];
		if ([Key compare:ParameterNameStr] == NSOrderedSame)
		{
			return FString([[KeyValue objectAtIndex:1] stringByRemovingPercentEncoding]);
		}
	}
	return FString();
}


FString FAppleHttpResponse::GetHeader(const FString& HeaderName)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetHeader()"));

	NSString* ConvertedHeaderName = HeaderName.GetNSString();
	return FString([[[ResponseWrapper Response] allHeaderFields] objectForKey:ConvertedHeaderName]);
}


TArray<FString> FAppleHttpResponse::GetAllHeaders()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetAllHeaders()"));

	NSDictionary* Headers = [GetResponseObj() allHeaderFields];
	TArray<FString> Result;
	Result.Reserve([Headers count]);
	for (NSString* Key in [Headers allKeys])
	{
		FString ConvertedValue([Headers objectForKey:Key]);
		FString ConvertedKey(Key);
		Result.Add( FString::Printf( TEXT("%s: %s"), *ConvertedKey, *ConvertedValue ) );
	}
	return Result;
}


FString FAppleHttpResponse::GetContentType()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetContentType()"));

	return GetHeader( TEXT( "Content-Type" ) );
}


int32 FAppleHttpResponse::GetContentLength()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetContentLength()"));

	return ResponseWrapper.Response.expectedContentLength;
}


const TArray<uint8>& FAppleHttpResponse::GetContent()
{
	if( !IsReady() )
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"), &Request);
	}
	else
	{
		Payload = [ResponseWrapper getPayload];
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetContent() - Num: %i"), [ResponseWrapper getPayload].Num());
	}

	return Payload;
}


FString FAppleHttpResponse::GetContentAsString()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetContentAsString()"));

	// Fill in our data.
	GetContent();

	TArray<uint8> ZeroTerminatedPayload;
	ZeroTerminatedPayload.AddZeroed( Payload.Num() + 1 );
	FMemory::Memcpy( ZeroTerminatedPayload.GetData(), Payload.GetData(), Payload.Num() );

	return UTF8_TO_TCHAR( ZeroTerminatedPayload.GetData() );
}


int32 FAppleHttpResponse::GetResponseCode()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetResponseCode()"));

	return [GetResponseObj() statusCode];
}


NSHTTPURLResponse* FAppleHttpResponse::GetResponseObj()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetResponseObj()"));

	return [ResponseWrapper Response];
}


bool FAppleHttpResponse::IsReady()
{
	bool Ready = [ResponseWrapper bIsReady];

	if( Ready )
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::IsReady()"));
	}

	return Ready;
}

bool FAppleHttpResponse::HadError()
{
	bool bHadError = [ResponseWrapper bHadError];
	
	if( bHadError )
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::HadError()"));
	}
	
	return bHadError;
}

const int32 FAppleHttpResponse::GetNumBytesReceived() const
{
	return [ResponseWrapper getPayload].Num();
}

const int32 FAppleHttpResponse::GetNumBytesWritten() const
{
    int32 NumBytesWritten = [ResponseWrapper getBytesWritten];
    return NumBytesWritten;
}
