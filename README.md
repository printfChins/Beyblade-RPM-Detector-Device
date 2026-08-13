# Beyblade RPM Detector Web

Web-based user interface for the **Beyblade RPM Detector (BRD)**.

This repository contains the browser-side application used to connect to a BRD device over Bluetooth Low Energy (BLE), display real-time RPM telemetry, receive launch events and RPM curve data, and manage measurement records.

> Device firmware and mechanical design files are maintained separately in [Beyblade-RPM-Detector-Device](https://github.com/printfChins/Beyblade-RPM-Detector-Device).

---

## Features

- Connect to a BRD device through Web Bluetooth.
- Display real-time RPM, maximum RPM, launch RPM, elapsed time, and device state.
- Receive and visualize complete RPM curve data.
- Mark the launch position on recorded RPM data.
- Store measurement history locally in the browser.
- Display BLE protocol and system event logs.
- Export and import measurement records from the Web UI.
- Detect unsupported browser/platform environments and provide BLE compatibility warnings.

---

## BRD BLE Interface

The Web application communicates with the BRD device using the following GATT service and characteristic:

```text
Service UUID:
7f510001-1b15-4d5f-9f4d-9b3c7a1d9a10

Characteristic UUID:
7f510002-1b15-4d5f-9f4d-9b3c7a1d9a10
```

The current BRD API CMD packet types handled by the Web application are:

| Packet | ID | Description |
|---|---:|---|
| LIVE | `0xB1` | Real-time device state and RPM telemetry |
| LAUNCH | `0xB2` | Launch event and launch RPM information |
| CURVE_START | `0xA1` | RPM curve metadata and measurement summary |
| CURVE_DATA | `0xA2` | RPM curve sample data |
| CURVE_END | `0xA3` | End of RPM curve transmission |

Binary packet fields are decoded in **Little Endian** format.

---

## Device States

| Value | State | Description |
|---:|---|---|
| `0` | `WAIT_LOAD` | Waiting for a Beyblade to be loaded |
| `1` | `LOADED_READY` | Beyblade loaded and ready |
| `2` | `SPINNING_LOADED` | RPM measurement while still loaded |
| `3` | `SPINNING_LAUNCHED` | Beyblade launched; RPM measurement continues |
| `4` | `RESULT_PENDING` | Measurement result pending or transmitting |

---

## Browser Requirements

This project uses the **Web Bluetooth API**.

Use a browser and operating system combination that supports Web Bluetooth. The application performs runtime checks and reports unsupported environments before attempting a BLE connection.

Web Bluetooth support depends on the operating system and browser, so BLE availability may differ between desktop and mobile platforms.

---

## Technology Stack

- React 19
- TypeScript
- Vite
- Tailwind CSS
- Lucide React
- Motion
- Web Bluetooth API

---

## Development

### Requirements

Install Node.js and npm.

### Install Dependencies

```bash
npm install
```

### Start Development Server

```bash
npm run dev
```

The development server is configured to run on port `3000`.

### TypeScript Check

```bash
npm run lint
```

### Production Build

```bash
npm run build
```

### Preview Production Build

```bash
npm run preview
```

---

## Project Structure

```text
Beyblade-RPM-Detector-Web/
├── assets/
├── src/
│   ├── components/
│   │   ├── RpmChart.tsx
│   │   └── Sparkline.tsx
│   │
│   ├── utils/
│   │   └── bleParser.ts
│   │
│   ├── App.tsx
│   ├── index.css
│   ├── main.tsx
│   └── types.ts
│
├── .env.example
├── index.html
├── metadata.json
├── package.json
├── tsconfig.json
└── vite.config.ts
```

### Main Files

| File | Description |
|---|---|
| `src/App.tsx` | Main Web application and BLE communication logic |
| `src/utils/bleParser.ts` | BRD BLE binary packet parser |
| `src/components/RpmChart.tsx` | RPM curve visualization |
| `src/components/Sparkline.tsx` | Compact RPM chart |
| `src/types.ts` | BRD data structures and TypeScript type definitions |
| `src/main.tsx` | React application entry point |

---

## Related Repository

### Beyblade RPM Detector Device

Firmware and mechanical design files for the physical BRD device are maintained in:

[Beyblade-RPM-Detector-Device](https://github.com/printfChins/Beyblade-RPM-Detector-Device)

The repositories are separated by responsibility:

```text
Beyblade-RPM-Detector
│
├── Beyblade-RPM-Detector-Web
│   ├── Web UI
│   ├── Web Bluetooth
│   ├── BLE packet parser
│   ├── RPM visualization
│   └── Measurement history
│
└── Beyblade-RPM-Detector-Device
    ├── ESP32-C3 firmware
    └── Mechanical design files
```

---

## License

Individual source files currently include their applicable SPDX license identifiers.

Refer to the repository files and license documentation for licensing information.

---

# 中文說明

# Beyblade RPM Detector Web

**Beyblade RPM Detector（BRD）網頁版上位機。**

本 Repository 為 BRD 的瀏覽器端上位機程式，透過 Bluetooth Low Energy（BLE）與 BRD 裝置進行通訊，可顯示即時 RPM、接收發射事件、RPM 曲線資料，以及管理量測歷史紀錄。

> BRD 裝置端韌體與機構設計檔案請參考 [Beyblade-RPM-Detector-Device](https://github.com/printfChins/Beyblade-RPM-Detector-Device)。

---

## 功能

- 透過 Web Bluetooth 連接 BRD 裝置。
- 顯示即時 RPM。
- 顯示最大 RPM。
- 顯示發射瞬間 RPM。
- 顯示量測經過時間與裝置狀態。
- 接收並顯示完整 RPM 曲線資料。
- 在 RPM 曲線中標記陀螺發射瞬間的位置。
- 將量測歷史紀錄儲存在瀏覽器本機。
- 顯示 BLE 通訊協議與系統事件紀錄。
- 支援從 Web UI 匯入與匯出量測紀錄。
- 自動檢查瀏覽器與平台是否支援 Web Bluetooth。

---

## BRD BLE 通訊介面

Web 上位機透過以下 BLE GATT Service UUID 與 Characteristic UUID 和 BRD 裝置進行通訊：

```text
Service UUID:
7f510001-1b15-4d5f-9f4d-9b3c7a1d9a10

Characteristic UUID:
7f510002-1b15-4d5f-9f4d-9b3c7a1d9a10
```

目前 Web 上位機支援以下 BRD API CMD 封包：

| 封包 | ID | 說明 |
|---|---:|---|
| LIVE | `0xB1` | 即時裝置狀態與 RPM 資料 |
| LAUNCH | `0xB2` | 發射事件與發射瞬間 RPM |
| CURVE_START | `0xA1` | RPM 曲線起始資訊與量測摘要 |
| CURVE_DATA | `0xA2` | RPM 曲線取樣資料 |
| CURVE_END | `0xA3` | RPM 曲線傳輸結束 |

所有多位元組數值皆使用 **Little Endian（小端序）** 進行解析。

---

## 裝置狀態

| 數值 | 狀態 | 說明 |
|---:|---|---|
| `0` | `WAIT_LOAD` | 等待裝載陀螺 |
| `1` | `LOADED_READY` | 陀螺已裝載並準備量測 |
| `2` | `SPINNING_LOADED` | 裝載狀態下進行 RPM 量測 |
| `3` | `SPINNING_LAUNCHED` | 陀螺已發射，持續進行 RPM 量測 |
| `4` | `RESULT_PENDING` | 量測結果等待處理或傳輸中 |

---

## 瀏覽器需求

本專案使用 **Web Bluetooth API** 與 BRD 裝置進行 BLE 通訊。

請使用支援 Web Bluetooth API 的瀏覽器與作業系統。

程式會在嘗試建立 BLE 連線前，自動檢查目前瀏覽器環境是否支援 Web Bluetooth。

Web Bluetooth 的支援程度會依瀏覽器、作業系統及行動平台而有所不同。

---

## 使用技術

- React 19
- TypeScript
- Vite
- Tailwind CSS
- Lucide React
- Motion
- Web Bluetooth API

---

## 開發環境

### 環境需求

請先安裝 Node.js 與 npm。

### 安裝相依套件

```bash
npm install
```

### 啟動開發伺服器

```bash
npm run dev
```

開發伺服器預設使用：

```text
Port 3000
```

### TypeScript 型別檢查

```bash
npm run lint
```

### 建立正式版本

```bash
npm run build
```

### 預覽正式版本

```bash
npm run preview
```

---

## 專案結構

```text
Beyblade-RPM-Detector-Web/
├── assets/
├── src/
│   ├── components/
│   │   ├── RpmChart.tsx
│   │   └── Sparkline.tsx
│   │
│   ├── utils/
│   │   └── bleParser.ts
│   │
│   ├── App.tsx
│   ├── index.css
│   ├── main.tsx
│   └── types.ts
│
├── .env.example
├── index.html
├── metadata.json
├── package.json
├── tsconfig.json
└── vite.config.ts
```

### 主要檔案

| 檔案 | 說明 |
|---|---|
| `src/App.tsx` | Web 上位機主程式與 BLE 通訊邏輯 |
| `src/utils/bleParser.ts` | BRD BLE 二進位封包解析器 |
| `src/components/RpmChart.tsx` | RPM 曲線圖顯示 |
| `src/components/Sparkline.tsx` | 縮略 RPM 曲線顯示 |
| `src/types.ts` | BRD 資料結構與 TypeScript 型別定義 |
| `src/main.tsx` | React 程式進入點 |

---

## 相關專案

### Beyblade RPM Detector Device

BRD 實體裝置的韌體與機構設計檔案位於：

[Beyblade-RPM-Detector-Device](https://github.com/printfChins/Beyblade-RPM-Detector-Device)

兩個 Repository 的功能分工如下：

```text
Beyblade-RPM-Detector
│
├── Beyblade-RPM-Detector-Web
│   ├── Web 上位機
│   ├── Web Bluetooth
│   ├── BLE 封包解析
│   ├── RPM 曲線顯示
│   └── 量測歷史紀錄
│
└── Beyblade-RPM-Detector-Device
    ├── ESP32-C3 韌體
    └── 機構設計檔案
```

### Beyblade-RPM-Detector-Web

負責：

- BLE 裝置連線。
- BLE 封包解析。
- 即時 RPM 顯示。
- 最大 RPM 顯示。
- 發射瞬間 RPM 顯示。
- RPM 曲線視覺化。
- 發射點標記。
- 量測歷史紀錄管理。

### Beyblade-RPM-Detector-Device

負責：

- ESP32-C3 韌體。
- IR RPM 轉速量測。
- 陀螺裝載偵測。
- 發射瞬間偵測。
- BLE GATT 通訊。
- OLED 顯示。
- 機構設計檔案。

---

## 授權

目前部分原始碼檔案內包含各自的 SPDX License Identifier。

實際授權方式請依 Repository 中的授權文件與原始碼標示為準。
