/**
 * @file SDDSModern.cc
 * @brief Standalone C++17 implementation of the serial SDDS protocol.
 *
 * @details Implements header parsing and serialization, ASCII and binary page
 * codecs, projection pushdown, compression, page indexing, append, update,
 * locking, reconnect, and live-file reading.
 *
 * @copyright
 *   - (c) 2026 The University of Chicago
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 */

#include "SDDS.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cfloat>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>
#include <unordered_set>

#include <lzma.h>
#include <zlib.h>

#if defined(_WIN32)
#  if !defined(NOMINMAX)
#    define NOMINMAX
#  endif
#  include <fcntl.h>
#  include <io.h>
#  include <sys/stat.h>
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <sys/file.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace sdds {
namespace {

constexpr std::int32_t kInt64RowCount = INT32_MIN;

[[noreturn]] void throwIo(const std::string &message, const std::filesystem::path &path = {}) {
  throw IoError(ErrorKind::Io, message, path);
}

[[noreturn]] void throwFormat(const std::string &message, const std::filesystem::path &path = {},
                              std::int64_t page = 0,
                              std::optional<std::string> field = std::nullopt,
                              std::optional<std::uint64_t> offset = std::nullopt,
                              std::optional<std::int64_t> row = std::nullopt) {
  throw FormatError(ErrorKind::Format, message, path, page, std::move(field), offset, row);
}

[[noreturn]] void throwType(const std::string &message,
                            std::optional<std::string> field = std::nullopt) {
  throw TypeError(ErrorKind::Type, message, {}, 0, std::move(field));
}

[[noreturn]] void throwState(const std::string &message) {
  throw StateError(ErrorKind::State, message);
}

[[noreturn]] void throwLimit(const std::string &message, const std::filesystem::path &path = {},
                             std::int64_t page = 0,
                             std::optional<std::string> field = std::nullopt,
                             std::optional<std::int64_t> row = std::nullopt) {
  throw LimitError(ErrorKind::Limit, message, path, page, std::move(field), std::nullopt, row);
}

[[noreturn]] void throwWithContext(const Error &error,
                                   const std::filesystem::path &path,
                                   std::int64_t page,
                                   std::optional<std::uint64_t> offset = std::nullopt,
                                   std::optional<std::int64_t> row = std::nullopt,
                                   std::optional<std::string> field = std::nullopt) {
  const std::filesystem::path effectivePath = error.path().empty() ? path : error.path();
  const std::int64_t effectivePage = error.page() ? error.page() : page;
  const auto effectiveOffset = error.offset() ? error.offset() : offset;
  const auto effectiveRow = error.row() ? error.row() : row;
  const auto effectiveField = error.field() ? error.field() : field;
  switch (error.kind()) {
  case ErrorKind::Io:
    throw IoError(ErrorKind::Io, error.what(), effectivePath, effectivePage, effectiveField,
                  effectiveOffset, effectiveRow);
  case ErrorKind::Format:
    throw FormatError(ErrorKind::Format, error.what(), effectivePath, effectivePage,
                      effectiveField, effectiveOffset, effectiveRow);
  case ErrorKind::Type:
    throw TypeError(ErrorKind::Type, error.what(), effectivePath, effectivePage, effectiveField,
                    effectiveOffset, effectiveRow);
  case ErrorKind::State:
    throw StateError(ErrorKind::State, error.what(), effectivePath, effectivePage, effectiveField,
                     effectiveOffset, effectiveRow);
  case ErrorKind::Limit:
    throw LimitError(ErrorKind::Limit, error.what(), effectivePath, effectivePage, effectiveField,
                     effectiveOffset, effectiveRow);
  }
  throw Error(error);
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

bool parseBool(const std::string &value) {
  const std::string text = lower(trim(value));
  if (text == "1" || text == "true" || text == "yes")
    return true;
  if (text == "0" || text == "false" || text == "no")
    return false;
  throwFormat("invalid boolean value: " + value);
}

template <class T>
T parseInteger(const std::string &text, const char *what) {
  T result{};
  const std::string value = trim(text);
  const char *begin = value.data();
  const char *end = begin + value.size();
  const auto converted = std::from_chars(begin, end, result, 10);
  if (converted.ec != std::errc() || converted.ptr != end)
    throwFormat(std::string("invalid ") + what + ": " + text);
  return result;
}

std::string unescape(std::string_view input) {
  std::string result;
  result.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    if (input[i] != '\\' || i + 1 >= input.size()) {
      result.push_back(input[i]);
      continue;
    }
    const char next = input[++i];
    if (next >= '0' && next <= '7') {
      unsigned value = static_cast<unsigned>(next - '0');
      unsigned digits = 1;
      while (digits < 3 && i + 1 < input.size() && input[i + 1] >= '0' &&
             input[i + 1] <= '7') {
        value = value * 8U + static_cast<unsigned>(input[++i] - '0');
        ++digits;
      }
      result.push_back(static_cast<char>(value));
    } else if (next == 'n') {
      result.push_back('\n');
    } else if (next == 'r') {
      result.push_back('\r');
    } else if (next == 't') {
      result.push_back('\t');
    } else {
      result.push_back(next);
    }
  }
  return result;
}

std::string quote(const std::optional<std::string> &value) {
  if (!value)
    return {};
  std::string result = "\"";
  for (unsigned char c : *value) {
    if (c == '\\' || c == '"' || c == '!') {
      result.push_back('\\');
      result.push_back(static_cast<char>(c));
    } else if (!std::isprint(c)) {
      char buffer[5];
      std::snprintf(buffer, sizeof(buffer), "\\%03o", c);
      result += buffer;
    } else {
      result.push_back(static_cast<char>(c));
    }
  }
  result.push_back('"');
  return result;
}

std::string quoteRequired(const std::string &value) {
  return quote(std::optional<std::string>(value));
}

Type parseType(const std::string &name) {
  const std::string value = lower(trim(name));
  if (value == "longdouble") return Type::LongDouble;
  if (value == "double") return Type::Double;
  if (value == "float") return Type::Float;
  if (value == "long64") return Type::Int64;
  if (value == "ulong64") return Type::UInt64;
  if (value == "long") return Type::Int32;
  if (value == "ulong") return Type::UInt32;
  if (value == "short") return Type::Int16;
  if (value == "ushort") return Type::UInt16;
  if (value == "string") return Type::String;
  if (value == "character") return Type::Character;
  throwFormat("unknown SDDS type: " + name);
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
  throwType("unknown SDDS type");
}

Scalar defaultScalar(Type type) {
  switch (type) {
  case Type::LongDouble: return static_cast<long double>(0);
  case Type::Double: return 0.0;
  case Type::Float: return 0.0F;
  case Type::Int64: return static_cast<std::int64_t>(0);
  case Type::UInt64: return static_cast<std::uint64_t>(0);
  case Type::Int32: return static_cast<std::int32_t>(0);
  case Type::UInt32: return static_cast<std::uint32_t>(0);
  case Type::Int16: return static_cast<std::int16_t>(0);
  case Type::UInt16: return static_cast<std::uint16_t>(0);
  case Type::String: return std::string{};
  case Type::Character: return static_cast<char>(0);
  }
  throwType("unknown SDDS type");
}

std::size_t valuesSize(const Values &values) {
  return std::visit([](const auto &v) { return v.size(); }, values);
}

class Stream {
 public:
  virtual ~Stream() = default;
  virtual std::size_t read(void *data, std::size_t size) = 0;
  virtual void write(const void *data, std::size_t size) = 0;
  virtual bool eof() const = 0;
  virtual bool seekable() const noexcept { return false; }
  virtual std::optional<std::uint64_t> size() const { return std::nullopt; }
  virtual std::uint64_t tell() const { throwState("stream is not seekable"); }
  virtual void seek(std::uint64_t) { throwState("stream is not seekable"); }
  virtual void truncate(std::uint64_t) { throwState("stream is not seekable"); }
  virtual void flush() = 0;
  virtual void sync() { throwState("stream does not support sync"); }
  virtual void close() = 0;
};

class InputSourceStream final : public Stream {
 public:
  explicit InputSourceStream(std::unique_ptr<InputSource> source) : source_(std::move(source)) {
    if (!source_) throwState("input source cannot be null");
  }
  std::size_t read(void *data, std::size_t size) override { return source_->read(data, size); }
  void write(const void *, std::size_t) override { throwState("stream is not writable"); }
  bool eof() const override { return source_->eof(); }
  bool seekable() const noexcept override { return source_->capabilities().seek; }
  std::uint64_t tell() const override { return source_->tell(); }
  void seek(std::uint64_t offset) override { source_->seek(offset); }
  void flush() override {}
  void close() override { source_->close(); }

 private:
  std::unique_ptr<InputSource> source_;
};

class OutputSinkStream final : public Stream {
 public:
  explicit OutputSinkStream(std::unique_ptr<OutputSink> sink) : sink_(std::move(sink)) {
    if (!sink_) throwState("output sink cannot be null");
  }
  std::size_t read(void *, std::size_t) override { throwState("stream is not readable"); }
  void write(const void *data, std::size_t size) override { sink_->write(data, size); }
  bool eof() const override { return false; }
  bool seekable() const noexcept override { return sink_->capabilities().seek; }
  std::uint64_t tell() const override { return sink_->tell(); }
  void seek(std::uint64_t offset) override { sink_->seek(offset); }
  void truncate(std::uint64_t length) override { sink_->truncate(length); }
  void flush() override { sink_->flush(); }
  void sync() override { sink_->sync(); }
  void close() override { sink_->close(); }

 private:
  std::unique_ptr<OutputSink> sink_;
};

class FileStream final : public Stream {
 public:
  FileStream(FILE *file, std::filesystem::path path, bool owned, bool writable,
             std::size_t bufferBytes = 256U * 1024U)
      : file_(file), path_(std::move(path)), owned_(owned), writable_(writable) {
    if (!file_)
      throwIo("null file stream", path_);
    if (bufferBytes) {
      buffer_.resize(bufferBytes);
      if (setvbuf(file_, reinterpret_cast<char *>(buffer_.data()), _IOFBF, buffer_.size()) != 0) {
        if (owned_) std::fclose(file_);
        file_ = nullptr;
        throwIo("unable to configure file buffer", path_);
      }
    }
  }

  ~FileStream() override {
    try { close(); } catch (...) {}
  }

  std::size_t read(void *data, std::size_t size) override {
    const std::size_t count = std::fread(data, 1, size, file_);
    if (count < size && std::ferror(file_))
      throwIo("read failure: " + std::string(std::strerror(errno)), path_);
    return count;
  }

  void write(const void *data, std::size_t size) override {
    if (!writable_)
      throwState("stream is not writable");
    if (size && std::fwrite(data, 1, size, file_) != size)
      throwIo("write failure: " + std::string(std::strerror(errno)), path_);
  }

  bool eof() const override { return std::feof(file_) != 0; }
  bool seekable() const noexcept override { return owned_; }

  std::optional<std::uint64_t> size() const override {
#if defined(_WIN32)
    struct _stat64 status{};
    if (_fstat64(_fileno(file_), &status) != 0 || status.st_size < 0)
      return std::nullopt;
#else
    struct stat status{};
    if (fstat(fileno(file_), &status) != 0 || status.st_size < 0)
      return std::nullopt;
#endif
    return static_cast<std::uint64_t>(status.st_size);
  }

  std::uint64_t tell() const override {
#if defined(_WIN32)
    const auto offset = _ftelli64(file_);
#else
    const auto offset = ftello(file_);
#endif
    if (offset < 0)
      throwIo("unable to query file position", path_);
    return static_cast<std::uint64_t>(offset);
  }

  void seek(std::uint64_t offset) override {
#if defined(_WIN32)
    const int status = _fseeki64(file_, static_cast<__int64>(offset), SEEK_SET);
#else
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
      throwLimit("file offset exceeds platform limit", path_);
    const int status = fseeko(file_, static_cast<off_t>(offset), SEEK_SET);
#endif
    if (status != 0)
      throwIo("seek failure", path_);
    std::clearerr(file_);
  }

  void truncate(std::uint64_t length) override {
    flush();
#if defined(_WIN32)
    if (_chsize_s(_fileno(file_), length) != 0)
#else
    if (length > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) ||
        ftruncate(fileno(file_), static_cast<off_t>(length)) != 0)
#endif
      throwIo("unable to truncate output file", path_);
  }

  void flush() override {
    if (writable_ && std::fflush(file_) != 0)
      throwIo("flush failure: " + std::string(std::strerror(errno)), path_);
  }

  void sync() override {
    flush();
#if defined(_WIN32)
    if (_commit(_fileno(file_)) != 0)
#else
    if (::fsync(fileno(file_)) != 0)
#endif
      throwIo("fsync failure", path_);
  }

  FILE *file() const noexcept { return file_; }

  void close() override {
    if (!file_)
      return;
    if (owned_) {
      FILE *closing = file_;
      file_ = nullptr;
      if (std::fclose(closing) != 0)
        throwIo("close failure", path_);
    } else if (writable_) {
      flush();
      file_ = nullptr;
    } else {
      file_ = nullptr;
    }
  }

 private:
  FILE *file_ = nullptr;
  std::filesystem::path path_;
  bool owned_ = false;
  bool writable_ = false;
  std::vector<unsigned char> buffer_;
};

class GzipStream final : public Stream {
 public:
  GzipStream(gzFile file, std::filesystem::path path, bool writable,
             std::size_t bufferBytes = 256U * 1024U)
      : file_(file), path_(std::move(path)), writable_(writable) {
    if (!file_)
      throwIo("unable to open gzip stream", path_);
    if (bufferBytes && gzbuffer(file_, static_cast<unsigned>(
          std::min<std::size_t>(bufferBytes, UINT_MAX))) != 0) {
      gzclose(file_);
      file_ = nullptr;
      throwIo("unable to configure gzip buffer", path_);
    }
  }

  ~GzipStream() override {
    try { close(); } catch (...) {}
  }

  std::size_t read(void *data, std::size_t size) override {
    std::size_t total = 0;
    while (total < size) {
      const unsigned amount = static_cast<unsigned>(std::min<std::size_t>(size - total, UINT_MAX));
      const int result = gzread(file_, static_cast<char *>(data) + total, amount);
      if (result < 0) {
        int code = 0;
        const char *message = gzerror(file_, &code);
        throwIo(std::string("gzip read failure: ") + (message ? message : "unknown"), path_);
      }
      if (result == 0)
        break;
      total += static_cast<std::size_t>(result);
    }
    return total;
  }

  void write(const void *data, std::size_t size) override {
    if (!writable_)
      throwState("gzip stream is not writable");
    std::size_t total = 0;
    while (total < size) {
      const unsigned amount = static_cast<unsigned>(std::min<std::size_t>(size - total, UINT_MAX));
      const int result = gzwrite(file_, static_cast<const char *>(data) + total, amount);
      if (result <= 0)
        throwIo("gzip write failure", path_);
      total += static_cast<std::size_t>(result);
    }
  }

  bool eof() const override { return gzeof(file_) != 0; }
  void flush() override {
    if (writable_ && gzflush(file_, Z_SYNC_FLUSH) != Z_OK)
      throwIo("gzip flush failure", path_);
  }
  void close() override {
    if (!file_)
      return;
    gzFile closing = file_;
    file_ = nullptr;
    if (gzclose(closing) != Z_OK)
      throwIo("gzip close failure", path_);
  }

 private:
  gzFile file_ = nullptr;
  std::filesystem::path path_;
  bool writable_ = false;
};

class LzmaStream final : public Stream {
 public:
  LzmaStream(FILE *file, std::filesystem::path path, bool writable,
             std::size_t bufferBytes = 256U * 1024U, std::uint32_t preset = 6,
             LzmaCheck check = LzmaCheck::Crc64, bool alone = false)
      : file_(file), path_(std::move(path)), writable_(writable),
        alone_(alone),
        input_(std::max<std::size_t>(bufferBytes, 4096U)),
        output_(std::max<std::size_t>(bufferBytes, 4096U)) {
    stream_ = LZMA_STREAM_INIT;
    lzma_ret result = LZMA_OK;
    if (!writable_) {
      result = lzma_auto_decoder(&stream_, UINT64_MAX, 0);
    } else if (alone) {
      lzma_options_lzma filters{};
      if (lzma_lzma_preset(&filters, preset)) result = LZMA_OPTIONS_ERROR;
      else result = lzma_alone_encoder(&stream_, &filters);
    } else {
      lzma_check selected = LZMA_CHECK_CRC64;
      switch (check) {
      case LzmaCheck::None: selected = LZMA_CHECK_NONE; break;
      case LzmaCheck::Crc32: selected = LZMA_CHECK_CRC32; break;
      case LzmaCheck::Crc64: selected = LZMA_CHECK_CRC64; break;
      case LzmaCheck::Sha256: selected = LZMA_CHECK_SHA256; break;
      }
      result = lzma_easy_encoder(&stream_, preset, selected);
    }
    if (result != LZMA_OK) {
      std::fclose(file_);
      file_ = nullptr;
      throwIo("unable to initialize LZMA codec", path_);
    }
  }

  ~LzmaStream() override {
    try { close(); } catch (...) {}
  }

  std::size_t read(void *data, std::size_t size) override {
    if (writable_)
      throwState("LZMA stream is not readable");
    stream_.next_out = static_cast<std::uint8_t *>(data);
    stream_.avail_out = size;
    while (stream_.avail_out && !finished_) {
      if (!stream_.avail_in) {
        const std::size_t count = std::fread(input_.data(), 1, input_.size(), file_);
        if (!count && std::ferror(file_))
          throwIo("LZMA backing-file read failure", path_);
        stream_.next_in = input_.data();
        stream_.avail_in = count;
        inputEof_ = count == 0;
      }
      const lzma_ret result = lzma_code(&stream_, inputEof_ ? LZMA_FINISH : LZMA_RUN);
      if (result == LZMA_STREAM_END) {
        finished_ = true;
      } else if (result != LZMA_OK) {
        throwFormat("invalid LZMA/XZ stream", path_);
      } else if (inputEof_ && !stream_.avail_in && stream_.avail_out == size) {
        throwFormat("truncated LZMA/XZ stream", path_);
      }
    }
    return size - stream_.avail_out;
  }

  void write(const void *data, std::size_t size) override {
    if (!writable_)
      throwState("LZMA stream is not writable");
    stream_.next_in = static_cast<const std::uint8_t *>(data);
    stream_.avail_in = size;
    encode(LZMA_RUN);
  }

  bool eof() const override { return finished_; }
  void flush() override {
    if (writable_ && !alone_)
      encode(LZMA_SYNC_FLUSH);
    if (std::fflush(file_) != 0)
      throwIo("LZMA flush failure", path_);
  }
  void close() override {
    if (!file_)
      return;
    if (writable_ && !finished_) {
      encode(LZMA_FINISH);
      finished_ = true;
    }
    lzma_end(&stream_);
    FILE *closing = file_;
    file_ = nullptr;
    if (std::fclose(closing) != 0)
      throwIo("LZMA close failure", path_);
  }

 private:
  void encode(lzma_action action) {
    do {
      stream_.next_out = output_.data();
      stream_.avail_out = output_.size();
      const lzma_ret result = lzma_code(&stream_, action);
      const std::size_t produced = output_.size() - stream_.avail_out;
      if (produced && std::fwrite(output_.data(), 1, produced, file_) != produced)
        throwIo("LZMA backing-file write failure", path_);
      if (result == LZMA_STREAM_END) {
        if (action == LZMA_FINISH)
          finished_ = true;
        break;
      }
      if (result != LZMA_OK)
        throwIo("LZMA encoding failure", path_);
      if (action == LZMA_RUN && stream_.avail_in == 0)
        break;
      if (action == LZMA_SYNC_FLUSH && stream_.avail_out != 0)
        break;
    } while (true);
  }

  FILE *file_ = nullptr;
  std::filesystem::path path_;
  bool writable_ = false;
  bool alone_ = false;
  bool finished_ = false;
  bool inputEof_ = false;
  lzma_stream stream_ = LZMA_STREAM_INIT;
  std::vector<std::uint8_t> input_;
  std::vector<std::uint8_t> output_;
};

class GzipCodecStream final : public Stream {
 public:
  GzipCodecStream(std::unique_ptr<Stream> backing, bool writable, std::size_t bufferBytes,
                  int level = -1)
      : backing_(std::move(backing)), writable_(writable),
        input_(std::max<std::size_t>(bufferBytes, 4096U)),
        output_(std::max<std::size_t>(bufferBytes, 4096U)) {
    if (!backing_) throwState("gzip codec requires a backing stream");
    std::memset(&codec_, 0, sizeof(codec_));
    const int result = writable_
        ? deflateInit2(&codec_, level < 0 ? Z_DEFAULT_COMPRESSION : level,
                       Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY)
        : inflateInit2(&codec_, 15 + 32);
    if (result != Z_OK) throwIo("unable to initialize gzip codec");
  }
  ~GzipCodecStream() override { try { close(); } catch (...) {} }

  std::size_t read(void *data, std::size_t size) override {
    if (writable_) throwState("gzip stream is not readable");
    std::size_t total = 0;
    while (total < size && !finished_) {
      const std::size_t chunk = std::min<std::size_t>(size - total, UINT_MAX);
      codec_.next_out = reinterpret_cast<Bytef *>(static_cast<char *>(data) + total);
      codec_.avail_out = static_cast<uInt>(chunk);
      while (codec_.avail_out && !finished_) {
        if (!codec_.avail_in && !inputEof_) {
          const std::size_t count = backing_->read(input_.data(), input_.size());
          codec_.next_in = input_.data();
          codec_.avail_in = static_cast<uInt>(count);
          inputEof_ = count == 0;
        }
        const uInt before = codec_.avail_out;
        const int result = inflate(&codec_, Z_NO_FLUSH);
        if (result == Z_STREAM_END) {
          finished_ = true;
        } else if (result != Z_OK && result != Z_BUF_ERROR) {
          throwFormat("invalid gzip stream");
        } else if (inputEof_ && !codec_.avail_in && codec_.avail_out == before) {
          throwFormat("truncated gzip stream");
        }
      }
      total += chunk - codec_.avail_out;
      if (codec_.avail_out) break;
    }
    return total;
  }
  void write(const void *data, std::size_t size) override {
    if (!writable_) throwState("gzip stream is not writable");
    const auto *bytes = static_cast<const unsigned char *>(data);
    while (size) {
      const std::size_t chunk = std::min<std::size_t>(size, UINT_MAX);
      codec_.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(bytes));
      codec_.avail_in = static_cast<uInt>(chunk);
      encode(Z_NO_FLUSH);
      bytes += chunk;
      size -= chunk;
    }
  }
  bool eof() const override { return finished_; }
  void flush() override {
    if (writable_) encode(Z_SYNC_FLUSH);
    backing_->flush();
  }
  void close() override {
    if (!backing_) return;
    if (writable_ && !finished_) {
      encode(Z_FINISH);
      finished_ = true;
    }
    writable_ ? deflateEnd(&codec_) : inflateEnd(&codec_);
    backing_->close();
    backing_.reset();
  }

 private:
  void encode(int flushMode) {
    do {
      codec_.next_out = output_.data();
      codec_.avail_out = static_cast<uInt>(output_.size());
      const int result = deflate(&codec_, flushMode);
      if (result != Z_OK && result != Z_STREAM_END &&
          !(flushMode == Z_FINISH && result == Z_BUF_ERROR))
        throwIo("gzip encoding failure");
      const std::size_t produced = output_.size() - codec_.avail_out;
      if (produced) backing_->write(output_.data(), produced);
      if (result == Z_STREAM_END) { finished_ = true; break; }
      if (flushMode == Z_NO_FLUSH && codec_.avail_in == 0) break;
      if (flushMode == Z_SYNC_FLUSH && codec_.avail_out != 0) break;
    } while (true);
  }
  std::unique_ptr<Stream> backing_;
  bool writable_ = false;
  bool finished_ = false;
  bool inputEof_ = false;
  z_stream codec_{};
  std::vector<unsigned char> input_;
  std::vector<unsigned char> output_;
};

