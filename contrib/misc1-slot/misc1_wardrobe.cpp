// Adds the Misc 1 tab to the wardrobe's Accessories page.
// misc1_wardrobe <ms_skater_slots_contentresources.ebx> <ms_tabbedcontentslotpages_contentresources.ebx> <out dir>
#include "Engine/Resource/ebx_carry.h"
#include "Engine/Resource/ebx_writer.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>

namespace ebx = dingosdk::frostbite::ebx;
namespace fs = std::filesystem;
using dingosdk::frostbite::Guid;

namespace {
std::vector<std::byte> load(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + p.string());
    std::vector<char> c((std::istreambuf_iterator<char>(f)), {});
    std::vector<std::byte> out(c.size());
    std::memcpy(out.data(), c.data(), c.size());
    return out;
}
void save(const fs::path &p, const std::vector<std::byte> &bytes) {
    std::ofstream(p, std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
Guid guid_of(std::string_view name) { // same scheme as misc1.cpp
    Guid g;
    std::uint64_t a = 0xcbf29ce484222325ULL, b = 0x84222325cbf29ce4ULL;
    for (unsigned char c : name) { a = (a ^ c) * 0x100000001b3ULL; b = (b ^ c) * 0x100000001b3ULL + 0x9e3779b97f4a7c15ULL; }
    std::memcpy(g.bytes.data(), &a, 8);
    std::memcpy(g.bytes.data() + 8, &b, 8);
    g.bytes[7] = static_cast<std::byte>((static_cast<unsigned>(g.bytes[7]) & 0x0F) | 0x40);
    g.bytes[8] = static_cast<std::byte>((static_cast<unsigned>(g.bytes[8]) & 0x3F) | 0x80);
    return g;
}
const ebx::Value *field(const ebx::Object &o, std::string_view name) {
    for (auto &f : o.fields)
        if (f.name == name) return &f.value;
    return nullptr;
}
std::string debug_name(const ebx::Document &d, std::size_t i) {
    if (!d.instances[i].object) return {};
    const auto *v = field(*d.instances[i].object, "DebugName");
    const auto *s = v ? std::get_if<std::string>(&v->data) : nullptr;
    return s ? *s : std::string{};
}
std::int32_t by_debug_name(const ebx::Document &d, std::string_view name) {
    for (std::size_t i = 0; i < d.instances.size(); ++i)
        if (debug_name(d, i) == name) return static_cast<std::int32_t>(i);
    throw std::runtime_error("no instance named " + std::string(name));
}
std::int32_t add_import(ebx::Document &d, const Guid &file, const Guid &instance) {
    for (std::size_t i = 0; i < d.imports.size(); ++i)
        if (d.imports[i].fileGuid == file && d.imports[i].classGuid == instance) return static_cast<std::int32_t>(i);
    d.imports.push_back({file, instance});
    return static_cast<std::int32_t>(d.imports.size() - 1);
}

// A deep copy of a value with every rule applied: boxed values are copied too, so the
// source document's objects are never edited.
struct Rewrite {
    std::map<std::int32_t, std::int32_t> internal;      // instance index -> replacement index
    std::map<std::int32_t, std::int32_t> imports;       // import index -> replacement import index
    std::map<std::string, std::string> strings;
    std::function<void(ebx::Value::Array &, const ebx::Value::Array &)> onArray; // may append to a copied array

    ebx::Value value(const ebx::Value &source) const {
        ebx::Value out;
        std::visit([&](const auto &x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, ebx::PointerReference>) {
                auto p = x;
                if (p.kind == ebx::PointerKind::internal) { if (auto f = internal.find(p.index); f != internal.end()) p.index = f->second; }
                else if (p.kind == ebx::PointerKind::external) { if (auto f = imports.find(p.index); f != imports.end()) p.index = f->second; }
                out.data = p;
            } else if constexpr (std::is_same_v<T, std::string>) {
                auto f = strings.find(x);
                out.data = f == strings.end() ? x : f->second;
            } else if constexpr (std::is_same_v<T, std::shared_ptr<ebx::Object>>) {
                out.data = x ? object(*x) : std::shared_ptr<ebx::Object>{};
            } else if constexpr (std::is_same_v<T, ebx::Value::Array>) {
                ebx::Value::Array copy;
                for (const auto &e : x) copy.push_back(value(e));
                if (onArray) onArray(copy, x);
                out.data = std::move(copy);
            } else if constexpr (std::is_same_v<T, ebx::BoxedReference>) {
                auto b = x;
                if (x.value) b.value = std::make_shared<const ebx::Value>(value(*x.value));
                out.data = b;
            } else {
                out.data = x;
            }
        }, source.data);
        return out;
    }
    std::shared_ptr<ebx::Object> object(const ebx::Object &source) const {
        auto o = std::make_shared<ebx::Object>();
        o->descriptor = source.descriptor;
        for (const auto &f : source.fields) o->fields.push_back({f.descriptor, f.name, value(f.value)});
        return o;
    }
};
} // namespace

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "usage: misc1_wardrobe <ms_skater_slots_contentresources.ebx> <ms_tabbedcontentslotpages_contentresources.ebx> <out dir>\n";
        return 2;
    }
    try {
        const fs::path slotsPath = argv[1], pagesPath = argv[2], out = argv[3];
        fs::create_directories(out);

        // ---- MS_Skater_Slots_ContentResources: the Misc 1 Slot / Stub / Tab --------------------
        auto slots = ebx::detail::clone_document(ebx::read_document(load(slotsPath)));
        const std::string prefix = "MS_Skater_Slots_ContentResources/MS_Skater_Accessories_";
        const auto slotIndex = by_debug_name(slots, prefix + "HeadwearSlot");
        const auto stubIndex = by_debug_name(slots, prefix + "Headwear_Stub");
        const auto tabIndex = by_debug_name(slots, prefix + "Headwear_Tab");
        const Guid newSlotGuid = guid_of("misc1/ui/slot"), newStubGuid = guid_of("misc1/ui/stub"), newTabGuid = guid_of("misc1/ui/tab");
        // The skater slot the tab is about: cust_hats in SkaterSlots, replaced by cust_misc1.
        Guid hatsSlotFile, hatsSlotInstance;
        bool found = false;
        for (const auto &imp : slots.imports)
            if (imp.classGuid.string() == "ef26341e-c80d-4dda-a594-15e7c2964868") hatsSlotFile = imp.fileGuid, hatsSlotInstance = imp.classGuid, found = true;
        if (!found) throw std::runtime_error("the cust_hats slot import is missing");
        Rewrite rewrite;
        for (std::size_t i = 0; i < slots.imports.size(); ++i)
            if (slots.imports[i].classGuid == hatsSlotInstance)
                rewrite.imports[static_cast<std::int32_t>(i)] = add_import(slots, hatsSlotFile, guid_of("misc1/skaterslot/instance"));
        const auto base = static_cast<std::int32_t>(slots.instances.size());
        rewrite.internal = {{slotIndex, base}, {stubIndex, base + 1}, {tabIndex, base + 2}};
        rewrite.strings = {
            {prefix + "HeadwearSlot", prefix + "Misc1Slot"},
            {prefix + "Headwear_Stub", prefix + "Misc1_Stub"},
            {prefix + "Headwear_Tab", prefix + "Misc1_Tab"},
            {"ID_CUSTOMIZE_SKATER_HEAD_HEADWEAR", "ID_CUSTOMIZE_SKATER_ACCESSORIES_MISC1"},
        };
        for (const auto &[from, guid] : {std::pair{slotIndex, newSlotGuid}, {stubIndex, newStubGuid}, {tabIndex, newTabGuid}}) {
            auto copy = slots.instances.at(from);
            copy.instanceGuid = guid;
            copy.rawImage.clear();
            copy.object = rewrite.object(*slots.instances.at(from).object);
            slots.instances.push_back(std::move(copy));
        }
        // The resource list at the root: the copies go after the originals.
        auto &rootObject = *slots.instances.at(0).object;
        for (auto &f : rootObject.fields)
            if (f.name == "Resources") {
                auto &list = std::get<ebx::Value::Array>(f.value.data);
                for (std::int32_t i = 0; i < 3; ++i) list.push_back(ebx::Value{ebx::PointerReference{ebx::PointerKind::internal, base + i}});
            }
        ebx::detail::sort_instances(slots);
        save(out / "ms_skater_slots_contentresources.ebx", ebx::write_document(slots));

        // ---- MS_TabbedContentSlotPages_ContentResources: Misc 1 on the Accessories page --------
        auto pages = ebx::detail::clone_document(ebx::read_document(load(pagesPath)));
        const auto accessories = by_debug_name(pages, "MS_TabbedContentSlotPages_ContentResources/MS_Skater_Accessories_TabbedSlotContent");
        const auto slotsFile = slots.fileGuid;
        // The page's Headwear entry is the instance that refers to the Headwear stub.
        Guid headwearStub;
        for (const auto &instance : slots.instances)
            if (instance.object && instance.instanceGuid != newStubGuid) {
                const auto *v = field(*instance.object, "DebugName");
                if (v && std::get_if<std::string>(&v->data) && *std::get_if<std::string>(&v->data) == prefix + "Headwear_Stub")
                    headwearStub = instance.instanceGuid;
            }
        std::int32_t stubImport = -1;
        for (std::size_t i = 0; i < pages.imports.size(); ++i)
            if (pages.imports[i].fileGuid == slotsFile && pages.imports[i].classGuid == headwearStub) stubImport = static_cast<std::int32_t>(i);
        if (stubImport < 0) throw std::runtime_error("the Accessories page does not list the Headwear stub");
        std::int32_t entry = -1;
        std::function<bool(const ebx::Value &)> mentions = [&](const ebx::Value &v) -> bool {
            if (auto *p = std::get_if<ebx::PointerReference>(&v.data)) return p->kind == ebx::PointerKind::external && p->index == stubImport;
            if (auto *o = std::get_if<std::shared_ptr<ebx::Object>>(&v.data)) { if (*o) for (auto &f : (*o)->fields) if (mentions(f.value)) return true; return false; }
            if (auto *a = std::get_if<ebx::Value::Array>(&v.data)) { for (auto &e : *a) if (mentions(e)) return true; return false; }
            if (auto *b = std::get_if<ebx::BoxedReference>(&v.data)) return b->value && mentions(*b->value);
            return false;
        };
        for (std::size_t i = 0; i < pages.instances.size(); ++i)
            if (static_cast<std::int32_t>(i) != accessories && pages.instances[i].object) {
                bool hit = false;
                for (auto &f : pages.instances[i].object->fields) hit |= mentions(f.value);
                if (hit) entry = static_cast<std::int32_t>(i);
            }
        if (entry < 0) throw std::runtime_error("the Accessories page's Headwear entry is missing");
        Rewrite entryCopy;
        entryCopy.imports[stubImport] = add_import(pages, slotsFile, newStubGuid);
        const auto newEntry = static_cast<std::int32_t>(pages.instances.size());
        auto copy = pages.instances.at(entry);
        copy.rawImage.clear();
        if (copy.exported) copy.instanceGuid = guid_of("misc1/ui/page-entry");
        copy.object = entryCopy.object(*pages.instances.at(entry).object);
        pages.instances.push_back(std::move(copy));
        // Append the new entry to whichever list on the page holds the Headwear entry.
        int appended = 0;
        Rewrite pageEdit;
        pageEdit.onArray = [&](ebx::Value::Array &copyList, const ebx::Value::Array &original) {
            for (const auto &e : original)
                if (auto *p = std::get_if<ebx::PointerReference>(&e.data); p && p->kind == ebx::PointerKind::internal && p->index == entry) {
                    copyList.push_back(ebx::Value{ebx::PointerReference{ebx::PointerKind::internal, newEntry}});
                    ++appended;
                    return;
                }
        };
        // The list sits in whichever object holds the entries directly (the page's own value
        // points at it); that object is the one rewritten.
        for (std::size_t i = 0; i < pages.instances.size(); ++i) {
            if (!pages.instances[i].object || static_cast<std::int32_t>(i) == newEntry) continue;
            const auto before = appended;
            auto edited = pageEdit.object(*pages.instances[i].object);
            if (appended != before) pages.instances[i].object = edited;
        }
        if (appended != 1) throw std::runtime_error("expected one tab list on the Accessories page, found " + std::to_string(appended));
        ebx::detail::sort_instances(pages);
        save(out / "ms_tabbedcontentslotpages_contentresources.ebx", ebx::write_document(pages));
        std::cout << "built: slots doc " << slots.instances.size() << " instances, pages doc " << pages.instances.size() << " instances\n";
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
