#include "bsp_button.h"


/* 全局链表头指针，指向首个已注册的按键 */
static bsp_button_t *s_btn_head = NULL;

/**
 * @brief  内部辅助函数：读取按键并判断是否处于有效按下状态
 * @param  btn 指向目标按键对象
 * @return 1: 按键已按下; 0: 按键未按下
 */
static inline uint8_t bsp_button_is_pressed(bsp_button_t *btn)
{
    /* 若用户未传入读取函数指针，默认判定为未按下 */
    if (btn->read_pin == NULL)
    {
        return 0;
    }

    /* 读取物理引脚电平 */
    uint8_t pin_level = btn->read_pin(btn->user_data);

    /* 比较引脚电平与有效电平，相等即判定为按下 */
    return (pin_level == btn->active_level) ? 1 : 0;
}

/**
 * @brief  内部辅助宏：更新事件并执行回调函数
 * @param  btn 指向目标按键对象
 * @param  evt 需要触发的新事件
 */
#define BSP_BTN_TRIGGER_EVENT(btn, evt)      \
    do {                                     \
        (btn)->event = (evt);                \
        if ((btn)->callback != NULL) {       \
            (btn)->callback(btn);            \
        }                                    \
    } while (0)

/**
 * @brief  注册按键对象至全局链表
 * @param  btn 指向按键结构体实例
 * @return 0: 成功; -1: 参数非法或按键已在链表中
 */
int32_t bsp_button_register(bsp_button_t *btn)
{
    /* 参数校验 */
    if (btn == NULL || btn->read_pin == NULL)
    {
        return -1;
    }

    /* 查重校验，避免重复注册导致链表死循环 */
    bsp_button_t *curr = s_btn_head;
    while (curr != NULL)
    {
        if (curr == btn)
        {
            return -1; /* 按键已存在于链表中 */
        }
        curr = curr->next;
    }

    /* 初始化运行期动态数据 */
    btn->tick_cnt  = 0;
    btn->click_cnt = 0;
    btn->status     = BSP_BTN_STATUS_DEFAULT;
    btn->event     = BSP_BTN_EVT_NONE;

    /* 头插法加入全局单向链表 */
    btn->next  = s_btn_head;
    s_btn_head = btn;

    return 0;
}

/**
 * @brief  获取指定按键当前记录的事件
 * @param  btn 指向按键结构体实例
 * @return 当前触发的按键事件
 */
bsp_btn_event_t bsp_button_get_event(const bsp_button_t *btn)
{
    if (btn == NULL)
    {
        return BSP_BTN_EVT_NONE;
    }

    return (bsp_btn_event_t)(btn->event);
}

/**
 * @brief  单个按键的状态机处理函数
 * @param  btn 指向目标按键对象
 */
static void bsp_button_process(bsp_button_t *btn)
{
    /* 1. 读取当前物理引脚状态 (1: 按下, 0: 松开) */
    uint8_t is_pressed = bsp_button_is_pressed(btn);

    /* 2. 状态机流转逻辑 */
    switch (btn->status)
    {
    case BSP_BTN_STATUS_DEFAULT:
        /* 空闲阶段：检测到按下信号 */
        if (is_pressed)
        {
            btn->tick_cnt  = 0;
            btn->click_cnt = 0;
            btn->status     = BSP_BTN_STATUS_DOWN;

            /* 触发按下事件 */
            BSP_BTN_TRIGGER_EVENT(btn, BSP_BTN_EVT_DOWN);
        }
        else
        {
            btn->event = BSP_BTN_EVT_NONE;
        }
        break;

    case BSP_BTN_STATUS_DOWN:
        /* 按下计时阶段 */
        if (is_pressed)
        {
            btn->tick_cnt++;

            /* 防溢出限制 */
            if (btn->tick_cnt >= 0xFFFE)
            {
                btn->tick_cnt = btn->long_hold_ticks;
            }

            /* 检测长按保持事件 (只触发一次或周期触发，此处在刚好达到阈值时触发) */
            if (btn->tick_cnt == btn->long_hold_ticks)
            {
                BSP_BTN_TRIGGER_EVENT(btn, BSP_BTN_EVT_LONG_HOLD);
            }
            /* 检测长按开始事件 */
            else if (btn->tick_cnt == btn->long_ticks)
            {
                BSP_BTN_TRIGGER_EVENT(btn, BSP_BTN_EVT_LONG_START);
            }
        }
        else
        {
            /* 按键松开抬起 */

            /* 判定为长按后的释放 */
            if (btn->tick_cnt >= btn->long_ticks)
            {
                btn->status = BSP_BTN_STATUS_DEFAULT;
                BSP_BTN_TRIGGER_EVENT(btn, BSP_BTN_EVT_LONG_UP);
            }
            /* 判定为有效短按 (超过消抖阈值，未到长按阈值) */
            else if (btn->tick_cnt >= btn->short_ticks)
            {
                btn->click_cnt++;
                btn->tick_cnt = 0;
                /* 转入等待连击阶段 */
                btn->status = BSP_BTN_STATUS_WAIT_MULTIPLE;
            }
            /* 未达到短按阈值视为硬件抖动，直接回退空闲 */
            else
            {
                btn->status = BSP_BTN_STATUS_DEFAULT;
            }
        }
        break;

    case BSP_BTN_STATUS_WAIT_MULTIPLE:
        /* 抬起等待连击阶段 */
        if (is_pressed)
        {
            /* 在连击窗口期内再次按下，重新进入按下阶段 */
            btn->tick_cnt = 0;
            btn->status    = BSP_BTN_STATUS_DOWN;
            BSP_BTN_TRIGGER_EVENT(btn, BSP_BTN_EVT_DOWN);
        }
        else
        {
            btn->tick_cnt++;

            /* 超过连击等待间隔，结算点击次数 */
            if (btn->tick_cnt > btn->multi_click_ticks)
            {
                if (btn->click_cnt == 1)
                {
                    BSP_BTN_TRIGGER_EVENT(btn, BSP_BTN_EVT_CLICK);
                }
                else if (btn->click_cnt == 2)
                {
                    BSP_BTN_TRIGGER_EVENT(btn, BSP_BTN_EVT_DOUBLE_CLICK);
                }
                else if (btn->click_cnt > 2)
                {
                    BSP_BTN_TRIGGER_EVENT(btn, BSP_BTN_EVT_REPEAT_CLICK);
                }

                /* 结算完成，状态归零重置 */
                btn->click_cnt = 0;
                btn->tick_cnt  = 0;
                btn->status     = BSP_BTN_STATUS_DEFAULT;
            }
        }
        break;

    default:
        btn->status = BSP_BTN_STATUS_DEFAULT;
        break;
    }
}

/**
 * @brief  按键周期扫描核心函数
 * @note   在定时器中断或周期任务中定期调用 (推荐周期 20ms)
 */
void bsp_button_scan(void)
{
    bsp_button_t *curr = s_btn_head;

    /* 遍历已注册的单向链表节点 */
    while (curr != NULL)
    {
        bsp_button_process(curr);
        curr = curr->next;
    }
}