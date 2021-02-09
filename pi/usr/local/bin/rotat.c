/* This has rotator routines to calculate position and rate */

#include "rotat.h"

#if 0 /* table from rottab */

                    ROTATOR RATE (deg/hr)
                       hour angle (hr)
dec    0.0    0.5    1.0    1.5    2.0    2.5    3.0    3.5    4.0
-45   15.6   15.5   15.1   14.6   13.9   13.2   12.4   11.8   11.2
-35   17.3   17.0   16.3   15.3   14.2   13.0   11.8   10.8    9.9
-25   20.0   19.6   18.2   16.4   14.5   12.6   11.0    9.6    8.4
-15   24.7   23.7   21.0   17.7   14.5   11.8    9.7    8.0    6.6
 -5   33.7   30.9   24.7   18.5   13.6   10.1    7.5    5.7    4.4
  5   55.4   44.2   27.3   16.3   10.1    6.5    4.2    2.7    1.6
 15  169.7   50.7   15.1    5.9    2.4    0.6   -0.4   -1.1   -1.6
 25 -154.8  -58.2  -21.9  -12.1   -8.4   -6.6   -5.8   -5.3   -5.1
 35  -53.7  -45.8  -32.3  -22.4  -16.4  -12.9  -10.7   -9.4   -8.5
 45  -33.1  -31.5  -27.6  -23.2  -19.3  -16.3  -14.1  -12.5  -11.5
 55  -24.5  -24.0  -22.7  -21.0  -19.1  -17.3  -15.8  -14.6  -13.6
 65  -19.9  -19.7  -19.3  -18.6  -17.9  -17.0  -16.2  -15.5  -14.8
 75  -17.2  -17.1  -17.0  -16.8  -16.5  -16.2  -15.9  -15.6  -15.3
 85  -15.5  -15.5  -15.5  -15.5  -15.4  -15.4  -15.3  -15.3  -15.2

#endif                                                      

/* astronomy stuff */


#define LATMKD 19.825814 
#define LATMK  0.346026
#define SLATMK 0.339162
#define CLATMK 0.940728
#define LONGMKD -155.476455
#define LONGMK -2.713576
#define ALTMK 4205.
#define ATMPMK  605.0
#define ATMTMK  271.

double latobsd = LATMK;
double latobs = LATMK;
double slatobs = SLATMK;
double clatobs = CLATMK;


double azelar[3]; /* az, alt, rot (rad) */

/* takes radian arguments, populates azelar with radian vales */
void
azelraw(double h, double dec)
{   
    double cos_h = cos(h);
    double sin_h = sin(h);   
    double cosdel = cos(dec);
    double sindel = sin(dec);
    double cosdeln = cos(dec + .0001);
    double sindeln = sin(dec + .0001);
    double rot;

    double zp = cosdel*cos_h*clatobs + sindel*slatobs;
    double xp = cosdel*sin_h;
    double yp = cosdel*cos_h*slatobs - sindel*clatobs;

    double zpn = cosdeln*cos_h*clatobs + sindeln*slatobs;
    double xpn = cosdeln*sin_h;
    double ypn = cosdeln*cos_h*slatobs - sindeln*clatobs;
    
    double azeldn[2];

    azelar[0] = atan2(-xp,yp);
    azelar[1] = atan2(zp,sqrt(xp*xp + yp*yp));
    azeldn[0] = atan2(-xpn,ypn);
    azeldn[1] = atan2(zpn,sqrt(xpn*xpn + ypn*ypn));
    rot = -atan2((azeldn[0] - azelar[0])*cos(azelar[1]), azeldn[1]-azelar[1]);
    /* this to make the rot continuous across h=0 for high-dec sources */
    if( h <= 0. && dec > latobs ) rot += 2.* PI;        
    azelar[2] = rot ;        
    return ;  
}


/* the increment to do the derivative is fine here, accurate to
 * ~1e-4 deg, ~0.3 arcsec
 */

/* only does rotator, use for running app. radian args and result */ 
double rotat(double h, double dec)
{
    double cos_h = cos(h);
    double sin_h = sin(h);   
    double cosdeln = cos(dec + .0001);
    double sindeln = sin(dec + .0001);
    double cosdelu = cos(dec - .0001);
    double sindelu = sin(dec - .0001);


    double zpn = cosdeln*cos_h*clatobs + sindeln*slatobs;
    double xpn = cosdeln*sin_h;
    double ypn = cosdeln*cos_h*slatobs - sindeln*clatobs;
    
    double zpu = cosdelu*cos_h*clatobs + sindelu*slatobs;
    double xpu = cosdelu*sin_h;
    double ypu = cosdelu*cos_h*slatobs - sindelu*clatobs;
    
    double azeldn[2];
    double azelun[2];
    
    double rot;

    azelun[0] = atan2(-xpu,ypu);
    azelun[1] = atan2(zpu,sqrt(xpu*xpu + ypu*ypu));
    azeldn[0] = atan2(-xpn,ypn);
    azeldn[1] = atan2(zpn,sqrt(xpn*xpn + ypn*ypn));
    rot = -atan2((azeldn[0] - azelun[0])*cos(0.5*(azelun[1]+azeldn[1])),
        azeldn[1]-azelun[1]);
    /* this to make the rot continuous across h=0 for high-dec sources */        
    if( h <= 0. && dec > latobs ) rot += 2.* PI;
    return rot ;  
}

#define ROTSTEP 509.29584      /* 4steps/rad; 3200/2pi */
#define HA_MS   7.2722052e-08  /* radians/ms in time   */

/* calculates rotator position in steps, input ha in ms, dec in decimal deg */

int 
rotatsd ( int hams, double decd )
{
    double dec = decd * PI/180. ;  /* dec in radians */
    double ha  = (double)hams * HA_MS ;
    double rot = rotat(ha, dec);
    return (int)(rot*ROTSTEP + 0.5);
}

/* this routine returns rate in **deg/hr** */
double 
rotatrate( double ha, double dec )
{
    double delha = .001;  /* radians */
    double dt = 2.* delha * 12./PI ;  /* hours; centered derivative */
    double rotd1 = rotat(ha - delha,dec) * 180. / PI ;
    double rotd2 = rotat(ha + delha,dec) * 180. / PI ;
    double rotr = rotd2;
    return rotr = (rotd2 - rotd1)/dt ;
}

