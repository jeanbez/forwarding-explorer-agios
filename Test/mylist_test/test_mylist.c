#include <stdlib.h>
#include <stdio.h>
#include "mylist.h"

AGIOS_LIST_HEAD(lista);
struct elem_t
{
	int valor;
	struct agios_list_head list;
};

struct elem_t* add_new_elem(int v)
{
	struct elem_t *new = malloc(sizeof(struct elem_t));
	new->valor = v;
	agios_list_add_tail(&new->list, &lista);
	return new;
}

void print_list()
{
	struct elem_t *i;

	agios_list_for_each_entry(i, &lista, list)
		printf("%d ", i->valor);
	printf("\n");
}

void print_elem(struct elem_t *elem)
{
	printf("this element is %d, previous is %d, next is %d. Is the previous the head? %d\n", elem->valor, agios_list_entry(elem->list.prev, struct elem_t, list)->valor, agios_list_entry(elem->list.next, struct elem_t, list)->valor, elem->list.prev == &lista);
}

int main()
{
	struct elem_t *current;

	print_list();
	current = add_new_elem(1);
	print_list();
	print_elem(current);
	current = add_new_elem(2);
	print_list();
	print_elem(current);
	current = add_new_elem(2);
	print_list();
	print_elem(current);
	
}
