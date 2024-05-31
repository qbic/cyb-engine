#include "core/logger.h"
#include "systems/event_system.h"
#include "systems/resource_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_freetype.h"
#include "editor/imgui_backend.h"
#ifdef _WIN32
#include "backends/imgui_impl_win32.cpp"
#endif
#include "editor/icons_font_awesome6.h"

using namespace cyb::scene;
using namespace cyb::graphics;
using namespace cyb::renderer;

struct ImGui_Impl_Data
{
	Shader vs;
	Shader fs;
	Texture fontTexture;
	Sampler sampler;
	VertexInputLayout inputLayout;
	PipelineState pso;
	IndexBufferFormat indexFormat;

	ImGui_Impl_Data()
	{
		memset(this, 0, sizeof(ImGui_Impl_Data));
	}
};

struct ImGuiConstants
{
	float mvp[4][4];
};

static ImGui_Impl_Data* ImGui_Impl_GetBackendData()
{
	return ImGui::GetCurrentContext() ? (ImGui_Impl_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

ImFont* AddFont(const char* filename, const ImWchar* ranges, float size, bool merge)
{
	ImFontConfig fontConfig = {};
	fontConfig.MergeMode = merge;
	fontConfig.PixelSnapH = true;
	float pixelSize = std::round(size * 96.0f / 72.0f);
	const std::string asset = cyb::resourcemanager::FindFile(filename);
	return ImGui::GetIO().Fonts->AddFontFromFileTTF(asset.c_str(), pixelSize, &fontConfig, ranges);
}

void ImGui_Impl_CybEngine_CreateDeviceObject()
{
	ImGui_Impl_Data* bd = ImGui_Impl_GetBackendData();
	ImGuiIO& io = ImGui::GetIO();

	// Load fonts:
	static const ImWchar fontAwesomeIconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	static const ImWchar notoSansRanges[] = { 0x20, 0x52f, 0x1ab0, 0x2189, 0x2c60, 0x2e44, 0xa640, 0xab65, 0 };
	static const ImWchar notoMonoRanges[] = { 0x20, 0x513, 0x1e00, 0x1f4d, 0 };

	AddFont("fonts/CascadiaCode-Regular.ttf", notoSansRanges, 14.0f, false);
	AddFont("fonts/" FONT_ICON_FILE_NAME_FAS, fontAwesomeIconRanges, 13.f, true);
	AddFont("fonts/" FONT_ICON_FILE_NAME_FAS, fontAwesomeIconRanges, 12.f, true);
	AddFont("fonts/NotoMono-Regular.ttf", notoMonoRanges, 14.0, false);

	// Build texture atlas:
	unsigned char* pixels;
	int width, height;
	io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	// Upload texture to graphics system:
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
	GetDevice()->CreateTexture(&texture_desc, &texture_data, &bd->fontTexture);

	SamplerDesc sampler_desc;
	sampler_desc.addressU = TextureAddressMode::Wrap;
	sampler_desc.addressV = TextureAddressMode::Wrap;
	sampler_desc.addressW = TextureAddressMode::Wrap;
	sampler_desc.filter = TextureFilter::Point;
	GetDevice()->CreateSampler(&sampler_desc, &bd->sampler);

	// Store our identifier:
	io.Fonts->SetTexID((ImTextureID)&bd->fontTexture);

	// Get the index buffer format from ImDrawIdx size (can be overwrittern in imconfig.h)
	assert(sizeof(ImDrawIdx) == 2 || sizeof(ImDrawIdx) == 4);
	if (sizeof(ImDrawIdx) == 2)
		bd->indexFormat = IndexBufferFormat::Uint16;
	else 
		bd->indexFormat = IndexBufferFormat::Uint32;
}

static void SetupCustomStyle()
{
	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.13f, 0.11f, 0.98f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.71f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.15f, 0.71f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.10f, 0.10f, 0.10f, 0.71f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.13f, 0.11f, 0.94f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 0.94f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.38f, 0.58f, 0.71f, 0.94f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.38f, 0.58f, 0.71f, 0.94f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.58f, 0.75f, 0.81f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);

	ImGui::GetStyle().FrameRounding = 4.0f;
	ImGui::GetStyle().GrabRounding = 4.0f;
}

static void LoadShaders()
{
	ImGui_Impl_Data* bd = ImGui_Impl_GetBackendData();
	LoadShader(ShaderStage::VS, bd->vs, "imgui.vert");
	LoadShader(ShaderStage::FS, bd->fs, "imgui.frag");

	bd->inputLayout.elements =
	{
		{ "in_position", 0, Format::R32G32_Float,   (uint32_t)offsetof(ImDrawVert, pos) },
		{ "in_uv",       0, Format::R32G32_Float,   (uint32_t)offsetof(ImDrawVert, uv)  },
		{ "in_color",    0, Format::R8G8B8A8_Unorm, (uint32_t)offsetof(ImDrawVert, col) }
	};

	PipelineStateDesc desc;
	desc.vs = &bd->vs;
	desc.fs = &bd->fs;
	desc.il = &bd->inputLayout;
	desc.dss = GetDepthStencilState(DSSTYPE_DEFAULT);
	desc.rs = GetRasterizerState(RSTYPE_DOUBLESIDED);
	desc.pt = PrimitiveTopology::TriangleList;
	GetDevice()->CreatePipelineState(&desc, &bd->pso);
}

void ImGui_Impl_CybEngine_Init(cyb::WindowHandle window)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

	// Setup backend capabilities flags
	ImGui_Impl_Data* bd = IM_NEW(ImGui_Impl_Data)();
	io.BackendRendererUserData = (void*)bd;
	io.BackendRendererName = "CybEngine";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

	// Compile shaders
	LoadShaders();
	static cyb::eventsystem::Handle handle = cyb::eventsystem::Subscribe(cyb::eventsystem::Event_ReloadShaders, [](uint64_t userdata) { LoadShaders(); });
	
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	SetupCustomStyle();

#ifdef _WIN32
	ImGui_ImplWin32_Init(window);
#elif defined(SDL2)
	ImGui_ImplSDL2_InitForVulkan(window);
#endif

	CYB_INFO("Initialized ImGui v{0}", IMGUI_VERSION);
}

