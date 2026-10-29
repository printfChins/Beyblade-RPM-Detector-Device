# BRD BPX V1.2 BLE OTA 設計

使用者已確認方案 A：ESP32-C3、Arduino、NimBLE、自有 OTA UUID，既有 GitHub Pages 網站新增 Web Bluetooth 更新入口。

## 範圍

- [新增] 獨立 OTA 服務，不修改 BBP 與設定服務 UUID。
- [新增] 閒置且電池有效、SOC 至少 20% 或正在充電才接受開始；量測捕捉在同一個安全窗口暫停，OTA 期間保留 BLE、禁止其他 Flash 寫入。
- [新增] 20-byte 控制/回覆，資料依裝置回報的 MTU 上限分包，最多 240 bytes 實際資料。
- [新增] 一次一包，成功寫入才回覆累計位移；重送完全相同的最後一包不重複寫入。
- [新增] 15 秒無有效傳輸、取消、斷線或低電/ADC 故障時中止未提交的 OTA，保留原開機分區。
- [新增] ESP32-C3 應用映像表頭、BRD 標記、長度、串流及 Flash 讀回 MD5、ESP-IDF 映像驗證通過後才切換開機分區，並於 5 秒後重啟。
- [修改] 版本 V1.2，OLED 顯示 OTA、VERIFY、DONE 及百分比；校驗前最多 99%。
- [保留] 既有 USB 完整燒錄從 0x000000 寫入並全擦除；OTA 僅寫備用 app 分區，不修改 NVS 或分割表。

## 協議

UUID 前綴為 c7570010、c7570011、c7570012、c7570013，後綴均為 -9040-4c7b-87b1-086ef858ace7，分別代表 Service、Control、Data、Status。

控制命令格式為 opcode、transaction_id、payload，均使用 little endian：INFO=1 無 payload；HASH=2 帶 16-byte MD5；BEGIN=3 帶 uint32 大小；END=4；ABORT=5。

DATA=16，格式為 opcode、transaction_id、uint32 offset、1..240 bytes 資料。HASH/BEGIN/DATA/END/ABORT 使用同一個非零 transaction_id。

每個回覆固定 20 bytes：opcode[0]、id[1]、result[2]、phase[3]、offset[4..7]、total[8..11]、capacity[12..15]、payload_limit[16..17]、flags[18]、protocol_version=1[19]。flags bit0=充電、bit1=電源允許、bit2=允許開始。

Phase：IDLE=0、RECEIVING=1、COMPLETE=2、ERROR=3。Result：OK=0、BAD_LENGTH=1、BUSY=2、POWER=3、SIZE=4、SESSION=5、OFFSET=6、IMAGE=7、WRITE=8、VERIFY=9、TIMEOUT=10、BAD_COMMAND=11。

網頁只接受 ESP32-C3 的純 app BIN，且必須含 BRD_BPX_OTA_IMAGE_V1_C3 標記。此標記與 MD5 用於相容性及完整性，並非發布者簽章。

## 邊界與交付

不承諾跨斷線續傳、不實作差分更新、不新增 Wi-Fi 或 App。斷線後重新選擇裝置並從頭更新。V1.1 第一次須 USB 安裝 V1.2。雙分區不等同已實作開機自檢回退。

交付完整 Arduino 專案與網站草稿 PR。主機可驗證邏輯與硬體邊界；無 ESP32-C3 實機及 Arduino 工具鏈時，不附冒稱已編譯的新 BIN。

提交邊界以目前連線世代的原子 commit ticket 檢查為起點，Flash API 放在 critical section 外；取得 ticket 後不可取消。
