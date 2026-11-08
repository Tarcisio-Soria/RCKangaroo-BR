CC := g++
NVCC := /usr/bin/nvcc
CUDA_PATH ?= /usr

CCFLAGS := -O3 -I$(CUDA_PATH)/include
# Otimizado para a RTX 3060 (Ampere, sm_86): gera SASS nativo (code=sm_86),
# evitando o JIT de PTX no primeiro arranque e maximizando a performance.
NVCCFLAGS := -O3 -gencode=arch=compute_86,code=sm_86
LDFLAGS := -L$(CUDA_PATH)/lib/x86_64-linux-gnu -lcudart -lnvidia-ml -pthread

CPU_SRC := RCKangaroo.cpp GpuKang.cpp Ec.cpp utils.cpp
GPU_SRC := RCGpuCore.cu

CPP_OBJECTS := $(CPU_SRC:.cpp=.o)
CU_OBJECTS := $(GPU_SRC:.cu=.o)

TARGET := rckangaroo

all: $(TARGET)

$(TARGET): $(CPP_OBJECTS) $(CU_OBJECTS)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CCFLAGS) -c $< -o $@

%.o: %.cu
	$(NVCC) $(NVCCFLAGS) -c $< -o $@

clean:
	rm -f $(CPP_OBJECTS) $(CU_OBJECTS)
