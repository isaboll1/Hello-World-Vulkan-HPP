#include "renderer.h"
#ifdef _WIN32
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

VkRenderer::VkRenderer(SDL_Window * window)
{
	GetSDLWindowInfo(window);
	InitInstance();
	CreateDeviceContext();
	CreateSurface(window);
	CreateSwapchain();
	CreateSwapchainImages();
	CreateDepthStencilImage();
	CreateRenderpass();
	CreateFramebuffers();
	
	render_area.setExtent(vk::Extent2D(render_width, render_height));

	CreateSynchronizations();
}


VkRenderer::~VkRenderer()
{
	DestroyShaderModule();
	DestroyPipelines();

	graphics_queue.waitIdle();

	DestroySynchronizations();

	DestroyFramebuffers();
	DestroyRenderpass();
	DestroyDepthStencilImage();
	DestroySwapchainImages();
	DestroySwapchain();
	DestroySurface();
	DestroyDeviceContext();
#ifdef VK_DEBUG
	DestroyDebug();
#endif
	DestroyInstance();
	
}


void VkRenderer::GetSDLWindowInfo(SDL_Window * window) //Get the necessary information from the SDL2 window handle for Vulkan support.
{
	uint32_t extension_count = 0;
	SDL_Vulkan_GetInstanceExtensions(window, &extension_count, NULL);
	instance_extensions.resize(extension_count);
	if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, instance_extensions.data())){
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Vulkan Error", "No instance extensions for creating a window surface", NULL);
		throw runtime_error("window creation error");
	}
	device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	// if you can get the window extention info for vulkan, you can get the window rendering size.
	SDL_Vulkan_GetDrawableSize(window, &render_width, &render_height);
}


void VkRenderer::InitInstance() //Initializes the Vulkan Instance
{
#ifdef VK_DEBUG
	SetupDebug();
#endif //add debug extentions

	auto app_info = vk::ApplicationInfo(
		"Vulkan SDL2 Application",
		VK_MAKE_VERSION(0, 2, 0),
		"Hello Vulkan++",
		VK_MAKE_VERSION(1, 1, 0)
	);

	instance = vk::createInstanceUnique(
		vk::InstanceCreateInfo(
			vk::InstanceCreateFlags(),
			&app_info,
			instance_layers.size(),
			instance_layers.data(),
			instance_extensions.size(),
			instance_extensions.data()
	));
	
	if (!instance) {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, "Create Instance Failed");
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan Error!", " Couldn't create instance.\n Make sure Vulkan driver is installed.", NULL);
		throw "No instance created";
	}

#ifdef VK_DEBUG
	InitDebug();
#endif

}

void VkRenderer::DestroyInstance()
{
	instance->destroy();
	instance.release();
}


