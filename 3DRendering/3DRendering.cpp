#include "Engine.h"
#include "File.h"
#include "Global.h"
#include "RenderingInterface.h"
#include "Window.h"
#include "glm/glm.hpp"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/mesh.h"
#include "ImGuiInterface.h"
#include "imgui_internal.h"

using namespace std;

struct AppGlobals
{
    Window* mWindow;
    Surface mSurface;
    SwapChain mSwap;
	
} Globals;

SwapChain CreateSwap(Window* Wnd, Surface Surf)
{
    int32_t Width, Height;
    GetWindowSize(Wnd, Width, Height);
    return GRenderAPI->CreateSwapChain(Surf, Width, Height);
}

struct FinalPassVertex
{
    glm::vec3 mPosition;
};

struct MeshVertex
{
    glm::vec3 Position;
};

struct Mesh
{
    VertexBuffer mBuffer;
    uint32_t mVertexCount;
    uint32_t mIndexCount;
};

struct Scene
{
    std::vector<Mesh> mMeshes;

};

Pipeline CreateFinalPassPipeline()
{
    ResourceLayoutCreateInfo RlCreateInfo{};
    ResourceLayout Layout = GRenderAPI->CreateResourceLayout(&RlCreateInfo);

    ShaderCreateInfo ShaderCreateInfo{};
    ShaderCreateInfo.VertexShaderVirtual = "/Shaders/FinalPass.vert";
    ShaderCreateInfo.FragmentShaderVirtual = "/Shaders/FinalPass.frag";

    VertexAttribute Attribs[] = {
        {VertexAttributeFormat::Float3, offsetof(FinalPassVertex, mPosition)}
    };
    PipelineCreateInfo CreateInfo{};
    CreateInfo.VertexAttributeCount = std::size(Attribs);
    CreateInfo.VertexAttributes = Attribs;
    CreateInfo.VertexBufferStride = sizeof(FinalPassVertex);
    CreateInfo.Shader = GRenderAPI->CreateShader(&ShaderCreateInfo);
    CreateInfo.CompatibleSwapChain = Globals.mSwap;
    CreateInfo.Layout = Layout;

    PipelineBlendSettings BlendSettings;
    BlendSettings.bBlendingEnabled = false;
    CreateInfo.BlendSettingCount = 1;
    CreateInfo.BlendSettings = &BlendSettings;

    return GRenderAPI->CreatePipeline(&CreateInfo);
}

template<typename VertexType>
Mesh CreateScreenSpaceMesh(VertexType DefaultVal)
{
    Mesh NewMesh{};

    VertexBufferCreateInfo VBOCreate{};
    VBOCreate.bCreateIndexBuffer = true;
    VBOCreate.Usage = BufferUsage::Static;
    VBOCreate.VertexBufferSize = sizeof(VertexType) * 4;
    VBOCreate.IndexBufferSize = sizeof(uint32_t) * 6;
    NewMesh.mBuffer = GRenderAPI->CreateVertexBuffer(&VBOCreate);

    VertexType ScreenSpace[4] = { DefaultVal, DefaultVal, DefaultVal, DefaultVal };
    uint32_t IndexBuffer[] = {
        0, 2, 1,
        0, 3, 2
    };
    if (std::is_same_v<decltype(DefaultVal.mPosition), glm::vec3>)
    {
        ScreenSpace[0].mPosition = { -1.0f, -1.0f, 0.0f };
        ScreenSpace[1].mPosition = { -1.0f, 1.0f, 0.0f };
        ScreenSpace[2].mPosition = { 1.0f, 1.0f, 0.0f };
        ScreenSpace[3].mPosition = { 1.0f, -1.0f, 0.0f };
    }

    GRenderAPI->UploadVertexBufferData(NewMesh.mBuffer, ScreenSpace, sizeof(ScreenSpace));
    GRenderAPI->UploadIndexBufferData(NewMesh.mBuffer, IndexBuffer, sizeof(IndexBuffer));

    NewMesh.mVertexCount = 4;
    NewMesh.mIndexCount = 6;

    return NewMesh;
}

