/**
 * @file SDDS.hpp
 * @brief Modern C++17 interface for serial SDDS version 1 through 5 files.
 *
 * @details Defines the strongly typed layout, page, reader, writer, stream,
 * conversion, transformation, and error interfaces provided by SDDS++.
 *
 * @copyright
 *   - (c) 2026 The University of Chicago
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
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

/** @brief Human-readable library name. */
inline constexpr std::string_view libraryName = "SDDS++";
/** @brief Source-level API generation implemented by this header. */
inline constexpr std::uint32_t cppApiVersion = 2;

/** @brief SDDS scalar data types and their protocol identifiers. */
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

/** @brief Physical encoding used for page data. */
enum class DataMode { Binary, Ascii };
/** @brief Ordering of column values in binary pages. */
enum class MajorOrder { Row, Column };
/** @brief Byte order used for numeric binary values. */
enum class ByteOrder { Native, Little, Big };
/** @brief Strategy used to encode page row counts. */
enum class RowCountMode { Variable, Fixed, None };
/** @brief Compression wrapper applied to a source or sink. */
enum class Compression { Auto, None, Gzip, Xz, Lzma };
/** @brief Policy for handling a recoverably truncated final page. */
enum class RecoveryMode { Strict, Recover, Automatic };
/** @brief On-disk representation used for long-double values. */
enum class LongDoubleEncoding { Extended80, LegacyFloat64 };
/** @brief Advisory locking mode for path-backed input and output. */
enum class LockMode { None, Shared, Exclusive };
/** @brief Policy for updating the last page of an existing dataset. */
enum class UpdateStrategy { Automatic, InPlaceOnly, AtomicRewrite };
/** @brief Integrity check stored in an LZMA or XZ stream. */
enum class LzmaCheck { None, Crc32, Crc64, Sha256 };
/** @brief Rounding policy for checked floating-point-to-integer conversion. */
enum class RoundingMode { Reject, TowardZero, Nearest, Down, Up };
/** @brief Whether matrix extraction requires exact types or checked conversion. */
enum class ConversionMode { Exact, Checked };
/** @brief Category of a field in an SDDS layout. */
enum class FieldKind { Parameter, Array, Column };
/** @brief Initial field-loading state for a manually constructed page. */
enum class LoadMode { All, None };

/** @brief Variant containing one value of any supported SDDS scalar type. */
using Scalar = std::variant<long double, double, float, std::int64_t, std::uint64_t,
                            std::int32_t, std::uint32_t, std::int16_t, std::uint16_t,
                            std::string, char>;
/** @brief Variant containing a homogeneous vector of any supported SDDS type. */
using Values = std::variant<std::vector<long double>, std::vector<double>, std::vector<float>,
                            std::vector<std::int64_t>, std::vector<std::uint64_t>,
                            std::vector<std::int32_t>, std::vector<std::uint32_t>,
                            std::vector<std::int16_t>, std::vector<std::uint16_t>,
                            std::vector<std::string>, std::vector<char>>;

/**
 * @brief Returns the SDDS type stored in a scalar variant.
 * @param value Scalar value to inspect.
 * @return Corresponding SDDS type.
 */
SDDSPP_API Type typeOf(const Scalar &value) noexcept;
/**
 * @brief Returns the SDDS type stored in a values variant.
 * @param values Homogeneous values to inspect.
 * @return Corresponding SDDS type.
 */
SDDSPP_API Type typeOf(const Values &values) noexcept;
/**
 * @brief Returns the protocol name for an SDDS type.
 * @param type Type to name.
 * @return Stable lowercase SDDS type name, or an empty view for an invalid type.
 */
SDDSPP_API std::string_view typeName(Type type) noexcept;
/**
 * @brief Converts one scalar with range and narrowing checks.
 * @param value Source value.
 * @param target Destination type.
 * @param rounding Floating-point-to-integer rounding policy.
 * @return Converted scalar.
 * @throws TypeError If the conversion is invalid or loses data without permission.
 */
SDDSPP_API Scalar convertScalar(const Scalar &value, Type target,
                               RoundingMode rounding = RoundingMode::Reject);
/**
 * @brief Converts a homogeneous values vector with range and narrowing checks.
 * @param values Source values.
 * @param target Destination type.
 * @param rounding Floating-point-to-integer rounding policy.
 * @return Converted homogeneous values.
 * @throws TypeError If any element cannot be converted.
 */
SDDSPP_API Values convertValues(const Values &values, Type target,
                               RoundingMode rounding = RoundingMode::Reject);

/** @brief Metadata common to parameters, arrays, and columns. */
struct FieldMetadata {
  std::string name;                       /**< Field name. */
  std::optional<std::string> symbol;      /**< Display symbol, when defined. */
  std::optional<std::string> units;       /**< Physical units, when defined. */
  std::optional<std::string> description; /**< Human-readable description. */
  std::optional<std::string> format;      /**< Suggested printf-style format. */
  Type type = Type::Double;               /**< Field data type. */
};

/** @brief Definition of one page-level scalar parameter. */
struct ParameterDefinition : FieldMetadata {
  std::optional<std::string> fixedValue; /**< Header-resident value, if fixed. */
};

/** @brief Definition of one row-varying column. */
struct ColumnDefinition : FieldMetadata {
  std::int32_t fieldLength = 0; /**< ASCII string field width, or zero. */
};

/** @brief Definition of one multidimensional page-level array. */
struct ArrayDefinition : FieldMetadata {
  std::int32_t fieldLength = 0;          /**< ASCII string field width, or zero. */
  std::int32_t dimensions = 1;           /**< Required number of dimensions. */
  std::optional<std::string> groupName;  /**< Optional array grouping name. */
};

