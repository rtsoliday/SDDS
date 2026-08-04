/**
 * @file sddsppdemo.cc
 * @brief Combined typed SDDS++ write and read demonstration.
 *
 * This is the C++ counterpart of SDDSlib/demo/sddsdemo.c. It demonstrates a
 * compact binary layout, writes two pages, and reads them back with typed
 * accessors and zero-copy row views.
 *
 * @copyright
 *   - (c) 2026 The University of Chicago
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 */

#include "SDDS.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

/** @brief Creates one parameter definition. */
sdds::ParameterDefinition parameter(std::string name, sdds::Type type,
                                    std::optional<std::string> units = std::nullopt) {
  sdds::ParameterDefinition definition;
  definition.name = std::move(name);
  definition.type = type;
  definition.units = std::move(units);
  return definition;
}

/** @brief Creates one column definition. */
sdds::ColumnDefinition column(std::string name, sdds::Type type,
                              std::optional<std::string> units = std::nullopt) {
  sdds::ColumnDefinition definition;
  definition.name = std::move(name);
  definition.type = type;
  definition.units = std::move(units);
  return definition;
}

/** @brief Builds the binary layout used in this demonstration. */
sdds::Layout makeLayout() {
  sdds::DataOptions data;
  data.mode = sdds::DataMode::Binary;

  sdds::LayoutBuilder builder;
  builder.setDescription("Combined SDDS++ demonstration", "typed C++ access")
      .setDataOptions(data)
      .addParameter(parameter("adouble", sdds::Type::Double, "meters"))
      .addParameter(parameter("afloat", sdds::Type::Float, "C"))
      .addParameter(parameter("along", sdds::Type::Int32, "seconds"))
      .addParameter(parameter("ashort", sdds::Type::Int16))
      .addParameter(parameter("astring", sdds::Type::String))
      .addColumn(column("number", sdds::Type::Int32))
      .addColumn(column("s", sdds::Type::Double, "s"))
      .addColumn(column("element", sdds::Type::String))
      .addColumn(column("x", sdds::Type::Double, "x"))
      .addColumn(column("xp", sdds::Type::Double, "x'"))
      .addColumn(column("y", sdds::Type::Double, "y"))
      .addColumn(column("yp", sdds::Type::Double, "y'"));
  return builder.build();
}

/** @brief Writes a single page with values matching the original C demo. */
void writePage(sdds::Writer &writer, std::int32_t rows, std::int32_t offset,
               double parameterBase, const std::string &text) {
  writer.beginPage(rows);
  writer.setParameter("adouble", parameterBase);
  writer.setParameter("afloat", static_cast<float>(parameterBase + 0.1));
  writer.setParameter("along", offset == 0 ? std::int32_t{1234567891} : std::int32_t{234567891});
  writer.setParameter("ashort", offset == 0 ? std::int16_t{12345} : std::int16_t{2345});
  writer.setParameter("astring", text);

  std::vector<std::int32_t> number;
  std::vector<double> s;
  std::vector<double> x;
  std::vector<double> xp;
  std::vector<double> y;
  std::vector<double> yp;
  std::vector<std::string> element;
  number.reserve(rows);
  s.reserve(rows);
  x.reserve(rows);
  xp.reserve(rows);
  y.reserve(rows);
  yp.reserve(rows);
  element.reserve(rows);
  for (std::int32_t row = 0; row < rows; ++row) {
    const auto value = static_cast<double>(row + offset);
    number.push_back(row + (offset == 0 ? 0 : 10));
    s.push_back(value);
    x.push_back(value + 0.1);
    xp.push_back(value + 0.2);
    y.push_back(value + 0.3);
    yp.push_back(value + 0.4);
    element.push_back("element " + std::to_string(row + offset));
  }
  writer.setColumn("number", std::move(number));
  writer.setColumn("s", std::move(s));
  writer.setColumn("element", std::move(element));
  writer.setColumn("x", std::move(x));
  writer.setColumn("xp", std::move(xp));
  writer.setColumn("y", std::move(y));
  writer.setColumn("yp", std::move(yp));
  writer.commitPage();
}

/** @brief Writes both pages and closes the output file. */
void writeDataset(const std::filesystem::path &path) {
  auto writer = sdds::Writer::create(path, makeLayout());
  writePage(writer, 5, 0, 250.0, "This is a string");
  writePage(writer, 6, 50, 451.0, "this is a string");
  writer.close();
  std::cout << "Wrote " << path << "\n";
}

/** @brief Reads both pages using typed accessors and row views. */
void readDataset(const std::filesystem::path &path) {
  auto reader = sdds::Reader::open(path);
  while (auto page = reader.next()) {
    std::cout << "Page " << page->number()
              << ": adouble=" << page->parameterAs<double>("adouble")
              << ", astring=\"" << page->parameterAs<std::string>("astring")
              << "\"\n";
    for (std::int64_t row = 0; row < page->rowCount(); ++row) {
      const auto view = page->row(row);
      std::cout << "  " << view.get<std::int32_t>("number") << ' '
                << view.get<std::string>("element")
                << " x=" << view.get<double>("x")
                << " xp=" << view.get<double>("xp")
                << " y=" << view.get<double>("y")
                << " yp=" << view.get<double>("yp") << '\n';
    }
  }
  reader.close();
}

}  // namespace

/**
 * @brief Runs the combined SDDS++ demonstration.
 * @param argc Argument count; an optional output path may be supplied.
 * @param argv Argument vector.
 * @return Zero on success, nonzero on failure.
 */
int main(int argc, char **argv) {
  if (argc > 2) {
    std::cerr << "usage: " << argv[0] << " [output.sdds]\n";
    return 1;
  }

  const std::filesystem::path path = argc == 2 ? argv[1] : "demo.sdds";
  try {
    writeDataset(path);
    readDataset(path);
  } catch (const std::exception &error) {
    std::cerr << "SDDS++ demo failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
