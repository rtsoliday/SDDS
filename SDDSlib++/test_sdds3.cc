/**
 * @file test_sdds3.cc
 * @brief Integration tests for the C++17 SDDS implementation.
 *
 * @copyright Copyright (c) 2026 The University of Chicago
 * @license Distributed under the Software License Agreement in LICENSE.
 */

#include "SDDS.hpp"
#include "SDDS3.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

static_assert(sdds::libraryName == "SDDS++");

namespace {

sdds::Layout makeLayout(sdds::DataMode mode, sdds::MajorOrder major,
                        sdds::ByteOrder order, sdds::RowCountMode rowCounts) {
  sdds::DataOptions data;
  data.mode = mode;
  data.majorOrder = major;
  data.byteOrder = order;
  data.rowCountMode = rowCounts;
  sdds::LayoutBuilder builder;
  builder.setDescription("C++17 round trip", "SDDS++ integration test")
      .setDataOptions(data)
      .addParameter({{"p64", std::nullopt, "count", std::nullopt, std::nullopt,
                      sdds::Type::Int64}, std::nullopt})
      .addParameter({{"fixed", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                      sdds::Type::String}, std::string("fixed value")})
      .addArray({{"matrix", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                  sdds::Type::UInt64}, 0, 2, std::string("group")})
      .addColumn({{"ld", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                   sdds::Type::LongDouble}, 0})
      .addColumn({{"value", std::nullopt, "m", std::nullopt, std::nullopt,
                   sdds::Type::Double}, 0})
      .addColumn({{"u64", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                   sdds::Type::UInt64}, 0})
      .addColumn({{"label", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                   sdds::Type::String}, 0})
      .addColumn({{"code", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                   sdds::Type::Character}, 0})
      .addAssociate({"source", std::string("source.sdds"), std::nullopt,
                     std::string("test associate"), std::nullopt, true});
  return builder.build();
}

sdds::Page makePage(const std::shared_ptr<const sdds::Layout> &layout, std::int64_t base) {
  sdds::Page page(layout);
  page.setParameter("p64", base);
  page.setParameter("fixed", std::string("fixed value"));
  page.setArray("matrix", {{2, 2}, std::vector<std::uint64_t>{1, 2, 3, 4}});
  page.setColumn("ld", std::vector<long double>{1.25L, -2.5L, 3.75L});
  page.setColumn("value", std::vector<double>{0.5, 1.5, 2.5});
  page.setColumn("u64", std::vector<std::uint64_t>{UINT64_C(1), UINT64_C(4294967297),
                                                    UINT64_C(18446744073709551614)});
  page.setColumn("label", std::vector<std::string>{"plain", "space value", "bang!\\quote\""});
  page.setColumn("code", std::vector<char>{'A', ' ', '!'});
  return page;
}

void verifyPage(const sdds::Page &page, std::int64_t base) {
  assert(page.rowCount() == 3);
  assert(page.parameterAs<std::int64_t>("p64") == base);
  assert(page.parameterAs<std::string>("fixed") == "fixed value");
  assert((page.array("matrix").dimensions == std::vector<std::int32_t>{2, 2}));
  assert((page.arrayAs<std::uint64_t>("matrix") == std::vector<std::uint64_t>{1, 2, 3, 4}));
  assert(std::fabs(page.columnAs<long double>("ld")[2] - 3.75L) < 1e-12L);
  assert(page.columnAs<double>("value")[1] == 1.5);
  assert(page.columnAs<std::uint64_t>("u64")[1] == UINT64_C(4294967297));
  assert(page.columnAs<std::string>("label")[2] == "bang!\\quote\"");
  assert(page.columnAs<char>("code")[1] == ' ');
}

void roundTrip(const std::filesystem::path &path, sdds::DataMode mode,
               sdds::MajorOrder major, sdds::ByteOrder order,
               sdds::RowCountMode rowCounts) {
  auto layout = makeLayout(mode, major, order, rowCounts);
  auto shared = std::make_shared<const sdds::Layout>(layout);
  auto writer = sdds::Writer::create(path, layout);
  writer.write(makePage(shared, INT64_C(9223372036854775000)));
  writer.write(makePage(shared, 17));
  writer.close();

  auto reader = sdds::Reader::open(path);
  assert(reader.layout().version == 5);
  auto first = reader.next();
  assert(first && first->number() == 1);
  verifyPage(*first, INT64_C(9223372036854775000));
  auto second = reader.next();
  assert(second && second->number() == 2);
  verifyPage(*second, 17);
  assert(!reader.next());
  reader.close();
}

void selectionAndGoto(const std::filesystem::path &path) {
  auto reader = sdds::Reader::open(path);
  sdds::ReadRequest request;
  request.rows.stride = 2;
  auto page = reader.next(request);
  assert(page && page->rowCount() == 2);
  assert((page->columnAs<double>("value") == std::vector<double>{0.5, 2.5}));
  reader.gotoPage(2);
  page = reader.next();
  assert(page && page->parameterAs<std::int64_t>("p64") == 17);
  reader.close();
}

void appendAndUpdate(const std::filesystem::path &path) {
  sdds::DataOptions data;
  data.mode = sdds::DataMode::Binary;
  sdds::LayoutBuilder builder;
  builder.setDataOptions(data)
      .addColumn({{"x", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                   sdds::Type::Int32}, 0});
  const auto layout = builder.build();

  auto writer = sdds::Writer::create(path, layout);
  writer.beginPage(3);
  writer.setColumn("x", std::vector<std::int32_t>{1, 2});
  writer.updatePage();
  writer.setColumn("x", std::vector<std::int32_t>{3}, 2);
  writer.commitPage();
  writer.close();

  writer = sdds::Writer::append(path);
  writer.beginPage(1);
  writer.setColumn("x", std::vector<std::int32_t>{4});
  writer.commitPage();
  writer.close();

  writer = sdds::Writer::appendToLastPage(path, 1);
  writer.setColumn("x", std::vector<std::int32_t>{5, 6}, 1);
  writer.updatePage(true);
  writer.close();

  auto reader = sdds::Reader::open(path);
  auto first = reader.next();
  assert(first && (first->columnAs<std::int32_t>("x") ==
                   std::vector<std::int32_t>{1, 2, 3}));
  auto second = reader.next();
  assert(second && (second->columnAs<std::int32_t>("x") ==
                    std::vector<std::int32_t>{4, 5, 6}));
  assert(!reader.next());
  reader.close();
}

void includesLimitsAndRecovery(const std::filesystem::path &directory,
                               const std::filesystem::path &testData) {
  const auto previousDirectory = std::filesystem::current_path();
  std::filesystem::current_path(testData.parent_path().parent_path());
  auto reader = sdds::Reader::open(testData);
  auto dataset = reader.readAll();
  reader.close();
  std::filesystem::current_path(previousDirectory);
  assert(dataset.layout.parameters.size() == 1);
  assert(dataset.pages.size() == 1);
  assert(dataset.pages[0].parameterAs<std::string>("origin") == "included");
  assert((dataset.pages[0].arrayAs<std::int32_t>("shape") ==
          std::vector<std::int32_t>{4, 5, 6}));

  const auto flattened = directory / "flattened-include.sdds";
  auto writer = sdds::Writer::create(flattened, dataset.layout);
  writer.write(dataset.pages[0]);
  writer.close();
  reader = sdds::Reader::open(flattened);
  assert(reader.layout().parameters.size() == 1);
  assert(reader.next());
  reader.close();

  const auto recoveryFile = directory / "fixed-recovery.sdds";
  sdds::DataOptions data;
  data.mode = sdds::DataMode::Binary;
  data.rowCountMode = sdds::RowCountMode::Fixed;
  sdds::LayoutBuilder builder;
  builder.setDataOptions(data)
      .addColumn({{"x", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                   sdds::Type::Int32}, 0});
  const auto layout = builder.build();
  writer = sdds::Writer::create(recoveryFile, layout);
  writer.beginPage(3);
  writer.setColumn("x", std::vector<std::int32_t>{1, 2, 3});
  writer.commitPage();
  writer.close();
  std::filesystem::resize_file(recoveryFile, std::filesystem::file_size(recoveryFile) - 2);

  reader = sdds::Reader::open(recoveryFile);
  auto recovered = reader.next();
  assert(recovered && recovered->recovered() && recovered->rowCount() == 2);
  reader.close();

  sdds::ReaderOptions strict;
  strict.recovery = sdds::RecoveryMode::Strict;
  bool threw = false;
  try {
    reader = sdds::Reader::open(recoveryFile, strict);
    (void)reader.next();
  } catch (const sdds::FormatError &) {
    threw = true;
  }
  assert(threw);

  sdds::ReaderOptions limited;
  limited.limits.maxRows = 1;
  threw = false;
  try {
    reader = sdds::Reader::open(directory / "binary-little.sdds", limited);
    (void)reader.next();
  } catch (const sdds::LimitError &) {
    threw = true;
  }
  assert(threw);
}

void legacyFacade(const std::filesystem::path &path) {
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  SDDSFile file(path.string().c_str(), true);
  char parameter[] = "p";
  char column[] = "x";
  assert(file.defineParameter(parameter, SDDS_LONG64) == 0);
  assert(file.defineColumn(column, SDDS_DOUBLE) == 0);
  assert(file.setParameter(1, parameter, 7.0) == 1);
  double values[] = {1, 2};
  assert(file.setColumn(column, 1, 0, values, 2) == 1);
  assert(file.writeFile() == 1);

  SDDSFile input(path.string().c_str());
  assert(input.readFile() == 1);
  assert(input.pageCount() == 1);
  assert(input.getParameterInDouble(parameter, 1) == 7);
  assert(input.getColumnInDouble(column, 1)[1] == 2);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: test_sddspp ARTIFACT_DIRECTORY [SDDS_FILE...]\n";
    return 2;
  }
  const std::filesystem::path directory(argv[1]);
  std::filesystem::create_directories(directory);
  roundTrip(directory / "ascii.sdds", sdds::DataMode::Ascii, sdds::MajorOrder::Row,
            sdds::ByteOrder::Native, sdds::RowCountMode::Variable);
  roundTrip(directory / "ascii-no-count.sdds", sdds::DataMode::Ascii, sdds::MajorOrder::Row,
            sdds::ByteOrder::Native, sdds::RowCountMode::None);
  roundTrip(directory / "binary-little.sdds", sdds::DataMode::Binary, sdds::MajorOrder::Row,
            sdds::ByteOrder::Little, sdds::RowCountMode::Variable);
  roundTrip(directory / "binary-big.sdds", sdds::DataMode::Binary, sdds::MajorOrder::Column,
            sdds::ByteOrder::Big, sdds::RowCountMode::Variable);
  roundTrip(directory / "binary.sdds.gz", sdds::DataMode::Binary, sdds::MajorOrder::Row,
            sdds::ByteOrder::Little, sdds::RowCountMode::Variable);
  roundTrip(directory / "binary.sdds.xz", sdds::DataMode::Binary, sdds::MajorOrder::Column,
            sdds::ByteOrder::Little, sdds::RowCountMode::Variable);
  selectionAndGoto(directory / "binary-little.sdds");
  appendAndUpdate(directory / "append-update.sdds");
  const auto testData = std::filesystem::absolute(argv[0]).parent_path().parent_path() /
                        "testdata/include-main.sdds";
  includesLimitsAndRecovery(directory, testData);
  legacyFacade(directory / "legacy.sdds");
  for (int argument = 2; argument < argc; ++argument) {
    auto reader = sdds::Reader::open(argv[argument]);
    auto dataset = reader.readAll();
    assert(!dataset.pages.empty());
    reader.close();
  }
  std::cout << "SDDS++ C++17 integration tests passed\n";
  return 0;
}
