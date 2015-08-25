#ifndef LOGGER_H
#define LOGGER_H

#include "event.h"

namespace nest
{

class RecordingDevice;

class Logger
{
public:
  Logger() {}
  virtual ~Logger() throw() {};

  virtual void enroll( const int virtual_process, RecordingDevice& device ) = 0;

  virtual void initialize() = 0;
  virtual void finalize() = 0;

  virtual void write_event( const RecordingDevice& device, const Event& event ) = 0;
  virtual void write_value( const RecordingDevice& device, const double& value ) = 0;
  virtual void write_end( const RecordingDevice& device ) = 0;
};

} // namespace

#endif // LOGGER_H