/** @brief Definition of one external file association. */
struct AssociateDefinition {
  std::string name;                       /**< Associate name. */
  std::optional<std::string> filename;    /**< Associated filename. */
  std::optional<std::string> path;        /**< Optional associated path. */
  std::optional<std::string> description; /**< Description of the association. */
  std::optional<std::string> contents;    /**< Description of file contents. */
  bool isSdds = false;                    /**< Whether the associated file is SDDS. */
};

/** @brief Data-mode options serialized in the SDDS header. */
struct DataOptions {
  DataMode mode = DataMode::Binary;             /**< ASCII or binary page data. */
  MajorOrder majorOrder = MajorOrder::Row;      /**< Binary value ordering. */
  ByteOrder byteOrder = ByteOrder::Native;      /**< Binary numeric byte order. */
  RowCountMode rowCountMode = RowCountMode::Variable; /**< Row-count encoding. */
  std::int32_t fixedRowIncrement = 500;          /**< Fixed-mode update increment. */
  std::int32_t linesPerRow = 1;                  /**< ASCII lines used per row. */
  std::int32_t additionalHeaderLines = 0;        /**< Extra lines after the header. */
  bool fsync = false;                            /**< Request durable writer sync. */
};

/** @brief Complete immutable description of an SDDS dataset layout. */
struct Layout {
  std::optional<std::string> description; /**< Dataset description text. */
  std::optional<std::string> contents;    /**< Description of dataset contents. */
  std::vector<ParameterDefinition> parameters; /**< Parameter definitions in file order. */
  std::vector<ArrayDefinition> arrays;          /**< Array definitions in file order. */
  std::vector<ColumnDefinition> columns;        /**< Column definitions in file order. */
  std::vector<AssociateDefinition> associates; /**< Associate definitions in file order. */
  DataOptions data;                       /**< Page encoding options. */
  std::int32_t version = 1;               /**< SDDS protocol version, from 1 through 5. */

  /** @brief Finds a parameter by name. @param name Parameter name. @return Zero-based index. */
  std::size_t parameterIndex(std::string_view name) const;
  /** @brief Finds an array by name. @param name Array name. @return Zero-based index. */
  std::size_t arrayIndex(std::string_view name) const;
  /** @brief Finds a column by name. @param name Column name. @return Zero-based index. */
  std::size_t columnIndex(std::string_view name) const;
  /** @brief Finds an associate by name. @param name Associate name. @return Zero-based index. */
  std::size_t associateIndex(std::string_view name) const;
};

/** @brief Validating fluent builder for new layouts. */
class SDDSPP_API LayoutBuilder {
 public:
  /** @brief Creates an empty layout builder. */
  LayoutBuilder() = default;
  /** @brief Creates a builder initialized from a layout. @param layout Initial layout. */
  explicit LayoutBuilder(Layout layout);
  /** @brief Sets dataset description metadata. @param text Description text. @param contents Contents text. @return This builder. */
  LayoutBuilder &setDescription(std::optional<std::string> text,
                                std::optional<std::string> contents = std::nullopt);
  /** @brief Sets page encoding options. @param options New data options. @return This builder. */
  LayoutBuilder &setDataOptions(DataOptions options);
  /** @brief Appends a parameter definition. @param definition Definition to append. @return This builder. */
  LayoutBuilder &addParameter(ParameterDefinition definition);
  /** @brief Appends an array definition. @param definition Definition to append. @return This builder. */
  LayoutBuilder &addArray(ArrayDefinition definition);
  /** @brief Appends a column definition. @param definition Definition to append. @return This builder. */
  LayoutBuilder &addColumn(ColumnDefinition definition);
  /** @brief Appends an associate definition. @param definition Definition to append. @return This builder. */
  LayoutBuilder &addAssociate(AssociateDefinition definition);
  /** @brief Validates and returns the assembled layout. @return Validated layout. */
  Layout build() const;

 private:
  Layout layout_;
};

/** @brief Fluent editor for copying and changing an existing layout. */
class SDDSPP_API LayoutEditor {
 public:
  /** @brief Creates an editor initialized from a layout. @param layout Layout to edit. */
  explicit LayoutEditor(Layout layout);

