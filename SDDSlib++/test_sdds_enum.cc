/**
 * @file test_sdds_enum.cc
 * @brief SDDS6 C/C++ differential, categorical and mapping regression tests.
 * @copyright Copyright (c) 2026 The University of Chicago.
 * @license Software License Agreement in LICENSE.
 * @author SDDS maintainers
 */
#include "SDDS.hpp"
#include "SDDS.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstdio>

/** @brief Both libraries read every fixed character byte from an SDDS6 file. */
static void checkFixedCharacters(const std::filesystem::path &path) {
  SDDS_DATASET dataset{};
  auto filename = path.string();
  assert(SDDS_InitializeInput(&dataset, filename.data()));
  assert(dataset.layout.version == 6 && SDDS_ReadPage(&dataset) > 0);
  for (int byte = 0; byte < 256; ++byte)
    assert(*static_cast<unsigned char *>(dataset.parameter[byte]) == byte);
  assert(SDDS_ReadPage(&dataset) < 0);
  assert(SDDS_Terminate(&dataset));
  auto reader = sdds::Reader::open(path);
  const auto page = reader.next();
  assert(page && reader.layout().version == 6);
  for (int byte = 0; byte < 256; ++byte)
    assert(static_cast<unsigned char>(page->parameterAs<char>("fixedByte" + std::to_string(byte))) == byte);
  assert(!reader.next());
}

/** @brief Fixed characters retain their data-token escaping with native enums. */
static void testFixedCharacterParameters(const std::filesystem::path &outputDirectory) {
  using namespace sdds;
  const auto directory = outputDirectory / "fixed-characters";
  std::filesystem::create_directories(directory);
  for (const auto mode : {DataMode::Ascii, DataMode::Binary}) {
    for (const auto *suffix : {".sdds", ".sdds.gz", ".sdds.xz", ".sdds.lzma"}) {
      const std::string stem = std::string(mode == DataMode::Ascii ? "ascii" : "binary") + suffix;
      const auto cPath = directory / ("c-" + stem), cppPath = directory / ("cpp-" + stem);
      SDDS_DATASET dataset{};
      auto filename = cPath.string();
      assert(SDDS_InitializeOutput(&dataset, mode == DataMode::Ascii ? SDDS_ASCII : SDDS_BINARY,
                                   1, nullptr, nullptr, filename.c_str()));
      DataOptions data;
      data.mode = mode;
      LayoutBuilder builder;
      builder.setDataOptions(data);
      for (int byte = 0; byte < 256; ++byte) {
        const auto name = "fixedByte" + std::to_string(byte);
        char encoded[5];
        std::snprintf(encoded, sizeof(encoded), "\\%03o", static_cast<unsigned>(byte));
        assert(SDDS_DefineParameter(&dataset, name.c_str(), nullptr, nullptr, nullptr,
                                    nullptr, SDDS_CHARACTER, encoded) == byte);
        ParameterDefinition fixed;
        fixed.name = name;
        fixed.type = Type::Character;
        fixed.fixedValue = std::string(1, static_cast<char>(byte));
        builder.addParameter(fixed);
      }
      assert(SDDS_DefineEnumColumn(&dataset, "characterEnumState", nullptr, nullptr, nullptr,
                                   nullptr, "0:OFF", 0) == 0);
      assert(SDDS_WriteLayout(&dataset) && SDDS_StartPage(&dataset, 1));
      std::int32_t state = 0;
      assert(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, &state, 1, 0));
      assert(SDDS_WritePage(&dataset) && SDDS_Terminate(&dataset));
      checkFixedCharacters(cPath);

      ColumnDefinition column;
      column.name = "characterEnumState";
      column.type = Type::Enum;
      column.enumDefinition = std::make_shared<const EnumDefinition>("0:OFF");
      const auto layout = builder.addColumn(column).build();
      auto writer = Writer::create(cppPath, layout);
      writer.beginPage(1);
      writer.setColumn(column.name, std::vector<EnumValue>{{0}});
      writer.commitPage();
      writer.close();
      checkFixedCharacters(cppPath);
    }
  }
}

