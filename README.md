Beyblade RPM Detector Device

Open-source firmware and mechanical design for a Beyblade RPM Detector.

Web Interface

Use the BRD Web interface here:

https://printfchins.github.io/Beyblade-RPM-Detector-Web/

Related Repository

Web application source code:

https://github.com/printfChins/Beyblade-RPM-Detector-Web

<img width="1849" height="723" alt="Beyblade RPM Detector" src="https://github.com/user-attachments/assets/cdb0cbb6-7182-4177-a16d-638426546b69" />

中文說明
Beyblade RPM Detector Device

Beyblade RPM Detector（BRD）的開源裝置端專案。

本 Repository 主要包含 BRD 的：

ESP32-C3 韌體
RPM 量測程式
BLE 通訊功能
OLED 顯示功能
機構設計檔案
Web 上位機

BRD Web 上位機可直接使用以下網址：

https://printfchins.github.io/Beyblade-RPM-Detector-Web/

可透過支援 Web Bluetooth 的瀏覽器連接 BRD 裝置，查看即時 RPM、最大 RPM、發射瞬間 RPM 與 RPM 曲線資料。

相關專案

Web 上位機原始碼 Repository：

https://github.com/printfChins/Beyblade-RPM-Detector-Web

兩個 Repository 的分工如下：

Beyblade-RPM-Detector
│
├── Beyblade-RPM-Detector-Device
│   ├── ESP32-C3 Firmware
│   └── Mechanical Design
│
└── Beyblade-RPM-Detector-Web
    ├── Web UI
    ├── Web Bluetooth
    ├── BLE Packet Parser
    └── RPM Visualization

中文：

Beyblade-RPM-Detector
│
├── Beyblade-RPM-Detector-Device
│   ├── ESP32-C3 韌體
│   └── 機構設計
│
└── Beyblade-RPM-Detector-Web
    ├── Web 上位機
    ├── Web Bluetooth
    ├── BLE 封包解析
    └── RPM 曲線顯示
