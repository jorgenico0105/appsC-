TARGET = index
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 \
    -I/usr/local/include \
    -I/usr/local/include/OpenXLSX \
    -I/usr/local/include/OpenXLSX/headers \
    -I/usr/include

LDFLAGS = -L/usr/local/lib -Wl,-rpath=/usr/local/lib
LIBS = -lmysqlcppconn -lOpenXLSX

SRC = index.cpp

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
