/**
 * @file benchmark_sdds3.cc
 * @brief Comparable serial C and C++ SDDS benchmark workloads.
 *
 * @copyright Copyright (c) 2026 The University of Chicago
 * @license Distributed under the Software License Agreement in LICENSE.
 */

#include "SDDS.hpp"
#include "SDDS.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#  if !defined(NOMINMAX)
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <psapi.h>
#else
#  include <sys/resource.h>
#endif

namespace {

constexpr std::int32_t columns = 20;
constexpr std::int32_t pages = 4;

std::vector<char> mutablePath(const std::filesystem::path &path) {
  const std::string text = path.string();
  std::vector<char> result(text.begin(), text.end());
  result.push_back('\0');
  return result;
}

std::uint64_t peakMemoryKib() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS counters{};
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) return 0;
  return static_cast<std::uint64_t>(counters.PeakWorkingSetSize / 1024U);
#else
  struct rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#  if defined(__APPLE__)
  return static_cast<std::uint64_t>(usage.ru_maxrss / 1024);
#  else
  return static_cast<std::uint64_t>(usage.ru_maxrss);
#  endif
#endif
}

sdds::Layout numericLayout() {
  sdds::DataOptions data;
  data.mode = sdds::DataMode::Binary;
  data.majorOrder = sdds::MajorOrder::Row;
  data.byteOrder = sdds::ByteOrder::Little;
  sdds::LayoutBuilder builder;
  builder.setDataOptions(data);
  for (std::int32_t column = 0; column < columns; ++column)
    builder.addColumn({{"c" + std::to_string(column), {}, {}, {}, {}, sdds::Type::Double}, 0});
  return builder.build();
}

sdds::Layout mixedLayout() {
  sdds::LayoutBuilder builder;
  builder.addArray({{"array", {}, {}, {}, {}, sdds::Type::Double}, 0, 1, {}})
      .addColumn({{"c0", {}, {}, {}, {}, sdds::Type::Double}, 0})
      .addColumn({{"text", {}, {}, {}, {}, sdds::Type::String}, 0});
  return builder.build();
}

std::vector<double> columnValues(std::int64_t rows, std::int32_t column,
                                 std::int32_t page) {
  std::vector<double> values(static_cast<std::size_t>(rows));
  for (std::int64_t row = 0; row < rows; ++row)
    values[static_cast<std::size_t>(row)] = page * 1000.0 + column + row * 0.001;
  return values;
}

void generateCpp(const std::filesystem::path &path, std::int64_t rows,
                 sdds::Compression compression = sdds::Compression::None) {
  auto layout = numericLayout();
  sdds::WriterOptions options;
  options.compression = compression;
  options.gzipLevel = 1;
  options.lzmaPreset = 1;
  auto writer = sdds::Writer::create(path, layout, options);
  for (std::int32_t page = 0; page < pages; ++page) {
    writer.beginPage(rows);
    for (std::int32_t column = 0; column < columns; ++column)
      writer.setColumn("c" + std::to_string(column), columnValues(rows, column, page));
    writer.commitPage();
  }
  writer.close();
}

void generateMixed(const std::filesystem::path &path, std::int64_t rows) {
  const auto layout = mixedLayout();
  auto writer = sdds::Writer::create(path, layout);
  std::vector<double> array(10000, 1.25);
  std::vector<double> numeric(static_cast<std::size_t>(rows), 2.5);
  std::vector<std::string> strings(static_cast<std::size_t>(rows));
  for (std::int64_t row = 0; row < rows; ++row)
    strings[static_cast<std::size_t>(row)] = "row-" + std::to_string(row % 1000);
  for (std::int32_t page = 0; page < pages; ++page) {
    writer.beginPage(rows);
    writer.setArray("array", {{static_cast<std::int32_t>(array.size())}, array});
    writer.setColumn("c0", numeric);
    writer.setColumn("text", strings);
    writer.commitPage();
  }
  writer.close();
}

