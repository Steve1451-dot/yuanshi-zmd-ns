#pragma once
#include "atomic_file_replace.hpp"
#include <cstdio>
#include <fstream>
#include <optional>
#include <string>
namespace cloudgenshin {
enum class NativeDecoderGame { Genshin, StarRail, Endfield };

// UI owner only. A failed/unclean hardware attempt must not cause a crash loop.
// This is an experimental preference, not proof of correct hardware pixels.
class NativeDecoderSelection {
public:
    NativeDecoderSelection()=default;
    NativeDecoderSelection(const NativeDecoderSelection&)=delete;
    NativeDecoderSelection& operator=(const NativeDecoderSelection&)=delete;
    bool begin(const std::string& home, NativeDecoderGame game) {
        if(owned_) return true;
        preference_path_.clear();
        marker_.clear();
        blocked_=false;
        if(home.empty()) {blocked_=true;return false;}
        const auto scope=home+"/"+FilePrefix(game);
        preference_path_=scope+"-decoder.txt";
        marker_=scope+"-nvdec-active.txt";
        const auto saved=ReadPreference(preference_path_);
        // Import the old explicit preference as a read-only default. An old
        // unscoped crash marker cannot identify the game that owned it.
        preferred_=saved ? *saved : ReadPreference(home+"/cloudgenshin-decoder.txt").value_or(true);
        if(!preferred_) return false;
        if(std::ifstream(marker_).good()) {blocked_=true;return false;}
        std::ofstream out(marker_,std::ios::binary|std::ios::trunc);
        out<<"CGNX_NVDEC_ACTIVE_V1\n";out.flush();const bool good=out.good();out.close();
        owned_=good && !out.fail();blocked_=!owned_;return owned_;
    }
    void finish() {if(owned_) {std::remove(marker_.c_str());owned_=false;}}
    bool preferred() const {return preferred_;}
    bool blocked() const {return blocked_;}
    bool toggle() {
        if(preference_path_.empty()) return false;
        const bool next=!preferred_;
        const auto temporary=preference_path_+".tmp";
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        out<<(next ? "hardware\n" : "software\n");out.flush();const bool good=out.good();out.close();
        if(!good || out.fail() || !opennow::storage::ReplaceWithTemporaryFile(temporary,preference_path_)) return false;
        preferred_=next;
        // A deliberate software -> hardware choice rearms a previous unclean
        // attempt, but never removes a marker owned by this live session.
        if(next && !owned_) std::remove(marker_.c_str());
        return true;
    }
private:
    static const char* FilePrefix(NativeDecoderGame game) {
        switch(game) {
        case NativeDecoderGame::Genshin: return "hk4e";
        case NativeDecoderGame::StarRail: return "hkrpg";
        case NativeDecoderGame::Endfield: return "endfield";
        }
        return "unknown";
    }
    static std::optional<bool> ReadPreference(const std::string& path) {
        bool present=false;
        for(const auto& file:{path,path+".bak"}) {
            std::ifstream in(file,std::ios::binary);if(!in) continue;
            present=true;char text[32]{};in.read(text,sizeof(text));
            if(!in.eof()) continue;
            const std::string value(text,static_cast<size_t>(in.gcount()));
            if(value=="hardware\n" || value=="hardware\r\n") return true;
            if(value=="software\n" || value=="software\r\n") return false;
        }
        if(present) return false; // A damaged saved preference defaults to software.
        return std::nullopt;
    }
    std::string preference_path_,marker_;
    bool preferred_=true,blocked_=false,owned_=false;
};
}
