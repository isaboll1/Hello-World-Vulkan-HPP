cd "`dirname "$0"`"
g++ -D VK_DEBUG -Wall -Wextra src/*.cpp -o bin/VKEngineDEBUG.x86_64 -lSDL2 -lvulkan
./bin/VKEngineDEBUG.x86_64
 exec bash