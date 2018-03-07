// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSView.h"
#include "IOSAppDelegate.h"
#include "IOSApplication.h"
#include "IOS/IOSInputInterface.h"
#include "Delegates/Delegate.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "IOSPlatformProcess.h"

#include <OpenGLES/ES2/gl.h>
#import "IOSAsyncTask.h"
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGLDrawable.h>
#import <UIKit/UIGeometry.h>

#if HAS_METAL
id<MTLDevice> GMetalDevice = nil;
#endif


@interface IndexedPosition : UITextPosition {
	NSUInteger _index;
	id <UITextInputDelegate> _inputDelegate;
}
@property (nonatomic) NSUInteger index;
+ (IndexedPosition *)positionWithIndex:(NSUInteger)index;
@end

@interface IndexedRange : UITextRange {
	NSRange _range;
}
@property (nonatomic) NSRange range;
+ (IndexedRange *)rangeWithNSRange:(NSRange)range;

@end


@implementation IndexedPosition
@synthesize index = _index;

+ (IndexedPosition *)positionWithIndex:(NSUInteger)index {
	IndexedPosition *pos = [[IndexedPosition alloc] init];
	pos.index = index;
	return pos;
}

@end

@implementation IndexedRange
@synthesize range = _range;

+ (IndexedRange *)rangeWithNSRange:(NSRange)nsrange {
	if (nsrange.location == NSNotFound)
		return nil;
	IndexedRange *range = [[IndexedRange alloc] init];
	range.range = nsrange;
	return range;
}

- (UITextPosition *)start {
	return [IndexedPosition positionWithIndex:self.range.location];
}

- (UITextPosition *)end {
	return [IndexedPosition positionWithIndex:(self.range.location + self.range.length)];
}

-(BOOL)isEmpty {
	return (self.range.length == 0);
}
@end



@implementation FIOSView

@synthesize keyboardType = KeyboardType;
@synthesize autocorrectionType = AutocorrectionType;
@synthesize autocapitalizationType = AutocapitalizationType;
@synthesize secureTextEntry = bSecureTextEntry;
@synthesize SwapCount, OnScreenColorRenderBuffer, OnScreenColorRenderBufferMSAA, markedTextStyle;

/**
 * @return The Layer Class for the window
 */
+ (Class)layerClass
{
#if HAS_METAL
	// make sure the project setting has enabled Metal support (per-project user settings in the editor)
	bool bSupportsMetal = false;
	bool bSupportsMetalMRT = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsMetal, GEngineIni);
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsMetalMRT, GEngineIni);

	// does commandline override?
	bool bForceES2 = FParse::Param(FCommandLine::Get(), TEXT("ES2"));

	bool bTriedToInit = false;

	// the check for the function pointer itself is to determine if the Metal framework exists, before calling it
	if ((bSupportsMetal || bSupportsMetalMRT) && !bForceES2 && MTLCreateSystemDefaultDevice != NULL)
	{
		// if the device is unable to run with Metal (pre-A7), this will return nil
		GMetalDevice = MTLCreateSystemDefaultDevice();

		// just tracking for printout below
		bTriedToInit = true;
	}

#if !UE_BUILD_SHIPPING
	if (GMetalDevice == nil)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Not using Metal because: [Project Settings Disabled Metal? %s :: Commandline Forced ES2? %s :: Older OS? %s :: Pre-A7 Device? %s]"),
			bSupportsMetal ? TEXT("No") : TEXT("Yes"),
			bForceES2? TEXT("Yes") : TEXT("No"),
			(MTLCreateSystemDefaultDevice == NULL) ? TEXT("Yes") : TEXT("No"),
			bTriedToInit ? TEXT("Yes") : TEXT("Unknown (didn't test)"));
	}
#endif

	if (GMetalDevice != nil)
	{
		return [CAMetalLayer class];
	}
	else
#endif
	{
		return [CAEAGLLayer class];
	}
}


