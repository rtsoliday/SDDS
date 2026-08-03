/**
 * @file SDDSLegacy.cc
 * @brief Deprecated SDDSFile method facade implemented on the C++17 SDDS API.
 *
 * @copyright Copyright (c) 2026 The University of Chicago
 * @license Distributed under the Software License Agreement in LICENSE.
 */

#include "SDDS3Legacy.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

sdds::Type checkedType(std::int32_t type) {
  if (type < static_cast<std::int32_t>(sdds::Type::LongDouble) ||
      type > static_cast<std::int32_t>(sdds::Type::Character))
    throw sdds::TypeError(sdds::ErrorKind::Type, "invalid SDDS type number");
  return static_cast<sdds::Type>(type);
}

sdds::Type checkedType(const char *name) {
  if (!name)
    throw sdds::TypeError(sdds::ErrorKind::Type, "null SDDS type name");
  for (std::int32_t value = 1; value <= 11; ++value) {
    const auto type = static_cast<sdds::Type>(value);
    if (sdds::typeName(type) == name)
      return type;
  }
  throw sdds::TypeError(sdds::ErrorKind::Type, "invalid SDDS type name");
}

std::optional<std::string> optionalText(const char *text) {
  return text ? std::optional<std::string>(text) : std::nullopt;
}

template <class T>
sdds::Scalar convertedScalar(sdds::Type type, const T &value) {
  if constexpr (std::is_same_v<T, std::string>) {
    if (type != sdds::Type::String)
      throw sdds::TypeError(sdds::ErrorKind::Type, "string value used for a non-string field");
    return value;
  } else {
    switch (type) {
    case sdds::Type::LongDouble: return static_cast<long double>(value);
    case sdds::Type::Double: return static_cast<double>(value);
    case sdds::Type::Float: return static_cast<float>(value);
    case sdds::Type::Int64: return static_cast<std::int64_t>(value);
    case sdds::Type::UInt64: return static_cast<std::uint64_t>(value);
    case sdds::Type::Int32: return static_cast<std::int32_t>(value);
    case sdds::Type::UInt32: return static_cast<std::uint32_t>(value);
    case sdds::Type::Int16: return static_cast<std::int16_t>(value);
    case sdds::Type::UInt16: return static_cast<std::uint16_t>(value);
    case sdds::Type::Character: return static_cast<char>(value);
    case sdds::Type::String:
      throw sdds::TypeError(sdds::ErrorKind::Type, "numeric value used for a string field");
    }
  }
  throw sdds::TypeError(sdds::ErrorKind::Type, "invalid SDDS type");
}

template <class T>
sdds::Values convertedValues(sdds::Type type, const T *values, std::size_t count) {
  if (!values && count)
    throw sdds::TypeError(sdds::ErrorKind::Type, "null column data");
  sdds::Values result;
  switch (type) {
  case sdds::Type::LongDouble: result = std::vector<long double>{}; break;
  case sdds::Type::Double: result = std::vector<double>{}; break;
  case sdds::Type::Float: result = std::vector<float>{}; break;
  case sdds::Type::Int64: result = std::vector<std::int64_t>{}; break;
  case sdds::Type::UInt64: result = std::vector<std::uint64_t>{}; break;
  case sdds::Type::Int32: result = std::vector<std::int32_t>{}; break;
  case sdds::Type::UInt32: result = std::vector<std::uint32_t>{}; break;
  case sdds::Type::Int16: result = std::vector<std::int16_t>{}; break;
  case sdds::Type::UInt16: result = std::vector<std::uint16_t>{}; break;
  case sdds::Type::Character: result = std::vector<char>{}; break;
  case sdds::Type::String:
    throw sdds::TypeError(sdds::ErrorKind::Type, "numeric data used for a string column");
  }
  std::visit([&](auto &destination) {
    using Vector = std::decay_t<decltype(destination)>;
    using Value = typename Vector::value_type;
    if constexpr (std::is_same_v<Value, std::string>) {
      throw sdds::TypeError(sdds::ErrorKind::Type,
                            "numeric data used for a string column");
    } else {
      destination.reserve(count);
      for (std::size_t i = 0; i < count; ++i)
        destination.push_back(static_cast<Value>(values[i]));
    }
  }, result);
  return result;
}