/** @brief Page transformations preserve header-resident enum parameters. */
static void checkFixedEnumTransforms(const sdds::Page &page, std::int32_t value) {
  using namespace sdds;
  assert(page.parameterAs<EnumValue>("fixedState").code == 10);
  const auto filtered = page.filtered(RowMask(static_cast<std::size_t>(page.rowCount()), true));
  assert(filtered.parameterAs<EnumValue>("fixedState").code == 10);
  assert(filtered.columnAs<std::int32_t>("value").at(0) == value);
  const auto projected = page.projected(ReadRequest{});
  assert(projected.parameterAs<EnumValue>("fixedState").code == 10);
  assert(projected.columnAs<std::int32_t>("value").at(0) == value);
  const auto converted = convertUnits(page, FieldKind::Column, "value", "m", 2);
  assert(converted.parameterAs<EnumValue>("fixedState").code == 10);
  assert(converted.columnAs<std::int32_t>("value").at(0) == 2 * value);
}

/** @brief Fixed enum headers initialize pages and survive ordinary transformations. */
static void testFixedEnumParameters(const std::filesystem::path &outputDirectory) {
  using namespace sdds;
  for (const auto mode : {DataMode::Ascii, DataMode::Binary}) {
    for (const bool mapsZero : {false, true}) {
      DataOptions data;
      data.mode = mode;
      ParameterDefinition fixed;
      fixed.name = "fixedState";
      fixed.type = Type::Enum;
      fixed.enumDefinition = std::make_shared<const EnumDefinition>(mapsZero ? "0:OFF,10:ON" : "10:ON");
      fixed.fixedValue = "10";
      ColumnDefinition column;
      column.name = "value";
      column.type = Type::Int32;
      const auto layout = LayoutBuilder().setDataOptions(data).addParameter(fixed).addColumn(column).build();
      const std::string stem = std::string(mode == DataMode::Ascii ? "fixed-enum-ascii" : "fixed-enum-binary") +
          (mapsZero ? "-with-zero" : "");
      const auto path = outputDirectory / (stem + ".sdds");
      auto writer = Writer::create(path, layout);
      writer.beginPage(1);
      writer.setColumn("value", std::vector<std::int32_t>{1});
      writer.commitPage();
      Page page(std::make_shared<const Layout>(layout));
      page.setColumn("value", std::vector<std::int32_t>{2});
      checkFixedEnumTransforms(page, 2);
      writer.write(page);
      writer.close();

      auto reader = Reader::open(path);
      for (std::int32_t value = 1; value <= 2; ++value) {
        const auto restored = reader.next();
        assert(restored && restored->parameterAs<EnumValue>("fixedState").code == 10);
        assert(restored->columnAs<std::int32_t>("value").at(0) == value);
        checkFixedEnumTransforms(*restored, value);
      }
      assert(!reader.next());
      reader.close();

      auto projectedReader = Reader::open(path);
      ReadRequest request;
      request.parameters = FieldSelection::noFields();
      const auto projected = projectedReader.next(request);
      assert(projected && !projected->parameterLoaded("fixedState"));
      assert(!projected->filtered(RowMask(1, true)).parameterLoaded("fixedState"));
      assert(!projected->projected(ReadRequest{}).parameterLoaded("fixedState"));
      assert(!convertUnits(*projected, FieldKind::Column, "value", "m", 2).parameterLoaded("fixedState"));
      projectedReader.close();

      SDDS_DATASET dataset{};
      auto filename = path.string();
      assert(SDDS_InitializeInput(&dataset, filename.data()));
      for (std::int32_t value = 1; value <= 2; ++value) {
        assert(SDDS_ReadPage(&dataset) > 0);
        assert(*static_cast<std::int32_t *>(dataset.parameter[0]) == 10);
        assert(static_cast<std::int32_t *>(dataset.data[0])[0] == value);
      }
      assert(SDDS_ReadPage(&dataset) < 0);
      assert(SDDS_Terminate(&dataset));

      fixed.enumDefinition = std::make_shared<const EnumDefinition>("10:DIFFERENT");
      const auto incompatible = LayoutBuilder().setDataOptions(data).addParameter(fixed).addColumn(column).build();
      Page incompatiblePage(std::make_shared<const Layout>(incompatible));
      auto checkedWriter = Writer::create(outputDirectory / (stem + "-incompatible.sdds"), layout);
      bool rejected = false;
      try { checkedWriter.write(incompatiblePage); } catch (const TypeError &) { rejected = true; }
      assert(rejected);
    }
  }
}

