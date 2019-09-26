#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <windows.h>

#include "mcmap.h"
#define nullptr ((void*)0)

int main()
{
    // printf("Hello world!\n");
     char cwd[PATH_MAX];
     char filename[] = "Worlds/world_0/region/";

     getcwd(cwd, sizeof(cwd));

   /* if (getcwd(cwd, sizeof(cwd)) != NULL) {
       printf("Current working dir: %s\n", cwd);
   } else {
       perror("getcwd() error");
       return 1;
   }*/

   GetFullPathName(filename, MAX_PATH, cwd, nullptr);

    mcmap_region_read(0, 0, cwd);
    getchar();
    return 0;
}
