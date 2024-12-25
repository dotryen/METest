//
// Created by ryen on 12/4/24.
//

#define SDL_MAIN_USE_CALLBACKS

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include "imgui/imgui_impl_sdlgpu3.h"
#include <string>
#include <tiny_gltf.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <spdlog/spdlog.h>

#include "asset/Material.h"
#include "asset/Mesh.h"
#include "asset/Shader.h"
#include "render/MainWindow.h"
#include "scene/SceneSystem.h"
#include "scene/sceneobj/SceneMesh.h"
#include "FileSystem.h"
#include "haxe/HaxeSystem.h"
#include "log/LogSystem.h"
#include "render/RenderPipeline.h"
#include "render/SimpleRenderPipeline.h"
#include "time/TimeGlobal.h"

me::math::Vector3 vertices[8] =
{
    me::math::Vector3(-1, -1, -1),
    me::math::Vector3(1, -1, -1),
    me::math::Vector3(1, 1, -1),
    me::math::Vector3(-1, 1, -1),
    me::math::Vector3(-1, -1, 1),
    me::math::Vector3(1, -1, 1),
    me::math::Vector3(1, 1, 1),
    me::math::Vector3(-1, 1, 1)
};

uint16_t indices[6 * 6] =
{
    0, 1, 3, 3, 1, 2,
    1, 5, 2, 2, 5, 6,
    5, 4, 6, 6, 4, 7,
    4, 0, 7, 7, 0, 3,
    3, 2, 7, 7, 2, 6,
    4, 5, 0, 0, 5, 1
};

struct HaxeStack {
    int padding;
};

struct AppContext {
    me::scene::ScenePtr scene;
    float imguiFloat;
    me::math::Vector3 imguiFloat3;
    me::scene::SceneObject* exampleObject;

    me::asset::MeshPtr cubeMesh;
    me::scene::SceneMesh* cubeMeshObject;

    me::asset::MeshPtr gltfMesh;
    me::scene::SceneMesh* gltfMeshObject;

    me::asset::MaterialPtr material;
    me::asset::ShaderPtr vertexShader;
    me::asset::ShaderPtr fragmentShader;
    std::unique_ptr<me::render::RenderPipeline> renderPipeline;

    HaxeStack haxeStack;
    std::unique_ptr<me::haxe::HaxeSystem> haxeSystem;
    me::haxe::HaxeType* otherTestType;
    me::haxe::HaxeObject* otherTestObject;
    vdynamic* haxeTestUpdate;
    vclosure updateFunction;

    bool shouldQuit;
};

me::asset::ShaderPtr LoadShader(const std::string& path, me::asset::ShaderType type) {
    vfspp::IFilePtr file = me::fs::OpenFile(path);
    if (file && file->IsOpened()) {
        SDL_Log(("Opened shader: " + path).c_str());
        size_t size = file->Size();
        unsigned char* buffer = new unsigned char[size + 1];
        memset(buffer, 0, size + 1);
        file->Read(buffer, size);
        file->Close();

        me::asset::ShaderPtr shader = std::make_shared<me::asset::Shader>(false, type, reinterpret_cast<char*>(buffer), size);
        return shader;
    }
    SDL_Log(("Failed to load shader: " + path).c_str());
    return nullptr;
}

template <typename T>
std::vector<T> ReadAccessor(tinygltf::Model& model, int id) {
    tinygltf::Accessor accessor = model.accessors[id];

    std::vector<T> vector(accessor.count);

    tinygltf::BufferView view = model.bufferViews[accessor.bufferView];
    tinygltf::Buffer buffer = model.buffers[view.buffer];

    const size_t stride = view.byteStride + sizeof(T);
    unsigned char* dataPointer = buffer.data.data() + view.byteOffset;
    for (size_t i = 0; i < accessor.count; i++) {
        vector[i] = *reinterpret_cast<T*>(dataPointer);
        dataPointer += stride;
    }

    return std::move(vector);
}

