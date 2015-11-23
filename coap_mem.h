/*******************************************************************************
 * Copyright (c)  2015  Dipl.-Ing. Tobias Rohde, http://www.lobaro.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *******************************************************************************/
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

void  coap_mem_init(uint8_t* pMemoryArea, int16_t size);

void coap_mem_stats();
void coap_mem_determinateStaticMem(); //used by coap_mem_stats to calculate "dynamic"=dangerous memory size (which should go to zero in the long term, otherwise we have a memory leak!)

void coap_mem_release(void* buf);
void* coap_mem_get(bufsize size); //define in for debug output of each requested mem
void* coap_mem_get0(bufsize size); //get zero initialized buf mem
int32_t bsize(uint8_t* buf);

uint8_t* coap_mem_buf_lowEnd();
uint8_t* coap_mem_buf_highEnd();

#endif
