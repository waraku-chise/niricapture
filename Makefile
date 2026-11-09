BUILD_DIR ?= build
BUILD_TYPE ?= Release
PREFIX ?= $(HOME)/.local
GENERATOR ?= Ninja

CMAKE ?= cmake
CLANG_TIDY ?= clang-tidy
PYTHON ?= python3

LINT_SOURCES := \
	src/main.cpp src/capture.cpp src/desktop-capture.cpp \
	src/region-selector.cpp src/editor-window.cpp src/surface-capture.cpp \
	src/cli-path.cpp src/icons.cpp src/pin.cpp src/pin-file.cpp \
	src/pin-layout.cpp src/instance-lock.cpp
LINT_CHECKS ?= -*,clang-analyzer-*,bugprone-*

.PHONY: all configure build clean install uninstall smoke desktop-smoke \
	contracts lint check

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX)

build: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel

desktop-smoke: build
	QT_QPA_PLATFORM=offscreen "$(BUILD_DIR)/niricapture-desktop-smoke"

smoke: build
	QT_QPA_PLATFORM=offscreen "$(BUILD_DIR)/niricapture-smoke" \
		"$(BUILD_DIR)/niricapture-smoke-output"
	$(MAKE) desktop-smoke

lint: build
	@set -eu; \
	if command -v "$(CLANG_TIDY)" >/dev/null 2>&1; then \
		commands_dir=$$(mktemp -d); \
		trap 'rm -rf "$$commands_dir"' EXIT; \
		cp "$(BUILD_DIR)/compile_commands.json" \
			"$$commands_dir/compile_commands.json"; \
		sed -i 's/ -mno-direct-extern-access//g' \
			"$$commands_dir/compile_commands.json"; \
		for source in $(LINT_SOURCES); do \
			"$(CLANG_TIDY)" -p "$$commands_dir" \
				-checks="$(LINT_CHECKS)" -header-filter='.*' "$$source"; \
		done; \
	else \
		echo "make lint: clang-tidy unavailable; skipping"; \
	fi

contracts:
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py' -v

check: build
	$(MAKE) contracts
	$(MAKE) smoke

clean:
	@if test -d "$(BUILD_DIR)"; then \
		$(CMAKE) --build "$(BUILD_DIR)" --target clean; \
	fi

install: build
	$(CMAKE) --install $(BUILD_DIR)

uninstall:
	./packaging/uninstall-local.sh --prefix "$(PREFIX)"
