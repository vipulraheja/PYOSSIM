//
//  ossimPlanetView.mm
//  ossimplanetCocoa
//
//  Created by Eric Wing on 4/7/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import "ossimPlanetView.h"
#include <iostream>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h> // handy for gluErrorString
// Needed for Multithreaded OpenGL Engine (sysctlbyname for num CPUs)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <OpenGL/OpenGL.h> // for CoreOpenGL (CGL) for Multithreaded OpenGL Engine

#include <cctype> // needed for isprint()


#include <osg/Node>
#include <osg/Material>
#include <osg/FrameStamp>
#include <osgGA/GUIEventHandler>
#include <osgGA/EventQueue>
#include <osgGA/EventVisitor>
#include <osgGA/MatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/SetSceneViewVisitor>
#include <osgDB/ReadFile>
#include <osgUtil/SceneView>
#include <ossimPlanet/ossimPlanet.h>
#include <ossimPlanet/ossimPlanetTextureLayerGroup.h>
#include <ossimPlanet/ossimPlanetSceneView.h>
#include <ossimPlanet/ossimPlanetManipulator.h>
#include <ossimPlanet/ossimPlanetDatabasePager.h>
#include "ossimPlanet/ossimPlanetActionRouter.h"



// This is optional. This allows memory for things like textures and displaylists to be shared among different contexts.
#define SIMPLEVIEWER_USE_SHARED_CONTEXTS
#ifdef SIMPLEVIEWER_USE_SHARED_CONTEXTS
static NSOpenGLContext* s_sharedOpenGLContext = NULL;
#endif // SIMPLEVIEWER_USE_SHARED_CONTEXTS

// Taken/Adapted from one of the Apple OpenGL developer examples
static void Internal_SetAlpha(NSBitmapImageRep *imageRep, unsigned char alpha_value)
{
	register unsigned char * sp = [imageRep bitmapData];
	register int bytesPerRow = [imageRep bytesPerRow];
	register int height = [imageRep pixelsHigh];
	register int width = [imageRep pixelsWide];

	for(int i=0; i<height; i++)
	{
		register unsigned int * the_pixel = (unsigned int *) sp;
		register int w = width;
		while (w-- > 0)
		{
			unsigned char* sp_char = (unsigned char *) the_pixel;
//			register unsigned char * the_red = sp_char;
//			register unsigned char * the_green = (sp_char+1);
//			register unsigned char * the_blue = (sp_char+2);
			register unsigned char * the_alpha = (sp_char+3);
	
			*the_alpha = alpha_value;
			*the_pixel++;
		}
		sp += bytesPerRow;
	}
}


// Need to notify the controller something has changed (for bindings)
static void Internal_NotifyControllerAboutNewValue(NSDictionary* binding_info, id new_value)
{
	id observed_object_for_value = [binding_info objectForKey:NSObservedObjectKey];
	NSString* observed_keypath_for_value = [binding_info objectForKey:NSObservedKeyPathKey];

	// May need to deal with value transformers. (NSColor and NSUserDefaults is the basic example which needs NSUnarchiveFromData.)
	NSDictionary* options_for_binding = [binding_info objectForKey:NSOptionsKey];

	// Help from Ron and also
	// http://www.bignerdranch.com/palettes/x342.htm

//	NSLog(@"binding_info: %@", binding_info);
	// Get value transformer (or the name of the transformer)
	NSValueTransformer* value_transformer = [options_for_binding objectForKey:NSValueTransformerBindingOption];
	if(nil == value_transformer)
	{
		// This nil check seems overly paranoid to me as I would expect both NSValueTransformerBindingOption
		// and NSValueTransformerNameBindingOption to return values if valid, but maybe Hillegass knows something.
		NSString* value_transformer_name = [options_for_binding objectForKey:NSValueTransformerNameBindingOption];
		if(nil != value_transformer_name)
		{
			value_transformer = [NSValueTransformer valueTransformerForName:value_transformer_name];
		}
	}
	// If we have a valid transformer, and it's possible to reverse the transform, then we want that data.
	if((value_transformer != (id)[NSNull null]) && ([[value_transformer class] allowsReverseTransformation]))
	{
//		NSLog(@"allowsReverse");
		new_value = [value_transformer reverseTransformedValue:new_value];
	}
	
	[observed_object_for_value setValue:new_value forKeyPath:observed_keypath_for_value];
}



@implementation ossimPlanetView


// My simple pixel format definition
+ (NSOpenGLPixelFormat*) basicPixelFormat
{
	NSOpenGLPixelFormatAttribute pixel_attributes[] =
	{
		NSOpenGLPFAWindow,
		NSOpenGLPFADoubleBuffer,  // double buffered
		NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)32, // depth buffer size in bits
//		NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24, // Not sure if this helps
//		NSOpenGLPFAAlphaSize, (NSOpenGLPixelFormatAttribute)8, // Not sure if this helps
		(NSOpenGLPixelFormatAttribute)nil
    };
    return [[[NSOpenGLPixelFormat alloc] initWithAttributes:pixel_attributes] autorelease];
}



////////////////////////////////////////////////////////////////////////
/////////////////////////// Init Stuff /////////////////////////////////
////////////////////////////////////////////////////////////////////////

+ (void) initialize
{
	[self exposeBinding:@"hudEnabled"];
}

/* This is the designated initializer for an NSOpenGLView. However, since I'm 
 * using Interface Builder to help, initWithCoder: is the initializer that gets called.
 * But for completeness, I implement this method here.
 */
- (id) initWithFrame:(NSRect)frame_rect pixelFormat:(NSOpenGLPixelFormat*)pixel_format
{
	self = [super initWithFrame:frame_rect pixelFormat:pixel_format];
	if(self)
	{
		[self commonInit];
	}
	return self;
}

/* Going through the IB palette, this initializer is calling instead of the designated initializer
 * initWithFrame:pixelFormat: 
 * But for some reason, the pixel format set in IB selected seems to be either ignored or is missing
 * a value I need. (The depth buffer looks too shallow to me and glErrors are triggered.)
 * So I explicitly set the pixel format inside here (overriding the IB palette options).
 * This probably should be investigated, but since IB is getting an overhaul for Leopard,
 * I'll wait on this for now.
 */
- (id) initWithCoder:(NSCoder*)the_coder
{
	self = [super initWithCoder:the_coder];
	if(self)
	{
		NSOpenGLPixelFormat* pixel_format = [[self class] basicPixelFormat];
		[self setPixelFormat:pixel_format];
		[self commonInit];
	}
	return self;
}

/* Some generic code expecting regular NSView's may call this initializer instead of the specialized NSOpenGLView designated initializer.
 * I override this method here to make sure it does the right thing.
 */
- (id) initWithFrame:(NSRect)frame_rect
{
	self = [super initWithFrame:frame_rect pixelFormat:[[self class] basicPixelFormat]];
	if(self)
	{
		[self commonInit];
	}
	return self;
}


// My custom methods to centralize common init stuff
- (void) commonInit
{
	isUsingCtrlClick = NO;
	isUsingOptionClick = NO;
	isUsingMultithreadedOpenGLEngine = NO;

	[self initSharedOpenGLContext];

	[self initOSGViewer];
        [self initAnimationTimer];
        [self initNetworkConnectionTimer];
	
	// Register for Drag and Drop
	[self registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, NSURLPboardType, nil]];
	
    // Add minification observer so we can set the Dock picture since OpenGL views don't do this automatically for us.
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(prepareForMiniaturization:) name:NSWindowWillMiniaturizeNotification object:nil];

}

/* Optional: This will setup shared OpenGL contexts so resources like textures, etc, can be shared/reused
 * by multiple instances of SimpleViwerCocoa views.
 */
- (void) initSharedOpenGLContext
{
#ifdef SIMPLEVIEWER_USE_SHARED_CONTEXTS

	NSOpenGLContext* this_views_opengl_context = nil;
	
	// create a context the first time through
	if(s_sharedOpenGLContext == NULL)
	{
		s_sharedOpenGLContext = [[NSOpenGLContext alloc]
                      initWithFormat:[[self class] basicPixelFormat]
						shareContext:nil];
		
	}
	
	this_views_opengl_context = [[NSOpenGLContext alloc]
                      initWithFormat:[[self class] basicPixelFormat]
						shareContext:s_sharedOpenGLContext];
 	[self setOpenGLContext:this_views_opengl_context];
//	[this_views_opengl_context makeCurrentContext];
#endif // SIMPLEVIEWER_USE_SHARED_CONTEXTS
}