- (id)initWithFrame:(CGRect)Frame
{
	if ((self = [super initWithFrame:Frame]))
	{
		// figure out if we should start up GL or Metal
#if HAS_METAL
		// if the device is valid, we know Metal is usable (see +layerClass)
		MetalDevice = GMetalDevice;
		if (MetalDevice != nil)
		{
			bIsUsingMetal = true;

			// grab the MetalLayer and typecast it to match what's in layerClass
			CAMetalLayer* MetalLayer = (CAMetalLayer*)self.layer;
			MetalLayer.presentsWithTransaction = NO;
			MetalLayer.drawsAsynchronously = YES;

			// set a background color to make sure the layer appears
			CGFloat components[] = { 0.0, 0.0, 0.0, 1 };
			MetalLayer.backgroundColor = CGColorCreate(CGColorSpaceCreateDeviceRGB(), components);

			// set the device on the rendering layer and provide a pixel format
			MetalLayer.device = MetalDevice;
			MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
			MetalLayer.framebufferOnly = NO;
		
		}
		else
#endif
		{
			bIsUsingMetal = false;

			// Get the layer
			CAEAGLLayer *EaglLayer = (CAEAGLLayer *)self.layer;
			EaglLayer.opaque = YES;
			NSMutableDictionary* Dict = [NSMutableDictionary dictionary];
			[Dict setValue : [NSNumber numberWithBool : NO] forKey : kEAGLDrawablePropertyRetainedBacking];
			[Dict setValue : kEAGLColorFormatRGBA8 forKey : kEAGLDrawablePropertyColorFormat];
			EaglLayer.drawableProperties = Dict;

			// Initialize a single, static OpenGL ES 2.0 context, shared by all EAGLView objects
			Context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

			// delete this on failure
			if (!Context || ![EAGLContext setCurrentContext : Context])
			{
				[self release];
				return nil;
			}
		}

		NSLog(@"::: Created a UIView that will support %@ :::", bIsUsingMetal ? @"Metal" : @"@GLES");

		// Initialize some variables
		SwapCount = 0;

		FMemory::Memzero(AllTouches, sizeof(AllTouches));
		[self setAutoresizingMask: UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
		bIsInitialized = false;
		

		[self InitKeyboard];
	}
	return self;
}

-(void)dealloc
{
	[markedTextStyle release];
	[super dealloc];
}

- (bool)CreateFramebuffer:(bool)bIsForOnDevice
{
	if (!bIsInitialized)
	{
		// look up what the device can support
		const float NativeScale = [[UIScreen mainScreen] scale];

		// look up the CVar for the scale factor
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
		float RequestedContentScaleFactor = CVar->GetFloat();

		// 0 means to leave the scale alone, use native
		if (RequestedContentScaleFactor == 0.0f)
		{
#ifdef __IPHONE_8_0
            if ([self.window.screen respondsToSelector:@selector(nativeScale)])
            {
                self.contentScaleFactor = self.window.screen.nativeScale;
                UE_LOG(LogIOS, Log, TEXT("Setting contentScaleFactor to nativeScale which is = %f"), self.contentScaleFactor);
            }
            else
#endif
            {
                UE_LOG(LogIOS, Log, TEXT("Leaving contentScaleFactor alone, with scale = %f"), NativeScale);
            }
		}
		else
		{
			// for TV screens, always use scale factor of 1
			self.contentScaleFactor = bIsForOnDevice ? RequestedContentScaleFactor : 1.0f;
			UE_LOG(LogIOS, Log, TEXT("Setting contentScaleFactor to %0.4f (optimal = %0.4f)"), self.contentScaleFactor, NativeScale);
		}


		// handle Metal or GL sizing
#if HAS_METAL
		if (bIsUsingMetal)
		{
			CAMetalLayer* MetalLayer = (CAMetalLayer*)self.layer;
			CGSize DrawableSize = self.bounds.size;
			DrawableSize.width *= self.contentScaleFactor;
			DrawableSize.height *= self.contentScaleFactor;
			MetalLayer.drawableSize = DrawableSize;
		}
		else
#endif
		{
			// make sure this is current
			[self MakeCurrent];

			// This is causing a pretty large slow down on iPad 3 and 4 using iOS 6, for now going to comment it out
	//		if (self.contentScaleFactor == 1.0f || self.contentScaleFactor == 2.0f)
	//		{
	//			UE_LOG(LogIOS,Log,TEXT("Setting layer filter to NEAREST"));
	//			CAEAGLLayer *EaglLayer = (CAEAGLLayer *)self.layer;
	//			EaglLayer.magnificationFilter = kCAFilterNearest;
	//		}

			// Create our standard displayable surface
			glGenRenderbuffers(1, &OnScreenColorRenderBuffer);
			check(glGetError() == 0);
			glBindRenderbuffer(GL_RENDERBUFFER, OnScreenColorRenderBuffer);
			check(glGetError() == 0);
			[Context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];

			// Get the size of the surface
			GLint OnScreenWidth, OnScreenHeight;
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &OnScreenWidth);
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &OnScreenHeight);

			// NOTE: This resolve FBO is necessary even if we don't plan on using MSAA because otherwise
			// the shaders will not warm properly. Future investigation as to why; it seems unnecessary.

			// Create an FBO used to target the resolve surface
			glGenFramebuffers(1, &ResolveFrameBuffer);
			check(glGetError() == 0);
			glBindFramebuffer(GL_FRAMEBUFFER, ResolveFrameBuffer);
			check(glGetError() == 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, OnScreenColorRenderBuffer);
			check(glGetError() == 0);
			check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

#if defined(USE_DETAILED_IPHONE_MEM_TRACKING) && USE_DETAILED_IPHONE_MEM_TRACKING
			//This value is used to allow the engine to track gl allocated memory see GetIPhoneOpenGLBackBufferSize
			uint32 SingleBufferSize = OnScreenWidth * OnScreenHeight * 4/*rgba8*/;
			IPhoneBackBufferMemSize = singleBufferSize * 3/*iphone back buffer system is tripple buffered*/;

			UE_LOG(LogIOS, Log, TEXT("IPhone Back Buffer Size: %i MB"), (IPhoneBackBufferMemSize / 1024) / 1024.f);
#endif
		}

		bIsInitialized = true;
	}    
	return true;
}

