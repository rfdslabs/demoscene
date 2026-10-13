// Pure-native Cocoa presentation of the CPU framebuffer. No SDL, no Metal.
// Each frame the app fills a W*H ARGB buffer; we wrap it in a CGImage and hand
// it to a CALayer with nearest-neighbour scaling for crisp chunky pixels.
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include "platform.h"
#include "surface.h"   // for ti::W, ti::H

namespace plat {
  // Master-clock state, updated by the audio callback (mac_audio.mm). Kept as
  // plain globals so this file needs no knowledge of the Synth type.
  unsigned long long g_audio_samples = 0;
  double             g_audio_sr = 0.0;

  static double g_wall_start = 0.0;

  double audio_time() {
    if (g_audio_sr > 0.0) return (double)g_audio_samples / g_audio_sr;
    return CACurrentMediaTime() - g_wall_start;
  }
}

// ----- The view: owns the ARGB buffer and a reusable bitmap context. ----------
@interface TIView : NSView {
@public
  plat::App* app;
  unsigned int* argb;
  CGContextRef ctx;
}
- (void)tick;
@end

@implementation TIView

- (instancetype)initWithApp:(plat::App*)a {
  self = [super initWithFrame:NSMakeRect(0, 0, ti::W, ti::H)];
  if (!self) return nil;
  app = a;
  argb = (unsigned int*)calloc((size_t)ti::W * ti::H, sizeof(unsigned int));
  CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
  // 0xAARRGGBB words, little-endian in memory, alpha ignored -> ARGB.
  ctx = CGBitmapContextCreate(argb, ti::W, ti::H, 8, ti::W * 4, cs,
      (CGBitmapInfo)((uint32_t)kCGImageAlphaNoneSkipFirst | (uint32_t)kCGBitmapByteOrder32Little));
  CGColorSpaceRelease(cs);

  self.wantsLayer = YES;
  self.layer.magnificationFilter = kCAFilterNearest;
  self.layer.contentsGravity = kCAGravityResize;
  return self;
}

- (void)dealloc {
  if (ctx) CGContextRelease(ctx);
  if (argb) free(argb);
}

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)isFlipped { return YES; }

- (void)tick {
  app->frame(plat::audio_time(), argb);
  CGImageRef img = CGBitmapContextCreateImage(ctx);
  self.layer.contents = (__bridge id)img;
  CGImageRelease(img);
}

- (void)keyDown:(NSEvent*)e {
  if (e.keyCode == plat::KEY_ESC) { [NSApp terminate:nil]; return; }
  app->key((int)e.keyCode, true);
}
- (void)keyUp:(NSEvent*)e { app->key((int)e.keyCode, false); }

- (void)forwardMouse:(NSEvent*)e down:(BOOL)down {
  NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
  CGFloat bw = self.bounds.size.width, bh = self.bounds.size.height;
  if (bw <= 0 || bh <= 0) return;
  int lx = (int)(p.x / bw * ti::W);
  int ly = (int)(p.y / bh * ti::H);   // isFlipped -> already top-down
  if (lx < 0) lx = 0; if (lx >= ti::W) lx = ti::W - 1;
  if (ly < 0) ly = 0; if (ly >= ti::H) ly = ti::H - 1;
  app->mouse(lx, ly, down);
}
- (void)mouseDown:(NSEvent*)e { [self forwardMouse:e down:YES]; }
- (void)mouseUp:(NSEvent*)e   { [self forwardMouse:e down:NO]; }

@end

// ----- App delegate drives a 60Hz timer on the main runloop. ------------------
@interface TIDelegate : NSObject <NSApplicationDelegate> {
@public
  NSWindow* window;
  TIView* view;
  NSTimer* timer;
}
@end

@implementation TIDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)n {
  [NSApp activateIgnoringOtherApps:YES];
  [window makeKeyAndOrderFront:nil];
  [window makeFirstResponder:view];
  timer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0
                                           target:view
                                         selector:@selector(tick)
                                         userInfo:nil
                                          repeats:YES];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)a { return YES; }
@end

namespace plat {

  int run(App& app, const char* title, int scale) {
    @autoreleasepool {
      g_wall_start = CACurrentMediaTime();
      [NSApplication sharedApplication];
      [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

      int w = ti::W * scale, h = ti::H * scale;
      NSRect frame = NSMakeRect(0, 0, w, h);
      NSWindow* win = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
      [win setTitle:[NSString stringWithUTF8String:title ? title : "TinyIntro"]];
      [win center];

      TIView* view = [[TIView alloc] initWithApp:&app];
      [view setFrame:frame];
      [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
      [win setContentView:view];

      TIDelegate* del = [[TIDelegate alloc] init];
      del->window = win;
      del->view = view;
      [NSApp setDelegate:del];
      [NSApp run];
    }
    return 0;
  }
}
