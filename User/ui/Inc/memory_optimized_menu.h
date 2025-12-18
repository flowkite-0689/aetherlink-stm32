/**
 * @file memory_optimized_menu.h
 * @brief 内存优化版本的菜单架构头文件
 * @author flowkite-0689
 * @version v2.0
 * @date 2025.12.18
 */

#ifndef __MEMORY_OPTIMIZED_MENU_H
#define __MEMORY_OPTIMIZED_MENU_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "oled_print.h"
#include "Key.h"
#include <stdio.h>
#include "beep.h"
#include "debug.h"

// ==================================
// 配置常量
// ==================================

#define MAX_MENU_ITEMS 32         // 最大菜单项数量
#define MAX_CHILDREN_PER_ITEM 5   // 每个菜单项最大子项数
#define MAX_NAME_LENGTH 16        // 菜单名称最大长度
#define MENU_NAME_POOL_SIZE 64     // 菜单名称池大小

// ==================================
// 菜单类型枚举
// ==================================

typedef enum {
    MENU_TYPE_HORIZONTAL_ICON = 0,
    MENU_TYPE_VERTICAL_LIST   = 1,
    MENU_TYPE_CUSTOM          = 2
} menu_type_t;

// ==================================
// 位标志定义
// ==================================

#define MENU_FLAG_SELECTED  (1 << 0)
#define MENU_FLAG_VISIBLE   (1 << 1)
#define MENU_FLAG_ENABLED   (1 << 2)
#define MENU_FLAG_ACTIVE    (1 << 3)

// ==================================
// 回调事件类型
// ==================================

typedef enum {
    MENU_CALLBACK_ENTER   = 1,
    MENU_CALLBACK_EXIT    = 2,
    MENU_CALLBACK_SELECT  = 4,
    MENU_CALLBACK_KEY     = 8
} menu_callback_event_t;

// ==================================
// 内存池结构
// ==================================

typedef struct {
    uint16_t data[128];  // 上下文数据池
    uint16_t used_bitmap[8];  // 使用位图 (128位 = 8个16位)
} context_pool_t;

typedef struct {
    char names[MENU_NAME_POOL_SIZE][MAX_NAME_LENGTH];  // 名称池
    uint8_t name_used[MENU_NAME_POOL_SIZE];  // 名称使用标记
} name_pool_t;

// ==================================
// 精简菜单项结构
// ==================================

typedef struct menu_item_slim {
    // 基本信息
    uint8_t type;          // 菜单类型
    uint8_t flags;         // 位标志：选中|可见|启用|激活
    uint8_t name_index;    // 名称池索引
    
    // 内容信息（使用联合体节省空间）
    union {
        const uint8_t *icon_data;      // 图标数据指针
        const char *text_data;         // 文本数据指针
        struct {
            void (*draw_func)(void *); // 自定义绘制函数
            uint16_t context_offset;   // 上下文在数据池中的偏移
        } custom;
    } content;
    
    // 层次关系（使用索引代替指针）
    uint8_t parent_index;           // 父菜单索引 (0xFF表示无)
    uint8_t children[MAX_CHILDREN_PER_ITEM];  // 子菜单索引数组
    uint8_t child_count;            // 子菜单数量
    uint8_t selected_child;        // 选中的子项索引
    
    // 回调函数（合并为单一回调）
    void (*callback)(struct menu_item_slim *item, uint8_t events);
    
    // 上下文信息
    uint16_t context_offset;        // 上下文数据在池中的偏移（0xFFFF表示无）
} menu_item_slim_t;

// ==================================
// 菜单系统结构（精简版）
// ==================================

typedef struct {
    // 当前状态
    uint8_t current_menu_index;     // 当前菜单索引
    uint8_t root_menu_index;        // 根菜单索引
    uint8_t menu_active;            // 菜单激活状态
    
    // 分页信息
    uint8_t current_page;           // 当前页码
    uint8_t total_pages;            // 总页数
    uint8_t items_per_page;         // 每页项目数
    
    // 显示控制
    uint8_t need_refresh;           // 需要刷新标志
    uint32_t last_refresh_time;     // 上次刷新时间
    uint8_t blink_state;            // 闪烁状态
    
    // FreeRTOS资源
    QueueHandle_t event_queue;       // 事件队列
    SemaphoreHandle_t display_mutex; // 显示互斥量
    
    // 按键处理
    uint32_t last_key_time;         // 上次按键时间
    uint8_t key_debounce_time;       // 按键去抖时间(ms)
} menu_system_slim_t;

// ==================================
// 菜单事件类型（保持不变）
// ==================================

typedef enum {
    MENU_EVENT_NONE = 0,
    MENU_EVENT_KEY_UP = 1,
    MENU_EVENT_KEY_DOWN = 2,
    MENU_EVENT_KEY_SELECT = 3,
    MENU_EVENT_KEY_ENTER = 4,
    MENU_EVENT_REFRESH = 5,
    MENU_EVENT_TIMEOUT = 6,
    MENU_EVENT_ALARM = 7
} menu_event_type_t;

typedef struct {
    menu_event_type_t type;
    uint32_t timestamp;
    uint8_t param;
} menu_event_t;

// ==================================
// 全局变量声明
// ==================================

extern menu_system_slim_t g_menu_sys_slim;
extern menu_item_slim_t g_menu_items[MAX_MENU_ITEMS];
extern uint8_t g_menu_used[MAX_MENU_ITEMS];
extern name_pool_t g_name_pool;
extern context_pool_t g_context_pool;

// ==================================
// 内存管理API
// ==================================

