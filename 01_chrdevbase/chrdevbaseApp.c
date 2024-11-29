/*
 * @Date: 2024-11-10
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-11-29
 * @FilePath: /1-STM32MP157/Drivers_MP157/01_chrdevbase/chrdevbaseApp.c
 * @Description: 
 */
#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

int main(int argc, char *argv[])
{
        int fd, retvalue;

        char *filename;

        if (argc != 2)
        {
                printf("Error Usage\r\n");
                return -1;
        }
        
        filename = argv[1];

        /* 打开字符设备 */
        fd = open(filename, O_RDWR);
        if (fd < 0)
        {
                printf("open %s failed\r\n", filename);
                return -1;
        }

        /* 向字符设备写入数据 */
        retvalue = write(fd, "Hello, World!\n", 14);
        if (retvalue < 0)
        {
                perror("write");
                close(fd);
                return -1;
        }
        printf("Write data to character device successfully\r\n");

        /* 关闭字符设备 */
        retvalue = close(fd);
        if (retvalue < 0)
        {
                printf("close failed\r\n");
        }
        
        return 0; 
}