  /** @brief Renames a parameter. @param name Current name. @param newName Replacement name. @return This editor. */
  LayoutEditor &renameParameter(std::string_view name, std::string newName);
  /** @brief Renames an array. @param name Current name. @param newName Replacement name. @return This editor. */
  LayoutEditor &renameArray(std::string_view name, std::string newName);
  /** @brief Renames a column. @param name Current name. @param newName Replacement name. @return This editor. */
  LayoutEditor &renameColumn(std::string_view name, std::string newName);
  /** @brief Renames an associate. @param name Current name. @param newName Replacement name. @return This editor. */
  LayoutEditor &renameAssociate(std::string_view name, std::string newName);
  /** @brief Drops a parameter. @param name Parameter name. @return This editor. */
  LayoutEditor &dropParameter(std::string_view name);
  /** @brief Drops an array. @param name Array name. @return This editor. */
  LayoutEditor &dropArray(std::string_view name);
  /** @brief Drops a column. @param name Column name. @return This editor. */
  LayoutEditor &dropColumn(std::string_view name);
  /** @brief Drops an associate. @param name Associate name. @return This editor. */
  LayoutEditor &dropAssociate(std::string_view name);
  /** @brief Replaces a parameter definition. @param name Current name. @param definition Replacement definition. @return This editor. */
  LayoutEditor &replaceParameter(std::string_view name, ParameterDefinition definition);
  /** @brief Replaces an array definition. @param name Current name. @param definition Replacement definition. @return This editor. */
  LayoutEditor &replaceArray(std::string_view name, ArrayDefinition definition);
  /** @brief Replaces a column definition. @param name Current name. @param definition Replacement definition. @return This editor. */
  LayoutEditor &replaceColumn(std::string_view name, ColumnDefinition definition);
  /** @brief Replaces an associate definition. @param name Current name. @param definition Replacement definition. @return This editor. */
  LayoutEditor &replaceAssociate(std::string_view name, AssociateDefinition definition);
  /** @brief Appends a parameter definition. @param definition Definition to append. @return This editor. */
  LayoutEditor &addParameter(ParameterDefinition definition);
  /** @brief Appends an array definition. @param definition Definition to append. @return This editor. */
  LayoutEditor &addArray(ArrayDefinition definition);
  /** @brief Appends a column definition. @param definition Definition to append. @return This editor. */
  LayoutEditor &addColumn(ColumnDefinition definition);
  /** @brief Appends an associate definition. @param definition Definition to append. @return This editor. */
  LayoutEditor &addAssociate(AssociateDefinition definition);
  /** @brief Returns the current working layout. @return Non-owning layout reference. */
  const Layout &layout() const noexcept { return layout_; }
  /** @brief Validates and returns the edited layout. @return Validated layout. */
  Layout build() const;

 private:
  Layout layout_;
};

/** @brief Materialized dimensions and values for one array field. */
struct ArrayData {
  std::vector<std::int32_t> dimensions; /**< Extent of each dimension. */
  Values values;                        /**< Flattened values in SDDS order. */
};

/** @brief Independent all, none, or named-field projection. */
struct FieldSelection {
  bool all = true;                 /**< Select all fields when true. */
  std::vector<std::string> names;  /**< Selected names when all is false. */

  /** @brief Selects every field in a category. @return All-fields selection. */
  static FieldSelection allFields() { return {}; }
  /** @brief Selects no fields in a category. @return Empty selection. */
  static FieldSelection noFields() { return {false, {}}; }
  /** @brief Selects named fields in a category. @param selected Field names. @return Named selection. */
  static FieldSelection only(std::vector<std::string> selected) {
    return {false, std::move(selected)};
  }
};

/** @brief Row range and stride requested from each page. */
struct RowSlice {
  std::int64_t first = 0;           /**< First zero-based row. */
  std::optional<std::int64_t> count; /**< Maximum selected source rows. */
  std::int64_t stride = 1;          /**< Positive row stride. */
  std::optional<std::int64_t> last; /**< Mutually exclusive trailing-row count. */
};

/** @brief Field projections and row selection for one reader operation. */
struct ReadRequest {
  FieldSelection parameters; /**< Parameter projection. */
  FieldSelection arrays;     /**< Array projection. */
  FieldSelection columns;    /**< Column projection. */
  RowSlice rows;              /**< Row selection. */
};

/** @brief Deprecated row-only selection retained for source migration. */
struct ReadSelection {
  std::int64_t sparseInterval = 1;        /**< Positive source-row stride. */
  std::int64_t sparseOffset = 0;          /**< First source row to consider. */
  std::optional<std::int64_t> lastRows;  /**< Optional trailing-row count. */
};

/** @brief Composable selection mask for page rows. */
class SDDSPP_API RowMask {
 public:
  RowMask() = default;
  /**
   * @brief Creates a mask with an initial value for every row.
   * @param rows Number of rows in the mask.
   * @param selected Initial selection state.
   */
  explicit RowMask(std::size_t rows, bool selected = false);

  /** @brief Creates a mask selecting every row. @param rows Row count. @return Mask. */
  static RowMask all(std::size_t rows) { return RowMask(rows, true); }
  /** @brief Creates a mask selecting no rows. @param rows Row count. @return Mask. */
  static RowMask none(std::size_t rows) { return RowMask(rows, false); }
  /** @brief Returns the number of mask entries. @return Row count. */
  std::size_t size() const noexcept { return selected_.size(); }
  /** @brief Counts selected rows. @return Number of selected rows. */
  std::size_t count() const noexcept;
  /** @brief Tests one row. @param row Zero-based row index. @return Selection state. */
  bool test(std::size_t row) const;
  /** @brief Changes one row. @param row Zero-based row index. @param selected New state. */
  void set(std::size_t row, bool selected = true);
  /** @brief Intersects this mask with another. @param other Mask of equal size. @return This mask. */
  RowMask &operator&=(const RowMask &other);
  /** @brief Unions this mask with another. @param other Mask of equal size. @return This mask. */
  RowMask &operator|=(const RowMask &other);
  /** @brief Inverts every selection bit. @return Inverted mask. */
  RowMask operator~() const;

 private:
  std::vector<std::uint8_t> selected_;
};

/** @brief Intersects two row masks. @param left First mask. @param right Second mask. @return Intersection. */
inline RowMask operator&(RowMask left, const RowMask &right) { return left &= right; }
/** @brief Unions two row masks. @param left First mask. @param right Second mask. @return Union. */
inline RowMask operator|(RowMask left, const RowMask &right) { return left |= right; }

class Page;

