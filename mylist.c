/* File:	mylist.c
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

#ifndef AGIOS_KERNEL_MODULE
#include <stdlib.h>
#endif
#include "mylist.h"
#include "common_functions.h"

void init_agios_list_head(struct agios_list_head *list)
{
	list->next = list;
	list->prev = list;
}
inline void __agios_list_add(struct agios_list_head *new, struct agios_list_head *prev, struct agios_list_head *next)
{
        next->prev = new;
        new->next = next;
        new->prev = prev;
        prev->next = new;
}
inline void agios_list_add(struct agios_list_head *new, struct agios_list_head *head)
{

        __agios_list_add(new, head, head->next);
}
inline void agios_list_add_tail(struct agios_list_head *new, struct agios_list_head *head)
{

	__agios_list_add(new, head->prev, head);
}
inline void __agios_list_del(struct agios_list_head * prev, struct agios_list_head * next)
{
        next->prev = prev;
        prev->next = next;
}
inline void agios_list_del(struct agios_list_head *entry)
{

        __agios_list_del(entry->prev, entry->next);
        entry->next = entry;
        entry->prev = entry;
}
inline void __agios_list_del_entry(struct agios_list_head *entry)
{
        __agios_list_del(entry->prev, entry->next);
}
inline void agios_list_del_init(struct agios_list_head *entry)
{
        __agios_list_del_entry(entry);
        init_agios_list_head(entry);
}
inline int agios_list_empty(const struct agios_list_head *head)
{
        return head->next == head;
}
