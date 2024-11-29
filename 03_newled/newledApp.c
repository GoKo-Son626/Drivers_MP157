/*
 * @Date: 2024-11-11
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-11-29
 * @FilePath: /1-STM32MP157/Drivers_MP157/02_led/ledApp.c
 * @Description: 
 */

#include "stdio.h"
#include "unistd.h"
#include "fcntl.h"
#include "stdlib.h"

#define LEDOFF 	0                                                                 农民               
#define LEDON 	1

int main(int argc, char *argv[])
{
	int fd, retvalue;
	char *filename;
	unsigned char databuf[1];
	
	if(argc != 3){
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];

	/* 打开led驱动 */
	fd = open(filename, O_RDWR);
	if(fd < 0){
		printf("file %s open failed!\r\n", argv[1]);
		return -1;
	}

	databuf[0] = atoi(argv[2]);	/* 要执行的操作：打开或关闭 */

	/* 向/dev/led文件写入数据 */
	retvalue = write(fd, databuf, sizeof(databuf));
	if(retvalue < 0){
		printf("LED Control Failed!\r\n");
		close(fd);
		return -1;
	}

	retvalue = close(fd); /* 关闭文件 */
	if(retvalue < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}
	return 0;
}