/* File:	mylist.h
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		Copy of the list implementation from the Linux kernel
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

#ifndef MYLIST_H
#define MYLIST_H


struct agios_list_head
{
	struct agios_list_head *prev,*next;
};

#define AGIOS_LIST_HEAD_INIT(name) { &(name), &(name) }

#define AGIOS_LIST_HEAD(name) \
         struct agios_list_head name = AGIOS_LIST_HEAD_INIT(name)

#define agios_offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define agios_container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - agios_offsetof(type,member) );})

#define agios_list_entry(ptr, type, member) \
        agios_container_of(ptr, type, member)

#define agios_list_for_each_entry(pos, head, member)                          \
         for (pos = agios_list_entry((head)->next, typeof(*pos), member);      \
              &pos->member != (head);        \
              pos = agios_list_entry(pos->member.next, typeof(*pos), member))

void init_agios_list_head(struct agios_list_head *list);

inline void __agios_list_add(struct agios_list_head *new, struct agios_list_head *prev, struct agios_list_head *next);
inline void __agios_list_del(struct agios_list_head * prev, struct agios_list_head * next);
//insert new after head
inline void agios_list_add(struct agios_list_head *new, struct agios_list_head *head);

//insert new before head
inline void agios_list_add_tail(struct agios_list_head *new, struct agios_list_head *head);




inline void agios_list_del(struct agios_list_head *entry);


inline void agios_list_del_init(struct agios_list_head *entry);

inline int agios_list_empty(const struct agios_list_head *head);

#endif
