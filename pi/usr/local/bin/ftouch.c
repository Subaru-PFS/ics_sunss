#include <fcntl.h>       /* AT_* constants */
#include <sys/stat.h>    /* file stats stuff */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

int
main( int argc, char** argv )
{
    int ftouch(char *);
    
    ftouch(argv[1]);
}


/* utilities */

/* updates file path -- MUST be absolute pathname; if file does not
 * exist, it is created.
 */
 

int
ftouch(char *path)
{
    int ret; 
    int fd;
    
    if( access(path, F_OK != 0)) {   /* file does not exist; create it */
        fd = creat(path,S_IWUSR|S_IRUSR);
        if(fd == -1){
            printf("Cannot create %s\n",path);
            return -1 ;
        }
        close(fd);
        ret = 0 ;
    }else{                  /* file does exist; update times */
        ret = utimensat(0,path,(struct timespec *)0,0);
        if( ret != 0){
            printf("Cannot touch %s\n", path);
            perror("FTOUCH");
            return -1 ;
        }
    }
    return ret ;
}    


                    