sdds::Values convertedStrings(char **values, std::size_t count) {
  std::vector<std::string> result;
  result.reserve(count);
  for (std::size_t i = 0; i < count; ++i)
    result.emplace_back(values && values[i] ? values[i] : "");
  return result;
}

template <class T>
T scalarNumber(const sdds::Scalar &value) {
  return std::visit([](const auto &item) -> T {
    using Value = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<Value, std::string>)
      return static_cast<T>(std::strtold(item.c_str(), nullptr));
    else
      return static_cast<T>(item);
  }, value);
}

template <class T>
std::vector<T> vectorNumbers(const sdds::Values &values) {
  return std::visit([](const auto &source) {
    using Vector = std::decay_t<decltype(source)>;
    using Value = typename Vector::value_type;
    std::vector<T> result;
    result.reserve(source.size());
    for (const auto &item : source) {
      if constexpr (std::is_same_v<Value, std::string>)
        result.push_back(static_cast<T>(std::strtold(item.c_str(), nullptr)));
      else
        result.push_back(static_cast<T>(item));
    }
    return result;
  }, values);
}

}  // namespace

struct SDDSFile::Impl {
  std::string filename;
  std::shared_ptr<sdds::Layout> layout = std::make_shared<sdds::Layout>();
  std::vector<sdds::Page> pages;
  std::vector<std::string> errors;
  std::size_t readCursor = 0;
  sdds::RecoveryMode recovery = sdds::RecoveryMode::Automatic;
  std::vector<std::int32_t> int32Scratch;
  std::vector<std::uint32_t> uint32Scratch;
  std::vector<double> doubleScratch;
  std::string stringScratch;
  std::vector<std::string> stringsScratch;
  std::vector<char *> stringPointers;

  int fail(const std::exception &error) {
    errors.emplace_back(error.what());
    return 0;
  }

  sdds::Page &page(std::uint32_t number) {
    if (!number)
      throw sdds::StateError(sdds::ErrorKind::State, "legacy page numbers start at one");
    while (pages.size() < number)
      pages.emplace_back(layout);
    return pages[number - 1];
  }

  const sdds::Page &page(std::uint32_t number) const {
    if (!number || number > pages.size())
      throw sdds::StateError(sdds::ErrorKind::State, "invalid legacy page number");
    return pages[number - 1];
  }

  void requireMutableLayout() const {
    if (!pages.empty())
      throw sdds::StateError(sdds::ErrorKind::State,
                             "definitions cannot be changed after page data is allocated");
  }

  template <class T>
  int setParameter(std::uint32_t pageNumber, std::size_t index, const T &value) {
    try {
      page(pageNumber).setParameter(index,
          convertedScalar(layout->parameters.at(index).type, value));
      return 1;
    } catch (const std::exception &error) {
      return fail(error);
    }
  }

  template <class T>
  int setColumn(std::size_t index, std::uint32_t pageNumber, std::uint32_t startRow,
                const T *values, std::uint32_t rows) {
    try {
      auto &targetPage = page(pageNumber);
      sdds::Values incoming = convertedValues(layout->columns.at(index).type, values, rows);
      if (!startRow) {
        targetPage.setColumn(index, std::move(incoming));
      } else {
        sdds::Values merged = targetPage.column(index);
        std::visit([&](auto &destination) {
          using Vector = std::decay_t<decltype(destination)>;
          const auto &source = std::get<Vector>(incoming);
          if (destination.size() < startRow)
            destination.resize(startRow);
          if (destination.size() < static_cast<std::size_t>(startRow) + source.size())
            destination.resize(static_cast<std::size_t>(startRow) + source.size());
          std::copy(source.begin(), source.end(), destination.begin() + startRow);
        }, merged);
        targetPage.setColumn(index, std::move(merged));
      }
      return 1;
    } catch (const std::exception &error) {
      return fail(error);
    }
  }
};