class LzmaCodecStream final : public Stream {
 public:
  LzmaCodecStream(std::unique_ptr<Stream> backing, bool writable, std::size_t bufferBytes,
                  std::uint32_t preset = 6, LzmaCheck check = LzmaCheck::Crc64,
                  bool alone = false)
      : backing_(std::move(backing)), writable_(writable),
        alone_(alone),
        input_(std::max<std::size_t>(bufferBytes, 4096U)),
        output_(std::max<std::size_t>(bufferBytes, 4096U)) {
    if (!backing_) throwState("LZMA codec requires a backing stream");
    codec_ = LZMA_STREAM_INIT;
    lzma_ret result = LZMA_OK;
    if (!writable_) {
      result = lzma_auto_decoder(&codec_, UINT64_MAX, 0);
    } else if (alone) {
      lzma_options_lzma options{};
      if (lzma_lzma_preset(&options, preset)) result = LZMA_OPTIONS_ERROR;
      else result = lzma_alone_encoder(&codec_, &options);
    } else {
      lzma_check selected = LZMA_CHECK_CRC64;
      switch (check) {
      case LzmaCheck::None: selected = LZMA_CHECK_NONE; break;
      case LzmaCheck::Crc32: selected = LZMA_CHECK_CRC32; break;
      case LzmaCheck::Crc64: selected = LZMA_CHECK_CRC64; break;
      case LzmaCheck::Sha256: selected = LZMA_CHECK_SHA256; break;
      }
      result = lzma_easy_encoder(&codec_, preset, selected);
    }
    if (result != LZMA_OK) throwIo("unable to initialize LZMA codec");
  }
  ~LzmaCodecStream() override { try { close(); } catch (...) {} }

  std::size_t read(void *data, std::size_t size) override {
    if (writable_) throwState("LZMA stream is not readable");
    codec_.next_out = static_cast<std::uint8_t *>(data);
    codec_.avail_out = size;
    while (codec_.avail_out && !finished_) {
      if (!codec_.avail_in) {
        const std::size_t count = backing_->read(input_.data(), input_.size());
        codec_.next_in = input_.data();
        codec_.avail_in = count;
        inputEof_ = count == 0;
      }
      const std::size_t before = codec_.avail_out;
      const lzma_ret result = lzma_code(&codec_, inputEof_ ? LZMA_FINISH : LZMA_RUN);
      if (result == LZMA_STREAM_END) finished_ = true;
      else if (result != LZMA_OK) throwFormat("invalid LZMA/XZ stream");
      else if (inputEof_ && !codec_.avail_in && codec_.avail_out == before)
        throwFormat("truncated LZMA/XZ stream");
    }
    return size - codec_.avail_out;
  }
  void write(const void *data, std::size_t size) override {
    if (!writable_) throwState("LZMA stream is not writable");
    codec_.next_in = static_cast<const std::uint8_t *>(data);
    codec_.avail_in = size;
    encode(LZMA_RUN);
  }
  bool eof() const override { return finished_; }
  void flush() override {
    if (writable_ && !alone_) encode(LZMA_SYNC_FLUSH);
    backing_->flush();
  }
  void close() override {
    if (!backing_) return;
    if (writable_ && !finished_) { encode(LZMA_FINISH); finished_ = true; }
    lzma_end(&codec_);
    backing_->close();
    backing_.reset();
  }

 private:
  void encode(lzma_action action) {
    do {
      codec_.next_out = output_.data();
      codec_.avail_out = output_.size();
      const lzma_ret result = lzma_code(&codec_, action);
      const std::size_t produced = output_.size() - codec_.avail_out;
      if (produced) backing_->write(output_.data(), produced);
      if (result == LZMA_STREAM_END) { if (action == LZMA_FINISH) finished_ = true; break; }
      if (result != LZMA_OK) throwIo("LZMA encoding failure");
      if (action == LZMA_RUN && codec_.avail_in == 0) break;
      if (action == LZMA_SYNC_FLUSH && codec_.avail_out != 0) break;
    } while (true);
  }
  std::unique_ptr<Stream> backing_;
  bool writable_ = false;
  bool alone_ = false;
  bool finished_ = false;
  bool inputEof_ = false;
  lzma_stream codec_ = LZMA_STREAM_INIT;
  std::vector<std::uint8_t> input_;
  std::vector<std::uint8_t> output_;
};

std::unique_ptr<Stream> wrapInputCodec(std::unique_ptr<Stream> stream,
                                       const ReaderOptions &options) {
  switch (options.compression) {
  case Compression::Auto:
  case Compression::None: return stream;
  case Compression::Gzip:
    return std::make_unique<GzipCodecStream>(std::move(stream), false, options.bufferBytes);
  case Compression::Xz:
  case Compression::Lzma:
    return std::make_unique<LzmaCodecStream>(std::move(stream), false, options.bufferBytes);
  }
  throwState("unknown compression mode");
}

std::unique_ptr<Stream> wrapOutputCodec(std::unique_ptr<Stream> stream,
                                        const WriterOptions &options) {
  switch (options.compression) {
  case Compression::Auto:
  case Compression::None: return stream;
  case Compression::Gzip:
    return std::make_unique<GzipCodecStream>(std::move(stream), true, options.bufferBytes,
                                              options.gzipLevel);
  case Compression::Xz:
    return std::make_unique<LzmaCodecStream>(std::move(stream), true, options.bufferBytes,
                                              options.lzmaPreset, options.lzmaCheck, false);
  case Compression::Lzma:
    return std::make_unique<LzmaCodecStream>(std::move(stream), true, options.bufferBytes,
                                              options.lzmaPreset, options.lzmaCheck, true);
  }
  throwState("unknown compression mode");
}

Compression compressionFor(const std::filesystem::path &path, Compression requested) {
  if (requested != Compression::Auto)
    return requested;
  const std::string extension = lower(path.extension().string());
  if (extension == ".gz") return Compression::Gzip;
  if (extension == ".xz") return Compression::Xz;
  if (extension == ".lzma") return Compression::Lzma;
  return Compression::None;
}

struct FileIdentity {
  std::uint64_t device = 0;
  std::uint64_t inode = 0;
  bool valid = false;
};

FileIdentity fileIdentity(const std::filesystem::path &path) {
  if (path.empty()) return {};
#if defined(_WIN32)
  const HANDLE handle = CreateFileW(path.wstring().c_str(), FILE_READ_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return {};
  BY_HANDLE_FILE_INFORMATION information{};
  const bool found = GetFileInformationByHandle(handle, &information) != 0;
  CloseHandle(handle);
  if (!found) return {};
  return {static_cast<std::uint64_t>(information.dwVolumeSerialNumber),
          (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32U) |
              information.nFileIndexLow,
          true};
#else
  struct stat status{};
  if (stat(path.c_str(), &status) != 0) return {};
  return {static_cast<std::uint64_t>(status.st_dev),
          static_cast<std::uint64_t>(status.st_ino), true};
#endif
}

bool sameFile(const FileIdentity &left, const FileIdentity &right) {
  return left.valid && right.valid && left.device == right.device && left.inode == right.inode;
}

void replaceFile(const std::filesystem::path &source,
                 const std::filesystem::path &destination) {
#if defined(_WIN32)
  if (!MoveFileExW(source.wstring().c_str(), destination.wstring().c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throwIo("unable to replace output after last-page update", destination);
#else
  std::error_code error;
  std::filesystem::rename(source, destination, error);
  if (error)
    throwIo("unable to replace output after last-page update: " + error.message(),
            destination);
#endif
}

class PathLock {
 public:
  PathLock(const std::filesystem::path &path, LockMode mode, bool create) : path_(path) {
    if (mode == LockMode::None || path.empty()) return;
#if defined(_WIN32)
    const DWORD access = mode == LockMode::Exclusive ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ;
    const DWORD disposition = create ? OPEN_ALWAYS : OPEN_EXISTING;
    handle_ = CreateFileW(path.wstring().c_str(), access,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE)
      throwIo("unable to open file for advisory locking", path);
    OVERLAPPED overlapped{};
    overlapped.Offset = MAXDWORD;
    overlapped.OffsetHigh = 0x7FFFFFFFU;
    const DWORD flags = LOCKFILE_FAIL_IMMEDIATELY |
                        (mode == LockMode::Exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0U);
    if (!LockFileEx(handle_, flags, 0, 1, 0, &overlapped)) {
      CloseHandle(handle_);
      handle_ = INVALID_HANDLE_VALUE;
      throwIo("unable to acquire advisory file lock", path);
    }
#else
    const int flags = mode == LockMode::Exclusive ? O_RDWR | (create ? O_CREAT : 0) : O_RDONLY;
    descriptor_ = ::open(path.c_str(), flags, 0666);
    if (descriptor_ < 0) throwIo("unable to open file for advisory locking", path);
    const int operation = (mode == LockMode::Exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB;
    if (flock(descriptor_, operation) != 0) {
      ::close(descriptor_);
      descriptor_ = -1;
      throwIo("unable to acquire advisory file lock", path);
    }
#endif
  }

  PathLock(const PathLock &) = delete;
  PathLock &operator=(const PathLock &) = delete;
  ~PathLock() {
#if defined(_WIN32)
    if (handle_ != INVALID_HANDLE_VALUE) {
      OVERLAPPED overlapped{};
      overlapped.Offset = MAXDWORD;
      overlapped.OffsetHigh = 0x7FFFFFFFU;
      UnlockFileEx(handle_, 0, 1, 0, &overlapped);
      CloseHandle(handle_);
    }
#else
    if (descriptor_ >= 0) {
      flock(descriptor_, LOCK_UN);
      ::close(descriptor_);
    }
#endif
  }

 private:
  std::filesystem::path path_;
#if defined(_WIN32)
  HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
  int descriptor_ = -1;
#endif
};

std::unique_ptr<PathLock> acquirePathLock(const std::filesystem::path &path, LockMode mode,
                                          bool create) {
  return mode == LockMode::None ? nullptr : std::make_unique<PathLock>(path, mode, create);
}

void setBinaryMode(FILE *file) {
#if defined(_WIN32)
  if (_setmode(_fileno(file), _O_BINARY) == -1)
    throwIo("unable to set standard stream to binary mode");
#else
  (void)file;
#endif
}

std::unique_ptr<Stream> openInput(const std::filesystem::path &path, Compression requested,
                                  std::size_t bufferBytes = 256U * 1024U) {
  const Compression compression = compressionFor(path, requested);
  if (compression == Compression::Gzip) {
    return std::make_unique<GzipStream>(gzopen(path.string().c_str(), "rb"), path, false,
                                        bufferBytes);
  }
  FILE *file = std::fopen(path.string().c_str(), "rb");
  if (!file)
    throwIo("unable to open input: " + std::string(std::strerror(errno)), path);
  if (compression == Compression::Xz || compression == Compression::Lzma)
    return std::make_unique<LzmaStream>(file, path, false, bufferBytes);
  return std::make_unique<FileStream>(file, path, true, false, bufferBytes);
}

std::unique_ptr<Stream> openOutput(const std::filesystem::path &path, Compression requested,
                                   const char *mode = "wb",
                                   std::size_t bufferBytes = 256U * 1024U,
                                   int gzipLevel = -1, std::uint32_t lzmaPreset = 6,
                                   LzmaCheck lzmaCheck = LzmaCheck::Crc64) {
  const Compression compression = compressionFor(path, requested);
  if (gzipLevel < -1 || gzipLevel > 9)
    throwState("gzip compression level must be between 0 and 9, or -1 for default");
  if (lzmaPreset > 9)
    throwState("LZMA preset must be between 0 and 9");
  if (compression == Compression::Gzip) {
    std::string gzipMode = "wb";
    if (gzipLevel >= 0) gzipMode.push_back(static_cast<char>('0' + gzipLevel));
    return std::make_unique<GzipStream>(gzopen(path.string().c_str(), gzipMode.c_str()),
                                        path, true, bufferBytes);
  }
  FILE *file = std::fopen(path.string().c_str(), mode);
  if (!file)
    throwIo("unable to open output: " + std::string(std::strerror(errno)), path);
  if (compression == Compression::Xz || compression == Compression::Lzma) {
    if (std::strcmp(mode, "wb") != 0) {
      std::fclose(file);
      throwState("compressed append/update is not supported");
    }
    return std::make_unique<LzmaStream>(file, path, true, bufferBytes, lzmaPreset,
                                        lzmaCheck, compression == Compression::Lzma);
  }
  return std::make_unique<FileStream>(file, path, true, true, bufferBytes);
}

class BufferedStream {
 public:
  explicit BufferedStream(std::unique_ptr<Stream> stream,
                          std::uint64_t maxReadBytes = UINT64_MAX,
                          std::filesystem::path path = {})
      : stream_(std::move(stream)), maxReadBytes_(maxReadBytes), path_(std::move(path)) {}

  std::size_t read(void *data, std::size_t size) {
    std::size_t total = 0;
    if (pushback_ && size) {
      *static_cast<unsigned char *>(data) = *pushback_;
      pushback_.reset();
      total = 1;
      ++offset_;
    }
    while (total < size) {
      if (offset_ >= maxReadBytes_)
        throwLimit("decompressed input exceeds configured limit", path_);
      if (readPosition_ < readSize_) {
        const std::size_t count = std::min(size - total, readSize_ - readPosition_);
        std::memcpy(static_cast<unsigned char *>(data) + total,
                    readBuffer_.data() + readPosition_, count);
        readPosition_ += count;
        offset_ += count;
        total += count;
        continue;
      }
      const std::size_t allowed = static_cast<std::size_t>(std::min<std::uint64_t>(
          size - total, maxReadBytes_ - offset_));
      if (allowed < readBuffer_.size()) {
        readPosition_ = 0;
        readSize_ = stream_->read(readBuffer_.data(), static_cast<std::size_t>(
            std::min<std::uint64_t>(readBuffer_.size(), maxReadBytes_ - offset_)));
        if (!readSize_)
          break;
        continue;
      }
      const std::size_t count = stream_->read(static_cast<unsigned char *>(data) + total,
                                              allowed);
      offset_ += count;
      if (!count)
        break;
      total += count;
    }
    return total;
  }

  void readExact(void *data, std::size_t size, const std::filesystem::path &path,
                 std::int64_t page = 0) {
    if (read(data, size) != size)
      throwFormat("unexpected end of file", path, page, std::nullopt, offset_);
  }

  std::optional<unsigned char> get() {
    unsigned char value = 0;
    if (read(&value, 1) != 1)
      return std::nullopt;
    return value;
  }

  void unget(unsigned char value) {
    if (pushback_)
      throwState("only one byte of pushback is supported");
    pushback_ = value;
    if (offset_)
      --offset_;
  }

  void skip(std::uint64_t size, const std::filesystem::path &path,
            std::int64_t page = 0) {
    if (!size) return;
    constexpr std::uint64_t seekThreshold = 64U * 1024U;
    if (size >= seekThreshold && !pushback_ && stream_->seekable()) {
      if (size > UINT64_MAX - tell()) throwLimit("file offset overflow", path, page);
      const std::uint64_t destination = tell() + size;
      if (destination > maxReadBytes_)
        throwLimit("decompressed input exceeds configured limit", path, page);
      const auto streamSize = stream_->size();
      if (streamSize && destination > *streamSize) {
        seek(*streamSize);
        throwFormat("unexpected end of file", path, page, std::nullopt, offset_);
      }
      if (streamSize) {
        seek(destination);
        return;
      }
    }
    std::array<unsigned char, 65536> discard;
    while (size) {
      const std::size_t amount = static_cast<std::size_t>(
          std::min<std::uint64_t>(size, discard.size()));
      if (read(discard.data(), amount) != amount)
        throwFormat("unexpected end of file", path, page, std::nullopt, offset_);
      size -= amount;
    }
  }

  std::optional<std::string> line(std::uint64_t maxBytes,
                                  const std::filesystem::path &path) {
    std::string result;
    while (true) {
      const auto byte = get();
      if (!byte)
        return result.empty() ? std::nullopt : std::optional<std::string>(std::move(result));
      if (*byte == '\n')
        break;
      if (*byte != '\r')
        result.push_back(static_cast<char>(*byte));
      if (result.size() > maxBytes)
        throwLimit("line exceeds configured limit", path);
    }
    return result;
  }

  void write(const void *data, std::size_t size) {
    if (readPosition_ < readSize_ || pushback_) {
      if (!stream_->seekable())
        throwState("cannot write after buffered input on a sequential stream");
      stream_->seek(offset_);
      readPosition_ = readSize_ = 0;
      pushback_.reset();
    }
    stream_->write(data, size);
    offset_ += size;
  }
  void write(std::string_view text) { write(text.data(), text.size()); }
  bool eof() const { return !pushback_ && readPosition_ == readSize_ && stream_->eof(); }
  bool seekable() const noexcept { return stream_->seekable(); }
  std::uint64_t tell() const noexcept { return offset_; }
  void seek(std::uint64_t offset) {
    if (offset > maxReadBytes_)
      throwLimit("decompressed input exceeds configured limit", path_);
    pushback_.reset();
    readPosition_ = readSize_ = 0;
    stream_->seek(offset);
    offset_ = offset;
  }
  void flush() { stream_->flush(); }
  void truncate(std::uint64_t length) { stream_->truncate(length); }
  void close() { stream_->close(); }
  void replace(std::unique_ptr<Stream> stream, std::uint64_t offset = 0) {
    stream_ = std::move(stream);
    pushback_.reset();
    readPosition_ = readSize_ = 0;
    offset_ = 0;
    if (offset) seek(offset);
  }
  Stream &raw() { return *stream_; }
  std::uint64_t offset() const noexcept { return offset_; }

 private:
  std::unique_ptr<Stream> stream_;
  std::optional<unsigned char> pushback_;
  std::array<unsigned char, 64U * 1024U> readBuffer_;
  std::size_t readPosition_ = 0;
  std::size_t readSize_ = 0;
  std::uint64_t offset_ = 0;
  std::uint64_t maxReadBytes_ = UINT64_MAX;
  std::filesystem::path path_;
};

using Tags = std::vector<std::pair<std::string, std::string>>;

Tags parseTags(std::string_view text) {
  Tags tags;
  std::size_t position = 0;
  while (position < text.size()) {
    while (position < text.size() &&
           (std::isspace(static_cast<unsigned char>(text[position])) || text[position] == ','))
      ++position;
    if (position >= text.size())
      break;
    const std::size_t keyStart = position;
    while (position < text.size() && text[position] != '=' && text[position] != ',' &&
           !std::isspace(static_cast<unsigned char>(text[position])))
      ++position;
    std::string key(text.substr(keyStart, position - keyStart));
    while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])))
      ++position;
    if (position >= text.size() || text[position] != '=')
      throwFormat("missing '=' after namelist tag " + key);
    ++position;
    while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])))
      ++position;
    std::string value;
    if (position < text.size() && text[position] == '"') {
      ++position;
      bool escaped = false;
      while (position < text.size()) {
        const char c = text[position++];
        if (c == '"' && !escaped)
          break;
        value.push_back(c);
        if (c == '\\' && !escaped)
          escaped = true;
        else
          escaped = false;
      }
      value = unescape(value);
    } else {
      const std::size_t valueStart = position;
      while (position < text.size() && text[position] != ',' &&
             !std::isspace(static_cast<unsigned char>(text[position])))
        ++position;
      value = unescape(text.substr(valueStart, position - valueStart));
    }
    tags.emplace_back(lower(std::move(key)), std::move(value));
  }
  return tags;
}

