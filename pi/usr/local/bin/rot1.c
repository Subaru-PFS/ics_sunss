#include "rotat.h"

/* calculates one rotator position with both rotat and azelar. Just for
 * checking sanity and consistency. In the end, we do not use rotat.
 */

int
main( int argc, char **argv )
{
    if( argc != 3 ){
        printf("\n%s\n","USAGE: rot1 ha(deg) dec(deg)");
        exit(-1);
    }
    
    double had = atof(argv[1]);
    double decd = atof(argv[2]);
    double ha = had * PI / 180. ;
    double dec = decd * PI / 180. ;
    double rot;
    double rotr;
    azelraw(ha,dec);
    rotr = azelar[2] * 180. / PI ;
    rot = rotat(ha,dec) * 180 / PI ;
    printf("ha = %6.4f dec = %6.4f, rotr = %6.4f rot = %6.4f\n",
        had, decd, rotr, rot);
    exit (0) ;        
}



                                                                                