SDDSFile::SDDSFile() : impl_(std::make_unique<Impl>()) {}
SDDSFile::SDDSFile(char *filename) : SDDSFile(static_cast<const char *>(filename)) {}
SDDSFile::SDDSFile(const char *filename) : SDDSFile() { setFileName(filename); }
SDDSFile::SDDSFile(bool binary) : SDDSFile() { if (binary) setBinaryMode(); }
SDDSFile::SDDSFile(char *filename, bool binary)
    : SDDSFile(static_cast<const char *>(filename), binary) {}
SDDSFile::SDDSFile(const char *filename, bool binary) : SDDSFile(binary) { setFileName(filename); }
SDDSFile::~SDDSFile() = default;

int32_t SDDSFile::initializeInput(char *filename) { return initializeInput(static_cast<const char *>(filename)); }
int32_t SDDSFile::initializeInput(const char *filename) {
  try {
    setFileName(filename);
    sdds::ReaderOptions options;
    options.recovery = impl_->recovery;
    auto reader = sdds::Reader::open(impl_->filename, options);
    auto dataset = reader.readAll();
    reader.close();
    impl_->layout = std::make_shared<sdds::Layout>(std::move(dataset.layout));
    impl_->pages = std::move(dataset.pages);
    impl_->readCursor = 0;
    return 1;
  } catch (const std::exception &error) {
    return impl_->fail(error);
  }
}

int32_t SDDSFile::initializeOutput(int32_t dataMode, int32_t linesPerRow,
                                   char *description, char *contents, char *filename) {
  try {
    impl_->layout = std::make_shared<sdds::Layout>();
    impl_->pages.clear();
    setFileName(filename);
    setDataMode(static_cast<std::uint32_t>(dataMode));
    impl_->layout->data.linesPerRow = linesPerRow;
    setDescription(description, contents);
    return 1;
  } catch (const std::exception &error) {
    return impl_->fail(error);
  }
}

int32_t SDDSFile::openInputFile() { return initializeInput(impl_->filename.c_str()); }
int32_t SDDSFile::openOutputFile() { return impl_->filename.empty() ? 0 : 1; }
int32_t SDDSFile::closeFile() { return 1; }
int32_t SDDSFile::readLayout() { return impl_->layout ? 1 : 0; }
int32_t SDDSFile::readFile() { return impl_->filename.empty() ? 0 : initializeInput(impl_->filename.c_str()); }
int32_t SDDSFile::readPage() {
  if (impl_->readCursor >= impl_->pages.size())
    return 0;
  return static_cast<int32_t>(++impl_->readCursor);
}
int32_t SDDSFile::readPages() { impl_->readCursor = impl_->pages.size(); return 1; }
int32_t SDDSFile::writeLayout() { return openOutputFile(); }
int32_t SDDSFile::writeFile() { return writePages(); }
int32_t SDDSFile::writePage(uint32_t page) { return writePages(page, page); }
int32_t SDDSFile::writePages() {
  return writePages(1, static_cast<std::uint32_t>(impl_->pages.size()));
}
int32_t SDDSFile::writePages(uint32_t startPage, uint32_t endPage) {
  try {
    if (impl_->filename.empty())
      throw sdds::StateError(sdds::ErrorKind::State, "no output filename was specified");
    if ((!impl_->pages.empty() && (!startPage || startPage > endPage || endPage > impl_->pages.size())) ||
        (impl_->pages.empty() && (startPage != 1 || endPage != 0)))
      throw sdds::StateError(sdds::ErrorKind::State, "invalid legacy output page range");
    auto writer = sdds::Writer::create(impl_->filename, *impl_->layout);
    for (std::uint32_t page = startPage; page <= endPage && page; ++page)
      writer.write(impl_->pages[page - 1]);
    writer.close();
    return 1;
  } catch (const std::exception &error) {
    return impl_->fail(error);
  }
}

