/**
 * @file    app_main.cpp
 * @brief   应用层入口 —— 任务01 工程要求 1（分层：Application 只依赖 interface）
 *
 * 这个文件里不允许出现 HAL_ 前缀。它只能看到：
 *   common/error_code.hpp   （错误码）
 *   common/log.hpp          （日志）
 *   bsp/uart.hpp 等接口     （外设能力）
 */
#include <cstring>

#include "cmsis_os2.h"
#include "bsp_uart.hpp"
#include "log.hpp"
#include "uart.hpp"

namespace {

// 测试用缓冲放静态区，不占任务栈（任务栈只有 512 B）
std::uint8_t g_test_buf[256];
std::uint32_t g_seq = 0;      // 已发出的帧序号（也是数据模式的种子）

/// 校验用：每帧 256 字节，第 j 字节 = (seq + j) & 0xFF
/// PC 端把收到的字节流按相邻差 1 检查，就能看出有没有错位/丢包
void fill_pattern(std::uint8_t* buf, std::size_t len, std::uint32_t seed)
{
  for (std::size_t j = 0; j < len; ++j) {
    buf[j] = static_cast<std::uint8_t>(seed + j);
  }
}

/// S-D3 压测任务：验证 write_async 的吞吐、忙保护、越界保护
void uart_dma_test_task(void* arg)
{
  (void)arg;
  osDelay(1000);                      // 等系统稳定、日志任务先跑起来

  LOG_I("=== UART DMA test start ===");

  // ① 越界保护：256 B 的缓冲塞 300 B，必须返回 Overflow 而不是截断发送
  fill_pattern(g_test_buf, sizeof(g_test_buf), 0);
  common::Err e = bsp::uart1.write_async(g_test_buf, 300);
  LOG_I("overflow test (300B) -> %s", common::to_str(e));

  // ② 忙保护：连发两帧不等完成，第二次必须返回 Busy
  e = bsp::uart1.write_async(g_test_buf, sizeof(g_test_buf));
  common::Err e2 = bsp::uart1.write_async(g_test_buf, sizeof(g_test_buf));
  LOG_I("busy test -> first=%s second=%s", common::to_str(e), common::to_str(e2));
  while (!bsp::uart1.is_idle()) osDelay(1);
  osDelay(50);

  // ③ 连续发送：100 帧 × 256 B = 25.6 KB，统计 Busy 重试次数
  std::uint32_t ok_cnt = 0, busy_cnt = 0, err_cnt = 0;
  for (int i = 0; i < 100; ++i) {
    fill_pattern(g_test_buf, sizeof(g_test_buf), g_seq);
    common::Err r;
    do {
      r = bsp::uart1.write_async(g_test_buf, sizeof(g_test_buf));
      if (r == common::Err::Busy) { ++busy_cnt; osDelay(1); }
    } while (r == common::Err::Busy);

    if (r == common::Err::Ok) { ++ok_cnt; ++g_seq; } else { ++err_cnt; }
  }
  while (!bsp::uart1.is_idle()) osDelay(1);
  osDelay(100);

  LOG_I("bulk test: ok=%lu busy_retry=%lu err=%lu bytes=%lu",
        static_cast<unsigned long>(ok_cnt),
        static_cast<unsigned long>(busy_cnt),
        static_cast<unsigned long>(err_cnt),
        static_cast<unsigned long>(ok_cnt * sizeof(g_test_buf)));
  LOG_I("=== UART DMA test done ===");

  for (;;) osDelay(10000);            // 测试完成，挂起（保留句柄便于观察）
}

}  // namespace

extern "C" void app_init(void)
{
  common::log_init();   // ?????,?????????
  LOG_I("app layer ready (C++17, HAL-free)");

  // 压测任务：优先级 Low，栈 1024 B（printf 走 vprintf，512 B 会被撑爆）
  const osThreadAttr_t attr = {
      .name = "uart_test", .stack_size = 1024, .priority = osPriorityLow1};
  if (osThreadNew(uart_dma_test_task, nullptr, &attr) == nullptr) {
    LOG_E("create uart_test task failed");
  }
}
