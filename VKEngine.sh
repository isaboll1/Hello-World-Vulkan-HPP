#!/bin/bash

cd "`dirname "$0"`"
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./bin/
exec ./bin/VKEngine.x86_64