// Allocate a SimpleViewer and do basic initialization. No assumption about having an
// a valid OpenGL context is made by this function.
- (void) initOSGViewer
{
/*
#ifdef SIMPLEVIEWER_USE_SHARED_CONTEXTS
	// Workaround: osgViewer::SimpleViewer automatically increments its context ID values.
	// Since we're using a shared context, we want all SimpleViewer's to use the same context ID.
	// There is no API to avoid this behavior, so we need to undo what SimpleViewer's constructor did.
    simpleViewer->getSceneView()->getState()->setContextID(0);
	osg::DisplaySettings::instance()->setMaxNumberOfGraphicsContexts(1);
#endif // SIMPLEVIEWER_USE_SHARED_CONTEXTS
*/

	// thePlanet = new ossimPlanet;
	thePassAllUnhandledEventsFlag = true;
	theSceneView = new ossimPlanetSceneView;
	theFrameStamp = new osg::FrameStamp;
	theTimerId = -1;
	theRedrawPolicy = ossimPlanetQtGlWidgetRedraw_CONSTANT;
	theMouseNavigationFlag = true;

	// init()
	theMatrixManipulator = new ossimPlanetManipulator;
	theMatrixManipulator->setUseFrameEventForUpdateFlag(true);
        theMatrixManipulator->setAutoCalculateIntersectionFlag(false);

	theStateSetManipulator = new osgGA::StateSetManipulator;
	theEventVisitor = new osgGA::EventVisitor;
	// Cocoa follows the same coordinate convention as OpenGL. osgViewer's default is inverted.
	theEventQueue = new osgGA::EventQueue(osgGA::GUIEventAdapter::Y_INCREASING_UPWARDS);
	theEventQueue->setStartTick(theInitialTick);
   
//	theTimerInterval = 1000/60;
//   theSceneView->setViewport(0,0,width(),height());
	theSceneView->setDefaults();
	theSceneView->setFrameStamp(theFrameStamp.get());
//	float aspectRatio = static_cast<float>(width())/static_cast<float>(height());
//	theSceneView->setProjectionMatrixAsPerspective(50.0,
  //                                                aspectRatio,
 //                                                 1.0, 20.0);
	theSceneView->setDefaults();//osgUtil::SceneView::NO_SCENEVIEW_LIGHT);
	theDatabasePager = new ossimPlanetDatabasePager;//osgDB::Registry::instance()->getOrCreateDatabasePager();
	theDatabasePager->setUseFrameBlock(true);
	theSceneView->getCullVisitor()->setDatabaseRequestHandler(theDatabasePager.get());
	theSceneView->getUpdateVisitor()->setDatabaseRequestHandler(theDatabasePager.get());
	theSceneView->getCullVisitor()->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
//    theSceneView->getCullVisitor()->setComputeNearFarMode(osg::CullSettings::COMPUTE_NEAR_FAR_USING_PRIMITIVES);
//    theSceneView->getCullVisitor()->setCullingMode(osg::CullSettings::ENABLE_ALL_CULLING);
//    theSceneView->getCullVisitor()->setCullingMode(osg::CullSettings::);
	theStateSetManipulator->setStateSet(theSceneView->getGlobalStateSet());

	//  FIXME: What's this?
//	handleGUIActionUpdate();
   
	theSceneView->setClearColor(osg::Vec4(0.0,0.0,0.0, 1.0));
	//timer()->start(theTimerInterval);
//	float aspectRatio = static_cast<float>(width())/static_cast<float>(height());
//	theSceneView->setProjectionMatrixAsPerspective(50.0,
  //                                                aspectRatio,
 //                                                 1.0, 20.0);
//   osg::Viewport* viewport = theSceneView->getViewport();
//   theProjToWindowMatrix = viewport->computeWindowMatrix();
//   theWindowToProjMatrix = osg::Matrixd::inverse(theProjToWindowMatrix);
   


}

- (void) initAnimationTimer
{
	// Cocoa is event driven, so by default, there is nothing to trigger redraws for animation.
	// The easiest way to animate is to set a repeating NSTimer which triggers a redraw.
	SEL the_selector;
	NSMethodSignature* a_signature;
	NSInvocation* an_invocation;
	// animationCallback is my animation callback method
	the_selector = @selector( animationCallback );
	a_signature = [[self class] instanceMethodSignatureForSelector:the_selector];
	an_invocation = [NSInvocation invocationWithMethodSignature:a_signature] ;
	[an_invocation setSelector:the_selector];
	[an_invocation setTarget:self];
	
	animationTimer = [NSTimer
		scheduledTimerWithTimeInterval:1.0/60.0 // fps
		invocation:an_invocation
		repeats:YES];
	[animationTimer retain];
	
	// For single threaded apps like this one,
	// Cocoa seems to block timers or events sometimes. This can be seen
	// when I'm animating (via a timer) and you open an popup box or move a slider.
	// Apparently, sheets and dialogs can also block (try printing).
	// To work around this, Cocoa provides different run-loop modes. I need to 
	// specify the modes to avoid the blockage.
	// NSDefaultRunLoopMode seems to be the default. I don't think I need to explicitly
	// set this one, but just in case, I will set it anyway.
	[[NSRunLoop currentRunLoop] addTimer:animationTimer forMode:NSDefaultRunLoopMode];
	// This seems to be the one for preventing blocking on other events (popup box, slider, etc)
	[[NSRunLoop currentRunLoop] addTimer:animationTimer forMode:NSEventTrackingRunLoopMode];
	// This seems to be the one for dialogs.
	[[NSRunLoop currentRunLoop] addTimer:animationTimer forMode:NSModalPanelRunLoopMode];
}

- (void) initNetworkConnectionTimer
{
    // totally copied from initAnimationTimer()
    
    SEL the_selector;
    NSMethodSignature* a_signature;
    NSInvocation* an_invocation;
    the_selector = @selector( networkConnectionCallback );
    a_signature = [[self class] instanceMethodSignatureForSelector:the_selector];
    an_invocation = [NSInvocation invocationWithMethodSignature:a_signature] ;
    [an_invocation setSelector:the_selector];
    [an_invocation setTarget:self];
    
    networkConnectionTimer = [NSTimer
		scheduledTimerWithTimeInterval:1.0/60.0 // fps
                                    invocation:an_invocation
                                       repeats:YES];
    [networkConnectionTimer retain];
    
    [[NSRunLoop currentRunLoop] addTimer:networkConnectionTimer forMode:NSDefaultRunLoopMode];
    [[NSRunLoop currentRunLoop] addTimer:networkConnectionTimer forMode:NSEventTrackingRunLoopMode];
    [[NSRunLoop currentRunLoop] addTimer:networkConnectionTimer forMode:NSModalPanelRunLoopMode];
}

- (void) dealloc
{
    [animationTimer invalidate];
    [animationTimer release];
    [networkConnectionTimer invalidate];
    [networkConnectionTimer release];
    //	delete simpleViewer;
    //	simpleViewer = NULL;
    [super dealloc];
}

- (void) finalize
{
//	delete simpleViewer;
//	simpleViewer = NULL;
	[super finalize];
}

/* NSOpenGLView defines this method to be called (only once) after the OpenGL
 * context is created and made the current context. It is intended to be used to setup
 * your initial OpenGL state. This seems like a good place to initialize the 
 * OSG stuff. This method exists in 10.3 and later. If you are running pre-10.3, you
 * must manually call this method sometime after the OpenGL context is created and 
 * made current, or refactor this code.
 */
