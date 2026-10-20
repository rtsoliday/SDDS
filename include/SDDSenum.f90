! Native SDDS6 enum interfaces for ISO_C_BINDING consumers.
! Copyright (c) 2026 The University of Chicago.
! Distributed under the Software License Agreement in LICENSE.
! Dataset pointers refer to C-owned SDDS_DATASET handles. Strings are NUL-terminated.
module SDDS_enum_interfaces
  use, intrinsic :: iso_c_binding
  implicit none
  integer(c_int32_t), parameter :: SDDS_ENUM = 12
  integer(c_int32_t), parameter :: SDDS_NUM_TYPES = 12
  integer(c_int32_t), parameter :: SDDS_ANY_NUMERIC_TYPE = 65537
  integer(c_int32_t), parameter :: SDDS_ANY_FLOATING_TYPE = 65538
  integer(c_int32_t), parameter :: SDDS_ANY_INTEGER_TYPE = 65539
  interface
    function SDDS_CreateEnumDefinition(mapping) bind(C, name="SDDS_CreateEnumDefinition") result(dictionary)
      import :: c_ptr, c_char
      character(kind=c_char), intent(in) :: mapping(*)
      type(c_ptr) :: dictionary
    end function
    subroutine SDDS_FreeEnumDefinition(dictionary) bind(C, name="SDDS_FreeEnumDefinition")
      import :: c_ptr
      type(c_ptr), value :: dictionary
    end subroutine
    function SDDS_EnumCodeToLabel(dictionary, code) bind(C, name="SDDS_EnumCodeToLabel") result(label)
      import :: c_ptr, c_int32_t
      type(c_ptr), value :: dictionary
      integer(c_int32_t), value :: code
      type(c_ptr) :: label
    end function
    function SDDS_EnumLabelToCode(dictionary, label, code) bind(C, name="SDDS_EnumLabelToCode") result(ok)
      import :: c_ptr, c_char, c_int32_t
      type(c_ptr), value :: dictionary
      character(kind=c_char), intent(in) :: label(*)
      integer(c_int32_t), intent(out) :: code
      integer(c_int32_t) :: ok
    end function
    function SDDS_EnumDefinitionsEqual(a, b) bind(C, name="SDDS_EnumDefinitionsEqual") result(equal)
      import :: c_ptr, c_int32_t
      type(c_ptr), value :: a, b
      integer(c_int32_t) :: equal
    end function
    function SDDS_EnumValues(dictionary) bind(C, name="SDDS_EnumValues") result(mapping)
      import :: c_ptr
      type(c_ptr), value :: dictionary
      type(c_ptr) :: mapping
    end function
    function SDDS_RemapEnumValues(source, target, input, output, count) bind(C, name="SDDS_RemapEnumValues") result(ok)
      import :: c_ptr, c_int32_t, c_int64_t
      type(c_ptr), value :: source, target
      integer(c_int32_t), intent(in) :: input(*)
      integer(c_int32_t), intent(out) :: output(*)
      integer(c_int64_t), value :: count
      integer(c_int32_t) :: ok
    end function
    function SDDS_DefineEnumColumn(dataset, name, symbol, units, description, format, mapping, fieldLength) &
        bind(C, name="SDDS_DefineEnumColumn") result(index)
      import :: c_ptr, c_char, c_int32_t
      type(c_ptr), value :: dataset, symbol, units, description, format
      character(kind=c_char), intent(in) :: name(*), mapping(*)
      integer(c_int32_t), value :: fieldLength
      integer(c_int32_t) :: index
    end function
    function SDDS_DefineEnumParameter(dataset, name, symbol, units, description, format, mapping, fixedValue) &
        bind(C, name="SDDS_DefineEnumParameter") result(index)
      import :: c_ptr, c_char, c_int32_t
      type(c_ptr), value :: dataset, symbol, units, description, format, fixedValue
      character(kind=c_char), intent(in) :: name(*), mapping(*)
      integer(c_int32_t) :: index
    end function
    function SDDS_DefineEnumArray(dataset, name, symbol, units, description, format, mapping, fieldLength, dimensions, group) &
        bind(C, name="SDDS_DefineEnumArray") result(index)
      import :: c_ptr, c_char, c_int32_t
      type(c_ptr), value :: dataset, symbol, units, description, format, group
      character(kind=c_char), intent(in) :: name(*), mapping(*)
      integer(c_int32_t), value :: fieldLength, dimensions
      integer(c_int32_t) :: index
    end function
    function SDDS_SetEnumReaderLimits(commandBytes, mappingBytes) bind(C, name="SDDS_SetEnumReaderLimits") result(ok)
      import :: c_int32_t, c_int64_t
      integer(c_int64_t), value :: commandBytes, mappingBytes
      integer(c_int32_t) :: ok
    end function
  end interface
end module SDDS_enum_interfaces
