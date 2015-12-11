
int main() {
	int x = 9;
	int v = 2;
	int w = 0, y = 0, z = 0;

	if(x < 3) {
		do{
			x = x + 1;
			w = v + 1;	
		} while(x < 10);
	} else {
		w = 3;
		y = x * 2;
		z = y + 5;
	}

	w = w * v;
}