- (void) prepareOpenGL
{
	[super prepareOpenGL];
	
	// The NSOpenGLCPSwapInterval seems to be vsync. If 1, buffers are swapped with vertical refresh.
	// If 0, flushBuffer will execute as soon as possible.
	GLint swap_interval = 1 ;
    [[self openGLContext] setValues:&swap_interval forParameter:NSOpenGLCPSwapInterval];


	// Try new multithreaded OpenGL engine?
	// See Technical Note TN2085 Enabling multi-threaded execution of the OpenGL framework
	// http://developer.apple.com/technotes/tn2006/tn2085.html
	uint64_t num_cpus = 0;
	size_t num_cpus_length = sizeof(num_cpus);
	// Multithreaded engine only benefits with muliple CPUs, so do CPU count check
	if(sysctlbyname("hw.activecpu", &num_cpus, &num_cpus_length, NULL, 0) == 0)
	{
//		NSLog(@"Num CPUs=%d", num_cpus);
		if(num_cpus >= 2)
		{
			CGLError error_val = CGLEnable((CGLContextObj)[[self openGLContext] CGLContextObj], kCGLCEMPEngine);

			if(error_val != 0)
			{
				// The likelihood of failure seems quite high on older hardware, at least for now.
				// NSLog(@"Failed to enable Multithreaded OpenGL Engine: %s", CGLErrorString(error_val));
				isUsingMultithreadedOpenGLEngine = NO;
			}
			else
			{
				// NSLog(@"Success! Multithreaded OpenGL Engine activated!");
				isUsingMultithreadedOpenGLEngine = YES;
			}
		}
		else
		{
			isUsingMultithreadedOpenGLEngine = NO;
		}
	}

	// This is also might be a good place to setup OpenGL state that OSG doesn't control.
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

/*
	GLint maxbuffers[1];
	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, maxbuffers);
	NSLog(@"GL_MAX_COLOR_ATTACHMENTS=%d", maxbuffers[0]);
*/

	// We need to tell the osgViewer what the viewport size is
	[self resizeViewport];

}

/* disableScreenUpdatesUntilFlush was introduced in Tiger. It will prevent
 * unnecessary screen flashing caused by NSSplitViews or NSScrollviews.
 * From Apple's release notes:
 
NSWindow -disableScreenUpdatesUntilFlush API (Section added since WWDC)

When a view that renders to a hardware surface (such as an OpenGL view) is placed in an NSScrollView or NSSplitView, there can be a noticeable flicker or lag when the scroll position or splitter position is moved. This happens because each move of the hardware surface takes effect immediately, before the remaining window content has had the chance to draw and flush.

To enable applications to eliminate this visual artifact, Tiger AppKit provides a new NSWindow message, -disableScreenUpdatesUntilFlush. This message asks a window to suspend screen updates until the window's updated content is subsequently flushed to the screen. This message can be sent from a view that is about to move its hardware surface, to insure that the hardware surface move and window redisplay will be visually synchronized. The window responds by immediately disabling screen updates via a call to NSDisableScreenUpdates(), and setting a flag that will cause it to call NSEnableScreenUpdates() later, when the window flushes. It is permissible to send this message to a given window more than once during a given window update cycle; the window will only suspend and re-enable updates once during that cycle.

A view class that renders to a hardware surface can send this message from an override of -renewGState (a method that is is invoked immediately before the view's surface is moved) to effectively defer compositing of the moved surface until the window has finished drawing and flushing its remaining content.
A -respondsToSelector: check has been used to provide compatibility with previous system releases. On pre-Tiger systems, where NSWindow has no -disableScreenUpdatesUntilFlush method, the -renewGState override will have no effect.
 */
- (void) renewGState
{
    NSWindow* the_window = [self window];
    if([the_window respondsToSelector:@selector(disableScreenUpdatesUntilFlush)])
	{
		[the_window disableScreenUpdatesUntilFlush];
    }
    [super renewGState];
}


/* When you minimize an app, you usually can see its shrunken contents 
 * in the dock. However, an OpenGL view by default only produces a blank
 * white window. So we use this method to do an image capture of our view
 * which will be used as its minimized picture.
 * (A possible enhancement to consider is to update the picture over time.)
 */
- (void) prepareForMiniaturization:(NSNotification*)notification
{
	NSBitmapImageRep* ns_image_rep = [self renderOpenGLSceneToFramebuffer];
	if([self lockFocusIfCanDraw])
	{
		[ns_image_rep draw];
		[self unlockFocus];
		[[self window] flushWindow];
	}
}

/* Allow people to easily query if the multithreaded OpenGL engine is activated.
 */
- (BOOL) isUsingMultithreadedOpenGLEngine
{
	return isUsingMultithreadedOpenGLEngine;
}


////////////////////////////////////////////////////////////////////////
/////////////////////////// End Init Stuff /////////////////////////////
////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////
/////////////////////////// Mouse Stuff ////////////////////////////////
////////////////////////////////////////////////////////////////////////

- (void) mouseDown:(NSEvent*)the_event
{
	// Because many Mac users have only a 1-button mouse, we should provide ways
	// to access the button 2 and 3 actions of osgViewer.
	// I will use the Ctrl modifer to represent right-clicking
	// and Option modifier to represent middle clicking.
	if([the_event modifierFlags] & NSControlKeyMask)
	{
		[self setIsUsingCtrlClick:YES];
		[self doRightMouseButtonDown:the_event];
	}
	else if([the_event modifierFlags] & NSAlternateKeyMask)
	{
		[self setIsUsingOptionClick:YES];
		[self doMiddleMouseButtonDown:the_event];
	}
	else if([the_event modifierFlags] & NSCommandKeyMask)
	{
		[self startDragAndDropAsSource:the_event];
	}
	else
	{
		[self doLeftMouseButtonDown:the_event];
	}
}

- (void) mouseDragged:(NSEvent*)the_event
{
	// We must convert the mouse event locations from the window coordinate system to the
	// local view coordinate system.
	NSPoint the_point = [the_event locationInWindow];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];
//    NSPoint converted_point = [self convertPoint:the_point fromView:nil];
	
	theEventQueue->mouseMotion(converted_point.x, converted_point.y);
	[self setNeedsDisplay:YES];
}

- (void) mouseUp:(NSEvent*)the_event
{
	// Because many Mac users have only a 1-button mouse, we should provide ways
	// to access the button 2 and 3 actions of osgViewer.
	// I will use the Ctrl modifer to represent right-clicking
	// and Option modifier to represent middle clicking.
	if([self isUsingCtrlClick] == YES)
	{
		[self setIsUsingCtrlClick:NO];
		[self doRightMouseButtonUp:the_event];
	}
	else if([self isUsingOptionClick] == YES)
	{
		[self setIsUsingOptionClick:NO];
		[self doMiddleMouseButtonUp:the_event];
	}
	else
	{
		[self doLeftMouseButtonUp:the_event];
	}
}

- (void) rightMouseDown:(NSEvent*)the_event
{
	[self doRightMouseButtonDown:the_event];
}

- (void) rightMouseDragged:(NSEvent*)the_event
{
	// We must convert the mouse event locations from the window coordinate system to the
	// local view coordinate system.
	NSPoint the_point = [the_event locationInWindow];
//    NSPoint converted_point = [self convertPoint:the_point fromView:nil];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];
	theEventQueue->mouseMotion(converted_point.x, converted_point.y);
	
//	simpleViewer->getEventQueue()->mouseMotion(converted_point.x, converted_point.y);
	[self setNeedsDisplay:YES];
}
- (void) middleMouseDragged:(NSEvent*)the_event
{
    NSPoint the_point = [the_event locationInWindow];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];
    theEventQueue->mouseMotion(converted_point.x, converted_point.y);
}

- (void) rightMouseUp:(NSEvent*)the_event
{
	[self doRightMouseButtonUp:the_event];
}

// "otherMouse" seems to capture middle button and any other buttons beyond (4th, etc).
- (void) otherMouseDown:(NSEvent*)the_event
{
	// Button 0 is left
	// Button 1 is right
	// Button 2 is middle
	// Button 3 keeps going
	// osgViewer expects 1 for left, 3 for right, 2 for middle
	// osgViewer has a reversed number mapping for right and middle compared to Cocoa
	if([the_event buttonNumber] == 2)
	{
		[self doMiddleMouseButtonDown:the_event];
	}
	else // buttonNumber should be 3,4,5,etc; must map to 4,5,6,etc in osgViewer
	{
		[self doExtraMouseButtonDown:the_event buttonNumber:[the_event buttonNumber]];
	}
}

- (void) otherMouseDragged:(NSEvent*)the_event
{

	// We must convert the mouse event locations from the window coordinate system to the
	// local view coordinate system.
	NSPoint the_point = [the_event locationInWindow];
//    NSPoint converted_point = [self convertPoint:the_point fromView:nil];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];
    theEventQueue->mouseMotion(converted_point.x, converted_point.y);
	
//	simpleViewer->getEventQueue()->mouseMotion(converted_point.x, converted_point.y);
	[self setNeedsDisplay:YES];
}

