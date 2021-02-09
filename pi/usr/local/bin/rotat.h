/* header file, contains astronomy quantities from rotat.c and motor
 * quantities from motor.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>

#define PI    3.14159265358979323846
#define RADPERDEG 1.745329252e-02
#define DEGPERRAD 57.295779513082320876

/* these are wiringpi pin designations */
/* Ahigh = Ap & !Am; Alow = !Ap & Am  */
#define Ap 5
#define Am 6

/* Bhigh = Bp & !Bm ; Blow = !Bp & Bm */
#define Bp 2
#define Bm 3

/* Lim is an input. onlimit = !Lim */
#define Lim 0

/* astronomy stuff from rotat.c */

extern double rotat( double h, double dec );
extern double rotatrate( double h, double dec );
extern void   azelraw( double h, double dec );
/*extern int    rotatsd ( int hams, double decd  );*/

extern double azelar[3] ;

/* from motor.c */
extern void motorsetup( void );
extern void stepcw( int ival );
extern void stepccw( int ival );
extern int  slew( int nstep );
extern void set0( void );
extern void alloff( void );
extern int  onlimit( void );

extern int slewing ;
extern int stepping ;
extern int hitlimit ;

extern int stepno ;
extern int steppos ;
extern int nslewtot ;

/* from sunss.c */
extern int ftouch( char * ) ;

extern int makebusy( void ) ;
extern int unbusy( void )   ;
extern int sslew( int, int ) ;
extern int sstep( int ) ;
extern void squit( void ) ; 
extern void gohome( void ) ;
