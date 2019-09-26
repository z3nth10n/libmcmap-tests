#include <stdio.h>
#include <stdlib.h>

#include "mcmap.h"

int main()
{
    // printf("Hello world!\n");
    mcmap_region_read(0, 0, "../Worlds/world_0/region/");
    getchar();
    return 0;
}
