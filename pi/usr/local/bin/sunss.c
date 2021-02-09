/* SuNSS rotator code */

#include <wiringPi.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>       /* AT_* constants */
#include <sys/stat.h>    /* file stats stuff */
#include <sys/types.h>
#include <unistd.h>      /* process stuff for getpid() */
#include <signal.h>

#include "rotat.h"

static void cc_handler();

/* compile with
gcc -Wall -o sunss sunss.c motor.c rotat.c -lwiringPi -lm  
or invoke the script `makesunss', which does this
 */


/* usage:
    sunss ha (deg) dec (deg) [ unixtime for valid ha ]
*/


/* these things should go in a header file ?? */

#define SIDRATE  1.002739  
    /* civil_sec * SIDRATE = sid_sec */
#define RANGE  1142      
    /* steps limit-to-limit. NB!! actually 1142-HYSTER real steps */
#define LRANGE 1200     
    /* steps which will always hit a limit */
#define HYSTER 20
    /* steps of backlash */    
#define STANDOFF 25
    /* steps away from limit (after applying HYSTER) to be `safe''
     * HYSTER + STANDOFF from the CCW limit after approaching from 
     * positive defines zero for steppos, so we set the zero at
     * 30 physical counts from the CCW limit. The zero is close to 2 degrees
     * (2.025) from the limit. Full physical range is 1142 - 20 = 1122
     * steps, 126.2 degrees. If we want to center the 120 degree range,
     * this means offsetting 3.1 degrees, 27 counts, from the lower 
     * limit. We will use 25 counts, 2.8 degrees from lower limit.
     * This puts the upper range at 122.8 degrees, 3.4 degrees, 30 steps,
     * from  upper limit. Should be OK. Should never hit the limits
     */       
#define DEGPERSTEP 0.11250
    /* degrees per step, 360/3200 */
#define STEPSPERDEG 8.888888888888888888
    /* steps per degree, 3200/360 */        
#define STEPONTIME 1
    /* 1 ms per conventional step--lengthen if necessary */
    
#if 0
note: positive for the motor code is clockwise looking at the sky. In the
north, the field rotates CCW looking at the sky. The rotator rate code 
outputs negative rates, so in principle we are OK.

Sidereal rate is 15 deg/hr, so 0.25 deg/min, 4.166666667e-3 deg/sec.
1 step = 360/3200 = 1.125e-1 deg, so sidereal rate is
3.703704e-2 steps/sec, 27 sec/step
#endif

int stepno = 0; /* total number of steps since call, either direction */
int steppos= 0; /* total signed number of steps, + for cw, - for ccw from
                 * zero, defined by STANDOFF steps from ccw limit, or
                 * STANDOFF + HYSTER steps after reaching CCW limit from
                 * above. Antibacklash moved do not increment this vbl */
double accelfac = 1.0;  /* testing accelerating factor; time passes
                         * at this rate times actual rate. Input parameter.
                         */
int dstepo  ;    /* extrapolated step to get to starting position (+/-1)
                  * at beginning; last nonzero move while running. Sign
                  * tells what direction we were moving last.
                  */


double haar[200];
double rotar[200];
double elar[200];   /* all degrees */
double rratear[200]; /* deg/hr */
int    dirar[200];  /* direction in initial probe */
char logfile[]     = "/tmp/sunsslog";
char runningfile[] = "/tmp/sunssrunning" ;
char busyfile[]    = "/tmp/sunssbusy" ;   
char gofile[]      = "/tmp/sunssgo";
char zerostr[]     = "0\n";
char onestr[]      = "1\n";
FILE *logfp;   /* file pointer to logfile */
FILE *busyfp;  /* file pointer to busyfile */
int logfd;     /* file descriptor of logfile */
FILE *runfp;   /* file pointer to runningfile */



/* compile:
 *  gcc -Wall -o sunss sunss.c motor.c rotat.c -lwiringPi -lm
 *  (makesunss)
 */

