/**
 * @file test_sdds3_advanced.cc
 * @brief Acceptance tests for selective I/O, custom streams, live files, and transformations.
 *
 * @copyright Copyright (c) 2026 The University of Chicago
 * @license Distributed under the Software License Agreement in LICENSE.
 */

#include "SDDS.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

sdds::Layout makeLayout(sdds::DataMode mode = sdds::DataMode::Binary,
                        sdds::MajorOrder major = sdds::MajorOrder::Row,
                        sdds::RowCountMode rowCounts = sdds::RowCountMode::Variable) {
  sdds::DataOptions data;
  data.mode = mode;
  data.majorOrder = major;
  data.rowCountMode = rowCounts;
  sdds::LayoutBuilder builder;
  builder.setDataOptions(data)
      .addParameter({{"p", std::nullopt, "count", std::nullopt, std::nullopt,
                      sdds::Type::Int64}, std::nullopt})
      .addArray({{"a", std::nullopt, "m", std::nullopt, std::nullopt,
                  sdds::Type::Double}, 0, 1, std::nullopt})
      .addColumn({{"x", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                   sdds::Type::Int32}, 0})
      .addColumn({{"y", std::nullopt, "m", std::nullopt, std::nullopt,
                   sdds::Type::Double}, 0})
      .addColumn({{"label", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                   sdds::Type::String}, 0});
  return builder.build();
}

sdds::Page makePage(const std::shared_ptr<const sdds::Layout> &layout) {
  sdds::Page page(layout);
  page.setParameter("p", std::int64_t{7});
  page.setArray("a", {{3}, std::vector<double>{1.0, 2.0, 3.0}});
  page.setColumn("x", std::vector<std::int32_t>{1, 2, 3, 4, 5});
  page.setColumn("y", std::vector<double>{0.5, 1.5, 2.5, 3.5, 4.5});
  page.setColumn("label", std::vector<std::string>{"one", "two", "three", "four", "five"});
  return page;
}

void writeOnePage(const std::filesystem::path &path, const sdds::Layout &layout,
                  sdds::WriterOptions options = {}) {
  auto shared = std::make_shared<const sdds::Layout>(layout);
  auto writer = sdds::Writer::create(path, layout, options);
  writer.write(makePage(shared));
  writer.close();
}

void selectiveIoAndIndex(const std::filesystem::path &path,
                         const std::filesystem::path &asciiPath) {
  writeOnePage(path, makeLayout());
  auto reader = sdds::Reader::open(path);
  sdds::ReadRequest request;
  request.parameters = sdds::FieldSelection::only({"p"});
  request.arrays = sdds::FieldSelection::noFields();
  request.columns = sdds::FieldSelection::only({"y"});
  request.rows.first = 1;
  request.rows.count = 2;
  request.rows.stride = 2;
  auto page = reader.next(request);
  assert(page && page->rowCount() == 2);
  assert(page->parameterLoaded("p"));
  assert(!page->arrayLoaded("a"));
  assert(!page->columnLoaded("x"));
  assert(page->columnLoaded("y"));
  assert((page->columnAs<double>("y") == std::vector<double>{1.5, 3.5}));
  bool threw = false;
  try {
    (void)page->column("x");
  } catch (const sdds::StateError &) {
    threw = true;
  }
  assert(threw);
  reader.close();

  auto layout = makeLayout(sdds::DataMode::Ascii, sdds::MajorOrder::Row,
                           sdds::RowCountMode::None);
  writeOnePage(asciiPath, layout);
  reader = sdds::Reader::open(asciiPath);
  request = {};
  request.parameters = sdds::FieldSelection::noFields();
  request.arrays = sdds::FieldSelection::noFields();
  request.columns = sdds::FieldSelection::only({"x"});
  request.rows.last = 2;
  page = reader.next(request);
  assert(page && (page->columnAs<std::int32_t>("x") ==
                  std::vector<std::int32_t>{4, 5}));
  reader.close();

  auto twoPageWriter = sdds::Writer::append(path);
  twoPageWriter.write(makePage(std::make_shared<const sdds::Layout>(makeLayout())));
  twoPageWriter.close();
  reader = sdds::Reader::open(path);
  reader.buildPageIndex();
  assert(reader.indexedPageCount() == 2);
  reader.gotoPage(2);
  page = reader.next();
  assert(page && page->number() == 2);
  reader.close();
}