/**
 * @brief 初始化菜单内存池
 */
void menu_memory_init(void);

/**
 * @brief 从菜单项池中分配一个菜单项
 * @return 分配的菜单项索引，0xFF表示失败
 */
uint8_t menu_item_alloc(void);

/**
 * @brief 释放菜单项回池中
 * @param index 菜单项索引
 * @return 0-成功，其他-失败
 */
int8_t menu_item_free(uint8_t index);

/**
 * @brief 从名称池中分配名称字符串
 * @param name 名称字符串
 * @return 分配的名称索引，0xFF表示失败
 */
uint8_t menu_name_alloc(const char *name);

/**
 * @brief 从名称索引获取名称字符串
 * @param index 名称索引
 * @return 名称字符串指针
 */
const char *menu_name_get(uint8_t index);

/**
 * @brief 从上下文池中分配空间
 * @param size 需要的大小（以2字节为单位）
 * @return 分配的偏移量，0xFFFF表示失败
 */
uint16_t menu_context_alloc(uint8_t size);

/**
 * @brief 释放上下文池空间
 * @param offset 偏移量
 * @param size 大小（以2字节为单位）
 */
void menu_context_free(uint16_t offset, uint8_t size);

// ==================================
// 精简版菜单API
// ==================================

/**
 * @brief 初始化精简版菜单系统
 * @return 0-成功，其他-失败
 */
int8_t menu_system_slim_init(void);

/**
 * @brief 创建精简版菜单项
 * @param name 菜单名称
 * @param type 菜单类型
 * @return 创建的菜单项索引，0xFF表示失败
 */
uint8_t menu_item_slim_create(const char *name, menu_type_t type);

/**
 * @brief 添加子菜单项
 * @param parent_index 父菜单索引
 * @param child_index 子菜单索引
 * @return 0-成功，其他-失败
 */
int8_t menu_add_child_slim(uint8_t parent_index, uint8_t child_index);

/**
 * @brief 设置菜单项图标
 * @param index 菜单项索引
 * @param icon_data 图标数据指针
 * @return 0-成功，其他-失败
 */
int8_t menu_set_icon(uint8_t index, const uint8_t *icon_data);

/**
 * @brief 设置菜单项文本
 * @param index 菜单项索引
 * @param text 文本指针
 * @return 0-成功，其他-失败
 */
int8_t menu_set_text(uint8_t index, const char *text);

/**
 * @brief 设置自定义绘制函数
 * @param index 菜单项索引
 * @param draw_func 绘制函数
 * @param context_size 上下文大小（字节）
 * @return 0-成功，其他-失败
 */
int8_t menu_set_custom_draw(uint8_t index, void (*draw_func)(void *), uint8_t context_size);

/**
 * @brief 设置回调函数
 * @param index 菜单项索引
 * @param callback 回调函数
 * @param events 支持的事件掩码
 * @return 0-成功，其他-失败
 */
int8_t menu_set_callback(uint8_t index, void (*callback)(struct menu_item_slim *, uint8_t), uint8_t events);

// ==================================
// 菜单操作API
// ==================================

/**
 * @brief 刷新菜单显示
 */
void menu_refresh_display_slim(void);

/**
 * @brief 处理菜单事件
 * @param event 菜单事件
 * @return 0-成功，其他-失败
 */
int8_t menu_process_event_slim(menu_event_t *event);

/**
 * @brief 进入指定菜单
 * @param menu_index 目标菜单索引
 * @return 0-成功，其他-失败
 */
int8_t menu_enter_slim(uint8_t menu_index);

/**
 * @brief 返回父菜单
 * @return 0-成功，其他-失败
 */
int8_t menu_back_to_parent_slim(void);

/**
 * @brief 进入选中的菜单项
 * @return 0-成功，其他-失败
 */
int8_t menu_enter_selected_slim(void);

// ==================================
// FreeRTOS任务API
// ==================================

/**
 * @brief 精简版菜单处理任务
 * @param pvParameters 任务参数
 */
void menu_task_slim(void *pvParameters);

/**
 * @brief 精简版按键处理任务
 * @param pvParameters 任务参数
 */
void menu_key_task_slim(void *pvParameters);

// ==================================
// 便利宏定义
// ==================================

// 创建自定义菜单项
#define MENU_ITEM_CUSTOM_SLIM(name, draw_func, context_size) \
    menu_item_slim_create(name, MENU_TYPE_CUSTOM); \
    menu_set_custom_draw(index, draw_func, context_size);

// 位操作宏
#define MENU_IS_SELECTED(item)   ((item)->flags & MENU_FLAG_SELECTED)
#define MENU_SET_SELECTED(item)  ((item)->flags |= MENU_FLAG_SELECTED)
#define MENU_CLEAR_SELECTED(item) ((item)->flags &= ~MENU_FLAG_SELECTED)

#define MENU_IS_VISIBLE(item)    ((item)->flags & MENU_FLAG_VISIBLE)
#define MENU_SET_VISIBLE(item)   ((item)->flags |= MENU_FLAG_VISIBLE)
#define MENU_CLEAR_VISIBLE(item)  ((item)->flags &= ~MENU_FLAG_VISIBLE)

#define MENU_IS_ENABLED(item)    ((item)->flags & MENU_FLAG_ENABLED)
#define MENU_SET_ENABLED(item)   ((item)->flags |= MENU_FLAG_ENABLED)
#define MENU_CLEAR_ENABLED(item)  ((item)->flags &= ~MENU_FLAG_ENABLED)

#endif // __MEMORY_OPTIMIZED_MENU_H