std::optional<std::string> tag(const Tags &tags, std::string_view name) {
  for (const auto &entry : tags)
    if (entry.first == name)
      return entry.second;
  return std::nullopt;
}

std::string requiredTag(const Tags &tags, std::string_view name) {
  auto value = tag(tags, name);
  if (!value)
    throwFormat("missing required namelist tag " + std::string(name));
  return *value;
}

struct Namelist {
  std::string name;
  Tags tags;
};

std::optional<Namelist> readNamelist(BufferedStream &stream, const ReaderOptions &options,
                                     const std::filesystem::path &path,
                                     std::optional<ByteOrder> &commentOrder,
                                     bool &fixedRowComment) {
  std::string command;
  bool started = false;
  while (true) {
    auto line = stream.line(options.limits.maxLayoutCommandBytes, path);
    if (!line)
      return std::nullopt;
    const std::string stripped = trim(*line);
    if (!started && !stripped.empty() && stripped.front() == '!') {
      const std::string special = lower(stripped);
      if (special.find("big-endian") != std::string::npos)
        commentOrder = ByteOrder::Big;
      if (special.find("little-endian") != std::string::npos)
        commentOrder = ByteOrder::Little;
      if (special.find("fixed-rowcount") != std::string::npos ||
          special.find("fixed-row-count") != std::string::npos)
        fixedRowComment = true;
      continue;
    }
    std::size_t start = 0;
    if (!started) {
      start = line->find('&');
      if (start == std::string::npos)
        continue;
      started = true;
    }
    if (!command.empty())
      command.push_back(' ');
    command.append(line->substr(start));
    if (command.size() > options.limits.maxLayoutCommandBytes)
      throwLimit("layout command exceeds configured limit", path);
    bool quoted = false;
    bool escaped = false;
    for (std::size_t i = 0; i + 3 < command.size(); ++i) {
      const char c = command[i];
      if (c == '"' && !escaped)
        quoted = !quoted;
      if (!quoted && c == '&' && lower(command.substr(i, 4)) == "&end") {
        const std::string body = command.substr(1, i - 1);
        const std::size_t separator = body.find_first_of(" \t\r\n");
        Namelist result;
        result.name = lower(separator == std::string::npos ? body : body.substr(0, separator));
        result.tags = parseTags(separator == std::string::npos ? std::string_view{} :
                               std::string_view(body).substr(separator + 1));
        return result;
      }
      if (c == '\\' && !escaped)
        escaped = true;
      else
        escaped = false;
    }
  }
}

void validateLayout(Layout &layout) {
  std::unordered_set<std::string> names;
  auto validateFields = [&](auto &definitions, const char *kind) {
    names.clear();
    for (auto &definition : definitions) {
      if (definition.name.empty())
        throwFormat(std::string(kind) + " name is empty");
      if (!names.insert(definition.name).second)
        throwFormat(std::string("duplicate ") + kind + " name: " + definition.name);
    }
  };
  validateFields(layout.parameters, "parameter");
  validateFields(layout.arrays, "array");
  validateFields(layout.columns, "column");
  names.clear();
  for (const auto &associate : layout.associates) {
    if (associate.name.empty())
      throwFormat("associate name is empty");
    if (!names.insert(associate.name).second)
      throwFormat("duplicate associate name: " + associate.name);
  }
  for (const auto &array : layout.arrays)
    if (array.dimensions < 1)
      throwFormat("array dimensions must be positive", {}, 0, array.name);
  if (layout.data.linesPerRow < 0 || layout.data.additionalHeaderLines < 0)
    throwFormat("negative data-mode count");
  if (layout.data.rowCountMode == RowCountMode::Fixed && layout.data.fixedRowIncrement < 1)
    throwFormat("fixed row increment must be positive");

  std::int32_t version = 1;
  auto consider = [&](Type type) {
    if (type == Type::UInt16 || type == Type::UInt32) version = std::max(version, 2);
    if (type == Type::LongDouble) version = std::max(version, 4);
    if (type == Type::Int64 || type == Type::UInt64) version = std::max(version, 5);
  };
  for (const auto &definition : layout.parameters) consider(definition.type);
  for (const auto &definition : layout.arrays) consider(definition.type);
  for (const auto &definition : layout.columns) consider(definition.type);
  if (layout.data.mode == DataMode::Binary && layout.data.majorOrder == MajorOrder::Column)
    version = std::max(version, 3);
  layout.version = version;
}

FieldMetadata fieldFrom(const Tags &tags) {
  FieldMetadata field;
  field.name = requiredTag(tags, "name");
  field.type = parseType(requiredTag(tags, "type"));
  field.symbol = tag(tags, "symbol");
  field.units = tag(tags, "units");
  field.description = tag(tags, "description");
  field.format = tag(tags, "format_string");
  return field;
}

void parseLayoutStream(BufferedStream &stream, Layout &layout, const ReaderOptions &options,
                       const std::filesystem::path &path, std::uint32_t depth,
                       std::unordered_set<std::string> &includeStack,
                       std::optional<ByteOrder> &commentOrder, bool &fixedRowComment,
                       bool topLevel) {
  if (depth > options.limits.maxIncludeDepth)
    throwLimit("maximum include depth exceeded", path);
  if (topLevel) {
    auto header = stream.line(options.limits.maxLayoutCommandBytes, path);
    if (!header || header->rfind("SDDS", 0) != 0)
      throwFormat("missing SDDS version header", path);
    const std::int32_t version = parseInteger<std::int32_t>(header->substr(4), "SDDS version");
    if (version < 1 || version > 5)
      throwFormat("unsupported SDDS version " + std::to_string(version), path);
    layout.version = version;
  }

  while (auto command = readNamelist(stream, options, path, commentOrder, fixedRowComment)) {
    if (command->name == "description") {
      layout.description = tag(command->tags, "text");
      layout.contents = tag(command->tags, "contents");
    } else if (command->name == "parameter") {
      ParameterDefinition definition;
      static_cast<FieldMetadata &>(definition) = fieldFrom(command->tags);
      definition.fixedValue = tag(command->tags, "fixed_value");
      layout.parameters.push_back(std::move(definition));
    } else if (command->name == "column") {
      ColumnDefinition definition;
      static_cast<FieldMetadata &>(definition) = fieldFrom(command->tags);
      if (auto value = tag(command->tags, "field_length"))
        definition.fieldLength = parseInteger<std::int32_t>(*value, "field_length");
      layout.columns.push_back(std::move(definition));
    } else if (command->name == "array") {
      ArrayDefinition definition;
      static_cast<FieldMetadata &>(definition) = fieldFrom(command->tags);
      if (auto value = tag(command->tags, "field_length"))
        definition.fieldLength = parseInteger<std::int32_t>(*value, "field_length");
      if (auto value = tag(command->tags, "dimensions"))
        definition.dimensions = parseInteger<std::int32_t>(*value, "array dimensions");
      else
        definition.dimensions = 1;
      definition.groupName = tag(command->tags, "group_name");
      layout.arrays.push_back(std::move(definition));
    } else if (command->name == "associate") {
      AssociateDefinition definition;
      definition.name = requiredTag(command->tags, "name");
      definition.filename = tag(command->tags, "filename");
      definition.path = tag(command->tags, "path");
      definition.description = tag(command->tags, "description");
      definition.contents = tag(command->tags, "contents");
      if (auto value = tag(command->tags, "sdds"))
        definition.isSdds = parseBool(*value);
      layout.associates.push_back(std::move(definition));
    } else if (command->name == "include") {
      const std::filesystem::path includePath(requiredTag(command->tags, "filename"));
      const std::string key = std::filesystem::absolute(includePath).lexically_normal().string();
      if (!includeStack.insert(key).second)
        throwFormat("cyclic SDDS include: " + includePath.string(), path);
      auto includeInput = openInput(includePath, Compression::Auto, options.bufferBytes);
      BufferedStream included(std::move(includeInput), options.limits.maxDecompressedBytes,
                              includePath);
      parseLayoutStream(included, layout, options, includePath, depth + 1, includeStack,
                        commentOrder, fixedRowComment, false);
      includeStack.erase(key);
    } else if (command->name == "data") {
      const std::string mode = lower(requiredTag(command->tags, "mode"));
      if (mode == "binary") layout.data.mode = DataMode::Binary;
      else if (mode == "ascii") layout.data.mode = DataMode::Ascii;
      else throwFormat("invalid SDDS data mode: " + mode, path);
      if (auto value = tag(command->tags, "lines_per_row"))
        layout.data.linesPerRow = parseInteger<std::int32_t>(*value, "lines_per_row");
      if (auto value = tag(command->tags, "additional_header_lines"))
        layout.data.additionalHeaderLines = parseInteger<std::int32_t>(*value, "additional_header_lines");
      if (auto value = tag(command->tags, "column_major_order"))
        layout.data.majorOrder = parseBool(*value) ? MajorOrder::Column : MajorOrder::Row;
      if (auto value = tag(command->tags, "endian")) {
        const std::string order = lower(*value);
        if (order == "big") layout.data.byteOrder = ByteOrder::Big;
        else if (order == "little") layout.data.byteOrder = ByteOrder::Little;
        else throwFormat("invalid endian value: " + *value, path);
      }
      if (auto value = tag(command->tags, "no_row_counts"))
        layout.data.rowCountMode = parseBool(*value) ? RowCountMode::None : RowCountMode::Variable;
      if (auto value = tag(command->tags, "fixed_row_count"))
        if (parseBool(*value)) layout.data.rowCountMode = RowCountMode::Fixed;
      if (fixedRowComment)
        layout.data.rowCountMode = RowCountMode::Fixed;
      if (commentOrder)
        layout.data.byteOrder = *commentOrder;
      for (std::int32_t i = 0; i < layout.data.additionalHeaderLines; ++i)
        if (!stream.line(options.limits.maxLayoutCommandBytes, path))
          throwFormat("unexpected EOF in additional header lines", path);
      return;
    } else {
      throwFormat("unknown SDDS layout command: " + command->name, path);
    }
  }
  if (topLevel)
    throwFormat("missing SDDS data command", path);
}

}  // namespace

LayoutBuilder::LayoutBuilder(Layout layout) : layout_(std::move(layout)) {}

LayoutBuilder &LayoutBuilder::setDescription(std::optional<std::string> text,
                                             std::optional<std::string> contents) {
  layout_.description = std::move(text);
  layout_.contents = std::move(contents);
  return *this;
}

LayoutBuilder &LayoutBuilder::setDataOptions(DataOptions options) {
  layout_.data = options;
  return *this;
}

LayoutBuilder &LayoutBuilder::addParameter(ParameterDefinition definition) {
  layout_.parameters.push_back(std::move(definition));
  return *this;
}

LayoutBuilder &LayoutBuilder::addArray(ArrayDefinition definition) {
  layout_.arrays.push_back(std::move(definition));
  return *this;
}

LayoutBuilder &LayoutBuilder::addColumn(ColumnDefinition definition) {
  layout_.columns.push_back(std::move(definition));
  return *this;
}

LayoutBuilder &LayoutBuilder::addAssociate(AssociateDefinition definition) {
  layout_.associates.push_back(std::move(definition));
  return *this;
}

Layout LayoutBuilder::build() const {
  Layout result = layout_;
  validateLayout(result);
  return result;
}

Page::Page(std::shared_ptr<const Layout> layout, LoadMode load) : layout_(std::move(layout)) {
  if (!layout_)
    throwState("page requires a layout");
  parameters_.reserve(layout_->parameters.size());
  for (const auto &definition : layout_->parameters)
    parameters_.push_back(defaultScalar(definition.type));
  arrays_.reserve(layout_->arrays.size());
  for (const auto &definition : layout_->arrays)
    arrays_.push_back({std::vector<std::int32_t>(static_cast<std::size_t>(definition.dimensions), 0),
                       emptyValues(definition.type)});
  columns_.reserve(layout_->columns.size());
  for (const auto &definition : layout_->columns)
    columns_.push_back(emptyValues(definition.type));
  const bool loaded = load == LoadMode::All;
  parametersLoaded_.assign(parameters_.size(), loaded);
  arraysLoaded_.assign(arrays_.size(), loaded);
  columnsLoaded_.assign(columns_.size(), loaded);
}

const Layout &Page::layout() const {
  if (!layout_) throwState("page has no layout");
  return *layout_;
}

