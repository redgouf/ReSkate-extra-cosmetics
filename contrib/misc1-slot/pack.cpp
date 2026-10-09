// Packs changed and added assets into a ReSkate mod folder, the way ReSkate Studio's
// patches are laid out: each touched bundle is listed again in full in the mod's own copy
// of its TOC, with a new asset list (manifest) and the new payloads in the mod's cas_01.cas;
// everything unchanged still points at the game's archives.
//
// pack <spec> <mod dir>
// spec lines:
//   TOC <base toc file> <toc path inside the mod, e.g. Win32/items.toc>
//   BUNDLE <bundle name> <file holding the base manifest>
//   EBX <asset name> <decoded payload>                 changed if the bundle has it, else added
//   RES <asset name> <decoded payload> <meta hex>       a changed resource, with its new resource meta
#include "Engine/Resource/toc.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>

namespace fb = dingosdk::frostbite;
namespace fs = std::filesystem;

namespace {
std::vector<std::byte> load(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + p.string());
    std::vector<char> c((std::istreambuf_iterator<char>(f)), {});
    std::vector<std::byte> out(c.size());
    std::memcpy(out.data(), c.data(), c.size());
    return out;
}

// SHA-1 (FIPS 180-4) of the decoded payload, as ReSkate's merge records it.
fb::Sha1 sha1(std::span<const std::byte> data) {
    std::uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::vector<std::uint8_t> m(reinterpret_cast<const std::uint8_t *>(data.data()), reinterpret_cast<const std::uint8_t *>(data.data()) + data.size());
    const std::uint64_t bits = static_cast<std::uint64_t>(data.size()) * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 7; i >= 0; --i) m.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));
    auto rol = [](std::uint32_t v, int s) { return (v << s) | (v >> (32 - s)); };
    for (std::size_t off = 0; off < m.size(); off += 64) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = (m[off + i * 4] << 24) | (m[off + i * 4 + 1] << 16) | (m[off + i * 4 + 2] << 8) | m[off + i * 4 + 3];
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20) f = (b & c) | (~b & d), k = 0x5A827999;
            else if (i < 40) f = b ^ c ^ d, k = 0x6ED9EBA1;
            else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
            else f = b ^ c ^ d, k = 0xCA62C1D6;
            const auto t = rol(a, 5) + f + e + k + w[i];
            e = d, d = c, c = rol(b, 30), b = a, a = t;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    }
    fb::Sha1 out;
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 4; ++j) out.bytes[i * 4 + j] = static_cast<std::byte>(h[i] >> (24 - j * 8));
    return out;
}

// Uncompressed CAS blocks, laid out as the game's own (and ReSkate's encode_cas) are.
std::vector<std::byte> encode_raw(std::span<const std::byte> decoded) {
    std::vector<std::byte> out;
    const std::size_t block = 0x10000;
    const auto count = std::max<std::size_t>(1, (decoded.size() + block - 1) / block);
    for (std::size_t i = 0; i < count; ++i) {
        const auto begin = std::min(decoded.size(), i * block);
        const auto size = std::min(block, decoded.size() - begin);
        const std::uint64_t packed = (static_cast<std::uint64_t>(size & 0xFFFFFF) << 32) | (0x7ULL << 20) | size;
        for (int shift = 56; shift >= 0; shift -= 8) out.push_back(static_cast<std::byte>((packed >> shift) & 0xFF));
        out.insert(out.end(), decoded.begin() + static_cast<std::ptrdiff_t>(begin), decoded.begin() + static_cast<std::ptrdiff_t>(begin + size));
    }
    return out;
}

struct Archive {
    std::vector<std::byte> bytes;
    fb::BundleFileInfo add(std::span<const std::byte> data) {
        fb::BundleFileInfo info;
        info.location = {true, 3162564249U, 1};
        info.offset = static_cast<std::uint32_t>(bytes.size());
        info.size = static_cast<std::uint32_t>(data.size());
        bytes.insert(bytes.end(), data.begin(), data.end());
        return info;
    }
};

std::vector<std::byte> unhex(const std::string &hex) {
    std::vector<std::byte> out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) out.push_back(static_cast<std::byte>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    return out;
}
std::string lower(std::string s) { for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
} // namespace

