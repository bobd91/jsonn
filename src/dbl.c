#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char *argv[])
{
        char *p = argv[1];

        char *start = p;
        char *dp;
        int inc = 1;
        int ev = 0;
        int einc = 1;
        int expv = 0;
        long long llv = 0;

        if('-' == *p) {
                inc = -1;
                p++;
        }

        if('0' == *p) {
                char cp1 = *(p + 1);
                if(cp1 != '.' && cp1 != 'e' && cp1 != 'E') {
                        printf("0\n");
                        return 0;
                }
        }

        int d = *p - '0';
        while(d >= 0 && d <= 9) {
                llv = (10 * llv) + d;
                p++;
                d = *p - '0';
        }

        if(*p == '.') {
                dp = p;
                p++;
                d = *p - '0';
                while(d >= 0 && d <= 9) {
                        llv = (10 * llv) + d;
                        p++;
                        d = *p - '0';
                }
                expv = 1 + dp - p;
        }

        if(*p == 'e' || *p == 'E') {
                dp = p;
                p++;
                if(*p == '-') {
                        p++;
                        einc = -1;
                } else if (*p == '+') {
                        p++;
                }
                d = *p - '0';
                while(d >= 0 && d <= 9) {
                        ev = (10 * ev) + d;
                        p++;
                        d = *p - '0';
                }
                expv += einc * ev;
        }
        if(!dp) {
                printf("%ld\n", inc * llv);
                return 0;
        }
        long double dv = (long double)llv;
        dv *= pow(10, expv);
        printf("%.18Lg\n", inc * dv);
        printf("%.18Lg\n", strtold(argv[1], NULL));
        return 0;
}





