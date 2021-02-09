/* sets  all the pins to the proferred values. Must have 4 int parameters */

/* cc allset.c motor.c -oallset -lwiringPi */ 

#include "rotat.h"
#include <wiringPi.h>
extern void allset(int *);
int stepping;
int stepno;
int steppos;
int slewing;


int
main(int argc, char **argv )
{
    int pstate[4];

    motorsetup();
    
    pstate[0] = atoi(argv[1]);
    pstate[1] = atoi(argv[2]);
    pstate[2] = atoi(argv[3]);
    pstate[3] = atoi(argv[4]);
    
    allset(pstate);
 
}
