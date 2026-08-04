/**
 * @file sddspp_read_demo.cc
 * @brief Demonstrates generic SDDS++ layout and page inspection.
 *
 * This is the C++ counterpart of SDDSlib/demo/sdds_read_demo.c. It reads any
 * supported SDDS file and prints its parameters, arrays, and columns.
 *
 * @copyright
 *   - (c) 2026 The University of Chicago
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 */

#include "SDDS.hpp"

#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <type_traits>
#include <variant>

namespace {

/** @brief Prints one value from an SDDS variant. */
template <class T>
void printValue(const T &value) {
  if constexpr (std::is_floating_point_v<T>)
    std::cout << std::scientific << std::setprecision(15) << value;
  else
    std::cout << value;
}

/** @brief Prints an SDDS scalar without switching on a numeric type ID. */
void printScalar(const sdds::Scalar &value) {
  std::visit([](const auto &item) { printValue(item); }, value);
}

/** @brief Prints all elements in an SDDS values vector. */
void printValues(const sdds::Values &values) {
  std::visit([](const auto &items) {
    for (const auto &item : items) {
      std::cout << "    ";
      printValue(item);
      std::cout << '\n';
    }
  }, values);
}

/** @brief Prints a decoded page using its immutable layout definitions. */
void printPage(const sdds::Page &page) {
  std::cout << "Page " << page.number() << " (" << page.rowCount() << " rows)\n";

  for (const auto &definition : page.layout().parameters) {
    std::cout << "  parameter " << definition.name << " ("
              << sdds::typeName(definition.type) << ") = ";
    printScalar(page.parameter(definition.name));
    std::cout << '\n';
  }

  for (const auto &definition : page.layout().arrays) {
    const auto &array = page.array(definition.name);
    std::cout << "  array " << definition.name << " ("
              << sdds::typeName(definition.type) << ") dimensions=";
    for (std::size_t index = 0; index < array.dimensions.size(); ++index) {
      if (index)
        std::cout << 'x';
      std::cout << array.dimensions[index];
    }
    std::cout << '\n';
    printValues(array.values);
  }

  for (const auto &definition : page.layout().columns) {
    std::cout << "  column " << definition.name << " ("
              << sdds::typeName(definition.type) << ")\n";
    printValues(page.column(definition.name));
  }
}

}  // namespace

/**
 * @brief Reads and displays an SDDS file.
 * @param argc Argument count.
 * @param argv Argument vector containing the input filename.
 * @return Zero on success, nonzero on failure.
 */
int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " input.sdds\n";
    return 1;
  }

  const std::filesystem::path input = argv[1];
  try {
    auto reader = sdds::Reader::open(input);
    const auto &layout = reader.layout();
    std::cout << "SDDS version " << layout.version << ", "
              << (layout.data.mode == sdds::DataMode::Ascii ? "ASCII" : "binary")
              << " data\n";
    while (auto page = reader.next())
      printPage(*page);
    reader.close();
  } catch (const std::exception &error) {
    std::cerr << "Unable to read " << input << ": " << error.what() << '\n';
    return 1;
  }
  return 0;
}
