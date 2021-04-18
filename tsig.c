// tsig.c ... functions on Tuple Signatures (tsig's)
// part of signature indexed files
// Written by John Shepherd, March 2019

#include <unistd.h>
#include <string.h>
#include "defs.h"
#include "tsig.h"
#include "reln.h"
#include "hash.h"
#include "bits.h"


Bits genCodeword(char *attr_value, int m, int u, int k) 
{
	int nbits = 0;
	Bits cword = newBits(m);
	srandom(hash_any(attr_value, strlen(attr_value)));
	while (nbits < k) {
		int i = random() % u;
		if (!bitIsSet(cword, i)) {
			setBit(cword, i);
			nbits++; 
		}
	}
	return cword;
}

// make a tuple signature

Bits makeTupleSig(Reln r, Tuple t)
{
	assert(r != NULL && t != NULL);
	//TODO
	Bits tsig = newBits(tsigBits(r));
	Count u = tsigBits(r) / nAttrs(r);
	
	char **tuplevals = tupleVals(r, t);
	for (int i = 0; i < nAttrs(r); i++) {
		if (i == 0) u += tsigBits(r) % nAttrs(r);
		// printf("count: %d\n", u);
		Bits cw = newBits(tsigBits(r));
		// printf("val is %s;	%d\n", tuplevals[i], strcmp(tuplevals[i], "?"));
		if (strcmp(tuplevals[i], "?") != 0) {
			cw = sigType(r) == 's' ? genCodeword(tuplevals[i], tsigBits(r), tsigBits(r), codeBits(r)) 
				: genCodeword(tuplevals[i], tsigBits(r), u, u / 2);
		}
		if (sigType(r) == 'c') {
			// SIMC codewords have the same length as the signatures they produce
			// printf("codeword: 			"); showBits(cw); printf("\n");
			shiftBits(cw, u * i);	// lowest cw shift 0 bit
			// printf("shifted codeword: 	"); showBits(cw); printf("\n");
			
		} 
		// printf("tsig: 	  			"); showBits(tsig); printf("\n");
		
		orBits(tsig, cw);	
		
		// else {				
		// 	// CATC codewords is u = m/n bits long, except for the lower-order codeword
			
		// 	printf("tsig: 	  "); showBits(tsig); printf("\n");
		// 	printf("codeword: "); showBits(cw); printf("\n");
		// }
		
		freeBits(cw);
	}
	free(tuplevals);
	return tsig;
}

// find "matching" pages using tuple signatures

void findPagesUsingTupSigs(Query q)
{
	assert(q != NULL);
	//TODO
	Bits qsig = makeTupleSig(q->rel, q->qstring);
	unsetAllBits(q->pages);	// all zero bits
	// Iterate pages of tsig file
	for (int pid = 0; pid < nTsigPages(q->rel); pid++) {
		Page p = getPage(tsigFile(q->rel), pid);
		// Iterate tsigs in page p to get each tsig in tsigFile
		for (int tid = 0; tid < pageNitems(p); tid++) {
			Bits tsig = newBits(tsigBits(q->rel));
			getBits(p, tid, tsig);	// get current tuple signature
			if (isSubset(qsig, tsig)) {
				// convert to pageID in data file from pageID in tsig file
				Offset datapid = (tid + pid * maxTsigsPP(q->rel)) / maxTupsPP(q->rel);
				setBit(q->pages, datapid);
			}
			freeBits(tsig);
			q->nsigs++;
		}
		free(p);
		q->nsigpages++;
	}
	freeBits(qsig);
	// printf("Matched Pages:"); showBits(q->pages); putchar('\n');
}