/** @brief Failed partial enum assignments preserve a writable staged page. */
static void testPartialEnumAssignments(const std::filesystem::path &outputDirectory) {
  using namespace sdds;
  for (const auto mode : {DataMode::Ascii, DataMode::Binary}) {
    for (const bool mapsZero : {false, true}) {
      DataOptions data;
      data.mode = mode;
      ColumnDefinition column;
      column.name = "state";
      column.type = Type::Enum;
      column.enumDefinition = std::make_shared<const EnumDefinition>(mapsZero ? "0:OFF,10:ON" : "10:ON");
      const auto layout = LayoutBuilder().setDataOptions(data).addColumn(column).build();
      const auto path = outputDirectory / (std::string("partial-enum-") +
          (mode == DataMode::Ascii ? "ascii" : "binary") + (mapsZero ? "-with-zero.sdds" : ".sdds"));
      auto writer = Writer::create(path, layout);
      writer.beginPage(2);
      writer.setColumn("state", std::vector<EnumValue>{{10}, {10}});
      for (const std::int64_t startRow : {1, 3}) {
        bool rejected = false;
        try { writer.setColumn("state", std::vector<EnumValue>{{10}, {99}}, startRow); }
        catch (const TypeError &) { rejected = true; }
        assert(rejected && writer.rowsPresent() == 2);
      }
      if (!mapsZero) {
        bool rejected = false;
        try { writer.setColumn("state", std::vector<EnumValue>{{10}}, 3); }
        catch (const TypeError &) { rejected = true; }
        assert(rejected && writer.rowsPresent() == 2);
      }
      writer.commitPage();
      writer.beginPage(3);
      writer.setColumn("state", std::vector<EnumValue>{{10}, {10}});
      writer.setColumn("state", std::vector<EnumValue>{{10}}, mapsZero ? 3 : 2);
      writer.commitPage();
      writer.close();

      auto reader = Reader::open(path);
      const auto unchanged = reader.next();
      assert(unchanged && unchanged->columnAs<EnumValue>("state") == std::vector<EnumValue>({{10}, {10}}));
      const auto extended = reader.next();
      const std::vector<EnumValue> expected = mapsZero ? std::vector<EnumValue>{{10}, {10}, {0}, {10}}
                                                      : std::vector<EnumValue>{{10}, {10}, {10}};
      assert(extended && extended->columnAs<EnumValue>("state") == expected);
      assert(!reader.next());
    }
  }
}

/** @brief Fixed enum codes accept the C library's leading and trailing whitespace. */
static void testFixedEnumWhitespace(const std::filesystem::path &outputDirectory) {
  using namespace sdds;
  const auto check = [](const std::filesystem::path &path, std::int32_t expected) {
    SDDS_DATASET dataset{};
    auto filename = path.string();
    assert(SDDS_InitializeInput(&dataset, filename.data()));
    assert(SDDS_ReadPage(&dataset) > 0);
    assert(*static_cast<std::int32_t *>(dataset.parameter[0]) == expected);
    assert(SDDS_ReadPage(&dataset) < 0);
    assert(SDDS_Terminate(&dataset));
    auto reader = Reader::open(path);
    const auto page = reader.next();
    assert(page && page->parameterAs<EnumValue>("fixedState").code == expected);
    assert(page->columnAs<std::int32_t>("value").at(0) == 1);
    assert(!reader.next());
  };
  for (const auto mode : {DataMode::Ascii, DataMode::Binary}) {
    int index = 0;
    for (std::string fixedValue : {"10\f", "\v10", " \t\f10\v \t", "\v\f-1\f\v"}) {
      // Use both positive and negative codes without requiring zero in the dictionary.
      const std::int32_t expected = fixedValue.find('-') == std::string::npos ? 10 : -1;
      const std::string stem = std::string(mode == DataMode::Ascii ? "ascii" : "binary") +
          "-fixed-enum-whitespace-" + std::to_string(index++);
      const auto cPath = outputDirectory / ("c-" + stem + ".sdds");
      SDDS_DATASET dataset{};
      const auto filename = cPath.string();
      assert(SDDS_InitializeOutput(&dataset, mode == DataMode::Ascii ? SDDS_ASCII : SDDS_BINARY,
                                   1, nullptr, nullptr, filename.c_str()));
      assert(SDDS_DefineEnumParameter(&dataset, "fixedState", nullptr, nullptr, nullptr,
                                      nullptr, "-1:FAULT,10:ON", fixedValue.data()) == 0);
      assert(SDDS_DefineColumn(&dataset, "value", nullptr, nullptr, nullptr, nullptr, SDDS_LONG, 0) == 0);
      assert(SDDS_WriteLayout(&dataset) && SDDS_StartPage(&dataset, 1));
      std::int32_t value = 1;
      assert(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, &value, 1, 0));
      assert(SDDS_WritePage(&dataset) && SDDS_Terminate(&dataset));
      check(cPath, expected);

      DataOptions data;
      data.mode = mode;
      ParameterDefinition fixed;
      fixed.name = "fixedState";
      fixed.type = Type::Enum;
      fixed.enumDefinition = std::make_shared<const EnumDefinition>("-1:FAULT,10:ON");
      fixed.fixedValue = fixedValue;
      ColumnDefinition column;
      column.name = "value";
      column.type = Type::Int32;
      const auto layout = LayoutBuilder().setDataOptions(data).addParameter(fixed).addColumn(column).build();
      const auto cppPath = outputDirectory / ("cpp-" + stem + ".sdds");
      auto writer = Writer::create(cppPath, layout);
      writer.beginPage(1);
      writer.setColumn("value", std::vector<std::int32_t>{1});
      writer.commitPage();
      writer.close();
      check(cppPath, expected);
    }
  }
}

