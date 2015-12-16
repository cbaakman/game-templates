all: bin/client bin/server bin/manager bin/test3d

clean:
	rm -f bin/client bin/test3d bin/server bin/manager obj/*.o obj/*/*.o

CLIENTLIBS = SDL2 SDL2_net SDL2_mixer GL GLEW png crypto xml2 cairo unzip
SERVERLIBS = SDL2 SDL2_net crypto
MANAGERLIBS = crypto ncurses SDL2
TEST3DLIBS = GL SDL2 GLEW png xml2 cairo unzip

CFLAGS = -std=c++11
ifdef DEBUG
CFLAGS += -g -D DEBUG
else
CFLAGS += -O3
endif

include make.config
make.config:
	./configure

obj/%.o: src/%.cpp
	mkdir -p $(@D)
	gcc $(CFLAGS) -c $< -o $@ $(INCDIRS:%=-I%)

bin/server: obj/geo2d.o obj/str.o obj/ini.o obj/account.o obj/server/server.o obj/err.o
	gcc $^ -o $@ -lstdc++ $(SERVERLIBS:%=-l%) $(LIBDIRS:%=-L%)

bin/client: obj/geo2d.o obj/ini.o obj/client/client.o obj/util.o obj/client/connection.o obj/str.o obj/err.o obj/client/textscroll.o\
	obj/client/gui.o obj/client/login.o obj/texture.o obj/io.o obj/font.o obj/xml.o
	gcc $^ -o $@ $(CLIENTLIBS:%=-l%) $(LIBDIRS:%=-L%)

bin/test3d: obj/load.o obj/thread.o obj/progress.o obj/test3d/vecs.o obj/test3d/mapper.o obj/test3d/water.o obj/ini.o obj/test3d/hub.o obj/xml.o obj/str.o obj/test3d/shadow.o obj/shader.o obj/test3d/app.o obj/random.o obj/err.o\
	obj/io.o obj/texture.o obj/test3d/mesh.o obj/util.o obj/font.o obj/test3d/collision.o obj/test3d/toon.o
	gcc $^ -o $@ $(TEST3DLIBS:%=-l%) $(LIBDIRS:%=-L%)

bin/manager: obj/manager/manager.o obj/str.o obj/account.o obj/err.o
	gcc $^ -o $@ -lstdc++ $(MANAGERLIBS:%=-l%) $(LIBDIRS:%=-L%)
