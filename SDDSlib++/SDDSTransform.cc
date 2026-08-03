/**
 * @file SDDSTransform.cc
 * @brief Typed layout and page transformations for the C++17 SDDS interface.
 *
 * @copyright Copyright (c) 2026 The University of Chicago
 * @license Distributed under the Software License Agreement in LICENSE.
 */

#include "SDDS.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace sdds {
namespace {

[[noreturn]] void typeError(const std::string &message,
                            std::optional<std::string> field = std::nullopt) {
  throw TypeError(ErrorKind::Type, message, {}, 0, std::move(field));
}

[[noreturn]] void stateError(const std::string &message) {
  throw StateError(ErrorKind::State, message);
}

template <class To, class From>
To checkedIntegralCast(From value) {
  static_assert(std::is_integral_v<To> && std::is_integral_v<From>);
  if constexpr (std::is_signed_v<From> == std::is_signed_v<To>) {
    if constexpr (sizeof(To) < sizeof(From)) {
      if (value < static_cast<From>(std::numeric_limits<To>::lowest()) ||
          value > static_cast<From>(std::numeric_limits<To>::max()))
        typeError("numeric conversion is out of range");
    }
  } else if constexpr (std::is_signed_v<From>) {
    if (value < 0)
      typeError("negative value cannot be converted to an unsigned type");
    using UnsignedFrom = std::make_unsigned_t<From>;
    if constexpr (sizeof(To) < sizeof(UnsignedFrom))
      if (static_cast<UnsignedFrom>(value) > std::numeric_limits<To>::max())
        typeError("numeric conversion is out of range");
  } else {
    using UnsignedTo = std::make_unsigned_t<To>;
    if constexpr (sizeof(To) <= sizeof(From))
      if (value > static_cast<From>(static_cast<UnsignedTo>(std::numeric_limits<To>::max())))
        typeError("numeric conversion is out of range");
  }
  return static_cast<To>(value);
}

long double rounded(long double value, RoundingMode mode) {
  if (!std::isfinite(value))
    typeError("non-finite value cannot be converted to an integer");
  switch (mode) {
  case RoundingMode::Reject:
    if (std::trunc(value) != value)
      typeError("floating-to-integer conversion requires an explicit rounding mode");
    return value;
  case RoundingMode::TowardZero: return std::trunc(value);
  case RoundingMode::Nearest: return std::nearbyint(value);
  case RoundingMode::Down: return std::floor(value);
  case RoundingMode::Up: return std::ceil(value);
  }
  typeError("unknown rounding mode");
}

template <class To, class From>
To checkedNumericCast(From value, RoundingMode rounding) {
  static_assert(std::is_arithmetic_v<To> && std::is_arithmetic_v<From>);
  if constexpr (std::is_integral_v<To> && std::is_integral_v<From>) {
    return checkedIntegralCast<To>(value);
  } else if constexpr (std::is_integral_v<To>) {
    const long double converted = rounded(static_cast<long double>(value), rounding);
    if (converted < static_cast<long double>(std::numeric_limits<To>::lowest()) ||
        converted > static_cast<long double>(std::numeric_limits<To>::max()))
      typeError("numeric conversion is out of range");
    return static_cast<To>(converted);
  } else {
    const long double converted = static_cast<long double>(value);
    if (std::isfinite(converted) &&
        (converted < -static_cast<long double>(std::numeric_limits<To>::max()) ||
         converted > static_cast<long double>(std::numeric_limits<To>::max())))
      typeError("numeric conversion is out of range");
    return static_cast<To>(value);
  }
}

template <class To>
Scalar convertTo(const Scalar &value, RoundingMode rounding) {
  return std::visit([&](const auto &source) -> Scalar {
    using From = std::decay_t<decltype(source)>;
    if constexpr (std::is_same_v<From, std::string>) {
      typeError("strings cannot be converted to numeric SDDS types");
    } else {
      return checkedNumericCast<To>(source, rounding);
    }
  }, value);
}

Scalar scalarAt(const Values &values, std::size_t index) {
  return std::visit([&](const auto &items) -> Scalar { return items.at(index); }, values);
}

std::uint64_t valueCount(const Values &values) {
  return std::visit([](const auto &items) {
    return static_cast<std::uint64_t>(items.size());
  }, values);
}

void appendScalar(Values &values, Scalar value) {
  std::visit([&](auto &items) {
    using Vector = std::decay_t<decltype(items)>;
    using Value = typename Vector::value_type;
    items.push_back(std::get<Value>(std::move(value)));
  }, values);
}

Values emptyValues(Type type) {
  switch (type) {
  case Type::LongDouble: return std::vector<long double>{};
  case Type::Double: return std::vector<double>{};
  case Type::Float: return std::vector<float>{};
  case Type::Int64: return std::vector<std::int64_t>{};
  case Type::UInt64: return std::vector<std::uint64_t>{};
  case Type::Int32: return std::vector<std::int32_t>{};
  case Type::UInt32: return std::vector<std::uint32_t>{};
  case Type::Int16: return std::vector<std::int16_t>{};
  case Type::UInt16: return std::vector<std::uint16_t>{};
  case Type::String: return std::vector<std::string>{};
  case Type::Character: return std::vector<char>{};
  }
  typeError("unknown SDDS type");
}

template <class Definition>
void renameDefinition(std::vector<Definition> &definitions, std::size_t index,
                      std::string newName) {
  if (newName.empty())
    typeError("definition names cannot be empty");
  for (std::size_t existing = 0; existing < definitions.size(); ++existing)
    if (existing != index && definitions[existing].name == newName)
      typeError("duplicate definition name: " + newName, newName);
  definitions.at(index).name = std::move(newName);
}

template <class Definition>
void replaceDefinition(std::vector<Definition> &definitions, std::size_t index,
                       Definition definition) {
  if (definition.name.empty())
    typeError("definition names cannot be empty");
  for (std::size_t existing = 0; existing < definitions.size(); ++existing)
    if (existing != index && definitions[existing].name == definition.name)
      typeError("duplicate definition name: " + definition.name, definition.name);
  definitions.at(index) = std::move(definition);
}

template <class Definition>
void eraseDefinition(std::vector<Definition> &definitions, std::size_t index) {
  definitions.erase(definitions.begin() + static_cast<std::ptrdiff_t>(index));
}

bool selectedRow(const RowSlice &slice, std::int64_t row, std::int64_t total) {
  if (slice.first < 0 || slice.stride < 1 || (slice.count && *slice.count < 0) ||
      (slice.last && *slice.last < 0) || (slice.last && (slice.first || slice.count)))
    stateError("invalid row slice");
  const std::int64_t begin = slice.last ? std::max<std::int64_t>(0, total - *slice.last)
                                        : std::min(slice.first, total);
  if (row < begin || (row - begin) % slice.stride)
    return false;
  if (!slice.count)
    return true;
  return (row - begin) / slice.stride < *slice.count;
}

std::vector<bool> selectedFields(const FieldSelection &selection,
                                 const std::vector<std::string> &names) {
  std::vector<bool> result(names.size(), selection.all);
  if (selection.all && selection.names.empty())
    return result;
  if (selection.all)
    stateError("an all-fields selection cannot also list field names");
  for (const auto &name : selection.names) {
    const auto found = std::find(names.begin(), names.end(), name);
    if (found == names.end())
      typeError("unknown projected field: " + name, name);
    result[static_cast<std::size_t>(found - names.begin())] = true;
  }
  return result;
}

template <class Definition>
std::vector<std::string> namesOf(const std::vector<Definition> &definitions) {
  std::vector<std::string> names;
  names.reserve(definitions.size());
  for (const auto &definition : definitions)
    names.push_back(definition.name);
  return names;
}

Scalar scaledScalar(const Scalar &value, Type type, long double factor) {
  if (type == Type::String)
    typeError("unit conversion requires numeric data");
  const long double source = std::visit([](const auto &item) -> long double {
    using Value = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<Value, std::string>)
      typeError("unit conversion requires numeric data");
    else
      return static_cast<long double>(item);
  }, value);
  return convertScalar(Scalar(source * factor), type, RoundingMode::Reject);
}

