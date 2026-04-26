/**
 * @file gpio_define.h
 * @author Kulib
 * @brief 定义 12832 屏幕的 GPIO 引脚
 * @date 2026-04-22
 * */
#ifndef GPIO_DEFINE_H
#define GPIO_DEFINE_H

#define GPIO_CS   14    // 片选引脚
#define GPIO_RES  13    // 复位引脚
#define GPIO_A0   12    // 数据/命令选择引脚
#define GPIO_WR   11    // 写使能引脚
#define GPIO_RD   10    // 读使能引脚

#define GPIO_D0   3     // 数据引脚 D0
#define GPIO_D1   8     // 数据引脚 D1
#define GPIO_D2   18    // 数据引脚 D2
#define GPIO_D3   17    // 数据引脚 D3
#define GPIO_D4   16    // 数据引脚 D4
#define GPIO_D5   15    // 数据引脚 D5
#define GPIO_D6   7     // 数据引脚 D6
#define GPIO_D7   6     // 数据引脚 D7

#endif // GPIO_DEFINE_H