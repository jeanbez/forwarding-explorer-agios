#ccflags-y += -DAGIOS_DEBUG=1
#ccflags-y += -DAGIOS_KERNEL_MODULE=1
#ccflags-y += -DORANGEFS_AGIOS=1

FILES=mylist.c hash.c proc.c request_cache.c consumer.c iosched.c agios.c predict.c trace.c get_access_times.c common_functions.c agios_config.c access_pattern_detection_tree.c scheduling_algorithm_selection_tree.c
OBJS=mylist.o hash.o proc.o request_cache.o consumer.o iosched.o agios.o predict.o trace.o get_access_times.o common_functions.o agios_config.o access_pattern_detection_tree.o scheduling_algorithm_selection_tree.o /usr/local/lib/libconfig.so
obj-m += agiosmodule.o
agiosmodule-objs := ${OBJS}



module: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

library: ${OBJS} 
#	ar -cr libagios.a ${OBJS}
#	gcc -shared -W1,-soname,libagios.so.1 -o libagios.so.1.0.1 ${OBJS}
	gcc -shared -o libagios.so.1.0.1 ${OBJS}

all: library

${OBJS}:
#	cp time_benchmark/access_times.h . 
	gcc -fPIC -Wall -L/usr/lib/ -L/usr/local/lib/ ${ccflags-y} -c ${FILES} -lpthread -lrt -lconfig

library_install: library
	sudo rm -rf /usr/lib/libagios.so.1.0.1 /usr/lib/libagios.so
	sudo cp ./libagios.so.1.0.1 /usr/lib
	#sudo cp ./agios.h /usr/include/agios.h
	sudo cp ./agios.include.h /usr/include/agios.h
	sudo chmod 0755 /usr/lib/libagios.so.1.0.1
	sudo ln -s /usr/lib/libagios.so.1.0.1 /usr/lib/libagios.so
	sudo ldconfig

clean: 
	rm -rf *.a
	rm -rf *.so.1.0.1
	rm -rf *.o
	rm -rf *.mod.c *.ko Module.symvers modules.order
	rm -rf .*.o.cmd .*.ko.cmd