// "otherMouse" seems to capture middle button and any other buttons beyond (4th, etc).
- (void) otherMouseUp:(NSEvent*)the_event
{
	// Button 0 is left
	// Button 1 is right
	// Button 2 is middle
	// Button 3 keeps going
	// osgViewer expects 1 for left, 3 for right, 2 for middle
	// osgViewer has a reversed number mapping for right and middle compared to Cocoa
	if([the_event buttonNumber] == 2)
	{
		[self doMiddleMouseButtonUp:the_event];
	}
	else // buttonNumber should be 3,4,5,etc; must map to 4,5,6,etc in osgViewer
	{
		// I don't think osgViewer does anything for these additional buttons,
		// but just in case, pass them along. But as a Cocoa programmer, you might 
		// think about things you can do natively here instead of passing the buck.
	}	[self doExtraMouseButtonUp:the_event buttonNumber:[the_event buttonNumber]];
}

- (void) setIsUsingCtrlClick:(BOOL)is_using_ctrl_click
{
	isUsingCtrlClick = is_using_ctrl_click;
}

- (BOOL) isUsingCtrlClick
{
	return isUsingCtrlClick;
}

- (void) setIsUsingOptionClick:(BOOL)is_using_option_click
{
	isUsingOptionClick = is_using_option_click;
}

- (BOOL) isUsingOptionClick
{
	return isUsingOptionClick;
}


- (void) doLeftMouseButtonDown:(NSEvent*)the_event
{
	// We must convert the mouse event locations from the window coordinate system to the
	// local view coordinate system.
	NSPoint the_point = [the_event locationInWindow];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];
//    NSPoint converted_point = [self convertPoint:the_point fromView:nil];
	if([the_event clickCount] == 1)
	{
		theEventQueue->mouseButtonPress(converted_point.x, converted_point.y, 1);
	}
	else
	{
		theEventQueue->mouseDoubleButtonPress(converted_point.x, converted_point.y, 1);
	}
	[self setNeedsDisplay:YES];
}

- (void) doLeftMouseButtonUp:(NSEvent*)the_event
{
	// We must convert the mouse event locations from the window coordinate system to the
	// local view coordinate system.
	NSPoint the_point = [the_event locationInWindow];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];
//    NSPoint converted_point = [self convertPoint:the_point fromView:nil];
	
	theEventQueue->mouseButtonRelease(converted_point.x, converted_point.y, 1);
	[self setNeedsDisplay:YES];
}

- (void) doRightMouseButtonDown:(NSEvent*)the_event
{
    // We must convert the mouse event locations from the window coordinate system to the
    // local view coordinate system.
    NSPoint the_point = [the_event locationInWindow];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];
    //    NSPoint converted_point = [self convertPoint:the_point fromView:nil];
    if([the_event clickCount] == 1)
    {
        //		simpleViewer->getEventQueue()->mouseButtonPress(converted_point.x, converted_point.y, 3);
    }
    else
    {
        //		simpleViewer->getEventQueue()->mouseDoubleButtonPress(converted_point.x, converted_point.y, 3);
    }
    theEventQueue->mouseButtonPress(converted_point.x, converted_point.y, 3);
    [self setNeedsDisplay:YES];
}


- (void) doRightMouseButtonUp:(NSEvent*)the_event
{
	// We must convert the mouse event locations from the window coordinate system to the
	// local view coordinate system.
	NSPoint the_point = [the_event locationInWindow];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];
//    NSPoint converted_point = [self convertPoint:the_point fromView:nil];
	
//	simpleViewer->getEventQueue()->mouseButtonRelease(converted_point.x, converted_point.y, 3);
	theEventQueue->mouseButtonRelease(converted_point.x, converted_point.y, 3);
	[self setNeedsDisplay:YES];
}

- (void) doMiddleMouseButtonDown:(NSEvent*)the_event
{
	// We must convert the mouse event locations from the window coordinate system to the
	// local view coordinate system.
	NSPoint the_point = [the_event locationInWindow];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];
//    NSPoint converted_point = [self convertPoint:the_point fromView:nil];
	
	if([the_event clickCount] == 1)
	{
//		simpleViewer->getEventQueue()->mouseButtonPress(converted_point.x, converted_point.y, 2);
	}
	else
	{
//		simpleViewer->getEventQueue()->mouseDoubleButtonPress(converted_point.x, converted_point.y, 2);
	}
	theEventQueue->mouseButtonPress(converted_point.x, converted_point.y, 2);
	[self setNeedsDisplay:YES];
}

- (void) doExtraMouseButtonDown:(NSEvent*)the_event buttonNumber:(int)button_number
{
	// We must convert the mouse event locations from the window coordinate system to the
	// local view coordinate system.
	NSPoint the_point = [the_event locationInWindow];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];
//    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];

	if([the_event clickCount] == 1)
	{
//		simpleViewer->getEventQueue()->mouseButtonPress(converted_point.x, converted_point.y, button_number+1);
	}
	else
	{
//		simpleViewer->getEventQueue()->mouseDoubleButtonPress(converted_point.x, converted_point.y, button_number+1);
	}
	[self setNeedsDisplay:YES];
}

- (NSPoint) convertToOsgPoint:(NSPoint)the_pt
{
   NSPoint result;
   NSRect rect = [self bounds];
   float w = rect.size.width;
   float h = rect.size.height;

   float cx = w/2.0;
   float cy = h/2.0;

   result.x = (the_pt.x-cx)/(w/2.0);
   result.y = (the_pt.y-cy)/(h/2.0);

   return result;
}

- (void) doMiddleMouseButtonUp:(NSEvent*)the_event
{
	// We must convert the mouse event locations from the window coordinate system to the
	// local view coordinate system.	NSPoint the_point = [the_event locationInWindow];
 	NSPoint the_point = [the_event locationInWindow];
//	NSPoint converted_point = [self convertPoint:the_point fromView:nil];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];

//	simpleViewer->getEventQueue()->mouseButtonRelease(converted_point.x, converted_point.y, 2);
	theEventQueue->mouseButtonRelease(converted_point.x, converted_point.y, 2);
	[self setNeedsDisplay:YES];
}

- (void) doExtraMouseButtonUp:(NSEvent*)the_event buttonNumber:(int)button_number
{
	// We must convert the mouse event locations from the window coordinate system to the
	// local view coordinate system.	NSPoint the_point = [the_event locationInWindow];
	NSPoint the_point = [the_event locationInWindow];
//	NSPoint converted_point = [self convertPoint:the_point fromView:nil];
    NSPoint converted_point = [self convertToOsgPoint:[self convertPoint:the_point fromView:nil] ];

	theEventQueue->mouseButtonRelease(converted_point.x, converted_point.y, button_number+1);
	
	[self setNeedsDisplay:YES];
}

// This is a job for Mighty Mouse!
// For the most fluid experience turn on 360 degree mode availble in 10.4.8+.
// With your Mighty Mouse plugged in, 
// open 'Keyboard & Mouse' in 'System Preferences'. 
// Select the 'Mouse' tab.
// Under 'Scrolling Options' select '360 degree'. 
// That should improve diagonal scrolling.
// You should also be able to use 'two-finger scrolling' on newer laptops.
- (void) scrollWheel:(NSEvent*)the_event
{
	// Unfortunately, it turns out mouseScroll2D doesn't actually do anything.
	// The camera manipulators don't seem to implement any code that utilize the scroll values.
	// This this call does nothing.
	theEventQueue->mouseScroll2D([the_event deltaX], [the_event deltaY]);

	// With the absense of a useful mouseScroll2D API, we can manually simulate the desired effect.
//	NSPoint the_point = [the_event locationInWindow];
//	NSPoint converted_point = [self convertPoint:the_point fromView:nil];
//	simpleViewer->getEventQueue()->mouseButtonPress(converted_point.x, converted_point.y, 1);
//	simpleViewer->getEventQueue()->mouseMotion(converted_point.x + -[the_event deltaX], converted_point.y + [the_event deltaY]);
//	simpleViewer->getEventQueue()->mouseButtonRelease(converted_point.x + -[the_event deltaX], converted_point.y + [the_event deltaY], 1);

	[self setNeedsDisplay:YES];
}

////////////////////////////////////////////////////////////////////////
/////////////////////////// End Mouse Stuff ////////////////////////////
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/////////////////////////// Keyboard Stuff /////////////////////////////
////////////////////////////////////////////////////////////////////////
// Needed to accept keyboard events
- (BOOL) acceptsFirstResponder
{
	return YES;
}