void ProcessNode(const aiScene* Scene, aiNode* Node)
{

}

Mesh BuildMesh(const aiMesh* AIMesh)
{
    Mesh NewMesh;

    std::vector<MeshVertex> Verts;
    for(uint32_t VertIndex = 0; VertIndex < AIMesh->mNumVertices; VertIndex++)
    {
        aiVector3D Pos = AIMesh->mVertices[VertIndex];

        MeshVertex Vert;
        Vert.Position = {Pos.x, Pos.y, Pos.z};
        Verts.push_back(Vert);
    }

    std::vector<uint32_t> Indicies;
    for(uint32_t VertIndex = 0; VertIndex < AIMesh->mNumFaces; VertIndex++)
    {
        if(AIMesh->mFaces[VertIndex].mNumIndices == 3)
        {
            Indicies.push_back(AIMesh->mFaces[VertIndex].mIndices[0]);
            Indicies.push_back(AIMesh->mFaces[VertIndex].mIndices[1]);
            Indicies.push_back(AIMesh->mFaces[VertIndex].mIndices[2]);
        }
    }

    VertexBufferCreateInfo CreateInfo{};
    CreateInfo.bCreateIndexBuffer = true;
    CreateInfo.VertexBufferSize = Verts.size() * sizeof(MeshVertex);
    CreateInfo.IndexBufferSize = Indicies.size() * sizeof(uint32_t);
    NewMesh.mBuffer = GRenderAPI->CreateVertexBuffer(&CreateInfo);

    GRenderAPI->UploadVertexBufferData(NewMesh.mBuffer, Verts.data(), CreateInfo.VertexBufferSize);
    GRenderAPI->UploadIndexBufferData(NewMesh.mBuffer, Indicies.data(), CreateInfo.IndexBufferSize);

    NewMesh.mVertexCount = Verts.size();
    NewMesh.mIndexCount = Indicies.size();

    return NewMesh;
}

Scene ImportScene(std::string File)
{
    Assimp::Importer Importer;
    const aiScene* AIScene = Importer.ReadFile(File,
        aiProcess_CalcTangentSpace       |
        aiProcess_Triangulate            |
        aiProcess_JoinIdenticalVertices  |
        aiProcess_SortByPType
    );

    Scene NewScene;

    // Build meshes
    for(uint32_t MeshIndex = 0; MeshIndex < AIScene->mNumMeshes; MeshIndex++)
    {
        aiMesh* Mesh = AIScene->mMeshes[MeshIndex];
        NewScene.mMeshes.push_back(BuildMesh(Mesh));
    }

    if(AIScene->mRootNode)
        ProcessNode(AIScene, AIScene->mRootNode);

    return NewScene;
}

void DrawImGui()
{
    static bool WindowOpen = true;
    if(ImGui::Begin("Test Window", &WindowOpen))
    {
	    
    }
    ImGui::End();
}

