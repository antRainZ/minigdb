void f2(long a,long b,long var,long var2,long var3,long var4) {
    long c = 2;
    long d = a + b;
}

void f(long a) {
    long b = 2;
    long c = a + b;
    f2(b,c,a,a+1,a+2,a+3);
}

int main() {
    long a = 3;
    long b = 2;
    long c = a + b;
    a = 4;
    f(a);
}