int main(int argc, char **argv) {
    if (argc != 3) { std::cerr << "usage: pack <spec> <mod dir>\n"; return 2; }
    try {
        const fs::path specPath = argv[1], mod = argv[2];
        struct Change { std::string name; fs::path payload; bool resource{}; std::vector<std::byte> meta; };
        struct BundleSpec { std::string name; fs::path manifest; std::vector<Change> changes; };
        struct TocSpec { fs::path base, inside; std::vector<BundleSpec> bundles; };
        std::vector<TocSpec> tocs;
        std::ifstream spec(specPath);
        for (std::string line; std::getline(spec, line);) {
            std::istringstream in(line);
            std::string kind;
            if (!(in >> kind) || kind.starts_with("#")) continue;
            if (kind == "TOC") { TocSpec t; std::string a, b; in >> a >> b; t.base = a; t.inside = b; tocs.push_back(t); }
            else if (kind == "BUNDLE") { BundleSpec b; std::string m; in >> b.name >> m; b.manifest = m; tocs.back().bundles.push_back(b); }
            else if (kind == "EBX") { Change c; std::string p; in >> c.name >> p; c.payload = p; tocs.back().bundles.back().changes.push_back(c); }
            else if (kind == "RES") { Change c; std::string p, meta; in >> c.name >> p >> meta; c.payload = p; c.resource = true; c.meta = unhex(meta); tocs.back().bundles.back().changes.push_back(c); }
            else throw std::runtime_error("unknown spec line: " + line);
        }

        Archive cas;
        for (const auto &toc : tocs) {
            const auto base = fb::read_toc(load(toc.base));
            std::vector<fb::TocBundle> written;
            for (const auto &want : toc.bundles) {
                const fb::TocBundle *found = nullptr;
                for (const auto &b : base.bundles) if (lower(b.name) == lower(want.name)) found = &b;
                if (!found) throw std::runtime_error("bundle not in " + toc.base.string() + ": " + want.name);
                const auto region = fb::read_bundle_region(found->region);
                if (!region.inlineManifest.empty()) throw std::runtime_error("inline manifests are not handled: " + want.name);
                auto manifest = fb::read_binary_bundle(load(want.manifest));
                const auto expected = manifest.ebx.size() + manifest.resources.size() + manifest.chunks.size();
                if (region.files.size() != expected + 1) throw std::runtime_error("manifest does not match the bundle: " + want.name);
                std::vector<fb::BundleFileInfo> ebxFiles(region.files.begin() + 1, region.files.begin() + 1 + static_cast<std::ptrdiff_t>(manifest.ebx.size()));
                std::vector<fb::BundleFileInfo> resFiles(region.files.begin() + 1 + static_cast<std::ptrdiff_t>(manifest.ebx.size()),
                                                         region.files.begin() + 1 + static_cast<std::ptrdiff_t>(manifest.ebx.size() + manifest.resources.size()));
                std::vector<fb::BundleFileInfo> chunkFiles(region.files.begin() + 1 + static_cast<std::ptrdiff_t>(manifest.ebx.size() + manifest.resources.size()), region.files.end());
                for (const auto &change : want.changes) {
                    const auto payload = load(change.payload);
                    const auto file = cas.add(encode_raw(payload));
                    auto &list = change.resource ? manifest.resources : manifest.ebx;
                    auto &files = change.resource ? resFiles : ebxFiles;
                    std::size_t at = list.size();
                    for (std::size_t i = 0; i < list.size(); ++i) if (lower(list[i].name) == lower(change.name)) at = i;
                    if (at == list.size()) {
                        if (change.resource) throw std::runtime_error("adding resources is not handled: " + change.name);
                        fb::BundleAsset asset;
                        asset.kind = fb::AssetKind::ebx;
                        asset.name = change.name;
                        list.push_back(asset);
                        files.push_back({});
                        std::cout << "  add    " << change.name << '\n';
                    } else {
                        std::cout << "  change " << change.name << '\n';
                    }
                    list[at].sha1 = sha1(payload);
                    list[at].originalSize = payload.size();
                    if (change.resource) list[at].resourceMeta = change.meta;
                    files[at] = file;
                }
                const auto manifestBytes = fb::write_binary_bundle(manifest);
                std::vector<fb::BundleFileInfo> files{cas.add(manifestBytes)};
                files.insert(files.end(), ebxFiles.begin(), ebxFiles.end());
                files.insert(files.end(), resFiles.begin(), resFiles.end());
                files.insert(files.end(), chunkFiles.begin(), chunkFiles.end());
                fb::TocBundle out{found->name, fb::write_bundle_region(files), found->loadFlag};
                written.push_back(std::move(out));
                std::cout << "bundle " << want.name << ": " << manifest.ebx.size() << " ebx, " << manifest.resources.size() << " resources, "
                          << manifest.chunks.size() << " chunks\n";
            }
            const auto path = mod / toc.inside;
            fs::create_directories(path.parent_path());
            const auto bytes = fb::write_patch_toc(written, {}, 3) /* patch TOCs (ReSkate Studio, ReSkate merge) use flags 3 */;
            std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        const auto casPath = mod / "Win32/configurations/layout/initialinstallpackage/cas_01.cas";
        fs::create_directories(casPath.parent_path());
        std::ofstream(casPath, std::ios::binary).write(reinterpret_cast<const char *>(cas.bytes.data()), static_cast<std::streamsize>(cas.bytes.size()));
        std::cout << "cas_01.cas: " << cas.bytes.size() << " bytes\n";
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
