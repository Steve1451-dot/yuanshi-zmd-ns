#include "mihoyo/native_decoder_selection.hpp"
#include <filesystem>
#include <chrono>
#include <cassert>
int main() {
    using cloudgenshin::NativeDecoderGame;
    using cloudgenshin::NativeDecoderSelection;
    const auto dir=std::filesystem::temp_directory_path()/
        ("cgnx-decoder-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    const auto home=dir.string();
    const auto shared_marker=dir/"cloudgenshin-nvdec-active.txt";
    const auto shared_preference=dir/"cloudgenshin-decoder.txt";
    const auto genshin_marker=dir/"hk4e-nvdec-active.txt";
    const auto endfield_marker=dir/"endfield-nvdec-active.txt";

    // The old marker has no owner. New sessions create an independent marker
    // before starting NVDEC and leave the old file available to older builds.
    std::ofstream(shared_marker)<<"CGNX_NVDEC_ACTIVE_V1\n";
    NativeDecoderSelection genshin;
    assert(genshin.begin(home,NativeDecoderGame::Genshin));
    assert(std::filesystem::exists(genshin_marker));
    NativeDecoderSelection endfield;
    assert(endfield.begin(home,NativeDecoderGame::Endfield));
    assert(std::filesystem::exists(endfield_marker));
    // Endfield's clean exit removes only its own marker.
    endfield.finish();
    assert(!std::filesystem::exists(endfield_marker));
    assert(std::filesystem::exists(genshin_marker));
    assert(std::filesystem::exists(shared_marker));

    NativeDecoderSelection blocked_genshin;
    assert(!blocked_genshin.begin(home,NativeDecoderGame::Genshin) && blocked_genshin.blocked());
    NativeDecoderSelection star_rail;
    assert(star_rail.begin(home,NativeDecoderGame::StarRail));
    star_rail.finish();
    assert(!std::filesystem::exists(dir/"hkrpg-nvdec-active.txt"));

    // Scoped preferences take precedence over the read-only shared default.
    std::ofstream(shared_preference)<<"software\n";
    NativeDecoderSelection software;
    assert(!software.begin(home,NativeDecoderGame::Endfield));
    assert(!software.preferred() && !software.blocked());
    assert(software.toggle() && software.preferred());
    {
        NativeDecoderSelection independent;
        assert(independent.begin(home,NativeDecoderGame::Endfield));
        // Simulate termination without the stream view's call to finish().
    }
    NativeDecoderSelection legacy_default;
    assert(!legacy_default.begin(home,NativeDecoderGame::StarRail) && !legacy_default.preferred());
    assert(std::filesystem::exists(genshin_marker));

    // A deliberate software-to-hardware toggle re-arms only this title.
    NativeDecoderSelection blocked_endfield;
    assert(!blocked_endfield.begin(home,NativeDecoderGame::Endfield) && blocked_endfield.blocked());
    assert(blocked_endfield.toggle() && blocked_endfield.toggle());
    assert(!std::filesystem::exists(endfield_marker));
    assert(std::filesystem::exists(genshin_marker));
    assert(std::filesystem::exists(shared_marker));
    genshin.finish();

    // Damaged scoped preferences still fail safe, with a valid backup usable.
    std::ofstream(dir/"endfield-decoder.txt")<<std::string(100,'x');
    NativeDecoderSelection damaged;
    assert(!damaged.begin(home,NativeDecoderGame::Endfield) && !damaged.preferred());
    std::ofstream(dir/"endfield-decoder.txt.bak",std::ios::binary)<<"hardware\r\n";
    NativeDecoderSelection backup;
    assert(backup.begin(home,NativeDecoderGame::Endfield));
    backup.finish();
    for(const auto* name:{"cloudgenshin-decoder.txt","cloudgenshin-nvdec-active.txt",
                          "endfield-decoder.txt","endfield-decoder.txt.bak"}) {
        std::filesystem::remove(dir/name);
    }
    std::filesystem::remove(dir);
}
