# BX-09 BeyBattle Pass BLE CMD / API 規格書

文件版本：Reverse Engineering API V1.1  
對象裝置：Takara Tomy Beyblade X BX-09 BeyBattle Pass  
通訊介面：Bluetooth Low Energy GATT  
文件性質：非官方，依公開逆向工程與實機觀察整理

---

# 1. 文件目的

本文件整理目前公開資料可確認的 BX-09 BLE 通訊行為，涵蓋：

- BLE Advertising
- GATT Service
- Notify 通道
- Write CMD
- `0x51`
- `0x61`
- `0x74`
- `0x75`
- `0xA0`
- `0xB0 ~ 0xB7`
- `0x70 ~ 0x73`
- Shot History
- Shot Profile
- 曲線原始資料格式
- 曲線時間軸重建
- 曲線 SP 換算
- Checksum
- 主動 Notify
- CMD Request / Response
- 掉包處理
- BRD_BBP 相容實作方式

本文件中的欄位名稱與演算法來自公開逆向專案，並非原廠正式 API。

---

# 2. BLE 角色

BX-09：

```text
BLE Peripheral
GATT Server
```

手機、PC、ATLAS 或其他上位機：

```text
BLE Central
GATT Client
```

基本流程：

```text
Central                         BX-09
   |                              |
   |-------- Scan --------------->|
   |<------- Advertising ---------|
   |                              |
   |-------- Connect ------------>|
   |                              |
   |-------- Discover GATT ------>|
   |                              |
   |-------- Subscribe Notify --->|
   |                              |
   |<------- Notify --------------|
```

---

# 3. Advertising

## 3.1 Device Name

```text
BEYBLADE_TOOL01
```

---

## 3.2 Primary Service UUID

```text
55C40000-F8EB-11EC-B939-0242AC120002
```

---

# 4. GATT Characteristic

目前公開專案明確使用的 Notify Characteristic：

```text
55C4F002-F8EB-11EC-B939-0242AC120002
```

用途：

```text
Device -> Central
Notification
```

Central 連線後需先啟用 Notify。

---

## 4.1 Write Characteristic

公開程式可以確認 BX-09 存在另外一個可寫入 CMD 的 Characteristic。

已確認可寫入：

```text
0x51
0x61
0x74
0x75
```

但目前沒有足夠交叉驗證資料，可以把實機 Write Characteristic UUID 當成正式確定值。

因此：

```text
F002 = Notify Characteristic
```

為已確認。

Write Characteristic UUID：

```text
尚未完全確認
```

不得僅依 UUID 編號規則推測一定是 `F001`。

---

# 5. CMD Write 格式

目前已知 CMD 都是：

```text
Length = 1 byte
```

例如：

```text
51
74
75
```

公開實作可看到：

```text
Write Without Response
```

模式。

相容 Peripheral 建議同時支援：

```text
WRITE
WRITE WITHOUT RESPONSE
```

---

# 6. Device -> Central 封包格式

目前解析的主要 BX-09 封包固定：

```text
17 bytes
```

格式：

```text
Byte 0      Header / Page ID
Byte 1~16   Payload
```

多 byte 數值主要採：

```text
Little Endian
```

例如：

```text
8D 1C
```

解析：

```text
0x1C8D
```

---

# 7. CMD 總表

| CMD | Direction | 功能 |
|---|---|---|
| `0x51` | Central -> BX-09 | 取得目前 Header / Status |
| `0x61` | Central -> BX-09 | 未解析命令 |
| `0x74` | Central -> BX-09 | 取得 Shot History / Profile |
| `0x75` | Central -> BX-09 | 清除儲存紀錄 |

---

# 8. CMD 0x51

Request：

```text
51
```

BX-09 回傳：

```text
A0 xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx
```

一次正常 `0x51` 通常對應一個：

```text
0xA0
```

Notification。

---

# 9. 0xA0 Status Packet

