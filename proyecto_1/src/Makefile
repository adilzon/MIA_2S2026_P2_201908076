CXX = g++
CXXFLAGS = -std=c++17 -pthread
LDFLAGS = -pthread

# En entornos Windows agrega ws2_32; en Linux permanece -pthread
ifeq ($(OS),Windows_NT)
    LDFLAGS = -lpthread -lws2_32
endif

OBJECTS = main.o ../include/scanner.o ../include/disco.o ../include/mount.o ../include/shared.o ../include/filesystem.o ../include/filemanager.o ../include/report.o ../include/users.o
API_OBJECTS = api_main.o ../include/scanner.o ../include/disco.o ../include/mount.o ../include/shared.o ../include/filesystem.o ../include/filemanager.o ../include/report.o ../include/users.o

all: run api

run: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

api: $(API_OBJECTS)
	$(CXX) $(CXXFLAGS) -o api_server $(API_OBJECTS) $(LDFLAGS)

api_main.o: api_main.cpp
	$(CXX) $(CXXFLAGS) -c api_main.cpp -I../lib

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp -I../lib

../include/scanner.o: ../include/scanner.cpp
	$(CXX) $(CXXFLAGS) -c ../include/scanner.cpp -I../lib -o ../include/scanner.o

../include/disco.o: ../include/disco.cpp
	$(CXX) $(CXXFLAGS) -c ../include/disco.cpp -I../lib -o ../include/disco.o

../include/mount.o: ../include/mount.cpp
	$(CXX) $(CXXFLAGS) -c ../include/mount.cpp -I../lib -o ../include/mount.o

../include/shared.o: ../include/shared.cpp
	$(CXX) $(CXXFLAGS) -c ../include/shared.cpp -I../lib -o ../include/shared.o

../include/filesystem.o: ../include/filesystem.cpp
	$(CXX) $(CXXFLAGS) -c ../include/filesystem.cpp -I../lib -o ../include/filesystem.o

../include/filemanager.o: ../include/filemanager.cpp
	$(CXX) $(CXXFLAGS) -c ../include/filemanager.cpp -I../lib -o ../include/filemanager.o

../include/report.o: ../include/report.cpp
	$(CXX) $(CXXFLAGS) -c ../include/report.cpp -I../lib -o ../include/report.o

../include/users.o: ../include/users.cpp
	$(CXX) $(CXXFLAGS) -c ../include/users.cpp -I../lib -o ../include/users.o

clean:
ifeq ($(OS),Windows_NT)
	-cmd /c del /f /q run run.exe api_server api_server.exe *.o ..\include\*.o 2>nul
else
	rm -f run run.exe api_server api_server.exe *.o ../include/*.o
endif