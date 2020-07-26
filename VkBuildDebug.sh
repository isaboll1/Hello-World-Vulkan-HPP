#! /bin/sh

g++ -std=c++17 -DVK_DEBUG -Wall -Wextra src/*.cpp -o bin/VKEngineDEBUG.x86_64 -lSDL2 -lvulkan