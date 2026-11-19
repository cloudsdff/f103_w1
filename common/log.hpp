/**
 * @file    log.hpp
 * @brief   统一日志接口 —— 任务01 工程要求 4（统一日志，禁止静默失败）
 *
 * 当前实现：同步输出（printf → bsp 层 → USART1），但加了一把互斥锁，
 *          保证多任务同时打印时不会把 newlib 的内部状态搞坏。
 * D13 计划：改成「队列 + 低优先级任务」异步输出（实测一行 33 B @115200 约 2.9 ms）。
 *
 * 用法：
 *   LOG_I("cnt=%lu", (unsigned long)cnt);
 *   LOG_E("i2c failed: %s", common::to_str(e));
 */
#ifndef COMMON_LOG_HPP
#define COMMON_LOG_HPP

#include <cstdarg>
#include <cstdint>

#include "error_code.hpp"

namespace common {

enum class LogLevel : std::uint8_t { Debug = 0, Info, Warn, Error };

/** 编译期日志级别裁剪：0=只留 Error，1=+Warn，2=+Info，3=+Debug */
#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

/// 创建日志互斥锁。必须在启动调度器之前调用一次（app_init 里）。
void log_init();

/// 实际输出：加锁 → 打印时间戳与正文 → 解锁
void log_impl(LogLevel lv, const char* fmt, ...);

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
