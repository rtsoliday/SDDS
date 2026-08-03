/**
 * @file SDDS.hpp
 * @brief Modern C++17 interface for serial SDDS version 1 through 5 files.
 *
 * @copyright Copyright (c) 2026 The University of Chicago
 * @license Distributed under the Software License Agreement in LICENSE.
 */

#ifndef SDDS_CPP_HPP
#define SDDS_CPP_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32) && (defined(EXPORT_SDDSPP) || defined(EXPORT_SDDS3))
#  define SDDSPP_API __declspec(dllexport)
#elif defined(_WIN32)
#  define SDDSPP_API __declspec(dllimport)
#else
#  define SDDSPP_API
#endif

/* Source compatibility for the historical SDDS3 C++ export annotation. */
#ifndef SDDS3_API
#  define SDDS3_API SDDSPP_API
#endif

namespace sdds {

inline constexpr std::string_view libraryName = "SDDS++";
inline constexpr std::uint32_t cppApiVersion = 2;

enum class Type : std::int32_t {
  LongDouble = 1,
  Double = 2,
  Float = 3,
  Int64 = 4,
  UInt64 = 5,
  Int32 = 6,
  UInt32 = 7,
  Int16 = 8,
  UInt16 = 9,
  String = 10,
  Character = 11
};

enum class DataMode { Binary, Ascii };
enum class MajorOrder { Row, Column };
enum class ByteOrder { Native, Little, Big };
enum class RowCountMode { Variable, Fixed, None };
enum class Compression { Auto, None, Gzip, Xz, Lzma };
enum class RecoveryMode { Strict, Recover, Automatic };
enum class LongDoubleEncoding { Extended80, LegacyFloat64 };
enum class LockMode { None, Shared, Exclusive };
enum class UpdateStrategy { Automatic, InPlaceOnly, AtomicRewrite };
enum class LzmaCheck { None, Crc32, Crc64, Sha256 };
enum class RoundingMode { Reject, TowardZero, Nearest, Down, Up };
enum class ConversionMode { Exact, Checked };
enum class FieldKind { Parameter, Array, Column };
enum class LoadMode { All, None };

using Scalar = std::variant<long double, double, float, std::int64_t, std::uint64_t,
                            std::int32_t, std::uint32_t, std::int16_t, std::uint16_t,
                            std::string, char>;
using Values = std::variant<std::vector<long double>, std::vector<double>, std::vector<float>,
                            std::vector<std::int64_t>, std::vector<std::uint64_t>,
                            std::vector<std::int32_t>, std::vector<std::uint32_t>,
                            std::vector<std::int16_t>, std::vector<std::uint16_t>,
                            std::vector<std::string>, std::vector<char>>;

SDDSPP_API Type typeOf(const Scalar &value) noexcept;
SDDSPP_API Type typeOf(const Values &values) noexcept;
SDDSPP_API std::string_view typeName(Type type) noexcept;
SDDSPP_API Scalar convertScalar(const Scalar &value, Type target,
                               RoundingMode rounding = RoundingMode::Reject);
SDDSPP_API Values convertValues(const Values &values, Type target,
                               RoundingMode rounding = RoundingMode::Reject);

struct FieldMetadata {
  std::string name;
  std::optional<std::string> symbol;
  std::optional<std::string> units;
  std::optional<std::string> description;
  std::optional<std::string> format;
  Type type = Type::Double;
};

struct ParameterDefinition : FieldMetadata {
  std::optional<std::string> fixedValue;
};

struct ColumnDefinition : FieldMetadata {
  std::int32_t fieldLength = 0;
};

struct ArrayDefinition : FieldMetadata {
  std::int32_t fieldLength = 0;
  std::int32_t dimensions = 1;
  std::optional<std::string> groupName;
};

struct AssociateDefinition {
  std::string name;
  std::optional<std::string> filename;
  std::optional<std::string> path;
  std::optional<std::string> description;
  std::optional<std::string> contents;
  bool isSdds = false;
};

struct DataOptions {
  DataMode mode = DataMode::Binary;
  MajorOrder majorOrder = MajorOrder::Row;
  ByteOrder byteOrder = ByteOrder::Native;
  RowCountMode rowCountMode = RowCountMode::Variable;
  std::int32_t fixedRowIncrement = 500;
  std::int32_t linesPerRow = 1;
  std::int32_t additionalHeaderLines = 0;
  bool fsync = false;
};

struct Layout {
  std::optional<std::string> description;
  std::optional<std::string> contents;
  std::vector<ParameterDefinition> parameters;
  std::vector<ArrayDefinition> arrays;
  std::vector<ColumnDefinition> columns;
  std::vector<AssociateDefinition> associates;
  DataOptions data;
  std::int32_t version = 1;