double cppRead(const std::filesystem::path &path, std::string_view mode) {
  auto reader = sdds::Reader::open(path);
  sdds::ReadRequest request;
  if (mode == "project") {
    request.parameters = sdds::FieldSelection::noFields();
    request.arrays = sdds::FieldSelection::noFields();
    request.columns = sdds::FieldSelection::only({"c0", "c1"});
  } else if (mode == "sparse") {
    request.rows.stride = 10;
  }
  double checksum = 0;
  while (auto page = reader.next(request)) {
    const auto &values = page->columnAs<double>("c0");
    for (const double value : values) checksum += value;
    if (mode == "project")
      for (const double value : page->columnAs<double>("c1")) checksum += value;
  }
  reader.close();
  return checksum;
}

double cppSeek(const std::filesystem::path &path) {
  auto reader = sdds::Reader::open(path);
  reader.buildPageIndex();
  sdds::ReadRequest request;
  request.parameters = sdds::FieldSelection::noFields();
  request.arrays = sdds::FieldSelection::noFields();
  request.columns = sdds::FieldSelection::only({"c0"});
  request.rows.first = 0;
  request.rows.count = 1;
  double checksum = 0;
  for (std::int32_t iteration = 0; iteration < 20; ++iteration) {
    reader.gotoPage(iteration % 2 ? pages : 1);
    checksum += reader.next(request)->columnAs<double>("c0")[0];
  }
  reader.close();
  return checksum;
}

double cppMixedRead(const std::filesystem::path &path, bool strings) {
  auto reader = sdds::Reader::open(path);
  sdds::ReadRequest request;
  request.parameters = sdds::FieldSelection::noFields();
  request.arrays = strings ? sdds::FieldSelection::noFields()
                           : sdds::FieldSelection::only({"array"});
  request.columns = strings ? sdds::FieldSelection::only({"text"})
                            : sdds::FieldSelection::noFields();
  double checksum = 0;
  while (auto page = reader.next(request)) {
    if (strings)
      for (const auto &value : page->columnAs<std::string>("text")) checksum += value.size();
    else
      for (const double value : page->arrayAs<double>("array")) checksum += value;
  }
  reader.close();
  return checksum;
}

double cRead(const std::filesystem::path &path, std::string_view mode) {
  SDDS_DATASET dataset{};
  auto filename = mutablePath(path);
  if (!SDDS_InitializeInput(&dataset, filename.data())) return NAN;
  if (mode == "project") {
    char first[] = "c0";
    char second[] = "c1";
    char *names[] = {first, second};
    SDDS_SetColumnFlags(&dataset, 0);
    if (!SDDS_SetColumnsOfInterest(&dataset, SDDS_NAME_ARRAY, 2, names)) return NAN;
  }
  double checksum = 0;
  std::int32_t page = 0;
  while ((page = mode == "sparse" ? SDDS_ReadPageSparse(&dataset, 0, 10, 0, 0)
                                    : SDDS_ReadPage(&dataset)) > 0) {
    double *values = SDDS_GetColumnInDoubles(&dataset, const_cast<char *>("c0"));
    const std::int64_t rows = SDDS_CountRowsOfInterest(&dataset);
    for (std::int64_t row = 0; row < rows; ++row) checksum += values[row];
    SDDS_Free(values);
    if (mode == "project") {
      values = SDDS_GetColumnInDoubles(&dataset, const_cast<char *>("c1"));
      for (std::int64_t row = 0; row < rows; ++row) checksum += values[row];
      SDDS_Free(values);
    }
  }
  SDDS_Terminate(&dataset);
  return checksum;
}

