
all: example test_ipc

example: example.c
	cc example.c -o example -lrt

test_ipc: test.c
	cc test.c -o test_ipc -lrt -pthread

test: example test_ipc
	./test_ipc
	./example
	if [ -n "`cat process.*.txt | sort | uniq -d`" ]; then \
		echo "FAIL: duplicate IDs"; exit 1; \
	fi
	echo "Generated `cat process.*.txt | wc -l` ID"

clean:
	rm -f ./example ./test_ipc ./process.*
