/**
 * @file    log.hpp
 * @brief   统一日志接口 —— 任务01 工程要求 4（统一日志，禁止静默失败）
 *
 * 当前版本：同步输出（走 newlib printf → __io_putchar → USART1）
 * D13 计划：改成「队列 + 低优先级任务」异步输出，避免阻塞业务任务
 *          （实测：一行 33 字节 @115200 要 ~2.9 ms，同步打印会吃掉实时性）
 *
 * 用法：
 *   LOG_I("cnt=%lu", (unsigned long)cnt);
 *   LOG_E("i2c failed: %s", common::to_str(e));
 */
#ifndef COMMON_LOG_HPP
#define COMMON_LOG_HPP

#include <cstdarg>
#include <cstdio>

#include "main.h"        ///< 仅为 HAL_GetTick() 提供时间戳；业务层不要直接调 HAL
#include "error_code.hpp"

namespace common {

enum class LogLevel : std::uint8_t { Debug = 0, Info, Warn, Error };

/** 编译期日志级别裁剪：0=只留 Error，1=+Warn，2=+Info，3=+Debug */
#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

inline void log_impl(LogLevel lv, const char* fmt, ...)
{
  static const char* kTag[] = {"D", "I", "W", "E"};
  std::printf("[%s][%lu] ", kTag[static_cast<int>(lv)],
              static_cast<unsigned long>(HAL_GetTick()));
  va_list ap;
  va_start(ap, fmt);
  std::vprintf(fmt, ap);
  va_end(ap);
  std::printf("\r\n");
}

}  // namespace common

#if LOG_LEVEL >= 3
#define LOG_D(...) ::common::log_impl(::common::LogLevel::Debug, __VA_ARGS__)
#else
#define LOG_D(...) do {} while (0)
#endif

#if LOG_LEVEL >= 2
#define LOG_I(...) ::common::log_impl(::common::LogLevel::Info, __VA_ARGS__)
#else
#define LOG_I(...) do {} while (0)
#endif

#if LOG_LEVEL >= 1
#define LOG_W(...) ::common::log_impl(::common::LogLevel::Warn, __VA_ARGS__)
#else
#define LOG_W(...) do {} while (0)
#endif

#define LOG_E(...) ::common::log_impl(::common::LogLevel::Error, __VA_ARGS__)

#endif  // COMMON_LOG_HPP
