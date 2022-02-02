make:
	g++ -std=c++11 -Wall -o buddy_allocator buddy_allocator.c

clean:
	rm -rf buddy_allocator
