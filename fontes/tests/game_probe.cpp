// Read-only live integration check. No authentication, input injection or game writes.
#include "entities.hpp"
#include "esp.hpp"
#include "imgui.h"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <cstring>
#include "render_capture.hpp"
int main(int argc,char** argv) {
    const bool capture_defuse=argc==3 && std::strcmp(argv[1],"--capture-defuse")==0;
    const bool capture=argc==3 && (std::strcmp(argv[1],"--capture")==0 || capture_defuse);
    Memory memory;
    if (!memory.attach("cs2.exe")) { std::puts("FAIL: CS2 nao acessivel"); return 1; }
    EntityCache cache; cache.init(&memory);
    if (!cache.resolve()) { std::printf("FAIL: %s\n", cache.status().c_str()); memory.detach(); return 2; }
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    RECT area{0,0,1280,720}; const auto host=FindWindowW(nullptr,L"Counter-Strike 2");
    if (host) GetClientRect(host,&area);
    const int sw=(std::max)(1L,area.right),sh=(std::max)(1L,area.bottom);
    ImGui::GetIO().DisplaySize = {float(sw),float(sh)};
    ImGui::GetIO().DeltaTime = 1.f/60.f;
    unsigned char* pixels; int width, height;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    RenderCapture renderer;
    if (capture && !renderer.init(sw,sh)) { std::puts("FAIL: offscreen renderer"); memory.detach(); return 8; }
    Esp esp; if (capture) esp.init(renderer.device());
    Config cfg; cfg.item_esp = cfg.spectator_list = cfg.bomb_timer = true;
    cfg.radar_sz=420; cfg.radar_x=2; cfg.radar_y=8;
    bool captured=false;
    for (int sample=0;sample<(capture_defuse ? 225 : 5);++sample) {
        const auto start = std::chrono::steady_clock::now();
        cache.update();
        if (capture_defuse && (!cache.ready() || !cache.bomb().defuse_valid)) {
            if (sample%25==0) { std::puts("Aguardando desarme ativo para capturar o painel; apenas leitura."); std::fflush(stdout); }
            Sleep(400); continue;
        }
        std::printf("cache_update_ms=%.3f\n",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        const auto* local = cache.local();
        int alive = 0, bones = 0, weapons = 0, utilities = 0, observers = 0, named=0;
        for (const auto& player : cache.players()) {
            alive += player.valid(); bones += player.bones_valid; weapons += player.weapon_id != 0;
            utilities += static_cast<int>(player.util_ids.size()); observers += player.obs_target != 0;
            named+=player.rig.named;
        }
        std::printf("build=%u ready=%d players=%zu alive=%d bones=%d weapons=%d utilities=%d observers=%d items=%zu sens=%.3f local_health=%d bomb=%d clock=%d remaining=%.2f\n",
            offsets::BUILD_NUMBER, cache.ready(), cache.players().size(), alive, bones, weapons, utilities, observers,
            cache.items().size(), cache.sensitivity(), local ? local->health : -1, cache.bomb().planted,
            cache.bomb().clock_valid, cache.bomb().remaining);
        std::printf("named_rigs=%d local_crosshair_target=%llu\n",named,static_cast<unsigned long long>(local ? local->crosshair_target : 0));
        std::printf("map=%s\n",cache.map_name().c_str());
        std::printf("defusing=%d defuse_remaining=%.3f defuse_length=%.3f\n",cache.bomb().defusing,cache.bomb().defuse_remaining,cache.bomb().defuse_length);
        if (!cache.ready() || !local || cache.players().empty()) { std::printf("FAIL: %s\n", cache.status().c_str()); memory.detach(); return 3; }
        for (const auto& player : cache.players()) if (player.valid()) {
            if (!player.origin.finite() || player.health > 1000 || !player.bones_valid) {
                std::puts("FAIL: invalid live player record"); memory.detach(); return 4;
            }
            for (const auto& edge:bones::CONNECTIONS) {
                const int a=player.rig.joint[edge[0]], b=player.rig.joint[edge[1]];
                if (a<0 || b<0) { std::puts("FAIL: missing named body joint"); memory.detach(); return 6; }
                if ((player.bone[a].pos()-player.bone[b].pos()).len()>96.f) {
                    std::puts("FAIL: body connection reaches an accessory/outlier"); memory.detach(); return 7;
                }
            }
        }
        if (capture) renderer.begin();
        ImGui::NewFrame();
        esp.render(cache, cfg, ImGui::GetBackgroundDrawList(),sw,sh);
        ImGui::Render();
        const auto* draw = ImGui::GetDrawData();
        std::printf("render_vertices=%d render_indices=%d\n", draw->TotalVtxCount, draw->TotalIdxCount);
        if (!draw->TotalVtxCount) { std::puts("FAIL: no rendered game data"); memory.detach(); return 5; }
        if (capture && (sample==4 || capture_defuse)) {
            if (!renderer.save(std::filesystem::path(argv[2]))) { std::puts("FAIL: capture PNG"); memory.detach(); return 9; }
            captured=true;
            if (capture_defuse) { std::puts("PASS: live defuse panel captured."); break; }
        }
        Sleep(400);
    }
    memory.detach();
    renderer.shutdown();
    ImGui::DestroyContext();
    if (capture_defuse && !captured) { std::puts("NOT OBSERVED: no active defuse within the capture period."); return 10; }
    std::puts("PASS: live read-only entity, local-player, bone, weapon and projection integration.");
}
