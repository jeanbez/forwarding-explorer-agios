ccflags-y += -DAGIOS_DEBUG=1

FILES=agios.c \
      agios_add_request.c \
      agios_cancel_request.c \
      agios_config.c \
      agios_counters.c \
      agios_release_request.c \
      agios_request.c \
      agios_thread.c \
      aIOLi.c \
      common_functions.c \
      data_structures.c \
      hash.c \
      MLF.c \
      mylist.c \
      NOOP.c \
      performance.c \
      process_request.c \
      req_hashtable.c \
      req_timeline.c \
      scheduling_algorithms.c \
      SJF.c \
      statistics.c \
      SW.c \
      TO.c \
      trace.c \
      TWINS.c \
      waiting_common.c

OBJS=agios.o \
      agios_add_request.o \
      agios_cancel_request.o \
      agios_config.o \
      agios_counters.o \
      agios_release_request.o \
      agios_request.o \
      agios_thread.o \
      aIOLi.o \
      common_functions.o \
      data_structures.o \
      hash.o \
      MLF.o \
      mylist.o \
      NOOP.o \
      performance.o \
      process_request.o \
      req_hashtable.o \
      req_timeline.o \
      scheduling_algorithms.o \
      SJF.o \
      statistics.o \
      SW.o \
      TO.o \
      trace.o \
      TWINS.o \
      waiting_common.o

library: ${OBJS} 
	gcc -shared -o libagios.so ${OBJS}

all: library

${OBJS}:
	gcc -fPIC -Wall -O ${ccflags-y} -c ${FILES} -lm -lpthread -lrt -lconfig

clean: 
	rm -rf *.so
	rm -rf *.o
