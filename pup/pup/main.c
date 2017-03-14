//
//  main.c
//  pup
//
//  Created by Charley Robinson on 3/11/17.
//
//

#include "unistd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pup_dogbone.h"


int main(int argc, const char * argv[]) {
    struct pup_dogbone_s* dogbone;
    pup_dogbone_alloc(&dogbone);

    while (1) {
        sleep(1);
    }

    return 0;
}
