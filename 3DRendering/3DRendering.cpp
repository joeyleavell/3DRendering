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
#include "glm/gtc/matrix_transform.hpp"
#include "imgui_internal.h"
#include <cmath>

using namespace std;

constexpr double PI = 3.14159265;
constexpr float DegreesToRadians(float Deg)
{
    return Deg * (PI / 180);
}

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

struct SceneVertexUniforms
{
    glm::mat4 ViewProjectionMatrix;
};

struct MeshVertex
{
    glm::vec3 mPosition;
    glm::vec3 mNormal;
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

struct Camera
{
    glm::vec3 Position;
    glm::vec3 Rotation;

    float NearClip = 0.01f;
    float FarClip = 5000.0f;
    float Aspect = 0.0f;
    float FieldOfView = 90.0f;
};

struct MetricCategory
{
    double LastTime = 0.0;
    double MinTime = std::numeric_limits<double>::max();
    double MaxTime = 0.0;
    double AvgTime = 0.0;

    // For tracking avg time
    double SumTime = 0.0;
    uint32_t NumPublishes = 0;
};

struct Metrics
{
    std::unordered_map<std::string, MetricCategory> mMetrics;

    void PublishTime(std::string Category, double Time)
    {
        if (!mMetrics.contains(Category))
            mMetrics.emplace(Category, MetricCategory{});

        MetricCategory& Met = mMetrics[Category];

        // Determine if outlier
        //if (Met.AvgTime > 0.0001 && (Time / Met.AvgTime > 2.0 || Time / Met.AvgTime < 0.5))
         //   return;

        Met.LastTime = Time;
    	Met.MinTime = std::min(Met.MinTime, Time);
        Met.MaxTime = std::max(Met.MaxTime, Time);

    	Met.SumTime += Time;
        Met.NumPublishes++;
        Met.AvgTime = Met.SumTime / Met.NumPublishes;
    }

    double GetLastTime(std::string Category)
    {
        return mMetrics[Category].LastTime * 1000.0;
    }

    double GetAvgTime(std::string Category)
    {
        return mMetrics[Category].AvgTime * 1000.0;
    }

    double GetMinTime(std::string Category)
    {
        return mMetrics[Category].MinTime * 1000.0;
    }

    double GetMaxTime(std::string Category)
    {
        return mMetrics[Category].MaxTime * 1000.0;
    }

} gMetrics;

struct Profiler
{
    std::chrono::high_resolution_clock::time_point Start;
	Profiler()
	{
        Start = std::chrono::high_resolution_clock::now();
	}

    double End()
	{
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - Start).count() / (double) 1e9;
	}
};

#define PROFILE_START(Category) Profiler Prof_##Category;
#define PROFILE_END(Category) gMetrics.PublishTime(#Category, Prof_##Category.End());

struct Stats
{
    float FrameTime;
};

struct SceneRenderResources
{
    SceneVertexUniforms mVertexUniforms;
    ResourceSet mForwardResources;
    ResourceLayout mForwardResourceLayout;
    Pipeline mForwardPipe;

    Camera mSceneCamera;

    bool mInitialized = false;

    void CreateForwardResources(SwapChain Swap)
    {
        ResourceSetCreateInfo CreateInfo{};
        CreateInfo.TargetSwap = Swap;
        CreateInfo.Layout = mForwardResourceLayout;
        mForwardResources = GRenderAPI->CreateResourceSet(&CreateInfo);
    }

    void CreateForwardPipeline()
    {
        ConstantBufferDescription ConstBuffer[] = {
            {0, 1, ShaderStage::Vertex, sizeof(SceneVertexUniforms)}
        };
        ResourceLayoutCreateInfo RlCreateInfo{};
        RlCreateInfo.ConstantBufferCount = std::size(ConstBuffer);
        RlCreateInfo.ConstantBuffers = ConstBuffer;
        mForwardResourceLayout = GRenderAPI->CreateResourceLayout(&RlCreateInfo);

        ShaderCreateInfo ShaderCreateInfo{};
        ShaderCreateInfo.VertexShaderVirtual = "/Shaders/Forward.vert";
        ShaderCreateInfo.FragmentShaderVirtual = "/Shaders/Forward.frag";

        VertexAttribute Attribs[] = {
            {VertexAttributeFormat::Float3, offsetof(MeshVertex, mPosition)},
            {VertexAttributeFormat::Float3, offsetof(MeshVertex, mNormal)}
        };
        PipelineCreateInfo CreateInfo{};
        CreateInfo.VertexAttributeCount = std::size(Attribs);
        CreateInfo.VertexAttributes = Attribs;
        CreateInfo.VertexBufferStride = sizeof(MeshVertex);
        CreateInfo.Shader = GRenderAPI->CreateShader(&ShaderCreateInfo);
        CreateInfo.CompatibleSwapChain = Globals.mSwap;
        CreateInfo.Layout = mForwardResourceLayout;

        PipelineBlendSettings BlendSettings;
        BlendSettings.bBlendingEnabled = false;
        CreateInfo.BlendSettingCount = 1;
        CreateInfo.BlendSettings = &BlendSettings;

        mForwardPipe = GRenderAPI->CreatePipeline(&CreateInfo);
    }

