#ifndef SNN_PARSE_EXCEPTION_HPP
#define SNN_PARSE_EXCEPTION_HPP

#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>

class SNNParseException : public std::runtime_error {
public:
    SNNParseException(const std::string& message, const YAML::Node& node);
    SNNParseException(const std::string& message);

private:
    static std::string buildErrorMessage(const std::string& message, const YAML::Node& node);
};

#endif // SNN_PARSE_EXCEPTION_HPP