#include <vector>
#include "../plugin.h"

namespace pickframes {

const std::vector<std::string> features = {
    "pickframes"
};

class PickFrames {
private:
    VSNodeRef* node;
    VSVideoInfo *vi;
    
    std::vector<int> indices;

public:
    PickFrames(VSNodeRef* node, VSVideoInfo *vi, std::vector<int>&& indices)
        : node(node), vi(vi), indices(std::move(indices)) {}

    ~PickFrames() {
        delete vi;
    }

    const VSFrameRef* getFrame(int n, int activationReason, VSFrameContext* frameCtx, const VSAPI* vsapi) const {
        if (n < 0 || n >= static_cast<int>(indices.size())) {
            return nullptr;
        }
        
        if (activationReason == arInitial) {
            vsapi->requestFrameFilter(indices[n], node, frameCtx);
        } else if (activationReason == arAllFramesReady) {
            return vsapi->getFrameFilter(indices[n], node, frameCtx);
        }
        return nullptr;
    }

    static const VSFrameRef* VS_CC staticGetFrame(int n, int activationReason, void** instanceData,
        [[maybe_unused]] void** frameData, VSFrameContext* frameCtx, [[maybe_unused]] VSCore* core, const VSAPI* vsapi) {
        auto *instance = static_cast<PickFrames*>(*instanceData);
        return instance->getFrame(n, activationReason, frameCtx, vsapi);
    }

    static void VS_CC staticFree(void* instanceData, [[maybe_unused]] VSCore* core, const VSAPI* vsapi) {
        auto *instance = static_cast<PickFrames*>(instanceData);
        vsapi->freeNode(instance->node);
        delete instance;
    }

    static void VS_CC staticInit(VSMap *in, VSMap *out, void** instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
        auto *instance = static_cast<PickFrames*>(*instanceData);
        vsapi->setVideoInfo(instance->vi, 1, node);
    }

    static void VS_CC create(const VSMap* in, VSMap* out, [[maybe_unused]] void* userData,
        VSCore* core, const VSAPI* vsapi) {
        
        int err = 0;
        VSNodeRef* node = vsapi->propGetNode(in, "clip", 0, &err);
        
        const VSVideoInfo* vi_src = vsapi->getVideoInfo(node);

        int numIndices = vsapi->propNumElements(in, "indices");
        if (numIndices <= 0) {
            vsapi->freeNode(node);
            vsapi->setError(out, "PickFrames: indices array must not be empty");
            return;
        }

        std::vector<int> indices;
        indices.reserve(numIndices);

        for (int i = 0; i < numIndices; i++) {
            int frameIndex = vsapi->propGetInt(in, "indices", i, &err);
            if (frameIndex < 0 || frameIndex >= vi_src->numFrames) {
                vsapi->freeNode(node);
                vsapi->setError(out, "PickFrames: frame index out of range");
                return;
            }
            indices.push_back(frameIndex);
        }

        VSVideoInfo* vi_out = new VSVideoInfo(*vi_src);
        vi_out->numFrames = numIndices;

        auto* instance = new PickFrames(node, vi_out, std::move(indices));

        vsapi->createFilter(in, out, "PickFrames", staticInit, staticGetFrame,
            staticFree, fmParallel, 0, instance, core);
    }
};

} // namespace pickframes

static void VS_CC versionCreate(const VSMap *in, VSMap *out, void *user_data, VSCore *core, const VSAPI *vsapi) {
    for (const auto &f : pickframes::features)
        vsapi->propSetData(out, "pickframes_features", f.c_str(), -1, paAppend);
}

#ifndef STANDALONE_PICKFRAMES
void VS_CC pickframeslInitialize(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    registerVersionFunc(versionCreate);
#else
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("info.akarin.plugin", "akarin2", "PickFrames plugin", VAPOURSYNTH_API_VERSION, 1, plugin);
#endif
    registerFunc("PickFrames",
        "clip:clip;indices:int[];"
        , pickframes::PickFrames::create, nullptr, plugin);
}