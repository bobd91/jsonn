#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

int main(int argc, char *argv[])
{
        errno = 0;
        long double ldreal = strtold(argv[1], NULL);
        if(errno) {
                perror("strtold failed");
                exit(1);
        }
        if(!isnormal(ldreal)) {
                fprintf(stderr, "Not normal\n");
                exit(1);
        }
        double real = (double)ldreal;
        long double realld = (long double)real;

        if(realld != ldreal) {
                fprintf(stderr, "Not equal, diff = %17Lg\n", realld - ldreal);
        }
        printf("%16g == %16Lg\n", real, ldreal);
}