/** @brief Non-owning, zero-copy view of one row in a page. */
class SDDSPP_API RowView {
 public:
  /** @brief Returns the source row index. @return Zero-based row index. */
  std::int64_t index() const noexcept { return row_; }
  /** @brief Reads a cell by column index. @param column Column index. @return Cell value. */
  Scalar value(std::size_t column) const;
  /** @brief Reads a cell by column name. @param column Column name. @return Cell value. */
  Scalar value(std::string_view column) const;

  /**
   * @brief Reads a cell with an exact C++ type.
   * @tparam T Requested alternative in Scalar.
   * @param column Column name.
   * @return Cell value.
   * @throws std::bad_variant_access If the requested type is not exact.
   */
  template <class T>
  T get(std::string_view column) const {
    return std::get<T>(value(column));
  }

  /**
   * @brief Reads a cell using checked numeric conversion.
   * @tparam T Destination C++ type.
   * @param column Column name.
   * @param rounding Floating-point-to-integer rounding policy.
   * @return Converted cell value.
   */
  template <class T>
  T getConverted(std::string_view column,
                 RoundingMode rounding = RoundingMode::Reject) const;

 private:
  friend class Page;
  RowView(const Page *page, std::int64_t row) : page_(page), row_(row) {}
  const Page *page_ = nullptr;
  std::int64_t row_ = 0;
};

/**
 * @brief Contiguous row-major matrix extracted from numeric columns.
 * @tparam T Matrix element type.
 */
template <class T>
struct DenseMatrix {
  std::size_t rows = 0;    /**< Matrix row count. */
  std::size_t columns = 0; /**< Matrix column count. */
  std::vector<T> values;   /**< Contiguous row-major elements. */

  /** @brief Reads one matrix element. @param row Zero-based row. @param column Zero-based column. @return Element reference. */
  const T &operator()(std::size_t row, std::size_t column) const {
    return values.at(row * columns + column);
  }
  /** @brief Changes one matrix element. @param row Zero-based row. @param column Zero-based column. @return Element reference. */
  T &operator()(std::size_t row, std::size_t column) {
    return values.at(row * columns + column);
  }
};

/** @brief One materialized or projected SDDS data page. */
class SDDSPP_API Page {
 public:
  Page() = default;
  /**
   * @brief Creates an empty page governed by a shared immutable layout.
   * @param layout Layout shared with the page.
   * @param load Whether fields initially count as loaded.
   */
  explicit Page(std::shared_ptr<const Layout> layout, LoadMode load = LoadMode::All);

  /** @brief Returns the one-based file page number, or zero for a new page. */
  std::int64_t number() const noexcept { return number_; }
  /** @brief Returns the number of rows represented by this page. */
  std::int64_t rowCount() const noexcept { return rowCount_; }
  /** @brief Reports whether truncated-final-page recovery produced this page. */
  bool recovered() const noexcept { return recovered_; }
  /** @brief Returns the complete layout, including unloaded fields. */
  const Layout &layout() const;

  /** @brief Reports whether a parameter was loaded. @param index Parameter index. */
  bool parameterLoaded(std::size_t index) const;
  /** @brief Reports whether a parameter was loaded. @param name Parameter name. */
  bool parameterLoaded(std::string_view name) const;
  /** @brief Reports whether an array was loaded. @param index Array index. */
  bool arrayLoaded(std::size_t index) const;
  /** @brief Reports whether an array was loaded. @param name Array name. */
  bool arrayLoaded(std::string_view name) const;
  /** @brief Reports whether a column was loaded. @param index Column index. */
  bool columnLoaded(std::size_t index) const;
  /** @brief Reports whether a column was loaded. @param name Column name. */
  bool columnLoaded(std::string_view name) const;
  /** @brief Reports whether every layout field is loaded. */
  bool allFieldsLoaded() const noexcept;

  /** @brief Returns a parameter by index. @throws StateError If it is unloaded. */
  const Scalar &parameter(std::size_t index) const;
  /** @brief Returns a parameter by name. @throws StateError If it is unloaded. */
  const Scalar &parameter(std::string_view name) const;
  /** @brief Returns an array by index. @throws StateError If it is unloaded. */
  const ArrayData &array(std::size_t index) const;
  /** @brief Returns an array by name. @throws StateError If it is unloaded. */
  const ArrayData &array(std::string_view name) const;
  /** @brief Returns a column by index. @throws StateError If it is unloaded. */
  const Values &column(std::size_t index) const;
  /** @brief Returns a column by name. @throws StateError If it is unloaded. */
  const Values &column(std::string_view name) const;
  /** @brief Returns all parameter slots in layout order. @return Parameter values. */
  const std::vector<Scalar> &parameters() const noexcept { return parameters_; }
  /** @brief Returns all array slots in layout order. @return Array values. */
  const std::vector<ArrayData> &arrays() const noexcept { return arrays_; }
  /** @brief Returns all column slots in layout order. @return Column values. */
  const std::vector<Values> &columns() const noexcept { return columns_; }

  /** @brief Sets and marks a parameter loaded. @param index Parameter index. @param value New value. */
  void setParameter(std::size_t index, Scalar value);
  /** @brief Sets and marks a parameter loaded. @param name Parameter name. @param value New value. */
  void setParameter(std::string_view name, Scalar value);
  /** @brief Sets, validates, and marks an array loaded. @param index Array index. @param value New value. */
  void setArray(std::size_t index, ArrayData value);
  /** @brief Sets, validates, and marks an array loaded. @param name Array name. @param value New value. */
  void setArray(std::string_view name, ArrayData value);
  /** @brief Sets, validates, and marks a column loaded. @param index Column index. @param value New value. */
  void setColumn(std::size_t index, Values value);
  /** @brief Sets, validates, and marks a column loaded. @param name Column name. @param value New value. */
  void setColumn(std::string_view name, Values value);
  /** @brief Returns a zero-copy row view. @param index Zero-based row. @return View. */
  RowView row(std::int64_t index) const;
  /** @brief Builds a mask by applying a predicate to a column. @param column Column name. @param predicate Cell predicate. @return Matching rows. */
  RowMask matchRows(std::string_view column,
                    const std::function<bool(const Scalar &)> &predicate) const;

