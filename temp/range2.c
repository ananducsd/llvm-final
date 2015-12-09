int main() {

	int i = 1;
	int arr[5];

	for(int j = 0; j <= 5; j++) {
		// should give a warning for index j = 5
		arr[j] = -1;
	}
	return arr[0];
}