目前整理格式：

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | `0xA0` |
| 1 | 1 | 常見 `0x3A` |
| 2 | 1 | Unknown |
| 3 | 1 | Status / Flags |
| 4 | 1 | Battery Raw |
| 5 | 1 | Unknown |
| 6 | 1 | Unknown |
| 7 | 2 | Maximum SP |
| 9 | 2 | Total Shot Counter |
| 11 | 6 | Device UID |

例如：

```text
A0 3A 00 00 96 00 00 8D 1C 02 00 7B 30 03 51 C4 DA
```

其中：

```text
8D 1C
= 0x1C8D
= 7309
```

```text
02 00
= Shot Counter 2
```

---

# 10. A0 Status 欄位

`A0[3]` 是目前逆向資料中仍存在解讀差異的欄位。

`atlas_bey` 的解讀：

```text
00 -> 04
Bey Attached

04 -> 00
Bey Detached
```

另一模式：

```text
10 -> 14
Attached

14 -> 10
Detached
```

因此相容 `atlas_bey` / `BeyMeter` 時可使用：

```text
0x04 / 0x14 = Loaded
0x00 / 0x10 = Unloaded
```

但 Protocol Layer 建議仍保存：

```c
uint8_t raw_status;
```

不要把整個欄位永久定義成單一物理意義。

---

# 11. 0x51 Polling

部分上位機會使用約：

```text
500 ms
```

週期送：

```text
0x51
```

例如：

```text
Central                   BX-09

   |--- 51 ---------------->|
   |<-- A0 -----------------|
   |
   | 500 ms
   |
   |--- 51 ---------------->|
   |<-- A0 -----------------|
```

但這是：

```text
Host Polling Policy
```

不是 BX-09 Device Protocol 規定的固定 Notify 週期。

---

# 12. 主動 Notify

`atlas_bey`、`BeyMeter` 類實作顯示：

Central 只需要：

```text
Subscribe F002
```

之後可直接等待 Notification。

因此 BX-09 通訊不能只理解成 Polling。

目前較合理的模型：

```text
Push Notify
+
Command / Response
```

兩者共存。

---

# 13. CMD 0x74

Request：

```text
74
```

用途：

```text
取得 Shot History
取得 Shot Profile
```

完整資料通常由：

```text
B0
B1
B2
B3
B4
B5
B6
B7
70
71
72
73
```

組成。

---

# 14. B0 ~ B6 Shot History

`B0 ~ B6` 保存 SP History。

每筆資料：

```text
uint16_t
Little Endian
```

`B0 ~ B5` 每頁可保存 8 筆：

```text
Byte 0      Header

Byte 1~2    Shot 1
Byte 3~4    Shot 2
Byte 5~6    Shot 3
Byte 7~8    Shot 4
Byte 9~10   Shot 5
Byte 11~12  Shot 6
Byte 13~14  Shot 7
Byte 15~16  Shot 8
```

對應：

```text
B0 = Shot 1 ~ 8
B1 = Shot 9 ~ 16
B2 = Shot 17 ~ 24
B3 = Shot 25 ~ 32
B4 = Shot 33 ~ 40
B5 = Shot 41 ~ 48
B6 = Shot 49 ~ 50 + Metadata
```

---

# 15. History Index

第 N 筆 History：

```text
page =
0xB0 + floor((N - 1) / 8)
```

資料 Offset：

```text
offset =
1 + ((N - 1) % 8) * 2
```

---

# 16. History Capacity

目前公開逆向實作採：

```text
50 shots
```

因此：

```text
Total Shot Counter
```

與：

```text
History Buffer Index
```

是不同概念。

Total Counter 可以持續增加。

實際 History 約保存最近：

```text
50
```

筆。

---

# 17. B6 Metadata

公開解析使用：

```text
B6[7..8]   Maximum SP
B6[9..10]  Total Shot Counter
B6[11]     Shot List Count / Index
```

其中：

```text
B6[11]
```

用於判斷目前最新 SP 位於 B0~B6 哪一個位置。

---

# 18. B7 Checksum

`B7`：

```text
Byte 0  = B7
Byte 16 = Checksum
```

Checksum：

