// vst3_load_test — loads the built VST3 with the Windows loader exactly as a
// host would, enumerates its classes and instantiates the audio processor.
//
// This is the closest thing to "does it load in Ableton" that can be checked
// without a DAW: LoadLibrary -> GetPluginFactory -> createInstance -> initialize.
//
// Build/run: tools/vst3_load_test.bat <path-to-SH-101.vst3 DLL>
#include <windows.h>

#include <cstdio>
#include <cstring>

#include "pluginterfaces/base/fplatform.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"

using namespace Steinberg;

namespace {

const char* mediaTypeName(Vst::MediaType t) {
    switch (t) {
        case Vst::kAudio: return "audio";
        case Vst::kEvent: return "event";
        default: return "other";
    }
}

const char* directionName(Vst::BusDirection d) {
    return d == Vst::kInput ? "input" : "output";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: vst3_load_test <path to SH-101.vst3 DLL>\n");
        return 2;
    }
    const char* dllPath = argv[1];

    HMODULE module = LoadLibraryA(dllPath);
    if (module == nullptr) {
        std::printf("FAIL: LoadLibrary failed with error %lu\n", GetLastError());
        return 1;
    }
    std::printf("LoadLibrary: ok\n");

    using GetFactoryFn = IPluginFactory*(PLUGIN_API*)();
    auto getFactory = reinterpret_cast<GetFactoryFn>(
        reinterpret_cast<void*>(GetProcAddress(module, "GetPluginFactory")));
    if (getFactory == nullptr) {
        std::printf("FAIL: GetPluginFactory not exported\n");
        return 1;
    }

    IPluginFactory* factory = getFactory();
    if (factory == nullptr) {
        std::printf("FAIL: GetPluginFactory returned null\n");
        return 1;
    }
    std::printf("GetPluginFactory: ok\n");

    const int classCount = factory->countClasses();
    std::printf("classes: %d\n", classCount);

    PClassInfo classInfo{};
    TUID componentCid{};
    bool haveComponent = false;

    for (int i = 0; i < classCount; ++i) {
        PClassInfo info{};
        if (factory->getClassInfo(i, &info) != kResultOk) continue;
        char cidString[40] = {};
        FUID uid(info.cid);            // PClassInfo carries a raw TUID
        uid.toRegistryString(cidString);
        std::printf("  [%d] %s | %s | %s\n", i, info.name, info.category, cidString);
        // "Audio Module Class" is the VST3 category string for an audio processor.
        if (std::strcmp(info.category, "Audio Module Class") == 0 && !haveComponent) {
            std::memcpy(componentCid, info.cid, sizeof(TUID));
            haveComponent = true;
        }
    }

    int exitCode = 0;
    if (!haveComponent) {
        std::printf("FAIL: no audio module class found\n");
        exitCode = 1;
    } else {
        // Instantiate the processor exactly as a host does.
        FUnknown* unknown = nullptr;
        tresult result = factory->createInstance(componentCid, Vst::IComponent::iid,
                                                 reinterpret_cast<void**>(&unknown));
        if (result != kResultOk || unknown == nullptr) {
            std::printf("FAIL: createInstance returned 0x%08x\n", static_cast<unsigned>(result));
            exitCode = 1;
        } else {
            auto* component = static_cast<Vst::IComponent*>(unknown);
            result = component->initialize(nullptr);
            if (result != kResultOk) {
                std::printf("FAIL: IComponent::initialize returned 0x%08x\n",
                            static_cast<unsigned>(result));
                exitCode = 1;
            } else {
                std::printf("IComponent::initialize: ok\n");
                for (int t = 0; t < 2; ++t) {
                    const auto media = static_cast<Vst::MediaType>(t == 0 ? Vst::kAudio : Vst::kEvent);
                    for (int d = 0; d < 2; ++d) {
                        const auto dir = static_cast<Vst::BusDirection>(d == 0 ? Vst::kInput
                                                                               : Vst::kOutput);
                        const int buses = component->getBusCount(media, dir);
                        for (int b = 0; b < buses; ++b) {
                            Vst::BusInfo busInfo{};
                            if (component->getBusInfo(media, dir, b, busInfo) == kResultOk) {
                                std::printf("  bus %s/%s [%d]: '%ls' channels=%d\n",
                                            mediaTypeName(media), directionName(dir), b,
                                            busInfo.name, busInfo.channelCount);
                            }
                        }
                    }
                }
                component->terminate();
                std::printf("IComponent::terminate: ok\n");
            }
            unknown->release();
        }
    }

    factory->release();
    FreeLibrary(module);
    std::printf(exitCode == 0 ? "RESULT: plugin loads and instantiates\n"
                              : "RESULT: FAILED\n");
    return exitCode;
}
