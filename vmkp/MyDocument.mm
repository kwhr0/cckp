#import "MyDocument.h"
#import "AppDelegate.h"
#import <sys/stat.h>

#define MEM_AMOUNT	0x200000
#define VRAM		0x140000		// amount: 0x96000

enum { CLOSING_WAIT = 1, CLOSING_READY };

struct Sym {
	Sym(int _adr, const char *_s = "") : adr(_adr), n(0), s(_s) {}
	int adr, n;
	std::string s;
};

static int getHex(char *&p, int n) {
	int r = 0;
	do {
		int c = *p++ - '0';
		if (c > 10) c -= 'A' - '0' - 10;
		if (c >= 0 && c < 16) r = r << 4 | c;
	} while (--n > 0);
	return r;
}

static CVReturn MyDisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp *now, const CVTimeStamp *outputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *context) {
	[(MyDocument *)context vsync];
	return kCVReturnSuccess;
}

static bool cmp_adr(const Sym &a, const Sym &b) { return a.adr < b.adr; }
static bool cmp_n(const Sym &a, const Sym &b) { return a.n > b.n; }

@interface MyDocument () {
	uint8_t m[MEM_AMOUNT];
	uint8_t lastVRAM[2 * TEX_X * TEX_Y];
	PC pc;
	volatile int closing;
	volatile BOOL saving, ready;
	MyViewGL *_view;
	int _keydata, _keymod;
	CVDisplayLinkRef displayLink;
	std::vector<Sym> sym;
	struct timespec mtimespec;
}
@end

@implementation MyDocument

- (NSString *)windowNibName { return @"MyDocument"; }

- (id)init {
	[super init];
	if (self) {
		_appDelegate = [[NSApplication sharedApplication] delegate];
		CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
		CVDisplayLinkSetOutputCallback(displayLink, MyDisplayLinkCallback, self);
		CVDisplayLinkStart(displayLink);
	}
	return self;
}

- (void)close {
	for (closing = CLOSING_WAIT; closing == CLOSING_WAIT;)
		;
	CVDisplayLinkStop(displayLink);
	CVDisplayLinkRelease(displayLink);
	displayLink = nil;
	[self dumpProfile];
	[super close];
}

- (void)dumpProfile {
	if (!sym.empty()) {
		std::vector<Sym>::iterator i;
		std::sort(sym.begin(), sym.end(), cmp_n);
		int t = 0;
		for (i = sym.begin(); i != sym.end() && i->n; i++) t += i->n;
		printf("------\n");
		for (i = sym.begin(); i != sym.end() && double(i->n) / t >= 1e-3; i++)
			printf("%4.1f%% %s", 100. * i->n / t, i->s.c_str());
		sym.clear();
	}
}

- (void)process:(int)clockN {
	@synchronized (self) {
		pc.INT(1);
		pc.Execute(clockN);
		if (!sym.empty()) {
			std::vector<Sym>::iterator it = upper_bound(sym.begin(), sym.end(), Sym(pc.GetPC()), cmp_adr);
			if (it != sym.begin() && it != sym.end()) it[-1].n++;
		}
	}
	if (memcmp(lastVRAM, &m[VRAM], sizeof(lastVRAM)) != 0)
		[self performSelectorOnMainThread:@selector(refresh) withObject:nil waitUntilDone:NO];
	memcpy(lastVRAM, &m[VRAM], sizeof(lastVRAM));
}

- (BOOL)run:(int)sampleN buf:(float *)buf {
	if (!ready) return FALSE;
	if (closing) {
		closing = CLOSING_READY;
		return FALSE;
	}
	_keymod = [NSEvent modifierFlags] & NSShiftKeyMask ? 1 : 0;
	int clockN = 1e6f / 44100 * sampleN * _appDelegate.cpuClock;
	[self process:clockN];
	for (int i = 0; i < sampleN; i++) {
		short l, r;
		pc.PSGUpdate(l, r);
		const float V = 3e-5f;
		buf[i << 1] = V * l;
		buf[(i << 1) + 1] = V * r;
	}
	return YES;
}

- (void)vsync {
	pc.INT(2);
}

- (void)refresh {
	[_view setNeedsDisplay:YES];
}

- (BOOL)loadSRecord:(const char *)path buf:(uint8_t *)buf len:(size_t)len {
	FILE *fi = fopen(path, "r");
	if (fi) {
		char s[256];
		while (fgets(s, sizeof(s), fi)) if (*s == 'S') {
			char *p = s + 1;
			int t = getHex(p, 1), n = getHex(p, 2) - 4, a = getHex(p, 6);
			if (t != 2) break;
			if (t == 2)
				while (--n >= 0)
					if (a < len) buf[a++] = getHex(p, 2);
		}
		fclose(fi);
		return YES;
	}
	return NO;
}

- (BOOL)load {
	struct stat st;
	if (stat(self.fileURL.fileSystemRepresentation, &st)) {
		NSLog(@"stat error");
		return NO;
	}
	if (mtimespec.tv_sec < st.st_mtimespec.tv_sec) {
		[self dumpProfile];
		if (![self loadSRecord:self.fileURL.path.fileSystemRepresentation buf:m len:sizeof(m)]) return NO;
		NSString *path = [self.fileURL.path.stringByDeletingPathExtension stringByAppendingString:@".adr"];
		FILE *fi = fopen(path.fileSystemRepresentation, "r");
		if (fi) {
			char s[256];
			while (fgets(s, sizeof(s), fi)) {
				int adr;
				if (sscanf(s, "%x", &adr) == 1 && strlen(s) > 6)
					sym.push_back(Sym(adr, s + 6));
			}
			fclose(fi);
			sym.push_back(Sym(0xfffff));
		}
		pc.Reset();
	}
	mtimespec = st.st_mtimespec;
	return YES;
}

- (void)becomeActive {
	@synchronized (self) {
		[self load];
	}
}

- (BOOL)readFromData:(NSData *)data ofType:(NSString *)typeName error:(NSError **)outError {
	if ([typeName isEqualToString:@"SRecord"]) {
		pc.SetMemory(m);
		pc.SetDoc(self);
		ready = [self load];
		return ready;
	}
	return NO;
}

- (void)key:(int)code isUp:(BOOL)f {
	_keydata = (code & 0x7f) | (f ? 0 : 0x80);
	pc.INT(3);
}

- (uint8_t *)vram {
	return m + VRAM;
}

@end
