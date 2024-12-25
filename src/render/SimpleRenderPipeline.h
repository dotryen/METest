//
// Created by ryen on 12/11/24.
//

#ifndef SIMPLERENDERPIPELINE_H
#define SIMPLERENDERPIPELINE_H

#include "render/RenderPipeline.h"
#include "asset/Material.h"

namespace me::render {
    class SimpleRenderPipeline : public RenderPipeline {
        private:
        asset::MaterialPtr material;
        SDL_GPUGraphicsPipeline* pipeline;

        public:
        SimpleRenderPipeline(asset::MaterialPtr material);

        void Render(scene::SceneWorld* world) override;
    };
}

#endif //SIMPLERENDERPIPELINE_H