int32_t SDDSFile::checkForErrors() { return static_cast<int32_t>(impl_->errors.size()); }
void SDDSFile::clearErrors() { impl_->errors.clear(); }
void SDDSFile::printErrors(FILE *file, int32_t) {
  FILE *output = file ? file : stderr;
  for (const auto &error : impl_->errors)
    std::fprintf(output, "%s\n", error.c_str());
}
void SDDSFile::printErrors() { printErrors(stderr, 0); }
void SDDSFile::setError(char *text) { impl_->errors.emplace_back(text ? text : ""); }
int32_t SDDSFile::readRecoveryPossible() const {
  return std::any_of(impl_->pages.begin(), impl_->pages.end(),
                     [](const auto &page) { return page.recovered(); }) ? 1 : 0;
}

void SDDSFile::setFileName(char *filename) { setFileName(static_cast<const char *>(filename)); }
void SDDSFile::setFileName(const char *filename) { impl_->filename = filename ? filename : ""; }
uint32_t SDDSFile::getDataMode() const {
  return impl_->layout->data.mode == sdds::DataMode::Binary ? SDDS_BINARY : SDDS_ASCII;
}
void SDDSFile::setDataMode(uint32_t mode) {
  impl_->layout->data.mode = mode == SDDS_BINARY ? sdds::DataMode::Binary : sdds::DataMode::Ascii;
}
void SDDSFile::setAsciiMode() { impl_->layout->data.mode = sdds::DataMode::Ascii; }
void SDDSFile::setBinaryMode() { impl_->layout->data.mode = sdds::DataMode::Binary; }
void SDDSFile::setColumnMajorOrder() { impl_->layout->data.majorOrder = sdds::MajorOrder::Column; }
void SDDSFile::setRowMajorOrder() { impl_->layout->data.majorOrder = sdds::MajorOrder::Row; }
void SDDSFile::setNativeEndian() { impl_->layout->data.byteOrder = sdds::ByteOrder::Native; }
void SDDSFile::setNonNativeEndian() {
  const std::uint16_t marker = 0x0102;
  const bool big = *reinterpret_cast<const unsigned char *>(&marker) == 0x01;
  impl_->layout->data.byteOrder = big ? sdds::ByteOrder::Little : sdds::ByteOrder::Big;
}
void SDDSFile::setNoRowCount() { impl_->layout->data.rowCountMode = sdds::RowCountMode::None; }
void SDDSFile::setUseRowCount() { impl_->layout->data.rowCountMode = sdds::RowCountMode::Variable; }
void SDDSFile::setReadRecoveryMode(int32_t mode) {
  impl_->recovery = mode ? sdds::RecoveryMode::Recover : sdds::RecoveryMode::Strict;
}
void SDDSFile::setLayoutVersion(int32_t version) { impl_->layout->version = version; }
void SDDSFile::setDescription(char *text, char *contents) {
  impl_->layout->description = optionalText(text);
  impl_->layout->contents = optionalText(contents);
}
int32_t SDDSFile::getDescription(char **text, char **contents) {
  if (text) *text = impl_->layout->description ? impl_->layout->description->data() : nullptr;
  if (contents) *contents = impl_->layout->contents ? impl_->layout->contents->data() : nullptr;
  return 1;
}
int32_t SDDSFile::readVersion() const { return impl_->layout->version; }
uint32_t SDDSFile::pageCount() const { return static_cast<std::uint32_t>(impl_->pages.size()); }
uint32_t SDDSFile::rowCount(uint32_t page) const {
  try { return static_cast<std::uint32_t>(impl_->page(page).rowCount()); }
  catch (...) { return 0; }
}
void SDDSFile::freePage() { impl_->pages.clear(); impl_->readCursor = 0; }

