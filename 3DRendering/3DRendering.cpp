#include "Engine.h"
#include "File.h"
#include "Global.h"
#include "RenderingInterface.h"
#include "Window.h"
#include "glm/glm.hpp"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

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

struct Mesh
{
    VertexBuffer mBuffer;
    uint32_t mVertexCount;
    uint32_t mIndexCount;
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

    CommandBuffer FinalPass = GRenderAPI->CreateSwapChainCommandBuffer(Globals.mSwap, true);
    Pipeline FinalPassPipeline = CreateFinalPassPipeline();

    Mesh ScreenSpaceMesh = CreateScreenSpaceMesh(FinalPassVertex{});

    while (!ShouldWindowClose(Globals.mWindow))
    {
        PollWindowEvents();

        // TODO: do stuff

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
                    GRenderAPI->SetViewport(FinalPass, 0, 0, static_cast<uint32_t>(FrameWidth), static_cast<uint32_t>(FrameHeight));
                    GRenderAPI->SetScissor(FinalPass, 0, 0, static_cast<uint32_t>(FrameWidth), static_cast<uint32_t>(FrameHeight));
                    GRenderAPI->DrawVertexBufferIndexed(FinalPass, ScreenSpaceMesh.mBuffer, ScreenSpaceMesh.mIndexCount);
                }
                GRenderAPI->EndRenderGraph(FinalPass);
            }
            GRenderAPI->End(FinalPass);

            GRenderAPI->SubmitSwapCommandBuffer(Globals.mSwap, FinalPass);
        }
        GRenderAPI->EndFrame(Globals.mSwap, Globals.mSurface, FrameWidth, FrameHeight);
    }

    GRenderAPI->DestroySwapChain(Globals.mSwap);
    GRenderAPI->DestroySurface(Globals.mSurface);
    DestroyWindow(Globals.mWindow);
}
