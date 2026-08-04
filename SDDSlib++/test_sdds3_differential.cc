/**
 * @file test_sdds3_differential.cc
 * @brief Differential serial-format tests between the C and C++ SDDS libraries.
 *
 * @details Writes representative files with each implementation and verifies
 * that the other implementation reads the same layout and values.
 *
 * @copyright
 *   - (c) 2026 The University of Chicago
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 */

#include "SDDS.hpp"
#include "SDDS.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::vector<char> mutablePath(const std::filesystem::path &path);

sdds::Layout allTypesLayout(sdds::DataMode mode = sdds::DataMode::Binary,
                            sdds::MajorOrder major = sdds::MajorOrder::Row,
                            sdds::ByteOrder order = sdds::ByteOrder::Native) {
  sdds::DataOptions data;
  data.mode = mode;
  data.majorOrder = major;
  data.byteOrder = order;
  sdds::LayoutBuilder builder;
  builder.setDataOptions(data)
      .addParameter({{"p64", {}, {}, {}, {}, sdds::Type::Int64}, {}})
      .addArray({{"array", {}, {}, {}, {}, sdds::Type::Double}, 0, 1, {}})
      .addAssociate({"related", std::string("related.sdds"), std::string("."),
                     std::string("related data"), std::string("SDDS"), true})
      .addColumn({{"longdouble", {}, {}, {}, {}, sdds::Type::LongDouble}, 0})
      .addColumn({{"double", {}, {}, {}, {}, sdds::Type::Double}, 0})
      .addColumn({{"float", {}, {}, {}, {}, sdds::Type::Float}, 0})
      .addColumn({{"long64", {}, {}, {}, {}, sdds::Type::Int64}, 0})
      .addColumn({{"ulong64", {}, {}, {}, {}, sdds::Type::UInt64}, 0})
      .addColumn({{"long", {}, {}, {}, {}, sdds::Type::Int32}, 0})
      .addColumn({{"ulong", {}, {}, {}, {}, sdds::Type::UInt32}, 0})
      .addColumn({{"short", {}, {}, {}, {}, sdds::Type::Int16}, 0})
      .addColumn({{"ushort", {}, {}, {}, {}, sdds::Type::UInt16}, 0})
      .addColumn({{"string", {}, {}, {}, {}, sdds::Type::String}, 0})
      .addColumn({{"character", {}, {}, {}, {}, sdds::Type::Character}, 0});
  return builder.build();
}

sdds::Page allTypesPage(const std::shared_ptr<const sdds::Layout> &layout) {
  sdds::Page page(layout);
  page.setParameter("p64", std::int64_t{1234567890123});
  page.setArray("array", {{3}, std::vector<double>{1.25, 2.5, 5.0}});
  page.setColumn("longdouble", std::vector<long double>{1.25L, -2.5L});
  page.setColumn("double", std::vector<double>{3.25, -4.5});
  page.setColumn("float", std::vector<float>{5.25F, -6.5F});
  page.setColumn("long64", std::vector<std::int64_t>{-7, INT64_C(5000000000)});
  page.setColumn("ulong64", std::vector<std::uint64_t>{8, UINT64_C(9000000000)});
  page.setColumn("long", std::vector<std::int32_t>{-9, 10});
  page.setColumn("ulong", std::vector<std::uint32_t>{11, 12});
  page.setColumn("short", std::vector<std::int16_t>{-13, 14});
  page.setColumn("ushort", std::vector<std::uint16_t>{15, 16});
  page.setColumn("string", std::vector<std::string>{"alpha", "space value"});
  page.setColumn("character", std::vector<char>{'A', '!'});
  return page;
}