Values scaledValues(const Values &values, Type type, long double factor) {
  Values result = emptyValues(type);
  const std::size_t count = std::visit([](const auto &items) { return items.size(); }, values);
  for (std::size_t index = 0; index < count; ++index)
    appendScalar(result, scaledScalar(scalarAt(values, index), type, factor));
  return result;
}

}  // namespace

Scalar convertScalar(const Scalar &value, Type target, RoundingMode rounding) {
  if (target == Type::String) {
    if (const auto *text = std::get_if<std::string>(&value))
      return *text;
    typeError("numeric values cannot be implicitly converted to strings");
  }
  switch (target) {
  case Type::LongDouble: return convertTo<long double>(value, rounding);
  case Type::Double: return convertTo<double>(value, rounding);
  case Type::Float: return convertTo<float>(value, rounding);
  case Type::Int64: return convertTo<std::int64_t>(value, rounding);
  case Type::UInt64: return convertTo<std::uint64_t>(value, rounding);
  case Type::Int32: return convertTo<std::int32_t>(value, rounding);
  case Type::UInt32: return convertTo<std::uint32_t>(value, rounding);
  case Type::Int16: return convertTo<std::int16_t>(value, rounding);
  case Type::UInt16: return convertTo<std::uint16_t>(value, rounding);
  case Type::Character: return convertTo<char>(value, rounding);
  case Type::String: break;
  }
  typeError("unknown conversion target");
}

