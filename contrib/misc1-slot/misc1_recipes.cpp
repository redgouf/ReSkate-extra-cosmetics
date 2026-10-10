// The game's computed recipes (gameplay/characters/data/recipe/computedrecipes/
// computedrecipe_casrecipetemplate*) list one equip per port of CASRecipeTemplate, in the
// template's order. With a port added to the template and none to them, the skater is
// not drawn. This appends an empty Misc 1 equip to each, matching the port appended to
// the template.
// misc1_recipes <out dir> <computed recipe.ebx>...
#include "Engine/Resource/ebx_carry.h"
#include "Engine/Resource/ebx_writer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace ebx = dingosdk::frostbite::ebx;
namespace fs = std::filesystem;

namespace {
constexpr std::uint32_t misc1SlotHash = 877037134;   // djb2-xor of "cust_misc1", as SkaterSlots has it

std::vector<std::byte> load(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + p.string());
    std::vector<char> c((std::istreambuf_iterator<char>(f)), {});
    std::vector<std::byte> out(c.size());
    std::memcpy(out.data(), c.data(), c.size());
    return out;
}
ebx::Value *field(ebx::Object &o, std::string_view name) {
    for (auto &f : o.fields)
        if (f.name == name) return &f.value;
    return nullptr;
}
// Sets a number field to `value`, keeping the type it was read as.
void set_number(ebx::Value &v, std::uint64_t value) {
    if (std::holds_alternative<std::int64_t>(v.data)) v.data = static_cast<std::int64_t>(static_cast<std::int32_t>(value));
    else v.data = value;
}
std::uint64_t number(const ebx::Value &v) {
    if (auto *i = std::get_if<std::int64_t>(&v.data)) return static_cast<std::uint32_t>(*i);
    if (auto *u = std::get_if<std::uint64_t>(&v.data)) return *u;
    throw std::runtime_error("not a number");
}
std::shared_ptr<ebx::Object> copy(const ebx::Object &o) {   // equips hold no pointers or boxes
    auto out = std::make_shared<ebx::Object>(o);
    for (auto &f : out->fields)
        if (auto *nested = std::get_if<std::shared_ptr<ebx::Object>>(&f.value.data); nested && *nested) f.value.data = copy(**nested);
    return out;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "usage: misc1_recipes <out dir> <computed recipe.ebx>...\n";
        return 2;
    }
    try {
        const fs::path out = argv[1];
        fs::create_directories(out);
        for (int a = 2; a < argc; ++a) {
            const fs::path path = argv[a];
            auto doc = ebx::detail::clone_document(ebx::read_document(load(path)));
            auto &root = *doc.instances.at(0).object;
            auto *compiled = field(root, "CompiledRecipe");
            auto *recipe = compiled ? std::get_if<std::shared_ptr<ebx::Object>>(&compiled->data) : nullptr;
            if (!recipe || !*recipe) throw std::runtime_error(path.string() + ": no CompiledRecipe");
            auto *equipsValue = field(**recipe, "Equips");
            auto *equips = equipsValue ? std::get_if<ebx::Value::Array>(&equipsValue->data) : nullptr;
            if (!equips || equips->empty()) throw std::runtime_error(path.string() + ": no Equips");
            bool present = false;
            const ebx::Object *empty = nullptr;
            for (auto &e : *equips) {
                auto &equip = **std::get_if<std::shared_ptr<ebx::Object>>(&e.data);
                if (number(*field(equip, "SlotHash")) == misc1SlotHash) present = true;
                const auto *key = std::get_if<std::string>(&field(equip, "AssetKey")->data);
                const auto *params = std::get_if<ebx::Value::Array>(&field(equip, "FloatParams")->data);
                if (!empty && key && key->empty() && params && params->empty()) empty = &equip;
            }
            if (present) { std::cout << path.filename().string() << ": already has Misc 1\n"; continue; }
            if (!empty) throw std::runtime_error(path.string() + ": no empty equip to copy");
            auto added = copy(*empty);
            set_number(*field(*added, "SlotHash"), misc1SlotHash);
            set_number(*field(*added, "AssetKeyHash"), 0);
            equips->push_back(ebx::Value{added});
            const auto bytes = ebx::write_document(doc);
            std::ofstream(out / path.filename(), std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            std::cout << path.filename().string() << ": " << equips->size() << " equips\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