/**
 * If view is resized, update the frame buffer so it is the same size as the display area.
 */
- (void)layoutSubviews
{
#if !PLATFORM_TVOS
	UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];
	FIOSApplication::OrientationChanged(orientation);
#endif
}

-(void)UpdateRenderWidth:(uint32)Width andHeight:(uint32)Height
{
#if HAS_METAL
	if (bIsUsingMetal)
	{
		if (MetalDevice != nil)
		{
			// grab the MetalLayer and typecast it to match what's in layerClass
			CAMetalLayer* MetalLayer = (CAMetalLayer*)self.layer;
			CGSize drawableSize = CGSizeMake(Width, Height);
			checkf( FMath::TruncToInt(drawableSize.width) == FMath::TruncToInt(self.bounds.size.width * self.contentScaleFactor) && 
				   FMath::TruncToInt(drawableSize.height) == FMath::TruncToInt(self.bounds.size.height * self.contentScaleFactor),
				TEXT("[IOSView UpdateRenderWidth:andHeight:] passed in size doesn't match what we expected. Width: %d, Expected Width = %d (%.2f * %.2f). Height = %d, Expected Height = %d (%.2f * %.2f)"),
				FMath::TruncToInt(drawableSize.width), FMath::TruncToInt(self.bounds.size.width * self.contentScaleFactor), (float)self.bounds.size.width, self.contentScaleFactor,
				FMath::TruncToInt(drawableSize.height), FMath::TruncToInt(self.bounds.size.height * self.contentScaleFactor), (float)self.bounds.size.height, self.contentScaleFactor);
			MetalLayer.drawableSize = drawableSize;
		}
		return;
	}
#endif

	// Allocate color buffer based on the current layer size
	glBindRenderbuffer(GL_RENDERBUFFER, OnScreenColorRenderBuffer);
	check(glGetError() == 0);
	[Context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];
	glBindFramebuffer(GL_FRAMEBUFFER, ResolveFrameBuffer);
	check(glGetError() == 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, OnScreenColorRenderBuffer);
	check(glGetError() == 0);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

#if HAS_METAL
- (id<CAMetalDrawable>)MakeDrawable
{
    return ![IOSAppDelegate GetDelegate].bIsSuspended ? [(CAMetalLayer*)self.layer nextDrawable] : nil;
}
#endif

- (void)DestroyFramebuffer
{
	if (bIsInitialized)
	{
		// nothing to do here for Metal
		if (!bIsUsingMetal)
		{
			// toss framebuffers
			if(ResolveFrameBuffer)
			{
				glDeleteFramebuffers(1, &ResolveFrameBuffer);
				ResolveFrameBuffer = 0;
			}
			if(OnScreenColorRenderBuffer)
			{
				glDeleteRenderbuffers(1, &OnScreenColorRenderBuffer);
				OnScreenColorRenderBuffer = 0;
			}
// 			if( GMSAAAllowed )
// 			{
// 				if(OnScreenColorRenderBufferMSAA)
// 				{
// 					glDeleteRenderbuffers(1, &OnScreenColorRenderBufferMSAA);
// 					OnScreenColorRenderBufferMSAA = 0;
// 				}
//			}
 		}

		// we are ready to be re-initialized
		bIsInitialized = false;
	}
}

- (void)MakeCurrent
{
	if (!bIsUsingMetal)
	{
		[EAGLContext setCurrentContext:Context];
	}
}

- (void)UnmakeCurrent
{
	if (!bIsUsingMetal)
	{
		[EAGLContext setCurrentContext:nil];
	}
}

- (void)SwapBuffers
{
	if (!bIsUsingMetal)
	{
// 		// We may need this in the MSAA case
// 		GLint CurrentFramebuffer = 0;
// 		// @todo-mobile: Fix this when we have MSAA support
// 		if( GMSAAAllowed && GMSAAEnabled )
// 		{
// 			// Get the currently bound FBO
// 			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &CurrentFramebuffer);
// 
// 			// Set up and perform the resolve (the READ is already set)
// 			glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE, ResolveFrameBuffer);
// 			glResolveMultisampleFramebufferAPPLE();
// 
// 			// After the resolve, we can discard the old attachments
// 			GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
// 			glDiscardFramebufferEXT( GL_READ_FRAMEBUFFER_APPLE, 3, attachments );
// 		}
// 		else
// 		{
// 			// Discard the now-unncessary depth buffer
// 			GLenum attachments[] = { GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
// 			glDiscardFramebufferEXT( GL_READ_FRAMEBUFFER_APPLE, 2, attachments );
// 		}
// 
		// Perform the actual present with the on-screen renderbuffer
		//glBindRenderbuffer(GL_RENDERBUFFER, OnScreenColorRenderBuffer);
		[Context presentRenderbuffer:GL_RENDERBUFFER];

// 		if( GMSAAAllowed && GMSAAEnabled )
// 		{
// 			// Restore the DRAW framebuffer object
// 			glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE, CurrentFramebuffer);
// 		}
	}

	// increment our swap counter
	SwapCount++;
}

/**
 * Returns the unique ID for the given touch
 */
-(int32) GetTouchIndex:(UITouch*)Touch
{
	// look for existing touch
	for (int Index = 0; Index < ARRAY_COUNT(AllTouches); Index++)
	{
		if (AllTouches[Index] == Touch)
		{
			return Index;
		}
	}
	
	// if we get here, it's a new touch, find a slot
	for (int Index = 0; Index < ARRAY_COUNT(AllTouches); Index++)
	{
		if (AllTouches[Index] == nil)
		{
			AllTouches[Index] = Touch;
			return Index;
		}
	}
	
	// if we get here, that means we are trying to use more than 5 touches at once, which is an error
	return -1;
}

/**
 * Pass touch events to the input queue for slate to pull off of, and trigger the debug console.
 *
 * @param View The view the event happened in
 * @param Touches Set of touch events from the OS
 */
-(void) HandleTouches:(NSSet*)Touches ofType:(TouchType)Type
{
	CGFloat Scale = self.contentScaleFactor;

	TArray<TouchInput> TouchesArray;
	for (UITouch* Touch in Touches)
	{
		// get info from the touch
		CGPoint Loc = [Touch locationInView:self];
		CGPoint PrevLoc = [Touch previousLocationInView:self];

		// convert TOuch pointer to a unique 0 based index
		int32 TouchIndex = [self GetTouchIndex:Touch];
		if (TouchIndex < 0)
		{
			continue;
		}

		// make a new touch event struct
		TouchInput TouchMessage;
		TouchMessage.Handle = TouchIndex;
		TouchMessage.Type = Type;
		TouchMessage.Position = FVector2D(FMath::Min<float>(self.frame.size.width - 1, Loc.x), FMath::Min<float>(self.frame.size.height - 1, Loc.y)) * Scale;
		TouchMessage.LastPosition = FVector2D(FMath::Min<float>(self.frame.size.width - 1, PrevLoc.x), FMath::Min<float>(self.frame.size.height - 1, PrevLoc.y)) * Scale;
		TouchesArray.Add(TouchMessage);

		if (Type == TouchBegan)
		{
			TouchInput EmulatedMessage;
			EmulatedMessage.Handle = TouchMessage.Handle;
			EmulatedMessage.Type = TouchMoved;
			EmulatedMessage.Position = TouchMessage.Position;
			EmulatedMessage.LastPosition = TouchMessage.Position;
			TouchesArray.Add(EmulatedMessage);
		}
		
		// clear out the touch when it ends
		if (Type == TouchEnded)
		{
			AllTouches[TouchIndex] = nil;
		}
	}

	FIOSInputInterface::QueueTouchInput(TouchesArray);
}

/**
 * Handle the various touch types from the OS
 *
 * @param touches Array of touch events
 * @param event Event information
 */
- (void) touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event 
{
	NumActiveTouches += touches.count;
	[self HandleTouches:touches ofType:TouchBegan];

#if !UE_BUILD_SHIPPING
#if WITH_SIMULATOR
	// use 2 on the simulator so that Option-Click will bring up console (option-click is for doing pinch gestures, which we don't care about, atm)
	if( NumActiveTouches >= 2 )
#else
	// If there are 3 active touches, bring up the console
	if( NumActiveTouches >= 4 )
#endif
	{
		bool bShowConsole = true;
		GConfig->GetBool(TEXT("/Script/Engine.InputSettings"), TEXT("bShowConsoleOnFourFingerTap"), bShowConsole, GInputIni);

		if (bShowConsole)
		{
			// disable the integrated keyboard when launching the console
/*			if (bIsUsingIntegratedKeyboard)
			{
				// press the console key twice to get the big one up
				// @todo keyboard: Find a direct way to bering this up (it can get into a bad state where two presses won't do it correctly)
				// and also the ` key could be changed via .ini
				FIOSInputInterface::QueueKeyInput('`', '`');
				FIOSInputInterface::QueueKeyInput('`', '`');
				
				[self ActivateKeyboard:true];
			}
			else*/
			{
				// Route the command to the main iOS thread (all UI must go to the main thread)
				[[IOSAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowConsole) withObject:nil waitUntilDone:NO];
			}
		}
	}
#endif
}

- (void) touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event
{
	[self HandleTouches:touches ofType:TouchMoved];
}

- (void) touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event
{
	NumActiveTouches -= touches.count;
	[self HandleTouches:touches ofType:TouchEnded];
}

- (void) touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event
{
	NumActiveTouches -= touches.count;
	[self HandleTouches:touches ofType:TouchEnded];
}












#pragma mark Keyboard


-(BOOL)canBecomeFirstResponder
{
	return YES;
}

- (BOOL)hasText
{
	return YES;
}

- (void)insertText:(NSString *)theText
{
	// insert text one key at a time, as chars, not keydowns
	for (int32 CharIndex = 0; CharIndex < [theText length]; CharIndex++)
	{
		int32 Char = [theText characterAtIndex:CharIndex];

		// FPlatformMisc::LowLevelOutputDebugStringf(TEXT("sending key '%c' to game\n"), Char);

		if (Char == '\n')
		{
			// send the enter keypress
			FIOSInputInterface::QueueKeyInput(KEYCODE_ENTER, Char);
			
			// hide the keyboard
			[self resignFirstResponder];
		}
		else
		{
			FIOSInputInterface::QueueKeyInput(Char, Char);
		}
	}
}

- (void)deleteBackward
{
	FIOSInputInterface::QueueKeyInput(KEYCODE_BACKSPACE, '\b');
}

-(void)ActivateKeyboard:(bool)bInSendEscapeOnClose
{
	FKeyboardConfig DefaultConfig;
	[self ActivateKeyboard:bInSendEscapeOnClose keyboardConfig:DefaultConfig];
}

-(void)ActivateKeyboard:(bool)bInSendEscapeOnClose keyboardConfig:(FKeyboardConfig)KeyboardConfig
{
	FPlatformAtomics::InterlockedIncrement(&KeyboardShowCount);
	
	dispatch_async(dispatch_get_main_queue(),^ {
		volatile int32 ShowCount = KeyboardShowCount;
		if (ShowCount == 1)
		{
			self.keyboardType = KeyboardConfig.KeyboardType;
			self.autocorrectionType = KeyboardConfig.AutocorrectionType;
			self.autocapitalizationType = KeyboardConfig.AutocapitalizationType;
			self.secureTextEntry = KeyboardConfig.bSecureTextEntry;
		
			// Remember the setting
			bSendEscapeOnClose = bInSendEscapeOnClose;
		
			// Dismiss the existing keyboard, if one exists, so the style can be overridden.
			[self endEditing:YES];
			[self becomeFirstResponder];
		}
		
		FPlatformAtomics::InterlockedDecrement(&KeyboardShowCount);
	});
}

-(void)DeactivateKeyboard
{
	dispatch_async(dispatch_get_main_queue(),^ {
		volatile int32 ShowCount = KeyboardShowCount;
		if (ShowCount == 0)
		{
			// Wait briefly, in case a keyboard activation is triggered.
			FPlatformProcess::Sleep(0.1F);
			
			ShowCount = KeyboardShowCount;
			if (ShowCount == 0)
			{
				// Dismiss the existing keyboard, if one exists.
				[self endEditing:YES];
			}
		}
	});
}

-(BOOL)becomeFirstResponder
{
	volatile int32 ShowCount = KeyboardShowCount;
	if (ShowCount >= 1)
	{
		return [super becomeFirstResponder];
	}
	else
	{
		return NO;
	}
}

-(BOOL)resignFirstResponder
{
	if (bSendEscapeOnClose)
	{
		// tell the console to close itself
		FIOSInputInterface::QueueKeyInput(KEYCODE_ESCAPE, 0);
	}
	
	return [super resignFirstResponder];
}


// @todo keyboard: This is a helper define to show functions that _may_ need to be implemented as we go forward with keyboard support
// for now, the very basics work, but most likely at some point for optimal functionality, we'll want to know the actual string
// in the box, but that needs to come from Slate, and we currently don't have a way to get it
#define REPORT_EVENT UE_LOG(LogIOS, Display, TEXT("Got a keyboard call, line %d"), __LINE__);


- (NSString *)textInRange:(UITextRange *)range
{
	// @todo keyboard: This is called
	return @"";
	//IndexedRange *r = (IndexedRange *)range;
	//return ([textStore substringWithRange:r.range]);
}

- (void)replaceRange:(UITextRange *)range withText:(NSString *)text
{
	REPORT_EVENT;
	return;
//	IndexedRange *r = (IndexedRange *)range;
//	[textStore replaceCharactersInRange:r.range withString:text];
}

- (UITextRange *)selectedTextRange
{
	// @todo keyboard: This is called
	return [IndexedRange rangeWithNSRange:NSMakeRange(0,0)];//self.textView.selectedTextRange];
}


- (void)setSelectedTextRange:(UITextRange *)range
{
	REPORT_EVENT;
	//IndexedRange *indexedRange = (IndexedRange *)range;
	//self.textView.selectedTextRange = indexedRange.range;
}


- (UITextRange *)markedTextRange
{
	// @todo keyboard: This is called
	return nil;
}


- (void)setMarkedText:(NSString *)markedText selectedRange:(NSRange)selectedRange
{
	CachedMarkedText = markedText;
	//NSLog(@"setting marked text to %@", markedText);
}


- (void)unmarkText
{
	if (CachedMarkedText != nil)
	{
		[self insertText:CachedMarkedText];
		CachedMarkedText = nil;
	}
}


- (UITextPosition *)beginningOfDocument
{
	// @todo keyboard: This is called
	return [IndexedPosition positionWithIndex:0];
}


- (UITextPosition *)endOfDocument
{
	REPORT_EVENT;
	return [IndexedPosition positionWithIndex:0];
}


- (UITextRange *)textRangeFromPosition:(UITextPosition *)fromPosition toPosition:(UITextPosition *)toPosition
{
	// @todo keyboard: This is called
	// Generate IndexedPosition instances that wrap the to and from ranges.
	IndexedPosition *fromIndexedPosition = (IndexedPosition *)fromPosition;
	IndexedPosition *toIndexedPosition = (IndexedPosition *)toPosition;
	NSRange range = NSMakeRange(MIN(fromIndexedPosition.index, toIndexedPosition.index), ABS(toIndexedPosition.index - fromIndexedPosition.index));
 
	return [IndexedRange rangeWithNSRange:range];
}


- (UITextPosition *)positionFromPosition:(UITextPosition *)position offset:(NSInteger)offset
{
	// @todo keyboard: This is called
	return nil;
}


- (UITextPosition *)positionFromPosition:(UITextPosition *)position inDirection:(UITextLayoutDirection)direction offset:(NSInteger)offset
{
	REPORT_EVENT;
	return nil;
}


- (NSComparisonResult)comparePosition:(UITextPosition *)position toPosition:(UITextPosition *)other
{
	// @todo keyboard: This is called
	return NSOrderedSame;
}


- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)toPosition
{
	REPORT_EVENT;
	IndexedPosition *fromIndexedPosition = (IndexedPosition *)from;
	IndexedPosition *toIndexedPosition = (IndexedPosition *)toPosition;
	return (toIndexedPosition.index - fromIndexedPosition.index);
}



