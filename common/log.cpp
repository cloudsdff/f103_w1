/**
 * @file    log.cpp
 * @brief   统一日志的实现（带互斥锁）
 *
 * 为什么要锁：newlib 的 printf 不是线程安全的。两个任务同时打印，
 * 会破坏它内部的 FILE 结构，轻则输出错行，重则直接 HardFault。
 * 这里用一把 CMSIS-RTOS2 的互斥锁把打印串起来。
 *
 * 对外提供两种入口：
 *   C++：LOG_I / LOG_W / LOG_E 宏 → common::log_impl
 *   C  ：common_log_info("...", ...)  → 供 CubeMX 生成的 main.c 使用
 */
#include "log.hpp"

#include <cstdio>

#include "cmsis_os2.h"
#include "main.h"      // HAL_GetTick()

namespace common {
namespace {

osMutexId_t g_log_mutex = nullptr;

const char* tag_of(LogLevel lv)
{
  static const char* kTag[] = {"D", "I", "W", "E"};
  return kTag[static_cast<int>(lv)];
}

}  // namespace

void log_init()
{
  if (g_log_mutex != nullptr) return;
  g_log_mutex = osMutexNew(nullptr);   // 允许在调度器启动前创建
}

void log_v(LogLevel lv, const char* fmt, va_list ap)
{
  // 调度器还没跑起来时（例如 main 里的初始化日志）直接打印，不加锁
  const bool locked = (g_log_mutex != nullptr) &&
                      (osKernelGetState() == osKernelRunning);
  if (locked) {
    osMutexAcquire(g_log_mutex, osWaitForever);
  }

  std::printf("[%s][%lu] ", tag_of(lv),
              static_cast<unsigned long>(HAL_GetTick()));
  std::vprintf(fmt, ap);
  std::printf("\r\n");

  if (locked) {
    osMutexRelease(g_log_mutex);
  }
}

void log_impl(LogLevel lv, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  log_v(lv, fmt, ap);
  va_end(ap);
}

}  // namespace common

/* --------------------------------------------------------------------------
 * 给 C 代码用的入口（CubeMX 生成的 main.c 是 C，不能直接用 LOG_ 宏）
 * 用法：common_log_info("cnt=%lu", (unsigned long)cnt);
 * ----------------------------------------------------------------------- */
extern "C" void common_log_info(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  ::common::log_v(::common::LogLevel::Info, fmt, ap);
  va_end(ap);
}