void memoryAndStreamIo() {
  const auto layout = makeLayout();
  auto shared = std::make_shared<const sdds::Layout>(layout);
  for (const auto compression : {sdds::Compression::None, sdds::Compression::Gzip,
                                 sdds::Compression::Xz, sdds::Compression::Lzma}) {
    std::vector<std::uint8_t> bytes;
    sdds::WriterOptions writerOptions;
    writerOptions.compression = compression;
    writerOptions.gzipLevel = 1;
    writerOptions.lzmaPreset = 1;
    auto writer = sdds::Writer::toSink(sdds::outputToMemory(bytes), layout, "memory",
                                       writerOptions);
    writer.write(makePage(shared));
    writer.close();
    assert(!bytes.empty());

    sdds::ReaderOptions readerOptions;
    readerOptions.compression = compression;
    auto reader = sdds::Reader::fromSource(sdds::inputFromMemory(bytes.data(), bytes.size()),
                                           "memory", readerOptions);
    auto page = reader.next();
    assert(page && page->columnAs<std::int32_t>("x")[4] == 5);
    reader.close();
  }

  std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
  auto writer = sdds::Writer::toSink(sdds::outputToStream(stream), layout, "stringstream");
  writer.write(makePage(shared));
  writer.close();
  stream.seekg(0);
  auto reader = sdds::Reader::fromSource(sdds::inputFromStream(stream), "stringstream");
  assert(reader.next());
  reader.close();
}

void sourceCapabilities(const std::filesystem::path &inputPath,
                        const std::filesystem::path &outputPath) {
  auto source = sdds::inputFromPath(inputPath);
  const auto inputCapabilities = source->capabilities();
  assert(inputCapabilities.read && inputCapabilities.seek && inputCapabilities.reopen);
  std::uint8_t bytes[4]{};
  assert(source->read(bytes, sizeof(bytes)) == sizeof(bytes));
  source->close();
  source->reopen();
  assert(source->tell() == sizeof(bytes));
  source->close();

  FILE *borrowedInput = std::fopen(inputPath.string().c_str(), "rb");
  assert(borrowedInput);
  auto fileSource = sdds::inputFromFile(borrowedInput);
  assert(fileSource->read(bytes, sizeof(bytes)) == sizeof(bytes));
  fileSource->close();
  assert(std::fseek(borrowedInput, 0, SEEK_SET) == 0);
  std::fclose(borrowedInput);

  auto sink = sdds::outputToPath(outputPath);
  const auto outputCapabilities = sink->capabilities();
  assert(outputCapabilities.write && outputCapabilities.seek && outputCapabilities.truncate &&
         outputCapabilities.flush && outputCapabilities.sync && outputCapabilities.reopen);
  const char first[] = "abc";
  sink->write(first, sizeof(first) - 1);
  sink->close();
  sink->reopen();
  const char second[] = "def";
  sink->write(second, sizeof(second) - 1);
  sink->close();
  assert(std::filesystem::file_size(outputPath) == 6);

  FILE *borrowedOutput = std::fopen(outputPath.string().c_str(), "wb+");
  assert(borrowedOutput);
  const auto layout = makeLayout();
  auto writer = sdds::Writer::toSink(sdds::outputToFile(borrowedOutput), layout,
                                     "borrowed-FILE");
  writer.write(makePage(std::make_shared<const sdds::Layout>(layout)));
  writer.close();
  assert(std::fseek(borrowedOutput, 0, SEEK_SET) == 0);
  char header[5]{};
  assert(std::fread(header, 1, 5, borrowedOutput) == 5);
  assert(std::string(header, 5) == "SDDS5");
  std::fclose(borrowedOutput);
}

void headerlessIo(const std::filesystem::path &path) {
  sdds::DataOptions data;
  data.mode = sdds::DataMode::Ascii;
  sdds::LayoutBuilder builder;
  builder.setDataOptions(data)
      .addColumn({{"x", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                   sdds::Type::Int32}, 0});
  const auto layout = builder.build();
  {
    std::ofstream output(path, std::ios::binary);
    output << "3\n1\n2\n3\n";
  }
  auto reader = sdds::Reader::openHeaderless(path, layout);
  auto page = reader.next();
  assert(page && (page->columnAs<std::int32_t>("x") ==
                  std::vector<std::int32_t>{1, 2, 3}));
  reader.close();
}

