// SPDX-License-Identifier: GPL-3.0-or-later
#include "SwfBytePatch.h"
#include "RaceSexMenuSwfFailurePolicy.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <zlib.h>
using namespace SKEE::SwfBytePatch;
void Require(bool value) { if (!value) throw std::runtime_error("Test failed"); }
void U32(Bytes& bytes, std::size_t at, std::uint32_t n) {
    for (unsigned i=0;i<4;++i) bytes.at(at+i)=static_cast<std::uint8_t>(n>>(i*8));
}
template<class F> void Rejected(F f) {
    bool caught{}; try { f(); } catch(const std::exception&) { caught=true; }
    Require(caught);
}
template<class F> void InputRejected(F f) {
    bool caught{}; try { f(); } catch(const IncompatibleSource&) { caught=true; }
    Require(caught);
}
template<class F> void OtherRejected(F f) {
    bool caught{};
    try { f(); } catch(const IncompatibleSource&) { throw std::runtime_error("Incorrectly blamed original SWF"); }
    catch(const std::exception&) { caught=true; }
    Require(caught);
}
Bytes Read(const char* path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if (!file || file.tellg()<0 || file.tellg()>kLimit) throw std::runtime_error(std::string("Invalid input: ")+path);
    Bytes bytes(static_cast<std::size_t>(file.tellg())); file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size())) throw std::runtime_error("Short read");
    return bytes;
}
int main(int argc, char** argv) try {
    const auto emptyHash = Sha256({});
    Require(emptyHash[0]==0xe3 && emptyHash[31]==0x55);
    Bytes source{'F','W','S',15,20,0,0,0,'s','y','n','t','h','e','t','i','c','!','!','!'};
    Require(Canonical(source)==source);
    Bytes compressed(8+compressBound(static_cast<uLong>(source.size()-8)));
    std::copy_n(source.begin(),8,compressed.begin()); compressed[0]='C';
    uLongf compressedSize=static_cast<uLongf>(compressed.size()-8);
    Require(compress2(compressed.data()+8,&compressedSize,source.data()+8,
        static_cast<uLong>(source.size()-8),Z_BEST_COMPRESSION)==Z_OK);
    compressed.resize(8+compressedSize);
    Require(Canonical(compressed)==source);
    for (std::size_t n=0;n<compressed.size();++n)
        Rejected([&]{Canonical(std::span(compressed).first(n));});
    auto badCompressed=compressed; badCompressed.push_back(0);
    Rejected([&]{Canonical(badCompressed);});
    badCompressed=compressed; badCompressed.back()^=1;
    Rejected([&]{Canonical(badCompressed);});
    for (auto length : {7U,19U,21U,0xffffffffU}) {
        badCompressed=compressed; U32(badCompressed,4,length);
        Rejected([&]{Canonical(badCompressed);});
    }
    auto badSource=source; badSource[0]='Z'; Rejected([&]{Canonical(badSource);});
    badSource=source; badSource.push_back(0); Rejected([&]{Canonical(badSource);});
    Bytes patch(129); std::copy_n("RMSWFP01",8,patch.begin());
    U32(patch,8,20); U32(patch,12,20); U32(patch,16,20); U32(patch,20,1);
    const auto hash=Sha256(source);
    for (auto at : {24,56,88}) std::copy(hash.begin(),hash.end(),patch.begin()+at);
    patch[120]=0; U32(patch,121,0); U32(patch,125,20);
    Require(Apply(source,patch)==source);
    auto wrongSource=source; wrongSource.push_back(0);
    InputRejected([&]{Apply(wrongSource,patch);});
    wrongSource=source; wrongSource.back()^=1;
    InputRejected([&]{Apply(wrongSource,patch);});
    auto wrongCanonical=patch; wrongCanonical[56]^=1;
    InputRejected([&]{Apply(source,wrongCanonical);});
    auto wrongOutput=patch; wrongOutput[88]^=1;
    OtherRejected([&]{Apply(source,wrongOutput);});
    OtherRejected([&]{ApplyRelease(source,patch);});
    using namespace SKEE::RaceSexMenuSwfPatch;
    const std::string incompatibleMessage=FailureMessage(FailureKind::IncompatibleMovie);
    const std::string installationMessage=FailureMessage(FailureKind::Initialization);
    Require(incompatibleMessage.find("Interface/VR/RaceSex_menu.swf")!=std::string::npos);
    Require(incompatibleMessage.find("0.4.20.0")!=std::string::npos);
    Require(incompatibleMessage.find("Restart Skyrim")!=std::string::npos);
    Require(installationMessage.find("RaceMenuNGVR2.log")!=std::string::npos);
    Require(installationMessage.find("incompatible menu file")==std::string::npos);
    for (std::size_t n=0;n<patch.size();++n) Rejected([&]{Apply(source,std::span(patch).first(n));});
    for (auto [at,n] : {std::pair{8,1U},{12,0U},{12,0xffffffffU},{16,7U},{16,0xffffffffU},
        {20,0U},{20,200001U},{121,0xffffffffU},{125,0U},{125,21U}}) {
        auto bad=patch; U32(bad,at,n); Rejected([&]{Apply(source,bad);});
    }
    for (auto at : {24,56,88,120}) { auto bad=patch; bad[at]^=255; Rejected([&]{Apply(source,bad);}); }
    auto bad=patch; bad.push_back(0); Rejected([&]{Apply(source,bad);});
    Rejected([&]{ApplyRelease(source,patch);});
    // Equivalent INSERT encoding, without relying on the generator.
    auto insert=patch; insert.resize(145); insert[120]=1; U32(insert,121,20);
    std::copy(source.begin(),source.end(),insert.begin()+125);
    Require(Apply(source,insert)==source);
    U32(insert,121,0); Rejected([&]{Apply(source,insert);});
    if (argc==4) {
        const auto original=Read(argv[1]), release=Read(argv[2]), target=Read(argv[3]);
        Require(ApplyRelease(original,release)==Canonical(target));
        auto altered=release; altered.back()^=1; Rejected([&]{ApplyRelease(original,altered);});
        auto wrong=original; wrong.back()^=1; Rejected([&]{ApplyRelease(wrong,release);});
        std::cout << "Private accepted SWF: native/reference exact match; release corruption rejected\n";
    } else if (argc!=1) throw std::runtime_error("Usage: test [original.swf release.rmp target.swf]");
    std::cout << "Native SWF patch policy tests passed\n";
    return 0;
} catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