bool Page::parameterLoaded(std::size_t index) const { return parametersLoaded_.at(index); }
bool Page::parameterLoaded(std::string_view name) const {
  return parameterLoaded(layout().parameterIndex(name));
}
bool Page::arrayLoaded(std::size_t index) const { return arraysLoaded_.at(index); }
bool Page::arrayLoaded(std::string_view name) const { return arrayLoaded(layout().arrayIndex(name)); }
bool Page::columnLoaded(std::size_t index) const { return columnsLoaded_.at(index); }
bool Page::columnLoaded(std::string_view name) const {
  return columnLoaded(layout().columnIndex(name));
}
bool Page::allFieldsLoaded() const noexcept {
  const auto all = [](const std::vector<bool> &loaded) {
    return std::all_of(loaded.begin(), loaded.end(), [](bool value) { return value; });
  };
  return all(parametersLoaded_) && all(arraysLoaded_) && all(columnsLoaded_);
}

const Scalar &Page::parameter(std::size_t index) const {
  if (!parameterLoaded(index))
    throwState("parameter was not requested: " + layout().parameters.at(index).name);
  return parameters_.at(index);
}
const Scalar &Page::parameter(std::string_view name) const { return parameter(layout().parameterIndex(name)); }
const ArrayData &Page::array(std::size_t index) const {
  if (!arrayLoaded(index))
    throwState("array was not requested: " + layout().arrays.at(index).name);
  return arrays_.at(index);
}
const ArrayData &Page::array(std::string_view name) const { return array(layout().arrayIndex(name)); }
const Values &Page::column(std::size_t index) const {
  if (!columnLoaded(index))
    throwState("column was not requested: " + layout().columns.at(index).name);
  return columns_.at(index);
}
const Values &Page::column(std::string_view name) const { return column(layout().columnIndex(name)); }

void Page::setParameter(std::size_t index, Scalar value) {
  if (typeOf(value) != layout().parameters.at(index).type)
    throwType("parameter type mismatch", layout().parameters.at(index).name);
  parameters_.at(index) = std::move(value);
  parametersLoaded_.at(index) = true;
}
void Page::setParameter(std::string_view name, Scalar value) {
  setParameter(layout().parameterIndex(name), std::move(value));
}
void Page::setArray(std::size_t index, ArrayData value) {
  const auto &definition = layout().arrays.at(index);
  if (typeOf(value.values) != definition.type)
    throwType("array type mismatch", definition.name);
  if (value.dimensions.size() != static_cast<std::size_t>(definition.dimensions))
    throwType("array dimension count mismatch", definition.name);
  std::uint64_t elements = 1;
  for (const auto dimension : value.dimensions) {
    if (dimension < 0)
      throwType("negative array dimension", definition.name);
    if (dimension && elements > UINT64_MAX / static_cast<std::uint64_t>(dimension))
      throwLimit("array dimension product overflow", {}, 0, definition.name);
    elements *= static_cast<std::uint64_t>(dimension);
  }
  if (elements != valuesSize(value.values))
    throwType("array element count does not match dimensions", definition.name);
  arrays_.at(index) = std::move(value);
  arraysLoaded_.at(index) = true;
}
void Page::setArray(std::string_view name, ArrayData value) {
  setArray(layout().arrayIndex(name), std::move(value));
}
void Page::setColumn(std::size_t index, Values value) {
  const auto &definition = layout().columns.at(index);
  if (typeOf(value) != definition.type)
    throwType("column type mismatch", definition.name);
  columns_.at(index) = std::move(value);
  columnsLoaded_.at(index) = true;
  rowCount_ = 0;
  for (std::size_t column = 0; column < columns_.size(); ++column)
    if (columnsLoaded_[column])
      rowCount_ = std::max(rowCount_, static_cast<std::int64_t>(valuesSize(columns_[column])));
}
void Page::setColumn(std::string_view name, Values value) {
  setColumn(layout().columnIndex(name), std::move(value));
}

namespace {

bool nativeBigEndian() noexcept {
  const std::uint16_t value = 0x0102;
  return *reinterpret_cast<const unsigned char *>(&value) == 0x01;
}

ByteOrder resolvedOrder(ByteOrder order) noexcept {
  if (order != ByteOrder::Native)
    return order;
  return nativeBigEndian() ? ByteOrder::Big : ByteOrder::Little;
}

bool mustSwap(ByteOrder order) noexcept {
  return (resolvedOrder(order) == ByteOrder::Big) != nativeBigEndian();
}

template <class T>
T readPod(BufferedStream &stream, ByteOrder order, const std::filesystem::path &path,
          std::int64_t page) {
  T value{};
  stream.readExact(&value, sizeof(value), path, page);
  if (mustSwap(order)) {
    auto *bytes = reinterpret_cast<unsigned char *>(&value);
    std::reverse(bytes, bytes + sizeof(value));
  }
  return value;
}

template <class T>
void writePod(BufferedStream &stream, T value, ByteOrder order) {
  if (mustSwap(order)) {
    auto *bytes = reinterpret_cast<unsigned char *>(&value);
    std::reverse(bytes, bytes + sizeof(value));
  }
  stream.write(&value, sizeof(value));
}

long double decodeExtended80(std::array<unsigned char, 16> bytes, ByteOrder order) {
  if (resolvedOrder(order) == ByteOrder::Big)
    std::reverse(bytes.begin(), bytes.begin() + 12);
  std::uint64_t significand = 0;
  for (int i = 7; i >= 0; --i)
    significand = (significand << 8U) | bytes[static_cast<std::size_t>(i)];
  const std::uint16_t signExponent =
      static_cast<std::uint16_t>(bytes[8]) |
      static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[9]) << 8U);
  const bool negative = (signExponent & 0x8000U) != 0;
  const std::uint16_t exponent = signExponent & 0x7fffU;
  if (exponent == 0 && significand == 0)
    return negative ? -0.0L : 0.0L;
  if (exponent == 0x7fffU) {
    if ((significand & UINT64_C(0x7fffffffffffffff)) == 0)
      return negative ? -std::numeric_limits<long double>::infinity()
                      : std::numeric_limits<long double>::infinity();
    return std::numeric_limits<long double>::quiet_NaN();
  }
  const long double fraction = std::ldexp(static_cast<long double>(significand), -63);
  const long double value = std::ldexp(fraction, static_cast<int>(exponent) - 16383);
  return negative ? -value : value;
}

std::array<unsigned char, 16> encodeExtended80(long double value, ByteOrder order) {
  std::array<unsigned char, 16> bytes{};
  const bool negative = std::signbit(value);
  const long double magnitude = std::fabs(value);
  std::uint16_t exponent = 0;
  std::uint64_t significand = 0;
  if (std::isnan(magnitude)) {
    exponent = 0x7fffU;
    significand = UINT64_C(0xc000000000000000);
  } else if (std::isinf(magnitude)) {
    exponent = 0x7fffU;
    significand = UINT64_C(0x8000000000000000);
  } else if (magnitude != 0) {
    int binaryExponent = 0;
    long double fraction = std::frexp(magnitude, &binaryExponent);
    fraction *= 2;
    --binaryExponent;
    const int biased = binaryExponent + 16383;
    if (biased <= 0) {
      exponent = 0;
      significand = static_cast<std::uint64_t>(
          std::ldexp(magnitude, 63 + 16382));
    } else if (biased >= 0x7fff) {
      exponent = 0x7fffU;
      significand = UINT64_C(0x8000000000000000);
    } else {
      exponent = static_cast<std::uint16_t>(biased);
      const long double scaled = std::ldexp(fraction, 63);
      significand = scaled >= std::ldexp(1.0L, 64)
                        ? UINT64_MAX
                        : static_cast<std::uint64_t>(scaled);
    }
  }
  for (std::size_t i = 0; i < 8; ++i) {
    bytes[i] = static_cast<unsigned char>(significand & 0xffU);
    significand >>= 8U;
  }
  const std::uint16_t signExponent = exponent | (negative ? 0x8000U : 0U);
  bytes[8] = static_cast<unsigned char>(signExponent & 0xffU);
  bytes[9] = static_cast<unsigned char>(signExponent >> 8U);
  if (resolvedOrder(order) == ByteOrder::Big)
    std::reverse(bytes.begin(), bytes.begin() + 12);
  return bytes;
}

Scalar parseAsciiScalar(Type type, const std::string &text) {
  if (type == Type::String)
    return text;
  if (type == Type::Character) {
    if (text.empty()) throwFormat("empty character value");
    return text.front();
  }
  const std::string value = trim(text);
  char *end = nullptr;
  errno = 0;
  switch (type) {
  case Type::LongDouble: {
    const long double result = std::strtold(value.c_str(), &end);
    if (errno == ERANGE || end != value.c_str() + value.size())
      throwFormat("invalid longdouble value: " + text);
    return result;
  }
  case Type::Double: {
    const double result = std::strtod(value.c_str(), &end);
    if (errno == ERANGE || end != value.c_str() + value.size())
      throwFormat("invalid double value: " + text);
    return result;
  }
  case Type::Float: {
    const float result = std::strtof(value.c_str(), &end);
    if (errno == ERANGE || end != value.c_str() + value.size())
      throwFormat("invalid float value: " + text);
    return result;
  }
  case Type::Int64: return parseInteger<std::int64_t>(value, "long64 value");
  case Type::UInt64: return parseInteger<std::uint64_t>(value, "ulong64 value");
  case Type::Int32: return parseInteger<std::int32_t>(value, "long value");
  case Type::UInt32: return parseInteger<std::uint32_t>(value, "ulong value");
  case Type::Int16: return parseInteger<std::int16_t>(value, "short value");
  case Type::UInt16: return parseInteger<std::uint16_t>(value, "ushort value");
  case Type::String:
  case Type::Character: break;
  }
  throwType("unknown ASCII scalar type");
}

std::string formatAsciiScalar(const Scalar &value) {
  return std::visit([](const auto &item) -> std::string {
    using T = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<T, std::string>) {
      return quoteRequired(item);
    } else if constexpr (std::is_same_v<T, char>) {
      return quoteRequired(std::string(1, item));
    } else if constexpr (std::is_floating_point_v<T>) {
      std::ostringstream output;
      output.imbue(std::locale::classic());
      output << std::setprecision(std::numeric_limits<T>::max_digits10) << item;
      return output.str();
    } else if constexpr (std::is_signed_v<T>) {
      return std::to_string(static_cast<long long>(item));
    } else {
      return std::to_string(static_cast<unsigned long long>(item));
    }
  }, value);
}

Scalar readBinaryScalar(BufferedStream &stream, Type type, ByteOrder order,
                        LongDoubleEncoding longDoubleEncoding,
                        const ReaderLimits &limits, const std::filesystem::path &path,
                        std::int64_t page) {
  switch (type) {
  case Type::LongDouble:
    if (longDoubleEncoding == LongDoubleEncoding::LegacyFloat64)
      return static_cast<long double>(readPod<double>(stream, order, path, page));
    else {
      std::array<unsigned char, 16> bytes{};
      stream.readExact(bytes.data(), bytes.size(), path, page);
      return decodeExtended80(bytes, order);
    }
  case Type::Double: return readPod<double>(stream, order, path, page);
  case Type::Float: return readPod<float>(stream, order, path, page);
  case Type::Int64: return readPod<std::int64_t>(stream, order, path, page);
  case Type::UInt64: return readPod<std::uint64_t>(stream, order, path, page);
  case Type::Int32: return readPod<std::int32_t>(stream, order, path, page);
  case Type::UInt32: return readPod<std::uint32_t>(stream, order, path, page);
  case Type::Int16: return readPod<std::int16_t>(stream, order, path, page);
  case Type::UInt16: return readPod<std::uint16_t>(stream, order, path, page);
  case Type::Character: {
    char value = 0;
    stream.readExact(&value, 1, path, page);
    return value;
  }
  case Type::String: {
    const std::int32_t length = readPod<std::int32_t>(stream, order, path, page);
    if (length < 0)
      throwFormat("negative binary string length", path, page, std::nullopt, stream.offset());
    if (static_cast<std::uint64_t>(length) > limits.maxStringBytes)
      throwLimit("binary string exceeds configured limit", path, page);
    std::string value(static_cast<std::size_t>(length), '\0');
    if (length)
      stream.readExact(value.data(), value.size(), path, page);
    return value;
  }
  }
  throwType("unknown binary scalar type");
}

std::uint64_t binaryScalarBytes(Type type, LongDoubleEncoding longDoubleEncoding) {
  switch (type) {
  case Type::LongDouble:
    return longDoubleEncoding == LongDoubleEncoding::LegacyFloat64 ? sizeof(double) : 16U;
  case Type::Double: return sizeof(double);
  case Type::Float: return sizeof(float);
  case Type::Int64: return sizeof(std::int64_t);
  case Type::UInt64: return sizeof(std::uint64_t);
  case Type::Int32: return sizeof(std::int32_t);
  case Type::UInt32: return sizeof(std::uint32_t);
  case Type::Int16: return sizeof(std::int16_t);
  case Type::UInt16: return sizeof(std::uint16_t);
  case Type::Character: return 1U;
  case Type::String: return 0U;
  }
  throwType("unknown binary scalar type");
}

void skipBinaryScalar(BufferedStream &stream, Type type, ByteOrder order,
                      LongDoubleEncoding longDoubleEncoding, const ReaderLimits &limits,
                      const std::filesystem::path &path, std::int64_t page) {
  if (type != Type::String) {
    stream.skip(binaryScalarBytes(type, longDoubleEncoding), path, page);
    return;
  }
  const std::int32_t length = readPod<std::int32_t>(stream, order, path, page);
  if (length < 0)
    throwFormat("negative binary string length", path, page, std::nullopt, stream.offset());
  if (static_cast<std::uint64_t>(length) > limits.maxStringBytes)
    throwLimit("binary string exceeds configured limit", path, page);
  stream.skip(static_cast<std::uint64_t>(length), path, page);
}

void reserveValues(Values &values, std::size_t count) {
  std::visit([&](auto &items) { items.reserve(count); }, values);
}

template <class T>
T fixedValue(const unsigned char *data, ByteOrder order) {
  T value{};
  std::memcpy(&value, data, sizeof(value));
  if (mustSwap(order)) {
    auto *bytes = reinterpret_cast<unsigned char *>(&value);
    std::reverse(bytes, bytes + sizeof(value));
  }
  return value;
}

void appendFixedBinaryValue(Values &values, Type type, const unsigned char *data,
                            ByteOrder order, LongDoubleEncoding longDoubleEncoding) {
  switch (type) {
  case Type::LongDouble:
    if (longDoubleEncoding == LongDoubleEncoding::LegacyFloat64)
      std::get<std::vector<long double>>(values).push_back(fixedValue<double>(data, order));
    else {
      std::array<unsigned char, 16> bytes{};
      std::memcpy(bytes.data(), data, bytes.size());
      std::get<std::vector<long double>>(values).push_back(decodeExtended80(bytes, order));
    }
    return;
  case Type::Double:
    std::get<std::vector<double>>(values).push_back(fixedValue<double>(data, order)); return;
  case Type::Float:
    std::get<std::vector<float>>(values).push_back(fixedValue<float>(data, order)); return;
  case Type::Int64:
    std::get<std::vector<std::int64_t>>(values).push_back(
        fixedValue<std::int64_t>(data, order)); return;
  case Type::UInt64:
    std::get<std::vector<std::uint64_t>>(values).push_back(
        fixedValue<std::uint64_t>(data, order)); return;
  case Type::Int32:
    std::get<std::vector<std::int32_t>>(values).push_back(
        fixedValue<std::int32_t>(data, order)); return;
  case Type::UInt32:
    std::get<std::vector<std::uint32_t>>(values).push_back(
        fixedValue<std::uint32_t>(data, order)); return;
  case Type::Int16:
    std::get<std::vector<std::int16_t>>(values).push_back(
        fixedValue<std::int16_t>(data, order)); return;
  case Type::UInt16:
    std::get<std::vector<std::uint16_t>>(values).push_back(
        fixedValue<std::uint16_t>(data, order)); return;
  case Type::Character:
    std::get<std::vector<char>>(values).push_back(static_cast<char>(*data)); return;
  case Type::String: throwState("string is not a fixed-width binary value");
  }
  throwType("unknown binary scalar type");
}

void writeBinaryScalar(BufferedStream &stream, const Scalar &value, ByteOrder order,
                       LongDoubleEncoding longDoubleEncoding) {
  std::visit([&](const auto &item) {
    using T = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<T, long double>) {
      if (longDoubleEncoding == LongDoubleEncoding::LegacyFloat64)
        writePod(stream, static_cast<double>(item), order);
      else {
        const auto bytes = encodeExtended80(item, order);
        stream.write(bytes.data(), bytes.size());
      }
    } else if constexpr (std::is_same_v<T, std::string>) {
      if (item.size() > static_cast<std::size_t>(INT32_MAX))
        throwLimit("string is too large for SDDS binary encoding");
      writePod(stream, static_cast<std::int32_t>(item.size()), order);
      stream.write(item.data(), item.size());
    } else if constexpr (std::is_same_v<T, char>) {
      stream.write(&item, 1);
    } else {
      writePod(stream, item, order);
    }
  }, value);
}

template <class T>
void storeFixedValue(unsigned char *destination, T value, ByteOrder order) {
  if (mustSwap(order)) {
    auto *bytes = reinterpret_cast<unsigned char *>(&value);
    std::reverse(bytes, bytes + sizeof(value));
  }
  std::memcpy(destination, &value, sizeof(value));
}

void storeFixedBinaryValue(unsigned char *destination, const Values &values,
                           std::size_t index, Type type, ByteOrder order,
                           LongDoubleEncoding longDoubleEncoding) {
  switch (type) {
  case Type::LongDouble: {
    const long double value = std::get<std::vector<long double>>(values).at(index);
    if (longDoubleEncoding == LongDoubleEncoding::LegacyFloat64)
      storeFixedValue(destination, static_cast<double>(value), order);
    else {
      const auto bytes = encodeExtended80(value, order);
      std::memcpy(destination, bytes.data(), bytes.size());
    }
    return;
  }
  case Type::Double:
    storeFixedValue(destination, std::get<std::vector<double>>(values).at(index), order); return;
  case Type::Float:
    storeFixedValue(destination, std::get<std::vector<float>>(values).at(index), order); return;
  case Type::Int64:
    storeFixedValue(destination, std::get<std::vector<std::int64_t>>(values).at(index), order);
    return;
  case Type::UInt64:
    storeFixedValue(destination, std::get<std::vector<std::uint64_t>>(values).at(index), order);
    return;
  case Type::Int32:
    storeFixedValue(destination, std::get<std::vector<std::int32_t>>(values).at(index), order);
    return;
  case Type::UInt32:
    storeFixedValue(destination, std::get<std::vector<std::uint32_t>>(values).at(index), order);
    return;
  case Type::Int16:
    storeFixedValue(destination, std::get<std::vector<std::int16_t>>(values).at(index), order);
    return;
  case Type::UInt16:
    storeFixedValue(destination, std::get<std::vector<std::uint16_t>>(values).at(index), order);
    return;
  case Type::Character:
    *destination = static_cast<unsigned char>(std::get<std::vector<char>>(values).at(index));
    return;
  case Type::String: throwState("string is not a fixed-width binary value");
  }
  throwType("unknown binary scalar type");
}