int32_t SDDSFile::defineParameter(char *name, int32_t type) {
  return defineParameter(name, nullptr, nullptr, nullptr, nullptr, type, nullptr);
}
int32_t SDDSFile::defineParameter(char *name, char *type) {
  try { return defineParameter(name, static_cast<int32_t>(checkedType(type))); }
  catch (const std::exception &error) { return impl_->fail(error); }
}
int32_t SDDSFile::defineParameter(char *name, char *symbol, char *units, char *description,
                                  char *formatString, int32_t type, char *fixedValue) {
  try {
    impl_->requireMutableLayout();
    sdds::ParameterDefinition definition;
    definition.name = name ? name : "";
    definition.symbol = optionalText(symbol);
    definition.units = optionalText(units);
    definition.description = optionalText(description);
    definition.format = optionalText(formatString);
    definition.type = checkedType(type);
    definition.fixedValue = optionalText(fixedValue);
    impl_->layout->parameters.push_back(std::move(definition));
    return static_cast<int32_t>(impl_->layout->parameters.size() - 1);
  } catch (const std::exception &error) { return impl_->fail(error); }
}

int32_t SDDSFile::defineColumn(char *name, int32_t type) {
  return defineColumn(name, nullptr, nullptr, nullptr, nullptr, type, 0);
}
int32_t SDDSFile::defineColumn(char *name, char *type) {
  try { return defineColumn(name, static_cast<int32_t>(checkedType(type))); }
  catch (const std::exception &error) { return impl_->fail(error); }
}
int32_t SDDSFile::defineColumn(char *name, char *symbol, char *units, char *description,
                               char *formatString, int32_t type, uint32_t fieldLength) {
  try {
    impl_->requireMutableLayout();
    sdds::ColumnDefinition definition;
    definition.name = name ? name : "";
    definition.symbol = optionalText(symbol);
    definition.units = optionalText(units);
    definition.description = optionalText(description);
    definition.format = optionalText(formatString);
    definition.type = checkedType(type);
    definition.fieldLength = static_cast<std::int32_t>(fieldLength);
    impl_->layout->columns.push_back(std::move(definition));
    return static_cast<int32_t>(impl_->layout->columns.size() - 1);
  } catch (const std::exception &error) { return impl_->fail(error); }
}

int32_t SDDSFile::defineArray(char *name, int32_t type, uint32_t dimensions) {
  return defineArray(name, nullptr, nullptr, nullptr, nullptr, nullptr, type, 0, dimensions);
}
int32_t SDDSFile::defineArray(char *name, char *type, uint32_t dimensions) {
  try { return defineArray(name, static_cast<int32_t>(checkedType(type)), dimensions); }
  catch (const std::exception &error) { return impl_->fail(error); }
}
int32_t SDDSFile::defineArray(char *name, char *symbol, char *units, char *description,
                              char *formatString, char *groupName, int32_t type,
                              uint32_t fieldLength, uint32_t dimensions) {
  try {
    impl_->requireMutableLayout();
    sdds::ArrayDefinition definition;
    definition.name = name ? name : "";
    definition.symbol = optionalText(symbol);
    definition.units = optionalText(units);
    definition.description = optionalText(description);
    definition.format = optionalText(formatString);
    definition.groupName = optionalText(groupName);
    definition.type = checkedType(type);
    definition.fieldLength = static_cast<std::int32_t>(fieldLength);
    definition.dimensions = static_cast<std::int32_t>(dimensions);
    impl_->layout->arrays.push_back(std::move(definition));
    return static_cast<int32_t>(impl_->layout->arrays.size() - 1);
  } catch (const std::exception &error) { return impl_->fail(error); }
}

