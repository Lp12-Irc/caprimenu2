#include "entities.hpp"
#include "input.hpp"
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
int main(int argc,char** argv) {
    const DWORD interval=argc>1?DWORD(std::clamp(std::atoi(argv[1]),1,80)):2;
    CacheOptions options;options.items=false;options.inventory=false;options.throttle_details=true;options.recoil=false;options.bomb=false;options.observers=false;
    Memory memory;
    if (!memory.attach("cs2.exe")) { std::puts("FAIL: no game"); return 1; }
    EntityCache cache; cache.init(&memory);
    if (!cache.resolve()) { std::puts(cache.status().c_str()); memory.detach(); return 2; }
    PollTimer timer;
    for (int i=0;i<20;++i) { cache.update(options); timer.wait(interval); }
    if (!cache.ready()) { std::puts(cache.status().c_str()); memory.detach(); return 3; }
    memory.profile_reads(true); memory.reset_read_stats();
    std::vector<double> times; times.reserve(200);
    FILETIME created{},exited{},kernel_before{},user_before{},kernel_after{},user_after{};
    GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel_before,&user_before);
    const auto wall_start=std::chrono::steady_clock::now();
    for (int i=0;i<200;++i) {
        const auto start=std::chrono::steady_clock::now(); cache.update(options);
        const auto snapshot=std::make_shared<const EntityCache>(cache.snapshot());
        times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()); timer.wait(interval);
    }
    const double wall_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-wall_start).count();
    GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel_after,&user_after);
    const auto ticks=[](FILETIME f){return (ULONGLONG(f.dwHighDateTime)<<32)|f.dwLowDateTime;};
    const double cpu_ms=double(ticks(kernel_after)+ticks(user_after)-ticks(kernel_before)-ticks(user_before))/10000.;
    std::printf("interval_ms=%lu duration_ms=%.1f cpu_one_core_pct=%.2f updates_per_second=%.1f\n",interval,wall_ms,cpu_ms*100/wall_ms,200000/wall_ms);
    const auto stats=memory.read_stats();
    std::sort(times.begin(),times.end()); double sum=0; for (const auto time:times) sum+=time;
    std::printf("samples=200 players=%zu mean_ms=%.3f median_ms=%.3f p95_ms=%.3f max_ms=%.3f read_calls_per_update=%.1f bytes_per_update=%.1f read_failures=%llu\n",
        cache.players().size(),sum/times.size(),times[100],times[190],times.back(),stats.calls/200.,stats.bytes/200.,static_cast<unsigned long long>(stats.failures));
    memory.detach();
}