  std::size_t parameterIndex(std::string_view name) const;
  std::size_t arrayIndex(std::string_view name) const;
  std::size_t columnIndex(std::string_view name) const;
  std::size_t associateIndex(std::string_view name) const;
};

class SDDSPP_API LayoutBuilder {
 public:
  LayoutBuilder() = default;
  explicit LayoutBuilder(Layout layout);
  LayoutBuilder &setDescription(std::optional<std::string> text,
                                std::optional<std::string> contents = std::nullopt);
  LayoutBuilder &setDataOptions(DataOptions options);
  LayoutBuilder &addParameter(ParameterDefinition definition);
  LayoutBuilder &addArray(ArrayDefinition definition);
  LayoutBuilder &addColumn(ColumnDefinition definition);
  LayoutBuilder &addAssociate(AssociateDefinition definition);
  Layout build() const;

 private:
  Layout layout_;
};

class SDDSPP_API LayoutEditor {
 public:
  explicit LayoutEditor(Layout layout);

  LayoutEditor &renameParameter(std::string_view name, std::string newName);
  LayoutEditor &renameArray(std::string_view name, std::string newName);
  LayoutEditor &renameColumn(std::string_view name, std::string newName);
  LayoutEditor &renameAssociate(std::string_view name, std::string newName);
  LayoutEditor &dropParameter(std::string_view name);
  LayoutEditor &dropArray(std::string_view name);
  LayoutEditor &dropColumn(std::string_view name);
  LayoutEditor &dropAssociate(std::string_view name);
  LayoutEditor &replaceParameter(std::string_view name, ParameterDefinition definition);
  LayoutEditor &replaceArray(std::string_view name, ArrayDefinition definition);
  LayoutEditor &replaceColumn(std::string_view name, ColumnDefinition definition);
  LayoutEditor &replaceAssociate(std::string_view name, AssociateDefinition definition);
  LayoutEditor &addParameter(ParameterDefinition definition);
  LayoutEditor &addArray(ArrayDefinition definition);
  LayoutEditor &addColumn(ColumnDefinition definition);
  LayoutEditor &addAssociate(AssociateDefinition definition);
  const Layout &layout() const noexcept { return layout_; }
  Layout build() const;

 private:
  Layout layout_;
};

struct ArrayData {
  std::vector<std::int32_t> dimensions;
  Values values;
};

struct FieldSelection {
  bool all = true;
  std::vector<std::string> names;

  static FieldSelection allFields() { return {}; }
  static FieldSelection noFields() { return {false, {}}; }
  static FieldSelection only(std::vector<std::string> selected) {
    return {false, std::move(selected)};
  }
};

struct RowSlice {
  std::int64_t first = 0;
  std::optional<std::int64_t> count;
  std::int64_t stride = 1;
  std::optional<std::int64_t> last;
};

struct ReadRequest {
  FieldSelection parameters;
  FieldSelection arrays;
  FieldSelection columns;
  RowSlice rows;
};

struct ReadSelection {
  std::int64_t sparseInterval = 1;
  std::int64_t sparseOffset = 0;
  std::optional<std::int64_t> lastRows;
};

class SDDSPP_API RowMask {
 public:
  RowMask() = default;
  explicit RowMask(std::size_t rows, bool selected = false);

  static RowMask all(std::size_t rows) { return RowMask(rows, true); }
  static RowMask none(std::size_t rows) { return RowMask(rows, false); }
  std::size_t size() const noexcept { return selected_.size(); }
  std::size_t count() const noexcept;
  bool test(std::size_t row) const;
  void set(std::size_t row, bool selected = true);
  RowMask &operator&=(const RowMask &other);
  RowMask &operator|=(const RowMask &other);
  RowMask operator~() const;

