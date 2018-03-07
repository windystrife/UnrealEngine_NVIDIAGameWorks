// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WebImage.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "HttpModule.h"
#include "Modules/ModuleManager.h"
#include "Styling/CoreStyle.h"

#include "ImageDownloadPrivate.h"


FWebImage::FWebImage()
: StandInBrush(FCoreStyle::Get().GetDefaultBrush())
, bDownloadSuccess(false)
{
}

FWebImage::~FWebImage()
{
	CancelDownload();
}

TAttribute< const FSlateBrush* > FWebImage::Attr() const
{
	return TAttribute< const FSlateBrush* >(AsShared(), &FWebImage::GetBrush);
}

bool FWebImage::BeginDownload(const FString& UrlIn, const TOptional<FString>& StandInETag, const FOnImageDownloaded& DownloadCb)
{
	CancelDownload();

	// store the url
	Url = UrlIn;

	// make a new request
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetURL(Url);
	HttpRequest->SetHeader(TEXT("Accept"), TEXT("image/png, image/x-png, image/jpeg; q=0.8, image/vnd.microsoft.icon, image/x-icon, image/bmp, image/*; q=0.5, image/webp; q=0.0"));
	HttpRequest->OnProcessRequestComplete().BindSP(this, &FWebImage::HttpRequestComplete);

	if (StandInETag.IsSet())
	{
		HttpRequest->SetHeader(TEXT("If-None-Match"), *StandInETag.GetValue());
	}

	// queue the request
	if (!HttpRequest->ProcessRequest())
	{
		return false;
	}
	else
	{
		PendingRequest = HttpRequest;
		PendingCallback = DownloadCb;
		return true;
	}
}

void FWebImage::HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	// clear our handle to the request
	PendingRequest.Reset();

	// get the request URL
	check(HttpRequest.IsValid()); // this should be valid, we did just send a request...
	if (HttpRequest->OnProcessRequestComplete().IsBound())
	{
		HttpRequest->OnProcessRequestComplete().Unbind();
	}
	bool bSuccess = ProcessHttpResponse(HttpRequest->GetURL(), bSucceeded ? HttpResponse : FHttpResponsePtr());

	// save this info
	bDownloadSuccess = bSuccess;
	DownloadTimeUtc = FDateTime::UtcNow();

	// fire the response delegate
	if (PendingCallback.IsBound())
	{
		PendingCallback.Execute(bSuccess);
		PendingCallback.Unbind();
	}
}

bool FWebImage::ProcessHttpResponse(const FString& RequestUrl, FHttpResponsePtr HttpResponse)
{
	// check for successful response
	if (!HttpResponse.IsValid())
	{
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: Connection Failed. url=%s"), *RequestUrl);
		return false;
	}

	ETag = HttpResponse->GetHeader("ETag");

	// check status code
	int32 StatusCode = HttpResponse->GetResponseCode();
	if (StatusCode / 100 != 2)
	{
		if ( StatusCode == 304)
		{
			// Not modified means that the image is identical to the placeholder image.
			return true;
		}
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: HTTP response %d. url=%s"), StatusCode, *RequestUrl);
		return false;
	}

	// build an image wrapper for this type
	static const FName MODULE_IMAGE_WRAPPER("ImageWrapper");
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(MODULE_IMAGE_WRAPPER);

	// Look at the signature of the downloaded image to detect image type. (and ignore the content type header except for error reporting)
	const TArray<uint8>& Content = HttpResponse->GetContent();
	EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(Content.GetData(), Content.Num());

	if (ImageFormat == EImageFormat::Invalid)
	{
		FString ContentType = HttpResponse->GetContentType();
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: Could not recognize file type of image downloaded from url %s, server-reported content type: %s"), *RequestUrl, *ContentType);
		return false;
	}

	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: Unable to make image wrapper for image format %d"), (int32)ImageFormat);
		return false;
	}

	// parse the content
	if (!ImageWrapper->SetCompressed(Content.GetData(), Content.Num()))
	{
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: Unable to parse image format %d from %s"), (int32)ImageFormat, *RequestUrl);
		return false;
	}

	// get the raw image data
	const TArray<uint8>* RawImageData = nullptr;
	if (!ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, RawImageData) || RawImageData == nullptr)
	{
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: Unable to convert image format %d to BGRA 8"), (int32)ImageFormat);
		return false;
	}

	// make a dynamic image
	FName ResourceName(*RequestUrl);
	DownloadedBrush = FSlateDynamicImageBrush::CreateWithImageData(ResourceName, FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight()), *RawImageData);
	return DownloadedBrush.IsValid();
}

void FWebImage::CancelDownload()
{
	if (PendingRequest.IsValid())
	{
		if (PendingRequest->OnProcessRequestComplete().IsBound())
		{
			PendingRequest->OnProcessRequestComplete().Unbind();
		}
		PendingRequest->CancelRequest();
		PendingRequest.Reset();
	}
	if (PendingCallback.IsBound())
	{
		PendingCallback.Unbind();
	}
	bDownloadSuccess = false;
}