Values convertValues(const Values &values, Type target, RoundingMode rounding) {
  if (typeOf(values) == target)
    return values;
  Values result = emptyValues(target);
  const std::size_t count = std::visit([](const auto &items) { return items.size(); }, values);
  for (std::size_t index = 0; index < count; ++index)
    appendScalar(result, convertScalar(scalarAt(values, index), target, rounding));
  return result;
}

LayoutEditor::LayoutEditor(Layout layout) : layout_(std::move(layout)) {}

LayoutEditor &LayoutEditor::renameParameter(std::string_view name, std::string newName) {
  renameDefinition(layout_.parameters, layout_.parameterIndex(name), std::move(newName));
  return *this;
}
LayoutEditor &LayoutEditor::renameArray(std::string_view name, std::string newName) {
  renameDefinition(layout_.arrays, layout_.arrayIndex(name), std::move(newName));
  return *this;
}
LayoutEditor &LayoutEditor::renameColumn(std::string_view name, std::string newName) {
  renameDefinition(layout_.columns, layout_.columnIndex(name), std::move(newName));
  return *this;
}
LayoutEditor &LayoutEditor::renameAssociate(std::string_view name, std::string newName) {
  renameDefinition(layout_.associates, layout_.associateIndex(name), std::move(newName));
  return *this;
}
LayoutEditor &LayoutEditor::dropParameter(std::string_view name) {
  eraseDefinition(layout_.parameters, layout_.parameterIndex(name)); return *this;
}
LayoutEditor &LayoutEditor::dropArray(std::string_view name) {
  eraseDefinition(layout_.arrays, layout_.arrayIndex(name)); return *this;
}
LayoutEditor &LayoutEditor::dropColumn(std::string_view name) {
  eraseDefinition(layout_.columns, layout_.columnIndex(name)); return *this;
}
LayoutEditor &LayoutEditor::dropAssociate(std::string_view name) {
  eraseDefinition(layout_.associates, layout_.associateIndex(name)); return *this;
}
LayoutEditor &LayoutEditor::replaceParameter(std::string_view name,
                                             ParameterDefinition definition) {
  replaceDefinition(layout_.parameters, layout_.parameterIndex(name), std::move(definition));
  return *this;
}
LayoutEditor &LayoutEditor::replaceArray(std::string_view name, ArrayDefinition definition) {
  replaceDefinition(layout_.arrays, layout_.arrayIndex(name), std::move(definition));
  return *this;
}
LayoutEditor &LayoutEditor::replaceColumn(std::string_view name, ColumnDefinition definition) {
  replaceDefinition(layout_.columns, layout_.columnIndex(name), std::move(definition));
  return *this;
}
LayoutEditor &LayoutEditor::replaceAssociate(std::string_view name,
                                             AssociateDefinition definition) {
  replaceDefinition(layout_.associates, layout_.associateIndex(name), std::move(definition));
  return *this;
}
LayoutEditor &LayoutEditor::addParameter(ParameterDefinition definition) {
  layout_ = LayoutBuilder(layout_).addParameter(std::move(definition)).build(); return *this;
}
LayoutEditor &LayoutEditor::addArray(ArrayDefinition definition) {
  layout_ = LayoutBuilder(layout_).addArray(std::move(definition)).build(); return *this;
}
LayoutEditor &LayoutEditor::addColumn(ColumnDefinition definition) {
  layout_ = LayoutBuilder(layout_).addColumn(std::move(definition)).build(); return *this;
}
LayoutEditor &LayoutEditor::addAssociate(AssociateDefinition definition) {
  layout_ = LayoutBuilder(layout_).addAssociate(std::move(definition)).build(); return *this;
}
Layout LayoutEditor::build() const { return LayoutBuilder(layout_).build(); }

