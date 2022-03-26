#import "AppDelegate.h"
#import "Audio.h"
#import "MyDocument.h"

static void audioCallback(float *buf, int n) {
	BOOL f = FALSE;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSArray *docs = [[NSDocumentController sharedDocumentController] documents];
	for (int i = 0; i < docs.count; i++) {
		id doc = [docs objectAtIndex:i];
		if ([doc isKindOfClass:[MyDocument class]]) f |= [(MyDocument *)doc run:n buf:buf];
	}
	[pool drain];
	if (!f) for (int i = 0; i < n << 1; i++) buf[i] = 0.f;
}

@implementation AppDelegate

- (void)normalizePref {
	if (_cpuClock <= 0 || _cpuClock > 400) _cpuClock = 100;
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification {
	NSUserDefaults *def = [NSUserDefaults standardUserDefaults];
	_cpuClock = [def doubleForKey:@"CPU_CLOCK"];
	[self normalizePref];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	AudioSetup(&audioCallback);
	AudioStart();
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
	NSUserDefaults *def = [NSUserDefaults standardUserDefaults];
	[def setObject:[NSNumber numberWithDouble:_cpuClock] forKey:@"CPU_CLOCK"];
	[def synchronize];
}

- (void)applicationWillBecomeActive:(NSNotification *)notification {
	NSArray *docs = [[NSDocumentController sharedDocumentController] documents];
	if (docs.count == 1 && [[docs objectAtIndex:0] isKindOfClass:[MyDocument class]]) {
		[(MyDocument *)[docs objectAtIndex:0] becomeActive];
	}
}

- (IBAction)showPreference:(id)sender {
	[prefCpuClock setDoubleValue:_cpuClock];
	[prefPanel makeKeyAndOrderFront:nil];
}

- (IBAction)prefSet:(id)sender {
	_cpuClock = [prefCpuClock doubleValue];
	[prefPanel close];
	[self normalizePref];
}

- (IBAction)prefCancel:(id)sender {
	[prefPanel close];
}

@end
