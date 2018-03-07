#pragma once 

#include "CoreMinimal.h"

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

// ARKit
#include <ARKit/ARKit.h>

@interface FAppleARKitSessionDelegate : NSObject<ARSessionDelegate>
{
}

- (id)initWithAppleARKitSystem:(class FAppleARKitSystem*)InAppleARKitSystem;

- (void)setMetalTextureCache:(CVMetalTextureCacheRef)InMetalTextureCache;

@end

#endif // ARKIT_SUPPORT
