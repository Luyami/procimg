cppfiles = ./cpp
headers  = ./headers
bin      = ./bin
util     = ./util
IMGUI = ./imgui
IMGUI_BACKENDS = $(IMGUI)/backends

all:
	# ImGui core
	g++ -I$(headers) -I$(util) -I$(IMGUI) -I$(IMGUI_BACKENDS) -c $(IMGUI)/imgui.cpp -o $(bin)/imgui.o
	g++ -I$(headers) -I$(util) -I$(IMGUI) -I$(IMGUI_BACKENDS) -c $(IMGUI)/imgui_draw.cpp -o $(bin)/imgui_draw.o
	g++ -I$(headers) -I$(util) -I$(IMGUI) -I$(IMGUI_BACKENDS) -c $(IMGUI)/imgui_tables.cpp -o $(bin)/imgui_tables.o
	g++ -I$(headers) -I$(util) -I$(IMGUI) -I$(IMGUI_BACKENDS) -c $(IMGUI)/imgui_widgets.cpp -o $(bin)/imgui_widgets.o

	# ImGui backends
	g++ -I$(headers) -I$(util) -I$(IMGUI) -I$(IMGUI_BACKENDS) -c $(IMGUI_BACKENDS)/imgui_impl_glfw.cpp -o $(bin)/imgui_impl_glfw.o
	g++ -I$(headers) -I$(util) -I$(IMGUI) -I$(IMGUI_BACKENDS) -c $(IMGUI_BACKENDS)/imgui_impl_opengl3.cpp -o $(bin)/imgui_impl_opengl3.o

	g++ -g -I./headers -I$(util) -I$(IMGUI) -c ./cpp/engine.cpp -o ./bin/engine.o
	g++ -g -I./headers -I$(util) -I$(IMGUI) -c ./cpp/fft.cpp -o ./bin/fft.o
	g++ -g -I./headers -I$(util) -I$(IMGUI) -c ./cpp/engine_input.cpp -o ./bin/engine_input.o
	g++ -g -I./headers -I$(util) -I$(IMGUI) -c ./cpp/engine_gui.cpp -o ./bin/engine_gui.o

	ar rcs ./bin/libmain.a ./bin/engine_input.o ./bin/engine_gui.o ./bin/engine.o ./bin/fft.o \
		$(bin)/imgui.o $(bin)/imgui_draw.o $(bin)/imgui_tables.o $(bin)/imgui_widgets.o $(bin)/imgui_impl_glfw.o $(bin)/imgui_impl_opengl3.o
	g++ -g -I./headers -I$(util) -I$(IMGUI) -L./bin main.cpp -o main -lmain -lglfw -lGLEW -lGL