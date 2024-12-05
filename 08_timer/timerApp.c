/*
 * @Date: 2024-12-01
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-12-05
 * @FilePath: /1-STM32MP157/Drivers_MP157/08_timer/driverApp.c
 * @Description: 
 */
#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "sys/ioctl.h"

#define CLOSE_CMD 		(_IO(0XEF, 0x1))	/* 关闭定时器 */
#define OPEN_CMD		(_IO(0XEF, 0x2))	/* 打开定时器 */
#define SETPERIOD_CMD	        (_IO(0XEF, 0x3))	/* 设置定时器周期命令 */
/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 		: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd, ret;
	char *filename;
	// unsigned char databuf[1];
        unsigned int cmd;
        unsigned int arg;
        unsigned char str[100];
	
	if(argc != 2){
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];

	/* 打开led驱动 */
	fd = open(filename, O_RDWR);
	if(fd < 0){
		printf("file %s open failed!\r\n", argv[1]);
		return -1;
	} else {
                printf("file %s open success!\r\n", argv[1]);
        }

	// databuf[0] = atoi(argv[2]);	/* 要执行的操作：打开或关闭 */

        /* 循环读取 */
        while (1) {
                printf("Input cmd:");
                ret = scanf("%d", &cmd);
                if (ret != 1) {
                        fgets(str, sizeof(str), stdin);
                }
                
                if (cmd == 1) {
                        ioctl(fd, CLOSE_CMD, &arg);
                } else if(cmd == 2) {
                        ioctl(fd, OPEN_CMD, &arg);
                } else if(cmd == 3) {
                        printf("Input Timer Period:");
                        ret = scanf("%d", &arg);
                        if (ret!= 1) {
                                fgets(str, sizeof(str), stdin);
                        }
                        ioctl(fd, SETPERIOD_CMD, &arg);
                } else if (cmd == 4) {
                        goto out;
                }
                

        }
        
out:
	ret = close(fd); /* 关闭文件 */
	if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}
	return 0;
}