int main(int argc, char **argv)
{ 
    if( argc < 3 ){
        printf("\n%s\n",
            "USAGE: sunss ha0(deg) dec(deg) [unixtime(or 0) [ accelfac ]] ");
        exit (-1) ;   
    }

    double ha_call = atof(argv[1]);  /* ha at call time, degrees */
    double decd = atof(argv[2]);
    time_t t_0 = time(0);  /* unix time NOW; adjust a little later */
    
    time_t t_call;         /* unix time from caller for valid ha */
    if ( argc >= 4 ){   /* starting time specified at which ha is valid */
        t_call = atol(argv[3]);
        /* if arg is 0, time is NOW; do so can use acceleration factors
         * without an explicit time entry */
        if (t_call == 0) t_call = t_0;
    }else{
        t_call = t_0 ; /* just punt and assume now; in any case, t_call is OK */
    }
    if( argc  >= 5 ){  /* testing accelerator specified */
        accelfac = atof(argv[4]) ;
    }else{
        accelfac = 1.0 ;
    }

    int i;    
       
    double ha0r ;                       /*beginning hour angle in radians*/
    double decr = decd * RADPERDEG ;    /* dec in radians */
        


    time_t t_run;          /* running unix time */    
    double ha0d;           /* ha in deg at beginning of probe or run; can(will)
                            * be later than ha_call
                            */
    double had, har;       /* running ha in degrees. radians */
    double elevd;          /* elevation in deg */ 
    double elev0d;         /* beginning elev in deg */
    double rotd;           /* running rotator in deg */ 
    
    double rot0d2;         /* debug */
    int nent = 0;          /* running step number */
    double rratedh;        /* running rotator rate, deg/hr. Actually, not
                            * real rate, just diff between init and 1st deg */
    double rratess;       /* same, steps/sec rratedh*STEPSPERDEG/3600 */

    double rot0r ;      
    double rot0d ;      /* initial rotator position, radians and deg */    
    double rratedh0 ;   /* initial rotator rate, deg/hr */ 
    double rratess0 ;   /* initial rotator rate, steps/sec */
    double rotmaxd ;
    double rotmind ;    /* max and min rotator, degrees */
    double rotold;      /* rotator at previous ha degree in probe */

    /* rotator count stuff */
    int rot0c   ;    /* rotator start in steps */
    int rotc    ;    /* rotator run in count units, calculated/commanded */ 
    int rotcrun ;    /* rotator counts delta from rot0c from motor data */
    int rotminc;     /* min rotator, steps */
  
    int dstep   ;    /* how many steps this time (nearly always 0 or +/- 1) */
    
    int imax=0, imin=0 ; /* probe steps at max and min rotator angle */
    int tmax, tmin;     /* corresponding times */
    int tend;           /* elapsed time at end of probe at appropriate limit */
    int trunning;       /* elapsed time in probe or run */
    time_t t_end;       /* calculated unix end time at whatever limit*/
    int sixover = 0;    /* run ends at 6 hrs over */
    int elevlow = 0;    /* run ends when elev < 15 deg */
    int rotlim  = 0;    /* run ends when rotator hits limit */
   
    char ct_0[128];     /* string civil time at beginning */
    char ct_end[128];   /* string civil time at end */
    char buf[128];      /* scratch buffer */
    
    int pinterval = 10; /* print and make a log entry every thismany seconds 
                         * NB!!! must not exceed 59, or sunssmonitor will not
                         * keep alive and will kill this process
                         */
    int opos;            /* stepppos at last log entry */
   
#if 0
    int ret;            /* scratch flag */
    /* this should not be necessary; monitor is supposed to do this;
        maybe does now */
    if( access(gofile, F_OK ) == 0){  /* gofile (still) exists; delete it */
        ret = unlink(gofile);
        if( ret != 0){
            fprintf (logfp,"%s\n","Could not unlink gofile");
            perror ("UNLINK GOFILE");
        }
    }
#endif

    signal(SIGINT, cc_handler);

    strcpy(ct_0,ctime(&t_0));

    logfp = fopen(logfile,"a");
    logfd = fileno(logfp); 
    ftouch(logfile);        /* to make monitor happy */

    /* create busy file and write notbusy string to it */
    unlink(busyfile);
    busyfp = fopen(busyfile,"w+");
    fwrite( zerostr, 1, 2, busyfp);

    /* this stuff needs to be repeated just prior to starting the move,
     * since setup can take a few seconds; reevaluate t_0, ha0X */
     
    ha0d = ha_call + difftime(t_0, t_call)*SIDRATE/240. ;  
        /* 240s/deg, update ha to NOW; we redo this later */
    
    ha0r = ha0d * RADPERDEG ;  
    azelraw(ha0r,decr);
    rot0r = azelar[2];
    rot0d = rot0r * DEGPERRAD ;
    rot0c = (int)(rot0d * STEPSPERDEG) ;
    rotmaxd = rot0d;
    rotmind = rot0d;
    
    /* probe the path of the rotator until one of the limits (rotator
     * limit, 15 deg elevation, 6hr over) is reached to find rotator
     * range of motion
     */     
    
    
    for(i=0; i<180; i++){
        had = ha0d + i;
        har = RADPERDEG * had;
        azelraw(har, decr );
        elevd = azelar[1] * DEGPERRAD;        
        rotd = azelar[2] * DEGPERRAD;
        if ( i == 0 ){ elev0d=elevd ; rot0d2 = rotd ; }

        if ( had > 90.){ sixover=1;  break; }
        if ( elevd < 15. ) { elevlow = 1; break; }
        if( rotd > rotmaxd) { rotmaxd = rotd; imax = i; } 
        if( rotd < rotmind) { rotmind = rotd; imin = i; }
        if( fabs(rotmaxd - rotmind) > 120.){  /* back up to safety, so 
                                               * last elements in array are OK
                                               * (they are anyway),
                                               * AND max, min are OK
                                               */
            rotd = rotold;
            if(imax == i){ imax-- ; rotmaxd = rotd; }
            if(imin == i){ imin-- ; rotmind = rotd; }
            rotlim = 1; break; 
        }
        haar[i] = had;
        elar[i] = elevd;
        rotar[i] = rotd;
        rratear[i] = rratedh = rotatrate(har,decr) ;
        dirar[i] = rratedh >= 0. ? 1 : -1 ;
        rotold = rotd;
        nent++;
    }        
    /* forced end at nent *degrees* over from start, this times 240 for max 
     * time run in seconds
     */   

    /* make table */

    fprintf(logfp,
        "\n\n\nRun beginning %s\n",ct_0);
    fprintf(logfp, "\n%s\n",
    "  id  HA(d)   HA(hr)    Rot  Rate_d/h Rate_s/s dir  Elev");
    for(i=0; i<nent; i++){   
        rratess  = rratear[i] * STEPSPERDEG/3600.;
        fprintf(logfp,            
            "%4d  %5.1f  %7.3f %7.2f %7.2f %8.3f %3d %6.2f\n", i, haar[i], 
            haar[i]/15., rotar[i], rratear[i], rratess, dirar[i], elar[i]);
    }

    /* now we know the rough path of the rotator */
    tmax = 240*imax/accelfac;   /* time of rotator max */
    tmin = 240*imin/accelfac;   /* time of rotator min */
    tend = 240*nent/accelfac;   /* max elapsed time */
    t_end = t_0 + tend;  /* unix time */
    strcpy(ct_end, ctime(&t_end));
    ct_end[strlen(ct_end) -1] = 0;  /* kill newline */

    /* report */
    if(accelfac != 1.0){
        fprintf(logfp,"\n\nAcceleration factor = %3.2f\n",accelfac);
    }else{
        fputs("\n\n",logfp);
    }
        
    fprintf(logfp,
    "Beginning hour angle=%4.2f, dec=%4.2f, elev=%4.2f, rot=%4.2f %4.2f\n",
         ha0d, decd, elev0d, rot0d, rot0d2 );
    fprintf(logfp,
        "Run ends no later than %s in %4.2f hr:",ct_end, (double)nent/15. );
    if(sixover){
        fprintf(logfp,"%s\n"," Six hours over"); 
    }
    if(elevlow){
        fprintf(logfp,"%s\n"," Elev < 15 deg");
    }
    if(rotlim){
        fprintf(logfp,"%s\n"," Rotator Limit");        
    }
    fprintf(logfp,
        "rot0 = %3.1f rotmax = %3.1f in %d sec rotmin = %3.1f in %d sec\n",
        rot0d, rotmaxd, tmax, rotmind, tmin);
                
    rot0c   = rot0d   * STEPSPERDEG ;
    rotminc = rotmind * STEPSPERDEG ;

    /* setup */
    motorsetup();
    
    /* init positioning code. sets steppos to zero for
     * ccw init, to RANGE - HYSTER for cw init, to whatever - HYSTER
     * if necessary with reverses. We always run to ccw limit to begin,
     * and then move to the starting position. We should have a function
     * which does this. (move to ccw, move to start, taking into accout
     * the loss of HYSTER steps moving off the ccw limit, but move a
     * little farther.. but simple enough; we just do it here. There
     * really is no logic. Just home, and move to rotmin-rot0 and go
     */

    /* slew home */

    sslew( -1 * LRANGE,0);
    /* offset into active range */
    sslew(HYSTER + STANDOFF, 0);  /* now at zero */
    fprintf(logfp,"%s\n","Now at zero position");

#if 0   /* debug, at home position */
    fflush(logfp);
    exit(0); 
#endif

    steppos = 0;
    stepno = 0;
    sslew(rot0c - rotminc, 1);  /* slew to starting position; rotminc
                                 * is the zero position */
    fprintf(logfp, 
        "Slewed %d counts to starting position, steppos=%d\n",
            rot0c-rotminc,steppos);

#if 0  /* debug, at starting position */
    fflush(logfp);
    exit(0); 
#endif


    /* now redo timing */
    t_0 = time(0);    
    ha0d = ha_call + difftime(t_0, t_call)*SIDRATE/240. ;  
    ha0r = ha0d * RADPERDEG ;  
    azelraw(ha0r,decr);
    rot0r = azelar[2];
    rot0d = rot0r * DEGPERRAD ;
    rot0c = (int)(rot0d * STEPSPERDEG) ;
    
    rratedh0 = rotatrate(ha0r,decr);  /* initial assumed direction, so */
    dstepo = rratedh0 > 0. ? 1 : -1 ; /* extrapolated last step to get
                                       * to starting position.      
                                       * this needs to be improved--need
                                       * to figure out what NEXT step will
                                       * be. All OK unless rate is nearly
                                       * zero at start.
                                       */
                                            
    rratess0 = rratedh0*STEPSPERDEG/3600.;
    fprintf(logfp, 
        "Initial rate = %5.3f deg/hr = %5.4f steps/sec\n", rratedh0,rratess0);
   
    runfp=fopen(runningfile,"a");
    ftouch(runningfile);
    t_run = time(0);
    rewind(runfp);
    sprintf(buf,"time=%ld steppos=%d\n",t_run,steppos);
    fwrite(buf, 1, strlen(buf), runfp);
    
    /* if coming down, take up backlash */
    if (dstepo == -1) sslew(-1 * HYSTER, 0);

    strcpy(ct_0,ctime(&t_0));
    fprintf(logfp,
        "\n\n\nRun beginning %s\n",ct_0);

    fprintf(logfp,"%s\n","running");
    if( accelfac != 1.0 ) fprintf(logfp,
        "Acceleration = %2.1f, init rate = %5.3f steps/sec\n",
        accelfac, accelfac*rratess0 );
    pinterval = (double)pinterval / accelfac ;
    if (pinterval == 0) pinterval = 1 ;
    
    /* doit */
    hitlimit = 0;
    trunning = 0;
    opos = steppos;
    
    while((trunning) < tend){
        t_run = time(0);
        trunning = t_run - t_0 ;        
        if(access(runningfile,F_OK) != 0){
            fprintf(logfp,
              "Exiting at %d sec; /tmp/sunssrunning has been removed\n",
                trunning);
            squit();
            exit(0);    
        }
        /* accelfac is a test accelerator, argv[4] if it exists, 1 otherwise*/
        had = ha0d + (double)trunning*accelfac*SIDRATE/240. ;
        har = had * RADPERDEG ;
        azelraw(har, decr);        
        elevd = azelar[1] * DEGPERRAD;
        rotd = azelar[2] * DEGPERRAD;
        rotc = rotd * STEPSPERDEG ;                
        rotcrun = rotc - rotminc;    /* where are we supposed to be? */
        
        /* direction and backlash logic goes here */
        dstep = rotcrun - steppos;
        if(dstep != 0){
            if (dstepo*dstep < 0 ){
                fprintf(logfp,
                    "%s\n","REVERSING; taking up backlash");
                if(dstep > 0) slew(HYSTER);
                else slew(-1 * HYSTER);
            }
            sstep(dstep);
            dstepo=dstep;  /* now dstepo is the last nonzero move */
            if(onlimit() != 0 || hitlimit != 0){
                fprintf(logfp,
            "Hit Limit:Exiting at %d sec; /tmp/sunssrunning has been removed\n",
                trunning);
                squit();
                exit(0);    
            }
            rewind(runfp);
            sprintf(buf,"time=%ld steppos=%d\n",t_run,steppos);
            fwrite(buf, 1, strlen(buf), runfp);
        }
        if((trunning % pinterval) == 0 ){
            fprintf(logfp,        
       "%5d  ha=%7.3fd  elev=%6.3fd  rot=%7.3fd,  stp,pos: %4d %4d   del=%2d\n",
                trunning,had,elevd,rotd,rotcrun,steppos,steppos-opos);
            opos=steppos;        
        }
        fflush(stdout);
        fflush(logfp);            
        sleep(1);
    }        

    fprintf(logfp,
        "Exiting at %d sec, at limit; /tmp/sunssrunning has been removed\n",
            trunning);

    squit();
    exit(0);    
            
}

