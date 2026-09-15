![CAN-адаптер селектора КПП Nissan](http://aelectrik.ru/wp-content/uploads/2025/05/gijoijmko.webp)


# CAN-адаптер селектора КПП для Nissan (CH32V203C8T6)

Прошивка для микроконтроллера **CH32V203C8T6**, которая читает состояние
селектора автоматической коробки передач (P / R / N / D / L / M2 / M3)
и отправляет соответствующий CAN-кадр в шину автомобиля.

Проект является портом оригинальной прошивки под STM32F103C8T6 (HAL)
на более дешёвый RISC-V микроконтроллер CH32V203C8T6 (WCH).

---

## Поддерживаемые автомобили

Выбор модели задаётся макросом в `platformio.ini` (секция `build_flags`).
Одновременно должен быть раскомментирован **ровно один** макрос.

| Макрос                       | Автомобиль                     | CAN ID | DLC | Байт-модификатор |
|------------------------------|--------------------------------|--------|-----|------------------|
| `MURANO_Z50`                 | Nissan Murano Z50              | 0x255  | 8   | 5                |
| `MURANO_Z50_PRND2_3minus`    | Nissan Murano Z50 (PRND + M2/M3)| 0x255 | 8   | 5                |
| `NOUT_3minus`                | Nissan Note (без режима M3)    | 0x255  | 8   | 5                |
| `JUKE`                       | Nissan Juke                    | 0x421  | 3   | 0                |
| `JUKE_Ds`                    | Nissan Juke (режим Ds)         | 0x421  | 3   | 0                |

Дополнительно можно включить макрос `ESP` — в этом режиме прошивка
периодически отправляет три фиксированных кадра (0x174, 0x176, 0x177)
для отладки или эмуляции другого узла.

---

## Аппаратная часть

### Микроконтроллер

- **CH32V203C8T6**, корпус LQFP48
- RISC-V, 144 МГц (в проекте используется 8 МГц напрямую от HSE)
- 64 КБ Flash, 20 КБ RAM

### Тактирование

- Внешний кварц **8 МГц** на пинах **PD0 / PD1**
- PLL **не используется** — SYSCLK = HSE = 8 МГц
- Это даёт максимальную помехоустойчивость и упрощает расчёт таймингов CAN

### Подключение CAN

| Пин MCU | Сигнал | Назначение           |
|---------|--------|----------------------|
| PA11    | CAN_RX | Вход приёмника       |
| PA12    | CAN_TX | Выход передатчика    |

Требуется внешний CAN-трансивер (например, **TJA1050** или **SN65HVD230**).
CH32V203 содержит только CAN-контроллер, без встроенного трансивера.

### Подключение селектора КПП

| Пин MCU | Назначение                        |
|---------|-----------------------------------|
| PA2     | P (Park)                          |
| PA3     | R (Reverse)                       |
| PA4     | N (Neutral)                       |
| PA5     | D (Drive)                         |
| PA6     | L / Ds / M2 (зависит от модели)   |
| PA7     | M3 (только для *_3minus вариантов)|
| PB0     | Свободный вход (резерв)           |

Все входы имеют подтяжку (вниз или вверх) в зависимости от модели.

---

## Программная часть

### Стек

- **PlatformIO** с платформой [Community-PIO-CH32V](https://github.com/Community-PIO-CH32V/platform-ch32v)
- Фреймворк: **noneos-sdk** (без операционной системы)
- Язык: **C** (файл должен оставаться `main.c`, не `main.cpp`)

### platformio.ini

```ini
[env:genericCH32V203C8T6]
platform = https://github.com/Community-PIO-CH32V/platform-ch32v.git
board = genericCH32V203C8T6
framework = noneos-sdk

build_flags =
    -DJUKE
    ; -DMURANO_Z50
    ; -DNOUT_3minus
    ; -DMURANO_Z50_PRND2_3minus
    ; -DJUKE_Ds
    -DESP
```

## Как это работает

1. **Тактирование.** После сброса функция `SetClockTo8MHzHSE()` принудительно
   переключает SYSCLK на HSE (8 МГц) без PLL. Это гарантирует, что CAN
   будет работать на точной частоте 500 кбит/с.

2. **CAN.** Настраивается на 500 кбит/с:
   - `Prescaler = 1`
   - `BS1 = 13 TQ`, `BS2 = 2 TQ`
   - `8 000 000 / (1 × (1 + 13 + 2)) = 500 000`

3. **Таймер.** TIM2 вызывает прерывание каждые **10 мс** (или 50 мс для JUKE).
   В обработчике `TIM2_IRQHandler`:
   - читается состояние входов селектора,
   - состояние фильтруется (ровно один активный вход = валидное положение),
   - обновляется глобальная переменная `last_sent_byte`,
   - отправляется CAN-кадр с актуальным положением селектора.

4. **Отправка.** Функция `CAN_SendFrame()` формирует стандартный кадр
   (11-битный ID, DLC, до 8 байт данных) и передаёт его в контроллер CAN1.

---

## Структура проекта

```
├── platformio.ini # конфигурация PlatformIO
├── src/
│ └── main.c # единственный файл прошивки
└── README.md
```


---

## Сборка и прошивка

### Требования

- PlatformIO Core (или VSCode + расширение PlatformIO)
- Программатор **WCH-LinkE** (или WCH-Link v2)
- Драйверы WCH для программатора

### Сборка

```bash
pio run
```

## Лицензия

Код распространяется «как есть», без каких-либо гарантий.
Использование в автомобиле — на ваш собственный риск.

## Разработка кастомной электроники (Контакты)

Я занимаюсь профессиональной разработкой и производством контрактной, автомобильной и кастомной электроники. Если вам требуется:
- Проектирование устройств и печатных плат (от идеи до серии);
- Реверс-инжиниринг и анализ автомобильных CAN-шин;
- Портирование устаревших/дорогих проектов (STM32, AVR, PIC) на современные и дешевые платформы (CH32V, GD32, RP2040, ESP32);
- Написание надежного встроенного ПО (Bare-metal / RTOS).

**Связаться со мной:**
- 📧 **Email:** [kozlovalex78@inbox.ru]
- 💬 **DRIVE2:** [@alex-kozlov]
- 🌐 **Портфолио / Сайт:** [https://22.aelectrik.ru]

## Дополнительные материалы

Также являюсь автором статей по реверс-инжинирингу автомобильных блоков управления:
- [Питание силовой части блока Denso](https://22.aelectrik.ru/stati/revers-inzhiniring-bloka-sertifikaci/)
- [Питание цифровой части блока Denso](https://22.aelectrik.ru/stati/revers-inzhiniring-bloka-sertifikaci-2/)
- [Подсмотрели обмен данными процессора и EEPROM S93C86](https://22.aelectrik.ru/stati/revers-inzhiniring-bloka-smart-key-toyota-chast-3-cif/)
- [Обвязка CAN трансивера блока Denso](https://aelectrik.ru/stati/kak-podklyuchen-kan-transiver-v-bloke-denso/)
- [Управление реле муфты кондиционера](https://aelectrik.ru/stati/revers-inzhiniring-yebu-fujitsu-ten-toyota-1nz-fe-2002-gg-tayny/)
- и многих других

# CAN Adapter for Nissan Gear Selector (CH32V203C8T6)

Firmware for the **CH32V203C8T6** microcontroller that reads the physical state of an automatic transmission gear selector (P / R / N / D / L / M2 / M3) and transmits the corresponding CAN frame into the vehicle's bus.

This project is a port of the original firmware written for STM32F103C8T6 (HAL) to a more cost-effective RISC-V microcontroller, the CH32V203C8T6 (WCH Peripheral Library).

---

## Supported Vehicles

The vehicle model is selected using preprocessor macros in `platformio.ini` (under `build_flags`). **Exactly one** vehicle macro must be uncommented at a time.

| Macro                        | Vehicle Model                  | CAN ID | DLC | Modifier Byte |
|------------------------------|--------------------------------|--------|-----|---------------|
| `MURANO_Z50`                 | Nissan Murano Z50              | 0x255  | 8   | 5             |
| `MURANO_Z50_PRND2_3minus`    | Nissan Murano Z50 (PRND+M2/M3) | 0x255  | 8   | 5             |
| `NOUT_3minus`                | Nissan Note (without M3 mode)  | 0x255  | 8   | 5             |
| `JUKE`                       | Nissan Juke                    | 0x421  | 3   | 0             |
| `JUKE_Ds`                    | Nissan Juke (Ds mode)          | 0x421  | 3   | 0             |

Additionally, the `ESP` macro can be enabled. In this mode, the firmware periodically transmits three fixed frames (`0x174`, `0x176`, `0x177`) for debugging or node emulation purposes.

---

## Hardware Configuration

### Microcontroller

- **CH32V203C8T6**, LQFP48 package
- RISC-V core up to 144 MHz (configured to run at **8 MHz** directly from HSE in this project)
- 64 KB Flash, 20 KB RAM

### Clock Tree

- External **8 MHz** crystal oscillator connected to pins **PD0 / PD1**
- PLL **is bypassed** — `SYSCLK = HSE = 8 MHz`
- This ensures maximum noise immunity in automotive environments and simplifies CAN bit-timing calculations.

### CAN Bus Wiring

| MCU Pin | Signal | Description         |
|---------|--------|---------------------|
| PA11    | CAN_RX | Transceiver RX data |
| PA12    | CAN_TX | Transceiver TX data |

*Note: A dedicated external CAN transceiver (e.g., **TJA1050**, **SN65HVD230**, or **VP230**) is strictly required. The CH32V203 includes the CAN controller on-chip but lacks the physical layer transceiver.*

### Gear Selector Inputs

| MCU Pin | Function                          |
|---------|-----------------------------------|
| PA2     | P (Park)                          |
| PA3     | R (Reverse)                       |
| PA4     | N (Neutral)                       |
| PA5     | D (Drive)                         |
| PA6     | L / Ds / M2 (Vehicle dependent)   |
| PA7     | M3 (For `*_3minus` options only)  |
| PB0     | Unused / Reserved input           |

*All digital inputs utilize internal pull-up or pull-down resistors automatically configured depending on the selected vehicle profile.*

---

## Software Architecture

### Toolchain & Framework

- **PlatformIO** powered by the [Community-PIO-CH32V](https://github.com/Community-PIO-CH32V/platform-ch32v) platform.
- Framework: **noneos-sdk** (Bare-metal Manufacturer's Peripheral Drivers).
- Language: **Pure C** (the source code must remain as `src/main.c`).

### platformio.ini Configuration

```ini
[env:genericCH32V203C8T6]
platform = https://github.com/Community-PIO-CH32V/platform-ch32v.git
board = genericCH32V203C8T6
framework = noneos-sdk

build_flags =
    -DJUKE
    ; -DMURANO_Z50
    ; -DNOUT_3minus
    ; -DMURANO_Z50_PRND2_3minus
    ; -DJUKE_Ds
    -DESP
```

## Core Workflow

1. **Clock Configuration:** Upon reset, `SetClockTo8MHzHSE()` explicitly switches the `SYSCLK` to `HSE` (8 MHz) bypassing the PLL. This ensures precise, jitter-free 500 kbps CAN communication.
2. **CAN Bus Timings:** Configured for 500 kbps bit rate using a 16 TQ bit-time map:
   - `Prescaler = 1`
   - `BS1 = 13 TQ`, `BS2 = 2 TQ`, `SJW = 1 TQ`
   - Calculation: `8,000,000 / (1 × (1 + 13 + 2)) = 500,000 bps` (Sample point at 87.5%).
3. **Timer Event:** TIM2 triggers an update interrupt every **10 ms** (or 50 ms for JUKE). Inside `TIM2_IRQHandler`:
   - Pin states are captured and debounced.
   - Validation ensures exactly one gear position is active.
   - The global `last_sent_byte` variable updates.
   - A CAN frame is pushed out with the valid selector position.
4. **Transmission:** `CAN_SendFrame()` handles internal Mailbox selection and initiates the hardware transmission of the 11-bit standard data frame.

---

