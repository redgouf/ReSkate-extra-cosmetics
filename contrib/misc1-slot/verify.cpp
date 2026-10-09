// For every bundle in the mod TOC: read its manifest from the mod's cas (as ReSkate's list_bundle does,
// raw), compare it with the game's manifest by name and sha1, and decode each changed or added
// payload from the mod's cas, printing its size and sha1 so they can be checked against the built files.
// usage: verify <mod toc> <base toc> <mod cas> <bundle>=<base manifest file>...
#include "Engine/Resource/toc.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <cstdio>

namespace fb = dingosdk::frostbite;
std::vector<std::byte> load(const std::string &p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + p);
    std::vector<char> c((std::istreambuf_iterator<char>(f)), {});
    std::vector<std::byte> out(c.size()); std::memcpy(out.data(), c.data(), c.size()); return out;
}
std::string hex(const fb::Sha1 &s) { std::string o; char b[3]; for (auto x : s.bytes) { std::snprintf(b, 3, "%02x", static_cast<unsigned>(x)); o += b; } return o; }
std::vector<std::byte> decode_raw(std::span<const std::byte> p) {
    std::vector<std::byte> out; std::size_t at = 0;
    auto u = [&](std::size_t i) { return static_cast<unsigned>(p[i]); };
    while (at < p.size()) {
        std::uint32_t decoded = (u(at) << 24) | (u(at + 1) << 16) | (u(at + 2) << 8) | u(at + 3);
        const unsigned compression = u(at + 4) | (u(at + 5) << 8);
        std::size_t encoded = (u(at + 6) << 8) | u(at + 7);
        encoded += static_cast<std::size_t>((compression >> 8) & 0x0F) << 16;
        decoded &= 0xFFFFFF;
        if ((compression & 0x7F) != 0) throw std::runtime_error("mod payload is not raw");
        if (encoded != decoded) throw std::runtime_error("raw block size mismatch");
        at += 8;
        out.insert(out.end(), p.begin() + static_cast<std::ptrdiff_t>(at), p.begin() + static_cast<std::ptrdiff_t>(at + encoded));
        at += encoded;
    }
    return out;
}
int main(int argc, char **argv) {
    try {
        const auto modToc = fb::read_toc(load(argv[1]));
        const auto baseToc = fb::read_toc(load(argv[2]));
        const auto cas = load(argv[3]);
        std::map<std::string, std::string> baseManifests;
        for (int a = 4; a < argc; ++a) { std::string s = argv[a]; const auto eq = s.find('='); baseManifests[s.substr(0, eq)] = s.substr(eq + 1); }
        for (const auto &bundle : modToc.bundles) {
            const fb::TocBundle *game = nullptr;
            for (const auto &b : baseToc.bundles) if (b.name == bundle.name) game = &b;
            if (!game) throw std::runtime_error("mod bundle not in the game: " + bundle.name);
            const auto region = fb::read_bundle_region(bundle.region);
            const auto gameRegion = fb::read_bundle_region(game->region);
            const auto &mf = region.files.at(0);
            if (!mf.location.patch) throw std::runtime_error("manifest is not in the mod");
            const auto manifest = fb::read_binary_bundle(std::span(cas).subspan(mf.offset, mf.size));
            const auto gameManifest = fb::read_binary_bundle(load(baseManifests.at(bundle.name)));
            if (manifest.ebx.size() + manifest.resources.size() + manifest.chunks.size() != region.files.size() - 1)
                throw std::runtime_error("manifest count does not match the region");
            std::map<std::string, std::pair<std::string, std::size_t>> gameEbx, gameRes;
            for (std::size_t i = 0; i < gameManifest.ebx.size(); ++i) gameEbx[gameManifest.ebx[i].name] = {hex(gameManifest.ebx[i].sha1), 1 + i};
            for (std::size_t i = 0; i < gameManifest.resources.size(); ++i) gameRes[gameManifest.resources[i].name] = {hex(gameManifest.resources[i].sha1), 1 + gameManifest.ebx.size() + i};
            std::size_t same = 0, patchedButSame = 0, movedUnpatched = 0;
            auto check = [&](const fb::BundleAsset &asset, std::size_t fileIndex, const std::map<std::string, std::pair<std::string, std::size_t>> &ref, const char *kind) {
                const auto &file = region.files.at(fileIndex);
                const auto found = ref.find(asset.name);
                if (found != ref.end() && found->second.first == hex(asset.sha1)) {
                    ++same;
                    const auto &gf = gameRegion.files.at(found->second.second);
                    if (file.location.patch) ++patchedButSame;
                    else if (gf.offset != file.offset || gf.size != file.size || gf.location.archive != file.location.archive) ++movedUnpatched;
                    return;
                }
                if (!file.location.patch) throw std::runtime_error(std::string("changed asset not in the mod: ") + asset.name);
                const auto payload = decode_raw(std::span(cas).subspan(file.offset, file.size));
                std::cout << (found == ref.end() ? "  ADDED   " : "  CHANGED ") << kind << ' ' << asset.name << "  size " << payload.size()
                          << " (manifest " << asset.originalSize << ")  sha1 " << hex(asset.sha1) << '\n';
                if (payload.size() != asset.originalSize) throw std::runtime_error("size mismatch for " + asset.name);
            };
            std::cout << bundle.name << '\n';
            for (std::size_t i = 0; i < manifest.ebx.size(); ++i) check(manifest.ebx[i], 1 + i, gameEbx, "ebx");
            for (std::size_t i = 0; i < manifest.resources.size(); ++i) check(manifest.resources[i], 1 + manifest.ebx.size() + i, gameRes, "res");
            std::cout << "  unchanged " << same << " (in the mod anyway " << patchedButSame << ", pointing elsewhere than the game " << movedUnpatched
                      << "), chunks " << manifest.chunks.size() << " vs game " << gameManifest.chunks.size() << '\n';
        }
    } catch (const std::exception &e) { std::cerr << "error: " << e.what() << '\n'; return 1; }
}
