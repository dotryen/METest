//
// Created by ryen on 12/11/24.
//

#include "SimpleRenderPipeline.h"

#include "../imgui/imgui_impl_sdlgpu3.h"
#include "render/RenderGlobals.h"
#include "render/Window.h"
#include "scene/sceneobj/SceneMesh.h"
#include "math/Transform.h"

namespace me::render {
    struct WorldBuffer {
        math::PackedMatrix4x4 view;
        math::PackedMatrix4x4 proj;
    };

    struct ObjectBuffer {
        math::PackedMatrix4x4 model;
    };

    SimpleRenderPipeline::SimpleRenderPipeline(asset::MaterialPtr material) {
        this->material = material;
        material->CreateGPUPipeline();
        pipeline = material->GetPipeline();
    }

    void SimpleRenderPipeline::Render(scene::SceneWorld* world) {
        auto list = world->GetSceneObjects();
        std::vector<scene::SceneMesh*> meshes;
        std::vector<asset::MeshTransfer> transfers;

        for (scene::SceneObject* obj : list) {
            scene::SceneMesh* meshObj = dynamic_cast<scene::SceneMesh*>(obj);
            if (meshObj == nullptr) continue;
            if (meshObj->mesh == nullptr) continue;

            meshes.push_back(meshObj);
            if (!meshObj->mesh->HasGPUBuffers()) {
                meshObj->mesh->CreateGPUBuffers();
                transfers.push_back(meshObj->mesh->StartTransfer());
            }
        }

        if (!transfers.empty()) {
            SDL_GPUCommandBuffer* transferCmd = SDL_AcquireGPUCommandBuffer(render::mainDevice);
            SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(transferCmd);

            for (asset::MeshTransfer& transfer : transfers) {
                for (asset::MeshTransferSection& section : transfer.sections) {
                    SDL_UploadToGPUBuffer(copyPass, &section.location, &section.region, false);
                }
            }
            SDL_EndGPUCopyPass(copyPass);

            for (asset::MeshTransfer& transfer : transfers) {
                // releasing before submitting looks wrong but it does it during a submit
                SDL_ReleaseGPUTransferBuffer(render::mainDevice, transfer.buffer);
            }

            SDL_SubmitGPUCommandBuffer(transferCmd);
        }

        SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(render::mainDevice);
        SDL_GPUTexture* swapchainTex;
        if (!SDL_AcquireGPUSwapchainTexture(commandBuffer, render::mainWindow->GetWindow(), &swapchainTex, NULL, NULL)) {
            return;
        }
        if (swapchainTex == nullptr) return;

        const SDL_GPUColorTargetInfo colorTargetInfo = {
            .texture = swapchainTex,
            .clear_color = (SDL_FColor){ 0.2f, 0.2f, 0.2f, 1.0f },
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE
        };

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, NULL);
        SDL_BindGPUGraphicsPipeline(renderPass, material->GetPipeline());

        WorldBuffer worldBuffer;
        world->GetCamera().GetTransform().Raw().ToSRT(true).StoreFloat4x4(worldBuffer.view);
        world->GetCamera().GetProjectionMatrix().StoreFloat4x4(worldBuffer.proj);
        SDL_PushGPUVertexUniformData(commandBuffer, 0, &worldBuffer, sizeof(WorldBuffer));

        for (scene::SceneMesh* mesh : meshes) {
            ObjectBuffer objectBuffer;
            mesh->GetTransform().Raw().ToTRS(true).StoreFloat4x4(objectBuffer.model);

            SDL_GPUBufferBinding vertexBinding = { mesh->mesh->GetGPUVertexBuffer(), 0 };
            SDL_GPUBufferBinding indexBinding = { mesh->mesh->GetGPUIndexBuffer(), 0 };
            SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
            SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
            SDL_PushGPUVertexUniformData(commandBuffer, 1, &objectBuffer, sizeof(ObjectBuffer));
            SDL_DrawGPUIndexedPrimitives(renderPass, mesh->mesh->GetIndexBuffer().size(), 1, 0, 0, 0);
        }
        SDL_EndGPURenderPass(renderPass);

        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commandBuffer, swapchainTex);
        SDL_SubmitGPUCommandBuffer(commandBuffer);
    }
}
