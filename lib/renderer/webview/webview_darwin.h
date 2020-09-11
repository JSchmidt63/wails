/*
 * MIT License
 *
 * Copyright (c) 2017 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef WEBVIEW_H
#define WEBVIEW_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef WEBVIEW_STATIC
#define WEBVIEW_API static
#else
#define WEBVIEW_API extern
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#import <objc/runtime.h>

struct webview_priv
{
  NSAutoreleasePool *pool;
  NSWindow *window;
  WebView *webview;
  id delegate;
  int should_exit;
};

  struct webview;

  typedef void (*webview_external_invoke_cb_t)(struct webview *w,
                                               const char *arg);

  struct webview
  {
    const char *url;
    const char *title;
    int width;
    int height;
    int resizable;
    int transparentTitlebar;
    int debug;
    webview_external_invoke_cb_t external_invoke_cb;
    struct webview_priv priv;
    void *userdata;
  };

  enum webview_dialog_type
  {
    WEBVIEW_DIALOG_TYPE_OPEN = 0,
    WEBVIEW_DIALOG_TYPE_SAVE = 1,
    WEBVIEW_DIALOG_TYPE_ALERT = 2
  };

#define WEBVIEW_DIALOG_FLAG_FILE (0 << 0)
#define WEBVIEW_DIALOG_FLAG_DIRECTORY (1 << 0)

#define WEBVIEW_DIALOG_FLAG_INFO (1 << 1)
#define WEBVIEW_DIALOG_FLAG_WARNING (2 << 1)
#define WEBVIEW_DIALOG_FLAG_ERROR (3 << 1)
#define WEBVIEW_DIALOG_FLAG_ALERT_MASK (3 << 1)

typedef void (*webview_dispatch_fn)(struct webview *w, void *arg);

struct webview_dispatch_arg
{
  webview_dispatch_fn fn;
  struct webview *w;
  void *arg;
};

#define DEFAULT_URL                                                            \
"data:text/"                                                                 \
"html,%3C%21DOCTYPE%20html%3E%0A%3Chtml%20lang=%22en%22%3E%0A%3Chead%3E%"    \
"3Cmeta%20charset=%22utf-8%22%3E%3Cmeta%20http-equiv=%22IE=edge%22%"         \
"20content=%22IE=edge%22%3E%3C%2Fhead%3E%0A%3Cbody%3E%3Cdiv%20id=%22app%22%" \
"3E%3C%2Fdiv%3E%3Cscript%20type=%22text%2Fjavascript%22%3E%3C%2Fscript%3E%"  \
"3C%2Fbody%3E%0A%3C%2Fhtml%3E"

#define CSS_INJECT_FUNCTION                                           \
"(function(e){var "                                                 \
"t=document.createElement('style'),d=document.head||document."      \
"getElementsByTagName('head')[0];t.setAttribute('type','text/"      \
"css'),t.styleSheet?t.styleSheet.cssText=e:t.appendChild(document." \
"createTextNode(e)),d.appendChild(t)})"

static const char *webview_check_url(const char *url)
{
  if (url == NULL || strlen(url) == 0)
  {
    return DEFAULT_URL;
  }
  return url;
}

WEBVIEW_API int webview(const char *title, const char *url, int width,
                        int height, int resizable);

WEBVIEW_API int webview_init(struct webview *w);
WEBVIEW_API int webview_loop(struct webview *w, int blocking);
WEBVIEW_API int webview_eval(struct webview *w, const char *js);
WEBVIEW_API int webview_inject_css(struct webview *w, const char *css);
WEBVIEW_API void webview_set_title(struct webview *w, const char *title);
WEBVIEW_API void webview_set_fullscreen(struct webview *w, int fullscreen);
WEBVIEW_API void webview_set_color(struct webview *w, uint8_t r, uint8_t g,
                                    uint8_t b, uint8_t a);
WEBVIEW_API void webview_dialog(struct webview *w,
                                enum webview_dialog_type dlgtype, int flags,
                                const char *title, const char *arg,
                                char *result, size_t resultsz, char *filter);
WEBVIEW_API void webview_dispatch(struct webview *w, webview_dispatch_fn fn,
                                  void *arg);
WEBVIEW_API void webview_terminate(struct webview *w);
WEBVIEW_API void webview_exit(struct webview *w);
WEBVIEW_API void webview_debug(const char *format, ...);
WEBVIEW_API void webview_print_log(const char *s);

WEBVIEW_API int webview(const char *title, const char *url, int width,
                        int height, int resizable)
{
  struct webview webview;
  memset(&webview, 0, sizeof(webview));
  webview.title = title;
  webview.url = url;
  webview.width = width;
  webview.height = height;
  webview.resizable = resizable;
  int r = webview_init(&webview);
  if (r != 0)
  {
    return r;
  }
  while (webview_loop(&webview, 1) == 0)
  {
  }
  webview_exit(&webview);
  return 0;
}

WEBVIEW_API void webview_debug(const char *format, ...)
{
  char buf[4096];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  webview_print_log(buf);
  va_end(ap);
}

static int webview_js_encode(const char *s, char *esc, size_t n)
{
  int r = 1; /* At least one byte for trailing zero */
  for (; *s; s++)
  {
    const unsigned char c = *s;
    if (c >= 0x20 && c < 0x80 && strchr("<>\\'\"", c) == NULL)
    {
      if (n > 0)
      {
        *esc++ = c;
        n--;
      }
      r++;
    }
    else
    {
      if (n > 0)
      {
        snprintf(esc, n, "\\x%02x", (int)c);
        esc += 4;
        n -= 4;
      }
      r += 4;
    }
  }
  return r;
}

