/**
 * @file SDDSIO.cc
 * @brief Public byte-source and byte-sink adapters for the C++17 SDDS interface.
 *
 * @details Implements path, standard stream, borrowed or owned FILE, C++
 * stream, and caller-owned memory adapters with explicit capability reporting.
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
#include <cerrno>
#include <cstring>
#include <istream>
#include <limits>
#include <ostream>

#if defined(_WIN32)
#  include <io.h>
#else
#  include <unistd.h>
#endif

namespace sdds {
namespace {

[[noreturn]] void unsupported(const char *operation) {
  throw StateError(ErrorKind::State, std::string("source does not support ") + operation);
}

[[noreturn]] void ioFailure(const char *operation) {
  throw IoError(ErrorKind::Io, std::string(operation) + ": " + std::strerror(errno));
}

class FileInput final : public InputSource {
 public:
  FileInput(FILE *file, bool owned) : file_(file), owned_(owned) {
    initialize();
  }
  explicit FileInput(std::filesystem::path path)
      : file_(std::fopen(path.string().c_str(), "rb")), owned_(true),
        path_(std::move(path)), reopenable_(true) {
    initialize();
  }
  void initialize() {
    if (!file_)
      throw IoError(ErrorKind::Io,
                    path_.empty() ? "null FILE input" :
                                    "unable to open input: " + std::string(std::strerror(errno)),
                    path_);
    const auto position = tellFile();
    seekable_ = position.has_value();
    if (!seekable_) std::clearerr(file_);
  }
  ~FileInput() override { try { close(); } catch (...) {} }

  std::size_t read(void *data, std::size_t size) override {
    requireOpen();
    const std::size_t count = std::fread(data, 1, size, file_);
    if (count < size && std::ferror(file_)) ioFailure("FILE read failed");
    return count;
  }
  bool eof() const override { return !file_ || std::feof(file_) != 0; }
  SourceCapabilities capabilities() const noexcept override {
    return {true, false, seekable_, false, false, false, reopenable_};
  }
  std::uint64_t tell() const override {
    requireOpen();
    const auto value = tellFile();
    if (!value) ioFailure("FILE tell failed");
    return *value;
  }
  void seek(std::uint64_t offset) override {
    requireOpen();
    if (!seekable_) unsupported("seek");
#if defined(_WIN32)
    if (_fseeki64(file_, static_cast<__int64>(offset), SEEK_SET) != 0)
#else
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) ||
        fseeko(file_, static_cast<off_t>(offset), SEEK_SET) != 0)
#endif
      ioFailure("FILE seek failed");
    std::clearerr(file_);
  }
  void close() override {
    if (!file_) return;
    if (reopenable_) reopenOffset_ = tell();
    FILE *closing = file_;
    file_ = nullptr;
    if (owned_ && std::fclose(closing) != 0) ioFailure("FILE close failed");
  }
  void reopen() override {
    if (!reopenable_) unsupported("reopen");
    if (file_) return;
    file_ = std::fopen(path_.string().c_str(), "rb");
    if (!file_) ioFailure("FILE reopen failed");
    seekable_ = true;
    seek(reopenOffset_);
  }

 private:
  void requireOpen() const { if (!file_) unsupported("access after close"); }
  std::optional<std::uint64_t> tellFile() const {
#if defined(_WIN32)
    const auto value = _ftelli64(file_);
#else
    const auto value = ftello(file_);
#endif
    if (value < 0) return std::nullopt;
    return static_cast<std::uint64_t>(value);
  }
  FILE *file_ = nullptr;
  bool owned_ = false;
  bool seekable_ = false;
  std::filesystem::path path_;
  std::uint64_t reopenOffset_ = 0;
  bool reopenable_ = false;
};

class FileOutput final : public OutputSink {
 public:
  FileOutput(FILE *file, bool owned) : file_(file), owned_(owned) {
    initialize();
  }
  FileOutput(std::filesystem::path path, bool truncateFile)
      : file_(std::fopen(path.string().c_str(), truncateFile ? "wb" : "r+b")),
        owned_(true), path_(std::move(path)), reopenable_(true) {
    initialize();
  }
  void initialize() {
    if (!file_)
      throw IoError(ErrorKind::Io,
                    path_.empty() ? "null FILE output" :
                                    "unable to open output: " + std::string(std::strerror(errno)),
                    path_);
    seekable_ = tellFile().has_value();
    if (!seekable_) std::clearerr(file_);
  }
  ~FileOutput() override { try { close(); } catch (...) {} }

  void write(const void *data, std::size_t size) override {
    requireOpen();
    if (size && std::fwrite(data, 1, size, file_) != size) ioFailure("FILE write failed");
  }
  SourceCapabilities capabilities() const noexcept override {
    return {false, true, seekable_, seekable_, true, seekable_, reopenable_};
  }
  std::uint64_t tell() const override {
    requireOpen();
    const auto value = tellFile();
    if (!value) ioFailure("FILE tell failed");
    return *value;
  }
  void seek(std::uint64_t offset) override {
    requireOpen();
    if (!seekable_) unsupported("seek");
#if defined(_WIN32)
    if (_fseeki64(file_, static_cast<__int64>(offset), SEEK_SET) != 0)
#else
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) ||
        fseeko(file_, static_cast<off_t>(offset), SEEK_SET) != 0)
#endif
      ioFailure("FILE seek failed");
  }
  void truncate(std::uint64_t length) override {
    requireOpen();
    flush();
#if defined(_WIN32)
    if (_chsize_s(_fileno(file_), length) != 0)
#else
    if (length > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) ||
        ftruncate(fileno(file_), static_cast<off_t>(length)) != 0)
#endif
      ioFailure("FILE truncate failed");
  }
  void flush() override {
    requireOpen();
    if (std::fflush(file_) != 0) ioFailure("FILE flush failed");
  }
  void sync() override {
    flush();
#if defined(_WIN32)
    if (_commit(_fileno(file_)) != 0)
#else
    if (::fsync(fileno(file_)) != 0)
#endif
      ioFailure("FILE sync failed");
  }
  void close() override {
    if (!file_) return;
    if (reopenable_) reopenOffset_ = tell();
    FILE *closing = file_;
    file_ = nullptr;
    if (owned_) {
      if (std::fclose(closing) != 0) ioFailure("FILE close failed");
    } else if (std::fflush(closing) != 0) {
      ioFailure("FILE flush failed");
    }
  }
  void reopen() override {
    if (!reopenable_) unsupported("reopen");
    if (file_) return;
    file_ = std::fopen(path_.string().c_str(), "r+b");
    if (!file_) ioFailure("FILE reopen failed");
    seekable_ = true;
    seek(reopenOffset_);
  }

 private:
  void requireOpen() const { if (!file_) unsupported("access after close"); }
  std::optional<std::uint64_t> tellFile() const {
#if defined(_WIN32)
    const auto value = _ftelli64(file_);
#else
    const auto value = ftello(file_);
#endif
    if (value < 0) return std::nullopt;
    return static_cast<std::uint64_t>(value);
  }
  FILE *file_ = nullptr;
  bool owned_ = false;
  bool seekable_ = false;
  std::filesystem::path path_;
  std::uint64_t reopenOffset_ = 0;
  bool reopenable_ = false;
};

class StreamInput final : public InputSource {
 public:
  explicit StreamInput(std::istream &stream) : stream_(&stream) {
    const auto position = stream_->tellg();
    seekable_ = position != std::istream::pos_type(-1);
    stream_->clear();
  }
  std::size_t read(void *data, std::size_t size) override {
    requireOpen();
    stream_->read(static_cast<char *>(data), static_cast<std::streamsize>(size));
    const auto count = stream_->gcount();
    if (stream_->bad()) throw IoError(ErrorKind::Io, "C++ stream read failed");
    return static_cast<std::size_t>(count);
  }
  bool eof() const override { return !stream_ || stream_->eof(); }
  SourceCapabilities capabilities() const noexcept override {
    return {true, false, seekable_, false, false, false, false};
  }
  std::uint64_t tell() const override {
    requireOpen();
    const auto value = stream_->tellg();
    if (value == std::istream::pos_type(-1)) throw IoError(ErrorKind::Io, "C++ stream tell failed");
    return static_cast<std::uint64_t>(value);
  }
  void seek(std::uint64_t offset) override {
    requireOpen();
    if (!seekable_) unsupported("seek");
    stream_->clear();
    stream_->seekg(static_cast<std::streamoff>(offset));
    if (!*stream_) throw IoError(ErrorKind::Io, "C++ stream seek failed");
  }
  void close() override { stream_ = nullptr; }

 private:
  void requireOpen() const { if (!stream_) unsupported("access after close"); }
  std::istream *stream_ = nullptr;
  bool seekable_ = false;
};

class StreamOutput final : public OutputSink {
 public:
  explicit StreamOutput(std::ostream &stream) : stream_(&stream) {
    const auto position = stream_->tellp();
    seekable_ = position != std::ostream::pos_type(-1);
    stream_->clear();
  }
  void write(const void *data, std::size_t size) override {
    requireOpen();
    stream_->write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
    if (!*stream_) throw IoError(ErrorKind::Io, "C++ stream write failed");
  }
  SourceCapabilities capabilities() const noexcept override {
    return {false, true, seekable_, false, true, false, false};
  }
  std::uint64_t tell() const override {
    requireOpen();
    const auto value = stream_->tellp();
    if (value == std::ostream::pos_type(-1)) throw IoError(ErrorKind::Io, "C++ stream tell failed");
    return static_cast<std::uint64_t>(value);
  }
  void seek(std::uint64_t offset) override {
    requireOpen();
    if (!seekable_) unsupported("seek");
    stream_->clear();
    stream_->seekp(static_cast<std::streamoff>(offset));
    if (!*stream_) throw IoError(ErrorKind::Io, "C++ stream seek failed");
  }
  void flush() override {
    requireOpen();
    stream_->flush();
    if (!*stream_) throw IoError(ErrorKind::Io, "C++ stream flush failed");
  }
  void close() override { if (stream_) flush(); stream_ = nullptr; }

 private:
  void requireOpen() const { if (!stream_) unsupported("access after close"); }
  std::ostream *stream_ = nullptr;
  bool seekable_ = false;
};

class MemoryInput final : public InputSource {
 public:
  MemoryInput(const void *data, std::size_t size)
      : data_(static_cast<const std::uint8_t *>(data)), size_(size) {
    if (!data_ && size_) throw StateError(ErrorKind::State, "null memory input");
  }
  std::size_t read(void *data, std::size_t size) override {
    requireOpen();
    const std::size_t count = std::min(size, size_ - position_);
    if (count) std::memcpy(data, data_ + position_, count);
    position_ += count;
    return count;
  }
  bool eof() const override { return !open_ || position_ == size_; }
  SourceCapabilities capabilities() const noexcept override {
    return {true, false, true, false, false, false, false};
  }
  std::uint64_t tell() const override { requireOpen(); return position_; }
  void seek(std::uint64_t offset) override {
    requireOpen();
    if (offset > size_) throw StateError(ErrorKind::State, "memory input seek is out of range");
    position_ = static_cast<std::size_t>(offset);
  }
  void close() override { open_ = false; }

 private:
  void requireOpen() const { if (!open_) unsupported("access after close"); }
  const std::uint8_t *data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t position_ = 0;
  bool open_ = true;
};

class MemoryOutput final : public OutputSink {
 public:
  explicit MemoryOutput(std::vector<std::uint8_t> &data) : data_(&data) {}
  void write(const void *data, std::size_t size) override {
    requireOpen();
    if (size > std::numeric_limits<std::size_t>::max() - position_)
      throw LimitError(ErrorKind::Limit, "memory output size overflow");
    if (data_->size() < position_ + size) data_->resize(position_ + size);
    if (size) std::memcpy(data_->data() + position_, data, size);
    position_ += size;
  }
  SourceCapabilities capabilities() const noexcept override {
    return {false, true, true, true, true, true, false};
  }
  std::uint64_t tell() const override { requireOpen(); return position_; }
  void seek(std::uint64_t offset) override {
    requireOpen();
    if (offset > data_->size()) throw StateError(ErrorKind::State, "memory output seek is out of range");
    position_ = static_cast<std::size_t>(offset);
  }
  void truncate(std::uint64_t length) override {
    requireOpen();
    if (length > std::numeric_limits<std::size_t>::max())
      throw LimitError(ErrorKind::Limit, "memory output size overflow");
    data_->resize(static_cast<std::size_t>(length));
    position_ = std::min(position_, data_->size());
  }
  void flush() override { requireOpen(); }
  void sync() override { requireOpen(); }
  void close() override { open_ = false; }

 private:
  void requireOpen() const { if (!open_) unsupported("access after close"); }
  std::vector<std::uint8_t> *data_ = nullptr;
  std::size_t position_ = 0;
  bool open_ = true;
};

}  // namespace

std::uint64_t InputSource::tell() const { unsupported("tell"); }
void InputSource::seek(std::uint64_t) { unsupported("seek"); }
void InputSource::reopen() { unsupported("reopen"); }
std::uint64_t OutputSink::tell() const { unsupported("tell"); }
void OutputSink::seek(std::uint64_t) { unsupported("seek"); }
void OutputSink::truncate(std::uint64_t) { unsupported("truncate"); }
void OutputSink::sync() { unsupported("sync"); }
void OutputSink::reopen() { unsupported("reopen"); }

std::unique_ptr<InputSource> inputFromPath(const std::filesystem::path &path) {
  return std::make_unique<FileInput>(path);
}
std::unique_ptr<InputSource> inputFromStdin() {
  return std::make_unique<FileInput>(stdin, false);
}

std::unique_ptr<InputSource> inputFromFile(FILE *file, bool takeOwnership) {
  return std::make_unique<FileInput>(file, takeOwnership);
}
std::unique_ptr<InputSource> inputFromStream(std::istream &stream) {
  return std::make_unique<StreamInput>(stream);
}
std::unique_ptr<InputSource> inputFromMemory(const void *data, std::size_t size) {
  return std::make_unique<MemoryInput>(data, size);
}
std::unique_ptr<OutputSink> outputToPath(const std::filesystem::path &path, bool truncate) {
  return std::make_unique<FileOutput>(path, truncate);
}
std::unique_ptr<OutputSink> outputToStdout() {
  return std::make_unique<FileOutput>(stdout, false);
}
std::unique_ptr<OutputSink> outputToFile(FILE *file, bool takeOwnership) {
  return std::make_unique<FileOutput>(file, takeOwnership);
}
std::unique_ptr<OutputSink> outputToStream(std::ostream &stream) {
  return std::make_unique<StreamOutput>(stream);
}
std::unique_ptr<OutputSink> outputToMemory(std::vector<std::uint8_t> &data) {
  return std::make_unique<MemoryOutput>(data);
}

}  // namespace sdds