me::asset::MeshPtr LoadMesh(const std::string& path) {
    vfspp::IFilePtr file = me::fs::OpenFile(path);
    if (file && file->IsOpened()) {
        size_t fileSize = file->Size();
        unsigned char buffer[fileSize];
        file->Read(buffer, fileSize);
        file->Close();

        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        if (loader.LoadBinaryFromMemory(&gltfModel, &err, &warn, buffer, fileSize)) {
            // assume mesh zero
            int posAccessorID = gltfModel.meshes[0].primitives[0].attributes["POSITION"];
            std::vector<me::math::Vector3> vertexBuffer = ReadAccessor<me::math::Vector3>(gltfModel, posAccessorID);

            // assume there are indices
            int indexAccessorID = gltfModel.meshes[0].primitives[0].indices;
            std::vector<uint16_t> indexBuffer = ReadAccessor<uint16_t>(gltfModel, indexAccessorID);

            return std::make_shared<me::asset::Mesh>(vertexBuffer, indexBuffer);
        }
    }
    return nullptr;
}

SDL_AppResult SDL_Fail() {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL Error: %s", SDL_GetError());
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return SDL_Fail();
    }

    me::log::Initialize();
    spdlog::info("hi!!!1 hi!!!!!!!");

    me::time::Initialize();
    me::fs::Initialize();
    me::window::OpenWindow("MANIFOLDEngine", { 1280, 720 });

    auto ctx = new AppContext();
    ctx->shouldQuit = false;

    // load shaders
    ctx->vertexShader = LoadShader("/shaders/vertex.hlsl", me::asset::ShaderType::Vertex);
    ctx->fragmentShader = LoadShader("/shaders/fragment.hlsl", me::asset::ShaderType::Fragment);
    ctx->material = std::make_shared<me::asset::Material>(ctx->vertexShader, ctx->fragmentShader);
    ctx->renderPipeline = std::make_unique<me::render::SimpleRenderPipeline>(ctx->material);

    ctx->scene = std::make_shared<me::scene::Scene>();

    // set camera pos
    ctx->scene->GetSceneWorld().GetCamera().GetTransform().SetPosition({ 0.f, 0.f, -10.f });

    // make blank object
    ctx->exampleObject = new me::scene::SceneObject("blank");
    ctx->scene->GetSceneWorld().AddObject(ctx->exampleObject);

    // make mesh and object
    ctx->cubeMesh = std::make_shared<me::asset::Mesh>(vertices, 8, indices, 36);
    ctx->cubeMeshObject = new me::scene::SceneMesh("cube");
    ctx->cubeMeshObject->mesh = ctx->cubeMesh;
    ctx->cubeMeshObject->material = ctx->material;
    ctx->scene->GetSceneWorld().AddObject(ctx->cubeMeshObject);

    ctx->gltfMesh = LoadMesh("/alitrophy.glb");
    ctx->gltfMeshObject = new me::scene::SceneMesh("gltf");
    ctx->gltfMeshObject->GetTransform().SetPosition({ 5.f, 0.f, 0.f });
    ctx->gltfMeshObject->mesh = ctx->gltfMesh;
    ctx->gltfMeshObject->material = ctx->material;
    ctx->scene->GetSceneWorld().AddObject(ctx->gltfMeshObject);

    hl_global_init();
    hl_sys_init(nullptr, 0, nullptr);
    hl_register_thread(&ctx->haxeStack);
    ctx->haxeSystem = std::make_unique<me::haxe::HaxeSystem>("/code.hl");
    ctx->haxeSystem->Load();

    ctx->otherTestType = ctx->haxeSystem->GetType(u"OtherUpdate");
    ctx->otherTestObject = ctx->otherTestType->CreateInstance();

    std::u16string str = u"TestUpdate";
    std::vector<hl_type*> types;
    for (int i = 0; i < ctx->haxeSystem->GetModule()->code->ntypes; i++) {
        auto type = &ctx->haxeSystem->GetModule()->code->types[i];
        if (type->kind == hl_type_kind::HOBJ) {
            std::u16string typeName = std::u16string(type->obj->name, ustrlen(type->obj->name));
            if (typeName == str) {
                types.push_back(type);
            }
        }
    }

    auto module = ctx->haxeSystem->GetModule();
    auto proto = &types[0]->obj->proto[0];
    ctx->haxeTestUpdate = hl_alloc_obj(types[0]);
    ctx->updateFunction.t = module->code->functions[module->functions_indexes[proto->findex]].type;
    ctx->updateFunction.fun = module->functions_ptrs[proto->findex];
    ctx->updateFunction.hasValue = false;
    // ctx->updateFunction.value = ctx->haxeTestUpdate;
    // ctx->updateFunction.hasValue = true;

    me::scene::Initialize();
    me::scene::AddScene(ctx->scene);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL3_InitForVulkan(me::window::window);
    ImGui_ImplSDLGPU3_Init(me::window::device, me::window::window);

    *appstate = ctx;
    SDL_Log("initialized");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* ctx = static_cast<AppContext*>(appstate);

    ImGui_ImplSDL3_ProcessEvent(event);

    if (event->type == SDL_EVENT_QUIT) {
        ctx->shouldQuit = true;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* ctx = static_cast<AppContext*>(appstate);

    me::time::Update();

    ctx->otherTestObject->CallMethod(u"Update", {});

    // raw method of calling
    bool isExcept = false;
    vdynamic* args[1] = { ctx->haxeTestUpdate };
    // hl_dyn_call_safe(&ctx->updateFunction, args, 1, &isExcept);
    if (isExcept) {
        spdlog::error("Haxe update failed");
    }

    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Console");
    ImGui::Text(me::log::stream.str().c_str());
    ImGui::End();

    ImGui::Begin("Haxe Test");
    if (ImGui::Button("Call Entrypoint")) {
        ctx->haxeSystem->CallEntryPoint();
    }
    ImGui::End();

    ImGui::Begin("Basic Debug Panel");

    ImGui::Text(fmt::format("Game Time: {:.2}", me::time::gameTime.GetElapsed()).c_str());
    ImGui::Text(fmt::format("Game Time Delta: {:.4}", me::time::gameTime.GetDelta()).c_str());

    if (ImGui::CollapsingHeader("Active Scene World")) {
        if (ImGui::TreeNode("Camera")) {
            auto& camera = ctx->scene->GetSceneWorld().GetCamera();
            auto& cameraTransform = camera.GetTransform();
            auto& raw = cameraTransform.Raw();
            ctx->imguiFloat = camera.GetFOV();
            ctx->imguiFloat3 = cameraTransform.GetAngles();

            ImGui::DragFloat3("Position", reinterpret_cast<float*>(&raw.position), 0.1f);
            if (ImGui::DragFloat3("Rotation", reinterpret_cast<float*>(&ctx->imguiFloat3), 2.f)) {
                cameraTransform.SetAngles(ctx->imguiFloat3);
            }

            if (ImGui::SliderFloat("Field of View", &ctx->imguiFloat, 1.0f, 179.0f)) {
                camera.SetFOV(ctx->imguiFloat);
                // SDL_Log(std::format("FOV = {}", ctx->cameraFOVBuffer).c_str());
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Objects")) {
            for (auto obj : ctx->scene->GetSceneWorld().GetSceneObjects()) {
                if (ImGui::TreeNode(std::format("SceneObject id {}", obj->GetName()).c_str())) {
                    auto& transform = obj->GetTransform();
                    auto& raw = transform.Raw();
                    ctx->imguiFloat3 = transform.GetAngles();

                    ImGui::Text("Transform");
                    ImGui::DragFloat3("Position", reinterpret_cast<float*>(&raw.position), 0.1f);
                    if (ImGui::DragFloat3("Rotation", reinterpret_cast<float*>(&ctx->imguiFloat3), 2.f)) {
                        transform.SetAngles(ctx->imguiFloat3);
                    }
                    ImGui::DragFloat3("Scale", reinterpret_cast<float*>(&raw.scale), 0.1f);
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
    }
    ImGui::End();

    ImGui::Render();
    ctx->renderPipeline->Render(&ctx->scene->GetSceneWorld());

    return ctx->shouldQuit ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* ctx = static_cast<AppContext*>(appstate);

    if (ctx) {
        delete ctx;
    }

    hl_global_free();

    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();
    me::window::CloseWindow();
    SDL_Log("Quitted");
}
