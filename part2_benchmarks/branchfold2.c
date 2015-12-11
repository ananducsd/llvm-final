#include <stdio.h>

int main() {
	if (1)
	{
		int c1 = 17;
		int c2 = 25;
		int c3 = c1 + c2;
		if(c3 != 0)
		{
			printf("\nValue = %d\n", c3);
		}
		else
			printf("\nShould be removed from analysis");
	}
	else
		printf("\nNever Executed - Should be removed bby analysis");
}