void appendScalar(Values &values, Scalar value) {
  if (typeOf(values) != typeOf(value))
    throwType("internal scalar/vector type mismatch");
  std::visit([&](auto &vector) {
    using VectorType = std::decay_t<decltype(vector)>;
    using T = typename VectorType::value_type;
    vector.push_back(std::get<T>(std::move(value)));
  }, values);
}

Scalar scalarAt(const Values &values, std::size_t index) {
  return std::visit([&](const auto &vector) -> Scalar { return vector.at(index); }, values);
}

std::vector<std::string> splitAsciiTokens(std::string_view line) {
  std::vector<std::string> tokens;
  std::size_t position = 0;
  while (position < line.size()) {
    while (position < line.size() && std::isspace(static_cast<unsigned char>(line[position])))
      ++position;
    if (position >= line.size()) break;
    if (line[position] == '!') break;
    std::string token;
    if (line[position] == '"') {
      ++position;
      bool escaped = false;
      while (position < line.size()) {
        const char c = line[position++];
        if (c == '"' && !escaped) break;
        token.push_back(c);
        if (c == '\\' && !escaped) escaped = true;
        else escaped = false;
      }
      token = unescape(token);
    } else {
      const std::size_t start = position;
      while (position < line.size() && !std::isspace(static_cast<unsigned char>(line[position])))
        ++position;
      token = unescape(line.substr(start, position - start));
    }
    tokens.push_back(std::move(token));
  }
  return tokens;
}

class AsciiCursor {
 public:
  AsciiCursor(BufferedStream &stream, const ReaderOptions &options,
              std::filesystem::path path, std::optional<std::string> pending = std::nullopt)
      : stream_(stream), options_(options), path_(std::move(path)), pending_(std::move(pending)) {}

  std::optional<std::string> nextLine(bool keepBlank = true) {
    while (true) {
      std::optional<std::string> line;
      if (pending_) {
        line = std::move(pending_);
        pending_.reset();
      } else {
        line = stream_.line(options_.limits.maxLayoutCommandBytes, path_);
      }
      if (!line) return std::nullopt;
      const std::string stripped = trim(*line);
      if (!stripped.empty() && stripped.front() == '!') continue;
      if (!keepBlank && stripped.empty()) continue;
      return line;
    }
  }

  std::vector<std::string> requiredTokens(const std::string &what) {
    auto line = nextLine(false);
    if (!line) throwFormat("unexpected EOF reading " + what, path_);
    auto tokens = splitAsciiTokens(*line);
    if (tokens.empty()) throwFormat("missing " + what, path_);
    return tokens;
  }

 private:
  BufferedStream &stream_;
  const ReaderOptions &options_;
  std::filesystem::path path_;
  std::optional<std::string> pending_;
};

std::uint64_t checkedArrayElements(const std::vector<std::int32_t> &dimensions,
                                   const ReaderLimits &limits,
                                   const std::filesystem::path &path,
                                   std::int64_t page, const std::string &name) {
  std::uint64_t elements = 1;
  for (const auto dimension : dimensions) {
    if (dimension < 0)
      throwFormat("negative array dimension", path, page, name);
    if (dimension && elements > UINT64_MAX / static_cast<std::uint64_t>(dimension))
      throwLimit("array dimension product overflow", path, page, name);
    elements *= static_cast<std::uint64_t>(dimension);
  }
  if (elements > limits.maxElements || elements > static_cast<std::uint64_t>(SIZE_MAX))
    throwLimit("array exceeds configured element limit", path, page, name);
  return elements;
}

std::string fieldTags(const FieldMetadata &field) {
  std::string result = "name=" + quoteRequired(field.name) + ", ";
  if (field.symbol) result += "symbol=" + quote(field.symbol) + ", ";
  if (field.units) result += "units=" + quote(field.units) + ", ";
  if (field.description) result += "description=" + quote(field.description) + ", ";
  if (field.format) result += "format_string=" + quote(field.format) + ", ";
  result += "type=" + std::string(typeName(field.type)) + ", ";
  return result;
}

void writeLayout(BufferedStream &stream, Layout &layout, const WriterOptions &options) {
  validateLayout(layout);
  layout.version = std::max(layout.version, options.minimumVersion);
  if (layout.version < 1 || layout.version > 5)
    throwState("minimum SDDS version must be in the range 1 through 5");
  stream.write("SDDS" + std::to_string(layout.version) + "\n");
  if (layout.data.mode == DataMode::Binary) {
    stream.write(resolvedOrder(layout.data.byteOrder) == ByteOrder::Big
                     ? "!# big-endian\n" : "!# little-endian\n");
  }
  if (layout.data.rowCountMode == RowCountMode::Fixed)
    stream.write("!# fixed-rowcount\n");
  if (layout.description || layout.contents) {
    std::string command = "&description ";
    if (layout.description) command += "text=" + quote(layout.description) + ", ";
    if (layout.contents) command += "contents=" + quote(layout.contents) + ", ";
    stream.write(command + "&end\n");
  }
  for (const auto &definition : layout.parameters) {
    std::string command = "&parameter " + fieldTags(definition);
    if (definition.fixedValue) command += "fixed_value=" + quote(definition.fixedValue) + ", ";
    stream.write(command + "&end\n");
  }
  for (const auto &definition : layout.arrays) {
    std::string command = "&array " + fieldTags(definition);
    if (definition.groupName) command += "group_name=" + quote(definition.groupName) + ", ";
    if (definition.fieldLength) command += "field_length=" + std::to_string(definition.fieldLength) + ", ";
    command += "dimensions=" + std::to_string(definition.dimensions) + ", &end\n";
    stream.write(command);
  }
  for (const auto &definition : layout.columns) {
    std::string command = "&column " + fieldTags(definition);
    if (definition.fieldLength) command += "field_length=" + std::to_string(definition.fieldLength) + ", ";
    stream.write(command + "&end\n");
  }
  for (const auto &definition : layout.associates) {
    std::string command = "&associate name=" + quoteRequired(definition.name) + ", ";
    if (definition.filename) command += "filename=" + quote(definition.filename) + ", ";
    if (definition.path) command += "path=" + quote(definition.path) + ", ";
    if (definition.description) command += "description=" + quote(definition.description) + ", ";
    if (definition.contents) command += "contents=" + quote(definition.contents) + ", ";
    command += std::string("sdds=") + (definition.isSdds ? "1" : "0") + ", &end\n";
    stream.write(command);
  }
  std::string data = "&data mode=";
  data += layout.data.mode == DataMode::Binary ? "binary, " : "ascii, ";
  if (layout.data.mode == DataMode::Ascii && layout.data.linesPerRow != 1)
    data += "lines_per_row=" + std::to_string(layout.data.linesPerRow) + ", ";
  if (layout.data.rowCountMode == RowCountMode::None) data += "no_row_counts=1, ";
  if (layout.data.rowCountMode == RowCountMode::Fixed) data += "fixed_row_count=1, ";
  if (layout.data.mode == DataMode::Binary) {
    data += resolvedOrder(layout.data.byteOrder) == ByteOrder::Big ? "endian=big, " : "endian=little, ";
    if (layout.data.majorOrder == MajorOrder::Column) data += "column_major_order=1, ";
  }
  if (layout.data.additionalHeaderLines)
    data += "additional_header_lines=" + std::to_string(layout.data.additionalHeaderLines) + ", ";
  stream.write(data + "&end\n");
  for (std::int32_t i = 0; i < layout.data.additionalHeaderLines; ++i)
    stream.write("! additional SDDS header line\n");
}

void validatePage(const Page &page, const Layout &layout) {
  if (page.parameters().size() != layout.parameters.size() ||
      page.arrays().size() != layout.arrays.size() || page.columns().size() != layout.columns.size())
    throwType("page does not match writer layout");
  if (!page.allFieldsLoaded())
    throwState("a projected page must be completed before it can be written");
  std::optional<std::size_t> rows;
  for (std::size_t i = 0; i < layout.parameters.size(); ++i)
    if (typeOf(page.parameters()[i]) != layout.parameters[i].type)
      throwType("parameter type mismatch", layout.parameters[i].name);
  for (std::size_t i = 0; i < layout.arrays.size(); ++i) {
    if (typeOf(page.arrays()[i].values) != layout.arrays[i].type)
      throwType("array type mismatch", layout.arrays[i].name);
    if (page.arrays()[i].dimensions.size() != static_cast<std::size_t>(layout.arrays[i].dimensions))
      throwType("array dimension count mismatch", layout.arrays[i].name);
    ReaderLimits unlimited;
    const auto count = checkedArrayElements(page.arrays()[i].dimensions, unlimited, {}, 0,
                                            layout.arrays[i].name);
    if (count != valuesSize(page.arrays()[i].values))
      throwType("array element count mismatch", layout.arrays[i].name);
  }
  for (std::size_t i = 0; i < layout.columns.size(); ++i) {
    if (typeOf(page.columns()[i]) != layout.columns[i].type)
      throwType("column type mismatch", layout.columns[i].name);
    if (!rows) rows = valuesSize(page.columns()[i]);
    else if (*rows != valuesSize(page.columns()[i]))
      throwType("columns have inconsistent row counts", layout.columns[i].name);
  }
}

struct DecodedPage {
  Page page;
  std::int64_t rows = 0;
  std::int64_t rawRows = 0;
  bool recovered = false;
};

bool recoveryEnabled(const Layout &layout, const ReaderOptions &options) {
  return options.recovery == RecoveryMode::Recover ||
         (options.recovery == RecoveryMode::Automatic &&
          layout.data.rowCountMode == RowCountMode::Fixed);
}

bool endOfStreamError(const FormatError &error) {
  const std::string message(error.what());
  return message.find("end of file") != std::string::npos ||
         message.find("truncated LZMA/XZ stream") != std::string::npos;
}

struct ResolvedReadRequest {
  std::vector<bool> parameters;
  std::vector<bool> arrays;
  std::vector<bool> columns;
  RowSlice rows;
};

template <class Definition>
std::vector<bool> resolveFields(const FieldSelection &selection,
                                const std::vector<Definition> &definitions,
                                std::uint32_t &projected, const ReaderOptions &options) {
  if (selection.all && !selection.names.empty())
    throwState("all-fields projection cannot also contain names");
  std::vector<bool> result(definitions.size(), selection.all);
  if (!selection.all) {
    for (const auto &name : selection.names) {
      const auto found = std::find_if(definitions.begin(), definitions.end(),
                                      [&](const auto &definition) {
                                        return definition.name == name;
                                      });
      if (found == definitions.end())
        throwType("unknown projected field: " + name, name);
      result[static_cast<std::size_t>(found - definitions.begin())] = true;
    }
  }
  for (const bool selected : result) {
    if (!selected) continue;
    if (projected == options.limits.maxProjectedFields)
      throwLimit("projection exceeds configured field limit");
    ++projected;
  }
  return result;
}

ResolvedReadRequest resolveRequest(const Layout &layout, const ReadRequest &request,
                                   const ReaderOptions &options) {
  if (request.rows.first < 0 || request.rows.stride < 1 ||
      (request.rows.count && *request.rows.count < 0) ||
      (request.rows.last && *request.rows.last < 0) ||
      (request.rows.last && (request.rows.first || request.rows.count)))
    throwState("invalid row slice");
  std::uint32_t projected = 0;
  ResolvedReadRequest result;
  result.parameters = resolveFields(request.parameters, layout.parameters, projected, options);
  result.arrays = resolveFields(request.arrays, layout.arrays, projected, options);
  result.columns = resolveFields(request.columns, layout.columns, projected, options);
  result.rows = request.rows;
  return result;
}

bool selectKnownRow(const RowSlice &slice, std::int64_t row, std::int64_t total) {
  const std::int64_t begin = slice.last ? std::max<std::int64_t>(0, total - *slice.last)
                                        : std::min(slice.first, total);
  if (row < begin || (row - begin) % slice.stride)
    return false;
  return !slice.count || (row - begin) / slice.stride < *slice.count;
}

void appendRingScalar(Values &values, Scalar value, std::size_t limit,
                      std::size_t position) {
  if (!limit) return;
  std::visit([&](auto &items) {
    using Vector = std::decay_t<decltype(items)>;
    using Value = typename Vector::value_type;
    if (items.size() < limit)
      items.push_back(std::get<Value>(std::move(value)));
    else
      items.at(position) = std::get<Value>(std::move(value));
  }, values);
}

void rotateRing(Values &values, std::size_t first) {
  if (!first) return;
  std::visit([&](auto &items) {
    if (first < items.size()) std::rotate(items.begin(), items.begin() + first, items.end());
  }, values);
}

std::vector<std::string> readAsciiTokens(AsciiCursor &cursor, std::size_t count,
                                         const std::string &what) {
  std::vector<std::string> result;
  result.reserve(count);
  while (result.size() < count) {
    auto line = cursor.nextLine(false);
    if (!line)
      throwFormat("unexpected EOF reading " + what);
    auto tokens = splitAsciiTokens(*line);
    result.insert(result.end(), std::make_move_iterator(tokens.begin()),
                  std::make_move_iterator(tokens.end()));
  }
  if (result.size() != count)
    throwFormat("too many values while reading " + what);
  return result;
}

void discardAsciiTokens(AsciiCursor &cursor, std::size_t count, const std::string &what,
                        Type type, const ReaderOptions &options,
                        const std::filesystem::path &path, std::int64_t page,
                        const std::string &field) {
  std::size_t consumed = 0;
  while (consumed < count) {
    auto line = cursor.nextLine(false);
    if (!line) throwFormat("unexpected EOF reading " + what, path, page, field);
    auto tokens = splitAsciiTokens(*line);
    if (tokens.size() > count - consumed)
      throwFormat("too many values while reading " + what, path, page, field);
    if (type == Type::String)
      for (const auto &token : tokens)
        if (token.size() > options.limits.maxStringBytes)
          throwLimit("ASCII string exceeds configured limit", path, page, field);
    consumed += tokens.size();
  }
}

void retainStride(Values &values, std::int64_t stride) {
  if (stride == 1) return;
  std::visit([&](auto &items) {
    using Vector = std::decay_t<decltype(items)>;
    Vector retained;
    retained.reserve((items.size() + static_cast<std::size_t>(stride) - 1) /
                     static_cast<std::size_t>(stride));
    for (std::size_t index = 0; index < items.size(); index += static_cast<std::size_t>(stride))
      retained.push_back(std::move(items[index]));
    items = std::move(retained);
  }, values);
}

std::optional<std::string> findAsciiPage(BufferedStream &stream, const ReaderOptions &options,
                                         const std::filesystem::path &path, bool &foundMarker) {
  foundMarker = false;
  while (auto line = stream.line(options.limits.maxLayoutCommandBytes, path)) {
    const std::string stripped = trim(*line);
    if (stripped.empty())
      continue;
    if (stripped.front() == '!') {
      if (lower(stripped).find("page number") != std::string::npos)
        foundMarker = true;
      continue;
    }
    return line;
  }
  return std::nullopt;
}

