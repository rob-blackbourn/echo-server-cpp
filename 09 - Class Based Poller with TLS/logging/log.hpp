#ifndef JETBLACK_LOGGING_LOG_HPP
#define JETBLACK_LOGGING_LOG_HPP

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fmt/format.h>
#include <format>
#include <map>
#include <mutex>
#include <source_location>
#include <string>
#include <utility>

namespace jetblack::logging {

  enum class Level
  {
    NONE     = 0,
    CRITICAL = 1,
    ERROR    = 2,
    WARNING  = 3,
    INFO     = 4,
    DEBUG    = 5,
    TRACE    = 6
  };

  namespace
  {
    static const std::string root_logger_name = "root";

    std::string to_string(Level level)
    {
      switch (level)
      {
      case Level::NONE:
        return "NONE";
      case Level::CRITICAL:
        return "CRITICAL";
      case Level::ERROR:
        return "ERROR";
      case Level::WARNING:
        return "WARNING";
      case Level::INFO:
        return "INFO";
      case Level::DEBUG:
        return "DEBUG";
      case Level::TRACE:
        return "TRACE";
      }

      std::unreachable();
    }

    Level parse_level_or(std::string& name, Level default_level)
    {
      if (name == "NAME")
        return Level::NONE;
      if (name == "CRITICAL")
        return Level::CRITICAL;
      if (name == "ERROR")
        return Level::ERROR;
      if (name == "WARNING")
        return Level::WARNING;
      if (name == "INFO")
        return Level::INFO;
      if (name == "DEBUG")
        return Level::DEBUG;
      if (name == "TRACE")
        return Level::TRACE;
      return default_level;
    }

    Level env_level_or(const std::string& logger_name, Level default_level)
    {
      // Check the environment variable "LOGGER_LEVEL" and "LOGGER_LEVEL_<name>".
      auto base_env_name = std::string("LOGGER_LEVEL");
      auto logger_env_name = std::format("{}_{}", base_env_name, logger_name);

      auto env_level = std::getenv(logger_env_name.c_str());
      if (env_level == nullptr)
        env_level = std::getenv(base_env_name.c_str());

      if (env_level == nullptr)
        return default_level;
      auto level_string = std::string(env_level);
      return parse_level_or(level_string, default_level);
    }
  }

  struct LogRecord
  {
    std::chrono::local_time<std::chrono::nanoseconds> time;
    std::string name;
    Level level;
    std::source_location loc;
    std::string msg;
  };

  class LogHandler
  {
  public:
    virtual ~LogHandler() {}

    virtual void emit(const LogRecord& log_record) = 0;
  };

  class StreamLogHandler : public LogHandler
  {
  private:
    FILE* stream_;
  public:
    StreamLogHandler(FILE* stream = stderr)
      : stream_(stream)
    {
    }

    void emit(const LogRecord& log_record) override
    {
        auto line = std::format(
          "{:%Y-%m-%d %X} {} {} {} ({}, {})\n",
          log_record.time,
          to_string(log_record.level),
          log_record.loc.function_name(),
          log_record.msg,
          log_record.loc.file_name(),
          log_record.loc.line());
        fputs(line.c_str(), stream_);
    }
  };

  class Logger
  {
  private:
    std::string name_;
    Level level_;
    std::shared_ptr<LogHandler> log_handler_;
    std::mutex key_;

  public:
    Logger() {}
    Logger(const std::string& name, Level level, std::shared_ptr<LogHandler> log_handler)
      : name_(name),
      level_(level),
      log_handler_(log_handler)
    {
    }
    Logger(const Logger& other)
      : name_(other.name_),
        level_(other.level_),
        log_handler_(other.log_handler_)
    {
    }
    Logger& operator = (const Logger& other)
    {
      this->name_ = other.name_;
      this->level_ = other.level_;
      this->log_handler_ = other.log_handler_;
      return *this;
    }

    const std::string& name() const noexcept { return name_; }

    Level level() const noexcept { return level_; }
    void level(Level level) noexcept { level_ = level; }

    void log(Level level, const std::string& message, std::source_location loc)
    {
      if (static_cast<int>(level) <= static_cast<int>(level_))
      {
        std::scoped_lock lock(key_);

        auto log_record = LogRecord
        {
          .time = std::chrono::current_zone()->to_local(std::chrono::system_clock::now()),
          .name = name_,
          .level = level,
          .loc = loc,
          .msg = message
        };
        log_handler_->emit(log_record);
      }
    }

    void trace(std::string message, std::source_location loc = std::source_location::current())
    {
      log(Level::TRACE, message, loc);
    }
    
    void debug(std::string message, std::source_location loc = std::source_location::current())
    {
      log(Level::DEBUG, message, loc);
    }
    
    void info(std::string message, std::source_location loc = std::source_location::current())
    {
      log(Level::INFO, message, loc);
    }
    
    void warning(std::string message, std::source_location loc = std::source_location::current())
    {
      log(Level::WARNING, message, loc);
    }
    
    void error(std::string message, std::source_location loc = std::source_location::current())
    {
      log(Level::ERROR, message, loc);
    }
    
    void critical(std::string message, std::source_location loc = std::source_location::current())
    {
      log(Level::CRITICAL, message, loc);
    }
  };

  namespace
  {
    class LogManager
    {
    private:
      LogManager()
      {
      }

    public:
      static Logger& get(const std::string& name = root_logger_name)
      {
        static LogManager instance;
        return instance.logger(name);
      }

    private:
      std::map<std::string, Logger> loggers_;

      Logger& logger(const std::string& name)
      {
        auto i = loggers_.find(name);
        if (i == loggers_.end())
        {
          auto level = env_level_or(name, Level::INFO);
          auto logger = Logger(name, level, std::make_shared<StreamLogHandler>(stderr));
          loggers_[name] = logger;
        }
        return loggers_[name];
      }
    };
  }

  Logger& logger(const std::string& name = root_logger_name)
  {
    return LogManager::get(name);
  }

  Level level() noexcept
  {
    return LogManager::get().level();
  }

  void level(Level l) noexcept
  {
    LogManager::get().level(l);
  }
  
  inline void
  log(Level level, std::string message, std::source_location loc = std::source_location::current())
  {
    LogManager::get().log(level, message, loc);
  }
  
  inline void
  trace(std::string message, std::source_location loc = std::source_location::current())
  {
    LogManager::get().trace(message, loc);
  }
  
  inline void
  debug(std::string message, std::source_location loc = std::source_location::current())
  {
    LogManager::get().debug(message, loc);
  }
  
  inline void
  info(std::string message, std::source_location loc = std::source_location::current())
  {
    LogManager::get().info(message, loc);
  }
  
  inline void
  warning(std::string message, std::source_location loc = std::source_location::current())
  {
    LogManager::get().warning(message, loc);
  }
  
  inline void
  error(std::string message, std::source_location loc = std::source_location::current())
  {
    LogManager::get().error(message, loc);
  }
  
  inline void
  critical(std::string message, std::source_location loc = std::source_location::current())
  {
    LogManager::get().critical(message, loc);
  }
}

#endif // JETBLACK_LOGGING_LOG_HPP
