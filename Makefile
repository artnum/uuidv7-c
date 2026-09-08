
all: example

example: example.c
	cc example.c -o example  -lrt

test: example
	./example
	cat process.*.txt | sort | uniq -d
	sleep 1
	echo "Generated `cat process.*.txt | wc -l` ID"

clean:
	rm ./example ./process.*
