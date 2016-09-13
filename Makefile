all: bin/client bin/server bin/manager bin/test3d

clean:
	rm -f bin/client bin/test3d bin/server bin/manager obj/*.o obj/*/*.o

CLIENTLIBS = SDL2 SDL2_net SDL2_mixer GL GLEW png crypto xml2 cairo unzip
SERVERLIBS = SDL2 SDL2_net crypto unzip
MANAGERLIBS = crypto ncurses SDL2
TEST3DLIBS = GL SDL2 GLEW png xml2 cairo unzip

BINDIR = /usr/local/bin

CC = /usr/bin/g++
CCVERSION = $(strip $(shell $(CC) --version | grep -o ' [0-9\.]\+$$'))

# The 4.8 compiler only knows c++1y, but it's deprecated
CFLAGS = -pthread
ifeq ($(shell echo $(CCVERSION) | cut -c1-3), 4.8)
CFLAGS += -std=c++1y
else
CFLAGS += -std=c++14
endif

ifdef DEBUG
CFLAGS += -g -D DEBUG
else
#CFLAGS += -O3

CONFDIR = /usr/local/etc
RESDIR = /usr/local/share
CFLAGS += -D CONFDIR=\"$(CONFDIR)\" -D RESDIR=\"$(RESDIR)\"
endif

include make.config
make.config:
	./configure

obj/%.o: src/%.cpp
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@ $(INCDIRS:%=-I%)

bin/server: obj/thread.o obj/str.o obj/ini.o obj/account.o obj/server/server.o \
	obj/err.o obj/http.o obj/io.o
	$(CC) $^ -o $@ $(SERVERLIBS:%=-l%) $(LIBDIRS:%=-L%)

bin/client: obj/thread.o obj/ini.o obj/client/client.o obj/GLutil.o \
	obj/client/connection.o obj/str.o obj/err.o obj/client/textscroll.o\
	obj/client/gui.o obj/client/login.o obj/texture.o obj/io.o obj/font.o obj/xml.o
	$(CC) $^ -o $@ $(CLIENTLIBS:%=-l%) $(LIBDIRS:%=-L%)

bin/test3d: obj/test3d/chunk.o obj/test3d/grass.o obj/load.o obj/thread.o\
	obj/progress.o obj/test3d/vecs.o obj/test3d/mapper.o obj/test3d/water.o\
	obj/ini.o obj/test3d/hub.o obj/xml.o obj/str.o obj/test3d/shadow.o \
	obj/shader.o obj/test3d/app.o obj/random.o obj/err.o obj/io.o obj/texture.o \
	obj/test3d/mesh.o obj/util.o obj/GLutil.o obj/font.o obj/test3d/collision.o \
	obj/test3d/toon.o
	$(CC) $^ -o $@ $(TEST3DLIBS:%=-l%) $(LIBDIRS:%=-L%)

bin/manager: obj/manager/manager.o obj/ini.o obj/str.o obj/account.o obj/err.o
	$(CC) $^ -o $@ -lstdc++ $(MANAGERLIBS:%=-l%) $(LIBDIRS:%=-L%)

ifndef DEBUG
install: bin/server bin/manager bin/client bin/test3d
	mkdir -m755 -p $(CONFDIR)/client $(CONFDIR)/server $(CONFDIR)/server/accounts \
        $(RESDIR)
	install -m755 bin/server $(BINDIR)/server
	install -m755 bin/manager $(BINDIR)/manager
	install -m755 bin/client $(BINDIR)/client
	install -m755 bin/test3d $(BINDIR)/test3d
	/bin/echo -e 'max-login=10\nport=12000\naccounts=$(CONFDIR)/server/accounts' \
            > $(CONFDIR)/server.ini
	/bin/echo -e 'host=localhost\nport=12000\nscreenwidth=800\nscreenheight=600\nfullscreen=0' \
            > $(CONFDIR)/client.ini
	/bin/echo -e 'screenwidth=800\nscreenheight=600\nfullscreen=0' \
            > $(CONFDIR)/test3d.ini
	install -m644 bin/server.zip $(RESDIR)/server.zip
	install -m644 bin/client.zip $(RESDIR)/client.zip
	install -m644 bin/test3d.zip $(RESDIR)/test3d.zip
	install -m644 client.desktop /usr/share/applications/client.desktop
	install -m644 test3d.desktop /usr/share/applications/test3d.desktop
	install -m644 art/client-icon.svg /usr/share/icons/hicolor/scalable/apps/client.svg
	install -m644 art/dummy-icon.svg /usr/share/icons/hicolor/scalable/apps/test3d.svg
	sed -e "s|__DEAMON__|$(BINDIR)/server|g" init.d/game-templates-server > /etc/init.d/game-templates-server
	chmod 755 /etc/init.d/game-templates-server

uninstall:
	rm -rf $(BINDIR)/server $(BINDIR)/account $(BINDIR)/client $(BINDIR)/test3d \
           $(CONFDIR)/server.ini $(CONFDIR)/client.ini $(CONFDIR)/test3d.ini \
           $(RESDIR)/server.zip $(RESDIR)/client.zip $(RESDIR)/test3d.zip \
           /etc/init.d/game-templates-server \
           /usr/share/applications/test3d.desktop /usr/share/icons/hicolor/scalable/apps/test3d.svg \
           /usr/share/applications/client.desktop /usr/share/icons/hicolor/scalable/apps/client.svg

endif