```text
sum(
    B0[1..16] +
    B1[1..16] +
    ...
    B6[1..16]
) & 0xFF
```

Header byte：

```text
B0
B1
...
B6
```

本身不加入計算。

---

# 19. 曲線 Profile 總覽

BX-09 的曲線資料不是直接傳：

```text
RPM1
RPM2
RPM3
...
```

也不是：

```text
固定每 50 ms 一點
```

公開逆向實作顯示，BX-09 傳輸的是：

```text
32 個 uint16 原始週期 / reference-count 類數值
```

公開專案通常將其命名為：

```text
nRefs
```

上位機再由 `nRefs` 計算：

```text
SP
```

以及：

```text
該曲線點所佔時間
```

最後重建：

```text
SP vs Time
```

曲線。

---

# 20. 曲線封包

完整 Shot Profile：

```text
70
71
72
73
```

共：

```text
4 packets
```

每包：

```text
17 bytes
```

每包包含：

```text
8 x uint16_t
```

所以總共：

```text
32 points
```

---

# 21. 曲線封包格式

## 0x70

```text
Byte 0      70
Byte 1~2    Point 1
Byte 3~4    Point 2
Byte 5~6    Point 3
Byte 7~8    Point 4
Byte 9~10   Point 5
Byte 11~12  Point 6
Byte 13~14  Point 7
Byte 15~16  Point 8
```

## 0x71

```text
Point 9 ~ Point 16
```

## 0x72

```text
Point 17 ~ Point 24
```

## 0x73

```text
Point 25 ~ Point 32
```

---

# 22. 曲線資料 Endian

每個曲線 Point：

```text
uint16_t Little Endian
```

例如：

```text
F4 01
```

解析：

```text
0x01F4
= 500
```

因此：

```text
nRefs = 500
```

---

# 23. 原廠曲線不是固定時間取樣

這是 BX-09 Profile 與一般固定取樣曲線最大的差異。

不是：

```text
0 ms
50 ms
100 ms
150 ms
...
```

而是每一個 Point 都會導出自己的：

```text
dt
```

所以時間軸是不等距的。

---

# 24. nRefs -> SP

`BeyMeter` 目前公開逆向程式採：

```text
SP = floor(7,500,000 / nRefs)
```

其中：

```text
nRefs != 0
```

例如：

```text
nRefs = 500
```

則：

```text
SP =
7,500,000 / 500

= 15,000
```

因此：

```text
Point Raw = 500
Point SP  = 15000
```

---

# 25. nRefs -> 單點時間

公開解析使用：

```text
dt_ms = nRefs / 125
```

例如：

```text
nRefs = 500
```

則：

```text
dt_ms =
500 / 125

= 4 ms
```

因此同一個 raw value：

```text
500
```

同時決定：

```text
SP = 15000
dt = 4 ms
```

---

# 26. 曲線時間軸累積

Profile 時間不是每點固定給 timestamp。

而是由：

```text
dt_ms
```

逐點累加。

演算法：

```text
elapsed = 0

for each point:
    dt = nRefs / 125
    elapsed += dt

    point.time = elapsed
    point.sp   = 7,500,000 / nRefs
```

例如：

```text
Point  Raw   SP       dt

1      750   10000    6 ms
2      625   12000    5 ms
3      500   15000    4 ms
4      600   12500    4.8 ms
```

最後時間軸：

```text
Point  Time      SP

1      6.0 ms    10000
2     11.0 ms    12000
3     15.0 ms    15000
4     19.8 ms    12500
```

因此曲線為：

```text
SP
^
|                 *
|          *
|                         *
|   *
+----------------------------> Time
```

---

# 27. Raw = 0

公開解析在：

```text
nRefs == 0
```

時會忽略該 Point。

例如：

```text
70
F4 01
20 03
00 00
00 00
...
```

解析：

```text
500
800
```

之後的：

```text
0
```

不會被當成：

```text
SP = infinity
```

而是：

```text
Invalid / Empty Point
```

所以 Parser 必須先判斷：

