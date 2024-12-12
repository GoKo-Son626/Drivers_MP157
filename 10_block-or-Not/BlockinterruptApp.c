/*
 * @Date: 2024-12-01
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-12-11
 * @FilePath: /1-STM32MP157/Drivers_MP157/09_interrupt/interruptApp.c
 * @Description: 
 */
#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

/*
 * @description		: main
 * @param - argc 	: Num of argv
 * @param - argv 	: Specific parameter
 * @return 		: 0 succeed;other fault
 */
int main(int argc, char* argv[]){
        int ret, fd, key_val;

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
        
        /* Read interrupt status */
        for ( ; ; ) {
                read(fd, &key_val, sizeof(key_val));    
                if (key_val == 0) {
                        printf("ERROR: %s file read failed!\n", argv[1]);
                } else if (key_val == 1) {
                        printf("Key Release\n");
                }
        }

        /* Close the device */
        ret = close(fd);
        return 0;
}
