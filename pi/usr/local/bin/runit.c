/* test routine for running stepper */

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rotat.h"
#include <time.h>

struct timespec treq;
struct timespec trem;

/* compile with
gcc -Wall -o runit runit.c motor.c -lwiringPi -lm  (makerunit)
 */


/* usage:
    runit
    nstep    -- how many full (4step) steps total, negative for ccw
    interval -- time between these steps in ms ( if <= 4* ontime, set to zero )
    ontime   -- time a winding is on in ms, a step takes 4 of these
*/

/* int stepping;
 * int slewing;
 * declared in motor.c 
 */
   
int stepno;
int steppos;
int enable;


int main(int argc, char **argv)
{ 

    if( argc == 1 ){
        printf("\n%s\n",
            "USAGE: runit nstep(signed) interval(ms) ontime(ms)");
        exit (-1) ;   
    }
    
    system("sudo rm -f /tmp/sunsstesting");
    system("touch /tmp/sunsstesting");

    int nstep    = atoi(argv[1]);
    int interval = atoi(argv[2]);
    int ontime   = atoi(argv[3]);
    int dir      = 1 ;
    int i;
    int t0;
    int dur;         /* real duration, ms */
    int pint= 10 ;   /* printing interval (s) */        
    int np = 0;      /* print seq number */
    int durs       ; /* time in scan (s) */
    int dp = pint;   /* time for next print (s) */
    int ns = 0;      /* number of steps, accumulating */
    int ut0 ;        /* unix time at beginning of move */
    int ute ;        /* approx unix end time */
    int tott;        /* approx total time */
    int rinterval;   /* real interval ~ interval - 4*ontime */
    int overh=372000; /* nanosec of overhead, ~380000 (0.38ms) */ 
    char limitstr[256]; /* limit status at beginning */
    int stepstolim=0; /* steps before limit is reached */
    int firstlimit=0; /* flag for first encountering limit */
    
    if(nstep < 0) dir = -1;
    
    wiringPiSetup();
    pullUpDnControl (0,PUD_OFF);  
     
    pinMode(Lim,INPUT);
    pinMode(Ap,OUTPUT);
    pinMode(Am,OUTPUT);
    pinMode(Bp,OUTPUT);
    pinMode(Bm,OUTPUT);

    ut0=(int)time(0);
    printf("\nTIME0=%d", ut0);
    printf("  %s",ctime((time_t *)&ut0));
    /* timing. If interval is less than 4*ontime, make it zero. But
     * the PERIOD is, at minimum, 4* ontime
     */
    rinterval = interval - 4*ontime;   /* wait between steps */
    if( rinterval < 0 ) rinterval = 0;

    treq.tv_sec = rinterval/1000;
    treq.tv_nsec = (rinterval % 1000) * 1000000 - overh; 
    
    if (treq.tv_nsec < 0) treq.tv_nsec = 0;

    tott = abs(nstep) * ( rinterval + 4*ontime) * 0.001;    
    if (tott < 4*ontime) tott = 4*ontime ;
    ute = ut0 + tott;  /* approx unix end time */

    printf(
        "\nRunning %d steps, ontime = %d ms, interval = %d ms for %d sec", 
        nstep, ontime, interval, tott );
    printf("\nApprox end time: %s",ctime((time_t*)&ute) );
    fflush(stdout);        
    
    enable = 1;
    set0();
    limitstr[0] = 0;
    
    t0 = millis();
    for(i=0; i < abs(nstep); i++){
        if(enable){
            if ( dir >= 0){
                stepcw(ontime);
            }else{
                stepccw(ontime);
            }
            if( digitalRead(0) == 0) {
                if( ns > 50 ){
                    alloff();
                    printf("\nHit limit at ns = %d\n", ns);
                    break;
                }else{
                    sprintf(limitstr, "On limit at beginning, ns=%d", ns);
                    /* backlash ~ 20+/-3 steps */
                }
                if( firstlimit == 0 ){
                    firstlimit = 1;
                    stepstolim++ ; /* count this step */
                }
            }
            if(firstlimit == 0) stepstolim++ ;    
            ns++ ;
            nanosleep(&treq,&trem);  /* wait between steps */
            durs=(millis() - t0)/1000 ;
            if (durs >= dp ){
                if(np == 0 && limitstr[0] != 0) printf("%s\n",limitstr);
                fprintf(stderr,"\r%d sec, %d counts",durs,ns);
                dp += pint;
                np++;
            }
            fflush(stdout);
            if(durs%300 == 0) system("sudo touch /tmp/sunsstesting");
        }
    }
    dur=millis() - t0 ;
    printf(
       "Duration %d milliseconds, %d steps, %d stepstolimit, %5.3f ms/step\n", 
            dur, ns, stepstolim, (double)dur/ns);
    alloff();
    fflush(stdout);
    system("sudo rm -f /tmp/sunsstesting");
    
}        
   
  
