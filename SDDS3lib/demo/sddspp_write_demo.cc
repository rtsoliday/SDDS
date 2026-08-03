/**
 * @file sddspp_write_demo.cc
 * @brief Demonstrates writing every SDDS scalar type with SDDS++.
 *
 * This is the C++ counterpart of SDDSlib/demo/sdds_write_demo.c. It writes
 * parameters, arrays, and columns to two pages of an ASCII SDDS file.
 *
 * @copyright Copyright (c) 2026 The University of Chicago
 * @license Distributed under the Software License Agreement in LICENSE.
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

/** @brief Creates a parameter definition for a named SDDS type. */
sdds::ParameterDefinition parameter(std::string name, sdds::Type type) {
  sdds::ParameterDefinition definition;
  definition.name = std::move(name);
  definition.type = type;
  return definition;
}

/** @brief Creates an array definition for a named SDDS type. */
sdds::ArrayDefinition array(std::string name, sdds::Type type,
                            std::int32_t dimensions) {
  sdds::ArrayDefinition definition;
  definition.name = std::move(name);
  definition.type = type;
  definition.dimensions = dimensions;
  return definition;
}

/** @brief Creates a column definition for a named SDDS type. */
sdds::ColumnDefinition column(std::string name, sdds::Type type) {
  sdds::ColumnDefinition definition;
  definition.name = std::move(name);
  definition.type = type;
  return definition;
}

/** @brief Builds the layout shared by both output pages. */
sdds::Layout makeLayout() {
  sdds::DataOptions data;
  data.mode = sdds::DataMode::Ascii;
  data.linesPerRow = 1;

  sdds::LayoutBuilder builder;
  builder.setDescription("Example SDDS Output", "SDDS++ Example")
      .setDataOptions(data);

  const std::vector<std::pair<const char *, sdds::Type>> fields = {
      {"short", sdds::Type::Int16},
      {"ushort", sdds::Type::UInt16},
      {"long", sdds::Type::Int32},
      {"ulong", sdds::Type::UInt32},
      {"long64", sdds::Type::Int64},
      {"ulong64", sdds::Type::UInt64},
      {"float", sdds::Type::Float},
      {"double", sdds::Type::Double},
      {"longdouble", sdds::Type::LongDouble},
      {"string", sdds::Type::String},
      {"char", sdds::Type::Character},
  };

  for (const auto &[name, type] : fields) {
    builder.addParameter(parameter(std::string(name) + "Param", type));
    const bool oneDimensional = type == sdds::Type::Int16 ||
                                type == sdds::Type::UInt16 ||
                                type == sdds::Type::Int32 ||
                                type == sdds::Type::UInt32;
    builder.addArray(array(std::string(name) + "Array", type,
                           oneDimensional ? 1 : 2));
    builder.addColumn(column(std::string(name) + "Col", type));
  }
  return builder.build();
}

