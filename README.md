# 学习OC Runloop源码

## CFRuntime.h

CFRuntime CF面向对象实现

CFRuntime 是固定的基础数据类型封装

`CFTypeID` 其实存储的是 类在系统类标注册的 index, 方便后期初始化 只需要传入相应id

## CFRuntime.c

`__CFInitialize`初始化CF系统所有的基础类