int32_t SDDSFile::getParameterCount() const { return static_cast<int32_t>(impl_->layout->parameters.size()); }
int32_t SDDSFile::getColumnCount() const { return static_cast<int32_t>(impl_->layout->columns.size()); }
int32_t SDDSFile::getArrayCount() const { return static_cast<int32_t>(impl_->layout->arrays.size()); }
int32_t SDDSFile::getParameterIndex(char *name) const {
  try { return static_cast<int32_t>(impl_->layout->parameterIndex(name ? name : "")); } catch (...) { return -1; }
}
int32_t SDDSFile::getColumnIndex(char *name) const {
  try { return static_cast<int32_t>(impl_->layout->columnIndex(name ? name : "")); } catch (...) { return -1; }
}
int32_t SDDSFile::getArrayIndex(char *name) const {
  try { return static_cast<int32_t>(impl_->layout->arrayIndex(name ? name : "")); } catch (...) { return -1; }
}
char *SDDSFile::getParameterName(int32_t index) { return impl_->layout->parameters.at(index).name.data(); }
char *SDDSFile::getColumnName(int32_t index) { return impl_->layout->columns.at(index).name.data(); }
char *SDDSFile::getArrayName(int32_t index) { return impl_->layout->arrays.at(index).name.data(); }
int32_t SDDSFile::getParameterType(int32_t index) const { return static_cast<int32_t>(impl_->layout->parameters.at(index).type); }
int32_t SDDSFile::getColumnType(int32_t index) const { return static_cast<int32_t>(impl_->layout->columns.at(index).type); }
int32_t SDDSFile::getArrayType(int32_t index) const { return static_cast<int32_t>(impl_->layout->arrays.at(index).type); }

int32_t SDDSFile::setParameter(uint32_t page, int32_t index, int32_t value) { return impl_->setParameter(page, index, value); }
int32_t SDDSFile::setParameter(uint32_t page, int32_t index, uint32_t value) { return impl_->setParameter(page, index, value); }
int32_t SDDSFile::setParameter(uint32_t page, int32_t index, double value) { return impl_->setParameter(page, index, value); }
int32_t SDDSFile::setParameter(uint32_t page, int32_t index, char *value) { return impl_->setParameter(page, index, std::string(value ? value : "")); }
int32_t SDDSFile::setParameter(uint32_t page, char *name, int32_t value) { return setParameter(page, getParameterIndex(name), value); }
int32_t SDDSFile::setParameter(uint32_t page, char *name, uint32_t value) { return setParameter(page, getParameterIndex(name), value); }
int32_t SDDSFile::setParameter(uint32_t page, char *name, double value) { return setParameter(page, getParameterIndex(name), value); }
int32_t SDDSFile::setParameter(uint32_t page, char *name, char *value) { return setParameter(page, getParameterIndex(name), value); }

#define SDDSPP_SET_COLUMN(TYPE) \
  int32_t SDDSFile::setColumn(int32_t index, uint32_t page, uint32_t startRow, \
                              TYPE *values, uint32_t rows) { \
    return impl_->setColumn(index, page, startRow, values, rows); \
  }
SDDSPP_SET_COLUMN(int16_t)
SDDSPP_SET_COLUMN(uint16_t)
SDDSPP_SET_COLUMN(int32_t)
SDDSPP_SET_COLUMN(uint32_t)
SDDSPP_SET_COLUMN(float)
SDDSPP_SET_COLUMN(double)
SDDSPP_SET_COLUMN(char)
#undef SDDSPP_SET_COLUMN

int32_t SDDSFile::setColumn(int32_t index, uint32_t page, uint32_t startRow,
                            char **values, uint32_t rows) {
  try {
    if (impl_->layout->columns.at(index).type != sdds::Type::String)
      throw sdds::TypeError(sdds::ErrorKind::Type, "string data used for a non-string column");
    auto &target = impl_->page(page);
    sdds::Values incoming = convertedStrings(values, rows);
    if (!startRow) target.setColumn(index, std::move(incoming));
    else {
      auto merged = target.column(index);
      auto &destination = std::get<std::vector<std::string>>(merged);
      const auto &source = std::get<std::vector<std::string>>(incoming);
      if (destination.size() < startRow) destination.resize(startRow);
      if (destination.size() < static_cast<std::size_t>(startRow) + source.size())
        destination.resize(static_cast<std::size_t>(startRow) + source.size());
      std::copy(source.begin(), source.end(), destination.begin() + startRow);
      target.setColumn(index, std::move(merged));
    }
    return 1;
  } catch (const std::exception &error) { return impl_->fail(error); }
}
int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, int32_t *values, uint32_t rows) { return setColumn(getColumnIndex(name), page, startRow, values, rows); }
int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, uint32_t *values, uint32_t rows) { return setColumn(getColumnIndex(name), page, startRow, values, rows); }
int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, double *values, uint32_t rows) { return setColumn(getColumnIndex(name), page, startRow, values, rows); }
int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, char **values, uint32_t rows) { return setColumn(getColumnIndex(name), page, startRow, values, rows); }

