#include <stdint.h>

#define ISA64

#define KP_TRACE		0

#if KP_TRACE
#define KP_TRACE_LOG(p, a)	(tracep->acs##p = a, tracep->adr##p = adr, tracep->data##p = r1)
#else
#define KP_TRACE_LOG(p, a)
#endif

typedef uint32_t adr_t;
typedef uint32_t inst_t;
#ifdef ISA64
typedef int64_t data_t;
typedef uint64_t data_ut;
typedef double data_ft;
#else
typedef int32_t data_t;
typedef uint32_t data_ut;
typedef float data_ft;
#endif

class KP {
public:
	KP() { Reset(); }
	void Reset();
	void SetMemory(uint8_t *_m) { m = _m; }
	int Execute(int n);
	void INT(int num) { intreq |= 1 << (num - 1); }
protected:
	virtual uint8_t input(adr_t adr) = 0;
	virtual void output(adr_t adr, uint8_t data) = 0;
	adr_t pc;
	int clock;
private:
#if DEBUG
	void wchk(adr_t adr);
#endif
	void ill();
	uint8_t *m;
	data_t r[16];
	int intreq;
	bool intperm;
#if KP_TRACE
	static const int TRACEMAX = 10000;
	enum { acs8 = 1, acs16, acs32, acs64 };
	struct TraceBuffer {
		data_t dataR, dataL, dataS;
		adr_t pc, adrR, adrL, adrS;
		int acsR, acsL, acsS;
	};
	TraceBuffer tracebuf[TRACEMAX];
	TraceBuffer *tracep;
	void StopTrace();
#endif
};
