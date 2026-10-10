// Builds the "Misc 1" accessory slot's game data from the base assets.
// misc1_data <in dir> <out dir>
//   in:  skaterslots.ebx cust_hats.ebx casrecipetemplate.ebx cas_scalphair_slot.ebx
//        cas_rsp_lualoader_prefab.ebx own_headwearhat_gen_beaniewatch_00009.ebx itemmastercollection.ebx
//        (decoded, from the game: see README.md)
//   out: the same edited, plus cust_misc1.ebx cas_misc1_slot.ebx own_misc1_test_beaniewatch.ebx,
//        and guids.txt (every new identity).
#include "Engine/Resource/ebx_carry.h"
#include "Engine/Resource/ebx_writer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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
ebx::Document read(const fs::path &p) { return ebx::read_document(load(p)); }

// A stable identity per name, so rebuilding gives the same guids (FNV-1a over the name, twice).
Guid guid_of(std::string_view name) {
    Guid g;
    std::uint64_t a = 0xcbf29ce484222325ULL, b = 0x84222325cbf29ce4ULL;
    for (unsigned char c : name) { a = (a ^ c) * 0x100000001b3ULL; b = (b ^ c) * 0x100000001b3ULL + 0x9e3779b97f4a7c15ULL; }
    std::memcpy(g.bytes.data(), &a, 8);
    std::memcpy(g.bytes.data() + 8, &b, 8);
    g.bytes[7] = static_cast<std::byte>((static_cast<unsigned>(g.bytes[7]) & 0x0F) | 0x40); // version 4 shape
    g.bytes[8] = static_cast<std::byte>((static_cast<unsigned>(g.bytes[8]) & 0x3F) | 0x80);
    return g;
}
std::uint32_t djb2(std::string_view s) {
    std::uint32_t h = 5381;
    for (unsigned char c : s) h = h * 33 ^ c;
    return h;
}

