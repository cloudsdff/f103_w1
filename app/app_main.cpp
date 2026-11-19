/**
 * @file    app_main.cpp
 * @brief   应用层入口 —— 任务01 工程要求 1（分层：Application 只依赖 interface）
 *
 * 这个文件里不允许出现 HAL_ 前缀。它只能看到：
 *   common/error_code.hpp   （错误码）
 *   common/log.hpp          （日志）
 *   bsp/uart.hpp 等接口     （外设能力）
 */
#include "log.hpp"

extern "C" void app_init(void)
{
  LOG_I("app layer ready (C++17, HAL-free)");
  // D2 之后在这里接 bsp::Uart 实例；D3 之后接 module::Mpu6050
}