WEBVIEW_API int webview_inject_css(struct webview *w, const char *css)
{
  int n = webview_js_encode(css, NULL, 0);
  char *esc = (char *)calloc(1, sizeof(CSS_INJECT_FUNCTION) + n + 4);
  if (esc == NULL)
  {
    return -1;
  }
  char *js = (char *)calloc(1, n);
  webview_js_encode(css, js, n);
  snprintf(esc, sizeof(CSS_INJECT_FUNCTION) + n + 4, "%s(\"%s\")",
            CSS_INJECT_FUNCTION, js);
  int r = webview_eval(w, esc);
  free(js);
  free(esc);
  return r;
}

#if (!defined MAC_OS_X_VERSION_10_12) || \
  MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
#define NSAlertStyleWarning NSWarningAlertStyle
#define NSAlertStyleCritical NSCriticalAlertStyle
#define NSWindowStyleMaskResizable NSResizableWindowMask
#define NSWindowStyleMaskMiniaturizable NSMiniaturizableWindowMask
#define NSWindowStyleMaskTitled NSTitledWindowMask
#define NSWindowStyleMaskClosable NSClosableWindowMask
#define NSWindowStyleMaskFullScreen NSFullScreenWindowMask
#define NSEventMaskAny NSAnyEventMask
#define NSEventModifierFlagCommand NSCommandKeyMask
#define NSEventModifierFlagOption NSAlternateKeyMask
#define NSAlertStyleInformational NSInformationalAlertStyle
#endif /* MAC_OS_X_VERSION_10_12 */
#if (!defined MAC_OS_X_VERSION_10_13) || \
  MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_13
#define NSModalResponseOK NSFileHandlingPanelOKButton
#endif /* MAC_OS_X_VERSION_10_12, MAC_OS_X_VERSION_10_13 */
static void webview_window_will_close(id self, SEL cmd, id notification)
{
  struct webview *w =
      (struct webview *)objc_getAssociatedObject(self, "webview");
  webview_terminate(w);
}

static BOOL webview_is_selector_excluded_from_web_script(id self, SEL cmd,
                                                          SEL selector)
{
  return selector != @selector(invoke:);
}

static NSString *webview_webscript_name_for_selector(id self, SEL cmd,
                                                      SEL selector)
{
  return selector == @selector(invoke:) ? @"invoke" : nil;
}

static void webview_did_clear_window_object(id self, SEL cmd, id webview,
                                            id script, id frame)
{
  [script setValue:self forKey:@"external"];
}

static void webview_run_input_open_panel(id self, SEL cmd, id webview,
                                          id listener, BOOL allowMultiple)
{
  char filename[256] = "";
  struct webview *w =
      (struct webview *)objc_getAssociatedObject(self, "webview");

  webview_dialog(w, WEBVIEW_DIALOG_TYPE_OPEN, WEBVIEW_DIALOG_FLAG_FILE, "", "",
                  filename, 255, "");
  filename[255] = '\0';
  if (strlen(filename) > 0)
  {
    [listener chooseFilename:[NSString stringWithUTF8String:filename]];
  }
  else
  {
    [listener cancel];
  }
}

static void webview_external_invoke(id self, SEL cmd, id arg)
{
  struct webview *w =
      (struct webview *)objc_getAssociatedObject(self, "webview");
  if (w == NULL || w->external_invoke_cb == NULL)
  {
    return;
  }
  if ([arg isKindOfClass:[NSString class]] == NO)
  {
    return;
  }
  w->external_invoke_cb(w, [(NSString *)(arg) UTF8String]);
}

