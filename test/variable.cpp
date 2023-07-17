void f2(long a,long b) {
    long c = 2;
    long d = a + b;
}

void f(long a) {
    long b = 2;
    long c = a + b;
    f2(b,c);
}

int main() {
    long a = 3;
    long b = 2;
    long c = a + b;
    a = 4;
    f(a);
}
