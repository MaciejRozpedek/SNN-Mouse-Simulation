#include "SNNParseException.hpp"

SNNParseException::SNNParseException(const std::string& message, const YAML::Node& node)
    : std::runtime_error(buildErrorMessage(message, node)) {}

SNNParseException::SNNParseException(const std::string& message)
    : std::runtime_error(message) {}

std::string SNNParseException::buildErrorMessage(const std::string& message, const YAML::Node& node) {
    if (node.Mark().line > 0) {
        return "Blad parsowania (linia: " + std::to_string(node.Mark().line + 1) +
               ", kolumna: " + std::to_string(node.Mark().column + 1) + "): " + message;
    }
    return "Blad parsowania: " + message;
}