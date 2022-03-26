SRCS = main.c viewer.c player.c base.c graph.c font.c diskio.c file.c midi.c snd.c util.c

CC = ~/Library/Developer/Xcode/DerivedData/cckp-*/Build/Products/Release/cckp
CFLAGS = -D__KLIBC__ -Iklibc/usr/include -Iklibc/usr/include/bits32 \
	-Iff14b/source -Itjpgd3/src -Ixprintf/src
LDFLAGS = -lklibc -lxprintf -lff -ltjpgd
DEPEND = Depend

all: font.c a.srec

a.srec: $(SRCS)
	make -f Makefile.klibc
	make -f Makefile.xprintf
	make -f Makefile.ff
	make -f Makefile.tjpgd
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS)

$(DEPEND):
	./mkfont.pl
	$(CC) -M $(CFLAGS) $(SRCS) > $(DEPEND)

font.c:
	./mkfont.pl

clean:
	rm -f font.c a.* *.lp $(DEPEND)
	find . -name \*\.o -exec rm {} \;

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPEND)
endif
