#pragma once
//Vulkan Header
#include <vulkan/vulkan.hpp>
//GLM
#include <glm/glm.hpp>
//SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <map>

using namespace std;

class VkRenderer
{
  public:
	vk::Queue graphics_queue;
	vk::UniqueDevice device;
	vk::PhysicalDeviceMemoryProperties gpu_memory_info;
	uint32_t graphics_family_index;
	vk::RenderPass renderpass;
	vector<vk::Framebuffer> frame_buffers;
	vector<vk::Image> swapchain_buffers{};
	vk::PipelineLayout pipeline_layout;
	map<string, vk::Pipeline> pipelines;
	int buffer_count = 3;
	vk::Semaphore present_semaphore;
	vk::Semaphore render_semaphore;
	vector<vk::Fence> wait_fences;
	vector<vk::Viewport> viewports;
	vector<vk::Rect2D> scissors;
	vk::PipelineRasterizationStateCreateInfo rasterizer = vk::PipelineRasterizationStateCreateInfo();
	vk::PipelineMultisampleStateCreateInfo multisampler = vk::PipelineMultisampleStateCreateInfo();
	int render_width, render_height;
	vk::Rect2D render_area;
	
	//__Functions__
	VkRenderer(SDL_Window * window);
   ~VkRenderer();

	//..Returns to you a module of the .spv shader that's been loaded
	vk::ShaderModule LoadShaderModule(string filename);
	// ..Destroys all of the shader modules if no path is given, or destroys the specified shader module;
	void DestroyShaderModule(string shader = "");

	//Rendering
	int AcquireNextBuffer(uint32_t &buf_num);
	void BeginRenderPresent(uint32_t &buf_num, vector<vk::CommandBuffer> buffers);

	void DestroyPipelines();

	//Resizing
	void ResizeSwapchain();
	int ResizeViewports(int width, int height, int i = -1);
	int ResizeScissors(int width, int height, int i = -1);

  private:
	//__Variables__
	map<string, vk::ShaderModule> shader_cache;
	VkResult result;
	vk::UniqueInstance instance;
	VkSurfaceKHR surface;
	bool present_mode_set = true;
	vk::PresentModeKHR present_mode = vk::PresentModeKHR::eImmediate;
	vk::SurfaceCapabilitiesKHR surface_caps;
	vk::SurfaceFormatKHR surface_format;
	vk::PhysicalDevice gpu;
	vk::DeviceMemory device_memory;
	VkPhysicalDeviceProperties gpu_properties;
	vector<uint32_t> queue_family_indices;
	vk::SwapchainKHR swapchain;
	vk::SwapchainKHR old_swapchain = nullptr;
	vector<vk::ImageView> swapchain_buffer_view{};
	vk::Image depth_stencil_buffer;
	vk::ImageView depth_stencil_buffer_view;
	vk::Format depth_buffer_format = vk::Format::eUndefined;
	bool stencil_support = false;
	vector<const char *> device_extensions{};
	vector<const char *> instance_layers{};
	vector<const char *> instance_extensions{};

	//Functions
	void GetSDLWindowInfo(SDL_Window *window);
	void InitInstance();
	void DestroyInstance();
	void CreateDeviceContext();
	void DestroyDeviceContext();
	void CreateSurface(SDL_Window *window);
	void DestroySurface();
	void CreateSwapchain();
	void DestroySwapchain();
	void CreateSwapchainImages();
	void DestroySwapchainImages();
	void CreateDepthStencilImage();
	void DestroyDepthStencilImage();
	void CreateRenderpass();
	void DestroyRenderpass();
	void CreateFramebuffers();
	void DestroyFramebuffers();

	void CreateSynchronizations();
	void DestroySynchronizations();

	
#ifdef VK_DEBUG
	VkDebugReportCallbackCreateInfoEXT debug_create_info;
	VkDebugReportCallbackEXT debug_report;
	void SetupDebug();
	void InitDebug();
	void DestroyDebug();
#endif
};

// Vertex Struct
struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	static vk::VertexInputBindingDescription GetBindingDescription(){
		vk::VertexInputBindingDescription binding_description;
		binding_description.setBinding(0);
		binding_description.setStride(sizeof(Vertex));
		binding_description.setInputRate(vk::VertexInputRate::eVertex);

		return binding_description;
	}

	static vector<vk::VertexInputAttributeDescription> GetAttributeDescription(){
		vector<vk::VertexInputAttributeDescription> attribute_descriptions = {
			vk::VertexInputAttributeDescription(
				//location, binding, format, offset
				0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)
			),
			vk::VertexInputAttributeDescription(
				//location, binding, format, offset
				1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)
			),
		};

		return attribute_descriptions;
	}
};

//Vertex Buffer
class VertexBuffer{
	public:
		vk::Buffer vertex_buffer;
		vector<Vertex> verex_info;
		vk::Device device;

		VertexBuffer(vector<Vertex>, vk::SharingMode, VkRenderer *);
		~VertexBuffer();
	private:
		vk::BufferCreateInfo buffer_info;
		vk::DeviceMemory buffer_memory;
};