  /**
   * @brief Builds a typed row mask by applying a predicate to a column.
   * @tparam T Exact column element type.
   * @tparam Predicate Callable accepting a T value.
   * @param column Column name.
   * @param predicate Cell predicate.
   * @return Matching rows.
   */
  template <class T, class Predicate>
  RowMask matchRows(std::string_view column, Predicate predicate) const {
    return matchRows(column, std::function<bool(const Scalar &)>(
        [predicate = std::move(predicate)](const Scalar &value) {
          return predicate(std::get<T>(value));
        }));
  }
  /** @brief Returns an immutable row-filtered copy. @param mask Rows to retain. */
  Page filtered(const RowMask &mask) const;
  /** @brief Returns an immutable field and row projection. @param request Projection. */
  Page projected(const ReadRequest &request) const;

  /**
   * @brief Returns a parameter with an exact C++ type.
   * @tparam T Requested Scalar alternative.
   * @param name Parameter name.
   * @return Parameter value.
   */
  template <class T>
  const T &parameterAs(std::string_view name) const {
    return std::get<T>(parameter(name));
  }

  /**
   * @brief Returns a parameter using checked numeric conversion.
   * @tparam T Destination C++ type.
   * @param name Parameter name.
   * @param rounding Floating-point-to-integer rounding policy.
   * @return Converted value.
   */
  template <class T>
  T parameterConverted(std::string_view name,
                       RoundingMode rounding = RoundingMode::Reject) const;

  /**
   * @brief Returns a column with an exact C++ element type.
   * @tparam T Requested element type.
   * @param name Column name.
   * @return Column vector.
   */
  template <class T>
  const std::vector<T> &columnAs(std::string_view name) const {
    return std::get<std::vector<T>>(column(name));
  }

  /**
   * @brief Returns a column using checked numeric conversion.
   * @tparam T Destination element type.
   * @param name Column name.
   * @param rounding Floating-point-to-integer rounding policy.
   * @return Converted column vector.
   */
  template <class T>
  std::vector<T> columnConverted(std::string_view name,
                                 RoundingMode rounding = RoundingMode::Reject) const;

  /**
   * @brief Returns flattened array values with an exact C++ element type.
   * @tparam T Requested element type.
   * @param name Array name.
   * @return Flattened array vector.
   */
  template <class T>
  const std::vector<T> &arrayAs(std::string_view name) const {
    return std::get<std::vector<T>>(array(name).values);
  }

  /**
   * @brief Returns flattened array values using checked numeric conversion.
   * @tparam T Destination element type.
   * @param name Array name.
   * @param rounding Floating-point-to-integer rounding policy.
   * @return Converted flattened array vector.
   */
  template <class T>
  std::vector<T> arrayConverted(std::string_view name,
                                RoundingMode rounding = RoundingMode::Reject) const;

  /**
   * @brief Extracts selected numeric columns into a contiguous row-major matrix.
   * @tparam T Matrix element type.
   * @param columns Column names in result order.
   * @param conversion Exact-type or checked-conversion mode.
   * @param rounding Floating-point-to-integer rounding policy.
   * @return Extracted matrix.
   */
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

/** @brief Per-reader resource limits for untrusted or very large input. */
struct ReaderLimits {
  std::int64_t maxRows = INT64_MAX; /**< Maximum source rows in one page. */
  std::uint64_t maxElements = static_cast<std::uint64_t>(SIZE_MAX); /**< Maximum values in one field. */
  std::uint64_t maxStringBytes = INT32_MAX; /**< Maximum bytes in one string. */
  std::uint64_t maxLayoutCommandBytes = 16U * 1024U * 1024U; /**< Maximum header command bytes. */
  std::uint64_t maxDecompressedBytes = UINT64_MAX; /**< Maximum total decompressed bytes. */
  std::uint64_t maxTransformationElements = static_cast<std::uint64_t>(SIZE_MAX); /**< Maximum transform output elements. */
  std::uint32_t maxIncludeDepth = 64; /**< Maximum nested include depth. */
  std::uint32_t maxProjectedFields = UINT32_MAX; /**< Maximum named projected fields. */
  std::uint64_t maxPageIndexEntries = UINT64_MAX; /**< Maximum cached page offsets. */
};

/** @brief Per-object options used when opening an SDDS reader. */
struct ReaderOptions {
  Compression compression = Compression::Auto; /**< Selected or detected compression wrapper. */
  RecoveryMode recovery = RecoveryMode::Automatic; /**< Truncated-final-page policy. */
  LongDoubleEncoding longDoubleEncoding = LongDoubleEncoding::Extended80; /**< Long-double disk encoding. */
  LockMode lockMode = LockMode::None; /**< Advisory input locking mode. */
  std::size_t bufferBytes = 256U * 1024U; /**< Per-reader buffer size. */
  ReaderLimits limits; /**< Resource limits. */
};

/** @brief Per-object options used when creating or updating an SDDS writer. */
struct WriterOptions {
  Compression compression = Compression::Auto; /**< Compression wrapper. */
  LongDoubleEncoding longDoubleEncoding = LongDoubleEncoding::Extended80; /**< Long-double disk encoding. */
  LockMode lockMode = LockMode::Exclusive; /**< Advisory output locking mode. */
  UpdateStrategy updateStrategy = UpdateStrategy::Automatic; /**< Last-page update policy. */
  std::size_t bufferBytes = 256U * 1024U; /**< Per-writer buffer size. */
  int gzipLevel = -1; /**< zlib compression level, or -1 for its default. */
  std::uint32_t lzmaPreset = 6; /**< LZMA compression preset. */
  LzmaCheck lzmaCheck = LzmaCheck::Crc64; /**< LZMA or XZ integrity check. */
  std::int32_t minimumVersion = 1; /**< Minimum SDDS version emitted when compatible. */
};

/** @brief Stable category attached to every SDDS++ exception. */
enum class ErrorKind { Io, Format, Type, State, Limit };

/** @brief Base exception carrying parser and I/O context. */
class SDDSPP_API Error : public std::runtime_error {
 public:
  /**
   * @brief Creates an SDDS++ exception with optional diagnostic context.
   * @param kind Stable error category.
   * @param message Human-readable error message.
   * @param path Source or destination path.
   * @param page One-based page number, or zero when unavailable.
   * @param field Field name, when applicable.
   * @param offset Byte offset, when known.
   * @param row Zero-based row number, when applicable.
   */
  Error(ErrorKind kind, std::string message, std::filesystem::path path = {},
        std::int64_t page = 0, std::optional<std::string> field = std::nullopt,
        std::optional<std::uint64_t> offset = std::nullopt,
        std::optional<std::int64_t> row = std::nullopt);

