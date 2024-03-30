add_library(MyVulkan 
Vulkan.cpp 
Buffer.cpp 
CommandBuffer.cpp 
CommandPool.cpp 
DescriptorPool.cpp 
DescriptorSetLayout.cpp 
Fence.cpp 
FrameBuffer.cpp 
Image.cpp 
Pipeline.cpp 
PipelineLayout.cpp 
RenderPass.cpp 
Semaphore.cpp 
ShaderModule.cpp 
SwapChain.cpp 
Vertex.cpp 
Sampler.cpp
Camera.cpp
Timer.cpp
Font.cpp
Editor.cpp
LineNumber.cpp
Grammar.cpp
Keyboard.cpp
Tools.cpp
CommandLine.cpp
)

target_link_libraries(MyVulkan vulkan-1 glfw3dll freetype)

add_executable(Main main.cpp)
target_link_libraries(Main MyVulkan vulkan-1 glfw3dll freetype)

add_executable(Test test.cpp)
target_link_libraries(Test MyVulkan)