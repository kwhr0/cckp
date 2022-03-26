@interface AppDelegate : NSObject <NSApplicationDelegate> {
	IBOutlet id prefPanel;
	IBOutlet id prefCpuClock;
	double _cpuClock;
}

- (IBAction)showPreference:(id)sender;
- (IBAction)prefSet:(id)sender;
- (IBAction)prefCancel:(id)sender;

@property (nonatomic, assign) double cpuClock;

@end
