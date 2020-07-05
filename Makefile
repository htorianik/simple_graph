CXX_OPTIONS = -std=c++17 -Wall
INCLUDE = /usr/local/Cellar/sdl2/2.0.12_1/include
LIBRARY = /usr/local/Cellar/sdl2/2.0.12_1/lib
LINK = -lSDL2

app : sdl_test.o
	g++ -L$(LIBRARY) $(LINK) -o bin/app bin/sdl_test.o 

sdl_test.o : main.cxx 
	g++ -c $(CXX_OPTIONS) -I$(INCLUDE) -o bin/sdl_test.o main.cxx

clean:
	rm bin/*
