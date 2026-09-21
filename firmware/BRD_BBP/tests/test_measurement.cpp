/* 檔案位置: BRD_BBP/tests/test_measurement.cpp
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
#include <cstdint>
#include <cstdio>
#include "host_stubs/soc/gpio_struct.h"
#include "../brd_bbp.h"
uint32_t host_micros=0,host_millis=0; void (*host_interrupts[32])()={}; host_gpio_t GPIO{};
#include "../brd_bbp_protocol.cpp"
#include "../brd_bbp_session.cpp"
static brd_bbp::Session host_session;
bool brd_bbp_begin(){return true;} void brd_bbp_stop(){} bool brd_bbp_prepare_restart(){return true;} void brd_bbp_update(bool){} bool brd_bbp_is_connected(){return false;} brd_bbp_diagnostics_t brd_bbp_get_diagnostics(){return {};}
void brd_bbp_capture_reset(){host_session.reset_capture();} void brd_bbp_capture_period(uint32_t p,uint32_t e,bool record){host_session.period(p,e,record);} void brd_bbp_capture_launch(uint32_t confirmed_us){host_session.launched(confirmed_us);} void brd_bbp_capture_finish(){host_session.finish();} void brd_bbp_capture_abort(){host_session.abort();}
#include "../brd_measurement.cpp"
static int failures;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);++failures;}}while(0)
static void at(uint32_t us){host_micros=us;host_millis=us/1000; brd_measurement_update();}
static void load(int level,uint32_t us){GPIO.in.val=level?2u:0u;host_micros=us;host_interrupts[1]();at(us+1000);}
static void rpm(uint32_t us){host_micros=us;host_interrupts[3]();}
int main(){
 brd_measurement_begin(); GPIO.in.val=2;host_micros=100;host_interrupts[1]();GPIO.in.val=0;host_micros=500;host_interrupts[1]();at(1499);CHECK(!brd_measurement_get_display().loaded); // bounce never survives debounce
 rpm(1000);rpm(7000);at(7000);CHECK(!(host_session.flags(50,20,5)&4));CHECK(host_session.state().total==0);
 load(1,10000);rpm(12000);rpm(18000);rpm(24000);at(24000);CHECK(host_session.flags(50,20,5)&4);
 load(0,25000);rpm(30000);rpm(60000);at(60000); // 2000rpm <=20% of 10000
 host_session.update(618000,600000);CHECK(host_session.state().total==1);
 // [R2 修改] 裝載後所有有效週期都保存；發射前及跨界週期不丟棄。
 CHECK(host_session.state().records[0]==10000);
 CHECK(host_session.state().curve[0]==750 && host_session.state().curve[1]==750);
 CHECK(host_session.state().curve[2]==750 && host_session.state().curve[3]==3750 && host_session.state().curve[4]==0);
 brd_measurement_max_frame_presented(brd_measurement_get_display().generation);at(3200000);
 load(1,3210000);rpm(3220000);rpm(3226000);at(3226000);load(0,3230000);at(3536001);host_session.update(3826000,600000);CHECK(host_session.state().total==2);
 // [R2 修改] 發射後沒有新週期，仍保留裝載拉轉時已取得的資料。
 CHECK(host_session.state().curve[0]==750);
 for(unsigned i=1;i<32;i++) CHECK(host_session.state().curve[i]==0);
 brd_measurement_stop();CHECK(brd_measurement_storage_allowed());
 brd_measurement_begin();load(1,4000000);for(unsigned i=0;i<RPM_ISR_QUEUE_SIZE+2;i++)rpm(4010000+i*6000);at(4800000);CHECK(brd_measurement_get_diagnostics().rpm_queue_overflows==1);host_session.update(5000000,600000);CHECK(host_session.state().total==2);
 brd_measurement_stop();host_session.update(6000000,600000);CHECK(host_session.state().total==2);
 return failures?1:0;
}