DecodedPage readAsciiPage(BufferedStream &stream, const std::shared_ptr<const Layout> &layout,
                          const ReaderOptions &options, const std::filesystem::path &path,
                          std::int64_t pageNumber, std::optional<std::string> firstLine,
                          const ResolvedReadRequest &request) {
  AsciiCursor cursor(stream, options, path, std::move(firstLine));
  Page page(layout, LoadMode::None);
  for (std::size_t i = 0; i < layout->parameters.size(); ++i) {
    const auto &definition = layout->parameters[i];
    std::string value;
    if (definition.fixedValue) {
      value = *definition.fixedValue;
    } else {
      auto line = cursor.nextLine(false);
      if (!line)
        throwFormat("unexpected EOF reading parameter", path, pageNumber, definition.name);
      auto tokens = splitAsciiTokens(*line);
      if (tokens.empty())
        throwFormat("missing parameter value", path, pageNumber, definition.name);
      value = definition.type == Type::String ? tokens.front() : tokens.front();
    }
    if (definition.type == Type::String && value.size() > options.limits.maxStringBytes)
      throwLimit("ASCII string exceeds configured limit", path, pageNumber, definition.name);
    if (request.parameters[i]) {
      try {
        page.setParameter(i, parseAsciiScalar(definition.type, value));
      } catch (const Error &error) {
        throwWithContext(error, path, pageNumber, stream.offset(), std::nullopt,
                         definition.name);
      }
    }
  }

  for (std::size_t i = 0; i < layout->arrays.size(); ++i) {
    const auto &definition = layout->arrays[i];
    const auto dimensionTokens = readAsciiTokens(
        cursor, static_cast<std::size_t>(definition.dimensions), "array dimensions");
    ArrayData array;
    array.dimensions.reserve(static_cast<std::size_t>(definition.dimensions));
    for (const auto &token : dimensionTokens) {
      try {
        array.dimensions.push_back(parseInteger<std::int32_t>(token, "array dimension"));
      } catch (const Error &error) {
        throwWithContext(error, path, pageNumber, stream.offset(), std::nullopt,
                         definition.name);
      }
    }
    const std::uint64_t elements = checkedArrayElements(array.dimensions, options.limits, path,
                                                        pageNumber, definition.name);
    array.values = emptyValues(definition.type);
    if (elements && request.arrays[i]) {
      const auto values = readAsciiTokens(cursor, static_cast<std::size_t>(elements),
                                          "array " + definition.name);
      for (const auto &value : values) {
        if (definition.type == Type::String && value.size() > options.limits.maxStringBytes)
          throwLimit("ASCII string exceeds configured limit", path, pageNumber, definition.name);
        try {
          appendScalar(array.values, parseAsciiScalar(definition.type, value));
        } catch (const Error &error) {
          throwWithContext(error, path, pageNumber, stream.offset(), std::nullopt,
                           definition.name);
        }
      }
    } else if (elements) {
      discardAsciiTokens(cursor, static_cast<std::size_t>(elements),
                         "array " + definition.name, definition.type, options,
                         path, pageNumber, definition.name);
    }
    if (request.arrays[i]) page.setArray(i, std::move(array));
  }

  std::int64_t expectedRows = 0;
  if (!layout->columns.empty() && layout->data.rowCountMode != RowCountMode::None) {
    auto line = cursor.nextLine(false);
    if (!line)
      throwFormat("unexpected EOF reading row count", path, pageNumber);
    const auto tokens = splitAsciiTokens(*line);
    if (tokens.empty())
      throwFormat("missing row count", path, pageNumber);
    expectedRows = parseInteger<std::int64_t>(tokens.front(), "row count");
    if (expectedRows < 0)
      throwFormat("negative row count", path, pageNumber);
    if (expectedRows > options.limits.maxRows)
      throwLimit("page exceeds configured row limit", path, pageNumber);
  }

  std::vector<Values> columns;
  columns.reserve(layout->columns.size());
  for (const auto &definition : layout->columns)
    columns.push_back(emptyValues(definition.type));

  bool recovered = false;
  std::int64_t rows = 0;
  if (!layout->columns.empty()) {
    std::vector<std::string> rowTokens;
    const bool counted = layout->data.rowCountMode != RowCountMode::None;
    while (!counted || rows < expectedRows) {
      while (rowTokens.size() < layout->columns.size()) {
        auto line = cursor.nextLine(true);
        if (!line || trim(*line).empty()) {
          if (counted && rows < expectedRows) {
            if (!recoveryEnabled(*layout, options))
              throwFormat("unexpected EOF in ASCII row data", path, pageNumber,
                          std::nullopt, stream.offset(), rows);
            recovered = true;
          } else if (!rowTokens.empty() && !recoveryEnabled(*layout, options)) {
            throwFormat("incomplete ASCII row", path, pageNumber, std::nullopt,
                        stream.offset(), rows);
          } else if (!rowTokens.empty()) {
            recovered = true;
          }
          rowTokens.clear();
          goto ascii_rows_done;
        }
        auto tokens = splitAsciiTokens(*line);
        rowTokens.insert(rowTokens.end(), std::make_move_iterator(tokens.begin()),
                         std::make_move_iterator(tokens.end()));
      }
      if (rowTokens.size() != layout->columns.size())
        throwFormat("ASCII row has the wrong number of fields", path, pageNumber,
                    std::nullopt, stream.offset(), rows);
      try {
        for (std::size_t i = 0; i < layout->columns.size(); ++i) {
          if (layout->columns[i].type == Type::String &&
              rowTokens[i].size() > options.limits.maxStringBytes)
            throwLimit("ASCII string exceeds configured limit", path, pageNumber,
                       layout->columns[i].name);
          bool selected = false;
          if (layout->data.rowCountMode == RowCountMode::None && request.rows.last) {
            selected = *request.rows.last > 0;
          } else if (layout->data.rowCountMode == RowCountMode::None) {
            const auto &slice = request.rows;
            selected = rows >= slice.first && (rows - slice.first) % slice.stride == 0 &&
                       (!slice.count || (rows - slice.first) / slice.stride < *slice.count);
          } else {
            selected = selectKnownRow(request.rows, rows, expectedRows);
          }
          if (request.columns[i] && selected) {
            Scalar value;
            try {
              value = parseAsciiScalar(layout->columns[i].type, rowTokens[i]);
            } catch (const Error &error) {
              throwWithContext(error, path, pageNumber, stream.offset(), rows,
                               layout->columns[i].name);
            }
            if (layout->data.rowCountMode == RowCountMode::None && request.rows.last) {
              const auto limit = static_cast<std::size_t>(*request.rows.last);
              appendRingScalar(columns[i], std::move(value), limit,
                               limit ? static_cast<std::size_t>(rows) % limit : 0);
            } else {
              appendScalar(columns[i], std::move(value));
            }
          }
        }
      } catch (const Error &error) {
        throwWithContext(error, path, pageNumber, stream.offset(), rows);
      }
      rowTokens.clear();
      ++rows;
      if (rows > options.limits.maxRows)
        throwLimit("page exceeds configured row limit", path, pageNumber,
                   std::nullopt, rows);
    }
  }
ascii_rows_done:
  if (layout->data.rowCountMode == RowCountMode::None && request.rows.last) {
    const auto limit = static_cast<std::size_t>(*request.rows.last);
    const std::size_t first = limit && rows > static_cast<std::int64_t>(limit)
        ? static_cast<std::size_t>(rows) % limit : 0;
    for (std::size_t i = 0; i < columns.size(); ++i)
      if (request.columns[i]) {
        rotateRing(columns[i], first);
        retainStride(columns[i], request.rows.stride);
      }
  }
  std::int64_t selectedRows = 0;
  for (std::size_t i = 0; i < columns.size(); ++i) {
    if (!request.columns[i]) continue;
    selectedRows = static_cast<std::int64_t>(valuesSize(columns[i]));
    page.setColumn(i, std::move(columns[i]));
  }
  if (layout->columns.empty() ||
      std::none_of(request.columns.begin(), request.columns.end(), [](bool value) { return value; })) {
    if (layout->data.rowCountMode == RowCountMode::None && request.rows.last) {
      const std::int64_t retained = std::min(rows, *request.rows.last);
      selectedRows = (retained + request.rows.stride - 1) / request.rows.stride;
    } else {
      for (std::int64_t row = 0; row < rows; ++row)
        if (layout->data.rowCountMode == RowCountMode::None
                ? (row >= request.rows.first && (row - request.rows.first) % request.rows.stride == 0 &&
                   (!request.rows.count ||
                    (row - request.rows.first) / request.rows.stride < *request.rows.count))
                : selectKnownRow(request.rows, row, expectedRows))
          ++selectedRows;
    }
  }
  return {std::move(page), selectedRows, rows, recovered};
}

DecodedPage readBinaryPage(BufferedStream &stream, const std::shared_ptr<const Layout> &layout,
                           const ReaderOptions &options, const std::filesystem::path &path,
                           std::int64_t pageNumber, const ResolvedReadRequest &request) {
  const ByteOrder order = layout->data.byteOrder;
  const std::int32_t count32 = readPod<std::int32_t>(stream, order, path, pageNumber);
  const std::int64_t expectedRows = count32 == kInt64RowCount
      ? readPod<std::int64_t>(stream, order, path, pageNumber) : count32;
  if (expectedRows < 0)
    throwFormat("negative row count", path, pageNumber);
  if (expectedRows > options.limits.maxRows)
    throwLimit("page exceeds configured row limit", path, pageNumber);

  Page page(layout, LoadMode::None);
  for (std::size_t i = 0; i < layout->parameters.size(); ++i) {
    const auto &definition = layout->parameters[i];
    try {
      if (definition.fixedValue) {
        if (request.parameters[i])
          page.setParameter(i, parseAsciiScalar(definition.type, *definition.fixedValue));
      } else if (request.parameters[i]) {
        page.setParameter(i, readBinaryScalar(stream, definition.type, order,
                                              options.longDoubleEncoding, options.limits,
                                              path, pageNumber));
      } else {
        skipBinaryScalar(stream, definition.type, order, options.longDoubleEncoding,
                         options.limits, path, pageNumber);
      }
    } catch (const Error &error) {
      throwWithContext(error, path, pageNumber, stream.offset(), std::nullopt,
                       definition.name);
    }
  }
  for (std::size_t i = 0; i < layout->arrays.size(); ++i) {
    const auto &definition = layout->arrays[i];
    try {
      ArrayData array;
      for (std::int32_t dimension = 0; dimension < definition.dimensions; ++dimension)
        array.dimensions.push_back(readPod<std::int32_t>(stream, order, path, pageNumber));
      const std::uint64_t elements = checkedArrayElements(array.dimensions, options.limits, path,
                                                          pageNumber, definition.name);
      array.values = emptyValues(definition.type);
      for (std::uint64_t element = 0; element < elements; ++element) {
        if (request.arrays[i])
          appendScalar(array.values,
                       readBinaryScalar(stream, definition.type, order,
                                        options.longDoubleEncoding, options.limits,
                                        path, pageNumber));
        else
          skipBinaryScalar(stream, definition.type, order, options.longDoubleEncoding,
                           options.limits, path, pageNumber);
      }
      if (request.arrays[i]) page.setArray(i, std::move(array));
    } catch (const Error &error) {
      throwWithContext(error, path, pageNumber, stream.offset(), std::nullopt,
                       definition.name);
    }
  }

  std::vector<Values> columns;
  columns.reserve(layout->columns.size());
  for (const auto &definition : layout->columns)
    columns.push_back(emptyValues(definition.type));
  bool recovered = false;
  std::int64_t rows = 0;
  const bool fixedWidthColumns = std::none_of(
      layout->columns.begin(), layout->columns.end(),
      [](const auto &definition) { return definition.type == Type::String; });
  if (fixedWidthColumns && layout->data.majorOrder == MajorOrder::Row) {
    std::vector<std::uint64_t> offsets(layout->columns.size());
    std::uint64_t rowBytes = 0;
    bool anySelected = false;
    std::size_t selectedCapacity = 0;
    for (std::int64_t row = 0; row < expectedRows; ++row)
      if (selectKnownRow(request.rows, row, expectedRows)) ++selectedCapacity;
    for (std::size_t i = 0; i < layout->columns.size(); ++i) {
      offsets[i] = rowBytes;
      const std::uint64_t width = binaryScalarBytes(layout->columns[i].type,
                                                     options.longDoubleEncoding);
      if (width > UINT64_MAX - rowBytes)
        throwLimit("binary row width overflow", path, pageNumber);
      rowBytes += width;
      if (request.columns[i]) {
        anySelected = true;
        reserveValues(columns[i], selectedCapacity);
      }
    }
    if (rowBytes && static_cast<std::uint64_t>(expectedRows) > UINT64_MAX / rowBytes)
      throwLimit("binary page size overflow", path, pageNumber);
    if (!anySelected && !recoveryEnabled(*layout, options)) {
      const std::uint64_t startOffset = stream.offset();
      try {
        stream.skip(rowBytes * static_cast<std::uint64_t>(expectedRows), path, pageNumber);
      } catch (const Error &error) {
        const std::uint64_t consumed = stream.offset() - startOffset;
        const std::uint64_t withinRow = consumed % rowBytes;
        std::optional<std::string> field;
        for (std::size_t i = 0; i < layout->columns.size(); ++i)
          if (withinRow >= offsets[i]) field = layout->columns[i].name;
        throwWithContext(error, path, pageNumber, stream.offset(),
                         static_cast<std::int64_t>(consumed / rowBytes), field);
      }
      rows = expectedRows;
    } else if (!rowBytes) {
      rows = expectedRows;
    } else {
      const std::uint64_t chunkRows = std::max<std::uint64_t>(
          1, (1024U * 1024U) / rowBytes);
      std::vector<unsigned char> buffer(static_cast<std::size_t>(
          std::min<std::uint64_t>(static_cast<std::uint64_t>(expectedRows), chunkRows) *
          rowBytes));
      while (rows < expectedRows) {
        const std::uint64_t requestedRows = std::min<std::uint64_t>(
            static_cast<std::uint64_t>(expectedRows - rows), chunkRows);
        const std::size_t requestedBytes = static_cast<std::size_t>(requestedRows * rowBytes);
        const std::size_t bytesRead = stream.read(buffer.data(), requestedBytes);
        const std::int64_t completeRows = static_cast<std::int64_t>(bytesRead / rowBytes);
        for (std::int64_t localRow = 0; localRow < completeRows; ++localRow) {
          const std::int64_t sourceRow = rows + localRow;
          if (!selectKnownRow(request.rows, sourceRow, expectedRows)) continue;
          const auto *rowData = buffer.data() + static_cast<std::size_t>(localRow * rowBytes);
          for (std::size_t i = 0; i < layout->columns.size(); ++i)
            if (request.columns[i])
              appendFixedBinaryValue(columns[i], layout->columns[i].type,
                                     rowData + offsets[i], order,
                                     options.longDoubleEncoding);
        }
        rows += completeRows;
        if (bytesRead != requestedBytes) {
          if (!recoveryEnabled(*layout, options)) {
            const std::uint64_t withinRow = bytesRead % rowBytes;
            std::optional<std::string> field;
            for (std::size_t i = 0; i < layout->columns.size(); ++i)
              if (withinRow >= offsets[i]) field = layout->columns[i].name;
            throwFormat("unexpected end of file", path, pageNumber, field,
                        stream.offset(), rows);
          }
          recovered = true;
          break;
        }
      }
    }
  } else if (fixedWidthColumns) {
    std::vector<std::int64_t> columnRows(layout->columns.size(), 0);
    std::size_t selectedCapacity = 0;
    for (std::int64_t row = 0; row < expectedRows; ++row)
      if (selectKnownRow(request.rows, row, expectedRows)) ++selectedCapacity;
    for (std::size_t i = 0; i < layout->columns.size(); ++i) {
      const std::uint64_t width = binaryScalarBytes(layout->columns[i].type,
                                                     options.longDoubleEncoding);
      if (width && static_cast<std::uint64_t>(expectedRows) > UINT64_MAX / width)
        throwLimit("binary column size overflow", path, pageNumber);
      if (!request.columns[i] && !recoveryEnabled(*layout, options)) {
        const std::uint64_t startOffset = stream.offset();
        try {
          stream.skip(width * static_cast<std::uint64_t>(expectedRows), path, pageNumber);
        } catch (const Error &error) {
          const std::uint64_t consumed = stream.offset() - startOffset;
          throwWithContext(error, path, pageNumber, stream.offset(),
                           static_cast<std::int64_t>(consumed / width),
                           layout->columns[i].name);
        }
        columnRows[i] = expectedRows;
        continue;
      }
      if (request.columns[i]) reserveValues(columns[i], selectedCapacity);
      const std::uint64_t chunkRows = std::max<std::uint64_t>(1, (1024U * 1024U) / width);
      std::vector<unsigned char> buffer(static_cast<std::size_t>(
          std::min<std::uint64_t>(static_cast<std::uint64_t>(expectedRows), chunkRows) * width));
      while (columnRows[i] < expectedRows) {
        const std::uint64_t requestedRows = std::min<std::uint64_t>(
            static_cast<std::uint64_t>(expectedRows - columnRows[i]), chunkRows);
        const std::size_t requestedBytes = static_cast<std::size_t>(requestedRows * width);
        const std::size_t bytesRead = stream.read(buffer.data(), requestedBytes);
        const std::int64_t completeRows = static_cast<std::int64_t>(bytesRead / width);
        if (request.columns[i])
          for (std::int64_t localRow = 0; localRow < completeRows; ++localRow) {
            const std::int64_t sourceRow = columnRows[i] + localRow;
            if (selectKnownRow(request.rows, sourceRow, expectedRows))
              appendFixedBinaryValue(columns[i], layout->columns[i].type,
                                     buffer.data() + static_cast<std::size_t>(localRow * width),
                                     order, options.longDoubleEncoding);
          }
        columnRows[i] += completeRows;
        if (bytesRead != requestedBytes) {
          if (!recoveryEnabled(*layout, options))
            throwFormat("unexpected end of file", path, pageNumber, layout->columns[i].name,
                        stream.offset(), columnRows[i]);
          recovered = true;
          break;
        }
      }
      if (recovered) break;
    }
    rows = columnRows.empty() ? expectedRows
                              : *std::min_element(columnRows.begin(), columnRows.end());
    if (recovered)
      for (std::size_t i = 0; i < columns.size(); ++i) {
        if (!request.columns[i]) continue;
        std::size_t selected = 0;
        for (std::int64_t row = 0; row < rows; ++row)
          if (selectKnownRow(request.rows, row, expectedRows)) ++selected;
        std::visit([&](auto &values) { values.resize(std::min(values.size(), selected)); },
                   columns[i]);
      }
  } else if (layout->data.majorOrder == MajorOrder::Row) {
    for (; rows < expectedRows; ++rows) {
      std::size_t currentColumn = 0;
      try {
        const bool selected = selectKnownRow(request.rows, rows, expectedRows);
        for (; currentColumn < layout->columns.size(); ++currentColumn) {
          const auto &definition = layout->columns[currentColumn];
          if (request.columns[currentColumn] && selected)
            appendScalar(columns[currentColumn], readBinaryScalar(
                stream, definition.type, order, options.longDoubleEncoding,
                options.limits, path, pageNumber));
          else
            skipBinaryScalar(stream, definition.type, order, options.longDoubleEncoding,
                             options.limits, path, pageNumber);
        }
      } catch (const FormatError &error) {
        if (!recoveryEnabled(*layout, options) || !endOfStreamError(error))
          throwWithContext(error, path, pageNumber, stream.offset(), rows,
                           currentColumn < layout->columns.size()
                               ? std::optional<std::string>(layout->columns[currentColumn].name)
                               : std::nullopt);
        recovered = true;
        break;
      }
    }
  } else {
    std::vector<std::int64_t> columnRows(layout->columns.size(), 0);
    for (std::size_t i = 0; i < layout->columns.size(); ++i) {
      try {
        for (; columnRows[i] < expectedRows; ++columnRows[i]) {
          if (request.columns[i] && selectKnownRow(request.rows, columnRows[i], expectedRows))
            appendScalar(columns[i], readBinaryScalar(stream, layout->columns[i].type, order,
                                                       options.longDoubleEncoding, options.limits,
                                                       path, pageNumber));
          else
            skipBinaryScalar(stream, layout->columns[i].type, order,
                             options.longDoubleEncoding, options.limits, path, pageNumber);
        }
      } catch (const FormatError &error) {
        if (!recoveryEnabled(*layout, options) || !endOfStreamError(error))
          throwWithContext(error, path, pageNumber, stream.offset(), columnRows[i],
                           layout->columns[i].name);
        recovered = true;
        break;
      }
    }
    rows = columnRows.empty() ? expectedRows
                              : *std::min_element(columnRows.begin(), columnRows.end());
    if (recovered)
      for (std::size_t i = 0; i < columns.size(); ++i) {
        if (!request.columns[i]) continue;
        std::size_t selected = 0;
        for (std::int64_t row = 0; row < rows; ++row)
          if (selectKnownRow(request.rows, row, expectedRows)) ++selected;
        std::visit([&](auto &values) { values.resize(std::min(values.size(), selected)); },
                   columns[i]);
      }
  }
  std::int64_t selectedRows = 0;
  for (std::int64_t row = 0; row < rows; ++row)
    if (selectKnownRow(request.rows, row, expectedRows)) ++selectedRows;
  for (std::size_t i = 0; i < columns.size(); ++i)
    if (request.columns[i]) page.setColumn(i, std::move(columns[i]));
  return {std::move(page), selectedRows, rows, recovered};
}

void writeRowCount(BufferedStream &stream, std::int64_t rows, ByteOrder order) {
  if (rows > INT32_MAX) {
    writePod(stream, kInt64RowCount, order);
    writePod(stream, rows, order);
  } else {
    writePod(stream, static_cast<std::int32_t>(rows), order);
  }
}

