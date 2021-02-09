/*
 * this routine makes a table of rotator pos for positive and negative ha,
 *  and a rate table.
 */
 
#include "rotat.h"



int
main(int argc, char **argv )
{
    
    double hah ; 
    double decd ;
    double ha ;
    double dec ;
    double rotd ;
    double rotd1 ;
    double rotd2 ;
    double delha = .001;  /* radians, for calculating rot rate as 
                           * centered diff between rot pos at ha + delha and
                           * ha - delha
                           */ 
    double dt = 2.* delha * 12./PI ;
    double ha0 = 0;
    double ha1 = 4.;
    double dha = 0.5 ;
    double dec0 = -45.;
    double dec1 = 85.;
    double ddec = 10.;
    int ndec = (dec1 - dec0)/ddec + 1;
    int nha = (ha1 - ha0)/dha + 1;


    double rotr ;
    int i, j ;
    
    if (argc == 1){
        ha0  = 0.0;
        ha1  = 4.0;
        dha  = 0.5;
        dec0 =-45.;
        dec1 = 85.;
        ddec = 10.;
    }else{
        ha0  = atof(argv[1]);
        ha1  = atof(argv[2]);
        dha  = atof(argv[3]);
        dec0 = atof(argv[4]);
        dec1 = atof(argv[5]);
        ddec = atof(argv[6]);
        ndec = (dec1 - dec0)/ddec + 1;
        nha = (ha1 - ha0)/dha + 1;
    }
                 
    
    printf("\n%s","                 ROTATOR(deg) ");
    printf("\n%s","               hour angle (hr) ");
    printf("\n%s","dec");
    for(i = 0; i < nha; i++){
        printf("%7.2f",ha0 + i*dha); 
    }

    for(j=0; j < ndec ; j++){
        decd = dec0 + (double)j * ddec ;
        dec = decd * PI / 180. ;
        printf("\n%3.0f",decd) ;
        for(i=0; i < nha; i++){
            hah = ha0 + dha * (double)i ;
            ha = hah * PI / 12. ;
            rotd = rotat(ha,dec) * 180. / PI ;
            printf("%7.1f",rotd);
        }
    }
    puts("\n");

    /* if ha0 is meridian, calculate for negative hour  angles */
    if( ha0 == 0. ){
        printf("\n%s","                 ROTATOR(deg) ");
        printf("\n%s","               hour angle (hr) ");
        printf("\n%s","dec");
        for(i = 0; i < nha; i++){
            printf("%7.2f", -i*dha); 
        }
    
        for(j=0; j < ndec ; j++){
            decd = dec0 + (double)j * ddec ;
            dec = decd * PI / 180. ;
            printf("\n%3.0f",decd) ;
            for(i=0; i < nha; i++){
                hah = -dha * (double)i ;
                ha = hah * PI / 12. ;
                rotd = rotat(ha,dec) * 180. / PI ;
                printf("%7.1f",rotd);
            }
        }
        puts("\n");
    }

    /* calculate and tabulate rotator rate, using centered
     * difference +/- delha, currently 1 milliradian
     */
    printf("\n%s","            ROTATOR RATE(deg/hr) ");
    printf("\n%s","               hour angle (hr) ");
    printf("\n%s","dec");
    for(i = 0; i < nha; i++){
        printf("%7.2f",ha0 + i*dha); 
    }

    for(j=0; j < ndec ; j++){
        decd = dec0 + (double)j * ddec;
        dec = decd * PI / 180. ;
        printf("\n%3.0f",decd) ;
        for(i=0; i < nha; i++){
            hah = ha0 + dha * (double)i ;
            ha = hah * PI / 12. ;
            rotd1 = rotat(ha - delha,dec) * 180. / PI ;
            rotd2 = rotat(ha + delha,dec) * 180. / PI ;
            rotr = rotd2;
            rotr = (rotd2 - rotd1)/dt ;
            printf("%7.1f",rotr);
        }
    }
    puts("\n");
    
    exit (0) ;        
}



