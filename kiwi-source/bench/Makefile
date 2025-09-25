WARN=-Wall -Wno-implicit-function-declaration -Wno-unused-but-set-variable
DEBUG=-g -ggdb

all:
	gcc -g -ggdb -Wall -I../engine -Wno-implicit-function-declaration -Wno-unused-but-set-variable bench.c kiwi.c -L ../engine -lindexer -lpthread -lsnappy -o kiwi-bench
clean:
	rm -f kiwi-bench
	rm -rf testdb