RowMask::RowMask(std::size_t rows, bool selected) : selected_(rows, selected ? 1U : 0U) {}
std::size_t RowMask::count() const noexcept {
  return static_cast<std::size_t>(std::count(selected_.begin(), selected_.end(), 1U));
}
bool RowMask::test(std::size_t row) const { return selected_.at(row) != 0; }
void RowMask::set(std::size_t row, bool selected) { selected_.at(row) = selected ? 1U : 0U; }
RowMask &RowMask::operator&=(const RowMask &other) {
  if (size() != other.size()) stateError("row masks have different sizes");
  for (std::size_t row = 0; row < size(); ++row) selected_[row] &= other.selected_[row];
  return *this;
}
RowMask &RowMask::operator|=(const RowMask &other) {
  if (size() != other.size()) stateError("row masks have different sizes");
  for (std::size_t row = 0; row < size(); ++row) selected_[row] |= other.selected_[row];
  return *this;
}
RowMask RowMask::operator~() const {
  RowMask result = *this;
  for (auto &selected : result.selected_) selected = selected ? 0U : 1U;
  return result;
}

Scalar RowView::value(std::size_t column) const {
  if (!page_) stateError("row view has no page");
  if (row_ < 0 || row_ >= page_->rowCount()) stateError("row index is out of range");
  return scalarAt(page_->column(column), static_cast<std::size_t>(row_));
}
Scalar RowView::value(std::string_view column) const {
  return value(page_->layout().columnIndex(column));
}

RowView Page::row(std::int64_t index) const {
  if (index < 0 || index >= rowCount_) stateError("row index is out of range");
  return RowView(this, index);
}

RowMask Page::matchRows(std::string_view name,
                        const std::function<bool(const Scalar &)> &predicate) const {
  if (!predicate) stateError("row predicate is empty");
  const Values &values = column(name);
  RowMask result(static_cast<std::size_t>(rowCount_));
  for (std::size_t rowIndex = 0; rowIndex < static_cast<std::size_t>(rowCount_); ++rowIndex)
    result.set(rowIndex, predicate(scalarAt(values, rowIndex)));
  return result;
}

Page Page::filtered(const RowMask &mask) const {
  if (mask.size() != static_cast<std::size_t>(rowCount_))
    stateError("row mask size does not match the page");
  Page result(layout_, LoadMode::None);
  result.maxTransformationElements_ = maxTransformationElements_;
  result.number_ = number_;
  result.recovered_ = recovered_;
  std::uint64_t elements = 0;
  const auto addElements = [&](std::uint64_t amount) {
    if (amount > maxTransformationElements_ - elements)
      throw LimitError(ErrorKind::Limit,
                       "page filtering exceeds transformation output limit");
    elements += amount;
  };
  for (std::size_t index = 0; index < parameters_.size(); ++index)
    if (parameterLoaded(index)) {
      addElements(1);
      result.setParameter(index, parameters_[index]);
    }
  for (std::size_t index = 0; index < arrays_.size(); ++index)
    if (arrayLoaded(index)) {
      addElements(valueCount(arrays_[index].values));
      result.setArray(index, arrays_[index]);
    }
  for (std::size_t columnIndex = 0; columnIndex < columns_.size(); ++columnIndex) {
    if (!columnLoaded(columnIndex)) continue;
    addElements(static_cast<std::uint64_t>(mask.count()));
    Values selected = emptyValues(layout().columns[columnIndex].type);
    for (std::size_t rowIndex = 0; rowIndex < mask.size(); ++rowIndex)
      if (mask.test(rowIndex)) appendScalar(selected, scalarAt(columns_[columnIndex], rowIndex));
    result.setColumn(columnIndex, std::move(selected));
  }
  result.rowCount_ = static_cast<std::int64_t>(mask.count());
  return result;
}

