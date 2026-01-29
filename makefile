CC = gcc
MODE ?= release

TARGET = calendar.out
BUILD_FOLDER = build

CPPFLAGS = -Iexternal/HttpServer/external/cJSON -Iexternal/HttpServer/include -Iinclude
DEBUG_LDFLAGS   = -fsanitize=address,undefined
RELEASE_LDFLAGS =
DEBUG_CFLAGS   = -g3 -O0 -Wall -Wextra -fsanitize=address,undefined
RELEASE_CFLAGS = -O4 -Wall -Wextra

HTTP_SO = external/HttpServer/libhttp.so
CJSON_SO = external/HttpServer/external/cJSON/libcjson.so

CERT = cert.pem
KEY = key.pem

ifeq ($(MODE),debug)
	CFLAGS = $(DEBUG_CFLAGS)
	LDFLAGS = $(DEBUG_LDFLAGS)
else
	CFLAGS = $(RELEASE_CFLAGS)
	LDFLAGS = $(DEBUG_LDFLAGS)
endif

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c, $(BUILD_FOLDER)/%.o, $(SRC))

default: $(HTTP_SO) $(TARGET) $(EXAMPLE_TARGET) $(CERT) $(KEY)

$(CERT) $(KEY):
	$(MAKE) -C external/HttpServer cert
	cp external/HttpServer/cert.pem .
	cp external/HttpServer/key.pem .

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(BUILD_FOLDER)/$@ $(LDFLAGS) \
	-L. -lcjson -lhttp \
	-Wl,-rpath,'$$ORIGIN'
	cp $(BUILD_FOLDER)/$(TARGET) .

$(HTTP_SO) $(CJSON_SO):
	git submodule update --init --recursive
	$(MAKE) -C external/HttpServer
	cp $(HTTP_SO) .
	cp -L $(CJSON_SO) .

$(BUILD_FOLDER)/%.o: src/%.c | $(BUILD_FOLDER)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_FOLDER):
	mkdir -p $@

clean:
	$(MAKE) -C external/HttpServer clean
	rm -rf $(BUILD_FOLDER)
	rm -f libcjson.so
	rm -f libhttp.so
	rm -f $(TARGET)
