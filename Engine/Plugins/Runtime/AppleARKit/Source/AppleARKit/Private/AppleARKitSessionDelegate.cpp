// AppleARKit
#include "AppleARKitSessionDelegate.h"
#include "AppleARKitSystem.h"
#include "AppleARKitModule.h"
#include "AppleARKitFrame.h"

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

@implementation FAppleARKitSessionDelegate
{
	FAppleARKitSystem* _AppleARKitSystem;
	CVMetalTextureCacheRef _metalTextureCache;
}



- (id)initWithAppleARKitSystem:(FAppleARKitSystem*)InAppleARKitSystem
{
	self = [super init];
	if (self)
	{
		UE_LOG(LogAppleARKit, Log, TEXT("Delegate created with session: %p"), InAppleARKitSystem);
		_AppleARKitSystem = InAppleARKitSystem;
		_metalTextureCache = NULL;	
	}
	return self;
}

- (void)setMetalTextureCache:(CVMetalTextureCacheRef)InMetalTextureCache
{
	// Release current?
	if ( _metalTextureCache != nullptr )
	{
		CFRelease( _metalTextureCache );
	}
	// Set new & retain
	_metalTextureCache = InMetalTextureCache;
	if ( _metalTextureCache != nullptr )
	{
		CFRetain( _metalTextureCache );
	}
}
#pragma mark - ARSessionDelegate Methods

- (void)session:(ARSession *)session didUpdateFrame:(ARFrame *)frame 
{
	// check if we get an update when the session is not initialized.
	if (!_metalTextureCache)
	{
		UE_LOG(LogAppleARKit, Log, TEXT("Delegate didUpdateFrame with no valid _metalTextureCache (ignoring)"));
		return;
	}

	// Bundle results into FAppleARKitFrame
	TSharedPtr< FAppleARKitFrame, ESPMode::ThreadSafe > AppleARKitFrame( new FAppleARKitFrame( frame, _metalTextureCache ) );
	
	// Pass result to session
	_AppleARKitSystem->SessionDidUpdateFrame_DelegateThread( AppleARKitFrame );
}

- (void)session:(ARSession *)session didFailWithError:(NSError *)error
{
	// Log error
	NSString *ErrorDescription = [error localizedDescription];
	UE_LOG( LogAppleARKit, Warning, TEXT("Session failed with error: %s"), *FString(ErrorDescription) );
}

- (void)session:(ARSession *)session didAddAnchors:(NSArray<ARAnchor*>*)anchors 
{
	// Pass updates to sessiom
	_AppleARKitSystem->SessionDidAddAnchors_DelegateThread( anchors );
}

- (void)session:(ARSession *)session didUpdateAnchors:(NSArray<ARAnchor*>*)anchors 
{    
	// Pass updates to sessiom
	_AppleARKitSystem->SessionDidUpdateAnchors_DelegateThread( anchors );
}

- (void)session:(ARSession *)session didRemoveAnchors:(NSArray<ARAnchor*>*)anchors 
{    
	// Pass updates to sessiom
	_AppleARKitSystem->SessionDidRemoveAnchors_DelegateThread( anchors );
}

@end
#endif // ARKIT_SUPPORT

