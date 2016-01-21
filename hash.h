/* File:	hash.h
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

#ifndef _HASH_H_
#define _HASH_H_

#define AGIOS_HASH_SHIFT 6						/* 64 slots in */
#define AGIOS_HASH_ENTRIES		(1 << AGIOS_HASH_SHIFT)		/* hash table  */
#define AGIOS_HASH_FN(inode)	 	(agios_hash_str(inode, AGIOS_HASH_SHIFT))
#define AGIOS_HASH_STR(inode)		(agios_hash_str(inode, AGIOS_HASH_SHIFT))


inline unsigned long agios_hash_str(const char *ptr, unsigned int bits);

#endif