void
gohome(void)
{
    int endpos = steppos;
    int ns;
    
    if(dstepo > 0) sslew( -1*HYSTER, 0 );  /* set up for negative move */
    /* we should be steppos + STANDOFF  out from ccw limit at this point,
     * moving -  */
    ns = sslew(-1*LRANGE,1);  /* go to CCW limit */
    fprintf(logfp,"Predicted steps from CCW limit = %d, measured = %d\n",
        (endpos + STANDOFF), -1*ns );
    sslew(HYSTER+STANDOFF,0);
    fprintf(logfp,"%s\n","At home");
}


/* utilities */

void
squit( void )
{
    unlink(runningfile);
    unlink(gofile);   /* jic */
    alloff();
    sleep(3);
    gohome();
    fflush(stdout);
    fflush(logfp);
    system("/usr/local/bin/logback");
}                           

/******************* FTOUCH() ***************************************/
/* updates file path -- MUST be absolute pathname and file MUST exist
 * and be writeable for the current user 
 */
int
ftouch(char *path)
{
    int ret; 
    int fd;
    
    if( access(path, F_OK != 0)) {   /* file does not exist; create it */
        fd = creat(path,S_IWUSR|S_IRUSR);
        if(fd == -1){
            fprintf(logfp,"Cannot create %s\n",path);
            return -1 ;
        }
        close(fd);
        ret = 0 ;
    }else{                  /* file does exist; update times */
        ret = utimensat(0,path,(struct timespec *)0,0);
        if( ret != 0){
            fprintf(logfp,"Cannot touch %s\n", path);
            perror("FTOUCH");
            return -1 ;
        }
    }
    return ret ;
}    

