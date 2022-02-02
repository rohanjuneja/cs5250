#include <iostream>
#include <atomic>

using namespace std;


int compare_and_swap(int *reg, int oldVal, int newVal )
{
	if(*reg == oldVal) {
		*reg = newVal;
		return 1;
	}
	return 0;
}

void spinlock_init (int *lock) {
	*lock = 0;
}

void spinlock_lock (int *lock) {
	cout << compare_and_swap(lock, 0, 1);
}

int main() {
	int *lock1 = new int;
	spinlock_init(lock1);
	spinlock_lock(lock1);
	return 0;
}
