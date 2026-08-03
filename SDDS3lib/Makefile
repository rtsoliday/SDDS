include ../Makefile.rules

LIBRARY = SDDSpp

LIBRARY_SRC = SDDSModern.cc \
              SDDSLayout.cc \
              SDDSIO.cc \
              SDDSTransform.cc \
              SDDSLegacy.cc

CCFLAGS += -std=c++17 -I../include -DEXPORT_SDDSPP

ifeq ($(OS), Windows)
CCFLAGS += -I../zlib -I../lzma
endif

include ../Makefile.build

TEST_PROGRAM = $(OBJ_DIR)/test_sddspp
ADVANCED_TEST_PROGRAM = $(OBJ_DIR)/test_sddspp_advanced
DIFFERENTIAL_TEST_PROGRAM = $(OBJ_DIR)/test_sddspp_differential
FUZZ_SMOKE_PROGRAM = $(OBJ_DIR)/fuzz_sddspp_smoke
FUZZ_PROGRAM = $(OBJ_DIR)/fuzz_sddspp
SANITIZER_TEST_PROGRAM = $(OBJ_DIR)/test_sddspp_sanitized
BENCHMARK_PROGRAM = $(OBJ_DIR)/benchmark_sddspp
TEST_ARTIFACTS = $(OBJ_DIR)/test-artifacts
FUZZ_CXX ?= clang++
SANITIZER_CXX ?= clang++

ifeq ($(OS), Windows)
DIFFERENTIAL_LIBS = $(LIB_DIR)/SDDS1.lib $(LIB_DIR)/rpnlib.lib \
                    $(LIB_DIR)/mdbmth.lib $(LIB_DIR)/mdblib.lib lzma.lib z.lib
else
DIFFERENTIAL_LIBS = $(LIB_DIR)/libSDDS1.a $(LIB_DIR)/librpnlib.a \
                    $(LIB_DIR)/libmdbmth.a $(LIB_DIR)/libmdblib.a \
                    -lz -llzma -lgsl -lgslcblas -lm
endif

$(LIBRARY) $(LIBRARY_OBJS): | $(OBJ_DIR)
$(TEST_PROGRAM) $(ADVANCED_TEST_PROGRAM) $(DIFFERENTIAL_TEST_PROGRAM) \
  $(FUZZ_SMOKE_PROGRAM) $(FUZZ_PROGRAM) $(SANITIZER_TEST_PROGRAM) \
  $(BENCHMARK_PROGRAM): | $(OBJ_DIR)
$(TEST_PROGRAM): test_sdds3.cc $(LIBRARY)
	$(CCC) $(CCFLAGS) -std=c++17 -I../include -o $@ test_sdds3.cc $(LIBRARY) -lz -llzma

$(ADVANCED_TEST_PROGRAM): test_sdds3_advanced.cc $(LIBRARY)
	$(CCC) $(CCFLAGS) -std=c++17 -I../include -o $@ test_sdds3_advanced.cc $(LIBRARY) -lz -llzma

$(DIFFERENTIAL_TEST_PROGRAM): test_sdds3_differential.cc $(LIBRARY)
	$(CCC) $(CCFLAGS) -std=c++17 -I../include -o $@ test_sdds3_differential.cc $(LIBRARY) $(DIFFERENTIAL_LIBS)

$(BENCHMARK_PROGRAM): benchmark_sdds3.cc $(LIBRARY)
	$(CCC) $(CCFLAGS) -std=c++17 -I../include -o $@ benchmark_sdds3.cc $(LIBRARY) $(DIFFERENTIAL_LIBS) $(if $(filter Windows,$(OS)),psapi.lib,)

$(FUZZ_SMOKE_PROGRAM): fuzz_sdds3.cc $(LIBRARY)
	$(CCC) $(CCFLAGS) -std=c++17 -DSDDSPP_FUZZ_STANDALONE -I../include -o $@ fuzz_sdds3.cc $(LIBRARY) -lz -llzma

.PHONY: test differential-test
test: $(TEST_PROGRAM) $(ADVANCED_TEST_PROGRAM) differential-test
	$(TEST_PROGRAM) $(TEST_ARTIFACTS)
	$(ADVANCED_TEST_PROGRAM) $(TEST_ARTIFACTS)/advanced

differential-test: $(DIFFERENTIAL_TEST_PROGRAM)
	$(DIFFERENTIAL_TEST_PROGRAM) $(TEST_ARTIFACTS)/differential