```c
if (n_refs == 0) {
    continue;
}
```

---

# 28. Profile 有效點數

雖然封包預留：

```text
32 points
```

但不代表每次一定有：

```text
32 個有效值
```

尾端可能：

```text
00 00
```

因此有效 Profile 長度應由：

```text
nRefs != 0
```

的點數決定。

---

# 29. 70~73 曲線解碼演算法

建議 Central Parser：

```text
profile_time = 0

for header = 0x70 to 0x73:

    for point = 0 to 7:

        raw = uint16_le(packet[offset])

        if raw == 0:
            continue

        dt_ms =
            raw / 125.0

        sp =
            floor(7,500,000 / raw)

        profile_time += dt_ms

        curve.push(
            time = profile_time,
            sp   = sp,
            raw  = raw
        )
```

---

# 30. C 型態表示

建議：

```c
typedef struct {
    uint16_t raw;
    float dt_ms;
    float time_ms;
    uint32_t sp;
} bbp_profile_point_t;
```

Profile：

```c
bbp_profile_point_t profile[32];
```

---

# 31. 曲線 Profile 與 History SP 的差異

`B0~B6` 保存：

```text
一筆 Shot 的代表 SP
```

`70~73` 保存：

```text
該 Shot 的動態 Profile
```

所以：

```text
History SP
```

與：

```text
Profile 最大值
```

不一定必須完全相等。

公開逆向程式也會另外計算：

```text
profile max SP
```

---

# 32. Profile Max SP

解析曲線時：

```text
max_sp = max(all valid curve point SP)
```

例如：

```text
10000
12000
15000
14300
13000
```

則：

```text
profile_max_sp = 15000
```

---

# 33. BX-09 Stored SP 與曲線估算

公開 `BeyMeter` 實作會同時保留：

```text
yourSp
```

即 B0~B6 History 中的原廠 SP，

以及：

```text
estSp
```

由 Profile 曲線估算得到的值。

因此上位機應避免假定：

```text
max(profile) == stored SP
```

永遠成立。

---

# 34. 曲線最少有效點

`BeyMeter` 目前實作使用：

```text
PROFILE_MIN_POINTS = 7
```

也就是如果有效曲線資料少於：

```text
7 points
```

則不嘗試完整 Profile SP 推估。

直接 fallback：

```text
estSp = stored SP
```

注意：

```text
7
```

是公開逆向程式的軟體判斷值，不一定是 BX-09 韌體規格本身。

---

# 35. 曲線初始峰值分析

`BeyMeter` 目前會尋找前段 Profile 的第一個合理局部峰值。

判斷大致為：

```text
前一點 < 目前點
且
目前點 >= 下一點
```

並排除太低的峰。

主要使用：

```text
前約 14 個有效點
```

搜尋發射初期峰值。

這屬於：

```text
上位機分析演算法
```

不是 `70~73` BLE Packet Format 本身。

Peripheral 不需要重現這套 Peak Detection。

---

# 36. Curve 與 Launch Marker

公開逆向實作會嘗試將：

```text
A0 Release Event
```

與：

```text
70~73 Profile
```

的接收時間關聯，

用於估算：

```text
launchMarkerMs
```

但 Notification 接收時間會受到：

```text
BLE connection interval
BLE stack queue
Host scheduling
```

影響。

因此：

```text
BLE Packet arrival timestamp
```

不能直接當成實際旋轉量測 timestamp。

真正曲線時間軸仍應使用：

```text
nRefs / 125
```

重建。

---

# 37. 曲線完成條件

完整 Profile 通常以：

```text
0x73
```

作為最後一頁。

因此：

```text
收到 73
```

代表：

```text
Profile transfer reached final page
```

但 Receiver 仍應檢查：

```text
70 exists
71 exists
72 exists
73 exists
```

不能只因收到：

```text
73
```

就假定前面一定完整。

---

# 38. 完整 Shot 資料完成條件

嚴格 Parser 建議要求：

```text
B0
B1
B2
B3
B4
B5
B6
B7
70
71
72
73
```

