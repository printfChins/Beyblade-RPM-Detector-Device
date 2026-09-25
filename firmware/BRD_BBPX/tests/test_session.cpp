/* 檔案位置: BRD_BBP/tests/test_session.cpp
   [R2 修改] 裝載後曲線、整次代表 SP、發布延遲與命令回歸。 */
#include "../brd_bbp_session.h"
#include <cstdio>
#include <cstring>

static int failures;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static void shot(brd_bbp::Session &session, uint32_t first, uint32_t now) {
    session.period(6000, first);
    session.launched(first);
    session.period(8000, first + 8000U);
    session.finish();
    session.update(now, 600000);
}

int main() {
    brd_bbp::Session session;
    const uint8_t uid[6] = {1, 2, 3, 4, 5, 6};
    uint8_t pages[12][17] = {};
    session.period(6000, 10000);
    CHECK(session.flags(50, 20, 5) & 4);
    session.launched(10000);
    session.period(8000, 18000);
    session.finish();
    session.period(4000, 22000); // 完成後不再變更。
    session.update(609999, 600000);
    CHECK(session.state().total == 0);
    session.update(610000, 600000);
    CHECK(session.state().total == 1 && session.state().lifetime_max == 10000);
    CHECK(session.state().curve[0] == 750 && session.state().curve[1] == 1000 && session.state().curve[2] == 0);

    const auto before = session.state();
    CHECK(session.command(0x61, 1, uid, 0, pages) == 0);
    CHECK(std::memcmp(&before, &session.state(), sizeof before) == 0);
    CHECK(session.command(0x51, 7, uid, 0, pages) == 1 && pages[0][0] == 0xa0);
    CHECK(session.command(0x74, 7, uid, 0, pages) == 12);
    CHECK(pages[0][0] == 0xb0 && pages[11][0] == 0x73);
    shot(session, 700000, 1300000);
    CHECK(session.state().total == 1); // reset 前不重複加入紀錄。

    session.command(0x75, 0, uid, 0, pages);
    CHECK(session.state().total == 0);
    shot(session, 1400000, 2100000);
    CHECK(session.state().total == 0);
    session.reset_capture();
    shot(session, 2200000, 2800000);
    CHECK(session.state().total == 1);
    brd_bbp::Session wrapped;
    shot(wrapped, 0xffff0000U, 0x000827c0U);
    CHECK(wrapped.state().total == 1);
    return failures == 0 ? 0 : 1;
}
