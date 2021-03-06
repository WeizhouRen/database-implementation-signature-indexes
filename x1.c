// Test Bits ADT

#include <stdio.h>
#include "defs.h"
#include "reln.h"
#include "tuple.h"
#include "bits.h"

int main(int argc, char **argv)
{
	Bits b = newBits(60);
	Bits b1 = newBits(10);
	Bits b2 = newBits(10);
	setBit(b1, 2);
	setBit(b1, 5);
	setBit(b2, 2);
	setBit(b2, 5);
	setBit(b2, 7);
	printf("b1: "); showBits(b1); printf("\n");
	printf("b2: "); showBits(b2); printf("\n");
	printf("b1 is subset of b2: %d\n", isSubset(b1, b2));
	
	setBit(b1, 9);
	printf("b1 is subset of b2: %d\n", isSubset(b1, b2));
	printf("t=0: "); showBits(b); printf("\n");
	setBit(b, 5);
	printf("t=1: "); showBits(b); printf("\n");
	setBit(b, 0);
	setBit(b, 50);
	setBit(b, 59);
	
	printf("t=2: "); showBits(b); printf("\n");
	if (bitIsSet(b,5)) printf("Bit 5 is set\n");
	if (bitIsSet(b,10)) printf("Bit 10 is set\n");
	setAllBits(b);
	printf("t=3: "); showBits(b); printf("\n");
	unsetBit(b, 40);
	printf("t=4: "); showBits(b); printf("\n");
	if (bitIsSet(b,20)) printf("Bit 20 is set\n");
	if (bitIsSet(b,40)) printf("Bit 40 is set\n");
	if (bitIsSet(b,50)) printf("Bit 50 is set\n");
	setBit(b, 59);
	printf("s=3: "); showBits(b); printf("\n");
	shiftBits(b, 11);
	printf("s=3: "); showBits(b); printf("\n");
	return 0;
}
