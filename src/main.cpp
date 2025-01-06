//
// Created by ryen on 12/4/24.
//

#define SDL_MAIN_USE_CALLBACKS

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include "imgui/imgui_impl_sdlgpu3.h"
#include <string>
#include <tiny_gltf.h>
#include <haxe/HaxeGlobals.h>
#include <render/RenderGlobals.h>
#include <scene/SceneGlobals.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <spdlog/spdlog.h>
#include <Jolt/Jolt.h>

#include "MECore.h"
#include "asset/Material.h"
#include "asset/Mesh.h"
#include "asset/Shader.h"
#include "scene/SceneSystem.h"
#include "scene/sceneobj/SceneMesh.h"
#include "fs/FileSystem.h"
#include "haxe/HaxeSystem.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "log/LogSystem.h"
#include "render/RenderPipeline.h"
#include "render/SimpleRenderPipeline.h"
#include "time/TimeGlobal.h"
#include "render/Window.h"

me::math::PackedVector3 vertices[8] =
{
    me::math::PackedVector3(me::math::Vector3(-1, -1, -1)),
    me::math::PackedVector3(me::math::Vector3(1, -1, -1)),
    me::math::PackedVector3(me::math::Vector3(1, 1, -1)),
    me::math::PackedVector3(me::math::Vector3(-1, 1, -1)),
    me::math::PackedVector3(me::math::Vector3(-1, -1, 1)),
    me::math::PackedVector3(me::math::Vector3(1, -1, 1)),
    me::math::PackedVector3(me::math::Vector3(1, 1, 1)),
    me::math::PackedVector3(me::math::Vector3(-1, 1, 1))
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

struct AppContext {
    float imguiFloat;
    me::math::Vector3 imguiFloat3;

    me::scene::ScenePtr scene;
    me::scene::SceneObject* exampleObject;

    me::asset::MeshPtr cubeMesh;
    me::scene::SceneMesh* cubeMeshObject;

    JPH::BoxShapeSettings floorShapeSettings { JPH::RVec3(100.0f, 1.0f, 100.0f) };
    JPH::Body* floor;

    JPH::BodyID cubeId;
    me::scene::SceneMesh* physicsCubeObject;

    me::asset::MeshPtr gltfMesh;
    me::scene::SceneMesh* gltfMeshObject;

    me::asset::MaterialPtr material;
    me::asset::ShaderPtr vertexShader;
    me::asset::ShaderPtr fragmentShader;
    std::unique_ptr<me::render::RenderPipeline> renderPipeline;

    me::haxe::HaxeType* otherTestType;
    me::haxe::HaxeObject* otherTestObject;
    me::haxe::HaxeObject* sinUpdate;

    me::scene::GameObject* gameObject;

    bool shouldQuit;
};

inline me::math::Vector3 Util_Convert(const JPH::Vec3& vec) {
    return { vec.GetX(), vec.GetY(), vec.GetZ() };
}

me::asset::ShaderPtr LoadShader(const std::string& path, me::asset::ShaderType type) {
    vfspp::IFilePtr file = me::fs::OpenFile(path);
    if (file && file->IsOpened()) {
        spdlog::info("Opened shader: " + path);
        size_t size = file->Size();
        unsigned char* buffer = new unsigned char[size + 1];
        memset(buffer, 0, size + 1);
        file->Read(buffer, size);
        file->Close();

        me::asset::ShaderPtr shader = std::make_shared<me::asset::Shader>(false, type, reinterpret_cast<char*>(buffer), size);
        return shader;
    }
    spdlog::info("Failed to load shader: " + path);
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
            std::vector<me::math::PackedVector3> vertexBuffer = ReadAccessor<me::math::PackedVector3>(gltfModel, posAccessorID);

            // assume there are indices
            int indexAccessorID = gltfModel.meshes[0].primitives[0].indices;
            std::vector<uint16_t> indexBuffer = ReadAccessor<uint16_t>(gltfModel, indexAccessorID);

            return std::make_shared<me::asset::Mesh>(vertexBuffer, indexBuffer);
        }
    }
    return nullptr;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!me::Initialize(me::MESystems::All)) {
        spdlog::critical("Failed to initialize MANIFOLDEngine");
        return SDL_APP_FAILURE;
    }
    me::render::CreateMainWindow("MECore Test", { 1280, 720 });
    // me::haxe::CreateMainSystem("/code.hl");

    auto ctx = new AppContext();
    ctx->shouldQuit = false;

    ctx->otherTestType = me::haxe::mainSystem->GetType(u"OtherUpdate");
    ctx->otherTestObject = ctx->otherTestType->CreateInstance();

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

    ctx->physicsCubeObject = new me::scene::SceneMesh("physics cube");
    ctx->physicsCubeObject->mesh = ctx->cubeMesh;
    ctx->physicsCubeObject->material = ctx->material;
    ctx->scene->GetSceneWorld().AddObject(ctx->physicsCubeObject);

    ctx->gltfMesh = LoadMesh("/alitrophy.glb");
    ctx->gltfMeshObject = new me::scene::SceneMesh("gltf");
    ctx->gltfMeshObject->GetTransform().SetPosition({ 5.f, 0.f, 0.f });
    ctx->gltfMeshObject->mesh = ctx->gltfMesh;
    ctx->gltfMeshObject->material = ctx->material;
    ctx->scene->GetSceneWorld().AddObject(ctx->gltfMeshObject);

    ctx->gameObject = new me::scene::GameObject("test object");
    ctx->scene->GetGameWorld().AddObject(ctx->gameObject);

    me::scene::mainSystem->AddScene(ctx->scene);

    // add physics cube
    auto& bodyInterface = ctx->scene->GetPhysicsWorld().GetInterface();

    ctx->floorShapeSettings.SetEmbedded();
    JPH::ShapeSettings::ShapeResult floorShapeResult = ctx->floorShapeSettings.Create();
    JPH::ShapeRefC floorShape = floorShapeResult.Get();

    JPH::BodyCreationSettings floorCreateSettings(floorShape, JPH::RVec3(0.0f, -1.0f, 0.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Static, static_cast<JPH::ObjectLayer>(me::physics::PhysicsLayer::Static));

    ctx->floor = bodyInterface.CreateBody(floorCreateSettings);
    bodyInterface.AddBody(ctx->floor->GetID(), JPH::EActivation::DontActivate);

    JPH::BodyCreationSettings cubeSettings(new JPH::BoxShape(JPH::RVec3(1, 1, 1)), JPH::RVec3(0, 10, 0), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, static_cast<JPH::ObjectLayer>(me::physics::PhysicsLayer::Dynamic));
    ctx->cubeId = bodyInterface.CreateAndAddBody(cubeSettings, JPH::EActivation::Activate);

    bodyInterface.SetLinearVelocity(ctx->cubeId, JPH::Vec3(0, 5, 0));

    // make scene transform
    auto ptr = vdynamic();
    ptr.t = &hlt_i64;
    ptr.v.i64 = reinterpret_cast<int64>(&ctx->cubeMeshObject->GetTransform());
    std::vector<vdynamic*> args = { &ptr };
    auto transform = me::haxe::mainSystem->GetType(u"me.scene.SceneTransform")->CreateInstance();
    transform->CallVirtualMethod(u"ME_Initialize", args);

    auto result = me::haxe::mainSystem->GetTypesWithName(u"SceneSystem");

    ctx->sinUpdate = me::haxe::mainSystem->GetType(u"SinUpdate")->CreateInstance();
    ctx->sinUpdate->SetPtr("target", transform);

    // INIT IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL3_InitForVulkan(me::render::mainWindow->GetWindow());
    ImGui_ImplSDLGPU3_Init(me::render::mainDevice, me::render::mainWindow->GetWindow());

    *appstate = ctx;
    spdlog::info("Initialized");

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

    me::scene::mainSystem->Update();
    me::time::Update();

    auto& bodyInterface = ctx->scene->GetPhysicsWorld().GetInterface();

    me::math::Vector3 pos = Util_Convert(bodyInterface.GetPosition(ctx->cubeId));
    ctx->physicsCubeObject->GetTransform().SetPosition(pos);

    // ctx->otherTestObject->CallMethod(u"Update", {});
    ctx->sinUpdate->CallMethod(u"Update", {});

    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Console");
    ImGui::Text(me::log::stream.str().c_str());
    ImGui::End();

    ImGui::Begin("Haxe Test");
    // if (ImGui::Button("Call Entrypoint")) {
    //     me::haxe::mainSystem->CallEntryPoint();
    // }
    if (ImGui::Button("Check Position")) {
        ctx->sinUpdate->CallMethod(u"PrintPos", {});
    }
    if (ImGui::Button("Check Time")) {
        ctx->sinUpdate->CallMethod(u"PrintTime", {});
    }
    if (ImGui::Button("Check Cache")) {
        ctx->sinUpdate->CallMethod(u"PrintCache", {});
    }
    if (ImGui::Button("Do Math")) {
        ctx->sinUpdate->CallMethod(u"DoMath", {});
    }
    if (ImGui::Button("Get Scene Count")) {
        ctx->sinUpdate->CallMethod(u"GetSceneCount", {});
    }
    ImGui::End();

    ImGui::Begin("Basic Debug Panel");

    ImGui::Text(fmt::format("Game Time: {:0.2}", me::time::mainGame.GetElapsed()).c_str());
    ImGui::Text(fmt::format("Game Time Delta: {:.4}", me::time::mainGame.GetDelta()).c_str());

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

    if (ImGui::CollapsingHeader("Active Game World")) {
        for (auto* obj : ctx->scene->GetGameWorld().GetObjects()) {
            if (ImGui::TreeNode(fmt::format("GameObject id {}", obj->GetName()).c_str())) {
                auto& transform = obj->GetTransform();
                auto& raw = transform.LocalRaw();
                ImGui::Text("Transform");
                ImGui::DragFloat3("Position", reinterpret_cast<float*>(&raw.position), 0.1f);

                ImGui::TreePop();
            }
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

    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();

    me::Shutdown();
}