void ImGui_Impl_CybEngine_Update()
{
	ImGui_Impl_Data* bd = ImGui_Impl_GetBackendData();

	if (!bd->fontTexture.IsValid())
		ImGui_Impl_CybEngine_CreateDeviceObject();

#ifdef _WIN32
	ImGui_ImplWin32_NewFrame();
#elif defined(SDL2)
	ImGui_ImplSDL2_NewFrame();
#endif
	ImGui::NewFrame();

	// This can be disabled from imconfig.h aswell
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

	ImGui_Impl_Data* bd = ImGui_Impl_GetBackendData();
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
	const float L = draw_data->DisplayPos.x;
	const float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	const float T = draw_data->DisplayPos.y;
	const float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
	float mvp[4][4] =
	{
		{ 2.0f / (R - L),    0.0f,              0.0f, 0.0f },
		{ 0.0f,              2.0f / (T - B),    0.0f, 0.0f },
		{ 0.0f,              0.0f,              0.5f, 0.0f },
		{ (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
	};

	ImGuiConstants constants = {};
	memcpy(&constants.mvp, mvp, sizeof(mvp));
	device->BindDynamicConstantBuffer(constants, 0, cmd);

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
	device->BindIndexBuffer(&indexBufferAllocation.buffer, bd->indexFormat, indexBufferAllocation.offset, cmd);

	Viewport viewport;
	viewport.width = (float)framebufferWidth;
	viewport.height = (float)framebufferHeight;
	device->BindViewports(&viewport, 1, cmd);
	device->BindPipelineState(&bd->pso, cmd);
	device->BindSampler(&bd->sampler, 1, cmd);

	// We'll project scissor/clipping rectangles into framebuffer space
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
				if (drawCmd->UserCallback != ImDrawCallback_ResetRenderState)
					drawCmd->UserCallback(drawList, drawCmd);
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