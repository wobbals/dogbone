//
//  pup_dogbone.h
//  pup
//
//  Created by Charley Robinson on 3/11/17.
//
//

#ifndef pup_dogbone_h
#define pup_dogbone_h

/**
 * Maintains a connection to the dogbone IPC process
 */

#include <stdio.h>
struct pup_dogbone_s;

int pup_dogbone_alloc(struct pup_dogbone_s** dogbone);
void pup_dogbone_free(struct pup_dogbone_s* dogbone);

#endif /* pup_dogbone_h */
