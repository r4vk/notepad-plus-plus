#import <Cocoa/Cocoa.h>

static void ConfigureMenu(NSApplication *app)
{
    NSMenu *menubar = [[NSMenu alloc] init];
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    [menubar addItem:appMenuItem];
    [app setMainMenu:menubar];

    NSMenu *appMenu = [[NSMenu alloc] init];
    NSString *quitTitle = [NSString stringWithFormat:@"Quit %@", [[NSProcessInfo processInfo] processName]];
    NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle
                                                          action:@selector(terminate:)
                                                   keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
}

static NSWindow *CreateMainWindow(CGSize size)
{
    NSRect frame = NSMakeRect(0.0, 0.0, size.width, size.height);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                       NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;

    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:style
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window center];
    [window setTitle:@"Notepad++ macOS Preview"];
    [window setReleasedWhenClosed:NO];

    return window;
}

static void InstallContent(NSWindow *window)
{
    NSView *contentView = [window contentView];

    NSTextField *headline = [[NSTextField alloc] initWithFrame:NSMakeRect(24, NSMaxY([contentView bounds]) - 72, NSWidth([contentView bounds]) - 48, 32)];
    [headline setStringValue:@"Liquid Glass UI preview shell"];
    [headline setFont:[NSFont boldSystemFontOfSize:24.0]];
    [headline setBezeled:NO];
    [headline setDrawsBackground:NO];
    [headline setEditable:NO];
    [headline setSelectable:NO];
    [headline setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
    [contentView addSubview:headline];

    NSTextField *message = [[NSTextField alloc] initWithFrame:NSMakeRect(24, NSMaxY([contentView bounds]) - 122, NSWidth([contentView bounds]) - 48, 56)];
    [message setStringValue:@"This native Cocoa shell validates the CI packaging path for the upcoming Qt-based Notepad++ experience on macOS."];
    [message setFont:[NSFont systemFontOfSize:13.0]];
    [message setBezeled:NO];
    [message setDrawsBackground:NO];
    [message setEditable:NO];
    [message setSelectable:NO];
    [message setLineBreakMode:NSLineBreakByWordWrapping];
    [message setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
    [contentView addSubview:message];

    NSTextField *status = [[NSTextField alloc] initWithFrame:NSMakeRect(24, 32, NSWidth([contentView bounds]) - 48, 20)];
    [status setStringValue:@"Status: scaffolding for macOS bundle automation completed via GitHub Actions."];
    [status setFont:[NSFont systemFontOfSize:12.0]];
    [status setBezeled:NO];
    [status setDrawsBackground:NO];
    [status setEditable:NO];
    [status setSelectable:NO];
    [status setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
    [contentView addSubview:status];
}

int main(int argc, const char *argv[])
{
    @autoreleasepool
    {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        ConfigureMenu(app);

        NSWindow *window = CreateMainWindow(CGSizeMake(880.0, 580.0));
        InstallContent(window);

        [window makeKeyAndOrderFront:nil];
        [app activateIgnoringOtherApps:YES];
        [app run];
    }
    return 0;
}
