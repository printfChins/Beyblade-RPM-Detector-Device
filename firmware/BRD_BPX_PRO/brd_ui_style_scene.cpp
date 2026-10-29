/*
 * File: BRD_BPX_PRO/brd_ui_style_scene.cpp
 * [修改] 2026-10-29：刪除 NEBULA／DIAL，只保留風格三 TORQUE。
 * [保留] TORQUE 每個頁面原有座標、配色、動畫及資料來源。
 * [修改] 改接 Model/Curve/History 真實資料；無硬編碼示範轉速。
 * [保留] 僅呼叫 Adafruit GFX 繪圖 API，不含 LCD 驅動命令。
 * [修改] 充電 UI：僅保留電池內亮帶，不顯示閃電、三點或 CHG 文字。
 */
#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "brd_ui_style_scene.h"
#include "brd_ui_warning_pages.h"
#include "brd_ui_types.h"
#include "brd_ui_curve.h"
#include "brd_ui_legible_text.h" // [新增] 次要文字改用現成 U8g2 字型庫。
#include "brd_config.h" // [保留] TORQUE 指示條滿格轉速由 CFG 設定。
namespace brd_ui { namespace style_scene {
namespace {
static Adafruit_GFX* gfx = nullptr;
#define lcd (*gfx)
static int bound_int(int v,int lower,int upper) {return v<lower?lower:(v>upper?upper:v);}
static float bound_float(float v,float lower,float upper) {return v<lower?lower:(v>upper?upper:v);}
static uint16_t rgb(int r,int g,int b){ return ((r>>3)<<11)|((g>>2)<<5)|(b>>3); }
static uint16_t mix(uint16_t a,uint16_t b,float t){
    auto r=[](uint16_t c){return int(((c>>11)&31)*255/31);};
    auto g=[](uint16_t c){return int(((c>>5)&63)*255/63);};
    auto bl=[](uint16_t c){return int((c&31)*255/31);};
    t=bound_float(t,0.0f,1.0f);
    return rgb(int(r(a)*(1-t)+r(b)*t),int(g(a)*(1-t)+g(b)*t),int(bl(a)*(1-t)+bl(b)*t));
}
struct Theme {const char* key;const char* name;uint16_t bg,fg,hi,lo,warn,red;};
// [修改] 唯一風格：原選單索引 2，原始設計代碼 06_TORQUE。
static const Theme kTorque = {
    "06_TORQUE", "TORQUE", rgb(20,9,5), rgb(255,240,229),
    rgb(250,124,77), rgb(100,48,27), rgb(255,211,88), rgb(255,83,95)
};
// [修改] 原 size=1 的 6x8 小字改為 U8g2 7x13 可讀字型，保留 size=2/3 原尺寸。
static void txt(int x,int y,const char* s,uint16_t c,int size=1){
    if(size==1){legible_text::draw(lcd,x,y,s,c);return;}
    lcd.setCursor(x,y);lcd.setTextSize(size);lcd.setTextColor(c);lcd.print(s);
}
// [新增] 狹窄欄位用 U8g2 6x10 字型，保留 5 位歷史 RPM、OTA 長字串。
static void txt_compact(int x,int y,const char* s,uint16_t c){
    legible_text::draw(lcd,x,y,s,c,legible_text::Density::Compact);
}
static void centered(int y,const char* s,uint16_t c,int size=1){
    int w=size==1?legible_text::width(lcd,s):(int)strlen(s)*6*size;
    txt(std::max(0,(160-w)/2),y,s,c,size);
}
static void line(int x,int y,int xx,int yy,uint16_t c){lcd.drawLine(x,y,xx,yy,c);}
static void box(int x,int y,int w,int h,uint16_t c){lcd.drawRect(x,y,w,h,c);}
static void fill(int x,int y,int w,int h,uint16_t c){lcd.fillRect(x,y,w,h,c);}
static void disk(int x,int y,int r,uint16_t c){lcd.fillCircle(x,y,r,c);}
static void fmt(char*b,size_t n,uint32_t v){std::snprintf(b,n,"%lu",(unsigned long)v);}
// [修改] 充電時只保留電池內流動亮帶，移除外部閃電。
// [保留] 電量格數依實際 ADC 百分比，不把動畫當成真實電量。
// [範圍] x..x+18, y..y+7；支援 MAIN 與 IR TEST，避免覆蓋 BLE/電壓。
static void battery(const Theme &t,int x,int y,int pct,bool charge,uint8_t phase){
    const int p=bound_int(pct,0,100);
    const uint16_t body=charge?t.hi:t.fg;
    box(x,y,17,8,body);
    fill(x+17,y+2,2,4,body);
    const int bar=12*p/100;
    if(bar>0) {
        fill(x+2,y+2,bar,4,charge?mix(t.bg,t.hi,0.60f):(p<20?t.red:t.hi));
    }
    if(!charge) return;

    // [新增] 輕量動畫：2px 亮帶只掃過已充入的部分，不偽造電池容量。
    if(bar>0) {
        const int scan=(bar<=2)?0:((int)(phase%4U)*(bar-2)/3);
        fill(x+2+scan,y+2,std::min(2,bar-scan),4,t.fg);
    }
}
/* [新增] Standard Bluetooth glyph drawn only inside the top-right BLE slot.
 * [修改] Replace the ambiguous connection dot with an identifiable BT rune.
 * Connected: theme accent; advertising/waiting: subdued gray;
 * BLE disabled: red with a diagonal slash.
 * Bounds: x-4..x+5, y..y+12 (does not touch the battery icon).
 */
static void ble_icon(const Theme &t,int x,int y,bool connected,bool disabled) {
    const uint16_t muted=mix(t.bg,t.fg,0.43f);
    const uint16_t color=disabled?t.red:(connected?t.hi:muted);
    line(x,y,x,y+12,color);                    // Main vertical stroke
    line(x,y,x+4,y+4,color);                   // Upper triangle
    line(x+4,y+4,x-3,y+9,color);               // Upper diagonal
    line(x-3,y+3,x+4,y+9,color);               // Lower diagonal
    line(x+4,y+9,x,y+12,color);                // Lower triangle
    if(disabled) {
        line(x-4,y+12,x+5,y,color);           // BLE intentionally disabled
    }
}

static void base(const Theme&t){lcd.fillScreen(t.bg);}

// [變更] 曲線資料來自量測快照，不再使用 DEMO 陣列。
static const brd_ui::Curve* active_curve = nullptr;
static void graph(const Theme&t,int x,int y,int w,int h,int type,int frame){
  uint16_t grid=mix(t.bg,t.lo,.7f);
  for(int i=0;i<=3;i++)line(x,y+i*h/3,x+w-1,y+i*h/3,grid);
  if(type%3==0)for(int i=1;i<5;i++)line(x+i*w/5,y,x+i*w/5,y+h,grid);
  int actual = active_curve ? active_curve->count : 0;
  int n=std::min(actual,std::max(0,4+frame*3));
  int divisions = actual > 1 ? actual-1 : 1;
  uint32_t peak = active_curve && active_curve->peak ? active_curve->peak : 1U;
  int xx0=0,yy0=0;
  for(int i=0;i<n;i++){
    int px=x+i*(w-1)/divisions;
    int py=y+h-1-int((h-4)*(uint32_t)active_curve->rpm[i]/peak);
    if(i){
      // [修改] TORQUE 保留原本的節點連線與淡化垂線。每個取樣點額外畫出圓點。
      line(xx0,yy0,px,py,t.hi);
      if(type%4==2)line(px,py,px,y+h-1,mix(t.hi,t.bg,.78f));
    }
    disk(px,py,1,t.fg); /* [新增] 第一點及每個後續取樣點清楚標示。 */
    xx0=px;yy0=py;
  }
}



// [保留] TORQUE W1 呼吸、L2 聚焦鎖定，位置及 25 FPS 排程不變。
// [保留] LOAD/RPM 量測邏輯完全不變；畫面仍使用真實量測快照。
// [防重疊] WAIT/LOADED 使用相同中心；L2 括號始終與文字保留至少 5px 間距。
// [防重疊] 狀態區都在 y<63 且與右上角 BLE/電池、轉速數字分開。
static void load_state_effect(const Theme &t,int tick,bool loaded,int xmin,int xmax,int y){
    const char *label = loaded ? "LOADED" : "WAIT LOAD";
    const int width = legible_text::width(lcd,label,legible_text::Density::Compact);
    if(!loaded){
        // [W1 新增] 約 1.6 秒一次的三角波呼吸，零 delay、零浮點三角函數。
        const int phase=tick%48;
        const int triangle=(phase<24)?phase:(48-phase);
        const float level=0.38f+(float)triangle*(0.57f/24.0f);
        const uint16_t color=mix(t.bg,t.hi,level);
        // [修改] WAIT LOAD 和 LOADED 使用相同的狀態區水平中心。
        // [刪除] WAIT LOAD 末端呼吸點；只保留文字本身的 W1 呼吸效果。
        const int text_x=(xmin+xmax-width)/2;
        txt_compact(text_x,y,label,color);
        return;
    }
    // [L2 新增] 約 8 幀向內收束，完成後保持；不進入文字包圍框內側。
    const int text_x=(xmin+xmax-width)/2;
    txt_compact(text_x,y,label,mix(t.hi,t.fg,0.30f));
    const int shift=std::max(0,7-std::min(7,tick));
    const int left=text_x-8-shift;
    const int right=text_x+width+7+shift;
    const uint16_t color=mix(t.hi,t.fg,0.37f);
    const int top=y-1;
    const int bottom=y+10;
    // [L2 防重疊] 左右括號水平短邊只向外畫，與文字最近仍相隔至少 5px。
    line(left,top,left,bottom,color);
    line(left,top,left+2,top,color);
    line(left,bottom,left+2,bottom,color);
    line(right,top,right,bottom,color);
    line(right-2,top,right,top,color);
    line(right-2,bottom,right,bottom,color);
}

static void main_scene(int tick,const brd_ui::Model& m) {
    const Theme &t=kTorque;
    base(t);
    // [保留] SP/MAX 取峰值供畫面顯示；量測與 BLE 數據不變。
    const bool show_peak=m.measurement.show_max;
    const uint16_t peak_color=show_peak?t.warn:t.fg;
    const uint32_t rpm=std::max(uint32_t(m.measurement.value),
                                uint32_t(m.measurement.peak_value));
    char val[24];fmt(val,sizeof val,rpm);
    // [保留] TORQUE 底部 16 段轉速比例，滿格以上只限制繪圖，不截斷 RPM。
    const uint32_t full_scale=BRD_UI_INDICATOR_FULL_SCALE_RPM;
    const int filled=(rpm>=full_scale) ? 100 :
        static_cast<int>((static_cast<uint64_t>(rpm)*100ULL)/full_scale);

    // [保留] 原 TORQUE 工業風左側面板與右側 SP／MAX 數值。
    fill(0,0,54,80,t.lo);
    txt(3,8,show_peak?"MAX":"SP",t.warn,3);
    // [刪除] MAX 左下角 PEAK 字樣。
    // [修改] 轉速 GFX 3 倍字，左界固定 X=60，位數變化不跳動。
    txt(60,25,val,peak_color,3);
    // [保留] WAIT LOAD／LOADED 獨立區域，排除 SP 數字重疊。
    if(!show_peak && (m.measurement.loaded || rpm==0U)) {
        load_state_effect(t,tick,m.measurement.loaded,91,159,48);
    }
    for(int i=0;i<16;i++)fill(61+i*6,65,4,7,i<filled*16/100?t.hi:t.lo);

    // [保留] 充電亮帶、BLE 狀態與電量百分比原座標。
    fill(90,0,70,17,t.bg);
    battery(t,105,4,m.battery,m.charging,m.charge_phase);
    char bp[8];std::snprintf(bp,sizeof bp,"%u%%",(unsigned)m.battery);
    txt_compact(133,3,bp,t.fg);
    ble_icon(t,96,2,m.connected,m.ble_disabled);
}
static void result_scene(int tick,const brd_ui::Model& m) {
    const Theme &t=kTorque;
    base(t);
    char v[20];fmt(v,sizeof v,m.measurement.value);
    // [保留] TORQUE 原風格 5 % 3 == 2 的 MAX 動畫。
    for(int j=0;j<5;j++)line(10,14+j*13,150,14+j*13,t.lo);
    for(int j=0;j<=tick;j++)fill(4+j*9,22,5,3,t.hi);
    txt(5,3,"NEW PEAK",t.hi,2);
    centered(31,v,t.warn,3);
    txt(6,66,"SAVED",t.hi);
    txt(111,66,"SP",t.fg);
}
static void graph_scene(int tick,const brd_ui::Model& m,const brd_ui::Curve &c) {
    const Theme &t=kTorque;
    base(t);
    (void)m;
    // [保留] 原 TORQUE 曲線區域 (3,22,154,44)，圖形演算法不變。
    txt(6,2,"CURVE",t.hi,2);
    graph(t,3,22,154,44,5,tick);
    char v[20];fmt(v,sizeof v,c.peak);
    txt(6,66,"MAX",t.warn);
    txt(34,66,v,t.fg);
}
// Original IR sensor geometry is copied verbatim from production brd_ui_page_ir.cpp.
// [保留] LOAD uses k_hexagon with x=44,y=36, zoom=2; RPM uses
// k_trapezoid with x=93,y=28, zoom=2. No redesigned symbol shapes.
static constexpr int8_t k_hexagon[6][2]={{16,7},{12,14},{4,14},{0,7},{4,0},{12,0}};
static constexpr int8_t k_trapezoid[6][2]={{0,4},{0,18},{10,22},{13,20},{13,2},{10,0}};
static void original_sensor(int x,int y,const int8_t p[][2],bool active,const Theme &t,uint16_t active_color){
    uint16_t color=active?active_color:mix(t.bg,t.lo,.85f);
    for(int i=0;i<6;i++){int j=(i+1)%6;
      lcd.drawLine(x+p[i][0]*2,y+p[i][1]*2,x+p[j][0]*2,y+p[j][1]*2,color);
    }
    if(active){
      for(int i=1;i<5;i++){
        lcd.fillTriangle(x+p[0][0]*2,y+p[0][1]*2,
          x+p[i][0]*2,y+p[i][1]*2,x+p[i+1][0]*2,y+p[i+1][1]*2,color);
      }
    }
}
static void sensor_scene(int tick,const brd_ui::Model&m){
 const Theme&t=kTorque;base(t);
 const int lp=bound_int(int(m.load_adc_raw)*100/4095,0,100);
 const int rp=bound_int(int(m.rpm_adc_raw)*100/4095,0,100);
 char left[12],right[12];fmt(left,sizeof(left),m.load_adc_raw);fmt(right,sizeof(right),m.rpm_adc_raw);
 const uint16_t adc_orange=t.warn;
 const uint16_t adc_yellow=mix(t.warn,t.fg,0.42f);
 // [保留] TORQUE 原 style=5 的 IR TEST 左右隔線與位置。
 line(0,19,159,19,t.lo);
 line(36,22,36,78,t.lo);
 line(125,22,125,78,t.lo);
 txt(3,3,"IR TEST",t.hi,2);
 // [修改] IR TEST 電池電壓使用真實 mV，且保留電池內部充電流光。
 // [修改] IR TEST 移除充電閃電及三點，保留電池亮帶與電壓。
 battery(t,93,4,m.battery,m.charging,m.charge_phase);
 char voltage[10];
 std::snprintf(voltage,sizeof(voltage),"%u.%02uV",
   (unsigned)(m.voltage_mv/1000U),(unsigned)((m.voltage_mv%1000U)/10U));
 txt(121,3,voltage,t.fg);
 txt(4,24,"LOAD",t.hi);
 txt(140,24,"SP",adc_orange);
 // [保留] TORQUE 原 style=5 % 4 == 1 的雙邊 ADC 直條。
 box(9,36,12,28,t.lo);box(139,36,12,28,t.lo);
 fill(11,62-lp*24/100,8,lp*24/100,adc_orange);
 fill(141,62-rp*24/100,8,rp*24/100,adc_yellow);
 txt(4,66,left,t.fg);txt(127,66,right,t.fg);
 // [保留] EXACT shape geometry from production firmware.
 const bool load_trigger=m.ir2, rpm_trigger=m.ir1;
 original_sensor(44,36,k_hexagon,load_trigger,t,t.hi);
 original_sensor(93,28,k_trapezoid,rpm_trigger,t,t.hi);
 // [新增] Independent LED-like indicator dot: triggered -> bright, not triggered -> dim.
 disk(78,70,2,load_trigger?adc_orange:t.lo);
 disk(122,70,2,rpm_trigger?adc_yellow:t.lo);
 (void)tick;
}
static void history_scene(int tick,const brd_ui::Model& m) {
    const Theme &t=kTorque;
    base(t);
    // [修改] 160x80：標題 TOP 與 8 筆資料，左側 #1~#4，右側 #5~#8。
    // [刪除] 每筆歷史的進度條／計量條。無紀錄欄位不印假數據。
    txt(4,2,"HISTORY",t.hi,2);
    char top[16];fmt(top,sizeof top,m.history_peak);
    txt_compact(99,4,"TOP",t.warn);
    txt_compact(120,4,top,t.warn);
    for (uint8_t i=0U; i<8U && i<m.history_count; ++i) {
        if (i >= static_cast<uint8_t>(std::min(8,2+tick*2))) { continue; }
        const int col=i/4U, row=i%4U;
        const int x=col==0?3:83;
        const int y=22+row*14;
        char index[5];std::snprintf(index,sizeof(index),"%u.",(unsigned)(i+1U));
        char value[12];fmt(value,sizeof value,m.history[i]);
        txt_compact(x,y,index,t.fg);
        // [修改] 歷史最高數值橘黃色高亮，其餘數值為白色。
        txt_compact(x+17,y,value,(m.history[i]==m.history_peak)?t.warn:t.fg);
    }
}
// [修改] LOW BATTERY 與 ADC FAULT 改由獨立模組繪製。
// [移除] 先前共用的警告畫面與大電池 / 格線共通樣式。
// [保留] 每個主題沿用相同 Theme 配色，並由 Dirty Tile 分段傳輸。
// [保留] TORQUE 低電量與 ADC 警示原始配色、500 ms 充電格閃爍。
static void warning_scene(int fault,int tick) {
    const Theme &t=kTorque;
    const warning_pages::Palette p{t.bg,t.fg,t.hi,t.lo,t.warn,t.red};
    warning_pages::draw(lcd,5U,
        fault ? warning_pages::Kind::AdcFault : warning_pages::Kind::LowBattery,
        static_cast<uint8_t>(tick),p);
}
static void boot_scene(int tick) {
    const Theme &t=kTorque;
    base(t);
    const uint16_t soft=mix(t.bg,t.lo,0.78f);
    // [保留] TORQUE 工業風開機畫面（原 style 5）。
    fill(0,0,54,80,t.lo);
    txt(6,10,"BRD",t.warn,2);
    txt(7,31,"BPX",t.fg,2);
    txt(7,52,"PRO",t.fg,2);
    txt(66,18,"BOOT",t.hi,2);
    txt(66,40,"SYSTEM",t.fg,2);
    for(int i=0;i<8;i++) fill(68+i*10,66,6,5,i<tick/2?t.hi:soft);
}
static void ota_scene(int tick,const brd_ui::Model& m) {
    (void)tick;
    const Theme &t=kTorque;
    base(t);
    const uint8_t percent=m.ota.percent>100U?100U:m.ota.percent;
    const int filled=int(percent);
    const uint16_t soft=mix(t.bg,t.lo,0.82f);
    char pct[8];std::snprintf(pct,sizeof pct,"%u%%",(unsigned)percent);
    const char* sub=(m.ota.phase==brd_ota::COMPLETE)?"DONE":
        (m.ota.verifying?"CHECK FLASH":"UPDATING");
    // [保留] TORQUE 左列 OTA/BLE、右側百分比和 16 段進度條。
    fill(0,0,54,80,t.lo);
    txt(1,8,"OTA",t.warn,3); /* [修改] OTA 文字左移，避免溢出左側橘色區塊。 */
    txt(8,54,"BLE",t.fg,2);
    txt(62,19,pct,t.fg,2);
    if(strlen(sub)>8U)txt_compact(63,45,sub,t.hi);
    else txt(63,42,sub,t.hi,2);
    for(int i=0;i<16;i++)fill(61+i*6,65,4,7,i<filled*16/100?t.hi:soft);
}
} // namespace
void draw(Adafruit_GFX& target,uint8_t selected,Page page,uint8_t frame,
          const Model& model,const Curve& curve) {
    gfx=&target;
    // [修改] 主題參數保留作舊 API 相容，所有索引只繪製 TORQUE。
    (void)selected;
    active_curve=&curve;
    const int tick=(int)frame;
    switch(page) {
        case Page::Main: main_scene(tick,model);break;
        case Page::Max: result_scene(tick,model);break;
        case Page::Graph: graph_scene(tick,model,curve);break;
        case Page::History: history_scene(tick,model);break;
        case Page::IrTest: sensor_scene(tick,model);break;
        case Page::Ota: ota_scene(tick,model);break;
        case Page::LowBattery: warning_scene(0,tick);break;
        case Page::AdcFault: warning_scene(1,tick);break;
        case Page::Boot: boot_scene(tick);break;
        default:break;
    }
    gfx=nullptr;
}
} } // namespace brd_ui::style_scene
