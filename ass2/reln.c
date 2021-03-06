// reln.c ... functions on Relations
// part of signature indexed files
// Written by John Shepherd, March 2019

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "defs.h"
#include "reln.h"
#include "page.h"
#include "tuple.h"
#include "tsig.h"
#include "psig.h"
#include "bits.h"
#include "hash.h"
#include <math.h>

// open a file with a specified suffix
// - always open for both reading and writing

File openFile(char *name, char *suffix)
{
	char fname[MAXFILENAME];
	sprintf(fname,"%s.%s",name,suffix);
	File f = open(fname,O_RDWR|O_CREAT,0644);
	assert(f >= 0);
	return f;
}

// create a new relation (five files)
// data file has one empty data page

Status newRelation(char *name, Count nattrs, float pF, char sigtype,
                   Count tk, Count tm, Count pm, Count bm)
{
	Reln r = malloc(sizeof(RelnRep));
	RelnParams *p = &(r->params);
	assert(r != NULL);
	p->nattrs = nattrs;
	p->pF = pF,
	p->sigtype = sigtype;
	p->tupsize = 28 + 7*(nattrs-2);
	Count available = (PAGESIZE-sizeof(Count));
	p->tupPP = available/p->tupsize;
	p->tk = tk; 
	if (tm%8 > 0) tm += 8-(tm%8); // round up to byte size
	p->tm = tm; p->tsigSize = tm/8; p->tsigPP = available/(tm/8);
	if (pm%8 > 0) pm += 8-(pm%8); // round up to byte size
	p->pm = pm; p->psigSize = pm/8; p->psigPP = available/(pm/8);
	if (p->psigPP < 2) { free(r); return -1; }
	if (bm%8 > 0) bm += 8-(bm%8); // round up to byte size
	p->bm = bm; p->bsigSize = bm/8; p->bsigPP = available/(bm/8);
	if (p->bsigPP < 2) { free(r); return -1; }
	r->infof = openFile(name,"info");
	r->dataf = openFile(name,"data");
	r->tsigf = openFile(name,"tsig");
	r->psigf = openFile(name,"psig");
	r->bsigf = openFile(name,"bsig");
	addPage(r->dataf); p->npages = 1; p->ntups = 0;
	addPage(r->tsigf); p->tsigNpages = 1; p->ntsigs = 0;
	addPage(r->psigf); p->psigNpages = 1; p->npsigs = 0;
	addPage(r->bsigf); p->bsigNpages = 1; p->nbsigs = 0; // replace this
	// Create a file containing "pm" all-zeroes bit-strings,
    // each of which has length "bm" bits
	//TODO
	Count bp = iceil(p->pm, p->bsigPP);	// number page need to add
	// printf ("pm is %d\nbm is %d\nbsig page needed is %d\n", p->pm, p->bm, bp);
	// printf ("bsig per page is %d\n", p->bsigPP);
	for (PageID bsigpid = 0; bsigpid < bp; bsigpid++) {
		Page bsigp = newPage();
		// add bsigs to each page
		for (Offset bid = 0; bid < p->bsigPP; bid++) {
			Bits bsig = newBits(p->bm);	// bsig has length bm bits
			putBits(bsigp, bid, bsig);
			addOneItem(bsigp);
			p->nbsigs++;
			freeBits(bsig);
			if (p->nbsigs == p->pm) break;
		}
		// add page into signature file and increase page count
		putPage(r->bsigf, bsigpid, bsigp);
		p->bsigNpages++;
	}
	p->bsigNpages--;
	closeRelation(r);
	return 0;
}

// check whether a relation already exists

Bool existsRelation(char *name)
{
	char fname[MAXFILENAME];
	sprintf(fname,"%s.info",name);
	File f = open(fname,O_RDONLY);
	if (f < 0)
		return FALSE;
	else {
		close(f);
		return TRUE;
	}
}

// set up a relation descriptor from relation name
// open files, reads information from rel.info

Reln openRelation(char *name)
{
	Reln r = malloc(sizeof(RelnRep));
	assert(r != NULL);
	r->infof = openFile(name,"info");
	r->dataf = openFile(name,"data");
	r->tsigf = openFile(name,"tsig");
	r->psigf = openFile(name,"psig");
	r->bsigf = openFile(name,"bsig");
	read(r->infof, &(r->params), sizeof(RelnParams));
	return r;
}

// release files and descriptor for an open relation
// copy latest information to .info file
// note: we don't write ChoiceVector since it doesn't change

void closeRelation(Reln r)
{
	// make sure updated global data is put in info file
	lseek(r->infof, 0, SEEK_SET);
	int n = write(r->infof, &(r->params), sizeof(RelnParams));
	assert(n == sizeof(RelnParams));
	close(r->infof); close(r->dataf);
	close(r->tsigf); close(r->psigf); close(r->bsigf);
	free(r);
}

// insert a new tuple into a relation
// returns page where inserted
// returns NO_PAGE if insert fails completely