void VkRenderer::CreateDeviceContext() //Creates the Vulkan Device Context.
{
	//Get the Physical GPU.
	auto physical_devices = instance->enumeratePhysicalDevices();
	if (!physical_devices.size()){
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, "Get Physical GPU Failed");
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan Error!",
		 " Couldn't find a Vulkan-enabled GPU.\n Make sure your GPU is supported by Vulkan, and if so make sure a Vulkan-enabled driver is installed for your GPU.\n To check if your GPU is supported, please visit https://vulkan.gpuinfo.org/", NULL);
		throw "No device found";
	}

	gpu = physical_devices[0];

	//Array used for displaying the Vulkan device type to console
	const char *device_type[5] = {
		"VK_PHYSICAL_DEVICE_TYPE_OTHER",
		"VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU",
		"VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU",
		"VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU",
		"VK_PHYSICAL_DEVICE_TYPE_CPU"
		};

	//Qeury GPU Info.
	gpu_properties = gpu.getProperties();
	gpu_memory_info = gpu.getMemoryProperties();

	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, "Driver Version: %d\n", gpu_properties.driverVersion);
	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, "Device Name: %s\n", gpu_properties.deviceName);
	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, "Device Type: %s\n", device_type[gpu_properties.deviceType]);
	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, "Vulkan Version: %d. %d. %d\n",
		(gpu_properties.apiVersion >> 22) & 0x3FF,
		(gpu_properties.apiVersion >> 12) & 0x3FF,
		(gpu_properties.apiVersion & 0xFFF));
	printf("\n");
	
	//Get the GPU supported operations.
	auto gpu_features = gpu.getFeatures();
	auto gpu_qProperties = gpu.getQueueFamilyProperties();
	auto gpu_qInfo = vector<vk::DeviceQueueCreateInfo>();
	graphics_family_index = 0;
	float gpu_qPriority =  1.0f;
	bool gr = false;
	
	for (auto queue_family : gpu_qProperties) {
		if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
			gpu_qInfo.push_back(vk::DeviceQueueCreateInfo(
				vk::DeviceQueueCreateFlags(),
				graphics_family_index,
				1,
				&gpu_qPriority
			));
			gr = true;
			break;
		}
		graphics_family_index++;
	}

	if (!gr) {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, "Graphics operations not supported");
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan Error!", "Graphics operations aren't supported for the GPU!", NULL);
		throw "GPU is crank!";
	}

	//Logical Device Context
	device = gpu.createDeviceUnique(vk::DeviceCreateInfo(
		vk::DeviceCreateFlags(),
		gpu_qInfo.size(),
		gpu_qInfo.data(),
		NULL,
		NULL, 
		device_extensions.size(),
		device_extensions.data(),
		&gpu_features
	));
	graphics_queue = device->getQueue(graphics_family_index, 0);

	queue_family_indices.push_back(graphics_family_index);
}

void VkRenderer::DestroyDeviceContext()
{
	device->destroy();
	device.release();
}


void VkRenderer::CreateSurface(SDL_Window * window) //Creates a platform agnostic Vulkan Window Surface using SDL2.
{
	SDL_Vulkan_CreateSurface(window, instance.get(), &surface);
	if (!gpu.getSurfaceSupportKHR(graphics_family_index, surface)) {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, "Could not not create window surface for Vulkan!");
	} 
	
	surface_caps = gpu.getSurfaceCapabilitiesKHR(surface);

	if (surface_caps.currentExtent.width < UINT32_MAX) {
		render_width = surface_caps.currentExtent.width;
		render_height = surface_caps.currentExtent.height;
	}

	vector<vk::SurfaceFormatKHR> formats = gpu.getSurfaceFormatsKHR(surface);	

	if (formats[0].format == vk::Format::eUndefined) {
		surface_format.format = vk::Format::eR8G8B8A8Unorm;
		surface_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
	}
	else { surface_format = formats[0]; }
	
}

void VkRenderer::DestroySurface(){
	instance->destroySurfaceKHR(surface);
}


void VkRenderer::CreateSwapchain(){ //Create the Swapchain.
	if (surface_caps.maxImageCount){
		if (buffer_count > surface_caps.maxImageCount){buffer_count = surface_caps.maxImageCount;}
		else if (buffer_count < surface_caps.minImageCount < surface_caps.maxImageCount){buffer_count = surface_caps.minImageCount + 1;}
	}

	auto present_modes = gpu.getSurfacePresentModesKHR(surface);

	if (!present_mode_set) { present_mode = present_modes[0]; }
	present_mode_set = true;
	
	swapchain = device->createSwapchainKHR(vk::SwapchainCreateInfoKHR(
		vk::SwapchainCreateFlagsKHR(),
		surface, buffer_count, 
		surface_format.format,
		surface_format.colorSpace,
		vk::Extent2D(render_width, render_height),
		1, vk::ImageUsageFlagBits::eColorAttachment, 
		vk::SharingMode::eExclusive,
		queue_family_indices.size(), queue_family_indices.data(),
		vk::SurfaceTransformFlagBitsKHR::eIdentity,
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		present_mode, VK_TRUE, old_swapchain
	));
}

void VkRenderer::DestroySwapchain(){
	device->destroySwapchainKHR(swapchain);
}