void cWrite(const std::filesystem::path &path) {
  SDDS_DATASET dataset{};
  auto filename = mutablePath(path);
  assert(SDDS_InitializeOutput(&dataset, SDDS_BINARY, 1, nullptr,
                               const_cast<char *>("C differential output"),
                               filename.data()));
  assert(SDDS_DefineSimpleParameter(&dataset, "p64", nullptr, SDDS_LONG64));
  assert(SDDS_DefineArray(&dataset, const_cast<char *>("array"), nullptr, nullptr, nullptr,
                          nullptr, SDDS_DOUBLE, 0, 1, nullptr) >= 0);
  const char *names[] = {"longdouble", "double", "float", "long64", "ulong64", "long",
                         "ulong", "short", "ushort", "string", "character"};
  const std::int32_t types[] = {SDDS_LONGDOUBLE, SDDS_DOUBLE, SDDS_FLOAT, SDDS_LONG64,
                                SDDS_ULONG64, SDDS_LONG, SDDS_ULONG, SDDS_SHORT,
                                SDDS_USHORT, SDDS_STRING, SDDS_CHARACTER};
  for (std::size_t index = 0; index < sizeof(types) / sizeof(types[0]); ++index)
    assert(SDDS_DefineSimpleColumn(&dataset, names[index], nullptr, types[index]));
  assert(SDDS_WriteLayout(&dataset));
  assert(SDDS_StartPage(&dataset, 2));
  assert(SDDS_SetParameters(&dataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "p64", INT64_C(1234567890123), nullptr));
  double array[] = {1.25, 2.5, 5.0};
  std::int32_t dimensions[] = {3};
  assert(SDDS_SetArray(&dataset, const_cast<char *>("array"), SDDS_CONTIGUOUS_DATA,
                       array, dimensions));
  long double longdoubleValues[] = {1.25L, -2.5L};
  double doubleValues[] = {3.25, -4.5};
  float floatValues[] = {5.25F, -6.5F};
  std::int64_t long64Values[] = {-7, INT64_C(5000000000)};
  std::uint64_t ulong64Values[] = {8, UINT64_C(9000000000)};
  std::int32_t longValues[] = {-9, 10};
  std::uint32_t ulongValues[] = {11, 12};
  std::int16_t shortValues[] = {-13, 14};
  std::uint16_t ushortValues[] = {15, 16};
  char alpha[] = "alpha";
  char spaced[] = "space value";
  char *stringValues[] = {alpha, spaced};
  char characterValues[] = {'A', '!'};
  void *values[] = {longdoubleValues, doubleValues, floatValues, long64Values, ulong64Values,
                    longValues, ulongValues, shortValues, ushortValues, stringValues,
                    characterValues};
  for (std::size_t index = 0; index < sizeof(values) / sizeof(values[0]); ++index)
    assert(SDDS_SetColumn(&dataset, SDDS_SET_BY_NAME, values[index], 2, names[index]));
  assert(SDDS_WritePage(&dataset));
  assert(SDDS_Terminate(&dataset));
}

void cppRead(const std::filesystem::path &path) {
  sdds::ReaderOptions options;
#if defined(_WIN32)
  options.longDoubleEncoding = sdds::LongDoubleEncoding::LegacyFloat64;
#endif
  auto reader = sdds::Reader::open(path, options);
  auto page = reader.next();
  assert(page && page->rowCount() == 2);
  assert(page->parameterAs<std::int64_t>("p64") == INT64_C(1234567890123));
  assert(page->arrayAs<double>("array")[2] == 5.0);
  assert(std::fabs(page->columnAs<long double>("longdouble")[1] + 2.5L) < 1e-12L);
  assert(page->columnAs<std::uint64_t>("ulong64")[1] == UINT64_C(9000000000));
  assert(page->columnAs<std::string>("string")[1] == "space value");
  assert(page->columnAs<char>("character")[1] == '!');
  reader.close();
}

void cppWrite(const std::filesystem::path &path, sdds::DataMode mode,
              sdds::MajorOrder major, sdds::ByteOrder order,
              sdds::Compression compression = sdds::Compression::None) {
  const auto layout = allTypesLayout(mode, major, order);
  sdds::WriterOptions options;
  options.compression = compression;
#if defined(_WIN32)
  options.longDoubleEncoding = sdds::LongDoubleEncoding::LegacyFloat64;
#endif
  auto writer = sdds::Writer::create(path, layout, options);
  writer.write(allTypesPage(std::make_shared<const sdds::Layout>(layout)));
  writer.close();
  auto reader = sdds::Reader::open(path);
  assert(reader.layout().associates.size() == 1);
  assert(reader.layout().associates[0].name == "related");
  assert(reader.layout().associates[0].isSdds);
  reader.close();
}

std::vector<char> mutablePath(const std::filesystem::path &path) {
  const std::string text = path.string();
  std::vector<char> result(text.begin(), text.end());
  result.push_back('\0');
  return result;
}

