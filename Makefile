CC = gcc
CFLAGS = -Wall -Werror -lpthread

# List of all source files
SOURCES := clientinit.c namingserver.c serverinit.c $(wildcard operations/*.c)
# List of all header files
HEADERS := definitions.h utilities.h $(wildcard operations/*.h)

# List of executable targets
EXE := $(patsubst %.c,%,$(SOURCES))

.PHONY: all clean

all: $(EXE)

# Rule to build executables
%: %.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(EXE)

cleansm:
	rm -f c s nm

namingserver:
	gcc -Wall -Werror -lpthread namingserver.c ./hashmap/map.c ./hashmap/map_operations.c ./LRU/lru.c -o nm

client:
	gcc -Wall -Werror -lpthread clientinit.c -o c

storageserver:
	gcc -Wall -Werror -lpthread storageserverinit.c ./operations/create_file.c ./operations/delete.c ./hashmap/map.c ./hashmap/map_operations.c -o s