#if 0

/* these are not useful here */
/******************* FDTOUCH() ***************************************/
/* updates file by descriptor -- MUST be open, valid
 * and be writeable for the current user 
 */
int
fdtouch(int fd)
{
    int ret; 
    
    if(fcntl(fd,F_GETFD) == -1){
       fprintf(logfp, "%s\n","Invalid file descriptor");
       return -1;
    }else{         /* fd is valid; update times */
        ret = futimens(fd,(struct timespec *)0);
        if( ret != 0){
            fprintf(logfp, "Cannot touch fd=%d\n", fd);
            perror("FDTOUCH");
            return -1 ;
        }
    }
    return ret ;
}    

/******************* FPTOUCH **********************************************/
/* updates file by pointer -- MUST be open, valid
 * and be writeable for the current user 
 */
int
fptouch(FILE *fp)
{
    int ret; 
    int fd;
    
    if( (fd =fileno(fp)) == -1){
       fprintf(logfp,"%s\n","Invalid file pointer");
       return -1;
    }else{         /* fd is valid; update times */
        ret = futimens(fd,(struct timespec *)0);
        if( ret != 0){
            fprintf(logfp,"Cannot touch fp=%p\n", (void *)fp);
            perror("FPTOUCH");
            return -1 ;
        }
    }
    return ret ;
}    

