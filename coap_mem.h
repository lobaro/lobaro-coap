//Taken from http://www.fourmilab.ch/bget/
//"BGET is in the public domain. You can do anything you like with it."
//Thank you Mr. Walker for your great work!

/*
    Interface definitions for bget.c, the memory management package.

*/
#ifndef __BGET__
#define __BGET__

#ifndef _
#ifdef PROTOTYPES
#define  _(x)  x		      /* If compiler knows prototypes */
#else
#define  _(x)  ()                     /* It it doesn't */
#endif /* PROTOTYPES */
#endif

typedef int16_t bufsize;

void	bpool	    _((void *buffer, bufsize len));
void   *bget	    _((bufsize size));
void   *bgetz	    _((bufsize size));
void   *bgetr	    _((void *buffer, bufsize newsize));
void	brel	    _((void *buf));
void	bectl	    _((int (*compact)(bufsize sizereq, int sequence),
		       void *(*acquire)(bufsize size),
		       void (*release)(void *buf), bufsize pool_incr));
void	bstats	    _((bufsize *curalloc, bufsize *totfree, bufsize *maxfree,
		       long *nget, long *nrel));
void	bstatse     _((bufsize *pool_incr, long *npool, long *npget,
		       long *nprel, long *ndget, long *ndrel));
void	bufdump     _((void *buf));
void	bpoold	    _((void *pool, int dumpalloc, int dumpfree));
int	bpoolv	    _((void *pool));

//Lobaro:
//4 byte header + alignment = overhead of allocator



void com_mem_init();
void com_mem_stats();
void com_mem_release(void* buf);
void* com_mem_get(bufsize size); //define in for debug output of each requested mem
void* com_mem_get0(bufsize size); //get zero initialized buf mem
int32_t bsize(void* buf);

uint8_t* com_mem_buf_lowEnd();
uint8_t* com_mem_buf_highEnd();

#endif
