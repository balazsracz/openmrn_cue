-include ../../config.mk
export TARGET := js.emscripten
include $(OPENMRNPATH)/etc/prog.mk
LDFLAGS += --bind -s DEMANGLE_SUPPORT=1 -s WASM=0

# How to prepare for releasing this:
# as administrator do
# npm install -g pkg
# then you can call make release
release:
	pkg -C Brotli .