 private:
  std::vector<std::uint8_t> selected_;
};

inline RowMask operator&(RowMask left, const RowMask &right) { return left &= right; }
inline RowMask operator|(RowMask left, const RowMask &right) { return left |= right; }

class Page;

class SDDSPP_API RowView {
 public:
  std::int64_t index() const noexcept { return row_; }
  Scalar value(std::size_t column) const;
  Scalar value(std::string_view column) const;

  template <class T>
  T get(std::string_view column) const {
    return std::get<T>(value(column));
  }

  template <class T>
  T getConverted(std::string_view column,
                 RoundingMode rounding = RoundingMode::Reject) const;

 private:
  friend class Page;
  RowView(const Page *page, std::int64_t row) : page_(page), row_(row) {}
  const Page *page_ = nullptr;
  std::int64_t row_ = 0;
};

template <class T>
struct DenseMatrix {
  std::size_t rows = 0;
  std::size_t columns = 0;
  std::vector<T> values;

  const T &operator()(std::size_t row, std::size_t column) const {
    return values.at(row * columns + column);
  }
  T &operator()(std::size_t row, std::size_t column) {
    return values.at(row * columns + column);
  }
};

class SDDSPP_API Page {
 public:
  Page() = default;
  explicit Page(std::shared_ptr<const Layout> layout, LoadMode load = LoadMode::All);

  std::int64_t number() const noexcept { return number_; }
  std::int64_t rowCount() const noexcept { return rowCount_; }
  bool recovered() const noexcept { return recovered_; }
  const Layout &layout() const;

  bool parameterLoaded(std::size_t index) const;
  bool parameterLoaded(std::string_view name) const;
  bool arrayLoaded(std::size_t index) const;
  bool arrayLoaded(std::string_view name) const;
  bool columnLoaded(std::size_t index) const;
  bool columnLoaded(std::string_view name) const;
  bool allFieldsLoaded() const noexcept;

  const Scalar &parameter(std::size_t index) const;
  const Scalar &parameter(std::string_view name) const;
  const ArrayData &array(std::size_t index) const;
  const ArrayData &array(std::string_view name) const;
  const Values &column(std::size_t index) const;
  const Values &column(std::string_view name) const;
  const std::vector<Scalar> &parameters() const noexcept { return parameters_; }
  const std::vector<ArrayData> &arrays() const noexcept { return arrays_; }
  const std::vector<Values> &columns() const noexcept { return columns_; }

  void setParameter(std::size_t index, Scalar value);
  void setParameter(std::string_view name, Scalar value);
  void setArray(std::size_t index, ArrayData value);
  void setArray(std::string_view name, ArrayData value);
  void setColumn(std::size_t index, Values value);
  void setColumn(std::string_view name, Values value);
  RowView row(std::int64_t index) const;
  RowMask matchRows(std::string_view column,
                    const std::function<bool(const Scalar &)> &predicate) const;

  template <class T, class Predicate>
  RowMask matchRows(std::string_view column, Predicate predicate) const {
    return matchRows(column, std::function<bool(const Scalar &)>(
        [predicate = std::move(predicate)](const Scalar &value) {
          return predicate(std::get<T>(value));
        }));
  }
  Page filtered(const RowMask &mask) const;
  Page projected(const ReadRequest &request) const;

  template <class T>
  const T &parameterAs(std::string_view name) const {
    return std::get<T>(parameter(name));
  }

  template <class T>
  T parameterConverted(std::string_view name,
                       RoundingMode rounding = RoundingMode::Reject) const;

  template <class T>
  const std::vector<T> &columnAs(std::string_view name) const {
    return std::get<std::vector<T>>(column(name));
  }

  template <class T>
  std::vector<T> columnConverted(std::string_view name,
                                 RoundingMode rounding = RoundingMode::Reject) const;

  template <class T>
  const std::vector<T> &arrayAs(std::string_view name) const {
    return std::get<std::vector<T>>(array(name).values);
  }

  template <class T>
  std::vector<T> arrayConverted(std::string_view name,
                                RoundingMode rounding = RoundingMode::Reject) const;

  template <class T>
  DenseMatrix<T> matrix(const std::vector<std::string> &columns,
                        ConversionMode conversion = ConversionMode::Exact,
                        RoundingMode rounding = RoundingMode::Reject) const;

