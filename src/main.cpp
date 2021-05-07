#include "renderer.h"
#include <cmath>

constexpr double PI = 3.14159265358979323846;
constexpr double CIRCLE_RAD = PI * 2;
constexpr double CIRCLE_THIRD = CIRCLE_RAD / 3.0;
constexpr double CIRCLE_THIRD_1 = 0;
constexpr double CIRCLE_THIRD_2 = CIRCLE_THIRD;
constexpr double CIRCLE_THIRD_3 = CIRCLE_THIRD * 2;

int WIDTH, HEIGHT;

int main(int argc, char ** argv) //Equivalent to WinMain() on Windows, this is the entry point.
{
	SDL_Init(SDL_INIT_VIDEO);       //This activates a specific SDL2 subsystem  

	//Forward Declerations
	SDL_Event event;          //This is the handle for the event subsystem
	SDL_Window * window;      //This is a handle for the window
	VkRenderer * renderer;    //This is a handle for the renderer
	WIDTH = 640, HEIGHT = 480;
	bool running = true;

	//Creating a window
	 window = SDL_CreateWindow("Vulkan Application", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE);

	if (!window){
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan Application Error!", SDL_GetError(), NULL);
		return -1;
	}

	//Creating a renderer
	 renderer = new VkRenderer(window);

	//Variables
	float dt = 0.0f;
	float rotator = 0.0f;

	//Triangle Vertices and Vertex Buffer
	vector<Vertex> vertices = {
		//{{position}, {color}}
		{{0.0f, -0.5f},{0.0f, 0.0f, 0.0f}}, 
		{{0.5f,  0.5f},{0.0f, 0.0f, 0.0f}},
		{{-0.5,  0.5f},{0.0f, 0.0f, 0.0f}},
	};

	auto triangle = new VertexBuffer(vertices, renderer);

	//Command Buffers
	auto command_buffers = renderer->GetCommandBuffers(vk::CommandBufferLevel::ePrimary, renderer->buffer_count);

	uint32_t i = 0;

	//Vertex Buffer

	//Triangle Pipeline Creation
	{
		auto vertex_shader = renderer->LoadShaderModule("shaders/triangle.vert.spv");
		auto fragment_shader = renderer->LoadShaderModule("shaders/triangle.frag.spv");

		vector<vk::PipelineShaderStageCreateInfo> shader_stages = {
			vk::PipelineShaderStageCreateInfo(       //VERTEX SHADER
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eVertex,
				vertex_shader, "main"),

			vk::PipelineShaderStageCreateInfo(      //FRAGMENT SHADER
				vk::PipelineShaderStageCreateFlags(), 
				vk::ShaderStageFlagBits::eFragment, 
				fragment_shader, "main")
		};

		const vk::VertexInputBindingDescription vertex_binding_description = Vertex::GetBindingDescription();
		const vector<vk::VertexInputAttributeDescription> vertex_attribute_description = Vertex::GetAttributeDescription();
		auto vertex_input_info = 
		vk::PipelineVertexInputStateCreateInfo(
			vk::PipelineVertexInputStateCreateFlags(),
			1, &vertex_binding_description,
			static_cast<uint32_t>(Vertex::GetAttributeDescription().size()),
			vertex_attribute_description.data()
		);

		auto input_assembly = 
		vk::PipelineInputAssemblyStateCreateInfo(
			vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleList,
			VK_FALSE
		);

		//Viewport and Scissor
		renderer->viewports.push_back(vk::Viewport(0, 0, WIDTH, HEIGHT, 0, 1.0f));
		renderer->scissors.push_back(vk::Rect2D(vk::Offset2D(), vk::Extent2D(WIDTH, HEIGHT)));

		auto viewport_state = vk::PipelineViewportStateCreateInfo(
			vk::PipelineViewportStateCreateFlags(), 
			renderer->viewports.size(),
			renderer->viewports.data(),
			renderer->scissors.size(),
			renderer->scissors.data()
		);

		//Rasterizer
		renderer->rasterizer.setDepthClampEnable(VK_FALSE);
		renderer->rasterizer.setRasterizerDiscardEnable(VK_FALSE);
		renderer->rasterizer.setPolygonMode(vk::PolygonMode::eFill);
		renderer->rasterizer.setLineWidth(1.0f);
		renderer->rasterizer.setCullMode(vk::CullModeFlagBits::eBack);
		renderer->rasterizer.setFrontFace(vk::FrontFace::eClockwise);
		renderer->rasterizer.setDepthBiasEnable(VK_FALSE);

		//Multisampling
		renderer->multisampler.setSampleShadingEnable(VK_FALSE);
		//Samples
		renderer->multisampler.setMinSampleShading(1.0); //minSampleShading
		//SampleMask
		//AlphaToCoverage
		//AlphaToOne
		//...Everything else just needs the defaults, but I labled where I would add the options

		//Depth and Stencil Tests
		auto depth_stencil_tests = vk::PipelineDepthStencilStateCreateInfo();
		//...Default is no tests

		//Color Blending - (The fragment shader's color needs to be combined with a color in a framebuffer)
		vector<vk::PipelineColorBlendAttachmentState> blend_attachments = {
			vk::PipelineColorBlendAttachmentState(
				VK_TRUE, vk::BlendFactor::eSrcAlpha,		   //blendEnable, srcColorBlendFactor
				vk::BlendFactor::eZero, vk::BlendOp::eAdd,	   //dstColorBlendFacctor, colorBlendOp
				vk::BlendFactor::eOne, vk::BlendFactor::eZero, //srcAlphaBlendFactor, dstAlphaBlendFactor
				vk::BlendOp::eAdd,							   //alphaBlendOp,
				vk::ColorComponentFlagBits::eR |			   //ColorComponentFlags
				vk::ColorComponentFlagBits::eG |
				vk::ColorComponentFlagBits::eB |
				vk::ColorComponentFlagBits::eA) 
		};

		auto color_blend = 
		vk::PipelineColorBlendStateCreateInfo(
			vk::PipelineColorBlendStateCreateFlags(),
			VK_FALSE, vk::LogicOp::eCopy, //LogicOpEnable, LogicOp
			blend_attachments.size(),     //AttachmentCount
			blend_attachments.data()      //Attachments
		);
		
		//Dynamic States
		vector<vk::DynamicState> dynamic_states = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor,
			vk::DynamicState::eLineWidth,
		};

		auto pipeline_dynamic_states = 
		vk::PipelineDynamicStateCreateInfo(
			vk::PipelineDynamicStateCreateFlags(),
			dynamic_states.size(),
			dynamic_states.data()
		);

		//Pipeline Layout
		auto pipeline_layout_info = vk::PipelineLayoutCreateInfo();
		// ...Defaults are nothing, which is perfect as descriptor layouts aren't being used yet

		renderer->pipeline_layout = renderer->device->createPipelineLayout(pipeline_layout_info).value;

		//Pipeline Object
		renderer->pipelines["Triangle"] =
			renderer->device->createGraphicsPipeline(
				nullptr, 
				vk::GraphicsPipelineCreateInfo(
					vk::PipelineCreateFlags(), 
					shader_stages.size(), shader_stages.data(),    //stageCount, shaderStages
					&vertex_input_info, &input_assembly, nullptr,  //vertexInputState, inputAssemblyState, tesselationState
					&viewport_state, &renderer->rasterizer,        //viewportState, rasterizationState
					&renderer->multisampler, &depth_stencil_tests, //multisampleState, depthStencilTests, 
					&color_blend, &pipeline_dynamic_states,        //colorblendState, pipelineDynamicStates,
					renderer->pipeline_layout, renderer->renderpass,//pipelineLayout, renderPass
					0, nullptr, -1)                                             //subPass
		).value;

		renderer->DestroyShaderModule("shaders/triangle.vert.spv");
		renderer->DestroyShaderModule("shaders/triangle.frag.spv");
	}

	//FPS Stuff
	double last_time = SDL_GetTicks() / 1000;
	float last_frame = SDL_GetTicks();
	int number_of_frames = 0;
	bool print_fps = true;
	double current_time = last_time;
	
	//Event System
	 while (running) {
		// Framerate Stuff
		if (print_fps) {
			number_of_frames++;
			if (current_time - last_time >= 1.0) {
				printf("\n Framerate: %f \n", 1.0 * (double)number_of_frames);
				number_of_frames = 0;
				last_time += 1.0;
			}
		}
		// Framerate Calculations
		float current_frame = SDL_GetTicks();
		dt = (current_frame - last_frame) / 1000;
		last_frame = current_frame;

		//Event Loop
		while (SDL_PollEvent(&event)){
			if (event.type == SDL_QUIT) {
				running = false;
				break;
			}

			if (event.type == SDL_WINDOWEVENT) {
				if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {

					// SDL_GetDrawableSize is used because, if High DPI support is queried for any platforms like MacOS (using MoltenVK)
					// or Windows, or Linux, we want to be able to get the right size of the display window for rendering.
					int draw_w, draw_h;
					SDL_Vulkan_GetDrawableSize(window, &draw_w, &draw_h);

					renderer->ResizeViewports(draw_w, draw_h);
					renderer->ResizeScissors(draw_w, draw_h);
					renderer->ResizeSwapchain();
				}
			}
		}
		//Game Logic
		//...nothing's here... :p

		 //Rendering Loop
		 if (renderer->AcquireNextBuffer(i)) { //AcquireNextBuffer: acquires the next command buffer index, used for setting the render commands for the next swapchain.
		 									  //It also returns wether the buffer is available or not, in which case that determines if the command buffer is filled.
			 current_time = SDL_GetTicks()/1000;

			 rotator += 0.001;

			 vector<vk::ClearValue> clear_color = {
				 vk::ClearColorValue(), // {depthStencil} indexed by attachments
				 // The clear color passed to the swapchain when it renders
				 vk::ClearColorValue(array<float, 4>{
					 float(sin(rotator + CIRCLE_THIRD_1) * 0.5 + 0.5), //R  
					 float(sin(rotator + CIRCLE_THIRD_2) * 0.5 + 0.5), //G 
					 float(sin(rotator + CIRCLE_THIRD_3) * 0.5 + 0.5), //B  
					 1.0f}), //A
			 };

			 command_buffers[i].begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
			 command_buffers[i].beginRenderPass(
				 vk::RenderPassBeginInfo(
					renderer->renderpass,
					renderer->frame_buffers[i],
					renderer->render_area,
					clear_color.size(),
					clear_color.data())
				, vk::SubpassContents::eInline);

			 //at this point drawing commands can begin...
			command_buffers[i].setViewport(0, renderer->viewports);
			command_buffers[i].setScissor(0, renderer->scissors);
			command_buffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, renderer->pipelines["Triangle"]);
			vector<vk::Buffer> vertex_buffers = {triangle->vertex_buffer};
			vector<vk::DeviceSize> offsets = {0};
			command_buffers[i].bindVertexBuffers(0, vertex_buffers, offsets);
			command_buffers[i].draw(vertices.size(), 1, 0, 0);

			//...up until this point
			command_buffers[i].endRenderPass();
			command_buffers[i].end();


		//The function below sends the command buffer to the graphics queue to begin the rendering process,
		//and when rendering is finished, it begins to present the rendered image to the corresponding swapchain
		renderer->BeginRenderPresent(i, command_buffers);
		 }
	 }
	renderer->device->waitIdle();
	delete triangle;
	delete renderer;
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}