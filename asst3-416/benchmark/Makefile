
all : test
test: ../my_vm.h
	gcc test.c -L../ -lm -lmy_vm -m32 -o test

clean:
	rm -rf test
