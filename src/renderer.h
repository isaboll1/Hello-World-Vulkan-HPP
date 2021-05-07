#pragma once
//Vulkan Header
#include <vulkan/vulkan.hpp>
//VMA
#include "vk_mem_alloc.hpp"
//GLM
#define GLM_FORCE_CTOR_INIT 
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
  	vk::Result result;
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
	vk::Semaphore present_semaphore = nullptr;
	vk::Semaphore render_semaphore = nullptr;
	vector<vk::Fence> wait_fences = {};
	vector<vk::Viewport> viewports = {};
	vector<vk::Rect2D> scissors = {};
	vk::PipelineRasterizationStateCreateInfo rasterizer = vk::PipelineRasterizationStateCreateInfo();
	vk::PipelineMultisampleStateCreateInfo multisampler = vk::PipelineMultisampleStateCreateInfo();
	int render_width, render_height;
	vk::Rect2D render_area = {};
	vma::Allocator gpu_allocator = nullptr;
	VkPhysicalDeviceProperties gpu_properties = {};
	vk::CommandPool command_pool = nullptr;
	//Array used for displaying the Vulkan device type to console
	const char *device_type[5] = {
		"VK_PHYSICAL_DEVICE_TYPE_OTHER",
		"VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU",
		"VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU",
		"VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU",
		"VK_PHYSICAL_DEVICE_TYPE_CPU"
	};
	
	//__Functions__
	VkRenderer(SDL_Window * window);
   ~VkRenderer();

	//..Returns to you a module of the .spv shader that's been loaded
	vk::ShaderModule LoadShaderModule(string filename);
	// ..Destroys all of the shader modules if no path is given, or destroys the specified shader module;
	void DestroyShaderModule(string shader = "");

	//Device Commands
	vk::CommandPool CreateDeviceCommandPool(uint32_t index, vk::CommandPoolCreateFlagBits flags);
	vector<vk::CommandBuffer> GetCommandBuffers(vk::CommandBufferLevel level, int buffer_count, vk::CommandPool pool = nullptr);
	void DestroyDeviceCommandPool(vk::CommandPool * pool);

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
	int required_i_extensions = 0;
	int required_d_extensions = 3;
	int supported_d_extensions = 0;
	VkResult res;
	map<string, vk::ShaderModule> shader_cache;
	vk::UniqueInstance instance;
	VkSurfaceKHR surface;
	bool present_mode_set = true;
	vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
	vk::SurfaceCapabilitiesKHR surface_caps;
	vk::SurfaceFormatKHR surface_format;
	vk::PhysicalDevice gpu;
	vk::DeviceMemory device_memory;
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
	vma::Allocation depth_buffer_allocation;
	vk::DispatchLoaderDynamic dldid;

	//Functions
	void GetSDLWindowInfo(SDL_Window *window);
	void GetExtraInstanceExtensions();
	void InitInstance();
	void DestroyInstance();
	int GetExtraDeviceExtensions(vk::PhysicalDevice * gpu);
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
		vma::Allocator * allocator;
		VkPhysicalDeviceProperties gpu_properties;

		VertexBuffer(vector<Vertex>, VkRenderer *);
		~VertexBuffer();
	private:
		vk::BufferCreateInfo buffer_info = {};
		vma::Allocation buffer_memory;
		vk::BufferCreateInfo staging_buffer_info = {};
		vk::Buffer staging_buffer = nullptr;
		vma::Allocation staged_memory = nullptr;
};