void VkRenderer::ResizeSwapchain(){
	surface_caps = gpu.getSurfaceCapabilitiesKHR(surface);
	if ((render_height != surface_caps.currentExtent.height) || (render_width != surface_caps.currentExtent.width)) {
		device->waitIdle();

		render_height = surface_caps.currentExtent.height;
		render_width = surface_caps.currentExtent.width;
		render_area.setExtent(vk::Extent2D(render_width, render_height));

		old_swapchain = swapchain;
		DestroyFramebuffers();
		DestroyRenderpass();
		DestroyDepthStencilImage();
		DestroySwapchainImages();

		CreateSwapchain();
		CreateSwapchainImages();
		CreateDepthStencilImage();
		CreateRenderpass();
		CreateFramebuffers();

		device->destroySwapchainKHR(old_swapchain);
		old_swapchain = nullptr;
	}
}

void VkRenderer::CreateSwapchainImages(){
	swapchain_buffers = device->getSwapchainImagesKHR(swapchain);
	for (auto buffer : swapchain_buffers) {
		swapchain_buffer_view.push_back(
			device->createImageView(
				vk::ImageViewCreateInfo(
					vk::ImageViewCreateFlags(), buffer, 
					vk::ImageViewType::e2D,
					surface_format.format,
					vk::ComponentMapping(),  //R,G,B,A: Identity Components
					vk::ImageSubresourceRange(
						vk::ImageAspectFlagBits::eColor,
						0,   //baseMiplevel
						1,   //level count
						0,   //baseArraylayers
						1)   //layers count
				)
			)
		);
	}
}

void VkRenderer::DestroySwapchainImages(){
	for (auto image : swapchain_buffer_view) {
		device->destroyImageView(image);
	}
	swapchain_buffer_view.resize(0);
}


void VkRenderer::CreateDepthStencilImage(){ //Create the Depth and Stencil Buffer Attachments for Swapchain Rendering

	//Check for the format of the Depth/Stencil Buffer
	vector<vk::Format> depth_formats = {vk::Format::eD32SfloatS8Uint, vk::Format::eD32Sfloat, 
										vk::Format::eD24UnormS8Uint, vk::Format::eD16UnormS8Uint,
										vk::Format::eD16Unorm};
	for (auto format : depth_formats) {
		auto format_properties = gpu.getFormatProperties(format);
		if (format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment){
			depth_buffer_format = format;
			break;
		}	
	}

	//Check the Buffer Aspect
	if (depth_buffer_format != vk::Format::eUndefined)
		stencil_support = true;

	depth_stencil_buffer = device->createImage(vk::ImageCreateInfo(
		vk::ImageCreateFlags(),vk::ImageType::e2D, depth_buffer_format,
		vk::Extent3D(vk::Extent2D(render_width, render_height), 1), 1,
		1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, 
		vk::ImageUsageFlagBits::eDepthStencilAttachment, 
		vk::SharingMode::eExclusive, queue_family_indices.size(), 
		queue_family_indices.data(),
		vk::ImageLayout::eUndefined)
	);

	auto memory_req = device->getImageMemoryRequirements(depth_stencil_buffer); //Get the required information about memory for the buffer
	uint32_t memory_index = UINT32_MAX; //Index of GPU memory Location
	auto mem_properties = vk::MemoryPropertyFlagBits::eDeviceLocal; //Application controlled memory property (GPU local memory)
	for (uint32_t i = 0; i < gpu_memory_info.memoryTypeCount; i++) {
		if (memory_req.memoryTypeBits &(1 << i)) {
			if ((gpu_memory_info.memoryTypes[i].propertyFlags & mem_properties) == mem_properties ) {
				memory_index = i;
				break;
			}
		}
	}

	SDL_assert(memory_index != UINT32_MAX);

	device_memory = device->allocateMemory(
		vk::MemoryAllocateInfo(memory_req.size, memory_index));

	device->bindImageMemory(depth_stencil_buffer,
		                    device_memory, 0);
	
	depth_stencil_buffer_view = device->createImageView(
		vk::ImageViewCreateInfo(
			vk::ImageViewCreateFlags(), depth_stencil_buffer,
			vk::ImageViewType::e2D, depth_buffer_format,
			vk::ComponentMapping(),  //R,G,B,A: Identity Components
			vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
				0, 1, 0, 1)
		));

}