/** @brief Writes the first page from the original C demonstration. */
void writeFirstPage(sdds::Writer &writer) {
  writer.beginPage(5);
  writer.setParameter("shortParam", std::int16_t{10});
  writer.setParameter("ushortParam", std::uint16_t{11});
  writer.setParameter("longParam", std::int32_t{1000});
  writer.setParameter("ulongParam", std::uint32_t{1001});
  writer.setParameter("long64Param", std::int64_t{1002});
  writer.setParameter("ulong64Param", std::uint64_t{1003});
  writer.setParameter("floatParam", 3.14F);
  writer.setParameter("doubleParam", 2.71828);
  writer.setParameter("longdoubleParam", 1.1L);
  writer.setParameter("stringParam", std::string("FirstPage"));
  writer.setParameter("charParam", 'A');

  writer.setArray("shortArray", {{3}, std::vector<std::int16_t>{1, 2, 3}});
  writer.setArray("ushortArray", {{3}, std::vector<std::uint16_t>{4, 5, 6}});
  writer.setArray("longArray", {{3}, std::vector<std::int32_t>{1000, 2000, 3000}});
  writer.setArray("ulongArray", {{3}, std::vector<std::uint32_t>{1001, 2001, 3001}});
  writer.setArray("long64Array", {{4, 2}, std::vector<std::int64_t>{
      1002, 2002, 3002, 4002, 5002, 6002, 7002, 8002}});
  writer.setArray("ulong64Array", {{4, 2}, std::vector<std::uint64_t>{
      1003, 2003, 3003, 4003, 5003, 6003, 7003, 8003}});
  writer.setArray("floatArray", {{4, 2}, std::vector<float>{
      1.1F, 1.2F, 1.3F, 1.4F, 1.5F, 1.6F, 1.7F, 1.8F}});
  writer.setArray("doubleArray", {{4, 2}, std::vector<double>{
      1.2, 2.2, 3.2, 4.2, 5.2, 6.2, 7.2, 8.2}});
  writer.setArray("longdoubleArray", {{4, 2}, std::vector<long double>{
      1.3L, 2.3L, 3.3L, 4.3L, 5.3L, 6.3L, 7.3L, 8.3L}});
  writer.setArray("stringArray", {{4, 2}, std::vector<std::string>{
      "one", "two", "three", "four", "five", "six", "seven", "eight"}});
  writer.setArray("charArray", {{4, 2}, std::vector<char>{
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'}});

  writer.setColumn("shortCol", std::vector<std::int16_t>{1, 2, 3, 4, 5});
  writer.setColumn("ushortCol", std::vector<std::uint16_t>{1, 2, 3, 4, 5});
  writer.setColumn("longCol", std::vector<std::int32_t>{100, 200, 300, 400, 500});
  writer.setColumn("ulongCol", std::vector<std::uint32_t>{100, 200, 300, 400, 500});
  writer.setColumn("long64Col", std::vector<std::int64_t>{100, 200, 300, 400, 500});
  writer.setColumn("ulong64Col", std::vector<std::uint64_t>{100, 200, 300, 400, 500});
  writer.setColumn("floatCol", std::vector<float>{1.1F, 2.2F, 3.3F, 4.4F, 5.5F});
  writer.setColumn("doubleCol", std::vector<double>{10.01, 20.02, 30.03, 40.04, 50.05});
  writer.setColumn("longdoubleCol", std::vector<long double>{
      10.01L, 20.02L, 30.03L, 40.04L, 50.05L});
  writer.setColumn("stringCol", std::vector<std::string>{
      "one", "two", "three", "four", "five"});
  writer.setColumn("charCol", std::vector<char>{'a', 'b', 'c', 'd', 'e'});
  writer.commitPage();
}

/** @brief Writes the second page from the original C demonstration. */
void writeSecondPage(sdds::Writer &writer) {
  writer.beginPage(3);
  writer.setParameter("shortParam", std::int16_t{20});
  writer.setParameter("ushortParam", std::uint16_t{21});
  writer.setParameter("longParam", std::int32_t{2000});
  writer.setParameter("ulongParam", std::uint32_t{2001});
  writer.setParameter("long64Param", std::int64_t{2002});
  writer.setParameter("ulong64Param", std::uint64_t{2003});
  writer.setParameter("floatParam", 6.28F);
  writer.setParameter("doubleParam", 1.41421);
  writer.setParameter("longdoubleParam", 2.2L);
  writer.setParameter("stringParam", std::string("SecondPage"));
  writer.setParameter("charParam", 'B');

  writer.setArray("shortArray", {{2}, std::vector<std::int16_t>{7, 8}});
  writer.setArray("ushortArray", {{2}, std::vector<std::uint16_t>{9, 10}});
  writer.setArray("longArray", {{2}, std::vector<std::int32_t>{4000, 5000}});
  writer.setArray("ulongArray", {{2}, std::vector<std::uint32_t>{4001, 5001}});
  writer.setArray("long64Array", {{2, 2}, std::vector<std::int64_t>{4002, 5002, 6002, 7002}});
  writer.setArray("ulong64Array", {{2, 2}, std::vector<std::uint64_t>{4003, 5003, 6003, 7003}});
  writer.setArray("floatArray", {{2, 2}, std::vector<float>{11.11F, 22.22F, 33.33F, 44.44F}});
  writer.setArray("doubleArray", {{2, 2}, std::vector<double>{33.33, 44.44, 55.55, 66.66}});
  writer.setArray("longdoubleArray", {{2, 2}, std::vector<long double>{
      55.55L, 66.66L, 77.77L, 88.88L}});
  writer.setArray("stringArray", {{2, 2}, std::vector<std::string>{
      "blue", "red", "yellow", "gold"}});
  writer.setArray("charArray", {{2, 2}, std::vector<char>{'W', 'X', 'Y', 'Z'}});

  writer.setColumn("shortCol", std::vector<std::int16_t>{6, 7, 8});
  writer.setColumn("ushortCol", std::vector<std::uint16_t>{6, 7, 8});
  writer.setColumn("longCol", std::vector<std::int32_t>{600, 700, 800});
  writer.setColumn("ulongCol", std::vector<std::uint32_t>{600, 700, 800});
  writer.setColumn("long64Col", std::vector<std::int64_t>{600, 700, 800});
  writer.setColumn("ulong64Col", std::vector<std::uint64_t>{600, 700, 800});
  writer.setColumn("floatCol", std::vector<float>{6.6F, 7.7F, 8.8F});
  writer.setColumn("doubleCol", std::vector<double>{60.06, 70.07, 80.08});
  writer.setColumn("longdoubleCol", std::vector<long double>{60.06L, 70.07L, 80.08L});
  writer.setColumn("stringCol", std::vector<std::string>{"six", "seven", "eight"});
  writer.setColumn("charCol", std::vector<char>{'f', 'g', 'h'});
  writer.commitPage();
}

}  // namespace

/**
 * @brief Writes an SDDS++ example file.
 * @param argc Argument count; an optional output path may be supplied.
 * @param argv Argument vector.
 * @return Zero on success, nonzero on failure.
 */
int main(int argc, char **argv) {
  if (argc > 2) {
    std::cerr << "usage: " << argv[0] << " [output.sdds]\n";
    return 1;
  }

  const std::filesystem::path output = argc == 2 ? argv[1] : "example.sdds";
  try {
    auto writer = sdds::Writer::create(output, makeLayout());
    writeFirstPage(writer);
    writeSecondPage(writer);
    writer.close();
    std::cout << "Wrote " << output << "\n";
  } catch (const std::exception &error) {
    std::cerr << "Unable to write " << output << ": " << error.what() << '\n';
    return 1;
  }
  return 0;
}