- (UITextPosition *)positionWithinRange:(UITextRange *)range farthestInDirection:(UITextLayoutDirection)direction
{
	REPORT_EVENT;
	return nil;
}


- (UITextRange *)characterRangeByExtendingPosition:(UITextPosition *)position inDirection:(UITextLayoutDirection)direction
{
	REPORT_EVENT;
	return nil;
}


- (UITextWritingDirection)baseWritingDirectionForPosition:(UITextPosition *)position inDirection:(UITextStorageDirection)direction
{
	REPORT_EVENT;
	// assume left to right for now
	return UITextWritingDirectionLeftToRight;
}


- (void)setBaseWritingDirection:(UITextWritingDirection)writingDirection forRange:(UITextRange *)range
{
	// @todo keyboard: This is called
}



- (CGRect)firstRectForRange:(UITextRange *)range
{
	REPORT_EVENT;
	return CGRectMake(0,0,0,0);
}


- (CGRect)caretRectForPosition:(UITextPosition *)position
{
	// @todo keyboard: This is called
	return CGRectMake(0,0,0,0);
}


- (UITextPosition *)closestPositionToPoint:(CGPoint)point
{
	REPORT_EVENT;
	return nil;
}

- (UITextPosition *)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange *)range
{
	REPORT_EVENT;
	return nil;
}

