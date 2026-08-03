/**
 * @file SDDSLayout.cc
 * @brief Core type, error-context, and immutable layout lookup operations.
 *
 * @copyright Copyright (c) 2026 The University of Chicago
 * @license Distributed under the Software License Agreement in LICENSE.
 */

#include "SDDS.hpp"

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
  return static_cast<Type>(value.index() + 1);
}

Type typeOf(const Values &values) noexcept {
  return static_cast<Type>(values.index() + 1);
}

std::string_view typeName(Type type) noexcept {
  constexpr std::array<std::string_view, 11> names = {
      "longdouble", "double", "float", "long64", "ulong64", "long", "ulong",
      "short", "ushort", "string", "character"};
  const auto index = static_cast<std::int32_t>(type) - 1;
  return index >= 0 && index < static_cast<std::int32_t>(names.size()) ? names[index] : "unknown";
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