WEBVIEW_API int webview_init(struct webview *w)
{
  w->priv.pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];

  Class webViewDelegateClass =
      objc_allocateClassPair([NSObject class], "WebViewDelegate", 0);
  class_addMethod(webViewDelegateClass, sel_registerName("windowWillClose:"),
                  (IMP)webview_window_will_close, "v@:@");
  class_addMethod(object_getClass(webViewDelegateClass),
                  sel_registerName("isSelectorExcludedFromWebScript:"),
                  (IMP)webview_is_selector_excluded_from_web_script, "c@::");
  class_addMethod(object_getClass(webViewDelegateClass),
                  sel_registerName("webScriptNameForSelector:"),
                  (IMP)webview_webscript_name_for_selector, "c@::");
  class_addMethod(webViewDelegateClass,
                  sel_registerName("webView:didClearWindowObject:forFrame:"),
                  (IMP)webview_did_clear_window_object, "v@:@@@");
  class_addMethod(
      webViewDelegateClass,
      sel_registerName("webView:runOpenPanelForFileButtonWithResultListener:"
                        "allowMultipleFiles:"),
      (IMP)webview_run_input_open_panel, "v@:@@c");
  class_addMethod(webViewDelegateClass, sel_registerName("invoke:"),
                  (IMP)webview_external_invoke, "v@:@");
  objc_registerClassPair(webViewDelegateClass);

  w->priv.delegate = [[webViewDelegateClass alloc] init];
  objc_setAssociatedObject(w->priv.delegate, "webview", (id)(w),
                            OBJC_ASSOCIATION_ASSIGN);

  NSRect r = NSMakeRect(0, 0, w->width, w->height);
  NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                      NSWindowStyleMaskMiniaturizable;
  if (w->resizable)
  {
    style = style | NSWindowStyleMaskResizable;
    // style = style | NSTexturedBackgroundWindowMask;
    // style = style | NSUnifiedTitleAndToolbarWindowMask;
  }

  // Transparent title bar
  // if (w->transparentTitlebar) {
  //   style = style | NSFullSizeContentViewWindowMask | NSUnifiedTitleAndToolbarWindowMask | NSTexturedBackgroundWindowMask;
  // }

  w->priv.window = [[NSWindow alloc] initWithContentRect:r
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
  [w->priv.window autorelease];

  // Title
  NSString *nsTitle = [NSString stringWithUTF8String:w->title];
  [w->priv.window setTitle:nsTitle];

  [w->priv.window setDelegate:w->priv.delegate];
  [w->priv.window center];

  //  NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"wat"];
  //   toolbar.showsBaselineSeparator = NO;
  //   [w->priv.window setToolbar:toolbar];

  // if (w->transparentTitlebar) {

  //   // Configure window look with hidden toolbar
  //   [w->priv.window setTitlebarAppearsTransparent:YES];
  //   [w->priv.window setTitleVisibility:NSWindowTitleHidden];
  //   // w->priv.window.isMovableByWindowBackground = true;
  // }

  [[NSUserDefaults standardUserDefaults] setBool:!!w->debug
                                          forKey:@"WebKitDeveloperExtras"];
  [[NSUserDefaults standardUserDefaults] synchronize];
  w->priv.webview =
      [[WebView alloc] initWithFrame:r
                            frameName:@"WebView"
                            groupName:nil];
  NSURL *nsURL = [NSURL
      URLWithString:[NSString stringWithUTF8String:webview_check_url(w->url)]];
  [[w->priv.webview mainFrame] loadRequest:[NSURLRequest requestWithURL:nsURL]];

  [w->priv.webview setAutoresizesSubviews:YES];
  [w->priv.webview
      setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  w->priv.webview.frameLoadDelegate = w->priv.delegate;
  w->priv.webview.UIDelegate = w->priv.delegate;
  [[w->priv.window contentView] addSubview:w->priv.webview];
  [w->priv.window orderFrontRegardless];

  // Disable scrolling - make this configurable
  // [[[w->priv.webview mainFrame] frameView] setAllowsScrolling:NO];

  // ----> Enables WebGL but won't pass the app store guidelines
  //
  // WebPreferences *p = [w->priv.webview preferences];
  // if ([p respondsToSelector:@selector(setWebGLEnabled:)]) {
  //     [p setWebGLEnabled:YES];
  // }

  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp finishLaunching];
  [NSApp activateIgnoringOtherApps:YES];

  NSMenu *menubar = [[[NSMenu alloc] initWithTitle:@""] autorelease];

  NSMenuItem *appMenuItem =
      [[[NSMenuItem alloc] initWithTitle:@"wails app" action:NULL keyEquivalent:@""]
          autorelease];
  NSMenu *appMenu = [[[NSMenu alloc] initWithTitle:@"wails app"] autorelease];
  [appMenuItem setSubmenu:appMenu];
  [menubar addItem:appMenuItem];

  NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:@"Hide"
                                                  action:@selector(hide:)
                                          keyEquivalent:@"h"] autorelease];
  [appMenu addItem:item];
  item = [[[NSMenuItem alloc] initWithTitle:@"Hide Others"
                                      action:@selector(hideOtherApplications:)
                              keyEquivalent:@"h"] autorelease];
  [item setKeyEquivalentModifierMask:(NSEventModifierFlagOption |
                                      NSEventModifierFlagCommand)];
  [appMenu addItem:item];
  item = [[[NSMenuItem alloc] initWithTitle:@"Show All"
                                      action:@selector(unhideAllApplications:)
                              keyEquivalent:@""] autorelease];
  [appMenu addItem:item];
  [appMenu addItem:[NSMenuItem separatorItem]];

  NSMenuItem *editMenuItem =
      [[[NSMenuItem alloc] initWithTitle:@"Edit" action:NULL keyEquivalent:@""]
          autorelease];
  NSMenu *editMenu = [[[NSMenu alloc] initWithTitle:@"Edit"] autorelease];
  [editMenuItem setSubmenu:editMenu];
  [menubar addItem:editMenuItem];

  item = [[[NSMenuItem alloc] initWithTitle:@"Cut"
                                      action:@selector(cut:)
                              keyEquivalent:@"x"] autorelease];
  [editMenu addItem:item];

  item = [[[NSMenuItem alloc] initWithTitle:@"Copy"
                                      action:@selector(copy:)
                              keyEquivalent:@"c"] autorelease];
  [editMenu addItem:item];

  item = [[[NSMenuItem alloc] initWithTitle:@"Paste"
                                      action:@selector(paste:)
                              keyEquivalent:@"v"] autorelease];
  [editMenu addItem:item];

  item = [[[NSMenuItem alloc] initWithTitle:@"Select All"
                                      action:@selector(selectAll:)
                              keyEquivalent:@"a"] autorelease];
  [editMenu addItem:item];

  [appMenu addItem:[NSMenuItem separatorItem]];

  item = [[[NSMenuItem alloc] initWithTitle:@"Quit"
                                      action:@selector(terminate:)
                              keyEquivalent:@"q"] autorelease];
  [appMenu addItem:item];

  [NSApp setMainMenu:menubar];

  w->priv.should_exit = 0;
  return 0;
}