  /** @brief Returns the stable error category. @return Error category. */
  ErrorKind kind() const noexcept { return kind_; }
  /** @brief Returns the associated path. @return Path, possibly empty. */
  const std::filesystem::path &path() const noexcept { return path_; }
  /** @brief Returns the associated page number. @return One-based page, or zero. */
  std::int64_t page() const noexcept { return page_; }
  /** @brief Returns the associated field. @return Optional field name. */
  const std::optional<std::string> &field() const noexcept { return field_; }
  /** @brief Returns the associated byte offset. @return Optional byte offset. */
  const std::optional<std::uint64_t> &offset() const noexcept { return offset_; }
  /** @brief Returns the associated row. @return Optional zero-based row. */
  const std::optional<std::int64_t> &row() const noexcept { return row_; }

 private:
  ErrorKind kind_;
  std::filesystem::path path_;
  std::int64_t page_;
  std::optional<std::string> field_;
  std::optional<std::uint64_t> offset_;
  std::optional<std::int64_t> row_;
};

/** @brief Input, output, seek, locking, or filesystem failure. */
class SDDSPP_API IoError : public Error { public: using Error::Error; };
/** @brief Malformed or unsupported SDDS content. */
class SDDSPP_API FormatError : public Error { public: using Error::Error; };
/** @brief Invalid type or checked conversion. */
class SDDSPP_API TypeError : public Error { public: using Error::Error; };
/** @brief Operation incompatible with the current object state. */
class SDDSPP_API StateError : public Error { public: using Error::Error; };
/** @brief Configured resource limit was exceeded. */
class SDDSPP_API LimitError : public Error { public: using Error::Error; };

/** @brief Capabilities advertised by a byte source or sink. */
struct SourceCapabilities {
  bool read = false;     /**< Supports reading bytes. */
  bool write = false;    /**< Supports writing bytes. */
  bool seek = false;     /**< Supports tell and absolute seek. */
  bool truncate = false; /**< Supports output truncation. */
  bool flush = false;    /**< Supports flushing buffered output. */
  bool sync = false;     /**< Supports durable synchronization. */
  bool reopen = false;   /**< Supports disconnect and reopen. */
};

/** @brief Abstract sequential or seekable byte source used by Reader. */
class SDDSPP_API InputSource {
 public:
  /** @brief Destroys the byte source. */
  virtual ~InputSource() = default;
  /** @brief Reads up to a requested byte count. @param data Destination buffer. @param size Buffer size. @return Number of bytes read. */
  virtual std::size_t read(void *data, std::size_t size) = 0;
  /** @brief Reports whether the source is at end of input. @return True at end of input. */
  virtual bool eof() const = 0;
  /** @brief Returns optional operations supported by the source. @return Capability flags. */
  virtual SourceCapabilities capabilities() const noexcept { return {}; }
  /** @brief Returns the current byte offset when supported. @return Absolute byte offset. */
  virtual std::uint64_t tell() const;
  /** @brief Seeks to an absolute byte offset when supported. @param offset Absolute byte offset. */
  virtual void seek(std::uint64_t offset);
  /** @brief Reopens a disconnected source when supported. */
  virtual void reopen();
  /** @brief Closes the source. */
  virtual void close() = 0;
};

/** @brief Abstract sequential or seekable byte sink used by Writer. */
class SDDSPP_API OutputSink {
 public:
  /** @brief Destroys the byte sink. */
  virtual ~OutputSink() = default;
  /** @brief Writes exactly the requested bytes or throws. @param data Source buffer. @param size Byte count. */
  virtual void write(const void *data, std::size_t size) = 0;
  /** @brief Returns optional operations supported by the sink. @return Capability flags. */
  virtual SourceCapabilities capabilities() const noexcept { return {}; }
  /** @brief Returns the current byte offset when supported. @return Absolute byte offset. */
  virtual std::uint64_t tell() const;
  /** @brief Seeks to an absolute byte offset when supported. @param offset Absolute byte offset. */
  virtual void seek(std::uint64_t offset);
  /** @brief Truncates the output to a byte length when supported. @param length New byte length. */
  virtual void truncate(std::uint64_t length);
  /** @brief Flushes process-local output buffers. */
  virtual void flush() = 0;
  /** @brief Requests durable synchronization when supported. */
  virtual void sync();
  /** @brief Reopens a disconnected sink when supported. */
  virtual void reopen();
  /** @brief Closes the sink. */
  virtual void close() = 0;
};

/** @brief Creates a seekable path-backed input source. @param path Input path. @return Owned source. */
SDDSPP_API std::unique_ptr<InputSource> inputFromPath(const std::filesystem::path &path);
/** @brief Creates a borrowed standard-input source. @return Owned adapter. */
SDDSPP_API std::unique_ptr<InputSource> inputFromStdin();
/** @brief Wraps a C FILE input stream. @param file Open stream. @param takeOwnership Whether closing the adapter closes the stream. @return Owned adapter. */
SDDSPP_API std::unique_ptr<InputSource> inputFromFile(FILE *file, bool takeOwnership = false);
/** @brief Wraps a borrowed C++ input stream. @param stream Open stream. @return Owned adapter. */
SDDSPP_API std::unique_ptr<InputSource> inputFromStream(std::istream &stream);
/** @brief Creates a caller-owned-memory input source. @param data First input byte. @param size Input byte count. @return Owned adapter. */
SDDSPP_API std::unique_ptr<InputSource> inputFromMemory(const void *data, std::size_t size);
/** @brief Creates a path-backed output sink. @param path Output path. @param truncate Whether to replace existing contents. @return Owned sink. */
SDDSPP_API std::unique_ptr<OutputSink> outputToPath(const std::filesystem::path &path,
                                                   bool truncate = true);
/** @brief Creates a borrowed standard-output sink. @return Owned adapter. */
SDDSPP_API std::unique_ptr<OutputSink> outputToStdout();
/** @brief Wraps a C FILE output stream. @param file Open stream. @param takeOwnership Whether closing the adapter closes the stream. @return Owned adapter. */
SDDSPP_API std::unique_ptr<OutputSink> outputToFile(FILE *file, bool takeOwnership = false);
/** @brief Wraps a borrowed C++ output stream. @param stream Open stream. @return Owned adapter. */
SDDSPP_API std::unique_ptr<OutputSink> outputToStream(std::ostream &stream);
/** @brief Creates a sink that appends to caller-owned memory. @param data Destination byte vector. @return Owned adapter. */
SDDSPP_API std::unique_ptr<OutputSink> outputToMemory(std::vector<std::uint8_t> &data);

/** @brief Complete layout and pages returned by Reader::readAll. */
struct MaterializedDataset {
  Layout layout;             /**< Complete dataset layout. */
  std::vector<Page> pages;   /**< Materialized pages in file order. */
};

/** @brief Newly observed rows returned by live-file polling. */
struct PageDelta {
  std::int64_t pageNumber = 0; /**< One-based updated page number. */
  std::int64_t firstRow = 0;   /**< First newly observed zero-based row. */
  Page page;                    /**< Page containing only newly observed rows. */
};

/** @brief Move-only streaming reader for serial SDDS datasets. */
class SDDSPP_API Reader {
 public:
  /** @brief Opens and parses a path-backed SDDS dataset. @param path Input path. @param options Reader options. @return Open reader. */
  static Reader open(const std::filesystem::path &path, ReaderOptions options = {});
  /** @brief Opens data without a header using a caller-supplied layout. @param path Input path. @param layout Validated data layout. @param options Reader options. @return Open reader. */
  static Reader openHeaderless(const std::filesystem::path &path, Layout layout,
                               ReaderOptions options = {});
  /** @brief Reads an SDDS dataset from standard input. @param options Reader options. @return Open reader. */
  static Reader fromStdin(ReaderOptions options = {});
  /** @brief Reads an SDDS dataset from a custom byte source. @param source Owned byte source. @param sourceName Diagnostic source name. @param options Reader options. @return Open reader. */
  static Reader fromSource(std::unique_ptr<InputSource> source,
                           std::string sourceName = {}, ReaderOptions options = {});
  /** @brief Reads headerless data from a custom byte source. @param source Owned byte source. @param layout Validated data layout. @param sourceName Diagnostic source name. @param options Reader options. @return Open reader. */
  static Reader fromHeaderlessSource(std::unique_ptr<InputSource> source, Layout layout,
                                     std::string sourceName = {}, ReaderOptions options = {});