double cSeek(const std::filesystem::path &path) {
  SDDS_DATASET dataset{};
  auto filename = mutablePath(path);
  SDDS_SetDefaultIOBufferSize(0);
  if (!SDDS_InitializeInput(&dataset, filename.data())) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return NAN;
  }
  double checksum = 0;
  while (SDDS_ReadPageSparse(&dataset, 0, 1000000000, 0, 0) > 0) {}
  for (std::int32_t iteration = 0; iteration < 20; ++iteration) {
    if (!SDDS_GotoPage(&dataset, iteration % 2 ? pages : 1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return NAN;
    }
    if (SDDS_ReadPageSparse(&dataset, 0, 1000000000, 0, 0) <= 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return NAN;
    }
    double *values = SDDS_GetColumnInDoubles(&dataset, const_cast<char *>("c0"));
    checksum += values[0];
    SDDS_Free(values);
  }
  SDDS_Terminate(&dataset);
  return checksum;
}

double cMixedRead(const std::filesystem::path &path, bool strings) {
  SDDS_DATASET dataset{};
  auto filename = mutablePath(path);
  if (!SDDS_InitializeInput(&dataset, filename.data())) return NAN;
  double checksum = 0;
  while (SDDS_ReadPage(&dataset) > 0) {
    if (strings) {
      char **values = SDDS_GetColumnInString(&dataset, const_cast<char *>("text"));
      const std::int64_t rows = SDDS_CountRowsOfInterest(&dataset);
      for (std::int64_t row = 0; row < rows; ++row)
        checksum += std::string(values[row]).size();
      SDDS_FreeStringArray(values, rows);
      SDDS_Free(values);
    } else {
      SDDS_ARRAY *array = SDDS_GetArray(&dataset, const_cast<char *>("array"), nullptr);
      if (!array) return NAN;
      const auto *values = static_cast<const double *>(array->data);
      for (std::int32_t index = 0; index < array->elements; ++index) checksum += values[index];
      SDDS_FreeArray(array);
    }
  }
  SDDS_Terminate(&dataset);
  return checksum;
}

double cppWrite(const std::filesystem::path &path, std::int64_t rows) {
  generateCpp(path, rows);
  return static_cast<double>(std::filesystem::file_size(path));
}

double cWrite(const std::filesystem::path &path, std::int64_t rows) {
  SDDS_DATASET dataset{};
  auto filename = mutablePath(path);
  if (!SDDS_InitializeOutput(&dataset, SDDS_BINARY, 0, nullptr, nullptr, filename.data()))
    return NAN;
  for (std::int32_t column = 0; column < columns; ++column) {
    const std::string name = "c" + std::to_string(column);
    if (!SDDS_DefineSimpleColumn(&dataset, name.c_str(), nullptr, SDDS_DOUBLE)) return NAN;
  }
  if (!SDDS_WriteLayout(&dataset)) return NAN;
  for (std::int32_t page = 0; page < pages; ++page) {
    if (!SDDS_StartPage(&dataset, rows)) return NAN;
    for (std::int32_t column = 0; column < columns; ++column) {
      const std::string name = "c" + std::to_string(column);
      auto values = columnValues(rows, column, page);
      if (!SDDS_SetColumn(&dataset, SDDS_SET_BY_NAME, values.data(), rows, name.c_str()))
        return NAN;
    }
    if (!SDDS_WritePage(&dataset)) return NAN;
  }
  if (!SDDS_Terminate(&dataset)) return NAN;
  return static_cast<double>(std::filesystem::file_size(path));
}

double cppAppend(const std::filesystem::path &path) {
  auto writer = sdds::Writer::append(path);
  writer.beginPage(1);
  for (std::int32_t column = 0; column < columns; ++column)
    writer.setColumn("c" + std::to_string(column), std::vector<double>{column * 1.0});
  writer.commitPage();
  writer.close();
  return static_cast<double>(std::filesystem::file_size(path));
}

double cAppend(const std::filesystem::path &path) {
  SDDS_DATASET dataset{};
  if (!SDDS_InitializeAppend(&dataset, path.string().c_str())) return NAN;
  if (!SDDS_StartPage(&dataset, 1)) return NAN;
  for (std::int32_t column = 0; column < columns; ++column) {
    const std::string name = "c" + std::to_string(column);
    double value = column;
    if (!SDDS_SetColumn(&dataset, SDDS_SET_BY_NAME, &value, 1, name.c_str())) return NAN;
  }
  if (!SDDS_WritePage(&dataset) || !SDDS_Terminate(&dataset)) return NAN;
  return static_cast<double>(std::filesystem::file_size(path));
}

