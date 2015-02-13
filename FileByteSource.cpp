#include "FileByteSource.h"

#include <algorithm>
#include <fcntl.h>
#include <glog/logging.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace facebook {
namespace wdt {

folly::ThreadLocalPtr<FileByteSource::Buffer> FileByteSource::buffer_;

FileByteSource::FileByteSource(FileMetaData *fileData, uint64_t size,
                               uint64_t offset, size_t bufferSize)
    : fileData_(fileData),
      size_(size),
      offset_(offset),
      bytesRead_(0),
      bufferSize_(bufferSize) {
  transferStats_.setId(getIdentifier());
}

ErrorCode FileByteSource::open() {
  bytesRead_ = 0;
  this->close();

  ErrorCode errCode = OK;
  if (!buffer_ || bufferSize_ > buffer_->size_) {
    buffer_.reset(new Buffer(bufferSize_));
  }
  const std::string &fullPath = fileData_->getFullPath();
  fd_ = ::open(fullPath.c_str(), O_RDONLY);
  if (fd_ < 0) {
    errCode = BYTE_SOURCE_READ_ERROR;
    PLOG(ERROR) << "error opening file " << fullPath;
  } else if (offset_ > 0 && lseek(fd_, offset_, SEEK_SET) < 0) {
    errCode = BYTE_SOURCE_READ_ERROR;
    PLOG(ERROR) << "error seeking file " << fullPath;
  }
  transferStats_.setErrorCode(errCode);
  return errCode;
}

char *FileByteSource::read(size_t &size) {
  size = 0;
  if (hasError() || finished()) {
    return nullptr;
  }
  size_t toRead =
      (size_t)std::min<uint64_t>(buffer_->size_, size_ - bytesRead_);
  ssize_t numRead = ::read(fd_, buffer_->data_, toRead);
  if (numRead < 0) {
    PLOG(ERROR) << "failure while reading file " << fileData_->getFullPath();
    this->close();
    transferStats_.setErrorCode(BYTE_SOURCE_READ_ERROR);
    return nullptr;
  }
  if (numRead == 0) {
    this->close();
    return nullptr;
  }
  bytesRead_ += numRead;
  size = numRead;
  return buffer_->data_;
}
}
}
