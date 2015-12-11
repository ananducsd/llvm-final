#include <stdio.h>

int f (int a, int b) {
	int c = a + b;
	int d = a + b;

	int e = c + d;

	return e;
}

int main() {
	
	f(2, 3);
	return 0;
}