/** @brief Discarded enum arrays are streamed and every code is still checked. */
static void testProjectedAsciiEnumArrays(const std::filesystem::path &outputDirectory) {
  using namespace sdds;
  for (const std::string lastToken : {"10", "99", "ON", "10 0"}) {
    const bool valid = lastToken == "10";
    const std::size_t count = valid ? 65536 : 128;
    const auto path = outputDirectory / ("projected-enum-array-" +
        std::to_string(valid ? 0 : lastToken == "99" ? 1 : lastToken == "ON" ? 2 : 3) + ".sdds");
    {
      std::ofstream file(path);
      file << "SDDS6\n"
           << "&array name=states,type=enum,enum_values=\"0:OFF,10:ON\",dimensions=1 &end\n"
           << "&column name=value,type=long &end\n"
           << "&data mode=ascii &end\n"
           << "! page number 1\n" << count << '\n';
      for (std::size_t index = 1; index < count; ++index)
        file << (index % 2 ? "0\n" : "10\n");
      file << lastToken << "\n1\n123\n"
           << "! page number 2\n0\n1\n456\n";
      assert(file);
    }
    auto reader = Reader::open(path);
    ReadRequest request;
    request.arrays = FieldSelection::noFields();
    bool rejected = false;
    try {
      const auto page = reader.next(request);
      assert(page && !page->arrayLoaded("states"));
      assert(page->columnAs<std::int32_t>("value").at(0) == 123);
      const auto next = reader.next(request);
      assert(next && !next->arrayLoaded("states"));
      assert(next->columnAs<std::int32_t>("value").at(0) == 456);
      assert(!reader.next(request));
    } catch (const Error &error) {
      assert(error.field() == "states" && error.page() == 1 && error.path() == path);
      rejected = true;
    }
    assert(rejected != valid);
  }
}

/** @brief Records bulk reads through the public custom-source interface. */
class EnumReadSource final : public sdds::InputSource {
 public:
  EnumReadSource(std::unique_ptr<sdds::InputSource> source, std::size_t &largestRead)
      : source_(std::move(source)), largestRead_(largestRead) {}
  std::size_t read(void *data, std::size_t size) override {
    largestRead_ = std::max(largestRead_, size);
    return source_->read(data, size);
  }
  bool eof() const override { return source_->eof(); }
  sdds::SourceCapabilities capabilities() const noexcept override { return source_->capabilities(); }
  std::uint64_t tell() const override { return source_->tell(); }
  void seek(std::uint64_t offset) override { source_->seek(offset); }
  void close() override { source_->close(); }

 private:
  std::unique_ptr<sdds::InputSource> source_;
  std::size_t &largestRead_;
};