 private:
  friend class Reader;
  friend class Writer;
  friend class RowView;
  friend SDDSPP_API Page convertUnits(const Page &, FieldKind, std::string_view,
                                     std::optional<std::string>, long double);
  std::shared_ptr<const Layout> layout_;
  std::int64_t number_ = 0;
  std::int64_t rowCount_ = 0;
  bool recovered_ = false;
  std::vector<Scalar> parameters_;
  std::vector<ArrayData> arrays_;
  std::vector<Values> columns_;
  std::vector<bool> parametersLoaded_;
  std::vector<bool> arraysLoaded_;
  std::vector<bool> columnsLoaded_;
  std::uint64_t maxTransformationElements_ = UINT64_MAX;
};

struct ReaderLimits {
  std::int64_t maxRows = INT64_MAX;
  std::uint64_t maxElements = static_cast<std::uint64_t>(SIZE_MAX);
  std::uint64_t maxStringBytes = INT32_MAX;
  std::uint64_t maxLayoutCommandBytes = 16U * 1024U * 1024U;
  std::uint64_t maxDecompressedBytes = UINT64_MAX;
  std::uint64_t maxTransformationElements = static_cast<std::uint64_t>(SIZE_MAX);
  std::uint32_t maxIncludeDepth = 64;
  std::uint32_t maxProjectedFields = UINT32_MAX;
  std::uint64_t maxPageIndexEntries = UINT64_MAX;
};

struct ReaderOptions {
  Compression compression = Compression::Auto;
  RecoveryMode recovery = RecoveryMode::Automatic;
  LongDoubleEncoding longDoubleEncoding = LongDoubleEncoding::Extended80;
  LockMode lockMode = LockMode::None;
  std::size_t bufferBytes = 256U * 1024U;
  ReaderLimits limits;
};

struct WriterOptions {
  Compression compression = Compression::Auto;
  LongDoubleEncoding longDoubleEncoding = LongDoubleEncoding::Extended80;
  LockMode lockMode = LockMode::Exclusive;
  UpdateStrategy updateStrategy = UpdateStrategy::Automatic;
  std::size_t bufferBytes = 256U * 1024U;
  int gzipLevel = -1;
  std::uint32_t lzmaPreset = 6;
  LzmaCheck lzmaCheck = LzmaCheck::Crc64;
  std::int32_t minimumVersion = 1;
};

enum class ErrorKind { Io, Format, Type, State, Limit };

class SDDSPP_API Error : public std::runtime_error {
 public:
  Error(ErrorKind kind, std::string message, std::filesystem::path path = {},
        std::int64_t page = 0, std::optional<std::string> field = std::nullopt,
        std::optional<std::uint64_t> offset = std::nullopt,
        std::optional<std::int64_t> row = std::nullopt);

  ErrorKind kind() const noexcept { return kind_; }
  const std::filesystem::path &path() const noexcept { return path_; }
  std::int64_t page() const noexcept { return page_; }
  const std::optional<std::string> &field() const noexcept { return field_; }
  const std::optional<std::uint64_t> &offset() const noexcept { return offset_; }
  const std::optional<std::int64_t> &row() const noexcept { return row_; }

