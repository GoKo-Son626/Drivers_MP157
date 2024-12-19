/*
 * @Date: 2024-12-01
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-12-19
 * @FilePath: /1-STM32MP157/Drivers_MP157/11_asynchronous/asynchronousApp.c
 * @Description: 
 */
#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <signal.h>

static int fd;


static void sigio_signal_func(int sig) 
{
        unsigned int key_val = 0;

        read(fd, &key_val, sizeof(unsigned int));
        if (key_val == 0) 
                printf("Key pressed\n");
        else if (key_val == 1)
                printf("Key Released\n");
}

/*
 * @description		: main
 * @param - argc 	: Num of argv
 * @param - argv 	: Specific parameter
 * @return 		: 0 succeed;other fault
 */
int main(int argc, char* argv[]){
        int flags = 0, ret;

        /* Check the number of arguments */
        if (argc != 2) {
                printf("ERROR:argc:%d\n", argc);
                return -1;
        }
        
        /* Open the device */
        if((fd = open(argv[1], O_RDONLY)) < 0) {
                printf("ERROR: %s file open failed!\n", argv[1]);
                return -1;
        }

        /* Set signal SIGIO and enable asynchronous notification */
        signal(SIGIO, sigio_signal_func);
        fcntl(fd, F_SETFL, getpid());

        flags = fcntl(fd, F_GETFD);				
        fcntl(fd, F_SETFL, flags | FASYNC);
        
        /* Read interrupt status */
        for ( ; ; ) {
                sleep(2);
        }

        /* Close the device */
        ret = close(fd);
        return 0;
}
