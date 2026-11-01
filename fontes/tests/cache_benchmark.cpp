#include "entities.hpp"
#include "input.hpp"
#include <chrono>
#include <algorithm>
#include <cstdio>
int main() {
    Memory memory;
    if (!memory.attach("cs2.exe")) { std::puts("FAIL: no game"); return 1; }
    EntityCache cache; cache.init(&memory);
    if (!cache.resolve()) { std::puts(cache.status().c_str()); memory.detach(); return 2; }
    PollTimer timer;
    for (int i=0;i<20;++i) { cache.update(); timer.wait(); }
    if (!cache.ready()) { std::puts(cache.status().c_str()); memory.detach(); return 3; }
    memory.profile_reads(true); memory.reset_read_stats();
    std::vector<double> times; times.reserve(200);
    for (int i=0;i<200;++i) {
        const auto start=std::chrono::steady_clock::now(); cache.update();
        times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()); timer.wait();
    }
    const auto stats=memory.read_stats();
    std::sort(times.begin(),times.end()); double sum=0; for (const auto time:times) sum+=time;
    std::printf("samples=200 players=%zu mean_ms=%.3f median_ms=%.3f p95_ms=%.3f max_ms=%.3f read_calls_per_update=%.1f bytes_per_update=%.1f read_failures=%llu\n",
        cache.players().size(),sum/times.size(),times[100],times[190],times.back(),stats.calls/200.,stats.bytes/200.,static_cast<unsigned long long>(stats.failures));
    memory.detach();
}