全部存在。

然後：

```text
1. Validate B0~B6 checksum
2. Parse stored SP
3. Parse 70~73
4. Build Profile
5. Produce Shot Result
```

---

# 39. Packet Map

由於 BX-09 實機資料可能出現：

```text
重複 Header
```

甚至傳輸節奏不完全固定，

Receiver 不應使用：

```text
第 1 包
第 2 包
第 3 包
```

來判斷資料位置。

應：

```text
packet_map[header] = packet
```

例如：

```text
packet_map[0x70]
packet_map[0x71]
packet_map[0x72]
packet_map[0x73]
```

---

# 40. Duplicate Profile Page

例如收到：

```text
70
70
71
72
73
```

建議：

```text
新的 70 覆蓋舊的 70
```

而不是將第二個：

```text
70
```

誤當成：

```text
71
```

---

# 41. 缺 Profile Page

例如：

```text
70
71
73
```

缺少：

```text
72
```

則：

```text
Profile Incomplete
```

不得把：

```text
73
```

的內容當成 Point 17~24。

---

# 42. Profile 無 Checksum

目前公開格式中：

```text
B7
```

Checksum 主要驗證：

```text
B0~B6
```

沒有看到另外一個專門用於：

```text
70~73
```

的 Profile Checksum。

因此 Profile 完整性主要靠：

```text
Page Header completeness
Packet length
BLE Link Layer reliability
```

判斷。

---

# 43. BRD_BBP 曲線資料來源

如果 BRD_BBP 要模擬 BX-09，

不建議繼續直接使用：

```text
每 50 ms 記錄一筆 RPM
```

作為 `70~73` 原始資料。

因為 BX-09 Profile 是：

```text
per-period / per-reference measurement
```

形式。

BRD 本身已經能在：

```text
IR_RPM FALLING edge
```

取得相鄰週期，

因此更適合直接保存：

```text
per revolution period
```

資料。

---

# 44. BRD 的原始 RPM 週期

BRD 現有量測：

```text
period_us =
current_edge_us - previous_edge_us
```

RPM：

```text
RPM =
60,000,000 / period_us
```

---

# 45. BRD RPM -> BX-09 nRefs

如果 BRD_BBP 暫時定義：

```text
BX-09 SP ≈ BRD RPM
```

則由：

```text
SP =
7,500,000 / nRefs
```

反算：

```text
nRefs =
7,500,000 / RPM
```

---

# 46. 編碼範例

假設 BRD：

```text
RPM = 15000
```

則：

```text
nRefs =
7,500,000 / 15000

= 500
```

500：

```text
0x01F4
```

Little Endian：

```text
F4 01
```

因此曲線 Packet 裡存：

```text
F4 01
```

接收端解碼：

```text
SP =
7,500,000 / 500

= 15000
```

時間：

```text
dt =
500 / 125

= 4 ms
```

---

# 47. BRD period_us 直接轉 nRefs

因為：

```text
RPM =
60,000,000 / period_us
```

而：

```text
nRefs =
7,500,000 / RPM
```

代入後：

```text
nRefs =
7,500,000 /
(60,000,000 / period_us)
```

整理：

```text
nRefs =
period_us / 8
```

這是一個非常重要的結果。

也就是對 BRD：

```text
BX-09 raw profile value
≈
IR period_us / 8
```

假設：

```text
period_us = 4000 us
```

則：

```text
nRefs =
4000 / 8

= 500
```

也就是：

```text
F4 01
```

---

# 48. BRD_BBP 最直接的編碼方式

因此 BRD_BBP 不需要：

```text
period -> RPM -> SP -> nRefs
```

繞一圈。

可以直接：

```c
n_refs = period_us / 8;
```

再存入：

```text
70~73
```

---

# 49. 精度處理

`nRefs` 為：

```text
uint16_t
```

建議使用四捨五入：

```c
n_refs = (period_us + 4U) / 8U;
```

而不是永遠向下截斷：

```c
n_refs = period_us / 8U;
```