void VkRenderer::DestroyDepthStencilImage(){
	device->destroyImageView(depth_stencil_buffer_view);
	device->freeMemory(device_memory);
	device->destroyImage(depth_stencil_buffer);
}


void VkRenderer::CreateRenderpass() {
	vector<vk::AttachmentDescription> attachment_descriptions = //Render pass attachment descriptions (currently for the color pass and depth pass)
		{
			vk::AttachmentDescription( 
				vk::AttachmentDescriptionFlags(), //DEPTH BUFFER ATTACHMENT
				depth_buffer_format,			  //format
				vk::SampleCountFlagBits::e1,	  //samples
				vk::AttachmentLoadOp::eClear,	  //loadOp
				vk::AttachmentStoreOp::eDontCare, //storeOp
				vk::AttachmentLoadOp::eDontCare,  //depth loadOp
				vk::AttachmentStoreOp::eStore,	  //depth storeOp
				vk::ImageLayout::eUndefined,	  //initial/final image layout
				vk::ImageLayout::eDepthStencilAttachmentOptimal),

			vk::AttachmentDescription(
				vk::AttachmentDescriptionFlags(), //STENCIL BUFFER ATTACHMENT
				surface_format.format,			  //format
				vk::SampleCountFlagBits::e1,	  //samples
				vk::AttachmentLoadOp::eClear,	  //loadOp
				vk::AttachmentStoreOp::eStore,	  //storeOp
				vk::AttachmentLoadOp::eDontCare,  //stencil loadOp
				vk::AttachmentStoreOp::eDontCare, //stencil storeOp
				vk::ImageLayout::eUndefined,	  //initial image layout
				vk::ImageLayout::ePresentSrcKHR   //final image layout
				),
			//extra attachments
			//~extra attachments
		};

	vector<vk::AttachmentReference> depth_reference = {
		vk::AttachmentReference(0, vk::ImageLayout::eDepthStencilAttachmentOptimal)
	};

	vector<vk::AttachmentReference> stencil_reference = {
		vk::AttachmentReference(1, vk::ImageLayout::eColorAttachmentOptimal) // synonymous with (in glsl): layout(location = 0) out vec4 FinalColor
		//extra color attachments
		//~extra color attachments
	};

	//subpasses used for stages that are part of the renderpass
	vector<vk::SubpassDescription> subpasses = {
		vk::SubpassDescription(
			vk::SubpassDescriptionFlags(),
			vk::PipelineBindPoint::eGraphics,
			0, nullptr, stencil_reference.size(),
			stencil_reference.data(), nullptr, 
			depth_reference.data(), 0, nullptr)

		//extra subpasses
		//~extra subpasses
	};

	//Subpass dependecies (for extra control)
	vector<vk::SubpassDependency> subpass_dependencies = {};

	//The renderpass where rendering operations are performed
	renderpass = device->createRenderPass(
		vk::RenderPassCreateInfo(
			vk::RenderPassCreateFlags(),
			attachment_descriptions.size(),
			attachment_descriptions.data(),
			subpasses.size(),
			subpasses.data(),
			subpass_dependencies.size(),
			subpass_dependencies.data()
		));
}


void VkRenderer::DestroyRenderpass() {
	device->destroyRenderPass(renderpass);
}


void VkRenderer::CreateFramebuffers() {
	frame_buffers.resize(buffer_count);
	for (int i = 0; i < buffer_count; i++) {
		vector<vk::ImageView> framebuffer_attachments =
			{depth_stencil_buffer_view, swapchain_buffer_view[i]};

		frame_buffers[i] = device->createFramebuffer(
			vk::FramebufferCreateInfo(
				vk::FramebufferCreateFlags(),
				renderpass, framebuffer_attachments.size(),
				framebuffer_attachments.data(),
				render_width, render_height, 1)
		);
	}

}


void VkRenderer::DestroyFramebuffers() {
	for (auto buffer : frame_buffers)
		device->destroyFramebuffer(buffer);
}


void VkRenderer::CreateSynchronizations() {
	present_semaphore = device->createSemaphore(vk::SemaphoreCreateInfo());
	render_semaphore = device->createSemaphore(vk::SemaphoreCreateInfo());
	wait_fences.resize(buffer_count);
	for (int i=0; i < buffer_count; i++){
		wait_fences[i] = device->createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
	}
}


