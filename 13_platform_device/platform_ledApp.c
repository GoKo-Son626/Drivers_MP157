/*
 * @Date: 2024-12-01
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-12-21
 * @FilePath: /1-STM32MP157/Drivers_MP157/13_platform_device/platform_ledApp.c
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
        int ret, fd;
        char *filename;
        unsigned char databuf[1];

        /* Check the number of arguments */
        if (argc != 3) {
                printf("ERROR:argc:%d\n", argc);
                return -1;
        }

        filename = argv[1];
        
        /* Open the device */
        if((fd = open(argv[1], O_RDWR)) < 0) {
                printf("ERROR: %s file open failed!\n", argv[1]);
                return -1;
        }

        databuf[0] = atoi(argv[2]);
        ret = write(fd, databuf, sizeof(databuf));
	if(ret < 0){
		printf("LED Control Failed!\r\n");
		close(fd);
		return -1;
	}

        /* Close the device */
        ret = close(fd);
        return 0;
}