.PHONY: benchmark benchmark-gate
benchmark: $(BENCHMARK_PROGRAM)
	$(BENCHMARK_PROGRAM) generate $(TEST_ARTIFACTS)/benchmark-numeric.sdds 50000
	$(BENCHMARK_PROGRAM) cpp-full $(TEST_ARTIFACTS)/benchmark-numeric.sdds
	$(BENCHMARK_PROGRAM) c-full $(TEST_ARTIFACTS)/benchmark-numeric.sdds
	$(BENCHMARK_PROGRAM) cpp-project $(TEST_ARTIFACTS)/benchmark-numeric.sdds
	$(BENCHMARK_PROGRAM) c-project $(TEST_ARTIFACTS)/benchmark-numeric.sdds
	$(BENCHMARK_PROGRAM) cpp-sparse $(TEST_ARTIFACTS)/benchmark-numeric.sdds
	$(BENCHMARK_PROGRAM) c-sparse $(TEST_ARTIFACTS)/benchmark-numeric.sdds
	$(BENCHMARK_PROGRAM) cpp-seek $(TEST_ARTIFACTS)/benchmark-numeric.sdds
	$(BENCHMARK_PROGRAM) c-seek $(TEST_ARTIFACTS)/benchmark-numeric.sdds
	$(BENCHMARK_PROGRAM) cpp-write $(TEST_ARTIFACTS)/benchmark-cpp-write.sdds 50000
	$(BENCHMARK_PROGRAM) c-write $(TEST_ARTIFACTS)/benchmark-c-write.sdds 50000
	$(BENCHMARK_PROGRAM) generate $(TEST_ARTIFACTS)/benchmark-cpp-append.sdds 50000
	$(BENCHMARK_PROGRAM) cpp-append $(TEST_ARTIFACTS)/benchmark-cpp-append.sdds
	$(BENCHMARK_PROGRAM) generate $(TEST_ARTIFACTS)/benchmark-c-append.sdds 50000
	$(BENCHMARK_PROGRAM) c-append $(TEST_ARTIFACTS)/benchmark-c-append.sdds
	$(BENCHMARK_PROGRAM) generate $(TEST_ARTIFACTS)/benchmark-cpp-update.sdds 50000
	$(BENCHMARK_PROGRAM) cpp-update $(TEST_ARTIFACTS)/benchmark-cpp-update.sdds
	$(BENCHMARK_PROGRAM) generate $(TEST_ARTIFACTS)/benchmark-c-update.sdds 50000
	$(BENCHMARK_PROGRAM) c-update $(TEST_ARTIFACTS)/benchmark-c-update.sdds
	$(BENCHMARK_PROGRAM) generate-mixed $(TEST_ARTIFACTS)/benchmark-mixed.sdds 20000
	$(BENCHMARK_PROGRAM) cpp-strings $(TEST_ARTIFACTS)/benchmark-mixed.sdds
	$(BENCHMARK_PROGRAM) c-strings $(TEST_ARTIFACTS)/benchmark-mixed.sdds
	$(BENCHMARK_PROGRAM) cpp-arrays $(TEST_ARTIFACTS)/benchmark-mixed.sdds
	$(BENCHMARK_PROGRAM) c-arrays $(TEST_ARTIFACTS)/benchmark-mixed.sdds
	$(BENCHMARK_PROGRAM) generate-gzip $(TEST_ARTIFACTS)/benchmark-numeric.sdds.gz 50000
	$(BENCHMARK_PROGRAM) cpp-compression $(TEST_ARTIFACTS)/benchmark-numeric.sdds.gz
	$(BENCHMARK_PROGRAM) c-compression $(TEST_ARTIFACTS)/benchmark-numeric.sdds.gz

benchmark-gate: $(BENCHMARK_PROGRAM)
	$(BENCHMARK_PROGRAM) generate $(TEST_ARTIFACTS)/benchmark-gate.sdds 100000
	python3 benchmark_gate.py $(BENCHMARK_PROGRAM) $(TEST_ARTIFACTS)/benchmark-gate.sdds

.PHONY: fuzz-smoke
fuzz-smoke: $(FUZZ_SMOKE_PROGRAM)
	$(FUZZ_SMOKE_PROGRAM) testdata/malformed/*.sdds

ifeq ($(OS), Windows)
.PHONY: sanitizer-test fuzz
sanitizer-test:
	@echo "sanitizer-test is supported on Unix-like builds"
fuzz:
	@echo "the libFuzzer target is supported on Unix-like builds"
else
$(SANITIZER_TEST_PROGRAM): test_sdds3_advanced.cc SDDSModern.cc SDDSLayout.cc SDDSIO.cc SDDSTransform.cc
	$(SANITIZER_CXX) $(CCFLAGS) -O1 -fno-omit-frame-pointer -fsanitize=address,undefined \
		-std=c++17 -I../include -o $@ test_sdds3_advanced.cc SDDSModern.cc SDDSLayout.cc SDDSIO.cc \
		SDDSTransform.cc -lz -llzma

.PHONY: sanitizer-test
sanitizer-test: $(SANITIZER_TEST_PROGRAM)
	ASAN_OPTIONS=detect_leaks=0 $(SANITIZER_TEST_PROGRAM) $(TEST_ARTIFACTS)/sanitized

$(FUZZ_PROGRAM): fuzz_sdds3.cc SDDSModern.cc SDDSLayout.cc SDDSIO.cc SDDSTransform.cc
	$(FUZZ_CXX) -O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined \
		-std=c++17 -I../include -o $@ fuzz_sdds3.cc SDDSModern.cc SDDSLayout.cc SDDSIO.cc \
		SDDSTransform.cc -lz -llzma

.PHONY: fuzz
fuzz: $(FUZZ_PROGRAM)
	@echo "Run $(FUZZ_PROGRAM) testdata/malformed for a bounded campaign"
endif
