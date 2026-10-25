#ifndef __BSP_BUTTON_H__
#define __BSP_BUTTON_H__

#include <stdint.h>

/* 默认扫描频率与时间换算宏定义 */
#define BSP_BTN_SCAN_FREQ_HZ        50  /* 扫描频率: 50Hz (即每 20ms 扫描一次) */
#define BSP_BTN_TICKS(ms)           ((uint16_t)((ms) / (1000 / BSP_BTN_SCAN_FREQ_HZ)))

/**
 * @brief 按键触发事件类型枚举
 */
typedef enum
{
    BSP_BTN_EVT_NONE = 0,           /* 无事件 */
    BSP_BTN_EVT_DOWN,               /* 按键按下事件 */
    BSP_BTN_EVT_CLICK,              /* 单击释放事件 */
    BSP_BTN_EVT_DOUBLE_CLICK,       /* 双击释放事件 */
    BSP_BTN_EVT_REPEAT_CLICK,       /* 多次连击事件 */
    BSP_BTN_EVT_LONG_START,         /* 达到长按时间阈值触发 */
    BSP_BTN_EVT_LONG_HOLD,          /* 达到长按保持时间阈值触发 */
    BSP_BTN_EVT_LONG_UP,            /* 长按后松开抬起事件 */
    BSP_BTN_EVT_MAX                 /* 事件最大边界值 */
} bsp_btn_event_t;

/**
 * @brief 按键内部有限状态机阶段枚举
 */
typedef enum
{
    BSP_BTN_STATUS_DEFAULT = 0,      /* 空闲阶段 (按键未按下) */
    BSP_BTN_STATUS_DOWN,             /* 按下确认与计时阶段 */
    BSP_BTN_STATUS_WAIT_MULTIPLE     /* 抬起等待连击间隔阶段 */
} bsp_btn_status_t;

/* 按键结构体前置声明 */
typedef struct bsp_button_s bsp_button_t;

/**
 * @brief 按键事件回调函数原型
 * @param btn 指向触发该事件的按键对象结构体指针
 */
typedef void (*bsp_btn_cb_t)(bsp_button_t *btn);

/**
 * @brief 按键对象控制块结构体
 */
struct bsp_button_s
{
    /* --- 硬件抽象与回调函数 --- */
    uint8_t (*read_pin)(void *arg);       /* 读取 GPIO 电平函数指针，返回 0 或 1 */
    bsp_btn_cb_t callback;                /* 事件响应回调函数 */
    void *user_data;                      /* 用户私有数据指针 (例如传入引脚编号或句柄) */

    /* --- 用户配置阈值参数 (单位: 扫描周期计数 ticks) --- */
    uint16_t short_ticks;                 /* 短按有效消抖阈值 */
    uint16_t long_ticks;                  /* 判定为长按起始的阈值 */
    uint16_t long_hold_ticks;             /* 判定为长按保持的阈值 */
    uint16_t multi_click_ticks;           /* 连击最大等待间隔阈值 */
    uint8_t  active_level;                /* 按下时的有效电平 (0: 低电平有效, 1: 高电平有效) */

    /* --- 驱动内部运行状态 (无需外部手动修改) --- */
    uint16_t tick_cnt;                    /* 内部扫描周期计数器 */
    uint8_t  click_cnt;                   /* 连击次数记录器 */
    uint8_t  status;                       /* 内部状态 (取值于 bsp_btn_status_t) */
    uint8_t  event;                       /* 当前触发的事件 (取值于 bsp_btn_event_t) */

    /* --- 链表节点指针 --- */
    struct bsp_button_s *next;            /* 指向下一个已注册的按键 */
};

/* --- 核心驱动接口 --- */

/**
 * @brief  注册按键对象至全局按键链表
 * @param  btn 指向用户分配的按键结构体实例
 * @return 0: 成功; -1: 失败 (参数为空或按键已存在)
 */
int32_t bsp_button_register(bsp_button_t *btn);

/**
 * @brief  获取指定按键当前记录的事件
 * @param  btn 指向按键结构体实例
 * @return 当前触发的按键事件 (bsp_btn_event_t)
 */
bsp_btn_event_t bsp_button_get_event(const bsp_button_t *btn);

/**
 * @brief  按键周期扫描核心函数
 * @note   需要用户在定时器中断或周期任务中定期调用 (例如每 20ms 一次)
 */
void bsp_button_scan(void);


#endif /* __BSP_BUTTON_H__ */