HOST_CXX ?= g++
HOST_CXXFLAGS ?= -std=c++11 -O2 -g -Wall -Wextra -pedantic
HOST_TEST_DIR ?= build-tests
HOST_AUDIO_LIBS ?= -lvorbisidec -logg

TESTS := logic party_config mesh arena_cache icon_loader page_navigation hit_presentation sfx
TEST_BINS := $(addprefix $(HOST_TEST_DIR)/,$(TESTS))
HEADERS := $(wildcard source/*.h tests/stubs/*.h tests/audio_stubs/*.h)

.PHONY: test
test: $(TEST_BINS) $(HOST_TEST_DIR)/runtime/.ready
	@set -e; for test in $(filter-out $(HOST_TEST_DIR)/sfx,$(TEST_BINS)); do echo "Running $$test"; "$$test"; done
	@cd $(HOST_TEST_DIR)/runtime && ../sfx

$(HOST_TEST_DIR)/runtime/.ready:
	mkdir -p $(HOST_TEST_DIR)/runtime
	ln -s $(abspath romfs) '$(HOST_TEST_DIR)/runtime/romfs:'
	touch $@

$(HOST_TEST_DIR):
	mkdir -p $@

$(HOST_TEST_DIR)/logic: tests/logic_test.cpp source/combat.cpp source/dialogue.cpp $(HEADERS) | $(HOST_TEST_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $(filter %.cpp,$^) -o $@

$(HOST_TEST_DIR)/party_config: tests/party_config_test.cpp source/party_config.cpp source/combat.cpp $(HEADERS) | $(HOST_TEST_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $(filter %.cpp,$^) -o $@

$(HOST_TEST_DIR)/mesh: tests/mesh_test.cpp $(HEADERS) | $(HOST_TEST_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $< -o $@

$(HOST_TEST_DIR)/arena_cache: tests/arena_cache_test.cpp $(HEADERS) | $(HOST_TEST_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) -Itests/stubs $< -o $@

$(HOST_TEST_DIR)/icon_loader: tests/icon_loader_test.cpp source/icon_loader.cpp source/third_party/lodepng.cpp source/third_party/lodepng.h $(HEADERS) | $(HOST_TEST_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $(filter %.cpp,$^) -o $@

$(HOST_TEST_DIR)/page_navigation: tests/page_navigation_test.cpp $(HEADERS) | $(HOST_TEST_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $< -o $@

$(HOST_TEST_DIR)/sfx: tests/sfx_test.cpp source/sfx.cpp source/bgm_decoder.cpp $(HEADERS) $(wildcard romfs/sfx/*.ogg) | $(HOST_TEST_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) -Itests/audio_stubs $(filter %.cpp,$^) $(HOST_AUDIO_LIBS) -o $@

$(HOST_TEST_DIR)/hit_presentation: tests/hit_presentation_test.cpp source/combat.cpp $(HEADERS) | $(HOST_TEST_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $(filter %.cpp,$^) -o $@
