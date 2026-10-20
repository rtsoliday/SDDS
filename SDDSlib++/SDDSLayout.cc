/**
 * @file SDDSLayout.cc
 * @brief Core type, error-context, and immutable layout lookup operations.
 *
 * @details Defines type identification, protocol type names, contextual
 * exceptions, and name-to-index lookup for immutable layouts.
 *
 * @copyright
 *   - (c) 2026 The University of Chicago
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 */

#include "SDDS.hpp"
#include "SDDSenum.h"

#include <array>

namespace sdds {
namespace {

[[noreturn]] void unknownField(const char *kind, std::string_view name) {
  const std::string field(name);
  throw TypeError(ErrorKind::Type, std::string("unknown ") + kind + ": " + field,
                  {}, 0, field);
}

}  // namespace

Type typeOf(const Scalar &value) noexcept {
  return std::visit([](const auto &item) { return detail::typeFor<std::decay_t<decltype(item)>>; }, value);
}

Type typeOf(const Values &values) noexcept {
  return std::visit([](const auto &items) { return detail::typeFor<typename std::decay_t<decltype(items)>::value_type>; }, values);
}

std::string_view typeName(Type type) noexcept {
  constexpr std::array<std::string_view, 12> names = {
      "longdouble", "double", "float", "long64", "ulong64", "long", "ulong",
      "short", "ushort", "string", "character", "enum"};
  const auto index = static_cast<std::int32_t>(type) - 1;
  return index >= 0 && index < static_cast<std::int32_t>(names.size()) ? names[index] : "unknown";
}

struct EnumDefinition::Impl {
  std::unique_ptr<SDDS_ENUM_DEFINITION, decltype(&sddsEnumDestroy)> mapping{nullptr, &sddsEnumDestroy};
};

EnumDefinition::EnumDefinition(std::string_view text, std::uint64_t maxBytes) {
  if (text.size() > maxBytes || text.find('\0') != std::string_view::npos)
    throw FormatError(ErrorKind::Format, "invalid or oversized enum definition");
  const std::string input(text);
  const char *error = nullptr;
  auto impl = std::make_shared<Impl>();
  impl->mapping.reset(sddsEnumParse(input.c_str(), static_cast<std::size_t>(maxBytes), &error));
  if (!impl->mapping) throw FormatError(ErrorKind::Format, error);
  impl_ = std::move(impl);
}

std::string_view EnumDefinition::values() const noexcept { return impl_->mapping->text; }
std::string_view EnumDefinition::label(std::int32_t code) const {
  const char *label = sddsEnumLabel(impl_->mapping.get(), code);
  if (!label) throw TypeError(ErrorKind::Type, "unmapped enum code " + std::to_string(code));
  return label;
}
std::int32_t EnumDefinition::code(std::string_view label) const {
  std::int32_t code;
  const std::string text(label);
  if (label.find('\0') != std::string_view::npos || !sddsEnumCode(impl_->mapping.get(), text.c_str(), &code))
    throw TypeError(ErrorKind::Type, "unmapped enum label");
  return code;
}
bool EnumDefinition::operator==(const EnumDefinition &other) const noexcept {
  return sddsEnumEqual(impl_->mapping.get(), other.impl_->mapping.get());
}
EnumValue remapEnum(EnumValue value, const EnumDefinition &source, const EnumDefinition &target) {
  return {target.code(source.label(value.code))};
}

Scalar convertEnumScalar(const Scalar &value, Type target, const EnumDefinition *source,
                         const EnumDefinition *destination) {
  const Type from = typeOf(value);
  if (from == Type::Enum && source) {
    const auto code = std::get<EnumValue>(value).code;
    const auto label = source->label(code);
    if (target == Type::Int32) return code;
    if (target == Type::String) return std::string(label);
    if (target == Type::Enum && destination) return EnumValue{destination->code(label)};
  } else if (target == Type::Enum && destination) {
    if (from == Type::String) return EnumValue{destination->code(std::get<std::string>(value))};
    if (from == Type::Int32) {
      const auto code = std::get<std::int32_t>(value);
      destination->label(code);
      return EnumValue{code};
    }
  }
  throw TypeError(ErrorKind::Type, "enum conversion requires dictionaries and enum, Int32 or String types");
}

Values convertEnumValues(const Values &values, Type target, const EnumDefinition *source,
                         const EnumDefinition *destination) {
  if (target != Type::Enum && target != Type::Int32 && target != Type::String)
    throw TypeError(ErrorKind::Type, "enum conversion target must be Enum, Int32 or String");
  const Type from = typeOf(values);
  if ((from == Type::Enum && (!source || (target == Type::Enum && !destination))) ||
      (from != Type::Enum && (target != Type::Enum || !destination ||
                             (from != Type::Int32 && from != Type::String))))
    throw TypeError(ErrorKind::Type, "enum conversion requires dictionaries and enum, Int32 or String types");
  Values result = target == Type::Enum ? Values{std::vector<EnumValue>{}} :
      target == Type::Int32 ? Values{std::vector<std::int32_t>{}} : Values{std::vector<std::string>{}};
  std::visit([&](const auto &input) {
    for (const auto &item : input) {
      const Scalar converted = convertEnumScalar(Scalar{item}, target, source, destination);
      if (target == Type::Enum) std::get<std::vector<EnumValue>>(result).push_back(std::get<EnumValue>(converted));
      else if (target == Type::Int32) std::get<std::vector<std::int32_t>>(result).push_back(std::get<std::int32_t>(converted));
      else std::get<std::vector<std::string>>(result).push_back(std::get<std::string>(converted));
    }
  }, values);
  return result;
}

Error::Error(ErrorKind kind, std::string message, std::filesystem::path path,
             std::int64_t page, std::optional<std::string> field,
             std::optional<std::uint64_t> offset, std::optional<std::int64_t> row)
    : std::runtime_error(std::move(message)), kind_(kind), path_(std::move(path)), page_(page),
      field_(std::move(field)), offset_(offset), row_(row) {}

std::size_t Layout::parameterIndex(std::string_view name) const {
  for (std::size_t index = 0; index < parameters.size(); ++index)
    if (parameters[index].name == name) return index;
  unknownField("parameter", name);
}

std::size_t Layout::arrayIndex(std::string_view name) const {
  for (std::size_t index = 0; index < arrays.size(); ++index)
    if (arrays[index].name == name) return index;
  unknownField("array", name);
}

std::size_t Layout::columnIndex(std::string_view name) const {
  for (std::size_t index = 0; index < columns.size(); ++index)
    if (columns[index].name == name) return index;
  unknownField("column", name);
}

std::size_t Layout::associateIndex(std::string_view name) const {
  for (std::size_t index = 0; index < associates.size(); ++index)
    if (associates[index].name == name) return index;
  unknownField("associate", name);
}

}  // namespace sdds