 private:
  ErrorKind kind_;
  std::filesystem::path path_;
  std::int64_t page_;
  std::optional<std::string> field_;
  std::optional<std::uint64_t> offset_;
  std::optional<std::int64_t> row_;
};

class SDDSPP_API IoError : public Error { public: using Error::Error; };
class SDDSPP_API FormatError : public Error { public: using Error::Error; };
class SDDSPP_API TypeError : public Error { public: using Error::Error; };
class SDDSPP_API StateError : public Error { public: using Error::Error; };
class SDDSPP_API LimitError : public Error { public: using Error::Error; };

struct SourceCapabilities {
  bool read = false;
  bool write = false;
  bool seek = false;
  bool truncate = false;
  bool flush = false;
  bool sync = false;
  bool reopen = false;
};

class SDDSPP_API InputSource {
 public:
  virtual ~InputSource() = default;
  virtual std::size_t read(void *data, std::size_t size) = 0;
  virtual bool eof() const = 0;
  virtual SourceCapabilities capabilities() const noexcept { return {}; }
  virtual std::uint64_t tell() const;
  virtual void seek(std::uint64_t offset);
  virtual void reopen();
  virtual void close() = 0;
};

class SDDSPP_API OutputSink {
 public:
  virtual ~OutputSink() = default;
  virtual void write(const void *data, std::size_t size) = 0;
  virtual SourceCapabilities capabilities() const noexcept { return {}; }
  virtual std::uint64_t tell() const;
  virtual void seek(std::uint64_t offset);
  virtual void truncate(std::uint64_t length);
  virtual void flush() = 0;
  virtual void sync();
  virtual void reopen();
  virtual void close() = 0;
};

SDDSPP_API std::unique_ptr<InputSource> inputFromPath(const std::filesystem::path &path);
SDDSPP_API std::unique_ptr<InputSource> inputFromStdin();
SDDSPP_API std::unique_ptr<InputSource> inputFromFile(FILE *file, bool takeOwnership = false);
SDDSPP_API std::unique_ptr<InputSource> inputFromStream(std::istream &stream);
SDDSPP_API std::unique_ptr<InputSource> inputFromMemory(const void *data, std::size_t size);
SDDSPP_API std::unique_ptr<OutputSink> outputToPath(const std::filesystem::path &path,
                                                   bool truncate = true);
SDDSPP_API std::unique_ptr<OutputSink> outputToStdout();
SDDSPP_API std::unique_ptr<OutputSink> outputToFile(FILE *file, bool takeOwnership = false);
SDDSPP_API std::unique_ptr<OutputSink> outputToStream(std::ostream &stream);
SDDSPP_API std::unique_ptr<OutputSink> outputToMemory(std::vector<std::uint8_t> &data);

struct MaterializedDataset {
  Layout layout;
  std::vector<Page> pages;
};

struct PageDelta {
  std::int64_t pageNumber = 0;
  std::int64_t firstRow = 0;
  Page page;
};

class SDDSPP_API Reader {
 public:
  static Reader open(const std::filesystem::path &path, ReaderOptions options = {});
  static Reader openHeaderless(const std::filesystem::path &path, Layout layout,
                               ReaderOptions options = {});
  static Reader fromStdin(ReaderOptions options = {});
  static Reader fromSource(std::unique_ptr<InputSource> source,
                           std::string sourceName = {}, ReaderOptions options = {});
  static Reader fromHeaderlessSource(std::unique_ptr<InputSource> source, Layout layout,
                                     std::string sourceName = {}, ReaderOptions options = {});

  Reader(Reader &&) noexcept;
  Reader &operator=(Reader &&) noexcept;
  Reader(const Reader &) = delete;
  Reader &operator=(const Reader &) = delete;
  ~Reader();

  const Layout &layout() const;
  std::optional<Page> next(ReadRequest request = {});
  [[deprecated("use next(ReadRequest)")]] std::optional<Page> next(ReadSelection selection);
  void gotoPage(std::int64_t pageNumber);
  void buildPageIndex();
  std::optional<std::int64_t> indexedPageCount() const noexcept;
  void disconnect();
  void reconnect();
  std::optional<PageDelta> readNewRows(ReadRequest request = {});
  MaterializedDataset readAll(ReadRequest request = {});
  void close();

