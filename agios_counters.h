/*! \file agios_counters.h
    \brief Headers to request and file counters.

    @see agios_counters.c
*/

#pragma once

extern int current_reqnb;
extern int current_reqfilenb;
extern pthread_mutex_t current_reqnb_lock;

int32_t get_current_reqnb(void); 
void inc_current_reqnb(void);
void dec_current_reqnb(int32_t hash);
void dec_many_current_reqnb(int32_t hash, int32_t value);
void inc_current_reqfilenb(void);
void dec_current_reqfilenb(void);
