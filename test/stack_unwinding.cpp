#include <stdio.h>
void funca(long arg1,long arg2,long arg3,long arg4,long arg5,long arg6) {
    long foo = 1;
    printf("foo=%ld,arg1=%ld,arg2=%ld\n",foo,arg1,arg2);
    printf("arg3=%ld,arg4=%ld,arg5=%ld\n",arg3,arg4,arg5);
    long a = 3;
    printf("a=%ld\n",a);
    long b = 2;
    printf("b=%ld\n",b);
    int  c = 4;
    printf("v=%d\n",c);
    long d = a + b + c + foo + arg6;
    printf("d=%ld\n",d);
}

void funcb(long arg1,long arg2,long arg3,long arg4,long arg5) {
    long foo = 2;
    printf("foo=%ld,arg1=%ld,arg2=%ld\n",foo,arg1,arg2);
    printf("arg3=%ld,arg4=%ld,arg5=%ld\n",arg3,arg4,arg5);
    funca(foo,arg1,arg2,arg3,arg4,arg5);
}

void funcc(long arg1,long arg2,long arg3,long arg4) {
    long foo = 3;
    printf("foo=%ld,arg1=%ld,arg2=%ld\n",foo,arg1,arg2);
    printf("foo=%ld,arg3=%ld,arg4=%ld\n",foo,arg3,arg4);
    funcb(foo,arg1,arg2,arg3,arg4);
}

void funcd(long arg1,long arg2,long arg3) {
    long foo = 4;
    printf("foo=%ld,arg1=%ld,arg2=%ld,arg3=%ld\n",foo,arg1,arg2,arg3);
    funcc(foo,arg1,arg2,arg3);
}

void funce(long arg1,long arg2) {
    
    long foo = 5;
    printf("foo=%ld,arg1=%ld,arg2=%ld\n",foo,arg1,arg2);
    funcd(foo,arg1,arg2);
}

void f(long arg) {
    long foo = 6;
    printf("foo=%ld,arg=%ld\n",foo,arg);
    funce(foo,arg);
}

int main() {
    long a=1;
    f(a);
}
