#include <wiringPi.h>
#include "rotat.h"
  
/* Connector PT0(2,6)A-10-6:
 * pin A is gnd/com, winding A- blue motor wire and B- black motor wires
 * pin B is winding A+ red motor wire
 * pin F is winding B+ green motor wire
 * Pin D is lim
 * Pins C, E NC
 */
 

/* wiringPi port names 
 * note that the system works by manipulating two ports
 * for each motor winding. A high is Ap high Am low; A low
 * is Ap low, Am high. Ap=Am is off, whether both low or both
 * high, though both high is never an assigned state. Same
 * for B. The signals are sent to a pair of  differential power amplifiers
 * to generate the motor drive with a gain of about 3.9. The motor
 * windings are nominally 10 ohms. There are 8 ohm 5 watt wirewound
 * resistors on the driver board, and the wire is about 2 ohms, so
 * roughly 20 ohms. 11V drive -> ~550ma per phase
  */

int hitlimit = 0;
int stepping = 0;
int slewing  = 0;

/* set up the GPIOs */
void motorsetup()
{
    wiringPiSetup();
    pullUpDnControl (0,PUD_OFF);

    pinMode(Lim,INPUT);
    pinMode(Ap,OUTPUT);
    pinMode(Am,OUTPUT);
    pinMode(Bp,OUTPUT);
    pinMode(Bm,OUTPUT);
    hitlimit = 0;
    stepping = 0;
    slewing  = 0;
}
                                       


/* turn everything off -- default state */
void alloff()
{
    digitalWrite(Ap,LOW);
    digitalWrite(Am,LOW);
    digitalWrite(Bp,LOW);
    digitalWrite(Bm,LOW);
    hitlimit = 0;
    stepping = slewing = 0;
}

/* set all the ports to input value: 0 or 1 for Ap, Am, Bp, Bm in a 4-int array 
 * just for sanity checking
 */
 
void allset(int* pvar) 
{
    printf ("Setting PORTS: Ap %d Am %d Bp %d Bm %d\n",
        pvar[0], pvar[1], pvar[2], pvar[3]);
    digitalWrite(Ap,pvar[0]);
    digitalWrite(Am,pvar[1]);
    digitalWrite(Bp,pvar[2]);
    digitalWrite(Bm,pvar[3]);
    
    printf ("Reading PORTS: Ap %d Am %d Bp %d Bm %d\n",
        digitalRead(Ap),
        digitalRead(Am),
        digitalRead(Bp),
        digitalRead(Bm) );
}

void ahi()
{
    digitalWrite(Ap,HIGH);
    digitalWrite(Am,LOW);
}

void alo()
{
    digitalWrite(Am,HIGH);
    digitalWrite(Ap,LOW);
}

void bhi()
{
    digitalWrite(Bp,HIGH);
    digitalWrite(Bm,LOW);
}


void blo()
{
    digitalWrite(Bm,HIGH);
    digitalWrite(Bp,LOW);
}


/* this sets the motor to the nearest magnetically stable state, where
 * we start and end each step (4 full steps)
 */
 
void set0()
{
    ahi();
    blo();
}

int onlimit()
{
    return ( digitalRead(Lim) == 0 ? 1 : 0 );
}

/* this does one full clockwise (motor, looking at shaft) 4-step, 
 * ending on a magnetically stable state 
 */

void stepcw(int ival)
{
    stepping = 1;
    ahi();
    bhi();
    delay(ival);               
    alo();
    delay(ival);
    blo();
    delay(ival);
    ahi();
    delay(ival);
    alloff();
    stepping = 0;
}

/* this does one full counterclockwise (motor, looking at shaft) 4-step, 
 * ending on a magnetically stable state 
 */
 
void stepccw(int ival)
{
    stepping = 1;
    blo();
    alo();
    delay(ival);               
    bhi();
    delay(ival);
    ahi(); 
    delay(ival);
    blo();
    delay(ival);
    alloff(); 
    stepping = 0;
}

/* this is a little tricky. Starting on a limit is problematical because
 * of the backlash, and the limit contact can come and go during the first
 * HYSTER steps, until you actually move clear. So we do not pay
 * attention to the moves which move us off a limit; really only the
 * first move to get to the origin, and this routine returns the number
 * of steps NOT on a limit assuming that you hit the limit, not
 * moving away from it.
 */
 
int nslewtot;

int slew(int nstep)    
{            
    int ontime = 1;
    int dir = nstep > 0 ? 1 : -1 ;
    int i;
    
    nstep = abs(nstep); 
    int ns = 0;
    int nsl = 0;
    slewing = 1;
    int firstlimit = 0;
    hitlimit = 0;

    if( nstep > 3200 ) nstep = 3200; /* go not more than 1 rev */
    set0();
    for(i=0; i<nstep; i++){
        if ( dir >= 0){
            /* step cw */
            ahi();  
            bhi();
            delay(ontime);               
            alo();
            delay(ontime);
            blo();
            delay(ontime);
            ahi();
            delay(ontime);
            alloff();
        }else{  
            /* step ccw */
            blo();
            alo();
            delay(ontime);               
            bhi();
            delay(ontime);
            ahi(); 
            delay(ontime);
            blo();
            delay(ontime);
            alloff(); 
        }
        ns++ ;
        /* allow 45 steps to get off limit, about 5 degrees */ 
        if( digitalRead(0) == 0 ){  /* on or hit limit */
            if( ns > 45 ){
                alloff();
                printf("%s\n","Hit limit");
                hitlimit = 1;
                break;
            }
            if( firstlimit == 0){
                firstlimit = 1;
                nsl++ ; /* count this step */
            }
        }
        /* count the step as a 'real' step if limit has not been hit */
        if(firstlimit == 0) nsl++ ;                    
        ns++;
    }
    slewing = 0;   
    nslewtot = ns;
    return dir*nsl;  
}           