- (void) keyDown:(NSEvent*)the_event
{
	osgGA::GUIEventAdapter::KeySymbol osg_key = (osgGA::GUIEventAdapter::KeySymbol)0;

	[self handleModifiers:the_event];
	BOOL handled_key = [self getOsgKey:osg_key fromCocoaEvent:the_event];
	
	if(handled_key)
	{
		theEventQueue->keyPress(osg_key);
	}
	else
	{
		[super keyDown:the_event];
	}

	[self setNeedsDisplay:YES];
}


- (void) keyUp:(NSEvent*)the_event
{
	osgGA::GUIEventAdapter::KeySymbol osg_key = (osgGA::GUIEventAdapter::KeySymbol)0;

	[self handleModifiers:the_event];
	BOOL handled_key = [self getOsgKey:osg_key fromCocoaEvent:the_event];
	
	if(handled_key)
	{
		theEventQueue->keyRelease(osg_key);
	}
	else
	{
		[super keyDown:the_event];
	}

	[self setNeedsDisplay:YES];
}

- (void) handleModifiers:(NSEvent*)the_event
{
	osgGA::GUIEventAdapter* the_adapter = theEventQueue->getCurrentEventState();
	unsigned int mod_key_mask = 0;

	if([the_event modifierFlags] & NSShiftKeyMask)
	{
		mod_key_mask |= osgGA::GUIEventAdapter::MODKEY_SHIFT;
	}
	if([the_event modifierFlags] & NSControlKeyMask)
	{
		mod_key_mask |= osgGA::GUIEventAdapter::MODKEY_CTRL;
	}
	if([the_event modifierFlags] & NSAlternateKeyMask)
	{
		mod_key_mask |= osgGA::GUIEventAdapter::MODKEY_ALT;
	}
	if([the_event modifierFlags] & NSCommandKeyMask)
	{
		mod_key_mask |= osgGA::GUIEventAdapter::MODKEY_META;
	}
	if([the_event modifierFlags] & NSNumericPadKeyMask)
	{
		mod_key_mask |= osgGA::GUIEventAdapter::MODKEY_NUM_LOCK;
	}
	if([the_event modifierFlags] & NSAlphaShiftKeyMask)
	{
		mod_key_mask |= osgGA::GUIEventAdapter::MODKEY_CAPS_LOCK;
	}
	the_adapter->setModKeyMask(mod_key_mask);
}

- (BOOL) getOsgKey:(osgGA::GUIEventAdapter::KeySymbol&)osg_key fromCocoaEvent:(NSEvent*)the_event 
{
	BOOL handled_event = NO;

	// Do you want characters or charactersIgnoringModifiers?
	NSString* event_characters = [the_event characters];
//	NSString* event_characters = [the_event charactersIgnoringModifiers];

	osg_key = (osgGA::GUIEventAdapter::KeySymbol)0;

	unichar unicode_character = [event_characters characterAtIndex:0];
	
	
	switch(unicode_character)
	{
		case NSUpArrowFunctionKey:
		{
			osg_key = osgGA::GUIEventAdapter::KEY_Up;
			handled_event = YES;
			break;
		}
		case NSDownArrowFunctionKey:
		{
			osg_key = osgGA::GUIEventAdapter::KEY_Down;
			handled_event = YES;
			break;
		}
		case NSLeftArrowFunctionKey:
		{
			osg_key = osgGA::GUIEventAdapter::KEY_Left;
			handled_event = YES;
			break;
		}
		case NSRightArrowFunctionKey:
		{
			osg_key = osgGA::GUIEventAdapter::KEY_Right;
			handled_event = YES;
			break;
		}
		case NSCarriageReturnCharacter:
		case NSEnterCharacter:
		{
			osg_key = osgGA::GUIEventAdapter::KEY_Return;
			handled_event = YES;
			break;
		}
		default:
		{
			/*
		
			if(((unicode_character >= 'A') &&
				(unicode_character <= 'Z'))||
				((unicode_character >= 'a')&&
				(unicode_character <= 'z')))
			*/
			// Should capture all ascii printable characters
			if( ((unicode_character & 0xFF80) == 0) 
				&& ( isprint (unicode_character & 0x7F) ) )
			{
				osg_key = (osgGA::GUIEventAdapter::KeySymbol)(unicode_character);
				handled_event = YES;
			}
		}
	}
	return handled_event;
}

////////////////////////////////////////////////////////////////////////
/////////////////////////// End Keyboard Stuff /////////////////////////
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/////////////////////////// View and Draw Stuff ////////////////////////
////////////////////////////////////////////////////////////////////////

// This method is periodically called by my timer.
- (void) animationCallback
{
	// Simply notify Cocoa that a drawRect needs to take place.
	// Potential optimization is to query the OSG stuff to find out if a redraw is actually necessary.
	[self setNeedsDisplay:YES];
}

// This method is periodically called by networkConnectionTimer
- (void) networkConnectionCallback
{
    ossimPlanetActionRouter::instance()->executeNetworkActions();
}

// This is an optional optimization. This states you don't have a transparent view/window.
// Obviously don't use this or set it to NO if you intend for your view to be see-through.
- (BOOL) isOpaque
{
	return YES;
}

// Resolution Independent UI is coming... (Tiger's Quartz Debug already has the tool.)
// We must think in 'point sizes', not pixel sizes, so a conversion is needed for OpenGL.
- (void) resizeViewport
{
	NSSize size_in_points = [self bounds].size;
	// This coordinate system conversion seems to make things work with Quartz Debug.
	NSSize size_in_window_coordinates = [self convertSize:size_in_points toView:nil];
//	simpleViewer->getEventQueue()->windowResize(0, 0, size_in_window_coordinates.width, size_in_window_coordinates.height);

	theSceneView->setViewport(0, 0, size_in_window_coordinates.width, size_in_window_coordinates.height);

	if(size_in_window_coordinates.height <= 0.0f)
	{
		size_in_window_coordinates.height = 10.0f;
	}
	
	float aspect_ratio = size_in_window_coordinates.width/size_in_window_coordinates.height;
	theSceneView->setProjectionMatrixAsPerspective(50.0,
                                                  aspect_ratio,
                                                  1.0, 20.0);
   osg::Viewport* viewport = theSceneView->getViewport();
   theProjToWindowMatrix = viewport->computeWindowMatrix();
   theWindowToProjMatrix = osg::Matrixd::inverse(theProjToWindowMatrix);
  
}



// For window resize
- (void) reshape
{
	[super reshape];
	[self resizeViewport];
}

// This is the code that actually draws.
// Remember you shouldn't call drawRect: directly and should use setNeedsDisplay:YES
// This is so the operating system can optimize when a draw is actually needed.
// (e.g. No sense drawing when the application is hidden.)
- (void) drawRect:(NSRect)the_rect
{
	if([[NSGraphicsContext currentContext] isDrawingToScreen])
	{
//		[[self openGLContext] makeCurrentContext];
		[self renderScene];
		[[self openGLContext] flushBuffer];
	}
	else // This is usually the print case
	{
//		[[self openGLContext] makeCurrentContext];

		// FIXME: We should be computing a size that fits best to the paper target
		NSSize size_in_points = [self bounds].size;
		NSSize size_in_window_coordinates = [self convertSize:size_in_points toView:nil];
		NSBitmapImageRep * bitmap_image_rep = [self renderOpenGLSceneToFramebufferAsFormat:GL_RGB viewWidth:size_in_window_coordinates.width viewHeight:size_in_window_coordinates.height];
		
		NSImage* ns_image = [self imageFromBitmapImageRep:bitmap_image_rep];

		if(ns_image)
		{
			NSSize image_size = [ns_image size];
			[ns_image drawAtPoint:NSMakePoint(0.0, 0.0) 
					fromRect: NSMakeRect(0.0, 0.0, image_size.width, image_size.height)
//				   operation: NSCompositeSourceOver
				   operation: NSCompositeCopy
					fraction: 1.0]; 	
		}
		else
		{
			NSLog(@"Image not valid");
		}
	}
}