 private:
  friend class Writer;
  struct Impl;
  explicit Reader(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

class SDDSPP_API Writer {
 public:
  static Writer create(const std::filesystem::path &path, Layout layout,
                       WriterOptions options = {});
  static Writer toStdout(Layout layout, WriterOptions options = {});
  static Writer toSink(std::unique_ptr<OutputSink> sink, Layout layout,
                       std::string sinkName = {}, WriterOptions options = {});
  static Writer append(const std::filesystem::path &path, WriterOptions options = {});
  static Writer appendToLastPage(const std::filesystem::path &path,
                                 std::int64_t updateInterval,
                                 WriterOptions options = {});

  Writer(Writer &&) noexcept;
  Writer &operator=(Writer &&) noexcept;
  Writer(const Writer &) = delete;
  Writer &operator=(const Writer &) = delete;
  ~Writer();

  const Layout &layout() const;
  std::int64_t rowsPresent() const noexcept;
  void write(const Page &page);
  void write(Page &&page);
  void beginPage(std::int64_t expectedRows);
  void setParameter(std::string_view name, Scalar value);
  void setArray(std::string_view name, ArrayData value);
  void setColumn(std::string_view name, Values value, std::int64_t startRow = 0);
  void commitPage();
  void updatePage(bool flushRows = false);
  void sync();
  void disconnect();
  void reconnect();
  void close();

 private:
  struct Impl;
  explicit Writer(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

struct CopyOptions {
  ReadRequest read;
  WriterOptions writer;
  std::function<Layout(const Layout &)> transformLayout;
  std::function<std::optional<Page>(Page &&, std::shared_ptr<const Layout>)> transformPage;
};

SDDSPP_API void copyDataset(Reader &reader, const std::filesystem::path &output,
                           CopyOptions options = {});
SDDSPP_API Page convertUnits(const Page &page, FieldKind kind, std::string_view name,
                            std::optional<std::string> units, long double factor);

namespace detail {

template <class T> struct TypeFor;
template <> struct TypeFor<long double> { static constexpr Type value = Type::LongDouble; };
template <> struct TypeFor<double> { static constexpr Type value = Type::Double; };
template <> struct TypeFor<float> { static constexpr Type value = Type::Float; };
template <> struct TypeFor<std::int64_t> { static constexpr Type value = Type::Int64; };
template <> struct TypeFor<std::uint64_t> { static constexpr Type value = Type::UInt64; };
template <> struct TypeFor<std::int32_t> { static constexpr Type value = Type::Int32; };
template <> struct TypeFor<std::uint32_t> { static constexpr Type value = Type::UInt32; };
template <> struct TypeFor<std::int16_t> { static constexpr Type value = Type::Int16; };
template <> struct TypeFor<std::uint16_t> { static constexpr Type value = Type::UInt16; };
template <> struct TypeFor<std::string> { static constexpr Type value = Type::String; };
template <> struct TypeFor<char> { static constexpr Type value = Type::Character; };

template <class T>
constexpr Type typeFor = TypeFor<T>::value;

}  // namespace detail

template <class T>
T Page::parameterConverted(std::string_view name, RoundingMode rounding) const {
  return std::get<T>(convertScalar(parameter(name), detail::typeFor<T>, rounding));
}

template <class T>
std::vector<T> Page::columnConverted(std::string_view name, RoundingMode rounding) const {
  const Values &source = column(name);
  const auto elements = std::visit([](const auto &values) { return values.size(); }, source);
  if (elements > maxTransformationElements_)
    throw LimitError(ErrorKind::Limit, "column conversion exceeds transformation output limit");
  return std::get<std::vector<T>>(convertValues(source, detail::typeFor<T>, rounding));
}

template <class T>
std::vector<T> Page::arrayConverted(std::string_view name, RoundingMode rounding) const {
  const Values &source = array(name).values;
  const auto elements = std::visit([](const auto &values) { return values.size(); }, source);
  if (elements > maxTransformationElements_)
    throw LimitError(ErrorKind::Limit, "array conversion exceeds transformation output limit");
  return std::get<std::vector<T>>(convertValues(source, detail::typeFor<T>, rounding));
}

template <class T>
T RowView::getConverted(std::string_view column, RoundingMode rounding) const {
  return std::get<T>(convertScalar(value(column), detail::typeFor<T>, rounding));
}

template <class T>
DenseMatrix<T> Page::matrix(const std::vector<std::string> &names,
                            ConversionMode conversion, RoundingMode rounding) const {
  DenseMatrix<T> result;
  result.rows = static_cast<std::size_t>(rowCount());
  result.columns = names.size();
  if (result.columns && result.rows > std::numeric_limits<std::size_t>::max() / result.columns)
    throw LimitError(ErrorKind::Limit, "matrix element count overflow");
  if (result.rows * result.columns > maxTransformationElements_)
    throw LimitError(ErrorKind::Limit, "matrix exceeds transformation output limit");
  result.values.reserve(result.rows * result.columns);
  std::vector<std::vector<T>> converted;
  converted.reserve(names.size());
  for (const auto &name : names) {
    if (conversion == ConversionMode::Exact)
      converted.push_back(columnAs<T>(name));
    else
      converted.push_back(columnConverted<T>(name, rounding));
  }
  for (std::size_t rowIndex = 0; rowIndex < result.rows; ++rowIndex)
    for (const auto &values : converted)
      result.values.push_back(values.at(rowIndex));
  return result;
}

}  // namespace sdds

#endif
