#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
// #include "sfs_api.h"

int main() {
    double y, z;

    y = (1044 / 1024) + ((1044 % 1024) != 0);

    printf("%f", y);
    return 0;
}

