#ifndef _HASH_H_
#define _HASH_H_

#define AGIOS_HASH_SHIFT 6						/* 64 slots in */
#define AGIOS_HASH_ENTRIES		(1 << AGIOS_HASH_SHIFT)		/* hash table  */
#define AGIOS_HASH_FN(inode)	 	(agios_hash_str(inode, AGIOS_HASH_SHIFT))
#define AGIOS_HASH_STR(inode)		(agios_hash_str(inode, AGIOS_HASH_SHIFT))


long agios_hash_str(const char *ptr, unsigned int bits);

#endif