#endif

/********************* MAKEBUSY() *****************************************/
/* creates busyfile and returns its file descriptor. If the file
 * exists, close it and recreate. This is not thread-safe, but we are OK.
 * Since the only functions which create the busy file are sslew and sstep,
 * and they both open and close it, there should not be a problem.
 */
 
int makebusy()
{
    int ret;

    rewind(busyfp);
    ret = fwrite(onestr,1,2,busyfp);
    fflush(busyfp);
    if( ret == 0 ) fprintf(logfp,"MAKEBUSY:Cannot write %d to busyfile", 1);
    return ret;
}    

int unbusy()
{
    int ret;

    rewind(busyfp);
    ret = fwrite(zerostr,1,2,busyfp);
    fflush(busyfp);
    if( ret == 0 ) fprintf(logfp,"UNBUSY:Cannot write %d to busyfile", 0);
    return ret;
}    

/****************** SSLEW(), SSTEP() ********************************/
/* these functions are like slew() and stepcw, stepccw() in motor.c,
 * but the step function takes a signed number (expected to be 1 or -1,
 * but cope if greater or zero, and both turn on the busy file while working
 * and sstep increments stepno and steppos--sslew does NOT!
 * the monitor freaks out if the busyfile exists and is too old
 * (half a minute) and shuts things down by deleting the runfile and
 * posting a log message )
 */
 