  /** @brief Move-constructs a reader. @param other Reader to consume. */
  Reader(Reader &&other) noexcept;
  /** @brief Move-assigns a reader. @param other Reader to consume. @return This reader. */
  Reader &operator=(Reader &&other) noexcept;
  Reader(const Reader &) = delete;
  Reader &operator=(const Reader &) = delete;
  /** @brief Closes and destroys the reader. */
  ~Reader();

  /** @brief Returns the parsed immutable layout. @return Layout reference. */
  const Layout &layout() const;
  /** @brief Decodes the next page according to a projection request. @param request Field and row projection. @return Next page, or no value at end of input. */
  std::optional<Page> next(ReadRequest request = {});
  /** @brief Decodes the next page using the deprecated row-only selection. @param selection Legacy row selection. @return Next page, or no value at end of input. */
  [[deprecated("use next(ReadRequest)")]] std::optional<Page> next(ReadSelection selection);
  /** @brief Positions the next read at a one-based page number. @param pageNumber Destination page. */
  void gotoPage(std::int64_t pageNumber);
  /** @brief Eagerly scans and caches all page offsets. */
  void buildPageIndex();
  /** @brief Returns the indexed page count when it is known. @return Page count, or no value before complete indexing. */
  std::optional<std::int64_t> indexedPageCount() const noexcept;
  /** @brief Closes a path handle while preserving reconnect state. */
  void disconnect();
  /** @brief Reopens a disconnected path after replacement checks. */
  void reconnect();
  /** @brief Nonblockingly returns rows appended to the updateable final page. @param request Field and row projection. @return New rows, or no value when unchanged. */
  std::optional<PageDelta> readNewRows(ReadRequest request = {});
  /** @brief Materializes every remaining page. @param request Field and row projection. @return Layout and remaining pages. */
  MaterializedDataset readAll(ReadRequest request = {});
  /** @brief Closes the reader and its owned source. */
  void close();

