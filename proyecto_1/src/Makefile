OBJECTS = main.o ../include/scanner.o ../include/disco.o ../include/mount.o ../include/shared.o ../include/filesystem.o ../include/filemanager.o ../include/report.o ../include/users.o

run: $(OBJECTS)
	g++ -std=c++17 -o $@ $(OBJECTS)

main.o: main.cpp
	g++ -std=c++17 -c main.cpp -I../lib

../include/scanner.o: ../include/scanner.cpp
	g++ -std=c++17 -c ../include/scanner.cpp -I../lib -o ../include/scanner.o

../include/disco.o: ../include/disco.cpp
	g++ -std=c++17 -c ../include/disco.cpp -I../lib -o ../include/disco.o

../include/mount.o: ../include/mount.cpp
	g++ -std=c++17 -c ../include/mount.cpp -I../lib -o ../include/mount.o

../include/shared.o: ../include/shared.cpp
	g++ -std=c++17 -c ../include/shared.cpp -I../lib -o ../include/shared.o

../include/filesystem.o: ../include/filesystem.cpp
	g++ -std=c++17 -c ../include/filesystem.cpp -I../lib -o ../include/filesystem.o

../include/filemanager.o: ../include/filemanager.cpp
	g++ -std=c++17 -c ../include/filemanager.cpp -I../lib -o ../include/filemanager.o

../include/report.o: ../include/report.cpp
	g++ -std=c++17 -c ../include/report.cpp -I../lib -o ../include/report.o

../include/users.o: ../include/users.cpp
	g++ -std=c++17 -c ../include/users.cpp -I../lib -o ../include/users.o

clean:
	rm -f run run.exe main.o ../include/*.o