void disconnectReconnectAndLocks(const std::filesystem::path &path,
                                 const std::filesystem::path &replacement,
                                 const std::filesystem::path &lockPath,
                                 const std::filesystem::path &saved) {
  writeOnePage(path, makeLayout());
  auto reader = sdds::Reader::open(path);
  reader.disconnect();
  reader.reconnect();
  assert(reader.next());
  reader.close();

  auto writer = sdds::Writer::create(replacement, makeLayout());
  writer.disconnect();
  writer.reconnect();
  writer.write(makePage(std::make_shared<const sdds::Layout>(makeLayout())));
  writer.close();

  writer = sdds::Writer::create(lockPath, makeLayout());
  bool threw = false;
  try {
    auto conflicting = sdds::Writer::append(lockPath);
    conflicting.close();
  } catch (const sdds::IoError &) {
    threw = true;
  }
  assert(threw);
  writer.close();

  reader = sdds::Reader::open(path);
  reader.disconnect();
  std::filesystem::rename(path, saved);
  std::filesystem::rename(replacement, path);
  threw = false;
  try {
    reader.reconnect();
  } catch (const sdds::StateError &) {
    threw = true;
  }
  assert(threw);
  reader.close();
}

sdds::Layout liveLayout(sdds::MajorOrder major = sdds::MajorOrder::Row) {
  sdds::DataOptions data;
  data.mode = sdds::DataMode::Binary;
  data.majorOrder = major;
  sdds::LayoutBuilder builder;
  builder.setDataOptions(data)
      .addColumn({{"x", std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                   sdds::Type::Int32}, 0});
  return builder.build();
}

void liveRowsAndUpdateStrategies(const std::filesystem::path &path,
                                 const std::filesystem::path &columnMajorPath,
                                 const std::filesystem::path &compressedPath) {
  auto layout = liveLayout();
  auto writer = sdds::Writer::create(path, layout);
  writer.beginPage(2);
  writer.setColumn("x", std::vector<std::int32_t>{1, 2});
  writer.commitPage();
  writer.close();

  auto follower = sdds::Reader::open(path);
  assert(follower.next());
  assert(!follower.next());
  writer = sdds::Writer::appendToLastPage(path, 1);
  writer.setColumn("x", std::vector<std::int32_t>{3, 4}, 2);
  writer.updatePage(true);
  writer.close();
  auto delta = follower.readNewRows();
  assert(delta && delta->pageNumber == 1 && delta->firstRow == 2);
  assert((delta->page.columnAs<std::int32_t>("x") == std::vector<std::int32_t>{3, 4}));
  assert(!follower.readNewRows());
  follower.close();

  layout = liveLayout(sdds::MajorOrder::Column);
  writer = sdds::Writer::create(columnMajorPath, layout);
  writer.beginPage(2);
  writer.setColumn("x", std::vector<std::int32_t>{1, 2});
  writer.commitPage();
  writer.close();
  sdds::WriterOptions options;
  options.updateStrategy = sdds::UpdateStrategy::InPlaceOnly;
  bool threw = false;
  try {
    writer = sdds::Writer::appendToLastPage(columnMajorPath, 1, options);
  } catch (const sdds::StateError &) {
    threw = true;
  }
  assert(threw);
  options.updateStrategy = sdds::UpdateStrategy::AtomicRewrite;
  writer = sdds::Writer::appendToLastPage(columnMajorPath, 1, options);
  writer.setColumn("x", std::vector<std::int32_t>{3}, 2);
  writer.updatePage(true);
  writer.close();
  auto reader = sdds::Reader::open(columnMajorPath);
  auto page = reader.next();
  assert(page && (page->columnAs<std::int32_t>("x") ==
                  std::vector<std::int32_t>{1, 2, 3}));
  reader.close();

  layout = liveLayout();
  writer = sdds::Writer::create(compressedPath, layout);
  writer.beginPage(2);
  writer.setColumn("x", std::vector<std::int32_t>{1, 2});
  writer.commitPage();
  writer.close();
  writer = sdds::Writer::appendToLastPage(compressedPath, 1);
  writer.setColumn("x", std::vector<std::int32_t>{3}, 2);
  writer.updatePage(true);
  writer.close();
  reader = sdds::Reader::open(compressedPath);
  page = reader.next();
  assert(page && (page->columnAs<std::int32_t>("x") ==
                  std::vector<std::int32_t>{1, 2, 3}));
  reader.close();
}

