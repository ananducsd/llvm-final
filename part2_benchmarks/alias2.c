struct my_struct {
  int f1;
  int f2;
};

void func(struct munger_struct *P) {
  P[0].f1 = P[1].f1 + P[2].f2;
}

my_struct Array[3];

func(Array);
