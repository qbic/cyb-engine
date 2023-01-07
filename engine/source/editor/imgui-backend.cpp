#include "core/logger.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "editor/imgui-backend.h"
#ifdef _WIN32
#include "backends/imgui_impl_win32.cpp"
#endif

using namespace cyb::scene;
using namespace cyb::graphics;
using namespace cyb::renderer;

static Shader imgui_vs;
static Shader imgui_fs;
static Texture font_texture;
static Sampler sampler;
static VertexInputLayout imgui_input_layout;
static PipelineState imgui_pso;

struct ImGui_Impl_Data
{
};

static ImGui_Impl_Data* ImGui_Impl_GetBackendData()
{
	return ImGui::GetCurrentContext() ? (ImGui_Impl_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

void ImGui_Impl_CybEngine_CreateDeviceObject()
{
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImFontConfig fontConfig = {};
	fontConfig.OversampleH = 3;
	fontConfig.OversampleV = 1;
	fontConfig.RasterizerMultiply = 1.2f;
	auto font = io.Fonts->AddFontFromFileTTF("Assets/Cascadia Code Regular 400.otf", 16.0, &fontConfig);
	io.FontDefault = font;

	// Build texture atlas
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	// Upload texture to graphics system
	TextureDesc texture_desc;
	texture_desc.width = width;
	texture_desc.height = height;
	texture_desc.mipLevels = 1;
	texture_desc.arraySize = 1;
	texture_desc.format = Format::R8G8B8A8_Unorm;
	texture_desc.bindFlags = BindFlags::ShaderResourceBit;

	SubresourceData texture_data;
	texture_data.mem = pixels;
	texture_data.rowPitch = width * GetFormatStride(texture_desc.format);
	texture_data.slicePitch = texture_data.rowPitch * height;
	GetDevice()->CreateTexture(&texture_desc, &texture_data, &font_texture);

	SamplerDesc sampler_desc;
	sampler_desc.addressU = TextureAddressMode::Wrap;
	sampler_desc.addressV = TextureAddressMode::Wrap;
	sampler_desc.addressW = TextureAddressMode::Wrap;
	sampler_desc.filter = TextureFilter::kPoint;
	GetDevice()->CreateSampler(&sampler_desc, &sampler);

	// Store our identifier
	io.Fonts->SetTexID((ImTextureID)&font_texture);

	imgui_input_layout.elements =
	{
		{ "in_position", 0, Format::R32G32_Float,   (uint32_t)IM_OFFSETOF(ImDrawVert, pos) },
		{ "in_uv",       0, Format::R32G32_Float,   (uint32_t)IM_OFFSETOF(ImDrawVert, uv)  },
		{ "in_color",    0, Format::R8G8B8A8_Unorm, (uint32_t)IM_OFFSETOF(ImDrawVert, col) }
	};

	// Create pipeline
	PipelineStateDesc desc;
	desc.vs = &imgui_vs;
	desc.fs = &imgui_fs;
	desc.il = &imgui_input_layout;
	desc.dss = GetDepthStencilState(DSSTYPE_DEFAULT);
	desc.rs = GetRasterizerState(RSTYPE_DOUBLESIDED);
	desc.pt = PrimitiveTopology::TriangleList;
	GetDevice()->CreatePipelineState(&desc, &imgui_pso);
}

void ImGui_Impl_CybEngine_Init()
{
	// Compile shaders
	{
		LoadShader(ShaderStage::VS, imgui_vs, "imgui.vert");
		LoadShader(ShaderStage::FS, imgui_fs, "imgui.frag");
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

#ifdef _WIN32
	ImGui_ImplWin32_Init(cyb::platform::main_window->GetNativePtr());
#elif defined(SDL2)
	ImGui_ImplSDL2_InitForVulkan(window);
#endif

	IM_ASSERT(io.BackendRendererUserData == NULL && "Already initialized a renderer backend!");

	// Setup backend capabilities flags
	ImGui_Impl_Data* bd = IM_NEW(ImGui_Impl_Data)();
	io.BackendRendererUserData = (void*)bd;
	io.BackendRendererName = "CybEngine";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
	CYB_INFO("Initialized ImGui v{0}", IMGUI_VERSION);
}

void ImGui_Impl_CybEngine_Update()
{
	// Start the Dear ImGui frame
	auto* backendData = ImGui_Impl_GetBackendData();
	IM_ASSERT(backendData != NULL);

	if (!font_texture.IsValid())
		ImGui_Impl_CybEngine_CreateDeviceObject();

#ifdef _WIN32
	ImGui_ImplWin32_NewFrame();
#elif defined(SDL2)
	ImGui_ImplSDL2_NewFrame();
#endif
	ImGui::NewFrame();

	//ImGui::ShowDemoWindow();
}

void ImGui_Impl_CybEngine_Compose(CommandList cmd)
{
	// Rendering
	ImGui::Render();

	auto draw_data = ImGui::GetDrawData();
	if (!draw_data || draw_data->TotalVtxCount == 0)
		return;

	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	int framebufferWidth = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
	int framebufferHeight = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
	if (framebufferWidth <= 0 || framebufferHeight <= 0)
		return;

	//auto* bd = ImGui_Impl_GetBackendData();

	GraphicsDevice* device = GetDevice();

	// Get memory for vertex and index buffers
	const uint64_t vbSize = sizeof(ImDrawVert) * draw_data->TotalVtxCount;
	const uint64_t ibSize = sizeof(ImDrawIdx) * draw_data->TotalIdxCount;
	auto vertexBufferAllocation = device->AllocateGPU(vbSize, cmd);
	auto indexBufferAllocation = device->AllocateGPU(ibSize, cmd);

	// Copy and convert all vertices into a single contiguous buffer
	ImDrawVert* vertexCPUMem = reinterpret_cast<ImDrawVert*>(vertexBufferAllocation.data);
	ImDrawIdx* indexCPUMem = reinterpret_cast<ImDrawIdx*>(indexBufferAllocation.data);
	for (int cmdListIdx = 0; cmdListIdx < draw_data->CmdListsCount; cmdListIdx++)
	{
		const ImDrawList* drawList = draw_data->CmdLists[cmdListIdx];
		memcpy(vertexCPUMem, &drawList->VtxBuffer[0], drawList->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(indexCPUMem, &drawList->IdxBuffer[0], drawList->IdxBuffer.Size * sizeof(ImDrawIdx));
		vertexCPUMem += drawList->VtxBuffer.Size;
		indexCPUMem += drawList->IdxBuffer.Size;
	}

	// Setup orthographic projection matrix into our constant buffer
	struct ImGuiConstants
	{
		float mvp[4][4];
	};

	{
		const float L = draw_data->DisplayPos.x;
		const float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		const float T = draw_data->DisplayPos.y;
		const float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

		ImGuiConstants constants;

		float mvp[4][4] =
		{
			{ 2.0f / (R - L),    0.0f,              0.0f, 0.0f },
			{ 0.0f,              2.0f / (T - B),    0.0f, 0.0f },
			{ 0.0f,              0.0f,              0.5f, 0.0f },
			{ (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
		};
		memcpy(&constants.mvp, mvp, sizeof(mvp));

		device->BindDynamicConstantBuffer(constants, 0, cmd);
	}

	const GPUBuffer* vbs[] = {
		&vertexBufferAllocation.buffer,
	};
	const uint32_t strides[] = {
		sizeof(ImDrawVert),
	};
	const uint64_t offsets[] = {
		vertexBufferAllocation.offset,
	};

	device->BindVertexBuffers(vbs, 1, strides, offsets, cmd);
	device->BindIndexBuffer(&indexBufferAllocation.buffer, IndexBufferFormat::Uint16, indexBufferAllocation.offset, cmd);

	Viewport viewport;
	viewport.width = (float)framebufferWidth;
	viewport.height = (float)framebufferHeight;
	device->BindViewports(&viewport, 1, cmd);
	device->BindPipelineState(&imgui_pso, cmd);
	device->BindSampler(&sampler, 1, cmd);

	// Will project scissor/clipping rectangles into framebuffer space
	ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
	ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

	// Render command lists
	int32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	for (uint32_t cmdListIdx = 0; cmdListIdx < (uint32_t)draw_data->CmdListsCount; ++cmdListIdx)
	{
		const ImDrawList* drawList = draw_data->CmdLists[cmdListIdx];
		for (uint32_t cmdIndex = 0; cmdIndex < (uint32_t)drawList->CmdBuffer.size(); ++cmdIndex)
		{
			const ImDrawCmd* drawCmd = &drawList->CmdBuffer[cmdIndex];
			if (drawCmd->UserCallback)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (drawCmd->UserCallback == ImDrawCallback_ResetRenderState)
				{
				}
				else
				{
					drawCmd->UserCallback(drawList, drawCmd);
				}
			}
			else
			{
				// Project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_min(drawCmd->ClipRect.x - clip_off.x, drawCmd->ClipRect.y - clip_off.y);
				ImVec2 clip_max(drawCmd->ClipRect.z - clip_off.x, drawCmd->ClipRect.w - clip_off.y);
				if (clip_max.x < clip_min.x || clip_max.y < clip_min.y)
					continue;

				// Apply scissor/clipping rectangle
				Rect scissor;
				scissor.left = (int32_t)clip_min.x;
				scissor.top = (int32_t)clip_min.y;
				scissor.right = (int32_t)clip_max.x;
				scissor.bottom = (int32_t)clip_max.y;
				device->BindScissorRects(&scissor, 1, cmd);

				const Texture* texture = (const Texture*)drawCmd->TextureId;
				device->BindResource(texture, 1, cmd);
				device->DrawIndexed(drawCmd->ElemCount, indexOffset, vertexOffset, cmd);
			}
			indexOffset += drawCmd->ElemCount;
		}
		vertexOffset += drawList->VtxBuffer.size();
	}
}