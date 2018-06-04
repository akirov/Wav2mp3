###############################################################################
# @author Assen Kirov                                                         #
###############################################################################

# WARNING: If the path contains spaces you need to escape them manually!
ROOT_DIR=$(CURDIR)

SRC_DIR=$(ROOT_DIR)/src
INCLUDE_DIRS=$(ROOT_DIR)/include

# Set LAME library location, if it is not standard
ifeq ($(OS),Windows_NT)
  ifeq (,$(findstring CYGWIN, $(shell uname)))
    # Windows, but not Cygwin (MinGW)
    BUILD_DIR=$(ROOT_DIR)/build/windows
    INCLUDE_DIRS += $(ROOT_DIR)/../lame-3.100-mingw530_32/include
    LDFLAGS += -L$(ROOT_DIR)/../lame-3.100-mingw530_32/lib
  else
    # Cygwin
    BUILD_DIR=$(ROOT_DIR)/build/cygwin
    LDFLAGS += -L$(ROOT_DIR)/../lame-3.100-cygwin2884_32/lib
  endif
else
  BUILD_DIR=$(ROOT_DIR)/build/linux
endif


TARGET=$(BUILD_DIR)/wav2mp3
SOURCES=$(wildcard $(SRC_DIR)/*.cpp)
HEADERS=$(wildcard $(ROOT_DIR)/include/*.h)
OBJS=$(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)


CXX=g++
CXXFLAGS += -g -Wall
CPPFLAGS += $(foreach includedir,$(INCLUDE_DIRS),-I$(includedir))
#CXXFLAGS += -std=c++11
#CPPFLAGS += -DUSE_CPP11_THREADS
RM = rm -f
#LDLIBS += -lpthread -lmp3lame
#LDLIBS += -static -lpthread -lmp3lame
LDLIBS += -Wl,-Bstatic -lmp3lame -Wl,-Bdynamic -lpthread


.PHONY: all clean


all: $(TARGET)


$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LDFLAGS) $(LDLIBS)


$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@


clean:
	$(RM) $(OBJS) $(TARGET)
