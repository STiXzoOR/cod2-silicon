/* The view the Direct3D 9 on Metal device presents into (COD2_MAC_D3D9).
 *
 * libd3d9metal sizes its drawable to the layer's bounds times its contentsScale
 * and copies the back buffer into it; a back buffer of another size would be
 * resampled. This view keeps the two equal. Its Metal layer sits in the middle
 * of the game window with the back buffer's proportions, and its contentsScale
 * is whatever makes bounds times scale the back buffer's size in pixels. So the
 * game's picture reaches the screen as it was drawn: one to one when the mode
 * matches the window's pixels, scaled by the window server when it does not,
 * with black bars when the proportions differ. That is what the OpenGL client
 * shows too. */
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <dispatch/dispatch.h>

@interface Cod2D3D9View : NSView
@property(nonatomic, strong) CAMetalLayer *metalLayer;
@property(nonatomic) int renderWidth;
@property(nonatomic) int renderHeight;
- (void)fit;
@end

@implementation Cod2D3D9View
- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame])) {
        /* A layer set before wantsLayer makes the view host it, so the tree below is ours. */
        CALayer *root = [CALayer layer];
        root.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
        root.opaque = YES;
        self.layer = root;
        self.wantsLayer = YES;
        self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _metalLayer = [CAMetalLayer layer];
        _metalLayer.opaque = YES;
        [root addSublayer:_metalLayer];
    }
    return self;
}

/* Clicks and movement go to SDL's view underneath. */
- (NSView *)hitTest:(NSPoint)point
{
    (void)point;
    return nil;
}

- (BOOL)isOpaque
{
    return YES;
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    [self fit];
}

- (void)fit
{
    NSSize size = self.bounds.size;
    if (_renderWidth <= 0 || _renderHeight <= 0 || size.width <= 0 || size.height <= 0)
        return;
    double scale = MIN(size.width / _renderWidth, size.height / _renderHeight);
    double width = _renderWidth * scale, height = _renderHeight * scale;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _metalLayer.frame = CGRectMake((size.width - width) / 2, (size.height - height) / 2, width, height);
    _metalLayer.contentsScale = 1 / scale;
    [CATransaction commit];
}
@end

static Cod2D3D9View *gameView;

static void *Update(SDL_Window *window, int renderWidth, int renderHeight)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!window || !SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_COCOA)
        return NULL;
    NSView *content = info.info.cocoa.window.contentView;
    if (!content)
        return NULL;
    if (gameView && gameView.superview != content) {
        [gameView removeFromSuperview];
        gameView = nil;
    }
    if (!gameView) {
        gameView = [[Cod2D3D9View alloc] initWithFrame:content.bounds];
        [content addSubview:gameView];
    }
    gameView.renderWidth = renderWidth;
    gameView.renderHeight = renderHeight;
    gameView.frame = content.bounds;
    [gameView fit];
    return (__bridge void *)gameView.metalLayer;
}

/* The window's Metal layer, made on first use and fitted to a back buffer of this
 * size. The layer lives as long as the window does. */
void *MacD3D9View_Layer(SDL_Window *window, int renderWidth, int renderHeight)
{
    __block void *layer = NULL;
    if (NSThread.isMainThread)
        return Update(window, renderWidth, renderHeight);
    dispatch_sync(dispatch_get_main_queue(), ^{
        layer = Update(window, renderWidth, renderHeight);
    });
    return layer;
}