WEBVIEW_API int webview_loop(struct webview *w, int blocking)
{
  NSDate *until = (blocking ? [NSDate distantFuture] : [NSDate distantPast]);
  NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                      untilDate:until
                                          inMode:NSDefaultRunLoopMode
                                        dequeue:YES];
  if (event)
  {
    [NSApp sendEvent:event];
  }
  return w->priv.should_exit;
}

WEBVIEW_API int webview_eval(struct webview *w, const char *js)
{
  NSString *nsJS = [NSString stringWithUTF8String:js];
  [[w->priv.webview windowScriptObject] evaluateWebScript:nsJS];
  return 0;
}

WEBVIEW_API void webview_set_title(struct webview *w, const char *title)
{
  NSString *nsTitle = [NSString stringWithUTF8String:title];
  [w->priv.window setTitle:nsTitle];
}

WEBVIEW_API void webview_set_fullscreen(struct webview *w, int fullscreen)
{
  int b = ((([w->priv.window styleMask] & NSWindowStyleMaskFullScreen) ==
            NSWindowStyleMaskFullScreen)
                ? 1
                : 0);
  if (b != fullscreen)
  {
    [w->priv.window toggleFullScreen:nil];
  }
}

WEBVIEW_API void webview_set_color(struct webview *w, uint8_t r, uint8_t g,
                                    uint8_t b, uint8_t a)
{
  [w->priv.window setBackgroundColor:[NSColor colorWithRed:(CGFloat)r / 255.0
                                                      green:(CGFloat)g / 255.0
                                                      blue:(CGFloat)b / 255.0
                                                      alpha:(CGFloat)a / 255.0]];
  if (0.5 >= ((r / 255.0 * 299.0) + (g / 255.0 * 587.0) + (b / 255.0 * 114.0)) /
                  1000.0)
  {
    [w->priv.window
        setAppearance:[NSAppearance
                          appearanceNamed:NSAppearanceNameVibrantDark]];
  }
  else
  {
    [w->priv.window
        setAppearance:[NSAppearance
                          appearanceNamed:NSAppearanceNameVibrantLight]];
  }
  [w->priv.window setOpaque:NO];
  [w->priv.window setTitlebarAppearsTransparent:YES];
  [w->priv.webview setDrawsBackground:NO];
}