int main()
{
    std::string ExePath;
    GetExePath(&ExePath);
    std::filesystem::path LogsPath = std::filesystem::path(ExePath).parent_path() / "Log.txt";

    auto FileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(LogsPath.string(), true);
    FileSink->set_level(spdlog::level::trace);

    auto ConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    ConsoleSink->set_level(spdlog::level::trace);

    GLog = new spdlog::logger("3D Renderer", spdlog::sinks_init_list{ FileSink, ConsoleSink });
    GLog->set_level(spdlog::level::trace);

    // Initialize windowing
    InitWindowing();

    // Mount shaders
    auto RootInstall = std::filesystem::path(ExePath).parent_path();
    auto ShadersRoot = RootInstall / "Shaders";
    auto ContentRoot = RootInstall / "Content";
    MountDirectory(ShadersRoot.string().c_str(), "Shaders");

    uint32_t FrameWidth = 16 * 50, FrameHeight = 9 * 50;

    WindowCreationOptions WindowOptions;
    WindowOptions.Title = "NewEngine";
    WindowOptions.Width = FrameWidth;
    WindowOptions.Height = FrameHeight;
    WindowOptions.bUseDecorations = true;

    Globals.mWindow = MakeWindow(WindowOptions);
    Globals.mWindow->OnWindowResize = [&](uint32_t NewWidth, uint32_t NewHeight)
    {
        FrameWidth = NewWidth;
        FrameHeight = NewHeight;
        GRenderAPI->RecreateSwapChain(Globals.mSwap, Globals.mSurface, NewWidth, NewHeight);
    };

    if (!InitializeRenderAPI(DynamicRenderAPI::Vulkan))
    {
        return 1;
    }

    // TODO: fix this part of the API
    Globals.mSurface = GRenderAPI->CreateSurfaceForWindow(Globals.mWindow); // Create main surface for window
    GRenderAPI->InitializeForSurface(Globals.mSurface); // Finish render API initialization
    Globals.mSwap = CreateSwap(Globals.mWindow, Globals.mSurface); // Create Vulkan swap chain for window

    ImGuiContext* Context = InitImGui(Globals.mWindow, Globals.mSwap, { true, true, true });
    ImGui::SetCurrentContext(Context);

    CommandBuffer FinalPass = GRenderAPI->CreateSwapChainCommandBuffer(Globals.mSwap, true);
    Pipeline FinalPassPipeline = CreateFinalPassPipeline();

    Mesh ScreenSpaceMesh = CreateScreenSpaceMesh(FinalPassVertex{});

    // Load the scene
    auto SceneFile = ContentRoot / "Sponza-master" / "sponza.obj";
    Scene NewScene = ImportScene(SceneFile.string());

    while (!ShouldWindowClose(Globals.mWindow))
    {
        PollWindowEvents();

        BeginImGuiFrame();
        {
            DrawImGui();
        }
        EndImGuiFrame();

        uint32_t SwapWidth, SwapHeight;
        GRenderAPI->GetSwapChainSize(Globals.mSwap, SwapWidth, SwapHeight);

        GRenderAPI->BeginFrame(Globals.mSwap, Globals.mSurface, FrameWidth, FrameHeight);
        {
            GRenderAPI->Reset(FinalPass);
            GRenderAPI->Begin(FinalPass);
            {
                RenderGraphInfo CompositeInfo = {
                    1,
                    ClearValue{ClearType::Float, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)}
                };

                GRenderAPI->BeginRenderGraph(FinalPass, Globals.mSwap, CompositeInfo);
                {

                    GRenderAPI->BindPipeline(FinalPass, FinalPassPipeline);
                    GRenderAPI->SetViewport(FinalPass, 0, 0, static_cast<uint32_t>(SwapWidth), static_cast<uint32_t>(SwapHeight));
                    GRenderAPI->SetScissor(FinalPass, 0, 0, static_cast<uint32_t>(SwapWidth), static_cast<uint32_t>(SwapHeight));
                    GRenderAPI->DrawVertexBufferIndexed(FinalPass, ScreenSpaceMesh.mBuffer, ScreenSpaceMesh.mIndexCount);
                }
                GRenderAPI->EndRenderGraph(FinalPass);

                // ImGui

                RecordImGuiDrawCmds(FinalPass);
            }
            GRenderAPI->End(FinalPass);

            GRenderAPI->SubmitSwapCommandBuffer(Globals.mSwap, FinalPass);
        }
        GRenderAPI->EndFrame(Globals.mSwap, Globals.mSurface, FrameWidth, FrameHeight);

        UpdateImGuiViewports();
    }

    GRenderAPI->DestroySwapChain(Globals.mSwap);
    GRenderAPI->DestroySurface(Globals.mSurface);
    DestroyWindow(Globals.mWindow);
}