/** @brief Fixed-width numeric columns surround selected and discarded enums. */
static sdds::Layout enumProjectionLayout(sdds::MajorOrder major, sdds::ByteOrder order) {
  using namespace sdds;
  DataOptions data;
  data.mode = DataMode::Binary;
  data.majorOrder = major;
  data.byteOrder = order;
  LayoutBuilder builder;
  builder.setDataOptions(data);
  for (const auto *name : {"prefix", "state", "middle", "unusedState", "suffix"}) {
    ColumnDefinition column;
    column.name = name;
    column.type = column.name == "state" || column.name == "unusedState" ? Type::Enum : Type::Double;
    if (column.type == Type::Enum)
      column.enumDefinition = std::make_shared<const EnumDefinition>("0:OFF,10:ON,-1:FAULT");
    builder.addColumn(column);
  }
  return builder.build();
}

/** @brief Produces enum pages with a deterministic categorical value pattern. */
static std::vector<std::uint8_t> enumProjectionBytes(const sdds::Layout &layout,
                                                    const std::vector<std::size_t> &pageRows) {
  using namespace sdds;
  std::vector<std::uint8_t> bytes;
  auto writer = Writer::toSink(outputToMemory(bytes), layout, "enum-projection");
  const std::int32_t codes[] = {0, 10, -1};
  for (std::size_t page = 0; page < pageRows.size(); ++page) {
    const auto rows = pageRows[page];
    std::vector<double> numbers(rows);
    std::vector<EnumValue> states(rows);
    for (std::size_t row = 0; row < rows; ++row) {
      numbers[row] = static_cast<double>(row + page);
      states[row] = EnumValue{codes[(row + page) % 3]};
    }
    writer.beginPage(static_cast<std::int64_t>(rows));
    for (const auto *name : {"prefix", "middle", "suffix"}) writer.setColumn(name, numbers);
    for (const auto *name : {"state", "unusedState"}) writer.setColumn(name, states);
    writer.commitPage();
  }
  writer.close();
  return bytes;
}

/** @brief Replaces an enum code in a single-page binary fixture without changing its mapping. */
static void corruptProjectedEnum(std::vector<std::uint8_t> &bytes, sdds::MajorOrder major,
                                 sdds::ByteOrder order, std::size_t rows,
                                 std::size_t column, std::size_t row) {
  const std::size_t offsets[] = {0, 8, 12, 20, 24};
  constexpr std::size_t rowBytes = 32;
  const std::size_t dataStart = bytes.size() - sizeof(std::int32_t) - rowBytes * rows;
  const std::size_t valueOffset = dataStart + sizeof(std::int32_t) +
      (major == sdds::MajorOrder::Row ? row * rowBytes + offsets[column]
                                     : offsets[column] * rows + row * sizeof(std::int32_t));
  for (std::size_t byte = 0; byte < sizeof(std::int32_t); ++byte) {
    const auto shift = 8 * (order == sdds::ByteOrder::Little ? byte : sizeof(std::int32_t) - byte - 1);
    bytes[valueOffset + byte] = static_cast<std::uint8_t>(99U >> shift);
  }
}