    void UpdateCamera()
    {
        uint32_t SwapWidth, SwapHeight;
        GRenderAPI->GetSwapChainSize(Globals.mSwap, SwapWidth, SwapHeight);

    	// Swap resolution
        mSceneCamera.Aspect = (float)SwapWidth / SwapHeight;
        mSceneCamera.FieldOfView = DegreesToRadians(75.0f);
        mSceneCamera.NearClip = 0.1f;
        mSceneCamera.FarClip = 5000.0f;
    }

    void Init(SwapChain Swap)
    {
        UpdateCamera();

        CreateForwardPipeline();
        CreateForwardResources(Swap);
    }


} SceneRes;

glm::mat4 CreateCameraProjection(Camera& Cam)
{
    return glm::perspective(Cam.FieldOfView, Cam.Aspect, Cam.NearClip, Cam.FarClip);
}

glm::mat4 CreateViewMatrix(Camera& Cam)
{
    glm::mat4 Translation = glm::translate(glm::mat4(1.0f), Cam.Position);
    glm::mat4 RotX = glm::rotate(glm::mat4(1.0f), Cam.Rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 RotY = glm::rotate(glm::mat4(1.0f), Cam.Rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 RotZ = glm::rotate(glm::mat4(1.0f), Cam.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 Transform = Translation * RotX * RotY * RotZ;

    return glm::inverse(Transform);
}

Pipeline CreateForwardPipeline()
{
    ResourceLayoutCreateInfo RlCreateInfo{};
    ResourceLayout Layout = GRenderAPI->CreateResourceLayout(&RlCreateInfo);

    ShaderCreateInfo ShaderCreateInfo{};
    ShaderCreateInfo.VertexShaderVirtual = "/Shaders/Foward.vert";
    ShaderCreateInfo.FragmentShaderVirtual = "/Shaders/Forward.frag";

    VertexAttribute Attribs[] = {
        {VertexAttributeFormat::Float3, offsetof(MeshVertex, mPosition)},
        {VertexAttributeFormat::Float3, offsetof(MeshVertex, mNormal)}
    };
    PipelineCreateInfo CreateInfo{};
    CreateInfo.VertexAttributeCount = std::size(Attribs);
    CreateInfo.VertexAttributes = Attribs;
    CreateInfo.VertexBufferStride = sizeof(MeshVertex);
    CreateInfo.Shader = GRenderAPI->CreateShader(&ShaderCreateInfo);
    CreateInfo.CompatibleSwapChain = Globals.mSwap;
    CreateInfo.Layout = Layout;

    PipelineBlendSettings BlendSettings;
    BlendSettings.bBlendingEnabled = false;
    CreateInfo.BlendSettingCount = 1;
    CreateInfo.BlendSettings = &BlendSettings;

    return GRenderAPI->CreatePipeline(&CreateInfo);
}

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

struct SceneResourceInit
{
    SceneResourceInit()
    {
	    
    }
};

void RenderScene(CommandBuffer Dst, Scene Render, uint32_t SwapWidth, uint32_t SwapHeight)
{
    if(!SceneRes.mInitialized)
    {
        SceneRes.Init(Globals.mSwap);
    	SceneRes.mInitialized = true;
    }

    RenderGraphInfo CompositeInfo = {
    1,
    ClearValue{ClearType::Float, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)}
    };

    GRenderAPI->BeginRenderGraph(Dst, Globals.mSwap, CompositeInfo);
    {
        GRenderAPI->UpdateUniformBuffer(SceneRes.mForwardResources, Globals.mSwap, 0, &SceneRes.mVertexUniforms, sizeof(SceneRes.mVertexUniforms));

    	GRenderAPI->BindPipeline(Dst, SceneRes.mForwardPipe);
        GRenderAPI->BindResources(Dst, SceneRes.mForwardResources);
    	GRenderAPI->SetViewport(Dst, 0, 0, static_cast<uint32_t>(SwapWidth), static_cast<uint32_t>(SwapHeight));
        GRenderAPI->SetScissor(Dst, 0, 0, static_cast<uint32_t>(SwapWidth), static_cast<uint32_t>(SwapHeight));

       // GRenderAPI->DrawVertexBufferIndexed(Dst, Render.mMeshes[0].mBuffer, Render.mMeshes[0].mIndexCount);

        for (auto Mesh : Render.mMeshes)
        {
            GRenderAPI->DrawVertexBufferIndexed(Dst, Mesh.mBuffer, Mesh.mIndexCount);
        }
    }
    GRenderAPI->EndRenderGraph(Dst);

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
        ScreenSpace[0].mPosition = { -1.0f, -1.0f, -5.0f };
        ScreenSpace[1].mPosition = { -1.0f, 1.0f, -5.0f };
        ScreenSpace[2].mPosition = { 1.0f, 1.0f, -5.0f };
        ScreenSpace[3].mPosition = { 1.0f, -1.0f, -5.0f };
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

static glm::vec3 Bl, Tr;

Mesh BuildMesh(const aiMesh* AIMesh)
{
    Mesh NewMesh;

    std::vector<MeshVertex> Verts;
    for(uint32_t VertIndex = 0; VertIndex < AIMesh->mNumVertices; VertIndex++)
    {
        aiVector3D Pos = AIMesh->mVertices[VertIndex];
        aiVector3D Norm = AIMesh->mNormals[VertIndex];

        MeshVertex Vert;
        Vert.mPosition = {Pos.x, Pos.y, Pos.z};
        Vert.mNormal = {Norm.x, Norm.y, Norm.z};
        Verts.push_back(Vert);

        Bl.x = std::min(Bl.x, Vert.mPosition.x);
        Bl.y = std::min(Bl.y, Vert.mPosition.y);
        Bl.z = std::min(Bl.z, Vert.mPosition.z);
        Tr.x = std::max(Tr.x, Vert.mPosition.x);
        Tr.y = std::max(Tr.y, Vert.mPosition.y);
        Tr.z = std::max(Tr.z, Vert.mPosition.z);
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
    if(ImGui::Begin("Profiling", &WindowOpen))
    {
        if (ImGui::CollapsingHeader("Bounds"))
        {
            ImGui::Text("Bottom Left: %.2f %.2f %.2f", Bl.x, Bl.y, Bl.z);
            ImGui::Text("Top Right: %.2f %.2f %.2f", Tr.x, Tr.y, Tr.z);
        }

        if(ImGui::CollapsingHeader("Frame"))
        {
            ImGui::Text("Last: %.2f ms", gMetrics.GetLastTime("Frame"));
            ImGui::Text("Avg: %.2f ms", gMetrics.GetAvgTime("Frame"));
        	ImGui::Text("Min: %.2f ms", gMetrics.GetMinTime("Frame"));
            ImGui::Text("Max: %.2f ms", gMetrics.GetMaxTime("Frame"));
        }
    }
    ImGui::End();
}
float z = 0.f;

void Tick(float Delta)
{
    z += Delta * 10.0f;
    SceneRes.mSceneCamera.Position = glm::vec3(0.0f, 0.0f, z);
    glm::mat4 Proj = CreateCameraProjection(SceneRes.mSceneCamera);
    glm::mat4 View = CreateViewMatrix(SceneRes.mSceneCamera);
    
	SceneRes.mVertexUniforms.ViewProjectionMatrix = glm::transpose(Proj * View);

    glm::vec4 Forward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    glm::vec4 Right = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
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

        SceneRes.UpdateCamera();
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

    auto ThisTime = std::chrono::high_resolution_clock::now();
    auto LastTime = ThisTime;

    while (!ShouldWindowClose(Globals.mWindow))
    {
        PollWindowEvents();

        // Update
        ThisTime = std::chrono::high_resolution_clock::now();
        float Delta = static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(ThisTime - LastTime).count() / (double)1e9);
        LastTime = ThisTime;
        Tick(Delta);

        BeginImGuiFrame();
        {
            DrawImGui();
        }
        EndImGuiFrame();

        uint32_t SwapWidth, SwapHeight;
        GRenderAPI->GetSwapChainSize(Globals.mSwap, SwapWidth, SwapHeight);

        PROFILE_START(Frame)
        GRenderAPI->BeginFrame(Globals.mSwap, Globals.mSurface, FrameWidth, FrameHeight);
        {
            GRenderAPI->Reset(FinalPass);
            GRenderAPI->Begin(FinalPass);
            {
                RenderScene(FinalPass, NewScene, SwapWidth, SwapHeight);
                RecordImGuiDrawCmds(FinalPass);
            }
            GRenderAPI->End(FinalPass);

            GRenderAPI->SubmitSwapCommandBuffer(Globals.mSwap, FinalPass);
        }
        GRenderAPI->EndFrame(Globals.mSwap, Globals.mSurface, FrameWidth, FrameHeight);
        PROFILE_END(Frame)


        UpdateImGuiViewports();
    }

    GRenderAPI->DestroySwapChain(Globals.mSwap);
    GRenderAPI->DestroySurface(Globals.mSurface);
    DestroyWindow(Globals.mWindow);
}
