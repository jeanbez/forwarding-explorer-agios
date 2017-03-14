/* File:	scheduling_algorithm_selection_tree.c
 * Created: 	2014 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides a decision tree to select the best scheduling algorithm
 *		depending on the situation. It requires information on applications'
 *		access patterns (spatiality and request size) and on the storage 
 *		device's sensitivity to access sequentiality (provided by SeRRa).
 *		Further information is available at http://inf.ufrgs.br/~fzboito/agios.html
 *
 * Obs.: this file was generated automatically from the tree provided by the
 * Weka data mining tool.
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "agios.h"
#include "agios_request.h"
#include "scheduling_algorithm_selection_tree.h"

char * scheduling_algorithm_selection_tree (int operation, int fileno, double serra, short int seq, short int size)
{
	char *ret = (char *)malloc(sizeof(char)*20);
	if (serra <= 8.17)
	{
		if (operation == RT_WRITE)
		{
			if (fileno <= 1)
			{
				if (size == AP_SMALL)
				{
					strcpy(ret, "SJF");
					return ret;
				}
				if (size == AP_LARGE)
				{
					strcpy(ret, "aIOLi");
					return ret;
				}
			}
			if (fileno > 1)
			{
				if (serra <= 2.37)
				{
					if (fileno <= 4)
					{
						strcpy(ret, "SJF");
						return ret;
					}
					if (fileno > 4)
					{
						if (fileno <= 16)
						{
							strcpy(ret, "TO-agg");
							return ret;
						}
						if (fileno > 16)
						{
							strcpy(ret, "SJF");
							return ret;
						}
					}
				}
				if (serra > 2.37)
				{
					strcpy(ret, "SJF");
					return ret;
				}
			}
		}
		if (operation == RT_READ)
		{
			if (size == AP_SMALL)
			{
				if (fileno <= 1)
				{
					strcpy(ret, "SJF");
					return ret;
				}
				if (fileno > 1)
				{
					if (fileno <= 16)
					{
						strcpy(ret, "TO-agg");
						return ret;
					}
					if (fileno > 16)
					{
						strcpy(ret, "aIOLi");
						return ret;
					}
				}
			}
			if (size == AP_LARGE)
			{
				if (fileno <= 4)
				{
					strcpy(ret, "aIOLi");
					return ret;
				}
				if (fileno > 4)
				{
					if (fileno <= 16)
					{
						strcpy(ret, "TO-agg");
						return ret;
					}
					if (fileno > 16)
					{
						strcpy(ret, "aIOLi");
						return ret;
					}
				}
			}
		}
	}
	if (serra > 8.17)
	{
		if (fileno <= 1)
		{
			if (operation == RT_WRITE)
			{
				if (size == AP_SMALL)
				{
					if (serra <= 15.12)
					{
						strcpy(ret, "SJF");
						return ret;
					}
					if (serra > 15.12)
					{
						strcpy(ret, "no_AGIOS");
						return ret;
					}
				}
				if (size == AP_LARGE)
				{
					strcpy(ret, "aIOLi");
					return ret;
				}
			}
			if (operation == RT_READ)
			{
				if (serra <= 25.46)
				{
					strcpy(ret, "SJF");
					return ret;
				}
				if (serra > 25.46)
				{
					if (serra <= 38.91)
					{
						strcpy(ret, "no_AGIOS");
						return ret;
					}
					if (serra > 38.91)
					{
						strcpy(ret, "SJF");
						return ret;
					}
				}
			}
		}
		if (fileno > 1)
		{
			if (serra <= 15.12)
			{
				if (fileno <= 4)
				{
					strcpy(ret, "MLF");
					return ret;
				}
				if (fileno > 4)
				{
					strcpy(ret, "SJF");
					return ret;
				}
			}
			if (serra > 15.12)
			{
				if (serra <= 38.91)
				{
					if (operation == RT_WRITE)
					{
						strcpy(ret, "aIOLi");
						return ret;
					}
					if (operation == RT_READ)
					{
						if (serra <= 25.46)
						{
							strcpy(ret, "SJF");
							return ret;
						}
						if (serra > 25.46)
						{
							strcpy(ret, "aIOLi");
							return ret;
						}
					}
				}
				if (serra > 38.91)
				{
					if (fileno <= 4)
					{
						if (seq == AP_CONTIG)
						{
							if (size == AP_SMALL)
							{
								strcpy(ret, "no_AGIOS");
								return ret;
							}
							if (size == AP_LARGE)
							{
								strcpy(ret, "MLF");
								return ret;
							}
						}
						if (seq == AP_NONCONTIG)
						{
							strcpy(ret, "MLF");
							return ret;
						}
					}
					if (fileno > 4)
					{
						strcpy(ret, "SJF");
						return ret;
					}
				}
			}
		}
	}
	return ret;
}
