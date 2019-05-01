/*! \file agios_thread.h
    \brief Headers of functions used to run and interact with the agios thread.

    @see agios_thread.c
*/
#pragma once


void * agios_thread(void *arg);
void consumer_signal_new_reqs(void);
void stop_the_agios_thread(void);