- (void) updateScene
{
//   theEventQueue->frame(theFrameStamp->getReferenceTime());
   osgGA::EventQueue::Events events;
   theEventQueue->takeEvents(events);
   if (theEventVisitor.valid())
   {
      theEventVisitor->setTraversalNumber(theFrameStamp->getFrameNumber());
   }
   if(thePlanet.valid())
   {
      theMatrixManipulator->setLosXYZ(thePlanet->getLineOfSitePoint());
   }
   
   // dispatch the events in order of arrival.
   for(osgGA::EventQueue::Events::iterator event_itr=events.begin();
       event_itr!=events.end();
       ++event_itr)
   {
      bool handled = false;
      if (theEventVisitor.valid())
      {
         theEventVisitor->reset();
         theEventVisitor->addEvent(event_itr->get());
         theSceneView->getSceneData()->accept(*theEventVisitor);
         if (theEventVisitor->getEventHandled())
         {
	    handled = true;
         }
      }
      if(!handled)
      {
         handled = theStateSetManipulator->handle(*(*event_itr),theActionAdapter);
      }
      if(!handled)
      {
         handled = theMatrixManipulator->handle(*(*event_itr),theActionAdapter);
      }
      
   }
}


- (void) renderScene
{
   osg::Timer_t initialT = theTimer.tick();
   theFrameStamp->setFrameNumber(theFrameStamp->getFrameNumber()+1);
   theFrameStamp->setReferenceTime(theTimer.delta_s(theInitialTick,theTimer.tick()));
   theEventQueue->frame(theFrameStamp->getReferenceTime());
   
  if (theEventVisitor.valid())
  {
     theEventVisitor->setTraversalNumber(theFrameStamp->getFrameNumber());
  }
   // Update the model view on the scene view.
   if(theMatrixManipulator.get())
   {
      theSceneView->setViewMatrix(theMatrixManipulator->getInverseMatrix());
   }
    theDatabasePager->signalBeginFrame(theFrameStamp.get());
    theDatabasePager->updateSceneGraph(theFrameStamp->getReferenceTime());

	[self updateScene];


    osg::Timer_t updateT = theTimer.tick();
    theSceneView->update();
    osg::Timer_t cullT = theTimer.tick();
    theSceneView->cull();
    osg::Timer_t drawT = theTimer.tick();
    theSceneView->draw();
    osg::Timer_t endT = theTimer.tick();
    theDatabasePager->signalEndFrame();
  //  swapBuffers();
    double availableTime = 0.0025; // 2.5 ms
    
    theUpdateTime = theTimer.delta_m(updateT, cullT);
    theCullTime   = theTimer.delta_m(cullT, drawT);
    theDrawTime   = theTimer.delta_m(drawT, endT);
    theFrameTime  = theTimer.delta_m(updateT, endT);

//     std::cout << "Frame time = " << theFrameTime << std::endl;
    
    // compile any GL objects that are required.
    theDatabasePager->compileGLObjects(*(theSceneView->getState()),availableTime);
    
    // flush deleted GL objects.
    theSceneView->flushDeletedGLObjects(availableTime);
//    theSceneView->flushAllDeletedGLObjects();
    double lat, lon, hgt, h, p, r;
	[self getViewPositionLat:lat lon:lon height:hgt heading:h pitch:p roll:r];

    if((ossimAbs(theViewLat-lat) > FLT_EPSILON)||
       (ossimAbs(theViewLon-lon) > FLT_EPSILON)||
       (ossimAbs(theViewHgt-hgt) > FLT_EPSILON)||
       (ossimAbs(theViewHeading-h) > FLT_EPSILON)||
       (ossimAbs(theViewPitch-p) > FLT_EPSILON)||
       (ossimAbs(theViewRoll-r) > FLT_EPSILON))
    {
       theViewLat = lat;
       theViewLon = lon;
       theViewHgt = hgt;
       theViewHeading = h;
       theViewPitch = p;
       theViewRoll = r;
 
		// FIXME: How is this supposed to work?
	   //      emit signalViewPositionChangedLatLonHgtHPR(theViewLat, theViewLon, theViewHgt,
       //                                           theViewHeading, theViewPitch, theViewRoll);
    }
}

/* Optional render to framebuffer stuff below. The code renders offscreen to assist in screen capture stuff.
 * This can be useful for things like the Dock minimization picture, drag-and-drop dropImage, copy and paste,
 * and printing.
 */

// Convenience version. Will use the current view's bounds and produce an RGB image with the current clear color.
- (NSBitmapImageRep*) renderOpenGLSceneToFramebuffer
{
	NSSize size_in_points = [self bounds].size;
	NSSize size_in_window_coordinates = [self convertSize:size_in_points toView:nil];
	const osg::Vec4& clear_color = theSceneView->getClearColor();

	return [self renderOpenGLSceneToFramebufferAsFormat:GL_RGB viewWidth:size_in_window_coordinates.width viewHeight:size_in_window_coordinates.height clearColorRed:clear_color[0] clearColorGreen:clear_color[1] clearColorBlue:clear_color[2] clearColorAlpha:clear_color[3]];
}

// Convenience version. Allows you to specify the view and height and format, but uses the current the current clear color.
- (NSBitmapImageRep*) renderOpenGLSceneToFramebufferAsFormat:(int)gl_format viewWidth:(float)view_width viewHeight:(float)view_height
{
	const osg::Vec4& clear_color = theSceneView->getClearColor();

	return [self renderOpenGLSceneToFramebufferAsFormat:gl_format viewWidth:view_width viewHeight:view_height clearColorRed:clear_color[0] clearColorGreen:clear_color[1] clearColorBlue:clear_color[2] clearColorAlpha:clear_color[3]];
}

// Renders to an offscreen buffer and returns a copy of the data to an NSBitmapImageRep.
// Allows you to specify the gl_format, width and height, and the glClearColor
// gl_format is only GL_RGB or GLRGBA.
- (NSBitmapImageRep*) renderOpenGLSceneToFramebufferAsFormat:(int)gl_format viewWidth:(float)view_width viewHeight:(float)view_height clearColorRed:(float)clear_red clearColorGreen:(float)clear_green clearColorBlue:(float)clear_blue clearColorAlpha:(float)clear_alpha
{
	// Round values and bring to closest integer.
	int viewport_width = (int)(view_width + 0.5f);
	int viewport_height = (int)(view_height + 0.5f);
	
	NSBitmapImageRep* ns_image_rep;
	osg::ref_ptr<osg::Image> osg_image = new osg::Image;
	
	if(GL_RGBA == gl_format)
	{
		// Introduced in 10.4, but gives much better looking results if you utilize transparency
		if([NSBitmapImageRep instancesRespondToSelector:@selector(initWithBitmapDataPlanes:pixelsWide:pixelsHigh:bitsPerSample:samplesPerPixel:hasAlpha:isPlanar:colorSpaceName:bitmapFormat:bytesPerRow:bitsPerPixel:)])
		{
			ns_image_rep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
												  pixelsWide:viewport_width
												  pixelsHigh:viewport_height
											   bitsPerSample:8
											 samplesPerPixel:4
													hasAlpha:YES
													isPlanar:NO
											  colorSpaceName:NSCalibratedRGBColorSpace
												bitmapFormat:NSAlphaNonpremultipliedBitmapFormat // 10.4+, gives much better looking results if you utilize transparency
												 bytesPerRow:osg::Image::computeRowWidthInBytes(viewport_width, GL_RGBA, GL_UNSIGNED_BYTE, 1)
												bitsPerPixel:32]
					autorelease];
		}
		else // fallback for 10.0 to 10.3
		{
			ns_image_rep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
												  pixelsWide:viewport_width
												  pixelsHigh:viewport_height
											   bitsPerSample:8
											 samplesPerPixel:4
													hasAlpha:YES
													isPlanar:NO
											  colorSpaceName:NSCalibratedRGBColorSpace
												// bitmapFormat:NSAlphaNonpremultipliedBitmapFormat // 10.4+, gives much better looking results if you utilize transparency
												 bytesPerRow:osg::Image::computeRowWidthInBytes(viewport_width, GL_RGBA, GL_UNSIGNED_BYTE, 1)
												bitsPerPixel:32]
					autorelease];
		}
		// This is an optimization. Instead of creating data in both an osg::Image and NSBitmapImageRep,
		// Allocate just the memory in the NSBitmapImageRep and give the osg::Image a reference to the data.
		// I let NSBitmapImageRep control the memory because I think it will be easier to deal with 
		// memory management in the cases where it must interact with other Cocoa mechanisms like Drag-and-drop
		// where the memory persistence is less obvious. Make sure that you don't expect to use the osg::Image
		// outside the scope of this function because there is no telling when the data will be removed out
		// from under it by Cocoa since osg::Image will not retain.
		osg_image->setImage([ns_image_rep pixelsWide], [ns_image_rep pixelsHigh], 1,  GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, [ns_image_rep bitmapData], osg::Image::NO_DELETE, 1);
	}
	else if(GL_RGB == gl_format)
	{
		ns_image_rep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
												  pixelsWide:viewport_width
												  pixelsHigh:viewport_height
											   bitsPerSample:8
											 samplesPerPixel:3
													hasAlpha:NO
													isPlanar:NO
											  colorSpaceName:NSCalibratedRGBColorSpace
												// bitmapFormat:(NSBitmapFormat)0 // 10.4+
												 bytesPerRow:osg::Image::computeRowWidthInBytes(viewport_width, GL_RGB, GL_UNSIGNED_BYTE, 1)
												bitsPerPixel:24]
					autorelease];

		// This is an optimization. Instead of creating data in both an osg::Image and NSBitmapImageRep,
		// Allocate just the memory in the NSBitmapImageRep and give the osg::Image a reference to the data.
		// I let NSBitmapImageRep control the memory because I think it will be easier to deal with 
		// memory management in the cases where it must interact with other Cocoa mechanisms like Drag-and-drop
		// where the memory persistence is less obvious. Make sure that you don't expect to use the osg::Image
		// outside the scope of this function because there is no telling when the data will be removed out
		// from under it by Cocoa since osg::Image will not retain.
		osg_image->setImage([ns_image_rep pixelsWide], [ns_image_rep pixelsHigh], 1,  GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, [ns_image_rep bitmapData], osg::Image::NO_DELETE, 1);
	}
	else
	{
		NSLog(@"Sorry, unsupported format in renderOpenGLSceneToFramebufferAsFormat");
		return nil;
	}

	// Can't find a way to query SimpleViewer for the current values, so recompute current view size.
	NSSize original_size_in_points = [self bounds].size;
	NSSize original_size_in_window_coordinates = [self convertSize:original_size_in_points toView:nil];