void writeAsciiPage(BufferedStream &stream, const Page &page, const Layout &layout,
                    std::int64_t pageNumber) {
  if (layout.data.rowCountMode == RowCountMode::None && pageNumber > 1)
    stream.write("\n");
  stream.write("! page number " + std::to_string(pageNumber) + "\n");
  for (std::size_t i = 0; i < layout.parameters.size(); ++i) {
    if (layout.parameters[i].fixedValue)
      continue;
    stream.write(formatAsciiScalar(page.parameter(i)) + "\n");
  }
  for (std::size_t i = 0; i < layout.arrays.size(); ++i) {
    const auto &definition = layout.arrays[i];
    const auto &array = page.array(i);
    for (const auto dimension : array.dimensions)
      stream.write(std::to_string(dimension) + " ");
    stream.write("          ! " + std::to_string(definition.dimensions) +
                 "-dimensional array " + definition.name + ":\n");
    for (std::size_t element = 0; element < valuesSize(array.values); ++element) {
      stream.write(formatAsciiScalar(scalarAt(array.values, element)));
      if ((element + 1) % 6 == 0 || element + 1 == valuesSize(array.values))
        stream.write("\n");
      else
        stream.write(" ");
    }
  }
  if (!layout.columns.empty() && layout.data.rowCountMode != RowCountMode::None)
    stream.write(std::to_string(page.rowCount()) + "\n");
  for (std::int64_t row = 0; row < page.rowCount(); ++row) {
    for (std::size_t column = 0; column < layout.columns.size(); ++column) {
      if (column)
        stream.write(" ");
      stream.write(formatAsciiScalar(scalarAt(page.column(column),
                                              static_cast<std::size_t>(row))));
    }
    stream.write("\n");
  }
}

void writeBinaryPage(BufferedStream &stream, const Page &page, const Layout &layout,
                     const WriterOptions &options) {
  const ByteOrder order = layout.data.byteOrder;
  writeRowCount(stream, page.rowCount(), order);
  for (std::size_t i = 0; i < layout.parameters.size(); ++i)
    if (!layout.parameters[i].fixedValue)
      writeBinaryScalar(stream, page.parameter(i), order, options.longDoubleEncoding);
  for (std::size_t i = 0; i < layout.arrays.size(); ++i) {
    const auto &array = page.array(i);
    for (const auto dimension : array.dimensions)
      writePod(stream, dimension, order);
    for (std::size_t element = 0; element < valuesSize(array.values); ++element)
      writeBinaryScalar(stream, scalarAt(array.values, element), order,
                        options.longDoubleEncoding);
  }
  const bool fixedWidthColumns = std::none_of(
      layout.columns.begin(), layout.columns.end(),
      [](const auto &definition) { return definition.type == Type::String; });
  if (fixedWidthColumns && layout.data.majorOrder == MajorOrder::Row &&
      !layout.columns.empty()) {
    std::vector<std::uint64_t> offsets(layout.columns.size());
    std::vector<const Values *> columnValues;
    columnValues.reserve(layout.columns.size());
    bool allDouble = true;
    std::uint64_t rowBytes = 0;
    for (std::size_t column = 0; column < layout.columns.size(); ++column) {
      columnValues.push_back(&page.column(column));
      allDouble = allDouble && layout.columns[column].type == Type::Double;
      offsets[column] = rowBytes;
      const std::uint64_t width = binaryScalarBytes(layout.columns[column].type,
                                                     options.longDoubleEncoding);
      if (width > UINT64_MAX - rowBytes) throwLimit("binary row width overflow");
      rowBytes += width;
    }
    const bool directDouble = allDouble && !mustSwap(order);
    const std::uint64_t chunkRows = std::max<std::uint64_t>(1, (1024U * 1024U) / rowBytes);
    std::vector<unsigned char> buffer(static_cast<std::size_t>(
        std::min<std::uint64_t>(static_cast<std::uint64_t>(page.rowCount()), chunkRows) *
        rowBytes));
    std::int64_t firstRow = 0;
    while (firstRow < page.rowCount()) {
      const std::uint64_t rows = std::min<std::uint64_t>(
          static_cast<std::uint64_t>(page.rowCount() - firstRow), chunkRows);
      for (std::uint64_t row = 0; row < rows; ++row) {
        auto *rowData = buffer.data() + static_cast<std::size_t>(row * rowBytes);
        if (allDouble) {
          for (std::size_t column = 0; column < layout.columns.size(); ++column) {
            const double value = std::get<std::vector<double>>(*columnValues[column])[
                static_cast<std::size_t>(firstRow + row)];
            if (directDouble)
              std::memcpy(rowData + offsets[column], &value, sizeof(value));
            else
              storeFixedValue(rowData + offsets[column], value, order);
          }
        } else {
          for (std::size_t column = 0; column < layout.columns.size(); ++column)
            storeFixedBinaryValue(rowData + offsets[column], *columnValues[column],
                                  static_cast<std::size_t>(firstRow + row),
                                  layout.columns[column].type, order,
                                  options.longDoubleEncoding);
        }
      }
      stream.write(buffer.data(), static_cast<std::size_t>(rows * rowBytes));
      firstRow += static_cast<std::int64_t>(rows);
    }
  } else if (fixedWidthColumns && layout.data.majorOrder == MajorOrder::Column) {
    for (std::size_t column = 0; column < layout.columns.size(); ++column) {
      const std::uint64_t width = binaryScalarBytes(layout.columns[column].type,
                                                     options.longDoubleEncoding);
      const std::uint64_t chunkRows = std::max<std::uint64_t>(1, (1024U * 1024U) / width);
      std::vector<unsigned char> buffer(static_cast<std::size_t>(
          std::min<std::uint64_t>(static_cast<std::uint64_t>(page.rowCount()), chunkRows) *
          width));
      std::int64_t firstRow = 0;
      while (firstRow < page.rowCount()) {
        const std::uint64_t rows = std::min<std::uint64_t>(
            static_cast<std::uint64_t>(page.rowCount() - firstRow), chunkRows);
        for (std::uint64_t row = 0; row < rows; ++row)
          storeFixedBinaryValue(buffer.data() + static_cast<std::size_t>(row * width),
                                page.column(column),
                                static_cast<std::size_t>(firstRow + row),
                                layout.columns[column].type, order,
                                options.longDoubleEncoding);
        stream.write(buffer.data(), static_cast<std::size_t>(rows * width));
        firstRow += static_cast<std::int64_t>(rows);
      }
    }
  } else if (layout.data.majorOrder == MajorOrder::Column) {
    for (std::size_t column = 0; column < layout.columns.size(); ++column)
      for (std::int64_t row = 0; row < page.rowCount(); ++row)
        writeBinaryScalar(stream, scalarAt(page.column(column),
                                            static_cast<std::size_t>(row)), order,
                          options.longDoubleEncoding);
  } else {
    for (std::int64_t row = 0; row < page.rowCount(); ++row)
      for (std::size_t column = 0; column < layout.columns.size(); ++column)
        writeBinaryScalar(stream, scalarAt(page.column(column),
                                            static_cast<std::size_t>(row)), order,
                          options.longDoubleEncoding);
  }
}

void writePageData(BufferedStream &stream, const Page &page, const Layout &layout,
                   const WriterOptions &options, std::int64_t pageNumber) {
  validatePage(page, layout);
  if (layout.data.mode == DataMode::Ascii)
    writeAsciiPage(stream, page, layout, pageNumber);
  else
    writeBinaryPage(stream, page, layout, options);
}

std::unique_ptr<Stream> standardInput(const ReaderOptions &options) {
  const Compression compression = options.compression;
  setBinaryMode(stdin);
  if (compression == Compression::Auto || compression == Compression::None)
    return std::make_unique<FileStream>(stdin, std::filesystem::path{}, false, false,
                                        options.bufferBytes);
#if defined(_WIN32)
  const int descriptor = _dup(_fileno(stdin));
#else
  const int descriptor = dup(fileno(stdin));
#endif
  if (descriptor < 0)
    throwIo("unable to duplicate standard input");
  if (compression == Compression::Gzip)
    return std::make_unique<GzipStream>(gzdopen(descriptor, "rb"),
                                        std::filesystem::path{}, false, options.bufferBytes);
#if defined(_WIN32)
  FILE *file = _fdopen(descriptor, "rb");
#else
  FILE *file = fdopen(descriptor, "rb");
#endif
  if (!file)
    throwIo("unable to open duplicate standard input");
  return std::make_unique<LzmaStream>(file, std::filesystem::path{}, false,
                                      options.bufferBytes);
}

std::unique_ptr<Stream> standardOutput(const WriterOptions &options) {
  const Compression compression = options.compression;
  if (options.gzipLevel < -1 || options.gzipLevel > 9)
    throwState("gzip compression level must be between 0 and 9, or -1 for default");
  if (options.lzmaPreset > 9)
    throwState("LZMA preset must be between 0 and 9");
  setBinaryMode(stdout);
  if (compression == Compression::Auto || compression == Compression::None)
    return std::make_unique<FileStream>(stdout, std::filesystem::path{}, false, true,
                                        options.bufferBytes);
#if defined(_WIN32)
  const int descriptor = _dup(_fileno(stdout));
#else
  const int descriptor = dup(fileno(stdout));
#endif
  if (descriptor < 0)
    throwIo("unable to duplicate standard output");
  if (compression == Compression::Gzip) {
    std::string mode = "wb";
    if (options.gzipLevel >= 0) mode.push_back(static_cast<char>('0' + options.gzipLevel));
    return std::make_unique<GzipStream>(gzdopen(descriptor, mode.c_str()),
                                        std::filesystem::path{}, true, options.bufferBytes);
  }
#if defined(_WIN32)
  FILE *file = _fdopen(descriptor, "wb");
#else
  FILE *file = fdopen(descriptor, "wb");
#endif
  if (!file)
    throwIo("unable to open duplicate standard output");
  return std::make_unique<LzmaStream>(file, std::filesystem::path{}, true,
                                      options.bufferBytes, options.lzmaPreset,
                                      options.lzmaCheck, compression == Compression::Lzma);
}

template <class T>
void mergeValues(std::vector<T> &destination, std::vector<T> &&source,
                 std::size_t startRow) {
  if (destination.size() < startRow)
    destination.resize(startRow);
  if (destination.size() < startRow + source.size())
    destination.resize(startRow + source.size());
  std::move(source.begin(), source.end(), destination.begin() + startRow);
}

void mergeValuesInPlace(Values &existing, Values incoming, std::int64_t startRow) {
  if (startRow < 0)
    throwState("column start row cannot be negative");
  if (existing.index() != incoming.index())
    throwType("column type mismatch");
  std::visit([&](auto &destination) {
    using Vector = std::decay_t<decltype(destination)>;
    mergeValues(destination, std::get<Vector>(std::move(incoming)),
                static_cast<std::size_t>(startRow));
  }, existing);
}

}  // namespace

struct Reader::Impl {
  Impl(std::filesystem::path sourcePath, ReaderOptions readerOptions,
       std::unique_ptr<Stream> input, std::optional<Layout> suppliedLayout = std::nullopt,
       bool isPathBacked = false)
      : path(std::move(sourcePath)), options(std::move(readerOptions)),
        stream(std::move(input), options.limits.maxDecompressedBytes, path),
        pathBacked(isPathBacked) {
    if (pathBacked) identity = fileIdentity(path);
    if (suppliedLayout) {
      validateLayout(*suppliedLayout);
      layout = std::make_shared<Layout>(std::move(*suppliedLayout));
    } else {
      Layout parsed;
      std::unordered_set<std::string> includeStack;
      std::optional<ByteOrder> commentOrder;
      bool fixedRowComment = false;
      parseLayoutStream(stream, parsed, options, path, 0, includeStack, commentOrder,
                        fixedRowComment, true);
      const std::int32_t declaredVersion = parsed.version;
      validateLayout(parsed);
      if (declaredVersion < parsed.version)
        throwFormat("SDDS version is too old for its layout", path);
      parsed.version = declaredVersion;
      layout = std::make_shared<Layout>(std::move(parsed));
    }
    if (stream.seekable()) {
      dataStart = stream.tell();
      rememberOffset(0, *dataStart);
    }
  }

  void rememberOffset(std::size_t index, std::uint64_t offset) {
    if (index >= options.limits.maxPageIndexEntries)
      throwLimit("page index exceeds configured entry limit", path);
    if (pageOffsets.size() <= index) pageOffsets.resize(index + 1);
    pageOffsets[index] = offset;
  }

  std::optional<DecodedPage> next(const ReadRequest &readRequest) {
    if (closed)
      throwState("reader is closed");
    if (disconnected)
      throwState("reader is disconnected");
    const ResolvedReadRequest request = resolveRequest(*layout, readRequest, options);
    if (stream.seekable()) rememberOffset(static_cast<std::size_t>(pageNumber), stream.tell());
    std::optional<DecodedPage> result;
    if (layout->data.mode == DataMode::Ascii) {
      bool marker = false;
      auto first = findAsciiPage(stream, options, path, marker);
      if (first || marker)
        result = readAsciiPage(stream, layout, options, path, pageNumber + 1,
                               std::move(first), request);
    } else {
      const auto first = stream.get();
      if (first) {
        stream.unget(*first);
        result = readBinaryPage(stream, layout, options, path, pageNumber + 1, request);
      }
    }
    if (!result) {
      indexComplete = true;
      indexedPages = pageNumber;
      if (!tailInitialized) {
        tailPageNumber = pageNumber;
        tailRows = lastRawRows;
        tailInitialized = true;
      }
      return std::nullopt;
    }
    if (stream.seekable()) rememberOffset(static_cast<std::size_t>(pageNumber + 1), stream.tell());
    return result;
  }

  std::filesystem::path path;
  ReaderOptions options;
  BufferedStream stream;
  std::shared_ptr<Layout> layout;
  std::optional<std::uint64_t> dataStart;
  std::vector<std::uint64_t> pageOffsets;
  std::optional<std::int64_t> indexedPages;
  std::int64_t pageNumber = 0;
  std::optional<std::uint64_t> disconnectedOffset;
  std::unique_ptr<PathLock> lock;
  FileIdentity identity;
  std::int64_t lastRawRows = 0;
  std::int64_t tailPageNumber = 0;
  std::int64_t tailRows = 0;
  bool tailInitialized = false;
  bool headerless = false;
  bool pathBacked = false;
  bool indexComplete = false;
  bool disconnected = false;
  bool closed = false;
};

