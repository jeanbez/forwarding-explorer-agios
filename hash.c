/* File:	hash.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides hash functions, used to include a request in the hashtable
 *		Further information is available at http://inf.ufrgs.br/~fzboito/agios.html
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 *		inspired in Adrien Lebre's aIOLi framework implementation
 *	
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "agios.h"
#include "hash.h"

#ifndef AGIOS_KERNEL_MODULE
#include <string.h>
#endif

#define GOLDEN_RATIO_PRIME_32 0x9e370001UL
#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

static inline int hash_32(int val, unsigned int bits)
{
        /* On some cpus multiply is faster, on others gcc will do shifts */
        int hash = val * GOLDEN_RATIO_PRIME_32;

        /* High bits are more random, so use them. */
        return hash >> (32 - bits);
}

static inline long long int hash_64(long long int val, unsigned int bits)
{
        long long int hash = val;

        /*  Sigh, gcc can't optimise this alone like it does for 32 bits. */
        long long int n = hash;
        n <<= 18;
        hash -= n;
        n <<= 33;
        hash -= n;
        n <<= 3;
        hash += n;
        n <<= 3;
        hash -= n;
        n <<= 4;
        hash += n;
        n <<= 2;
        hash += n;

        /* High bits are more random, so use them. */
        return hash >> (64 - bits);
}

static inline long long int hash_long(long long int val, unsigned int bits)
{
	int bits_per_long = sizeof(long)*8;
	if(bits_per_long == 32)
		return hash_32(val, bits);
	else
		return hash_64(val, bits);
}

inline unsigned long agios_hash_str(const char *ptr, unsigned int bits)
{
	unsigned long rep=0;
	int i;
	/*makes a number using the sum of all the characters of the name of the file*/
	for(i=0; i< strlen(ptr); i++)
		rep += ptr[i];
	/*returns the hash*/
	return hash_long(rep, bits);
}
