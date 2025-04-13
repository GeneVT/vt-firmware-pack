REPO_NAME := vt-firmware-pack

DIR := ${CURDIR}
GllIT_VERSION := $(patsubst v.%,%,$(shell git describe --tags))
CONTAINER_NAME := vt-firmware-pack
DOCKER_CMD := docker exec $(CONTAINER_NAME)
DOCKER_ENV := $(DIR)/.env
DEBIAN_PKG_DIR := debian
OUT_DIR := $(DIR)/out
TMP := $(shell mktemp -d)

SRC_REGEX := -name '*.cpp' -o -name '*.cc' -o -name '*.c'
SRCS = $(shell find $(SRC_DIR) -type f $(SRC_REGEX))


ifeq ($(ARCH),arm)
  CXX=arm-linux-gnueabihf-g++ -march=armv7-a -fno-tree-vectorize \
            -mthumb-interwork -mfloat-abi=hard -mfpu=neon -mtune=cortex-a8 \
            -pipe $(DEBUG_FLAGS)
  CC=arm-linux-gnueabihf-gcc -march=armv7-a -fno-tree-vectorize \
            -mthumb-interwork -mfloat-abi=hard -mfpu=neon -mtune=cortex-a8 \
            -pipe $(DEBUG_FLAGS)
  CFLAGS+=$(INCLUDES) $(LIBS_PATH) $(LIBS)
else
  CXX=g++
  CC=gcc
endif

.PHONY: build test build_container run_container deb_pkg all clean

all: build test deb_pkg

clean: $(OUT_DIR)
	rm -f $(OUT_DIR)/*.deb
	rm -f $(OUT_DIR)/build

$(OUT_DIR):
	mkdir -p $@



build: 
	@rm -rf build
	@mkdir build && cd build && cmake .. && make

test:
	@cd build && ctest --verbose

deb_pkg:
	@mkdir -p $(OUT_DIR)
	@mkdir -p $(TMP)
	set -e
	@echo $(TMP)
	install -d $(TMP)/DEBIAN
	install -D -t $(TMP)/DEBIAN $(DEBIAN_PKG_DIR)/*
	install -d $(TMP)/usr/bin/
	install -D -t $(TMP)/usr/bin $(OUT_DIR)/vt-shadow-publish
	install -d $(TMP)/tmp/
	install -D -t $(TMP)/tmp $(PWD)/vt-stable-firmware.json
	# sed -i -e "s/Version.*/Version: $(GIT_VERSION)/" $(TMP)/DEBIAN/control
	sed -i -e "s/Version.*/Version: 1.0.0/" $(TMP)/DEBIAN/control
	fakeroot dpkg-deb --build $(TMP) $(OUT_DIR)/$(DEBIAN_PKG)
	rm -rf $(TMP)

run_container: build_container kill
	docker run \
	--rm \
	--name=$(CONTAINER_NAME) \
	-it \
	-d \
	-v $(PWD):/root/$(REPO_NAME) \
	--privileged=true \
	$(REPO_NAME)

build_container:
	# Register Arm executables to run on x64 machines
	- docker run --rm --privileged multiarch/qemu-user-static:register

	docker build -t $(REPO_NAME) .

kill:
	docker kill $(CONTAINER_NAME) || true

debug:
	docker exec -it $(CONTAINER_NAME) bash