void VkRenderer::DestroySynchronizations() {
	for (auto fence: wait_fences)
		device->destroyFence(fence);
	device->destroySemaphore(render_semaphore);
	device->destroySemaphore(present_semaphore);
}

//_______________________________ FUNCTIONS RELATED TO RENDERING _____________________________________________
int VkRenderer::AcquireNextBuffer(uint32_t &buf_num){
	vk::Result result = device->acquireNextImageKHR(swapchain, UINT64_MAX, present_semaphore, nullptr, &buf_num);
	try {
		if ((result == vk::Result::eSuccess) || (result == vk::Result::eSuboptimalKHR)) {
			device->waitForFences(1, &wait_fences[buf_num], VK_TRUE, UINT64_MAX);
			device->resetFences(1, &wait_fences[buf_num]);
			return 1;
		}
	}catch(const runtime_error& e){
		ResizeSwapchain();
	}	
	return 0;
}


void VkRenderer::BeginRenderPresent(uint32_t &buf_num, vector<vk::CommandBuffer> buffers) {
	vk::PipelineStageFlags pipeline_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	auto submit_info = vk::SubmitInfo(1, &present_semaphore, &pipeline_flags, 1, &buffers[buf_num], 1, &render_semaphore);

	//Submitting the command buffer to the graphics queue begins the rendering process for those set of commands
	graphics_queue.submit(submit_info, wait_fences[buf_num]);

	//presentKHR presents from the graphics queue, the finished swapchain that has been rendered to.
	try{
		graphics_queue.presentKHR(
			vk::PresentInfoKHR(
			1,
			&render_semaphore,
			1,
			&swapchain,
			&buf_num,
			nullptr)
		);
	} catch (const runtime_error& e){
		ResizeSwapchain();
	}
}

void VkRenderer::DestroyPipelines(){
	device->destroyPipelineLayout(pipeline_layout);
	for (auto pipeline: pipelines) {
		device->destroyPipeline(pipeline.second);
	}
}

int VkRenderer::ResizeViewports(int width, int height, int i) {
	if (i >= int(viewports.size())){return 0;}
	if (i <= -1){
		for (int j = 0; j < viewports.size(); j++) {
			viewports[j].width = width;
			viewports[j].height = height;
		}
		return 1;
	}
	viewports[i].width = width;
	viewports[i].height = height;
	return 1;
}

int VkRenderer::ResizeScissors(int width, int height, int i) {
	if (i >= int(scissors.size())){return 0;}
	if (i <= -1) {
		for (int j = 0; j < scissors.size(); j++) {
			scissors[j].extent.width = width;
			scissors[j].extent.height = height;
		}
		return 1;
	}
	scissors[i].extent.width = width;
	scissors[i].extent.height = height;
	return 1;
}

static vector<char> ReadShaderFile(string filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{	
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Rendering Error!",  string("Can't load shader, might not be in path: \n" + filename).data(), NULL);
		throw runtime_error("failed to open file!");
		
	}

	size_t file_size = file.tellg();
	std::vector<char> buffer(file_size);

	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), file_size);

	file.close();

	return buffer;
}

