/**
 * @file memory_optimization_recommendations.md
 * @brief 菜单架构内存优化建议和实现方案
 * @author CodeBuddy Code
 * @date 2025.12.18
 */

# 菜单架构内存优化方案

## 概述

通过对当前菜单架构的分析，发现主要内存占用问题在于动态内存分配过多、结构体设计不够紧凑。本文档提供了详细的优化方案和实现建议。

## 主要问题分析

### 1. 动态内存分配过多
- 每个菜单项都使用 `pvPortMalloc` 动态分配
- 子菜单数组频繁重新分配和释放
- 上下文数据分散分配，造成内存碎片

### 2. 结构体设计不够紧凑
- `menu_item_t` 结构体包含过多字段，每个约80+字节
- 大量指针占用额外空间（4字节/指针）
- 多个回调函数指针可以合并

### 3. 字符串存储效率低
- 菜单名称使用完整字符串存储
- 可以使用枚举索引或字符串池优化

## 优化方案

### 方案1：静态内存池（推荐）

#### 优点
- 避免内存碎片
- 分配/释放速度快
- 内存占用可预测
- 适合嵌入式系统

#### 实现
- 预分配固定大小的菜单项池
- 使用索引代替指针
- 统一管理所有内存资源

### 方案2：结构体优化

#### 精简菜单项结构
```c
// 原结构体：约80字节
typedef struct menu_item {
    const char *name;           // 4字节
    menu_type_t type;           // 1字节
    menu_content_t content;     // 4-8字节
    // ... 更多字段
} menu_item_t;

// 优化后：约32字节
typedef struct menu_item_slim {
    uint8_t type;              // 1字节
    uint8_t flags;             // 1字节（位字段组合多个状态）
    uint8_t name_index;        // 1字节（字符串池索引）
    // ... 其他优化字段
} menu_item_slim_t;
```

#### 使用位字段
```c
// 使用单个字节的位字段代替多个布尔变量
#define MENU_FLAG_SELECTED  (1 << 0)
#define MENU_FLAG_VISIBLE   (1 << 1)
#define MENU_FLAG_ENABLED   (1 << 2)
#define MENU_FLAG_ACTIVE    (1 << 3)

uint8_t flags;  // 代替 is_selected, is_visible, is_enabled, is_active
```

### 方案3：统一回调函数

#### 合并多个回调函数
```c
// 原方案：4个函数指针 = 16字节
void (*on_enter)(menu_item_t *);
void (*on_exit)(menu_item_t *);
void (*on_select)(menu_item_t *);
void (*on_key)(menu_item_t *, uint8_t);

// 优化方案：1个函数指针 + 事件类型 = 4字节
void (*callback)(menu_item_t *item, uint8_t events);

// 使用示例
void my_menu_callback(menu_item_t *item, uint8_t events) {
    if (events & MENU_CALLBACK_ENTER) {
        // 处理进入事件
    }
    if (events & MENU_CALLBACK_KEY) {
        // 处理按键事件
    }
}
```

### 方案4：字符串池优化

#### 使用名称池
```c
// 原方案：每个名称单独存储，长度不固定
const char *name;  // 指向字符串，字符串长度可变

// 优化方案：固定大小的名称池
char name_pool[POOL_SIZE][MAX_NAME_LENGTH];  // 统一管理
uint8_t name_index;  // 只存储索引
```

### 方案5：子菜单管理优化

#### 使用固定数组代替动态指针数组
```c
// 原方案：动态分配指针数组
struct menu_item **children;  // 需要额外malloc/free

// 优化方案：固定大小数组
uint8_t children[MAX_CHILDREN];  // 存储子项索引
uint8_t child_count;
```

## 内存占用对比

| 方案 | 原架构 | 优化后 | 节省比例 |
|------|--------|--------|----------|
| 菜单项大小 | ~80字节 | ~32字节 | 60% |
| 子菜单管理 | 动态分配 | 固定数组 | 避免碎片 |
| 字符串存储 | 变长字符串 | 固定池 | 30-50% |
| 回调函数 | 4个指针 | 1个指针 | 75% |

## 实现建议

### 1. 渐进式优化
- 先实现静态内存池
- 逐步优化结构体
- 最后优化字符串和回调

### 2. 保持API兼容性
- 提供兼容层，允许现有代码逐步迁移
- 使用宏定义简化过渡

### 3. 内存监控
- 添加内存使用统计
- 实现内存泄漏检测

### 4. 测试验证
- 确保功能正确性
- 测试边界条件
- 验证内存使用效率

## 示例代码

`memory_optimized_menu.h` 提供了完整的优化实现方案，包括：
- 精简的菜单项结构
- 静态内存池管理
- 统一的回调机制
- 优化的API接口

## 预期效果

- 内存占用减少 50-60%
- 避免内存碎片
- 提高运行效率
- 更好的可预测性

## 注意事项

1. 需要合理设置池大小，避免浪费
2. 固定数组大小需要根据实际需求调整
3. 代码可读性可能略有下降，需要充分注释
4. 调试时需要额外的工具支持