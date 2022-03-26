#! /usr/bin/perl

$yn = 12;

open(S, "mplus_bitmap_fonts/fonts_e/mplus_f${yn}r.bdf") || die;
while (<S>) {
	if (/^ENCODING\s+(\d+)/) {
		$a = $yn * ($1 - 0x20);
		$count = 0;
	}
	elsif ($a >= 0 && $a < 0x1000 && /^[0-9A-Fa-f]{2}$/) {
		$d[$a++] = hex($&) if $count++;
	}
}
close S;
open(D, "> font.c") || die;
printf D "unsigned char gFont[] = {\n";
for ($i = 0; $i < @d; $i++) {
	printf D "0x%02x,", $d[$i];
	printf D "//0x%02x\n", $i / $yn + 0x20 if $i % $yn == $yn - 1;
}
printf D "\n};\n";
close D;
exit 0;