 private:
  friend class Writer;
  struct Impl;
  explicit Reader(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

/** @brief Move-only streaming writer and updater for serial SDDS datasets. */
class SDDSPP_API Writer {
 public:
  /** @brief Creates or replaces a path-backed dataset. @param path Output path. @param layout Output layout. @param options Writer options. @return Open writer. */
  static Writer create(const std::filesystem::path &path, Layout layout,
                       WriterOptions options = {});
  /** @brief Writes a dataset to standard output. @param layout Output layout. @param options Writer options. @return Open writer. */
  static Writer toStdout(Layout layout, WriterOptions options = {});
  /** @brief Writes a dataset to a custom byte sink. @param sink Owned byte sink. @param layout Output layout. @param sinkName Diagnostic sink name. @param options Writer options. @return Open writer. */
  static Writer toSink(std::unique_ptr<OutputSink> sink, Layout layout,
                       std::string sinkName = {}, WriterOptions options = {});
  /** @brief Opens a dataset for appending complete pages. @param path Existing dataset path. @param options Writer options. @return Open writer. */
  static Writer append(const std::filesystem::path &path, WriterOptions options = {});
  /** @brief Opens the final page for compatible incremental updates. @param path Existing dataset path. @param updateInterval Row-count publication interval. @param options Writer options. @return Open writer. */
  static Writer appendToLastPage(const std::filesystem::path &path,
                                 std::int64_t updateInterval,
                                 WriterOptions options = {});

  /** @brief Move-constructs a writer. @param other Writer to consume. */
  Writer(Writer &&other) noexcept;
  /** @brief Move-assigns a writer. @param other Writer to consume. @return This writer. */
  Writer &operator=(Writer &&other) noexcept;
  Writer(const Writer &) = delete;
  Writer &operator=(const Writer &) = delete;
  /** @brief Finalizes and destroys the writer. */
  ~Writer();

  /** @brief Returns the immutable output layout. @return Layout reference. */
  const Layout &layout() const;
  /** @brief Returns rows currently staged or present during an update. @return Row count. */
  std::int64_t rowsPresent() const noexcept;
  /** @brief Writes a complete page by copying its values. @param page Page to write. */
  void write(const Page &page);
  /** @brief Writes a complete page by moving its values. @param page Page to consume and write. */
  void write(Page &&page);
  /** @brief Starts an incrementally populated page. @param expectedRows Expected final row count. */
  void beginPage(std::int64_t expectedRows);
  /** @brief Sets one parameter on the active page. @param name Parameter name. @param value New value. */
  void setParameter(std::string_view name, Scalar value);
  /** @brief Sets one array on the active page. @param name Array name. @param value New value. */
  void setArray(std::string_view name, ArrayData value);
  /** @brief Sets or extends one column on the active page. @param name Column name. @param value Values to write. @param startRow First destination row. */
  void setColumn(std::string_view name, Values value, std::int64_t startRow = 0);
  /** @brief Validates and writes the active page. */
  void commitPage();
  /** @brief Publishes an updateable page row count and optionally flushes. @param flushRows Whether to flush row data immediately. */
  void updatePage(bool flushRows = false);
  /** @brief Flushes and requests durable synchronization. */
  void sync();
  /** @brief Closes a path handle while preserving reconnect state. */
  void disconnect();
  /** @brief Reopens a disconnected output path after replacement checks. */
  void reconnect();
  /** @brief Finalizes and closes the writer. */
  void close();

 private:
  struct Impl;
  explicit Writer(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

/** @brief Optional read, write, layout, and page transforms for copyDataset. */
struct CopyOptions {
  ReadRequest read; /**< Source field and row projection. */
  WriterOptions writer; /**< Destination writer options. */
  std::function<Layout(const Layout &)> transformLayout; /**< Optional layout transformation. */
  std::function<std::optional<Page>(Page &&, std::shared_ptr<const Layout>)> transformPage; /**< Optional page transformation or rejection. */
};

/**
 * @brief Streams a dataset through optional layout and page transformations.
 * @param reader Source reader positioned before the first page to copy.
 * @param output Destination path.
 * @param options Projection, writer options, and callbacks.
 */
SDDSPP_API void copyDataset(Reader &reader, const std::filesystem::path &output,
                           CopyOptions options = {});
/**
 * @brief Scales a field and changes its unit metadata as one operation.
 * @param page Source page.
 * @param kind Field category.
 * @param name Field name.
 * @param units Replacement units, or no units.
 * @param factor Multiplicative conversion factor.
 * @return Converted page with an updated layout.
 */
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
