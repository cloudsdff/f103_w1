/**
 * @file    app_main.cpp
 * @brief   应用层入口 + 两个自检任务（UART / MPU6050）
 *
 * 约定：这个文件里不允许出现 HAL_ 前缀。
 */
#include <cstdint>

#include "bsp_i2c.hpp"
#include "bsp_uart.hpp"
#include "cmsis_os2.h"
#include "i2c.hpp"
#include "log.hpp"
#include "mpu6050.hpp"
#include "uart.hpp"

namespace {

std::uint8_t g_uart_buf[256];
std::uint32_t g_seq = 0;

void fill_pattern(std::uint8_t* buf, std::size_t len, std::uint32_t seed)
{
  for (std::size_t j = 0; j < len; ++j) buf[j] = static_cast<std::uint8_t>(seed + j);
}

bool wait_uart_idle(std::uint32_t timeout_ms)
{
  const std::uint32_t t0 = osKernelGetTickCount();
  while (!bsp::uart1.is_idle()) {
    if ((osKernelGetTickCount() - t0) > timeout_ms) return false;
    osDelay(1);
  }
  return true;
}

// ---------------- UART 自检（只跑保护类检查，批量压测已在 docs/02 记录） ------------
void uart_selftest_task(void* arg)
{
  (void)arg;
  osDelay(500);
  fill_pattern(g_uart_buf, sizeof(g_uart_buf), 0);
  const common::Err e1 = bsp::uart1.write_async(g_uart_buf, sizeof(g_uart_buf));
  const bool idle_ok = wait_uart_idle(200);
  LOG_I("uart selftest: write=%s idle=%s", common::to_str(e1),
        idle_ok ? "yes" : "TIMEOUT");

  const common::Err e2 = bsp::uart1.write_async(g_uart_buf, 300);
  LOG_I("uart overflow(300B) -> %s", common::to_str(e2));

  fill_pattern(g_uart_buf, sizeof(g_uart_buf), g_seq);
  const common::Err b1 = bsp::uart1.write_async(g_uart_buf, sizeof(g_uart_buf));
  const common::Err b2 = bsp::uart1.write_async(g_uart_buf, sizeof(g_uart_buf));
  (void)wait_uart_idle(200);
  LOG_I("uart busy test: first=%s second=%s", common::to_str(b1), common::to_str(b2));

  for (;;) osDelay(10000);
}

// ---------------- MPU6050 自检 --------------------------------------------------
module::Mpu6050 g_mpu(bsp::i2c1);

void mpu_selftest_task(void* arg)
{
  (void)arg;
  osDelay(1500);

  common::Err e = g_mpu.init();
  LOG_I("mpu init -> %s", common::to_str(e));
  if (e != common::Err::Ok) {
    // 探测失败：先试一次总线恢复，再报告
    LOG_W("mpu init failed, trying bus recovery...");
    const common::Err r = bsp::i2c1.recover_bus();
    LOG_W("bus recovery -> %s", common::to_str(r));
    for (;;) osDelay(10000);
  }

  // 阻塞方式连读 5 帧
  for (int i = 0; i < 5; ++i) {
    module::Mpu6050::Raw raw{};
    const common::Err r = g_mpu.read_raw(raw);
    if (r == common::Err::Ok) {
      LOG_I("acc[%d,%d,%d] gyro[%d,%d,%d]", raw.ax, raw.ay, raw.az, raw.gx, raw.gy,
            raw.gz);
    } else {
      LOG_E("read_raw failed: %s", common::to_str(r));
    }
    osDelay(500);
  }

  // 中断方式：发起 → 等 busy 清零 → 再读
  module::Mpu6050::Raw async_raw{};
  const common::Err ae = g_mpu.read_raw_async(async_raw);
  LOG_I("async start -> %s", common::to_str(ae));
  const std::uint32_t t0 = osKernelGetTickCount();
  while (g_mpu.busy() && (osKernelGetTickCount() - t0) < 100) osDelay(1);
  LOG_I("async busy=%s elapsed=%lu ms", g_mpu.busy() ? "STILL" : "done",
        static_cast<unsigned long>(osKernelGetTickCount() - t0));

  for (;;) {
    module::Mpu6050::Raw raw{};
    if (g_mpu.read_raw(raw) == common::Err::Ok) {
      LOG_I("acc[%d,%d,%d] gyro[%d,%d,%d]", raw.ax, raw.ay, raw.az, raw.gx, raw.gy,
            raw.gz);
    } else {
      LOG_W("mpu lost, recovery...");
      (void)bsp::i2c1.recover_bus();
    }
    osDelay(1000);
  }
}

}  // namespace

extern "C" void app_init(void)
{
  common::log_init();
  LOG_I("app layer ready (C++17, HAL-free)");

  const osThreadAttr_t a1 = {.name = "uart_test", .stack_size = 1024,
                             .priority = osPriorityLow1};
  if (osThreadNew(uart_selftest_task, nullptr, &a1) == nullptr) {
    LOG_E("create uart_test failed");
  }

  const osThreadAttr_t a2 = {.name = "mpu_test", .stack_size = 1024,
                             .priority = osPriorityLow1};
  if (osThreadNew(mpu_selftest_task, nullptr, &a2) == nullptr) {
    LOG_E("create mpu_test failed");
  }
}