int32_t SDDSFile::getParameterInInt32(int32_t index, uint32_t page) { return scalarNumber<int32_t>(impl_->page(page).parameter(index)); }
int32_t SDDSFile::getParameterInInt32(char *name, uint32_t page) { return getParameterInInt32(getParameterIndex(name), page); }
uint32_t SDDSFile::getParameterInUInt32(int32_t index, uint32_t page) { return scalarNumber<uint32_t>(impl_->page(page).parameter(index)); }
uint32_t SDDSFile::getParameterInUInt32(char *name, uint32_t page) { return getParameterInUInt32(getParameterIndex(name), page); }
double SDDSFile::getParameterInDouble(int32_t index, uint32_t page) { return scalarNumber<double>(impl_->page(page).parameter(index)); }
double SDDSFile::getParameterInDouble(char *name, uint32_t page) { return getParameterInDouble(getParameterIndex(name), page); }
char *SDDSFile::getParameterInString(int32_t index, uint32_t page) {
  const auto &value = impl_->page(page).parameter(index);
  if (const auto *text = std::get_if<std::string>(&value)) return const_cast<char *>(text->c_str());
  impl_->stringScratch = std::visit([](const auto &item) {
    using Value = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<Value, std::string>) return item;
    else if constexpr (std::is_same_v<Value, char>) return std::string(1, item);
    else return std::to_string(item);
  }, value);
  return impl_->stringScratch.data();
}
char *SDDSFile::getParameterInString(char *name, uint32_t page) { return getParameterInString(getParameterIndex(name), page); }
int32_t *SDDSFile::getColumnInInt32(int32_t index, uint32_t page) { impl_->int32Scratch = vectorNumbers<int32_t>(impl_->page(page).column(index)); return impl_->int32Scratch.data(); }
int32_t *SDDSFile::getColumnInInt32(char *name, uint32_t page) { return getColumnInInt32(getColumnIndex(name), page); }
uint32_t *SDDSFile::getColumnInUInt32(int32_t index, uint32_t page) { impl_->uint32Scratch = vectorNumbers<uint32_t>(impl_->page(page).column(index)); return impl_->uint32Scratch.data(); }
uint32_t *SDDSFile::getColumnInUInt32(char *name, uint32_t page) { return getColumnInUInt32(getColumnIndex(name), page); }
double *SDDSFile::getColumnInDouble(int32_t index, uint32_t page) { impl_->doubleScratch = vectorNumbers<double>(impl_->page(page).column(index)); return impl_->doubleScratch.data(); }
double *SDDSFile::getColumnInDouble(char *name, uint32_t page) { return getColumnInDouble(getColumnIndex(name), page); }
char **SDDSFile::getColumnInString(int32_t index, uint32_t page) {
  impl_->stringsScratch = std::get<std::vector<std::string>>(impl_->page(page).column(index));
  impl_->stringPointers.clear();
  for (auto &text : impl_->stringsScratch) impl_->stringPointers.push_back(text.data());
  return impl_->stringPointers.data();
}
char **SDDSFile::getColumnInString(char *name, uint32_t page) { return getColumnInString(getColumnIndex(name), page); }
void *SDDSFile::getInternalColumn(int32_t index, uint32_t page) {
  return std::visit([](const auto &values) -> void * {
    return const_cast<typename std::decay_t<decltype(values)>::value_type *>(values.data());
  }, impl_->page(page).column(index));
}
void *SDDSFile::getInternalColumn(char *name, uint32_t page) { return getInternalColumn(getColumnIndex(name), page); }
