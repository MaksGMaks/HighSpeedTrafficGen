# 🚀 High-Speed Traffic Generator – Build & Usage Guide

## 🛠️ How to Build

The build process is mostly automated and consists of three simple steps:

### 1. Create and Enter the Build Directory

```bash
mkdir build && cd build
```

### 2. Run CMake Preset

```bash
cmake ..
```

> 🔐 **Note:** This step may request `sudo` rights to install dependencies such as PF_RING, DPDK, and Qt.  
> The function `install_package_if_missing` ensures missing packages are installed.  
> If unsure about giving sudo permissions, review the script to see which packages will be installed.

> 📦 **Qt Note:**  
> Qt is too large to install automatically. Please install it manually from the official site:  
> [Download Qt](https://www.qt.io/download-qt-installer-oss)

Set the Qt path either in your `.bashrc`:
```bash
export CMAKE_PREFIX_PATH=/path/to/Qt6/lib/cmake
```
Or directly via the CMake command:
```bash
cmake -DCMAKE_PREFIX_PATH=/path/to/Qt6/lib/cmake -DCMAKE_PREFIX_PATH=/path/to/Qt6/bin/lrelease ..
```

### 3. Build the Project

```bash
cmake --build .
```

---

## ▶️ How to Use

Run the binary with:

```bash
sudo ./TrafficGenerator
```

> ⚠️ **Before running**, ensure proper setup for the packet processing backend (PF_RING, PF_RING_ZC, or DPDK).

---

### 🌀 PF_RING Setup

Load the kernel module manually:

```bash
sudo insmod ./pf_ring.ko [min_num_slots=N] [enable_tx_capture=1|0] [enable_ip_defrag=1|0]
```

**Parameters:**
- `min_num_slots` – Minimum number of packets the kernel module can enqueue (default: `4096`)
- `enable_tx_capture` – `1` to capture outgoing packets, `0` to disable TX capture (default: RX+TX)
- `enable_ip_defrag` – `1` to enable IP defragmentation (RX only), `0` to disable (default: disabled)

---

### ⚙️ PF_RING ZC Setup

Unload any previously loaded PF_RING module:

```bash
sudo rmmod pf_ring
```

Then load PF_RING ZC driver using the helper script:

```bash
sudo ./load_driver.sh
```

> 📍 Script location: `third-party/pf_ring_drivers/<driver_name>/load_driver.sh`  
> 📝 Not all network interfaces are compatible with PF_RING ZC.

---

### ⚡ DPDK Setup

1. **Check current device bindings:**
```bash
sudo dpdk-devbind.py -s
```

2. **Disable the interface you plan to use:**
```bash
sudo ifconfig <interface> down
```

3. **Load kernel modules for DPDK:**
```bash
sudo modprobe uio
sudo modprobe uio_pci_generic
```

4. **Bind the interface to the DPDK driver:**
```bash
sudo dpdk-devbind.py -b uio_pci_generic <PCI_ID_of_interface>
```

> 💡 You can re-enable the interface with a system reboot.

---

## 🇺🇦 Інструкція українською

### 🛠️ Як зібрати

Збірка переважно автоматизована:

#### 1. Створіть директорію збірки та перейдіть у неї

```bash
mkdir build && cd build
```

#### 2. Запустіть CMake

```bash
cmake ..
```

> 🔐 **Примітка:** Буде потрібен `sudo`, щоб встановити залежності: PF_RING, DPDK, Qt.  
> Функція `install_package_if_missing` перевіряє та встановлює відсутні пакунки.

> 📦 **Qt:**  
> Занадто великий для автоматичної установки. Завантажити вручну з:  
> [Завантажити Qt](https://www.qt.io/download-qt-installer-oss)

Вкажіть шлях до Qt через `.bashrc`:
```bash
export CMAKE_PREFIX_PATH=/шлях/до/Qt6/lib/cmake
```
Або безпосередньо через команду CMake:
```bash
cmake -DCMAKE_PREFIX_PATH=/шлях/до/Qt6/lib/cmake -DCMAKE_PREFIX_PATH=/шлях/до/Qt6/bin/lrelease ..
```

#### 3. Збірка проєкту

```bash
cmake --build .
```

---

### ▶️ Як використовувати

Запустіть програму:

```bash
sudo ./TrafficGenerator
```

> ⚠️ **Перед запуском потрібно налаштувати бекенд обробки пакетів (PF_RING, PF_RING_ZC або DPDK).**

---

### 🌀 Налаштування PF_RING

Завантажити модуль ядра:

```bash
sudo insmod ./pf_ring.ko [min_num_slots=N] [enable_tx_capture=1|0] [enable_ip_defrag=1|0]
```

**Параметри:**
- `min_num_slots` – Мінімальна кількість слотів (типово: `4096`)
- `enable_tx_capture` – `1` для перехоплення вихідних пакетів, `0` для вимкнення (типово: RX+TX)
- `enable_ip_defrag` – `1` для дефрагментації IP (тільки RX), `0` для вимкнення (типово: вимкнено)

---

### ⚙️ Налаштування PF_RING ZC

Видаліть вже завантажений PF_RING:

```bash
sudo rmmod pf_ring
```

Завантажте драйвер ZC через скрипт:

```bash
sudo ./load_driver.sh
```

> 📍 Скрипт знаходиться у: `third-party/pf_ring_drivers/<driver_name>/load_driver.sh`  
> 📝 Не всі мережеві інтерфейси підтримують PF_RING ZC.

---

### ⚡ Налаштування DPDK

1. **Перевірка прив'язки пристроїв:**
```bash
sudo dpdk-devbind.py -s
```

2. **Вимкнення інтерфейсу:**
```bash
sudo ifconfig <інтерфейс> down
```

3. **Завантаження модулів ядра:**
```bash
sudo modprobe uio
sudo modprobe uio_pci_generic
```

4. **Прив'язка інтерфейсу до драйвера DPDK:**
```bash
sudo dpdk-devbind.py -b uio_pci_generic <PCI_ID_інтерфейсу>
```

> 💡 Інтерфейс буде активовано після перезавантаження.

---

🔗 **Проєкт для ентузіастів високошвидкісного мережевого трафіку. Підтримка PF_RING, PF_RING ZC, DPDK.**