Reader::Reader(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Reader::Reader(Reader &&) noexcept = default;
Reader &Reader::operator=(Reader &&) noexcept = default;
Reader::~Reader() = default;

Reader Reader::open(const std::filesystem::path &path, ReaderOptions options) {
  try {
    auto lock = acquirePathLock(path, options.lockMode, false);
    auto impl = std::make_unique<Impl>(path, options,
                                       openInput(path, options.compression, options.bufferBytes),
                                       std::nullopt, true);
    impl->lock = std::move(lock);
    return Reader(std::move(impl));
  } catch (const Error &error) {
    throwWithContext(error, path, 0);
  }
}

Reader Reader::openHeaderless(const std::filesystem::path &path, Layout layout,
                              ReaderOptions options) {
  try {
    auto lock = acquirePathLock(path, options.lockMode, false);
    auto impl = std::make_unique<Impl>(path, options,
                                       openInput(path, options.compression, options.bufferBytes),
                                       std::move(layout), true);
    impl->lock = std::move(lock);
    impl->headerless = true;
    return Reader(std::move(impl));
  } catch (const Error &error) {
    throwWithContext(error, path, 0);
  }
}

Reader Reader::fromStdin(ReaderOptions options) {
  return Reader(std::make_unique<Impl>(std::filesystem::path{}, options,
                                       standardInput(options)));
}

Reader Reader::fromSource(std::unique_ptr<InputSource> source, std::string sourceName,
                          ReaderOptions options) {
  const std::filesystem::path path(sourceName);
  try {
    auto stream = wrapInputCodec(std::make_unique<InputSourceStream>(std::move(source)), options);
    return Reader(std::make_unique<Impl>(path, options, std::move(stream)));
  } catch (const Error &error) {
    throwWithContext(error, path, 0);
  }
}

Reader Reader::fromHeaderlessSource(std::unique_ptr<InputSource> source, Layout layout,
                                    std::string sourceName, ReaderOptions options) {
  const std::filesystem::path path(sourceName);
  try {
    auto stream = wrapInputCodec(std::make_unique<InputSourceStream>(std::move(source)), options);
    auto impl = std::make_unique<Impl>(path, options, std::move(stream), std::move(layout));
    impl->headerless = true;
    return Reader(std::move(impl));
  } catch (const Error &error) {
    throwWithContext(error, path, 0);
  }
}

const Layout &Reader::layout() const {
  if (!impl_ || impl_->closed)
    throwState("reader is closed");
  return *impl_->layout;
}

std::optional<Page> Reader::next(ReadRequest request) {
  if (!impl_)
    throwState("reader has no implementation");
  std::optional<DecodedPage> decoded;
  try {
    decoded = impl_->next(request);
  } catch (const Error &error) {
    throwWithContext(error, impl_->path, impl_->pageNumber + 1, impl_->stream.offset());
  }
  if (!decoded)
    return std::nullopt;
  ++impl_->pageNumber;
  impl_->lastRawRows = decoded->rawRows;
  decoded->page.number_ = impl_->pageNumber;
  decoded->page.rowCount_ = decoded->rows;
  decoded->page.recovered_ = decoded->recovered;
  decoded->page.maxTransformationElements_ = impl_->options.limits.maxTransformationElements;
  return std::move(decoded->page);
}

std::optional<Page> Reader::next(ReadSelection selection) {
  ReadRequest request;
  request.rows.first = selection.sparseOffset;
  request.rows.stride = selection.sparseInterval;
  request.rows.last = selection.lastRows;
  return next(std::move(request));
}

void Reader::gotoPage(std::int64_t pageNumber) {
  if (!impl_ || impl_->closed)
    throwState("reader is closed");
  if (pageNumber < 1)
    throwState("page numbers start at one");
  if (!impl_->dataStart)
    throwState("gotoPage requires a seekable, uncompressed input");
  std::size_t startIndex = 0;
  if (static_cast<std::size_t>(pageNumber - 1) < impl_->pageOffsets.size()) {
    startIndex = static_cast<std::size_t>(pageNumber - 1);
  } else if (!impl_->pageOffsets.empty()) {
    startIndex = impl_->pageOffsets.size() - 1;
  }
  impl_->stream.seek(impl_->pageOffsets.at(startIndex));
  impl_->pageNumber = static_cast<std::int64_t>(startIndex);
  ReadRequest skip;
  skip.parameters = FieldSelection::noFields();
  skip.arrays = FieldSelection::noFields();
  skip.columns = FieldSelection::noFields();
  while (impl_->pageNumber + 1 < pageNumber)
    if (!next(skip))
      throwState("requested page is beyond end of file");
}

void Reader::buildPageIndex() {
  if (!impl_ || impl_->closed) throwState("reader is closed");
  if (!impl_->dataStart) throwState("page indexing requires a seekable, uncompressed input");
  const std::uint64_t savedOffset = impl_->stream.tell();
  const std::int64_t savedPage = impl_->pageNumber;
  impl_->stream.seek(*impl_->dataStart);
  impl_->pageNumber = 0;
  ReadRequest skip;
  skip.parameters = FieldSelection::noFields();
  skip.arrays = FieldSelection::noFields();
  skip.columns = FieldSelection::noFields();
  while (next(skip)) {}
  impl_->indexedPages = impl_->pageNumber;
  impl_->indexComplete = true;
  impl_->stream.seek(savedOffset);
  impl_->pageNumber = savedPage;
}

std::optional<std::int64_t> Reader::indexedPageCount() const noexcept {
  return impl_ && impl_->indexComplete ? impl_->indexedPages : std::nullopt;
}

void Reader::disconnect() {
  if (!impl_ || impl_->closed) throwState("reader is closed");
  if (impl_->disconnected) return;
  if (!impl_->pathBacked || compressionFor(impl_->path, impl_->options.compression) != Compression::None ||
      !impl_->stream.seekable())
    throwState("disconnect requires a path-backed, uncompressed, seekable input");
  impl_->disconnectedOffset = impl_->stream.tell();
  impl_->stream.close();
  impl_->disconnected = true;
}

void Reader::reconnect() {
  if (!impl_ || impl_->closed) throwState("reader is closed");
  if (!impl_->disconnected) return;
  const FileIdentity current = fileIdentity(impl_->path);
  if (!sameFile(impl_->identity, current))
    throwState("input file was replaced while disconnected");
  auto input = openInput(impl_->path, Compression::None, impl_->options.bufferBytes);
  impl_->stream.replace(std::move(input), *impl_->disconnectedOffset);
  impl_->disconnected = false;
}

std::optional<PageDelta> Reader::readNewRows(ReadRequest request) {
  if (!impl_ || impl_->closed) throwState("reader is closed");
  if (!impl_->pathBacked || compressionFor(impl_->path, impl_->options.compression) != Compression::None ||
      impl_->layout->data.mode != DataMode::Binary ||
      impl_->layout->data.majorOrder != MajorOrder::Row)
    throwState("reading newly appended rows requires a path-backed, uncompressed "
               "row-major binary input");
  (void)resolveRequest(*impl_->layout, request, impl_->options);
  if (!sameFile(impl_->identity, fileIdentity(impl_->path)))
    throwState("input file was replaced while following it");
  ReaderOptions pollOptions = impl_->options;
  pollOptions.lockMode = LockMode::None;
  Reader poll = Reader::open(impl_->path, pollOptions);
  poll.buildPageIndex();
  const std::int64_t pageNumber = poll.indexedPageCount().value_or(0);
  if (!pageNumber) {
    poll.close();
    return std::nullopt;
  }
  ReadRequest probe;
  probe.parameters = FieldSelection::noFields();
  probe.arrays = FieldSelection::noFields();
  probe.columns = FieldSelection::noFields();
  poll.gotoPage(pageNumber);
  if (!poll.next(probe)) throwState("indexed final page could not be read");
  const std::int64_t observedRows = poll.impl_->lastRawRows;
  if (!impl_->tailInitialized) {
    impl_->tailPageNumber = pageNumber;
    impl_->tailRows = observedRows;
    impl_->tailInitialized = true;
    poll.close();
    return std::nullopt;
  }
  if (pageNumber != impl_->tailPageNumber)
    throwState("readNewRows only follows growth of the existing final page");
  if (observedRows < impl_->tailRows)
    throwState("the followed page was truncated");
  if (observedRows == impl_->tailRows) {
    poll.close();
    return std::nullopt;
  }
  const std::int64_t firstRow = impl_->tailRows;
  const std::int64_t newRows = observedRows - firstRow;
  if (request.rows.last) {
    request.rows.first = firstRow + std::max<std::int64_t>(0, newRows - *request.rows.last);
    request.rows.last.reset();
  } else {
    request.rows.first = firstRow + std::min(request.rows.first, newRows);
  }
  poll.gotoPage(pageNumber);
  auto delta = poll.next(std::move(request));
  poll.close();
  if (!delta) throwState("final page disappeared while reading newly appended rows");
  impl_->tailRows = observedRows;
  return PageDelta{pageNumber, firstRow, std::move(*delta)};
}

MaterializedDataset Reader::readAll(ReadRequest request) {
  MaterializedDataset result{layout(), {}};
  while (auto page = next(request))
    result.pages.push_back(std::move(*page));
  return result;
}

void Reader::close() {
  if (!impl_ || impl_->closed)
    return;
  if (!impl_->disconnected) impl_->stream.close();
  impl_->lock.reset();
  impl_->closed = true;
}

struct Writer::Impl {
  std::filesystem::path path;
  WriterOptions options;
  std::shared_ptr<Layout> layout;
  std::unique_ptr<BufferedStream> stream;
  std::optional<Page> current;
  std::int64_t pageNumber = 0;
  std::int64_t expectedRows = 0;
  std::int64_t updateInterval = 0;
  std::optional<std::uint64_t> pageStart;
  std::optional<std::uint64_t> disconnectedOffset;
  std::unique_ptr<PathLock> lock;
  FileIdentity identity;
  bool snapshotWritten = false;
  bool rewriteLastPage = false;
  bool pathBacked = false;
  bool disconnected = false;
  bool closed = false;

  void writeCurrent(bool keepOpen) {
    if (!current)
      throwState("no page has been started");
    if (disconnected)
      throwState("writer is disconnected");
    validatePage(*current, *layout);
    if (rewriteLastPage) {
      const std::filesystem::path temporary(path.string() + ".sddspp.tmp");
      if (std::filesystem::exists(temporary))
        throwState("temporary rewrite file already exists: " + temporary.string());
      try {
        Layout outputLayout = *layout;
        BufferedStream output(openOutput(temporary,
                                         compressionFor(path, options.compression), "wb",
                                         options.bufferBytes, options.gzipLevel,
                                         options.lzmaPreset, options.lzmaCheck));
        writeLayout(output, outputLayout, options);
        std::int64_t number = 0;
        Reader source = Reader::open(path);
        while (number < pageNumber) {
          auto page = source.next();
          if (!page)
            throwState("source file lost a page during atomic last-page update");
          writePageData(output, *page, outputLayout, options, ++number);
        }
        source.close();
        writePageData(output, *current, outputLayout, options, ++number);
        output.close();
#if defined(_WIN32)
        lock.reset();
#endif
        replaceFile(temporary, path);
        identity = fileIdentity(path);
        if (options.lockMode != LockMode::None)
          lock = acquirePathLock(path, options.lockMode, false);
      } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
      }
      snapshotWritten = true;
    } else {
      if (!stream)
        throwState("writer has no output stream");
      if (snapshotWritten) {
        if (!pageStart || !stream->seekable())
          throwState("page update requires a seekable, uncompressed output");
        stream->seek(*pageStart);
      } else {
        pageStart = stream->seekable() ? std::optional<std::uint64_t>(stream->tell())
                                       : std::nullopt;
      }
      writePageData(*stream, *current, *layout, options, pageNumber + 1);
      if (snapshotWritten)
        stream->truncate(stream->tell());
      snapshotWritten = true;
      stream->flush();
    }
    if (!keepOpen) {
      ++pageNumber;
      current.reset();
      pageStart.reset();
      snapshotWritten = false;
    }
  }
};

Writer::Writer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Writer::Writer(Writer &&) noexcept = default;
Writer &Writer::operator=(Writer &&) noexcept = default;
Writer::~Writer() = default;

Writer Writer::create(const std::filesystem::path &path, Layout layout,
                      WriterOptions options) {
  const Compression compression = compressionFor(path, options.compression);
  if (compression == Compression::Xz || compression == Compression::Lzma)
    layout.data.mode = DataMode::Binary;
  validateLayout(layout);
  auto lock = acquirePathLock(path, options.lockMode, true);
  auto impl = std::make_unique<Impl>();
  impl->path = path;
  impl->pathBacked = true;
  impl->options = options;
  impl->layout = std::make_shared<Layout>(std::move(layout));
  impl->stream = std::make_unique<BufferedStream>(
      openOutput(path, options.compression, "wb", options.bufferBytes,
                 options.gzipLevel, options.lzmaPreset, options.lzmaCheck));
  impl->identity = fileIdentity(path);
  impl->lock = std::move(lock);
  writeLayout(*impl->stream, *impl->layout, options);
  return Writer(std::move(impl));
}

Writer Writer::toStdout(Layout layout, WriterOptions options) {
  if (options.compression == Compression::Xz || options.compression == Compression::Lzma)
    layout.data.mode = DataMode::Binary;
  validateLayout(layout);
  auto impl = std::make_unique<Impl>();
  impl->options = options;
  impl->layout = std::make_shared<Layout>(std::move(layout));
  impl->stream = std::make_unique<BufferedStream>(standardOutput(options));
  writeLayout(*impl->stream, *impl->layout, options);
  return Writer(std::move(impl));
}

Writer Writer::toSink(std::unique_ptr<OutputSink> sink, Layout layout,
                      std::string sinkName, WriterOptions options) {
  if (options.compression == Compression::Xz || options.compression == Compression::Lzma)
    layout.data.mode = DataMode::Binary;
  validateLayout(layout);
  auto impl = std::make_unique<Impl>();
  impl->path = std::filesystem::path(std::move(sinkName));
  impl->options = options;
  impl->layout = std::make_shared<Layout>(std::move(layout));
  auto stream = wrapOutputCodec(std::make_unique<OutputSinkStream>(std::move(sink)), options);
  impl->stream = std::make_unique<BufferedStream>(std::move(stream));
  writeLayout(*impl->stream, *impl->layout, options);
  return Writer(std::move(impl));
}

Writer Writer::append(const std::filesystem::path &path, WriterOptions options) {
  if (compressionFor(path, options.compression) != Compression::None)
    throwState("appending new pages to compressed SDDS files is not supported");
  auto lock = acquirePathLock(path, options.lockMode, false);
  Reader reader = Reader::open(path);
  Layout layout = reader.layout();
  std::int64_t pages = 0;
  if (layout.data.mode == DataMode::Ascii) {
    reader.buildPageIndex();
    pages = reader.indexedPageCount().value_or(0);
  }
  reader.close();
  auto impl = std::make_unique<Impl>();
  impl->path = path;
  impl->pathBacked = true;
  impl->options = options;
  impl->layout = std::make_shared<Layout>(std::move(layout));
  impl->pageNumber = pages;
  impl->stream = std::make_unique<BufferedStream>(
      openOutput(path, Compression::None, "r+b", options.bufferBytes));
  impl->identity = fileIdentity(path);
  impl->lock = std::move(lock);
  impl->stream->seek(std::filesystem::file_size(path));
  return Writer(std::move(impl));
}

Writer Writer::appendToLastPage(const std::filesystem::path &path,
                                std::int64_t updateInterval, WriterOptions options) {
  if (updateInterval < 1)
    throwState("update interval must be positive");
  auto lock = acquirePathLock(path, options.lockMode, false);
  Reader reader = Reader::open(path);
  Layout layout = reader.layout();
  std::optional<std::uint64_t> lastPageOffset;
  std::optional<Page> lastPage;
  std::int64_t totalPages = 0;
  if (reader.impl_->dataStart) {
    reader.buildPageIndex();
    totalPages = reader.indexedPageCount().value_or(0);
    if (totalPages) {
      lastPageOffset = reader.impl_->pageOffsets.at(static_cast<std::size_t>(totalPages - 1));
      reader.gotoPage(totalPages);
      lastPage = reader.next();
    }
  } else {
    while (auto page = reader.next()) {
      lastPage = std::move(*page);
      ++totalPages;
    }
  }
  if (!lastPage)
    throwState("cannot append to the last page of an empty SDDS file");
  reader.close();
  auto impl = std::make_unique<Impl>();
  impl->path = path;
  impl->pathBacked = true;
  impl->options = options;
  impl->layout = std::make_shared<Layout>(std::move(layout));
  impl->current = std::move(*lastPage);
  impl->pageNumber = totalPages - 1;
  impl->updateInterval = updateInterval;
  const bool canUpdateInPlace = compressionFor(path, options.compression) == Compression::None &&
      impl->layout->data.mode == DataMode::Binary &&
      impl->layout->data.majorOrder == MajorOrder::Row;
  if (options.updateStrategy == UpdateStrategy::InPlaceOnly && !canUpdateInPlace)
    throwState("in-place last-page updates require uncompressed row-major binary data");
  impl->rewriteLastPage = options.updateStrategy == UpdateStrategy::AtomicRewrite ||
                          !canUpdateInPlace;
  if (!impl->rewriteLastPage) {
    if (!lastPageOffset)
      throwState("in-place last-page update requires a seekable input");
    impl->stream = std::make_unique<BufferedStream>(
        openOutput(path, Compression::None, "r+b", options.bufferBytes));
    impl->pageStart = *lastPageOffset;
    impl->snapshotWritten = true;
  }
  impl->identity = fileIdentity(path);
  impl->lock = std::move(lock);
  return Writer(std::move(impl));
}

const Layout &Writer::layout() const {
  if (!impl_ || impl_->closed)
    throwState("writer is closed");
  return *impl_->layout;
}

std::int64_t Writer::rowsPresent() const noexcept {
  return impl_ && impl_->current ? impl_->current->rowCount() : 0;
}

void Writer::write(const Page &page) {
  if (!impl_ || impl_->closed)
    throwState("writer is closed");
  if (impl_->current)
    throwState("commit the current page before writing another page");
  impl_->current = page;
  try {
    impl_->writeCurrent(false);
  } catch (const Error &error) {
    throwWithContext(error, impl_->path, impl_->pageNumber + 1,
                     impl_->stream ? std::optional<std::uint64_t>(impl_->stream->offset())
                                   : std::nullopt);
  }
}

void Writer::write(Page &&page) {
  if (!impl_ || impl_->closed) throwState("writer is closed");
  if (impl_->disconnected) throwState("writer is disconnected");
  if (impl_->current) throwState("commit the current page before writing another page");
  impl_->current.emplace(std::move(page));
  try {
    impl_->writeCurrent(false);
  } catch (const Error &error) {
    throwWithContext(error, impl_->path, impl_->pageNumber + 1,
                     impl_->stream ? std::optional<std::uint64_t>(impl_->stream->offset())
                                   : std::nullopt);
  }
}

void Writer::beginPage(std::int64_t expectedRows) {
  if (!impl_ || impl_->closed)
    throwState("writer is closed");
  if (impl_->current)
    throwState("a page is already active");
  if (expectedRows < 0)
    throwState("expected row count cannot be negative");
  impl_->expectedRows = expectedRows;
  impl_->current.emplace(impl_->layout);
}

void Writer::setParameter(std::string_view name, Scalar value) {
  if (!impl_ || !impl_->current)
    throwState("no page has been started");
  impl_->current->setParameter(name, std::move(value));
}

void Writer::setArray(std::string_view name, ArrayData value) {
  if (!impl_ || !impl_->current)
    throwState("no page has been started");
  impl_->current->setArray(name, std::move(value));
}

void Writer::setColumn(std::string_view name, Values value, std::int64_t startRow) {
  if (!impl_ || !impl_->current)
    throwState("no page has been started");
  const std::size_t index = impl_->layout->columnIndex(name);
  if (startRow == 0)
    impl_->current->setColumn(index, std::move(value));
  else {
    mergeValuesInPlace(impl_->current->columns_.at(index), std::move(value), startRow);
    impl_->current->columnsLoaded_.at(index) = true;
    impl_->current->rowCount_ = 0;
    for (std::size_t column = 0; column < impl_->current->columns_.size(); ++column)
      if (impl_->current->columnsLoaded_[column])
        impl_->current->rowCount_ = std::max(
            impl_->current->rowCount_,
            static_cast<std::int64_t>(valuesSize(impl_->current->columns_[column])));
  }
}

void Writer::commitPage() {
  if (!impl_ || impl_->closed)
    throwState("writer is closed");
  try {
    impl_->writeCurrent(false);
  } catch (const Error &error) {
    throwWithContext(error, impl_->path, impl_->pageNumber + 1,
                     impl_->stream ? std::optional<std::uint64_t>(impl_->stream->offset())
                                   : std::nullopt);
  }
}

void Writer::updatePage(bool flushRows) {
  if (!impl_ || impl_->closed)
    throwState("writer is closed");
  if (!impl_->rewriteLastPage && impl_->stream && !impl_->stream->seekable() &&
      impl_->snapshotWritten)
    throwState("page updates require a seekable output or atomic-rewrite mode");
  try {
    impl_->writeCurrent(true);
  } catch (const Error &error) {
    throwWithContext(error, impl_->path, impl_->pageNumber + 1,
                     impl_->stream ? std::optional<std::uint64_t>(impl_->stream->offset())
                                   : std::nullopt);
  }
  if (flushRows)
    sync();
}

void Writer::sync() {
  if (!impl_ || impl_->closed)
    throwState("writer is closed");
  if (impl_->stream) {
    impl_->stream->flush();
    if (impl_->layout->data.fsync) {
      impl_->stream->raw().sync();
    }
  }
}

void Writer::disconnect() {
  if (!impl_ || impl_->closed) throwState("writer is closed");
  if (impl_->disconnected) return;
  if (!impl_->pathBacked || compressionFor(impl_->path, impl_->options.compression) != Compression::None ||
      !impl_->stream || !impl_->stream->seekable())
    throwState("disconnect requires a path-backed, uncompressed, seekable output");
  impl_->disconnectedOffset = impl_->stream->tell();
  impl_->stream->close();
  impl_->disconnected = true;
}

void Writer::reconnect() {
  if (!impl_ || impl_->closed) throwState("writer is closed");
  if (!impl_->disconnected) return;
  if (!sameFile(impl_->identity, fileIdentity(impl_->path)))
    throwState("output file was replaced while disconnected");
  impl_->stream->replace(openOutput(impl_->path, Compression::None, "r+b",
                                    impl_->options.bufferBytes),
                         *impl_->disconnectedOffset);
  impl_->disconnected = false;
}

void Writer::close() {
  if (!impl_ || impl_->closed)
    return;
  try {
    if (impl_->current)
      impl_->writeCurrent(false);
    if (impl_->stream && !impl_->disconnected)
      impl_->stream->close();
  } catch (const Error &error) {
    throwWithContext(error, impl_->path, impl_->pageNumber + 1,
                     impl_->stream ? std::optional<std::uint64_t>(impl_->stream->offset())
                                   : std::nullopt);
  }
  impl_->lock.reset();
  impl_->closed = true;
}

}  // namespace sdds