void cRead(const std::filesystem::path &path) {
  SDDS_DATASET dataset{};
  auto filename = mutablePath(path);
  assert(SDDS_InitializeInput(&dataset, filename.data()));
  assert(SDDS_ReadPage(&dataset) == 1);
  assert(SDDS_CountRowsOfInterest(&dataset) == 2);
  std::int64_t parameter = 0;
  assert(SDDS_GetParameter(&dataset, const_cast<char *>("p64"), &parameter));
  assert(parameter == INT64_C(1234567890123));
  auto *long64Values = static_cast<std::int64_t *>(
      SDDS_GetColumn(&dataset, const_cast<char *>("long64")));
  assert(long64Values && long64Values[1] == INT64_C(5000000000));
  SDDS_Free(long64Values);
  char **strings = SDDS_GetColumnInString(&dataset, const_cast<char *>("string"));
  assert(strings && std::string(strings[1]) == "space value");
  SDDS_FreeStringArray(strings, 2);
  SDDS_Free(strings);
  SDDS_ARRAY *array = SDDS_GetArray(&dataset, const_cast<char *>("array"), nullptr);
  assert(array && array->elements == 3 && static_cast<double *>(array->data)[2] == 5.0);
  SDDS_FreeArray(array);
  assert(SDDS_Terminate(&dataset));
}

void cWriteRowMode(const std::filesystem::path &path, sdds::DataMode dataMode,
                   sdds::RowCountMode rowCountMode) {
  SDDS_DATASET dataset{};
  auto filename = mutablePath(path);
  assert(SDDS_InitializeOutput(&dataset,
                               dataMode == sdds::DataMode::Ascii ? SDDS_ASCII : SDDS_BINARY,
                               1, nullptr, nullptr, filename.data()));
  assert(SDDS_DefineSimpleColumn(&dataset, "x", nullptr, SDDS_DOUBLE));
  const std::uint32_t cMode = rowCountMode == sdds::RowCountMode::Fixed ?
                                  SDDS_FIXEDROWCOUNT : SDDS_NOROWCOUNT;
  assert(SDDS_SetRowCountMode(&dataset, cMode));
  assert(SDDS_WriteLayout(&dataset));
  assert(SDDS_StartPage(&dataset, 2));
  double values[] = {1.0, 2.0};
  assert(SDDS_SetColumn(&dataset, SDDS_SET_BY_NAME, values, 2, "x"));
  assert(SDDS_WritePage(&dataset));
  assert(SDDS_Terminate(&dataset));
}

void cppReadRows(const std::filesystem::path &path, sdds::RowCountMode mode) {
  auto reader = sdds::Reader::open(path);
  assert(reader.layout().data.rowCountMode == mode);
  auto page = reader.next();
  assert(page && page->rowCount() == 2);
  assert(page->columnAs<double>("x")[1] == 2.0);
  reader.close();
}

void cppWriteRowMode(const std::filesystem::path &path, sdds::DataMode dataMode,
                     sdds::RowCountMode rowCountMode) {
  sdds::DataOptions data;
  data.mode = dataMode;
  data.rowCountMode = rowCountMode;
  sdds::LayoutBuilder builder;
  builder.setDataOptions(data).addColumn({{"x", {}, {}, {}, {}, sdds::Type::Double}, 0});
  const auto layout = builder.build();
  auto writer = sdds::Writer::create(path, layout);
  sdds::Page page(std::make_shared<const sdds::Layout>(layout));
  page.setColumn("x", std::vector<double>{1.0, 2.0});
  writer.write(std::move(page));
  writer.close();

  SDDS_DATASET dataset{};
  auto filename = mutablePath(path);
  assert(SDDS_InitializeInput(&dataset, filename.data()));
  assert(SDDS_ReadPage(&dataset) == 1);
  assert(SDDS_CountRowsOfInterest(&dataset) == 2);
  assert(SDDS_Terminate(&dataset));
}

void rowCountMatrix(const std::filesystem::path &directory) {
  for (const auto dataMode : {sdds::DataMode::Ascii, sdds::DataMode::Binary}) {
    const std::string modeName = dataMode == sdds::DataMode::Ascii ? "ascii" : "binary";
    const auto cPath = directory / ("c-fixed-" + modeName + ".sdds");
    cWriteRowMode(cPath, dataMode, sdds::RowCountMode::Fixed);
    cppReadRows(cPath, sdds::RowCountMode::Fixed);
    const auto cppPath = directory / ("cpp-fixed-" + modeName + ".sdds");
    cppWriteRowMode(cppPath, dataMode, sdds::RowCountMode::Fixed);
  }
  const auto cNoCount = directory / "c-no-row-count.sdds";
  cWriteRowMode(cNoCount, sdds::DataMode::Ascii, sdds::RowCountMode::None);
  cppReadRows(cNoCount, sdds::RowCountMode::None);
  const auto cppNoCount = directory / "cpp-no-row-count.sdds";
  cppWriteRowMode(cppNoCount, sdds::DataMode::Ascii, sdds::RowCountMode::None);
}

