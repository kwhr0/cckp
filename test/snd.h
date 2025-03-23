#include "types.h"

#define INTERVAL	11610L		// uS

void SndInit();
void SndUpdate();
void SndKeyOn(u8 prog, u8 note, u8 velo, u8 volex, u8 pan, s16 bend, u16 id);
void SndKeyOff(u16 id);
void SndVolex(u8 id_low, u8 volex, u8 pan);
void SndBend(u8 id_low, s16 bend);
u8 SndToggleMute(u8 midi_ch);
