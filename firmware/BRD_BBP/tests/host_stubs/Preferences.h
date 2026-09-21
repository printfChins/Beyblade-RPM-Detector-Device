/* 檔案位置: BRD_BBP/tests/host_stubs/Preferences.h
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
namespace host_nvs { inline std::vector<uint8_t> blob; inline int begin_failures=0; inline bool short_read=false; inline bool put_fail=false; inline unsigned puts=0; }
class Preferences { public:
 bool begin(const char*,bool){if(host_nvs::begin_failures){--host_nvs::begin_failures;return false;}return true;}
 bool isKey(const char*){return !host_nvs::blob.empty();}
 size_t getBytesLength(const char*){return host_nvs::blob.size();}
 size_t getBytes(const char*,void* p,size_t n){size_t z=host_nvs::short_read&&n? n-1:n;if(z>host_nvs::blob.size())z=host_nvs::blob.size();std::memcpy(p,host_nvs::blob.data(),z);return z;}
 size_t putBytes(const char*,const void*p,size_t n){++host_nvs::puts;if(host_nvs::put_fail)return 0;host_nvs::blob.assign((const uint8_t*)p,(const uint8_t*)p+n);return n;}
 void end(){}
};
