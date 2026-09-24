#include "ExportSkinClonePolicy.h"
#include <memory>
#include <stdexcept>
#include <iostream>

struct Skin { int root = 7; int bones = 11; };
int main()
{
    auto source = std::make_shared<Skin>();
    unsigned calls = 0;
    auto alias = SKEE::RequestIndependentExportSkin(source.get(), [&](Skin*) {
        ++calls; return source;
    });
    if (alias || calls != 1 || source->root != 7 || source->bones != 11 || source.use_count() != 1)
        throw std::runtime_error("Aliased skin clone was not rejected without source mutation");
    auto missing = SKEE::RequestIndependentExportSkin(source.get(), [](Skin*) {
        return std::shared_ptr<Skin>();
    });
    if (missing) throw std::runtime_error("Missing clone was accepted");
    auto copy = SKEE::RequestIndependentExportSkin(source.get(), [](Skin* s) {
        return std::make_shared<Skin>(*s);
    });
    if (!copy || copy.get() == source.get()) throw std::runtime_error("Independent clone was rejected");
    copy->root = 23; copy->bones = 29;
    if (source->root != 7 || source->bones != 11)
        throw std::runtime_error("Export mutation changed the source skin");
    std::cout << "Export skin clone: alias/null rejection, single request, source preservation passed.\n";
}