/* 
 * this function slews the motor. nstep is a signed quantity, and
 * stepno is incremented or not according to the value of rflg;
 * rflg= 0 is `raw'; no position bookeeping, used for backlash
 * compensation. nstep is always incrementd. Any nonzero value
 * increments steppos. Note that slew() is aborted if a limit is
 * encountered, and the global hitlimit is set. The ontime is
 * in motor.c and is 1 ms for slew()
 */

int sslew( int nstep, int rflg )
{
    int ns ; /* actual number of steps */
    if( nstep == 0 ) return 0 ;  /* do nothing */    
    (void)makebusy();
    ns = slew(nstep);
    if(rflg != 0) steppos += ns; 
    stepno += abs(ns);
    (void)unbusy();
    return ns;
}

/*
 * This function takes (signed) nstep steps 
 */

int sstep( int nstep )
{
    int i;
    int dir = 1;
    int ns = 0;
    
    if(nstep == 0) return 0 ;  /* do NOTHING */
 
    (void)makebusy();  /* create busy file */
      
    if(nstep < 0){  /* ccw */
        dir = -1;
        nstep = abs(nstep);
    }
    for(i=0; i < nstep; i++){
        if(dir > 0){
            stepcw(STEPONTIME);
        }else{
            stepccw(STEPONTIME);
        }
        ns++ ;
        if( onlimit()){
            fprintf(logfp,"Hit Limit at %d steps, steppos = %d\n",ns,steppos);
            hitlimit=1;
            break ;
        }
    }
    (void)unbusy();
    stepno += abs(ns);       
    steppos += dir*ns;
    return ns;
}

/********************** cc_handler **********************************/

static void cc_handler()
{
    puts( "Exiting with ^C interrupt");
    fputs( "Exiting with ^C interrupt", logfp);
    unlink(runningfile);
}

/*************************************************************************/
