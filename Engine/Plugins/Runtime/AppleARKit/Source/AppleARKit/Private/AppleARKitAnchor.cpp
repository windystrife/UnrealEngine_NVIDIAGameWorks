// AppleARKit
#include "AppleARKitAnchor.h"
#include "AppleARKitModule.h"
#include "AppleARKitTransform.h"

// UE4
#include "Misc/ScopeLock.h"

FTransform UAppleARKitAnchor::GetTransform() const
{
	FScopeLock ScopeLock( &UpdateLock );

	return Transform;
}

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

void UAppleARKitAnchor::Update_DelegateThread( ARAnchor* Anchor )
{
	FScopeLock ScopeLock( &UpdateLock );

	// @todo arkit use World Settings WorldToMetersScale
	Transform = FAppleARKitTransform::ToFTransform( Anchor.transform );
}

#endif // ARKIT_SUPPORT
