/* File:	access_pattern_detection_tree.h
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides a decision tree that detects access pattern aspects from 
 *		requests streams. It requires the "average distance between consecutive 
 *		requests" and "average stripe access time difference" metrics.
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
#ifndef ACCESS_PATTERN_DECISION_TREE_H
#define ACCESS_PATTERN_DECISION_TREE_H

void access_pattern_detection_tree (int operation, long double avgdist, long double timediff, short int *spatiality, short int *reqsize);

#endif