//	simpleViewer->getEventQueue()->windowResize(0, 0, original_size_in_window_coordinates.width, original_size_in_window_coordinates.height);
#if 0
	simpleViewer->getEventQueue()->windowResize(0, 0, viewport_width, viewport_height);

	/*
	 * I want to use a Framebuffer Object because it seems to be the OpenGL sanctioned way of rendering offscreen.
	 * Also, I want to try to decouple the image capture from the onscreen rendering. This is potentially useful
	 * for two reasons:
	 * 1) You may want to customize the image dimensions to best fit the situation (consider printing to a page to fit)
	 * 2) You may want to customize the scene for the target (consider special output for a printer, or removed data for a thumbnail)
	 * Unfortunately, I have hit two problems.
	 * 1) osg::Camera (which seems to be the way to access Framebuffer Objects in OSG) doesn't seem to capture if it is the root node.
	 * The workaround is to copy the camera attributes into another camera, and then add a second camera node into the scene.
	 * I'm hoping OSG will simplify this in the future.
	 * 2) I may have encountered a bug. Under some circumstances, the offscreen renderbuffer seems to get drawn into the onscreen view
	 * when using a DragImage for drag-and-drop. I reproduced a non-OSG example, but learned I missed two important FBO calls which trigger gl errors.
	 * So I'm wondering if OSG made the same mistake.
	 * But the problem doesn't seem critical. It just looks bad.
	 */
	//NSLog(@"Before camera glGetError: %s", gluErrorString(glGetError()));
	osg::Camera* root_camera = simpleViewer->getCamera();

	// I originally tried the clone() method and the copy construction, but it didn't work right,
	// so I manually copy the attributes.
	osg::Camera* the_camera = new osg::Camera;

	the_camera->setClearMask(root_camera->getClearMask());
	the_camera->setProjectionMatrix(root_camera->getProjectionMatrix());
	the_camera->setViewMatrix(root_camera->getViewMatrix());
	the_camera->setViewport(root_camera->getViewport());
	the_camera->setClearColor(
		osg::Vec4(
			clear_red,
			clear_green,
			clear_blue,
			clear_alpha
		)
	);

	// This must be ABSOLUTE_RF, and not a copy of the root camera because the transforms are additive.
	the_camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);

	// We need to insert the new (second) camera into the scene (below the root camera) and attach 
	// the scene data to the new camera.
	osg::ref_ptr<osg::Node> root_node = simpleViewer->getSceneView()->getSceneData();

	the_camera->addChild(root_node.get());
	// Don't call (bypass) simpleViewer's setSceneData, but the underlying SceneView's setSceneData.
	// Otherwise, the camera position gets reset to the home position.
	simpleViewer->getSceneView()->setSceneData(the_camera);

	// set the camera to render before the main camera.
	the_camera->setRenderOrder(osg::Camera::PRE_RENDER);

	// tell the camera to use OpenGL frame buffer object where supported.
	the_camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);


	// attach the image so its copied on each frame.
	the_camera->attach(osg::Camera::COLOR_BUFFER, osg_image.get());


	//NSLog(@"Before frame(), glGetError: %s", gluErrorString(glGetError()));


	// Render the scene
	simpleViewer->frame();

	// Not sure if I really need this (seems to work without it), and if so, not sure if I need flush or finish
	glFlush();
//	glFinish();

	//NSLog(@"After flush(), glGetError: %s", gluErrorString(glGetError()));



	// The image is upside-down to Cocoa, so invert it.
	osg_image.get()->flipVertical();

	// Clean up everything I changed
//	the_camera->detach(osg::Camera::COLOR_BUFFER);
//	the_camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER);
	// Don't call (bypass) simpleViewer's setSceneData, but the underlying SceneView's setSceneData.
	// Otherwise, the camera position gets reset to the home position.
	simpleViewer->getSceneView()->setSceneData(root_node.get());
	simpleViewer->getEventQueue()->windowResize(0, 0, original_size_in_window_coordinates.width, original_size_in_window_coordinates.height);


	// Ugh. Because of the bug I mentioned, I'm losing the picture in the display when I print.
	[self setNeedsDisplay:YES];
	//NSLog(@"at return, glGetError: %s", gluErrorString(glGetError()));
#endif
	return ns_image_rep;
}

// Convenience method
- (NSImage*)imageFromBitmapImageRep:(NSBitmapImageRep*)bitmap_image_rep
{
	if(nil == bitmap_image_rep)
	{
		return nil;
	}
	NSImage* image = [[[NSImage alloc] initWithSize:[bitmap_image_rep size]] autorelease];
	[image addRepresentation:bitmap_image_rep];
	// This doesn't seem to work as I want it to. The image only gets flipped when rendered in a regular view.
	// It doesn't flip for the printer view. I must actually invert the pixels.
//	[image setFlipped:YES];
	return image;
}




////////////////////////////////////////////////////////////////////////
/////////////////////////// End View and Draw Stuff ////////////////////
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/////////////////////////// For drag and drop //////////////////////////
////////////////////////////////////////////////////////////////////////
- (unsigned int) draggingEntered:(id <NSDraggingInfo>)the_sender
{
	if([the_sender draggingSource] != self)
	{
		NSPasteboard* paste_board = [the_sender draggingPasteboard];
		// I respond to filename types or URL types
		NSArray* supported_types = [NSArray arrayWithObjects:NSFilenamesPboardType, NSURLPboardType, nil];
		// If any of the supported types are being dragged in, activate the copy operation
		NSString* first_type = [paste_board availableTypeFromArray:supported_types];
		if(first_type != nil)
		{
			return NSDragOperationCopy;
		}
	}
	// Means we don't support this type
	return NSDragOperationNone;
}

// We're not using this method, but here it is as an example.
- (void) draggingExited:(id <NSDraggingInfo>)the_sender
{
}

- (BOOL) prepareForDragOperation:(id <NSDraggingInfo>)the_sender
{
	return YES;
}

- (BOOL) performDragOperation:(id <NSDraggingInfo>)the_sender
{
	NSPasteboard* paste_board = [the_sender draggingPasteboard];

 
    if([[paste_board types] containsObject:NSFilenamesPboardType])
	{
        NSArray* file_names = [paste_board propertyListForType:NSFilenamesPboardType];
//        int number_of_files = [file_names count];
		// Exercise for the reader: Try loading all files in the array
		NSString* single_file = [file_names objectAtIndex:0];
	    osg::ref_ptr<osg::Node> loaded_model = osgDB::readNodeFile([single_file UTF8String]);
		if(!loaded_model)
		{
			NSLog(@"File: %@ failed to load", single_file);
			return NO;
		}
//		simpleViewer->setSceneData(loaded_model.get());
		return YES;
    }
	else if([[paste_board types] containsObject:NSURLPboardType])
	{
		NSURL* file_url = [NSURL URLFromPasteboard:paste_board];
		// See if the URL is valid file path
		if(![file_url isFileURL])
		{
			NSLog(@"URL: %@ needs to be a file for readNodeFile()", file_url);
			return NO;
		}
		NSString* file_path = [file_url path];
	    osg::ref_ptr<osg::Node> loaded_model = osgDB::readNodeFile([file_path UTF8String]);
		if(!loaded_model)
		{
			NSLog(@"URL: %@ failed to load, %@", file_url, file_path);
			return NO;
		}
//		simpleViewer->setSceneData(loaded_model.get());
		return YES;
	}
    return NO;
}

