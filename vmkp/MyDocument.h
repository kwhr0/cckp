#import "PC.h"
#import <string>
#import <vector>

#define TEX_X		640
#define TEX_Y		480

@class MyViewGL;
@class AppDelegate;

@interface MyDocument : NSDocument

@property (nonatomic, assign) IBOutlet MyViewGL *view;
@property (nonatomic, assign) int keydata, keymod;
@property (nonatomic, assign) AppDelegate *appDelegate;

- (void)key:(int)code isUp:(BOOL)f;
- (BOOL)run:(int)sampleN buf:(float *)buf;
- (uint8_t *)vram;
- (void)becomeActive;
- (void)vsync;

@end