是否要四捨五入可依實測與 BX-09 原始資料再調整。

---

# 50. uint16 範圍

Profile raw：

```text
1 ~ 65535
```

如果：

```text
nRefs = 0
```

則應視為：

```text
Unused Point
```

所以有效 Point 最低值應至少：

```text
1
```

---

# 51. BRD_BBP Profile Buffer

建議：

```c
#define BBP_PROFILE_POINTS 32U

typedef struct {
    uint16_t n_refs[BBP_PROFILE_POINTS];
    uint8_t count;
} bbp_profile_t;
```

---

# 52. 每圈保存

RPM ISR 或 ISR 後處理流程：

```text
FALLING edge
    ↓
取得 period_us
    ↓
基本有效性檢查
    ↓
nRefs = period_us / 8
    ↓
寫入 profile buffer
```

直到：

```text
32 points
```

或發射量測結束。

---

# 53. 不足 32 點

如果本次有效資料只有：

```text
18 points
```

則：

```text
Point 1~18
寫入有效 nRefs

Point 19~32
填 0x0000
```

因此：

```text
70
71
72
```

可能部分有效。

```text
73
```

可能全部為：

```text
00 00
```

但仍可送出作為 Profile Final Page。

---

# 54. 超過 32 點

如果 BRD 收到：

```text
> 32 revolutions
```

需要定義保留策略。

若目標接近目前公開 BX-09 Profile 使用方式，建議保留：

```text
發射初期的前 32 個有效週期
```

因為主要加速、峰值及發射特徵通常集中在前段。

不要使用：

```text
每 N 點平均
```

除非實測證明 BX-09 原廠也會做重採樣。

目前公開資料沒有證據支持固定平均重採樣。

---

# 55. BRD 原 50 ms Curve Buffer

BRD 原本：

```text
每 50 ms
記錄一次 RPM
```

這種資料適合：

```text
BRD 自有曲線協定
```

但不適合作為：

```text
BX-09 raw 70~73
```

直接來源。

BRD_BBP 建議另建：

```text
BBP per-revolution profile buffer
```

不要破壞原本 BRD Native Curve。

---

# 56. 曲線與發射終止條件分離

Profile 收集與 BRD 發射狀態機應分開。

例如 BRD 可以仍使用：

```text
RPM 降至 MAX 20%
```

或：

```text
1 秒無脈衝
```

作為量測結束。

Profile Buffer 則只是：

```text
保存發射初期最多 32 個週期
```

兩者不應互相限制。

---

# 57. Shot 完成時的 Profile 流程

建議：

```text
Launch detected
     ↓
開始 Profile Capture
     ↓
每個 FALLING edge 保存 period
     ↓
最多 32 點
     ↓
BRD Shot End
     ↓
Freeze Profile
     ↓
Encode 70
Encode 71
Encode 72
Encode 73
```

---

# 58. 70~73 Encode

Point Index：

```text
0~7   -> 70
8~15  -> 71
16~23 -> 72
24~31 -> 73
```

Offset：

```text
offset =
1 + ((index % 8) * 2)
```

Little Endian：

```c
packet[offset] = n_refs & 0xFF;
packet[offset + 1] = (n_refs >> 8) & 0xFF;
```

---

# 59. Curve Encoding Pseudocode

```c
for (uint8_t page = 0; page < 4; page++) {
    uint8_t packet[17] = {0};

    packet[0] = 0x70U + page;

    for (uint8_t i = 0; i < 8; i++) {
        uint8_t index = page * 8U + i;
        uint16_t value = profile[index];

        uint8_t offset = 1U + i * 2U;

        packet[offset] = value & 0xFFU;
        packet[offset + 1U] = value >> 8;
    }

    notify(packet, 17);
}
```

---

# 60. Profile Decode Pseudocode

```c
elapsed_ms = 0;

for each raw point {
    if (raw == 0) {
        continue;
    }

    dt_ms =
        raw / 125.0;

    sp =
        7500000 / raw;

    elapsed_ms += dt_ms;

    curve_point.time_ms = elapsed_ms;
    curve_point.sp = sp;
}
```