uint32_t FindMemoryType(VkRenderer * renderer, uint32_t type_filter, vk::MemoryPropertyFlags properties){
	auto memory_properties = renderer->gpu_memory_info;
	for (uint32_t i=0; i < memory_properties.memoryTypeCount; i++){
		if ((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	throw "failed to find the most suitable memory type!";
}

vk::ShaderModule VkRenderer::LoadShaderModule(string filename) {
	if (!shader_cache.count(filename)){
		vector<char> binary = ReadShaderFile(filename);
		shader_cache[filename] = device->createShaderModule(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),
				binary.size(),
				reinterpret_cast<const uint32_t *>(binary.data())
				));
		if (!shader_cache[filename]){
			throw runtime_error("We couldn't create a shader module");
		}
	}
	return shader_cache[filename];
}

void VkRenderer::DestroyShaderModule(string shader){
	if (shader.empty()){
		for (auto module : shader_cache){
			device->destroyShaderModule(module.second);
		}
		return;
	}
	device->destroyShaderModule(shader_cache[shader]);
	shader_cache.erase(shader);
}

// _______________________________ VK_VALIDATION_LAYERS_(DEBUG)_______________________________________________

#ifdef VK_DEBUG

PFN_vkCreateDebugReportCallbackEXT fvkCreateDebugReportCallbackEXT = nullptr;
PFN_vkDestroyDebugReportCallbackEXT fvkDestroyDebugReportCallbackEXT = nullptr;

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCall(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  obj_type,
	uint64_t                    src_object,
	size_t                      location,
	int32_t                     msg_code,
	const char*                 layer_prefix,
	const char*                 msg,
	void *                      user_data)
{
	string stream = msg;
	/*
	if (stream.length() > 100)
		stream.resize(120);
	*/

	//Warnings are given message boxes because anything that could mess with conformance should be known and not hidden from sight
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, stream.c_str());
		printf("\n");
		return false;
	}
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, msg);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Vulkan Warning!", stream.c_str(), NULL);
		printf("\n");
		return false;
	}
	else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, msg);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan Error!", stream.c_str(), NULL);
		printf("\n");
		return false;
	}

	else {
		return true;
	}

}

void VkRenderer::SetupDebug()
{
	debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	debug_create_info.pfnCallback = VulkanDebugCall;
	debug_create_info.flags =
		VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
		VK_DEBUG_REPORT_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_ERROR_BIT_EXT |
		0;

	instance_layers.push_back("VK_LAYER_LUNARG_standard_validation");
	instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
}

void VkRenderer::InitDebug()
{
	fvkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance.get(), "vkCreateDebugReportCallbackEXT");
	fvkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance.get(), "vkDestroyDebugReportCallbackEXT");
	if (fvkCreateDebugReportCallbackEXT == nullptr || fvkDestroyDebugReportCallbackEXT == nullptr) {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, "Could not get debug functions!");
	}

	result = fvkCreateDebugReportCallbackEXT(instance.get(), &debug_create_info, nullptr, &debug_report);
}

void VkRenderer::DestroyDebug()
{
	fvkDestroyDebugReportCallbackEXT(instance.get(), debug_report, nullptr);
}

#endif
//________________________________________________________________________________

// VERTEX BUFFER CLASS____________________________________________________________
VertexBuffer::VertexBuffer(vector<Vertex> vertices, vk::SharingMode sharing_mode, VkRenderer * renderer){
	device = renderer->device.get();
	buffer_info = vk::BufferCreateInfo(
		vk::BufferCreateFlags(), sizeof(vertices[0]) * vertices.size(),
		vk::BufferUsageFlagBits::eVertexBuffer
	);

	device.createBuffer(&buffer_info, nullptr , &vertex_buffer);
	
	if (!vertex_buffer){
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, "Create Buffer Failed");
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan Error!", " Couldn't create Vertex Buffer.\n Make sure information is filled out correctly.", NULL);
		throw "Vertex Buffer Creation Failed!";
	}

	auto memory_reqs = device.getBufferMemoryRequirements(vertex_buffer);
	auto allocate_info = vk::MemoryAllocateInfo(
		memory_reqs.size,
		FindMemoryType(renderer, memory_reqs.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eHostVisible|vk::MemoryPropertyFlagBits::eHostCoherent
		)
	);
	buffer_memory = device.allocateMemory(allocate_info, nullptr);
	if (!buffer_memory){
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, "Create Buffer Failed");
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan Error!", " Couldn't create Vertex Buffer Memory.\n Make sure information is filled out correctly.", NULL);
		throw "Memory not bound for Vertex Buffer!";
	}
	device.bindBufferMemory(vertex_buffer, buffer_memory, 0);

	void * data;
	device.mapMemory(buffer_memory, 0, buffer_info.size, vk::MemoryMapFlags(), &data);
	memcpy(data, vertices.data(), (size_t)buffer_info.size);
	device.unmapMemory(buffer_memory);
}

VertexBuffer::~VertexBuffer(){
	device.destroyBuffer(vertex_buffer);
	device.freeMemory(buffer_memory);
}