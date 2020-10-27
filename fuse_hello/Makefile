all: hello

install-deps:
	sudo apt install fuse3 libfuse3-3 libfuse3-dev

hello: hello.c
	mkdir mnt
	gcc -Wall $< `pkg-config fuse3 --cflags --libs` -o $@

clean:
	rm hello
	rmdir mnt
