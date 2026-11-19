/**
 * @file    app_main.cpp
 * @brief   应用层入口 + UART DMA 压测任务
 *
 * 这个文件里不允许出现 HAL_ 前缀，只能看见 common/ 与 bsp/ 的接口。
 *
 * 压测任务的写法有一条硬规则：
 *   DMA 正在发送时不要打印（printf 也走同一个串口，会把发送路径搅乱）。
 *   所以所有日志都放在"确认空闲之后"再打。
 */
#include <cstdint>
#include <cstring>

#include "bsp_uart.hpp"
#include "cmsis_os2.h"
#include "log.hpp"
#include "uart.hpp"

namespace {

std::uint8_t g_test_buf[256];
std::uint32_t g_seq = 0;

/// 数据模式：第 j 字节 = (seq + j) & 0xFF。PC 端按"相邻字节差 1"校验错位/丢包。
void fill_pattern(std::uint8_t* buf, std::size_t len, std::uint32_t seed)
{
  for (std::size_t j = 0; j < len; ++j) {
    buf[j] = static_cast<std::uint8_t>(seed + j);
  }
}

/// 等发送完成，最多等 timeout_ms。返回 true = 等到空闲
bool wait_idle(std::uint32_t timeout_ms)
{
  const std::uint32_t t0 = osKernelGetTickCount();
  while (!bsp::uart1.is_idle()) {
    if ((osKernelGetTickCount() - t0) > timeout_ms) return false;
    osDelay(1);
  }
  return true;
}

void uart_dma_test_task(void* arg)
{
  (void)arg;
  osDelay(1000);
  LOG_I("=== UART DMA test start ===");

  // 1) 单帧：先确认"写一帧 + 等完成"这条最基本的路
  fill_pattern(g_test_buf, sizeof(g_test_buf), 0);
  const common::Err e1 = bsp::uart1.write_async(g_test_buf, sizeof(g_test_buf));
  const bool idle_ok = wait_idle(200);
  LOG_I("single frame: write=%s idle=%s", common::to_str(e1),
        idle_ok ? "yes" : "TIMEOUT");

  // 2) 越界保护：256 B 缓冲塞 300 B，必须返回 Overflow
  const common::Err e2 = bsp::uart1.write_async(g_test_buf, 300);
  LOG_I("overflow test (300B) -> %s", common::to_str(e2));

  // 3) 忙保护：连发两帧，第二次必须 Busy（先记结果，等空闲后再打印）
  fill_pattern(g_test_buf, sizeof(g_test_buf), g_seq);
  const common::Err b1 = bsp::uart1.write_async(g_test_buf, sizeof(g_test_buf));
  const common::Err b2 = bsp::uart1.write_async(g_test_buf, sizeof(g_test_buf));
  if (b1 == common::Err::Ok) ++g_seq;
  (void)wait_idle(200);
  LOG_I("busy test: first=%s second=%s", common::to_str(b1), common::to_str(b2));

  // 4) 连续 100 帧 × 256 B = 25.6 KB
  std::uint32_t ok_cnt = 0, busy_cnt = 0, err_cnt = 0, timeout_cnt = 0;
  for (int i = 0; i < 100; ++i) {
    fill_pattern(g_test_buf, sizeof(g_test_buf), g_seq);
    common::Err r;
    do {
      r = bsp::uart1.write_async(g_test_buf, sizeof(g_test_buf));
      if (r == common::Err::Busy) { ++busy_cnt; osDelay(1); }
    } while (r == common::Err::Busy);

    if (r == common::Err::Ok) {
      ++ok_cnt;
      ++g_seq;
    } else {
      ++err_cnt;
    }
  }
  if (!wait_idle(3000)) ++timeout_cnt;

  LOG_I("bulk: ok=%lu busy_retry=%lu err=%lu idle_timeout=%lu bytes=%lu",
        static_cast<unsigned long>(ok_cnt), static_cast<unsigned long>(busy_cnt),
        static_cast<unsigned long>(err_cnt), static_cast<unsigned long>(timeout_cnt),
        static_cast<unsigned long>(ok_cnt * sizeof(g_test_buf)));
  LOG_I("=== UART DMA test done ===");

  for (;;) osDelay(10000);
}

}  // namespace

extern "C" void app_init(void)
{
  common::log_init();
  LOG_I("app layer ready (C++17, HAL-free)");

  const osThreadAttr_t attr = {
      .name = "uart_test", .stack_size = 1024, .priority = osPriorityLow1};
  if (osThreadNew(uart_dma_test_task, nullptr, &attr) == nullptr) {
    LOG_E("create uart_test task failed");
  }
}
