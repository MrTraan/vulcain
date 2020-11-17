git submodule update --init --recursive
# Install SDL2 and OpenGL 4.2
brew install lz4 tinyxml2 glm tracy git-lfs
git lfs fetch
git lfs checkout

# Setup flags for lz4, put in your ~/.bashrc for example
export C_INCLUDE_PATH="$C_INCLUDE_PATH:/usr/local/Cellar/lz4/1.9.2/include"
export CPATH="$CPATH:/usr/local/Cellar/lz4/1.9.2/include"
export CXX_INCLUDE_PATH="$CXX_INCLUDE_PATH:/usr/local/Cellar/lz4/1.9.2/include"
export CXXPATH="$CXXINCLUDE_PATH:/usr/local/Cellar/lz4/1.9.2/include"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/local/Cellar/lz4/1.9.2/lib"
export DYLD_LIBRARY_PATH="$DYLD_LIBRARY_PATH:/usr/local/Cellar/lz4/1.9.2/lib"
export PATH="$PATH:/usr/local/Cellar/lz4/1.9.2/bin"