/** @brief Enum projections keep bulk I/O and validate discarded columns and rows. */
static void testProjectedBinaryEnumColumns() {
  using namespace sdds;
  for (const auto major : {MajorOrder::Row, MajorOrder::Column}) {
    for (const auto order : {ByteOrder::Little, ByteOrder::Big}) {
      const auto layout = enumProjectionLayout(major, order);
      const auto bytes = enumProjectionBytes(layout, {70000, 3});
      std::size_t largestRead = 0;
      auto source = std::make_unique<EnumReadSource>(inputFromMemory(bytes.data(), bytes.size()), largestRead);
      ReaderOptions options;
      options.recovery = RecoveryMode::Strict;
      auto reader = Reader::fromSource(std::move(source), "enum-projection", options);
      ReadRequest request;
      request.columns = FieldSelection::only({"state"});
      request.rows.first = 1;
      request.rows.stride = 3;
      request.rows.count = 4;
      const auto page = reader.next(request);
      assert(page && page->rowCount() == 4);
      assert(page->columnAs<EnumValue>("state") == std::vector<EnumValue>(4, EnumValue{10}));
      for (const auto *name : {"prefix", "middle", "unusedState", "suffix"})
        assert(!page->columnLoaded(name));
      // Scalar reads only fill the 64 KiB input buffer. Larger requests verify
      // that an enum no longer disables the binary bulk-reading path.
      assert(largestRead > 64U * 1024U);
      request.rows = {};
      const auto next = reader.next(request);
      assert(next && next->columnAs<EnumValue>("state") == std::vector<EnumValue>({{10}, {-1}, {0}}));
      assert(!reader.next(request));

      ReadRequest sparse;
      sparse.columns = FieldSelection::only({"state"});
      sparse.rows.first = 1;
      sparse.rows.stride = 2;
      sparse.rows.count = 2;
      ReadRequest last;
      last.columns = FieldSelection::only({"state"});
      last.rows.last = 1;
      ReadRequest numeric;
      numeric.columns = FieldSelection::only({"prefix"});
      ReadRequest none;
      none.columns = FieldSelection::noFields();
      for (const auto recovery : {RecoveryMode::Strict, RecoveryMode::Recover}) {
        options.recovery = recovery;
        for (const auto &projection : {sparse, last, numeric, none}) {
          const auto valid = enumProjectionBytes(layout, {5});
          auto validReader = Reader::fromSource(inputFromMemory(valid.data(), valid.size()), "valid-enum-projection", options);
          assert(validReader.next(projection));
          assert(!validReader.next(projection));
          for (const std::size_t column : {1U, 3U}) {
            for (const std::size_t row : {0U, 2U, 4U}) {
              auto invalid = valid;
              corruptProjectedEnum(invalid, major, order, 5, column, row);
              auto invalidReader = Reader::fromSource(inputFromMemory(invalid.data(), invalid.size()), "bad-enum-projection", options);
              bool rejected = false;
              try { invalidReader.next(projection); }
              catch (const Error &error) {
                assert(error.field() == layout.columns[column].name && error.row() == static_cast<std::int64_t>(row));
                assert(error.page() == 1 && error.path() == "bad-enum-projection");
                rejected = true;
              }
              assert(rejected);
            }
          }
        }
      }

      // Recovery may drop an incomplete final row, but an invalid complete
      // enum preceding the truncated numeric field still rejects the input.
      options.recovery = RecoveryMode::Recover;
      for (const auto &projection : {sparse, none}) {
        auto truncated = enumProjectionBytes(layout, {5});
        truncated.resize(truncated.size() - 4);
        auto recovering = Reader::fromSource(inputFromMemory(truncated.data(), truncated.size()), "truncated-enum-projection", options);
        const auto recovered = recovering.next(projection);
        assert(recovered && recovered->recovered());
        assert(recovered->rowCount() == (projection.columns.all || !projection.columns.names.empty() ? 2 : 4));
        for (const std::size_t column : {1U, 3U}) {
          auto invalid = enumProjectionBytes(layout, {5});
          corruptProjectedEnum(invalid, major, order, 5, column, 4);
          invalid.resize(invalid.size() - 4);
          auto invalidReader = Reader::fromSource(inputFromMemory(invalid.data(), invalid.size()), "truncated-bad-enum-projection", options);
          bool rejected = false;
          try { invalidReader.next(projection); }
          catch (const Error &error) {
            assert(error.field() == layout.columns[column].name && error.row() == 4 && error.page() == 1);
            rejected = true;
          }
          assert(rejected);
        }
      }
    }
  }
}

/** @brief Empty and populated vectors enforce the same enum conversion contract. */
static void testEnumBulkConversions() {
  using namespace sdds;
  const EnumDefinition source("0:OFF,10:ON"), target("7:OFF,8:ON");
  const auto rejects = [](auto operation) {
    bool rejected = false;
    try { operation(); } catch (const TypeError &) { rejected = true; }
    assert(rejected);
  };
  for (const std::size_t count : {0, 1}) {
    const Values enums = std::vector<EnumValue>(count, EnumValue{10});
    const Values integers = std::vector<std::int32_t>(count, 10);
    const Values labels = std::vector<std::string>(count, "ON");
    const Values numbers = std::vector<double>(count, 10);
    assert(std::get<std::vector<std::int32_t>>(convertEnumValues(enums, Type::Int32, &source)) ==
           std::vector<std::int32_t>(count, 10));
    assert(std::get<std::vector<std::string>>(convertEnumValues(enums, Type::String, &source)) ==
           std::vector<std::string>(count, "ON"));
    assert(std::get<std::vector<EnumValue>>(convertEnumValues(enums, Type::Enum, &source, &target)) ==
           std::vector<EnumValue>(count, EnumValue{8}));
    assert(std::get<std::vector<EnumValue>>(convertEnumValues(integers, Type::Enum, nullptr, &source)) ==
           std::vector<EnumValue>(count, EnumValue{10}));
    assert(std::get<std::vector<EnumValue>>(convertEnumValues(labels, Type::Enum, nullptr, &target)) ==
           std::vector<EnumValue>(count, EnumValue{8}));
    assert(convertValues(enums, Type::Enum) == enums);
    for (const Type type : {Type::Int32, Type::String, Type::Double}) {
      rejects([&] { convertValues(enums, type); });
      rejects([&] { convertEnumValues(enums, type, nullptr); });
    }
    rejects([&] { convertEnumValues(enums, Type::Double, &source); });
    rejects([&] { convertEnumValues(enums, Type::Enum, &source); });
    rejects([&] { convertEnumValues(enums, Type::Enum, nullptr, &target); });
    rejects([&] { convertEnumValues(enums, Type::Enum, nullptr); });
    rejects([&] { convertEnumValues(numbers, Type::Enum, nullptr); });
    rejects([&] { convertEnumValues(numbers, Type::Enum, nullptr, &target); });
    rejects([&] { convertEnumValues(integers, Type::Enum, nullptr); });
    rejects([&] { convertEnumValues(labels, Type::Enum, nullptr); });
    rejects([&] { convertEnumValues(integers, Type::String, nullptr, &target); });
    rejects([&] { convertEnumValues(labels, Type::Int32, nullptr, &target); });
    rejects([&] { convertValues(integers, Type::Enum); });
  }
}