Page Page::projected(const ReadRequest &request) const {
  const auto parameterSelection = selectedFields(request.parameters, namesOf(layout().parameters));
  const auto arraySelection = selectedFields(request.arrays, namesOf(layout().arrays));
  const auto columnSelection = selectedFields(request.columns, namesOf(layout().columns));
  Page result(layout_, LoadMode::None);
  result.maxTransformationElements_ = maxTransformationElements_;
  result.number_ = number_;
  result.recovered_ = recovered_;
  for (std::size_t index = 0; index < parameters_.size(); ++index)
    if (parameterSelection[index] && parameterLoaded(index)) result.setParameter(index, parameters_[index]);
  for (std::size_t index = 0; index < arrays_.size(); ++index)
    if (arraySelection[index] && arrayLoaded(index)) result.setArray(index, arrays_[index]);
  for (std::size_t index = 0; index < columns_.size(); ++index)
    if (columnSelection[index] && columnLoaded(index)) result.setColumn(index, columns_[index]);
  RowMask rows(static_cast<std::size_t>(rowCount_));
  for (std::int64_t index = 0; index < rowCount_; ++index)
    rows.set(static_cast<std::size_t>(index), selectedRow(request.rows, index, rowCount_));
  result.rowCount_ = rowCount_;
  return result.filtered(rows);
}

Page convertUnits(const Page &page, FieldKind kind, std::string_view name,
                  std::optional<std::string> units, long double factor) {
  Layout changed = page.layout();
  if (!std::isfinite(factor)) typeError("unit conversion factor must be finite");
  switch (kind) {
  case FieldKind::Parameter: changed.parameters.at(changed.parameterIndex(name)).units = units; break;
  case FieldKind::Array: changed.arrays.at(changed.arrayIndex(name)).units = units; break;
  case FieldKind::Column: changed.columns.at(changed.columnIndex(name)).units = units; break;
  }
  auto layout = std::make_shared<const Layout>(LayoutBuilder(std::move(changed)).build());
  Page result(layout, LoadMode::None);
  result.maxTransformationElements_ = page.maxTransformationElements_;
  result.number_ = page.number_;
  result.recovered_ = page.recovered_;
  result.rowCount_ = page.rowCount_;
  for (std::size_t index = 0; index < page.parameters_.size(); ++index) {
    if (!page.parameterLoaded(index)) continue;
    Scalar value = page.parameters_[index];
    if (kind == FieldKind::Parameter && page.layout().parameters[index].name == name)
      value = scaledScalar(value, page.layout().parameters[index].type, factor);
    result.setParameter(index, std::move(value));
  }
  for (std::size_t index = 0; index < page.arrays_.size(); ++index) {
    if (!page.arrayLoaded(index)) continue;
    ArrayData value = page.arrays_[index];
    if (kind == FieldKind::Array && page.layout().arrays[index].name == name)
      value.values = scaledValues(value.values, page.layout().arrays[index].type, factor);
    result.setArray(index, std::move(value));
  }
  for (std::size_t index = 0; index < page.columns_.size(); ++index) {
    if (!page.columnLoaded(index)) continue;
    Values value = page.columns_[index];
    if (kind == FieldKind::Column && page.layout().columns[index].name == name)
      value = scaledValues(value, page.layout().columns[index].type, factor);
    result.setColumn(index, std::move(value));
  }
  result.rowCount_ = page.rowCount_;
  return result;
}

void copyDataset(Reader &reader, const std::filesystem::path &output, CopyOptions options) {
  Layout outputLayout = options.transformLayout ? options.transformLayout(reader.layout())
                                                : reader.layout();
  auto shared = std::make_shared<const Layout>(outputLayout);
  Writer writer = Writer::create(output, outputLayout, options.writer);
  while (auto page = reader.next(options.read)) {
    if (options.transformPage) {
      auto transformed = options.transformPage(std::move(*page), shared);
      if (transformed) writer.write(std::move(*transformed));
    } else {
      writer.write(std::move(*page));
    }
  }
  writer.close();
}

}  // namespace sdds
