# Dự Án Trạm Hàn Nhiệt Độ T12 — Nhóm 2 (STM32F103C8T6 + STD/SPL + FreeRTOS)

Bộ khung dự án chuẩn (**Base Project — Giai đoạn 1: Bước 1 & Bước 2**) do **Nhóm trưởng (Dev 1)** khởi tạo.

---

## 1. Cấu Trúc Thư Mục Dự Án

```text
t12-stm32-freertos-std/
├── libraries/                        # Đặt thư viện chuẩn CMSIS & STM32F10x_StdPeriph_Driver (SPL v3.5.0) tại đây
│   ├── CMSIS/                        # core_cm3.c, system_stm32f10x.c, startup_stm32f10x_md.s
│   └── STM32F10x_StdPeriph_Driver/   # inc/ & src/ (rcc, gpio, adc, dma, tim, i2c, usart, flash, iwdg, misc)
├── middlewares/                      # FreeRTOS Kernel & Thư viện đồ họa U8g2 C
│   ├── FreeRTOSConfig.h              # Cấu hình Kernel (72MHz, 10KB Heap, PriorityGroup_4)
│   ├── FreeRTOS/                     # Đặt source FreeRTOS (tasks.c, queue.c, list.c, port.c, heap_4.c)
│   └── u8g2/                         # Đặt source C của thư viện U8g2
├── bsp/                              # Board Support Package (Độc quyền quản lý bởi Dev 1)
│   ├── bsp_board.h / .c              # Định nghĩa Pinout LQFP48, Clock 72MHz, NVIC, SWD Remap, Watchdog IWDG
│   ├── stm32f10x_conf.h              # Khai báo các module SPL sử dụng
│   └── stm32f10x_it.h / .c           # Trình phục vụ ngắt ngoại lệ (HardFault tự ngắt PWM bảo vệ mũi hàn)
├── hal/                              # Tầng trừu tượng phần cứng viết bằng thư viện chuẩn STD/SPL
│   ├── inc/                          # Hợp đồng giao diện Driver (Đã khóa nguyên mẫu hàm)
│   └── src/                          # Cài đặt chi tiết bởi 3 thành viên trên nhánh feature/hal-*
├── app/                              # Tầng ứng dụng Đa nhiệm FreeRTOS
│   ├── inc/                          # app_cfg.h, app_ipc.h, task_sensor_pid.h, task_display.h, task_input_comm.h
│   └── src/                          # app_ipc.c và các file thực thi Task của 3 thành viên
├── main.c                            # Khởi tạo hệ thống và chạy vTaskStartScheduler()
└── .gitignore                        # Chặn file biên dịch rác
```

---

## 2. Phân Công Nhánh Git Cho 3 Thành Viên

| Thành Viên | Nhánh Giai Đoạn 1 (Driver STD/SPL) | Nhánh Giai Đoạn 2 (FreeRTOS Task) | Các Tập Tin Phụ Trách Chính |
| :--- | :--- | :--- | :--- |
| **Dev 1 (Leader)** | `feature/hal-timer-adc` | `feature/task-sensor` | `hal_adc.c`, `hal_timer.c`, `curve_fitting.c`, `task_sensor_pid.c`, `bsp/*`, `app_ipc.c`, `main.c` |
| **Dev 2 (UI & Flash)** | `feature/hal-i2c-spi` | `feature/task-display` | `hal_i2c.c`, `hal_flash.c`, `task_display.c`, `ui_main.c`, `ui_menu.c` |
| **Dev 3 (Input & Comm)** | `feature/hal-gpio-uart` | `feature/task-comm` | `hal_gpio.c`, `hal_uart.c`, `task_input_comm.c` (`Task_InputSleep`, `Task_Buzzer`, `Task_DebugUART`) |

---

## 3. Lệnh Git Khởi Tạo Dành Cho Nhóm Trưởng (Dev 1)

```bash
cd t12-stm32-freertos-std
git init
git checkout -b main
git add .
git commit -m "chore(init): setup base project structure, bsp, ipc and api headers (STD/SPL + FreeRTOS)"
git checkout -b develop
# Push cả main và develop lên GitHub/GitLab của nhóm:
# git remote add origin <URL_REPO_CUA_NHOM>
# git push -u origin main
# git push -u origin develop
```

