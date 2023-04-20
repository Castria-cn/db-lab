ifeq ($(CONFIG), test)
FILES := test.c
else
FILES := main.c
endif

main: extmem.c extmem.h test.c relation.c relation.h hash_table.c hash_table.h
	gcc -o main extmem.c relation.c hash_table.c $(FILES)
clean:
	rm -rf main.exe
	rm -rf *.blk