double cppUpdate(const std::filesystem::path &path) {
  auto writer = sdds::Writer::appendToLastPage(path, 1);
  const std::int64_t firstRow = writer.rowsPresent();
  for (std::int32_t column = 0; column < columns; ++column)
    writer.setColumn("c" + std::to_string(column), std::vector<double>{column * 1.0}, firstRow);
  writer.updatePage(true);
  writer.close();
  return static_cast<double>(std::filesystem::file_size(path));
}

double cUpdate(const std::filesystem::path &path) {
  SDDS_DATASET dataset{};
  std::int64_t rowsPresent = 0;
  if (!SDDS_InitializeAppendToPage(&dataset, path.string().c_str(), 1, &rowsPresent)) return NAN;
  if (!SDDS_LengthenTable(&dataset, 1)) return NAN;
  if (!SDDS_SetRowValues(&dataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rowsPresent,
                         "c0", 0.0, "c1", 1.0, nullptr))
    return NAN;
  if (!SDDS_UpdatePage(&dataset, FLUSH_TABLE) || !SDDS_Terminate(&dataset)) return NAN;
  return static_cast<double>(std::filesystem::file_size(path));
}

void result(const std::string &engine, const std::string &mode, double seconds,
            double checksum) {
  std::cout << "{\"engine\":\"" << engine << "\",\"mode\":\"" << mode
            << "\",\"seconds\":" << seconds << ",\"peak_kib\":" << peakMemoryKib()
            << ",\"checksum\":" << checksum << "}\n";
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "usage: benchmark_sddspp MODE PATH [ROWS]\n";
    return 2;
  }
  const std::string mode(argv[1]);
  const std::filesystem::path path(argv[2]);
  const std::int64_t rows = argc > 3 ? std::stoll(argv[3]) : 100000;
  if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
  if (mode == "generate") {
    generateCpp(path, rows);
    return 0;
  }
  if (mode == "generate-gzip") {
    generateCpp(path, rows, sdds::Compression::Gzip);
    return 0;
  }
  if (mode == "generate-xz") {
    generateCpp(path, rows, sdds::Compression::Xz);
    return 0;
  }
  if (mode == "generate-mixed") {
    generateMixed(path, rows);
    return 0;
  }
  const auto start = std::chrono::steady_clock::now();
  double checksum = NAN;
  std::string engine;
  std::string workload;
  if (mode.rfind("cpp-", 0) == 0) {
    engine = "cpp";
    workload = mode.substr(4);
    if (workload == "full" || workload == "project" || workload == "sparse" ||
        workload == "compression")
      checksum = cppRead(path, workload == "compression" ? "full" : workload);
    else if (workload == "strings" || workload == "arrays")
      checksum = cppMixedRead(path, workload == "strings");
    else if (workload == "seek") checksum = cppSeek(path);
    else if (workload == "write") checksum = cppWrite(path, rows);
    else if (workload == "append") checksum = cppAppend(path);
    else if (workload == "update") checksum = cppUpdate(path);
  } else if (mode.rfind("c-", 0) == 0) {
    engine = "c";
    workload = mode.substr(2);
    if (workload == "full" || workload == "project" || workload == "sparse" ||
        workload == "compression")
      checksum = cRead(path, workload == "compression" ? "full" : workload);
    else if (workload == "strings" || workload == "arrays")
      checksum = cMixedRead(path, workload == "strings");
    else if (workload == "seek") checksum = cSeek(path);
    else if (workload == "write") checksum = cWrite(path, rows);
    else if (workload == "append") checksum = cAppend(path);
    else if (workload == "update") checksum = cUpdate(path);
  }
  if (engine.empty() || std::isnan(checksum)) return 2;
  const double seconds = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - start).count();
  result(engine, workload, seconds, checksum);
  return 0;
}