PageID addToRelation(Reln r, Tuple t)
{
	assert(r != NULL && t != NULL && strlen(t) == tupSize(r));
	Page p;  PageID pid;
	RelnParams *rp = &(r->params);
	
	// add tuple to last page
	pid = rp->npages-1;
	p = getPage(r->dataf, pid);
	// check if room on last page; if not add new page	
	if (pageNitems(p) == rp->tupPP) {		
		addPage(r->dataf);
		rp->npages++;
		pid++;
		free(p);
		p = newPage();	
		if (p == NULL) return NO_PAGE;
	}
	addTupleToPage(r, p, t);
	rp->ntups++;  //written to disk in closeRelation()
	putPage(r->dataf, pid, p);

	// compute tuple signature and add to tsigf
		
	//TODO
	Bits tsig = makeTupleSig(r, t);				// compute tuple signature
	PageID tsigpid = rp->tsigNpages - 1;		// get current tsig pid
	Page tsigp = getPage(r->tsigf, tsigpid);	// get last page
	// check if slot on last page; if not add new page
	if (pageNitems(tsigp) == rp->tsigPP) {
		addPage(r->tsigf);
		rp->tsigNpages++;
		tsigpid++;
		free(tsigp);
		tsigp = newPage();
		if (tsigp == NULL) return NO_PAGE;
	}
	putBits(tsigp, pageNitems(tsigp), tsig);
	rp->ntsigs++;
	addOneItem(tsigp);
	putPage(r->tsigf, tsigpid, tsigp);
	freeBits(tsig);
	
	// compute page signature and add to psigf
	//TODO
	// check if the last page in data file is new added
	// if new added, add the psig directly to psigf
	// otherwise, get psig of current data page and orBits
	Bits psig = makePageSig(r, t);				// compute page signature
	PageID psigpid = rp->psigNpages - 1; 		// get current psig pid
	Page psigp = getPage(r->psigf, psigpid); 	// get last psig page
		
	if (rp->npsigs != rp->npages) { 
		// new added page
		if (pageNitems(psigp) == rp->psigPP) {
			addPage(r->psigf);
			rp->psigNpages++;
			psigpid++;
			free(psigp);
			psigp = newPage();
			if (psigp == NULL) return NO_PAGE;
		}
		rp->npsigs++;
		// add new psig
		putBits(psigp, pageNitems(psigp), psig);
		addOneItem(psigp);
		
	} else {
		// get current psig and merge with new psig
		Bits curpsig = newBits(psigBits(r));
		getBits(psigp, pid % maxPsigsPP(r), curpsig);
		orBits(curpsig, psig);
		putBits(psigp, pid % maxPsigsPP(r), curpsig);
		free(curpsig);
	}
	putPage(r->psigf, psigpid, psigp);

	// use page signature to update bit-slices

	//TODO
	// Iterate every bit-slice
	for (int i = 0; i < rp->pm; i++) {
		// update bit-slices corresponding to 1-bits in the page signature
		if (bitIsSet(psig, i)) {
			PageID curbsigpid = i / rp->bsigPP;	
			Page curbsigp = getPage(r->bsigf, curbsigpid);
			Offset bi = i % rp->bsigPP;
			Bits slice = newBits(rp->bm);
			// get i'th bit slice from bsigFile
			getBits(curbsigp, bi, slice);
			// set the PID'th bit in Slice
			setBit(slice, pid);
			// write updated Slice back to bsigFile
			putBits(curbsigp, bi, slice);
			putPage(r->bsigf, curbsigpid, curbsigp);
			freeBits(slice);
		}
	}
	freeBits(psig);
	
	return nPages(r)-1;
}

// displays info about open Reln (for debugging)

void relationStats(Reln r)
{
	RelnParams *p = &(r->params);
	printf("Global Info:\n");
	printf("Dynamic:\n");
    printf("  #items:  tuples: %d  tsigs: %d  psigs: %d  bsigs: %d\n",
			p->ntups, p->ntsigs, p->npsigs, p->nbsigs);
    printf("  #pages:  tuples: %d  tsigs: %d  psigs: %d  bsigs: %d\n",
			p->npages, p->tsigNpages, p->psigNpages, p->bsigNpages);
	printf("Static:\n");
    printf("  tups   #attrs: %d  size: %d bytes  max/page: %d\n",
			p->nattrs, p->tupsize, p->tupPP);
	printf("  sigs   %s",
            p->sigtype == 'c' ? "catc" : "simc");
    if (p->sigtype == 's')
	    printf("  bits/attr: %d", p->tk);
    printf("\n");
	printf("  tsigs  size: %d bits (%d bytes)  max/page: %d\n",
			p->tm, p->tsigSize, p->tsigPP);
	printf("  psigs  size: %d bits (%d bytes)  max/page: %d\n",
			p->pm, p->psigSize, p->psigPP);
	printf("  bsigs  size: %d bits (%d bytes)  max/page: %d\n",
			p->bm, p->bsigSize, p->bsigPP);
}
