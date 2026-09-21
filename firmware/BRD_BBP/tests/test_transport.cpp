/* 檔案位置: BRD_BBP/tests/test_transport.cpp
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
#include <cstdint>
#include <cstdio>
#include <cstring>
/* [修正 新增] 此組維持純輪詢回歸；主動模式另由 test_subscriber.cpp 驗證。 */
#define BBP_COMPAT_AUTONOTIFY 0
#include "host_stubs/Arduino.h"
#include "host_stubs/Preferences.h"
#include "host_stubs/NimBLEDevice.h"
uint32_t host_micros=0,host_millis=0;void(*host_interrupts[32])()={};
static bool battery_allowed=true;static uint8_t battery_percent=50;
#include "../brd_bbp_protocol.cpp"
#include "../brd_bbp_session.cpp"
bool brd_battery_measurement_allowed(){return battery_allowed;}uint8_t brd_battery_get_percent(){return battery_percent;}
#include "../brd_bbp.cpp"
static int failures;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);++failures;}}while(0)
static void reset_all(){brd_bbp_stop();g_storage_loaded=false;g_start_attempted=false;g_last_storage_attempt_ms=0;g_session=brd_bbp::Session();g_diagnostics={};host_nvs::begin_failures=0;host_nvs::short_read=false;host_nvs::put_fail=false;host_nvs::puts=0;host_ble::notifications.clear();host_ble::notify_results.clear();host_ble::notify_index=0;host_ble::init_calls=0;host_millis=host_micros=0;}
static void make_history(){brd_bbp::State s;brd_bbp::clear(s);brd_bbp::Profile p;p.add_period_us(6000);brd_bbp::append(s,p,10000);uint8_t b[brd_bbp::STORAGE_SIZE];brd_bbp::encode_storage(s,b);host_nvs::blob.assign(b,b+sizeof b);}
static void connect(){NimBLEDevice::server.hostConnect(7);NimBLEDevice::server.s.c.hostSubscribe(7,1);}
static void cmd(uint8_t c){NimBLEDevice::server.s.c.hostWrite(std::string(1,char(c)),7);}
static void tick(uint32_t ms){host_millis=ms;host_micros=ms*1000;brd_bbp_update(true);}
int main(){
 reset_all();make_history();auto original=host_nvs::blob;host_nvs::begin_failures=1;CHECK(!brd_bbp_begin());brd_bbp_capture_reset();brd_bbp_capture_period(6000,0);brd_bbp_capture_launch(host_micros);brd_bbp_capture_finish();tick(700);CHECK(host_nvs::blob==original&&host_nvs::puts==0);host_millis=1000;CHECK(brd_bbp_begin());CHECK(g_session.state().total==1);
 brd_bbp_capture_reset();brd_bbp_capture_period(6000,1000000);brd_bbp_capture_launch(host_micros);brd_bbp_capture_finish();tick(1600);CHECK(g_session.state().total==2);
 reset_all();make_history();host_nvs::short_read=true;CHECK(!brd_bbp_begin());CHECK(!g_storage_loaded&&host_nvs::puts==0);host_nvs::short_read=false;host_nvs::blob[20]^=1;host_millis=1000;CHECK(!brd_bbp_begin());CHECK(!g_storage_loaded&&host_nvs::puts==0);
 reset_all();make_history();CHECK(brd_bbp_begin());connect();host_nvs::put_fail=true;cmd(0x75);brd_bbp_update(false);CHECK(g_session.state().total==0&&g_session.dirty());CHECK(!brd_bbp_prepare_restart());host_nvs::put_fail=false;host_millis+=1000;CHECK(brd_bbp_prepare_restart());brd_bbp::State empty;CHECK(brd_bbp::decode_storage(host_nvs::blob.data(),host_nvs::blob.size(),empty)&&empty.total==0);
 cmd(0x51);tick(host_millis+10);CHECK(host_ble::notifications.size()==1);if(!host_ble::notifications.empty())CHECK(host_ble::notifications.back()[0]==0xa0);cmd(0x74);for(int i=0;i<12;i++)tick(host_millis+10);CHECK(host_ble::notifications.size()==13);if(host_ble::notifications.size()>=13)for(int i=0;i<12;i++)CHECK(host_ble::notifications[1+i][0]==uint8_t(i<8?0xb0+i:0x70+i-8));
 host_ble::notify_results={false,true};host_ble::notify_index=0;cmd(0x51);tick(host_millis+10);tick(host_millis+10);CHECK(host_ble::notifications.size()==15);if(host_ble::notifications.size()>=15)CHECK(host_ble::notifications[13]==host_ble::notifications[14]);
 cmd(0x74);tick(host_millis+10);size_t sent=host_ble::notifications.size();NimBLEDevice::server.s.c.hostSubscribe(7,0);for(int i=0;i<20;i++)tick(host_millis+10);CHECK(host_ble::notifications.size()==sent);NimBLEDevice::server.s.c.hostSubscribe(7,1);
 cmd(0x74);tick(host_millis+10);sent=host_ble::notifications.size();NimBLEDevice::server.disconnect(7);for(int i=0;i<20;i++)tick(host_millis+10);CHECK(host_ble::notifications.size()==sent);
 auto invalid=g_diagnostics.invalid_commands;NimBLEDevice::server.s.c.hostWrite("xx",7);CHECK(g_diagnostics.invalid_commands==invalid+1);
 reset_all();host_nvs::blob.clear();battery_allowed=false;CHECK(!brd_bbp_begin()&&host_ble::init_calls==0);
 return failures?1:0;
}
