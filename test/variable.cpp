void f(long a) {
    long b = 2;
    long c = a + b;
}

int main() {
    long a = 3;
    long b = 2;
    long c = a + b;
    a = 4;
    f(a);
}
