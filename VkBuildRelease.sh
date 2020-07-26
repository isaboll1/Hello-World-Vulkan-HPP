#! /bin/sh

g++ -std=c++17 -Wall -Wextra src/*.cpp -o bin/VKEngine.x86_64 -lSDL2 -lvulkan