WEBVIEW_API void webview_dialog(struct webview *w,
                                enum webview_dialog_type dlgtype, int flags,
                                const char *title, const char *arg,
                                char *result, size_t resultsz, char *filter)
{
  if (dlgtype == WEBVIEW_DIALOG_TYPE_OPEN ||
      dlgtype == WEBVIEW_DIALOG_TYPE_SAVE)
  {
    NSSavePanel *panel;
    NSString *filter_str = [NSString stringWithUTF8String:filter];
    filter_str = [filter_str stringByReplacingOccurrencesOfString:@"*."
                                    withString:@""];
    NSArray *fileTypes = [filter_str componentsSeparatedByString:@","];
    if (dlgtype == WEBVIEW_DIALOG_TYPE_OPEN)
    {
      NSOpenPanel *openPanel = [NSOpenPanel openPanel];
      if (flags & WEBVIEW_DIALOG_FLAG_DIRECTORY)
      {
        [openPanel setCanChooseFiles:NO];
        [openPanel setCanChooseDirectories:YES];
      }
      else
      {
        [openPanel setCanChooseFiles:YES];
        [openPanel setCanChooseDirectories:NO];
        if(filter[0] != NULL)
        {
          [openPanel setAllowedFileTypes:fileTypes];
        }
      }
      [openPanel setResolvesAliases:NO];
      [openPanel setAllowsMultipleSelection:NO];
      panel = openPanel;
    }
    else
    {
      panel = [NSSavePanel savePanel];
    }
    [panel setCanCreateDirectories:YES];
    [panel setShowsHiddenFiles:YES];
    [panel setExtensionHidden:NO];
    [panel setCanSelectHiddenExtension:NO];
    if(filter[0] != NULL)
    {
      [panel setAllowedFileTypes:fileTypes];
    }
    [panel setTreatsFilePackagesAsDirectories:YES];
    [panel beginSheetModalForWindow:w->priv.window
                  completionHandler:^(NSInteger result) {
                    [NSApp stopModalWithCode:result];
                  }];
    if ([NSApp runModalForWindow:panel] == NSModalResponseOK)
    {
      const char *filename = [[[panel URL] path] UTF8String];
      strlcpy(result, filename, resultsz);
    }
  }
  else if (dlgtype == WEBVIEW_DIALOG_TYPE_ALERT)
  {
    NSAlert *a = [NSAlert new];
    switch (flags & WEBVIEW_DIALOG_FLAG_ALERT_MASK)
    {
    case WEBVIEW_DIALOG_FLAG_INFO:
      [a setAlertStyle:NSAlertStyleInformational];
      break;
    case WEBVIEW_DIALOG_FLAG_WARNING:
      NSLog(@"warning");
      [a setAlertStyle:NSAlertStyleWarning];
      break;
    case WEBVIEW_DIALOG_FLAG_ERROR:
      NSLog(@"error");
      [a setAlertStyle:NSAlertStyleCritical];
      break;
    }
    [a setShowsHelp:NO];
    [a setShowsSuppressionButton:NO];
    [a setMessageText:[NSString stringWithUTF8String:title]];
    [a setInformativeText:[NSString stringWithUTF8String:arg]];
    [a addButtonWithTitle:@"OK"];
    [a runModal];
    [a release];
  }
}

static void webview_dispatch_cb(void *arg)
{
  struct webview_dispatch_arg *context = (struct webview_dispatch_arg *)arg;
  (context->fn)(context->w, context->arg);
  free(context);
}

WEBVIEW_API void webview_dispatch(struct webview *w, webview_dispatch_fn fn,
                                  void *arg)
{
  struct webview_dispatch_arg *context = (struct webview_dispatch_arg *)malloc(
      sizeof(struct webview_dispatch_arg));
  context->w = w;
  context->arg = arg;
  context->fn = fn;
  dispatch_async_f(dispatch_get_main_queue(), context, webview_dispatch_cb);
}

WEBVIEW_API void webview_terminate(struct webview *w)
{
  w->priv.should_exit = 1;
}
WEBVIEW_API void webview_exit(struct webview *w) { [NSApp terminate:NSApp]; }
  WEBVIEW_API void webview_print_log(const char *s) { NSLog(@"%s", s); }

#ifdef __cplusplus
}
#endif

#endif /* WEBVIEW_H */