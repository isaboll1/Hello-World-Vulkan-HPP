# Hello-World-Vulkan-HPP
A Vulkan "Hello World" HPP program i've been working with following tutorials and junk. Rewritten in Vulkan HPP.
Very similar code to the other project I have for the same purpose that was instead written using regular Vulkan C-style code.

The project currently incorporates the Vulkan Memory Allocator (c++ bindings like Vulkan HPP) to help with using device memory (much easier!), although there is a version without using that library.

On linux, if you don't want to use VSCode, you can use the
shell script named "VkCompileAndRun.sh" to compile and run the program. If the program has already been compiled, you can run
"VKEngine.sh" instead.

Make sure you have both the Vulkan SDK and SDL2 installed to run this.