- (UITextRange *)characterRangeAtPoint:(CGPoint)point
{
	REPORT_EVENT;
	return nil;
}

- (NSArray *)selectionRectsForRange:(UITextRange *)range
{
	REPORT_EVENT;
	return nil;
}


- (NSDictionary *)textStylingAtPosition:(UITextPosition *)position inDirection:(UITextStorageDirection)direction
{
	// @todo keyboard: This is called
	return @{ };
}


- (void) setInputDelegate: (id <UITextInputDelegate>) delegate
{
	// @todo keyboard: This is called
}

- (id <UITextInputTokenizer>) tokenizer
{
	// @todo keyboard: This is called
	return nil;
}

- (id <UITextInputDelegate>) inputDelegate
{
	// @todo keyboard: This is called
	return nil;
}





#if !PLATFORM_TVOS
- (void)keyboardWasShown:(NSNotification*)aNotification
{
	// send a callback to let the game know where to sldie the textbox up above
	NSDictionary* info = [aNotification userInfo];
	CGRect Frame = [[info objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
	
	FPlatformRect ScreenRect;
	ScreenRect.Top = Frame.origin.y;
	ScreenRect.Bottom = (Frame.origin.y + Frame.size.height);
	ScreenRect.Left = Frame.origin.x;
	ScreenRect.Right = (Frame.origin.x + Frame.size.width);
	
	[FIOSAsyncTask CreateTaskWithBlock:^bool(void){
		[IOSAppDelegate GetDelegate].IOSApplication->OnVirtualKeyboardShown().Broadcast(ScreenRect);
		return true;
	 }];
}

// Called when the UIKeyboardWillHideNotification is sent
- (void)keyboardWillBeHidden:(NSNotification*)aNotification
{
	[FIOSAsyncTask CreateTaskWithBlock:^bool(void){
		[IOSAppDelegate GetDelegate].IOSApplication->OnVirtualKeyboardHidden().Broadcast();
		return true;
	 }];
}
#endif

- (void)InitKeyboard
{
#if !PLATFORM_TVOS
	KeyboardShowCount = 0;
	
	bool bUseIntegratedKeyboard = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bUseIntegratedKeyboard"), bUseIntegratedKeyboard, GEngineIni);
	
	// get notifications when the keyboard is in view
	bIsUsingIntegratedKeyboard = FParse::Param(FCommandLine::Get(), TEXT("NewKeyboard")) || bUseIntegratedKeyboard;
	if (bIsUsingIntegratedKeyboard)
	{
		[[NSNotificationCenter defaultCenter] addObserver:self
												 selector:@selector(keyboardWasShown:)
													 name:UIKeyboardDidShowNotification object:nil];
		
		[[NSNotificationCenter defaultCenter] addObserver:self
												 selector:@selector(keyboardWillBeHidden:)
													 name:UIKeyboardWillHideNotification object:nil];
		
	}
#endif
}





@end


#pragma mark IOSViewController

@implementation IOSViewController

/**
 * The ViewController was created, so now we need to create our view to be controlled (an EAGLView)
 */
- (void) loadView
{
	// get the landcape size of the screen
	CGRect Frame = [[UIScreen mainScreen] bounds];
	if (![IOSAppDelegate GetDelegate].bDeviceInPortraitMode)
	{
		Swap(Frame.size.width, Frame.size.height);
	}

	self.view = [[UIView alloc] initWithFrame:Frame];

	// settings copied from InterfaceBuilder
#if defined(__IPHONE_7_0)
	if ([IOSAppDelegate GetDelegate].OSVersion >= 7.0)
	{
		self.edgesForExtendedLayout = UIRectEdgeNone;
	}
#endif

	self.view.clearsContextBeforeDrawing = NO;
#if !PLATFORM_TVOS
	self.view.multipleTouchEnabled = NO;
#endif
}

/**
 * View was unloaded from us
 */ 
- (void) viewDidUnload
{
	UE_LOG(LogIOS, Log, TEXT("IOSViewController unloaded the view. This is unexpected, tell Josh Adams"));
	[super viewDidUnload];
}

/**
 * Tell the OS what the default supported orientations are
 */
- (NSUInteger)supportedInterfaceOrientations
{
	return UIInterfaceOrientationMaskAll;
}

/**
 * Tell the OS that our view controller can auto-rotate between supported orientations
 */
- (BOOL)shouldAutorotate
{
	return YES;
}

/**
 * Tell the OS to hide the status bar (iOS 7 method for hiding)
 */
- (BOOL)prefersStatusBarHidden
{
	return YES;
}







@end