void versionMatrix(const std::filesystem::path &directory) {
  sdds::LayoutBuilder builder;
  builder.addColumn({{"x", {}, {}, {}, {}, sdds::Type::Double}, 0});
  const auto layout = builder.build();
  for (std::int32_t version = 1; version <= 5; ++version) {
    const auto path = directory / ("version-" + std::to_string(version) + ".sdds");
    sdds::WriterOptions options;
    options.minimumVersion = version;
    auto writer = sdds::Writer::create(path, layout, options);
    sdds::Page page(std::make_shared<const sdds::Layout>(layout));
    page.setColumn("x", std::vector<double>{1.0, 2.0});
    writer.write(std::move(page));
    writer.close();
    SDDS_DATASET dataset{};
    auto filename = mutablePath(path);
    assert(SDDS_InitializeInput(&dataset, filename.data()));
    assert(dataset.layout.version == version);
    assert(SDDS_ReadPage(&dataset) == 1);
    assert(SDDS_Terminate(&dataset));
  }
}

void includeMatrix(const std::filesystem::path &directory) {
  const auto includePath = std::filesystem::absolute(directory / "included-layout.sdds");
  const auto rootPath = directory / "include-root.sdds";
  {
    std::ofstream included(includePath);
    included << "&column name=included, type=long &end\n";
    assert(included.good());
  }
  {
    std::ofstream root(rootPath);
    root << "SDDS1\n"
         << "&include filename=\"" << includePath.generic_string() << "\" &end\n"
         << "&data mode=ascii &end\n"
         << "0\n";
    assert(root.good());
  }

  auto reader = sdds::Reader::open(rootPath);
  assert(reader.layout().columns.size() == 1);
  assert(reader.layout().columns[0].name == "included");
  auto page = reader.next();
  assert(page && page->rowCount() == 0);
  reader.close();

  SDDS_DATASET dataset{};
  auto filename = mutablePath(rootPath);
  assert(SDDS_InitializeInput(&dataset, filename.data()));
  assert(dataset.layout.n_columns == 1);
  assert(std::string(dataset.layout.column_definition[0].name) == "included");
  assert(SDDS_ReadPage(&dataset) == 1);
  assert(SDDS_Terminate(&dataset));
}

}  // namespace

int main(int argc, char **argv) {
#if defined(_WIN32)
  _putenv_s("SDDS_LONGDOUBLE_64BITS", "1");
#endif
  if (argc != 2) return 2;
  const std::filesystem::path directory(argv[1]);
  std::filesystem::create_directories(directory);
  const auto cOutput = directory / "c-to-cpp.sdds";
  cWrite(cOutput);
  cppRead(cOutput);
  for (const auto mode : {sdds::DataMode::Ascii, sdds::DataMode::Binary}) {
    for (const auto major : {sdds::MajorOrder::Row, sdds::MajorOrder::Column}) {
      if (mode == sdds::DataMode::Ascii && major == sdds::MajorOrder::Column) continue;
      for (const auto order : {sdds::ByteOrder::Little, sdds::ByteOrder::Big}) {
        const auto path = directory / (std::string(mode == sdds::DataMode::Ascii ? "ascii" : "binary") +
                                       (major == sdds::MajorOrder::Row ? "-row" : "-column") +
                                       (order == sdds::ByteOrder::Little ? "-little.sdds" :
                                                                          "-big.sdds"));
        cppWrite(path, mode, major, order);
        cRead(path);
      }
    }
  }
  for (const auto compression : {sdds::Compression::Gzip, sdds::Compression::Xz,
                                 sdds::Compression::Lzma}) {
    const char *extension = compression == sdds::Compression::Gzip ? ".sdds.gz" :
                            compression == sdds::Compression::Xz ? ".sdds.xz" : ".sdds.lzma";
    const auto path = directory / (std::string("compressed") + extension);
    cppWrite(path, sdds::DataMode::Binary, sdds::MajorOrder::Row,
             sdds::ByteOrder::Little, compression);
    cRead(path);
  }
  versionMatrix(directory);
  rowCountMatrix(directory);
  includeMatrix(directory);
  return 0;
}