---

# 61. CMD 0x74 與曲線

收到：

```text
0x74
```

時，如果目前保存了一筆完整 Shot：

Peripheral 應可以重新輸出：

```text
B0~B7
70~73
```

也就是：

```text
History
+
Latest Profile
```

---

# 62. 重送曲線

BX-09 沒有觀察到：

```text
NACK 71
```

或：

```text
Retry only 72
```

因此如果 Central 發現：

```text
70
72
73
```

缺：

```text
71
```

應重新：

```text
0x74
```

取得整組資料。

---

# 63. CMD 0x75

Request：

```text
75
```

用途：

```text
Clear stored Shot History
```

屬於破壞性命令。

不得在：

```text
Connect
Init
Keep Alive
Reconnect
```

時自動送出。

---

# 64. CMD 0x61

目前：

```text
存在
```

但用途：

```text
Unknown
```

Peripheral 相容實作建議：

```text
安全忽略
```

不要：

```text
Clear
Reset
Disconnect
```

---

# 65. 掉包處理

GATT 使用：

```text
Notify
```

不是：

```text
Indication
```

所以 ATT Application Layer 沒有每包 ACK。

BLE Link Layer 本身仍具有：

```text
CRC
ACK
Automatic Retransmission
```

---

# 66. BX-09 Protocol 沒有觀察到

目前未發現：

```text
Sequence Number
Packet ACK
Packet NACK
Selective Retry
Session ID
```

---

# 67. History 掉包

例如：

```text
B0
B1
B2
B4
B5
B6
B7
```

缺：

```text
B3
```

則：

```text
Incomplete
```

不能解析成有效 Shot History。

---

# 68. Profile 掉包

例如：

```text
70
71
73
```

缺：

```text
72
```

則：

```text
Profile Incomplete
```

建議：

```text
Discard partial transfer
Send 0x74 again
```

---

# 69. Checksum Error

如果：

```text
B0~B6
```

全部存在，

但：

```text
Calculated Checksum
!=
B7[16]
```

則：

```text
History Invalid
```

清除該次接收 Cache，

再重新：

```text
0x74
```

---

# 70. BRD_BBP 建議 Peripheral CMD Dispatcher

```text
0x51
    -> Notify latest A0

0x74
    -> Notify B0~B7
    -> Notify 70~73

0x75
    -> Clear History

0x61
    -> Reserved / Ignore

Unknown
    -> Ignore safely
```

---

# 71. BRD_BBP Event Notify

除了 CMD Response，

Peripheral 也應支援事件 Notify。

例如：

```text
LOAD Change
    ↓
Update A0 state
    ↓
Notify A0
```

Shot 完成：

```text
Update history
Update profile
Update max SP
Update counter
    ↓
Notify data
```

---

# 72. 建議 Shot Complete 流程

```text
LOAD = HIGH
Bey Loaded
    ↓
開始等待 launch
    ↓
LOAD HIGH -> LOW
    ↓
Launch
    ↓
Profile Capture
    ↓
量測結束
    ↓
Calculate representative SP
    ↓
Push History
    ↓
Build B0~B7
    ↓
Build 70~73
    ↓
Notify
```

---

# 73. Central 建議解析流程

```text
Connect
   ↓
Subscribe Notify
   ↓
Receive A0
   ↓
Monitor Shot Counter / State
   ↓
Receive or request Shot Data
   ↓
Collect by Header
   ↓
B0~B7 complete?
   ↓
Checksum OK?
   ↓
70~73 complete?
   ↓
Decode nRefs
   ↓
Build time axis
   ↓
Build SP curve
   ↓
Shot Ready
```

---

# 74. BRD_BBP 曲線實作重點

BX-09 相容 Profile：

```text
不是：
每 50 ms 一點

而是：
每個旋轉週期一點
```

BRD 已知：

```text
period_us
```

可以直接轉：

```text
nRefs ≈ period_us / 8
```

接著填入：

```text
70~73
```

上位機再透過：

