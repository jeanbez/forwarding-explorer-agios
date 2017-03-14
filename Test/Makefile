# Makefile
# File: "/home/fran/libaioli/libagios_new/throughput_test/Makefile"
# Created: "Ter, 28 Jan 2014 23:09:21 -0200 (fran)"
# Updated: "Qui, 30 Jan 2014 14:30:27 -0200 (fran)"
# $Id$
# Copyright (C) 2014, Francieli Zanon Boito <fzboito@inf.ufrgs.br> 

all:
	gcc -o throughput_test throughput_test.c -lagios -lrt -lpthread -lm
	gcc -o addrequest_test addrequest_test.c -lagios -lrt -lpthread -lm
	gcc -o timewindow_test timewindow_test.c -lagios -lrt -lpthread -lm

throughput:
	gcc -lm -lpthread -lagios -o throughput_test throughput_test.c 

clean:
	rm -rf throughput_test addrequest_test timewindow_test 

local:
	gcc -o throughput_test throughput_test.c ../libagios.so.1.0.1 -lrt -lpthread -lm
	gcc -o addrequest_test addrequest_test.c ../libagios.so.1.0.1 -lrt -lpthread -lm
	gcc -o timewindow_test timewindow_test.c ../libagios.so.1.0.1 -lrt -lpthread -lm