void transformations(const std::filesystem::path &input,
                     const std::filesystem::path &output) {
  writeOnePage(input, makeLayout());
  auto reader = sdds::Reader::open(input);
  auto page = reader.next();
  assert(page);
  assert(page->row(2).get<std::int32_t>("x") == 3);
  auto high = page->matchRows("x", [](const sdds::Scalar &value) {
    return std::get<std::int32_t>(value) >= 3;
  });
  auto odd = page->matchRows<std::int32_t>("x", [](std::int32_t value) {
    return value % 2 != 0;
  });
  auto filtered = page->filtered(high & odd);
  assert((filtered.columnAs<std::int32_t>("x") == std::vector<std::int32_t>{3, 5}));
  auto matrix = page->matrix<double>({"x", "y"}, sdds::ConversionMode::Checked);
  assert(matrix.rows == 5 && matrix.columns == 2);
  assert(matrix(3, 0) == 4.0 && matrix(3, 1) == 3.5);
  auto converted = sdds::convertUnits(*page, sdds::FieldKind::Column, "y", "cm", 100.0L);
  assert(converted.layout().columns[1].units == std::optional<std::string>("cm"));
  assert(converted.columnAs<double>("y")[1] == 150.0);

  bool threw = false;
  try {
    (void)sdds::convertScalar(std::numeric_limits<std::uint64_t>::max(), sdds::Type::Int32);
  } catch (const sdds::TypeError &) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)sdds::convertScalar(1.5, sdds::Type::Int32);
  } catch (const sdds::TypeError &) {
    threw = true;
  }
  assert(threw);
  assert(std::get<std::int32_t>(sdds::convertScalar(
             1.5, sdds::Type::Int32, sdds::RoundingMode::Nearest)) == 2);

  sdds::LayoutEditor editor(makeLayout());
  const auto edited = editor.renameColumn("y", "distance").dropColumn("label").build();
  assert(edited.columns.size() == 2 && edited.columns[1].name == "distance");
  reader.close();

  reader = sdds::Reader::open(input);
  sdds::CopyOptions copy;
  copy.transformLayout = [](const sdds::Layout &layout) {
    return sdds::LayoutEditor(layout).renameColumn("y", "distance").build();
  };
  sdds::copyDataset(reader, output, copy);
  reader.close();
  reader = sdds::Reader::open(output);
  page = reader.next();
  assert(page && page->columnAs<double>("distance")[4] == 4.5);
  reader.close();
}

void limitsAndMalformedProjection(const std::filesystem::path &path,
                                  const std::filesystem::path &truncated) {
  writeOnePage(path, makeLayout());
  sdds::ReaderOptions options;
  options.limits.maxProjectedFields = 1;
  bool threw = false;
  try {
    auto reader = sdds::Reader::open(path, options);
    (void)reader.next();
  } catch (const sdds::LimitError &) {
    threw = true;
  }
  assert(threw);

  options = {};
  options.limits.maxTransformationElements = 4;
  auto reader = sdds::Reader::open(path, options);
  auto page = reader.next();
  threw = false;
  try {
    (void)page->matrix<double>({"x"}, sdds::ConversionMode::Checked);
  } catch (const sdds::LimitError &) {
    threw = true;
  }
  assert(threw);
  reader.close();

  std::filesystem::copy_file(path, truncated,
                             std::filesystem::copy_options::overwrite_existing);
  std::filesystem::resize_file(truncated, std::filesystem::file_size(truncated) - 1);
  reader = sdds::Reader::open(truncated);
  sdds::ReadRequest request;
  request.parameters = sdds::FieldSelection::noFields();
  request.arrays = sdds::FieldSelection::noFields();
  request.columns = sdds::FieldSelection::noFields();
  threw = false;
  try {
    (void)reader.next(request);
  } catch (const sdds::FormatError &error) {
    threw = true;
    assert(error.path() == truncated);
    assert(error.page() == 1);
    assert(error.offset().has_value());
    assert(error.row().has_value());
    assert(error.field() == std::optional<std::string>("label"));
  }
  assert(threw);
  reader.close();
}

}  // namespace

int main(int argc, char **argv) {
  assert(sdds::cppApiVersion == 2);
  if (argc != 2) return 2;
  const std::filesystem::path directory(argv[1]);
  std::filesystem::create_directories(directory);
  selectiveIoAndIndex(directory / "selective.sdds", directory / "last-rows.sdds");
  memoryAndStreamIo();
  sourceCapabilities(directory / "selective.sdds", directory / "raw-output.bin");
  headerlessIo(directory / "headerless.dat");
  disconnectReconnectAndLocks(directory / "reconnect.sdds",
                              directory / "replacement.sdds",
                              directory / "locked.sdds",
                              directory / "saved.sdds");
  liveRowsAndUpdateStrategies(directory / "live.sdds", directory / "column-update.sdds",
                              directory / "compressed-update.sdds.gz");
  transformations(directory / "transform-input.sdds", directory / "transform-output.sdds");
  limitsAndMalformedProjection(directory / "limits.sdds", directory / "truncated.sdds");
  return 0;
}