```text
SP = 7,500,000 / nRefs
```

與：

```text
dt_ms = nRefs / 125
```

完整還原曲線。

---

# 75. API 快速索引

BLE Name：

```text
BEYBLADE_TOOL01
```

Service：

```text
55C40000-F8EB-11EC-B939-0242AC120002
```

Notify：

```text
55C4F002-F8EB-11EC-B939-0242AC120002
```

Packet：

```text
17 bytes
Little Endian
```

CMD：

```text
51 = Get Status
61 = Unknown
74 = Get History/Profile
75 = Clear History
```

Response：

```text
A0 = Status

B0~B6 = Shot History
B7 = History Checksum

70~73 = Latest Shot Profile
```

Profile：

```text
32 x uint16 raw values
```

Decode：

```text
SP =
floor(7,500,000 / nRefs)
```

```text
dt_ms =
nRefs / 125
```

BRD Encode：

```text
nRefs ≈ period_us / 8
```

---

# 76. 信心等級

高信心：

```text
BEYBLADE_TOOL01

55C40000 Service

55C4F002 Notify

17-byte packets

0x51

0x74

0x75

A0

B0~B7

70~73

Little Endian

32-point profile structure

B7 checksum

nRefs based Profile decoding
```

公開逆向實作確認、但不等於原廠正式規格：

```text
SP = 7,500,000 / nRefs

dt_ms = nRefs / 125

Profile peak estimation

minimum 7 valid points

first-peak analysis
```

尚未完全定案：

```text
Write Characteristic UUID

A0[3] precise semantics

A0[2]

A0[5]

A0[6]

0x61 meaning

0x75 exact response sequence

原廠 App 最終 SP 評分演算法
```

---

# 77. 實作原則

如果 BRD_BBP 目標為 BX-09 Compatibility：

```text
1. CMD 格式保持 BX-09 形式

2. F002 使用 Notify

3. 0x51 回 A0

4. 0x74 可重新取得完整 Shot

5. 0x75 真正清除資料

6. Profile 使用 70~73

7. Profile 保存 per-revolution / per-period 資料

8. 不直接塞 BRD 50 ms RPM Sample

9. 使用 Little Endian

10. Receiver 依 Header 重組

11. 保留最後一次 Shot 供 0x74 重送

12. 不自行加入 ACK、SEQ、NACK 到 BX-09 Packet

13. BRD 自有可靠傳輸功能應保留在另一套 BRD Native Protocol
```

---

# 78. 主要公開逆向來源

主要交叉參考：

```text
shark-minister/atlas_bey
```

確認：

```text
Service UUID
F002 Notify
A0
B0~B7
70~73
Checksum
Profile structure
Event-driven Notify
```

```text
bahamutonX/BeyMeter
```

確認：

```text
17-byte parser
A0
B0~B7
70~73
nRefs
SP conversion
Profile time reconstruction
Profile analysis
```

```text
danielbyomujuni/BeyStats
```

確認：

```text
0x51
0x74
0x75
Write Without Response
CMD -> Notify response model
```

以及 BX-09 實機逆向紀錄：

```text
battlepass-emulation.md
```

用於比對：

```text
CMD
A0
History
Profile
Checksum
Polling
Duplicate packet
Timing behavior
```

---

# 79. BRD_BBP 最終曲線資料鏈

BRD 實際硬體：

```text
IR FALLING Edge
```

取得：

```text
period_us
```

轉換：

```text
nRefs =
period_us / 8
```

存入：

```text
Profile[0..31]
```

發射結束：

```text
Profile[0..7]   -> 0x70
Profile[8..15]  -> 0x71
Profile[16..23] -> 0x72
Profile[24..31] -> 0x73
```

上位機：

```text
nRefs
  ↓
SP = 7,500,000 / nRefs
  ↓
dt = nRefs / 125
  ↓
累計 dt
  ↓
SP vs Time Curve
```

這是目前公開逆向資料下，BRD_BBP 要模擬 BX-09 曲線格式最直接且一致的實作方式。