ebx::Value &field(ebx::Object &o, std::string_view name) {
    for (auto &f : o.fields)
        if (f.name == name) return f.value;
    throw std::runtime_error("no field " + std::string(name));
}
template <class T> T &as(ebx::Value &v, const char *what) {
    auto *p = std::get_if<T>(&v.data);
    if (!p) throw std::runtime_error(std::string("unexpected value type for ") + what);
    return *p;
}
void set_number(ebx::Value &v, std::uint64_t n) {
    if (std::holds_alternative<std::int64_t>(v.data)) v.data = static_cast<std::int64_t>(n);
    else v.data = n;
}
ebx::Object &root(ebx::Document &d) { return *d.instances.at(0).object; }
std::int32_t add_import(ebx::Document &d, const Guid &file, const Guid &instance) {
    for (std::size_t i = 0; i < d.imports.size(); ++i)
        if (d.imports[i].fileGuid == file && d.imports[i].classGuid == instance) return static_cast<std::int32_t>(i);
    d.imports.push_back({file, instance});
    return static_cast<std::int32_t>(d.imports.size() - 1);
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 3) { std::cerr << "usage: misc1_data <decoded base assets dir> <out dir>\n"; return 2; }
    try {
        const fs::path in = argv[1], out = argv[2];
        fs::create_directories(out);
        std::ofstream ids(out / "guids.txt");

        // --- New identities ------------------------------------------------------
        const std::string typeName = "cust_misc1";
        const auto typeHash = djb2(typeName);
        const std::string slotAssetName = "Characters/MainCharacters/Generic/CAS/Common/Slots/CAS_Misc1_Slot";
        const std::string itemKey = "Own_Misc1_Test_BeanieWatch_00009";
        const Guid typeFile = guid_of("misc1/itemtype/file"), typeRoot = guid_of("misc1/itemtype/root"),
                   typeData = guid_of("misc1/itemtype/data");
        const Guid slotInstance = guid_of("misc1/skaterslot/instance");
        const Guid appearanceFile = guid_of("misc1/appearanceslot/file"), appearanceRoot = guid_of("misc1/appearanceslot/root");
        const Guid itemFile = guid_of("misc1/testitem/file"), itemRoot = guid_of("misc1/testitem/root");

        // --- Item type: a copy of cust_hats ----------------------------------------
        auto type = ebx::detail::clone_document(read(in / "cust_hats.ebx"));
        type.fileGuid = typeFile;
        type.instances.at(0).instanceGuid = typeRoot;
        if (type.instances.size() > 1) type.instances.at(1).instanceGuid = typeData;
        auto &typeObject = root(type);
        field(typeObject, "Name").data = std::string("Items/" + typeName + "/" + typeName);
        field(typeObject, "ItemTypeName").data = typeName;
        set_number(field(typeObject, "ItemTypeHash"), typeHash);
        field(typeObject, "ItemTypeDisplayName").data = std::string("Misc 1");
        save(out / "cust_misc1.ebx", ebx::write_document(type));

        // --- SkaterSlots: a copy of the cust_hats slot, pointed at cust_misc1 -------------
        auto slots = ebx::detail::clone_document(read(in / "skaterslots.ebx"));
        auto &slotList = as<ebx::Value::Array>(field(root(slots), "Slots"), "Slots");
        std::int32_t hatsIndex = -1;
        for (const auto &entry : slotList) {
            const auto &pointer = std::get<ebx::PointerReference>(entry.data);
            auto &object = *slots.instances.at(pointer.index).object;
            if (as<std::string>(field(object, "SlotName"), "SlotName") == "cust_hats") hatsIndex = pointer.index;
        }
        if (hatsIndex < 0) throw std::runtime_error("cust_hats slot not found");
        const auto &hats = slots.instances.at(hatsIndex);
        ebx::InstanceRecord misc = hats;
        misc.instanceGuid = slotInstance;
        misc.rawImage.clear();
        misc.object = ebx::detail::clone_object(*hats.object);
        field(*misc.object, "SlotName").data = typeName;
        set_number(field(*misc.object, "SlotHash"), typeHash);
        field(*misc.object, "ItemType").data = ebx::PointerReference{ebx::PointerKind::external, add_import(slots, typeFile, typeRoot)};
        slots.instances.push_back(misc);
        slotList.push_back(ebx::Value{ebx::PointerReference{ebx::PointerKind::internal, static_cast<std::int32_t>(slots.instances.size() - 1)}});
        ebx::detail::sort_instances(slots);
        save(out / "skaterslots.ebx", ebx::write_document(slots));

        // --- CASRecipeTemplate: a "Misc1" port on the new slot -------------------------------
        auto tpl = ebx::detail::clone_document(read(in / "casrecipetemplate.ebx"));
        auto &ports = as<ebx::Value::Array>(field(root(tpl), "Slots"), "template Slots");
        std::shared_ptr<ebx::Object> headgear;
        for (auto &port : ports) {
            auto &object = as<std::shared_ptr<ebx::Object>>(port, "port");
            if (as<std::string>(field(*object, "PortName"), "PortName") == "Headgear") headgear = object;
        }
        if (!headgear) throw std::runtime_error("Headgear port not found");
        auto port = ebx::detail::clone_object(*headgear);
        field(*port, "PortName").data = std::string("Misc1");
        field(*port, "Slot").data = ebx::PointerReference{ebx::PointerKind::external, add_import(tpl, slots.fileGuid, slotInstance)};
        ports.push_back(ebx::Value{port});
        save(out / "casrecipetemplate.ebx", ebx::write_document(tpl));

        // --- The body mesh slot: a copy of CAS_ScalpHair_Slot ---------------------------------
        auto appearance = ebx::detail::clone_document(read(in / "cas_scalphair_slot.ebx"));
        appearance.fileGuid = appearanceFile;
        appearance.instances.at(0).instanceGuid = appearanceRoot;
        field(root(appearance), "Name").data = slotAssetName;
        save(out / "cas_misc1_slot.ebx", ebx::write_document(appearance));

        // --- The Lua loader's slot list: "Misc1" -> CAS_Misc1_Slot -------------------------------
        auto prefab = ebx::detail::clone_document(read(in / "cas_rsp_lualoader_prefab.ebx"));
        std::shared_ptr<ebx::Object> loader;
        std::size_t loaderIndex = 0;
        for (std::size_t i = 0; i < prefab.instances.size(); ++i)
            if (prefab.instances[i].object && prefab.types.at(prefab.instances[i].object->descriptor).name == "DingoLuaAppearanceLoaderEntityData")
                loader = prefab.instances[i].object, loaderIndex = i;
        if (!loader) throw std::runtime_error("loader entity not found");
        auto &definitions = as<ebx::Value::Array>(field(*loader, "SlotDefinitions"), "SlotDefinitions");
        std::int32_t headgearDefinition = -1;
        for (auto &entry : definitions) {
            const auto &pointer = std::get<ebx::PointerReference>(entry.data);
            if (as<std::string>(field(*prefab.instances.at(pointer.index).object, "LuaVarName"), "LuaVarName") == "Headgear")
                headgearDefinition = pointer.index;
        }
        if (headgearDefinition < 0) throw std::runtime_error("Headgear slot definition not found");
        ebx::InstanceRecord definition = prefab.instances.at(headgearDefinition);
        definition.rawImage.clear();
        definition.object = ebx::detail::clone_object(*definition.object);
        field(*definition.object, "LuaVarName").data = std::string("Misc1");
        field(*definition.object, "PathToAsset").data = slotAssetName;
        prefab.instances.push_back(definition);
        definitions.push_back(ebx::Value{ebx::PointerReference{ebx::PointerKind::internal, static_cast<std::int32_t>(prefab.instances.size() - 1)}});
        // The loader finds the slot by its path; the import is what makes ReSkate's mod merge
        // carry the new slot asset along with this change (it only carries what a change refers to).
        add_import(prefab, appearanceFile, appearanceRoot);
        (void)loaderIndex;
        ebx::detail::sort_instances(prefab);
        save(out / "cas_rsp_lualoader_prefab.ebx", ebx::write_document(prefab));

        // --- A test item: the black beanie, as a Misc 1 item ------------------------------------
        auto item = ebx::detail::clone_document(read(in / "own_headwearhat_gen_beaniewatch_00009.ebx"));
        item.fileGuid = itemFile;
        item.instances.at(0).instanceGuid = itemRoot;
        for (std::size_t i = 1; i < item.instances.size(); ++i)
            if (item.instances[i].exported) item.instances[i].instanceGuid = guid_of("misc1/testitem/" + std::to_string(i));
        auto &itemObject = root(item);
        field(itemObject, "Name").data = std::string("Items/" + typeName + "/" + itemKey);
        field(itemObject, "Key").data = itemKey;
        set_number(field(itemObject, "HashedAssetKey"), djb2(itemKey));
        // ReSkate refuses a mod item that looks like an item the game's store sells
        // (mod_store_copies.h). Beanie 00009 is a store item; 00001 is not, so the test
        // item wears 00001's appearance preset (colours) on the beanie mesh both share.
        {
            const auto &pointer = as<ebx::PointerReference>(field(itemObject, "ItemData"), "ItemData");
            if (pointer.kind != ebx::PointerKind::internal) throw std::runtime_error("ItemData is not in the item");
            auto &data = *item.instances.at(pointer.index).object;
            int replaced = 0;
            for (auto &path : as<ebx::Value::Array>(field(data, "AssetPaths"), "AssetPaths")) {
                auto &entry = *as<std::shared_ptr<ebx::Object>>(path, "asset path");
                auto &name = as<std::string>(field(entry, "AssetName"), "AssetName");
                if (name == "Gen_Hat_BeanieWatch_00009_AP") name = "Gen_Hat_BeanieWatch_00001_AP", ++replaced;
            }
            if (replaced != 1) throw std::runtime_error("the beanie's appearance preset was not found");
        }
        auto &categories = as<ebx::Value::Array>(field(itemObject, "Categories"), "Categories");
        categories.assign(1, ebx::Value{ebx::PointerReference{ebx::PointerKind::external, add_import(item, typeFile, typeRoot)}});
        ebx::detail::sort_instances(item);
        save(out / "own_misc1_test_beaniewatch.ebx", ebx::write_document(item));

        // --- Register the test item in the item master collection (a root list: mods combine) ---
        auto collection = ebx::detail::clone_document(read(in / "itemmastercollection.ebx"));
        auto &items = as<ebx::Value::Array>(field(root(collection), "Items"), "Items");
        items.push_back(ebx::Value{ebx::PointerReference{ebx::PointerKind::external, add_import(collection, itemFile, itemRoot)}});
        save(out / "itemmastercollection.ebx", ebx::write_document(collection));

        ids << "item_type " << typeName << " hash " << typeHash << " file " << typeFile.string() << " root " << typeRoot.string() << '\n'
            << "skater_slot instance " << slotInstance.string() << " in " << slots.fileGuid.string() << '\n'
            << "appearance_slot " << slotAssetName << " hash " << djb2(slotAssetName) << " file " << appearanceFile.string()
            << " root " << appearanceRoot.string() << '\n'
            << "test_item " << itemKey << " hash " << djb2(itemKey) << " file " << itemFile.string() << " root " << itemRoot.string() << '\n';
        std::cout << "built\n";
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
