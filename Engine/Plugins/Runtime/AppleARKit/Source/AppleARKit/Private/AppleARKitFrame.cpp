// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// AppleARKit
#include "AppleARKitFrame.h"
#include "AppleARKitModule.h"
#include "ScopeLock.h"

// Default constructor
FAppleARKitFrame::FAppleARKitFrame()
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
	: CapturedYImage(nullptr)
	, CapturedCbCrImage( nullptr )
#endif
{
};

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

FAppleARKitFrame::FAppleARKitFrame( ARFrame* InARFrame, CVMetalTextureCacheRef MetalTextureCache )
  : Camera( InARFrame.camera )
  , LightEstimate( InARFrame.lightEstimate )
{
	// Sanity check
	check( InARFrame );
	check( MetalTextureCache );

	// Copy timestamp
	Timestamp = InARFrame.timestamp;
	
	// Copy / convert pass-through camera image's CVPixelBuffer to an MTLTexture so we can pass it
	// directly to FTextureResource's.
	// @see AppleARKitCameraTextureResource.cpp
	CapturedYImage = nullptr;
	CapturedYImageWidth = InARFrame.camera.imageResolution.width;
	CapturedYImageHeight = InARFrame.camera.imageResolution.height;
	
	CapturedCbCrImage = nullptr;
	CapturedCbCrImageWidth = InARFrame.camera.imageResolution.width;
	CapturedCbCrImageHeight = InARFrame.camera.imageResolution.height;
	
	if ( InARFrame.capturedImage )
	{
		// Update SizeX & Y
		CapturedYImageWidth = CVPixelBufferGetWidthOfPlane( InARFrame.capturedImage, 0 );
		CapturedYImageHeight = CVPixelBufferGetHeightOfPlane( InARFrame.capturedImage, 0 );
		CapturedCbCrImageWidth = CVPixelBufferGetWidthOfPlane( InARFrame.capturedImage, 1 );
		CapturedCbCrImageHeight = CVPixelBufferGetHeightOfPlane( InARFrame.capturedImage, 1 );
		
		// Create a metal texture from the CVPixelBufferRef. The CVMetalTextureRef will
		// be released in the FAppleARKitFrame destructor.
		// NOTE: On success, CapturedImage will be a new CVMetalTextureRef with a ref count of 1
		// 		 so we don't need to CFRetain it. The corresponding CFRelease is handled in
		//
		CVReturn Result = CVMetalTextureCacheCreateTextureFromImage( nullptr, MetalTextureCache, InARFrame.capturedImage, nullptr, MTLPixelFormatR8Unorm, CapturedYImageWidth, CapturedYImageHeight, /*PlaneIndex*/0, &CapturedYImage );
		check( Result == kCVReturnSuccess );
		check( CapturedYImage );
		check( CFGetRetainCount(CapturedYImage) == 1);
		
		Result = CVMetalTextureCacheCreateTextureFromImage( nullptr, MetalTextureCache, InARFrame.capturedImage, nullptr, MTLPixelFormatRG8Unorm, CapturedCbCrImageWidth, CapturedCbCrImageHeight, /*PlaneIndex*/1, &CapturedCbCrImage );
		check( Result == kCVReturnSuccess );
		check( CapturedCbCrImage );
		check( CFGetRetainCount(CapturedCbCrImage) == 1);
	}
}

FAppleARKitFrame::FAppleARKitFrame( const FAppleARKitFrame& Other )
  : Timestamp( Other.Timestamp )
  , CapturedYImage( nullptr )
  , CapturedCbCrImage( nullptr )
  , CapturedYImageWidth( Other.CapturedYImageWidth )
  , CapturedYImageHeight( Other.CapturedYImageHeight )
  , CapturedCbCrImageWidth( Other.CapturedCbCrImageWidth )
  , CapturedCbCrImageHeight( Other.CapturedCbCrImageHeight )
  , Camera( Other.Camera )
  , LightEstimate( Other.LightEstimate )
{
}

FAppleARKitFrame::~FAppleARKitFrame()
{
	// Release captured image
	if ( CapturedYImage != nullptr )
	{
		CFRelease( CapturedYImage );
	}
	if ( CapturedCbCrImage != nullptr )
	{
		CFRelease( CapturedCbCrImage );
	}
	
}

FAppleARKitFrame& FAppleARKitFrame::operator=( const FAppleARKitFrame& Other )
{
	// Release outgoing image
	if ( CapturedYImage != nullptr )
	{
		CFRelease( CapturedYImage );
	}
	if ( CapturedCbCrImage != nullptr )
	{
		CFRelease( CapturedCbCrImage );
	}
	
	// Member-wise copy
	Timestamp = Other.Timestamp;
	CapturedYImage = nullptr;
	CapturedYImageWidth = Other.CapturedYImageWidth;
	CapturedYImageHeight = Other.CapturedYImageHeight;
	CapturedCbCrImage = nullptr;
	CapturedCbCrImageWidth = Other.CapturedCbCrImageWidth;
	CapturedCbCrImageHeight = Other.CapturedCbCrImageHeight;
	Camera = Other.Camera;
	LightEstimate = Other.LightEstimate;

	return *this;
}

#endif // ARKIT_SUPPORT
