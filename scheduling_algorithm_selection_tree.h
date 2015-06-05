/* File:	scheduling_algorithm_selection_tree.h
 * Created: 	2015 
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
#ifndef SCHEDULING_ALGORITHM_DECISION_TREE_H
#define SCHEDULING_ALGORITHM_DECISION_TREE_H

char * scheduling_algorithm_selection_tree (int operation, int fileno, double serra, short int spatiality, short int reqsize);

#endif
