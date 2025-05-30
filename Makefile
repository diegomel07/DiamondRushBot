# Makefile para DiamondRush

CXX = g++
CXXFLAGS = -O2 -std=c++17 -fopenmp
LDFLAGS = -lX11 -fopenmp

# Archivos fuente
SRCS = procesamiento_imagen.cpp tomar_captura.cpp
OBJS = $(SRCS:.cpp=.o)

# Ejecutable final
TARGET = diamondrush

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)