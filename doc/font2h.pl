#!/usr/bin/perl -w

my @data;
{ local $/; @data = split(//, <>); }

die "Invalid length" unless scalar @data eq 2048; # 256 chars, 8 bytes per char

printf "static uint8_t font_data[256 * 8] = {\n";
for(my $n = 0; $n < 2048; ) {
	printf "\t";
	for (my $i = 0; $i < 8; $n++, $i++) {
		printf "0x%02x, ", ord($data[$n]);
	}
	printf "\n";
}
#print "[$data]\n";
printf "};\n";
