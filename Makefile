
#CXX=/usr/local/cuda/bin/nvcc
CXX=g++
NVCC=/usr/local/cuda/bin/nvcc

nvcc_ARCH := -gencode=arch=compute_61,code=\"sm_61,compute_61\"
nvcc_ARCH += -gencode=arch=compute_52,code=\"sm_52,compute_52\"
nvcc_ARCH += -gencode=arch=compute_35,code=\"sm_35,compute_35\"

nvcc_FLAGS = $(nvcc_ARCH) -I/usr/local/cuda/include -I. -Xcompiler -Wdeprecated-declarations \
    $(JANSSON_INCLUDES) -O2

LD_FLAGS += -lpthread -L. -lsecp256k1 -L/usr/local/cuda/lib64 -L/usr/local/cuda/lib64/stubs

miner_OBJS = \
	main.o \
	rpc.o \
	miner.o \
	submiter.o \
	block.o \
	uint256.o \
	transaction.o \
	utilstrencodings.o \
	amount.o \
	hash.o \
	sha256.o \
	script.o \
	cleanse.o \
	base58.o \
	standard.o \
	pubkey.o \
	ripemd160.o \
	hmac_sha512.o \
	sha512.o \
	blake2b.o \
	arith_uint256.o \
	mean.o \
	interpreter.o \
	sha1.o

all: $(miner_OBJS)
	$(CXX) -o bfcminer $^ $(LD_FLAGS) -Lcompat/curl-for-linux -lcurl -Wl,-Bstatic -ljsoncpp -Wl,-Bstatic -lcrypto -lz -lcudart_static -Wl,-Bdynamic -lcuda -ldl -lrt

%.o : %.cu
	$(NVCC)  -std=c++11 $(nvcc_FLAGS) -c $< -o $@

%.o : %.cpp
	$(CXX) -std=c++11 -O2 -Icompat/curl-for-windows -I/usr/local/cuda/include -c $< -o $@


clean:
	rm -rf *.o