// This method isn't really needed (I could move setNeedsDisplay up), but is here as an example
- (void) concludeDragOperation:(id <NSDraggingInfo>)the_sender
{
	[self setNeedsDisplay:YES];
}

////////////////////////////////////////////////////////////////////////
/////////////////////////// End of drag and drop (receiver) ////////////
////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// For drag and drop and copy/paste (source) ////////////////
//////////////////////////////////////////////////////////////////////////////////////
- (IBAction) copy:(id)sender
{
    NSString* type = NSTIFFPboardType;
    NSData* image_data = [self contentsAsDataOfType:type];
        
    if(image_data)
	{
        NSPasteboard* general_pboard = [NSPasteboard generalPasteboard];
        [general_pboard declareTypes:[NSArray arrayWithObjects:type, nil] owner: nil];
        [general_pboard setData:image_data forType:type];
    }
}

- (NSData*) dataWithTIFFOfContentView
{
	NSBitmapImageRep * image = [self renderOpenGLSceneToFramebuffer];
	NSData* data = nil;

	if(image != nil)
	{
		data = [image TIFFRepresentation];
	}
	return data;
}

/* Returns a data object containing the current contents of the receiving window */
- (NSData*) contentsAsDataOfType:(NSString *)pboardType
{
	NSData * data = nil;
	if ([pboardType isEqualToString: NSTIFFPboardType] == YES)
	{
		data = [self dataWithTIFFOfContentView];
	}
    return data;
}


- (void) startDragAndDropAsSource:(NSEvent*)the_event
{
	NSPasteboard* drag_paste_board;
	NSImage* the_image;
	NSSize the_size;
	NSPoint the_point;

	NSSize size_in_points = [self bounds].size;
	NSSize size_in_window_coordinates = [self convertSize:size_in_points toView:nil];

	// Create the image that will be dragged
	NSString * type = NSTIFFPboardType;

	// I want two images. One to be rendered for the target, and one as the drag-image.
	// I want the drag-image to be translucent.
	// I think this is where render GL_COLOR_ATTACHMENTn (COLOR_BUFFERn?) would be handy.
	// But my hardware only returns 1 for glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, maxbuffers);
	// So I won't bother and will just render twice.
	NSBitmapImageRep* bitmap_image_rep = [self renderOpenGLSceneToFramebufferAsFormat:GL_RGB viewWidth:size_in_window_coordinates.width viewHeight:size_in_window_coordinates.height];
	NSBitmapImageRep* bitmap_image_rep_transparent_copy = [self renderOpenGLSceneToFramebufferAsFormat:GL_RGBA viewWidth:size_in_window_coordinates.width viewHeight:size_in_window_coordinates.height];
//	NSBitmapImageRep* bitmap_image_rep = [self renderOpenGLSceneToFramebufferAsFormat:GL_RGBA viewWidth:size_in_window_coordinates.width viewHeight:size_in_window_coordinates.height clearColorRed:1.0f clearColorGreen:1.0f clearColorBlue:0.0f clearColorAlpha:0.4f];

//NSBitmapImageRep* bitmap_image_rep_transparent_copy = bitmap_image_rep;

	// 0x32 is an arbitrary number. Basically, I want something between 0 and 0xFF.
	Internal_SetAlpha(bitmap_image_rep_transparent_copy, 0x32);

	NSData* image_data = [bitmap_image_rep TIFFRepresentation];

    if(image_data)
	{
	
		drag_paste_board = [NSPasteboard pasteboardWithName:NSDragPboard];
		// is owner:self or nil? (Hillegass says self)
        [drag_paste_board declareTypes: [NSArray arrayWithObjects: type, nil] owner: self];
        [drag_paste_board setData:image_data forType: type];
		
		// create an image from the data
		the_image = [[NSImage alloc] initWithData:[bitmap_image_rep_transparent_copy TIFFRepresentation]];
		
		the_point = [self convertPoint:[the_event locationInWindow] fromView:nil];
		the_size = [the_image size];
		
		// shift the point to the center of the image
		the_point.x = the_point.x - the_size.width/2.0;
		the_point.y = the_point.y - the_size.height/2.0;

		// start drag
		[self dragImage:the_image
					 at:the_point
				 offset:NSMakeSize(0,0)
				  event:the_event
			 pasteboard:drag_paste_board
				 source:self
			  slideBack:YES];
			  
		[the_image release];
	}
	else
	{
		NSLog(@"Error, failed to create image data");
	}
	
}

//////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// For drag and drop and copy/paste (source) ////////////////
//////////////////////////////////////////////////////////////////////////////////////






////////////////////////////////////////////////////////////////////////
/////////////////////////// IBAction examples  /////////////////////////
////////////////////////////////////////////////////////////////////////

- (void) stopClearPaging
{
  theDatabasePager->setAcceptNewDatabaseRequests(false);
  theDatabasePager->clear();
  theDatabasePager->setDatabasePagerThreadPause(true);
}

- (void) clearPager
{
  theDatabasePager->clear();
}

- (void) startPaging
{
  theDatabasePager->setAcceptNewDatabaseRequests(true);
  theDatabasePager->setDatabasePagerThreadPause(false);
}


- (void) setSceneData:(osg::Node*)scene_data
{
   theSceneView->setSceneData(scene_data);
   if (theMatrixManipulator.valid())
   {     
      osgGA::GUIEventAdapter* dummyEvent = new  osgGA::GUIEventAdapter();
      dummyEvent->setEventType(osgGA::GUIEventAdapter::FRAME);
      theMatrixManipulator->home(*dummyEvent,theActionAdapter);
      theMatrixManipulator->setNode(scene_data);
   }
   osgGA::SetSceneViewVisitor ssvv(0,
                                   &theActionAdapter,
                                   theSceneView.get());
   theMatrixManipulator->accept(ssvv);
   theDatabasePager->registerPagedLODs(theSceneView->getSceneData()); 
   theSceneView->getCullVisitor()->setDatabaseRequestHandler(theDatabasePager.get());
   theSceneView->getUpdateVisitor()->setDatabaseRequestHandler(theDatabasePager.get());
   
   	[self setNeedsDisplay:YES];
}

- (void) setPlanet:(ossimPlanet*)the_planet
{
	thePlanet = the_planet;
}


- (void) getViewPositionLat:(double&)lat lon:(double&)lon height:(double&)height heading:(double&)heading pitch:(double&)pitch roll:(double&)roll
{
   theMatrixManipulator->getLatLonHgtHPR(lat, lon, height, heading, pitch, roll);
}



////////////////////////////////////////////////////////////////////////
/////////////////////////// Binding Stuff //////////////////////////////
////////////////////////////////////////////////////////////////////////


- (NSArray*) exposedBindings
{
	NSLog(@"In exposedBindings");
	return [NSArray arrayWithObjects:
		@"hudEnabled", 
		nil];
}

////////////////////////////////////////////////////////////////////////
/////////////////////////// End Binding Stuff //////////////////////////
////////////////////////////////////////////////////////////////////////


- (void) setHudEnabled:(BOOL)is_hud_enabled
{
	if(!thePlanet.valid())
	{
		return;
	}
	// Check to avoid infinite recursion if notifying controller
	if(thePlanet->getEnableHudFlag() == is_hud_enabled)
	{
		return;
	}
	thePlanet->setEnableHudFlag(is_hud_enabled);
	// Need to notify the controller something has changed (for bindings)
	Internal_NotifyControllerAboutNewValue([self infoForBinding:@"hudEnabled"], [NSNumber numberWithBool:is_hud_enabled]);
}

- (BOOL) isHudEnabled
{
	if(!thePlanet.valid())
	{
		return NO;
	}
	return (thePlanet->getEnableHudFlag());
}

@end