/** @brief Run enum round trips in both directions using the C regression fixtures. */
int main(int argc, char **argv) try {
  using namespace sdds;
  assert(argc == 3);
  const std::filesystem::path inputDirectory(argv[1]), outputDirectory(argv[2]);
  std::filesystem::create_directories(outputDirectory);
  testFixedCharacterParameters(outputDirectory);
  testFixedEnumParameters(outputDirectory);
  testPartialEnumAssignments(outputDirectory);
  testFixedEnumWhitespace(outputDirectory);
  testProjectedAsciiEnumArrays(outputDirectory);
  testProjectedBinaryEnumColumns();
  testEnumBulkConversions();
  const EnumDefinition a("0:OFF,10:ON,-1:FAULT"), b("-1:FAULT,0:OFF,10:ON"), c("7:OFF,8:ON,9:FAULT");
  assert(a == b && a != c && a.code("FAULT") == -1);
  assert(remapEnum(EnumValue{10}, a, c).code == 8);
  bool rejected = false;
  try { convertScalar(EnumValue{10}, Type::Int32); } catch (const TypeError &) { rejected = true; }
  assert(rejected);
  rejected = false;
  try { EnumDefinition invalid("0:A,1:A"); } catch (const FormatError &) { rejected = true; }
  assert(rejected);
  assert(std::get<std::string>(convertEnumScalar(EnumValue{-1}, Type::String, &a)) == "FAULT");
  assert(std::get<EnumValue>(convertEnumScalar(std::string("ON"), Type::Enum, nullptr, &c)).code == 8);
  assert(std::get<std::int32_t>(convertEnumScalar(EnumValue{-1}, Type::Int32, &a)) == -1);
  {
    std::ifstream manifest("../SDDSlib/testdata/enums/manifest.txt");
    assert(manifest);
    int valid;
    std::string filename;
    while (manifest >> valid >> filename) {
      for (const bool projection : {false, true}) {
        bool accepted = false;
        try {
          auto fixture = Reader::open(std::filesystem::path("../SDDSlib/testdata/enums") / filename);
          ReadRequest request;
          if (projection) {
            request.parameters = FieldSelection::noFields();
            request.arrays = FieldSelection::noFields();
            request.columns = FieldSelection::noFields();
            request.rows.first = 100;
          }
          accepted = fixture.next(request).has_value();
        } catch (const Error &) {}
        if (accepted != static_cast<bool>(valid)) std::cerr << "Unexpected enum fixture: " << filename << " projection=" << projection << '\n';
        assert(accepted == static_cast<bool>(valid));
      }
    }
  }
  for (const auto &entry : std::filesystem::directory_iterator(inputDirectory)) {
    if (!entry.is_regular_file()) continue;
    auto reader = Reader::open(entry.path());
    assert(reader.layout().version == 6);
    const auto output = outputDirectory / entry.path().filename();
    auto writer = Writer::create(output, reader.layout());
    int pages = 0;
    while (auto page = reader.next()) {
      assert(page->columnAs<EnumValue>("state").at(2).code == -1);
      assert(page->layout().columns[0].enumDefinition->label(10) == "ON");
      assert(std::get<EnumValue>(page->parameter("fixedState")).code == -1);
      assert(std::get<std::string>(page->parameter("fixedText")) == "a\\b");
      assert(std::get<std::string>(page->parameter("longFixedText")) == std::string(2048, 'x'));
      writer.write(*page);
      ++pages;
    }
    assert(pages == 2);
    writer.close();
    SDDS_DATASET dataset{};
    auto filename = output.string();
    assert(SDDS_InitializeInput(&dataset, filename.data()));
    assert(dataset.layout.version == 6);
    assert(SDDS_ReadPage(&dataset) > 0);
    assert(static_cast<std::int32_t *>(dataset.data[0])[3] == 20);
    assert(std::string(*static_cast<char **>(dataset.parameter[2])) == "a\\b");
    assert(std::string(*static_cast<char **>(dataset.parameter[3])) == std::string(2048, 'x'));
    assert(SDDS_Terminate(&dataset));
    auto projectedReader = Reader::open(output);
    ReadRequest request;
    request.rows.stride = 2;
    request.parameters = FieldSelection::noFields();
    request.arrays = FieldSelection::noFields();
    auto page = projectedReader.next(request);
    assert(page && page->columnAs<EnumValue>("state").size() == 2);
    assert(page->columnAs<EnumValue>("state")[1].code == -1);
    // Windows cannot replace the compressed output while this reader is open.
    projectedReader.close();
    {
      auto updater = Writer::appendToLastPage(output, 1);
      updater.setColumn("state", std::vector<EnumValue>{{0}}, 4);
      updater.updatePage(true);
      updater.close();
      auto updatedReader = Reader::open(output);
      assert(updatedReader.next());
      auto updated = updatedReader.next();
      assert(updated && updated->rowCount() == 5 && updated->columnAs<EnumValue>("state")[4].code == 0);
      auto filtered = updated->filtered(updated->matchRows<EnumValue>("state", [](EnumValue value) { return value.code == 0; }));
      assert(filtered.rowCount() == 2 && filtered.layout().columns[0].enumDefinition == updated->layout().columns[0].enumDefinition);
    }
    if (output.extension() == ".sdds") {
      auto appender = Writer::append(output);
      auto fullReader = Reader::open(entry.path());
      appender.write(*fullReader.next());
      appender.close();
    }
  }
  for (const auto &entry : std::filesystem::directory_iterator(inputDirectory / "edgecases")) {
    if (entry.path().filename().string().rfind("long-name.", 0) == 0) {
      auto reader = Reader::open(entry.path());
      assert(reader.layout().columns.at(0).name.size() == 20000);
      const auto page = reader.next();
      assert(page && page->rowCount() == 1);
      assert(std::get<std::vector<EnumValue>>(page->column(0)).at(0).code == 10);
      assert(std::get<EnumValue>(page->parameter(0)).code == 10);
      assert(std::get<std::vector<EnumValue>>(page->array(0).values).at(0).code == 10);
      assert(!reader.next());
      continue;
    }
    bool invalid = false;
    try {
      auto reader = Reader::open(entry.path());
      ReadRequest request;
      request.columns = FieldSelection::noFields();
      request.rows.last = 1;
      reader.next(request);
    } catch (const Error &) { invalid = true; }
    assert(invalid);
  }
  {
    auto reader = Reader::open(inputDirectory / "enum-1-0-0.sdds");
    const auto emptyPath = outputDirectory / "empty.sdds";
    auto writer = Writer::create(emptyPath, reader.layout());
    Page empty(std::make_shared<const Layout>(reader.layout()));
    empty.setColumn("state", std::vector<EnumValue>{});
    empty.setArray("stateArray", {{0}, std::vector<EnumValue>{}});
    writer.write(empty);
    writer.close();
    SDDS_DATASET dataset{};
    auto path = emptyPath.string();
    assert(SDDS_InitializeInput(&dataset, path.data()));
    assert(SDDS_ReadPage(&dataset) > 0 && dataset.n_rows == 0);
    assert(SDDS_Terminate(&dataset));
  }
  std::cout << "SDDS6 C/C++ differential tests passed\n";
} catch (const std::exception &error) {
  std::cerr << "SDDS6 C/C++ differential tests failed: " << error.what() << '\n';
  return 1;
}
