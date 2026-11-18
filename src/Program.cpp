#include "EasyMachineLanguage.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <istream>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace easyml {
namespace {

constexpr std::size_t kMaximumLoopIterations = 10000;
constexpr std::size_t kMaximumExpandedStatements = 100000;
constexpr std::size_t kMaximumBlockDepth = 128;

std::string locationText(const SourceLocation& location) {
    return location.source + ":" + std::to_string(location.line);
}

std::runtime_error programError(const SourceLocation& location,
                                const std::string& detail) {
    return std::runtime_error(locationText(location) + ": " + detail);
}

template <typename Function>
decltype(auto) invokeAtLocation(const SourceLocation& location, Function&& function) {
    try {
        return function();
    } catch (const std::exception& error) {
        const std::string prefix = locationText(location) + ":";
        if (std::string(error.what()).compare(0, prefix.size(), prefix) == 0) throw;
        throw programError(location, error.what());
    }
}

std::vector<std::string> tokenize(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) tokens.push_back(token);
    return tokens;
}

bool isIdentifier(const std::string& value) {
    return !value.empty() &&
           (std::isalpha(static_cast<unsigned char>(value.front())) != 0 ||
            value.front() == '_') &&
           std::all_of(value.begin() + 1, value.end(), [](unsigned char ch) {
               return std::isalnum(ch) != 0 || ch == '_' || ch == '.';
           });
}

long long parseInteger(const std::string& token, const SourceLocation& location) {
    std::size_t parsed = 0;
    long long value = 0;
    try {
        value = std::stoll(token, &parsed);
    } catch (const std::exception&) {
        throw programError(location, "FOR values and step must be integers");
    }
    if (parsed != token.size()) {
        throw programError(location, "FOR bounds must be integers");
    }
    return value;
}

struct ParsedBlock {
    std::vector<Node> nodes;
    std::string terminator;
    SourceLocation terminatorLocation;
};

class Parser {
public:
    Parser(const std::vector<std::pair<std::size_t, std::string>>& lines,
           std::string source)
        : lines_(lines), source_(std::move(source)) {}

    std::vector<Node> parse() {
        ParsedBlock block = parseBlock(0);
        if (!block.terminator.empty()) {
            throw programError(block.terminatorLocation,
                               block.terminator + " without matching block");
        }
        return std::move(block.nodes);
    }

private:
    ParsedBlock parseBlock(std::size_t depth) {
        // Recursively consume nested blocks and return END/ELSE to the caller that owns them.
        ParsedBlock block;
        while (index_ < lines_.size()) {
            const auto& line = lines_[index_];
            const SourceLocation location{source_, line.first};
            const auto tokens = tokenize(line.second);
            if (tokens.empty()) {
                ++index_;
                continue;
            }
            if (tokens[0] == "END" || tokens[0] == "ELSE") {
                if (tokens.size() != 1) {
                    throw programError(location, tokens[0] + " takes no arguments");
                }
                block.terminator = tokens[0];
                block.terminatorLocation = location;
                ++index_;
                return block;
            }

            Node node;
            node.location = location;
            node.text = line.second;
            if (tokens[0] == "FOR") {
                if (depth >= kMaximumBlockDepth) {
                    throw programError(location, "program block nesting exceeds 128 levels");
                }
                if ((tokens.size() != 6 && tokens.size() != 8) ||
                    !isIdentifier(tokens[1]) || tokens[2] != "=" || tokens[4] != "TO" ||
                    (tokens.size() == 8 && tokens[6] != "STEP")) {
                    throw programError(location,
                                       "expected: FOR <variable> = <integer> TO <integer> "
                                       "[STEP <positive integer>]");
                }
                node.kind = Node::Kind::For;
                node.variable = tokens[1];
                node.first = parseInteger(tokens[3], location);
                node.last = parseInteger(tokens[5], location);
                if (tokens.size() == 8) {
                    node.step = parseInteger(tokens[7], location);
                    if (node.step <= 0) {
                        throw programError(location, "FOR step must be a positive integer");
                    }
                }
                ++index_;
                ParsedBlock body = parseBlock(depth + 1);
                if (body.terminator != "END") {
                    throw programError(location, "FOR block is missing END");
                }
                node.body = std::move(body.nodes);
            } else if (tokens[0] == "IF") {
                if (depth >= kMaximumBlockDepth) {
                    throw programError(location, "program block nesting exceeds 128 levels");
                }
                if (tokens.size() != 4) {
                    throw programError(location,
                                       "expected: IF <value> <comparison> <value>");
                }
                const std::string& comparison = tokens[2];
                if (comparison != "<" && comparison != "<=" && comparison != "==" &&
                    comparison != "!=" && comparison != ">=" && comparison != ">") {
                    throw programError(location, "unsupported IF comparison: " + comparison);
                }
                node.kind = Node::Kind::If;
                node.condition = {tokens[1], comparison, tokens[3]};
                ++index_;
                ParsedBlock trueBlock = parseBlock(depth + 1);
                node.body = std::move(trueBlock.nodes);
                if (trueBlock.terminator == "ELSE") {
                    ParsedBlock falseBlock = parseBlock(depth + 1);
                    if (falseBlock.terminator != "END") {
                        throw programError(location, "IF block is missing END");
                    }
                    node.elseBody = std::move(falseBlock.nodes);
                } else if (trueBlock.terminator != "END") {
                    throw programError(location, "IF block is missing END");
                }
            }
            block.nodes.push_back(std::move(node));
            if (block.nodes.back().kind == Node::Kind::Instruction) ++index_;
        }
        return block;
    }

    const std::vector<std::pair<std::size_t, std::string>>& lines_;
    std::string source_;
    std::size_t index_{0};
};

void consumeStep(std::size_t& count, const SourceLocation& location) {
    if (++count > kMaximumExpandedStatements) {
        throw programError(location, "program expansion exceeds 100000 statements");
    }
}

std::uint64_t distanceBetween(long long lhs, long long rhs) {
    if (lhs <= rhs) {
        return static_cast<std::uint64_t>(rhs) - static_cast<std::uint64_t>(lhs);
    }
    return static_cast<std::uint64_t>(lhs) - static_cast<std::uint64_t>(rhs);
}

std::size_t loopCount(long long first,
                      long long last,
                      long long step,
                      const SourceLocation& location) {
    if (step <= 0) {
        throw programError(location, "FOR step must be a positive integer");
    }
    const auto intervals = distanceBetween(first, last) / static_cast<std::uint64_t>(step);
    if (intervals >= kMaximumLoopIterations) {
        throw programError(location, "FOR range exceeds the 10000-iteration limit");
    }
    return static_cast<std::size_t>(intervals + 1);
}

std::string substituteVariables(const std::string& text,
                                const std::map<std::string, long long>& variables,
                                const SourceLocation& location) {
    std::string result;
    for (std::size_t index = 0; index < text.size();) {
        if (text[index] != '{') {
            if (text[index] == '}') {
                throw programError(location, "unmatched '}' in variable expression");
            }
            result += text[index++];
            continue;
        }
        const std::size_t end = text.find('}', index + 1);
        if (end == std::string::npos) {
            throw programError(location, "unmatched '{' in variable expression");
        }
        const std::string name = text.substr(index + 1, end - index - 1);
        const auto variable = variables.find(name);
        if (!isIdentifier(name) || variable == variables.end()) {
            throw programError(location, "unknown loop variable: " + name);
        }
        result += std::to_string(variable->second);
        index = end + 1;
    }
    return result;
}

bool parseNumber(const std::string& token, double& result) {
    std::size_t parsed = 0;
    try {
        result = std::stod(token, &parsed);
    } catch (const std::exception&) {
        return false;
    }
    return parsed == token.size() && std::isfinite(result);
}

void validateOperand(const std::string& operand,
                     const std::map<std::string, long long>& variables,
                     ProgramHost& host,
                     const SourceLocation& location) {
    if (variables.find(operand) != variables.end()) return;
    double number = 0.0;
    if (parseNumber(operand, number)) return;
    if (!isIdentifier(operand)) {
        throw programError(location, "invalid IF operand: " + operand);
    }
    invokeAtLocation(location, [&] { host.validateIdentifier(operand, location); });
}

double resolveOperand(const std::string& operand,
                      const std::map<std::string, long long>& variables,
                      ProgramHost& host,
                      const SourceLocation& location) {
    const auto variable = variables.find(operand);
    if (variable != variables.end()) return static_cast<double>(variable->second);
    double number = 0.0;
    if (parseNumber(operand, number)) return number;
    if (!isIdentifier(operand)) {
        throw programError(location, "invalid IF operand: " + operand);
    }
    const double result = invokeAtLocation(location, [&] {
        return host.resolveIdentifier(operand, location);
    });
    if (!std::isfinite(result)) {
        throw programError(location, "IF values must be finite");
    }
    return result;
}

bool evaluate(const Condition& condition,
              const std::map<std::string, long long>& variables,
              ProgramHost& host,
              const SourceLocation& location) {
    const double lhs = resolveOperand(condition.lhs, variables, host, location);
    const double rhs = resolveOperand(condition.rhs, variables, host, location);
    if (condition.comparison == "<") return lhs < rhs;
    if (condition.comparison == "<=") return lhs <= rhs;
    if (condition.comparison == "==") return lhs == rhs;
    if (condition.comparison == "!=") return lhs != rhs;
    if (condition.comparison == ">=") return lhs >= rhs;
    return lhs > rhs;
}

std::string conditionText(const Condition& condition) {
    return condition.lhs + " " + condition.comparison + " " + condition.rhs;
}

template <typename Function>
void forEachLoopValue(long long first,
                      long long last,
                      long long step,
                      Function&& callback) {
    const bool ascending = first <= last;
    const auto stride = static_cast<std::uint64_t>(step);
    long long value = first;
    for (;;) {
        callback(value);
        const auto remaining = distanceBetween(value, last);
        if (stride > remaining) break;
        if (ascending) {
            value += step;
        } else {
            value -= step;
        }
    }
}

void validateNodes(const std::vector<Node>& nodes,
                   ProgramHost& host,
                   std::map<std::string, long long>& variables,
                   std::size_t& steps) {
    for (const Node& node : nodes) {
        consumeStep(steps, node.location);
        if (node.kind == Node::Kind::Instruction) {
            const std::string instruction =
                substituteVariables(node.text, variables, node.location);
            invokeAtLocation(node.location, [&] {
                host.validateInstruction(instruction, node.location);
            });
            continue;
        }
        if (node.kind == Node::Kind::For) {
            loopCount(node.first, node.last, node.step, node.location);
            const auto previous = variables.find(node.variable);
            const bool hadPrevious = previous != variables.end();
            const long long oldValue = hadPrevious ? previous->second : 0;
            forEachLoopValue(node.first, node.last, node.step, [&](long long value) {
                variables[node.variable] = value;
                validateNodes(node.body, host, variables, steps);
            });
            if (hadPrevious) variables[node.variable] = oldValue;
            else variables.erase(node.variable);
            continue;
        }

        validateOperand(node.condition.lhs, variables, host, node.location);
        validateOperand(node.condition.rhs, variables, host, node.location);
        // Both branches are checked because runtime values are unknown before execution.
        std::unique_ptr<ProgramHost> trueBranch =
            invokeAtLocation(node.location, [&] { return host.cloneForValidation(); });
        std::unique_ptr<ProgramHost> falseBranch =
            invokeAtLocation(node.location, [&] { return host.cloneForValidation(); });
        if (!trueBranch || !falseBranch) {
            throw programError(node.location, "host failed to clone validation state");
        }
        validateNodes(node.body, *trueBranch, variables, steps);
        validateNodes(node.elseBody, *falseBranch, variables, steps);
        invokeAtLocation(node.location, [&] {
            // Host-specific merging rejects branches that leave incompatible machine state.
            host.mergeValidatedBranches(*trueBranch, *falseBranch);
        });
    }
}

void executeNodes(const std::vector<Node>& nodes,
                  ProgramHost& host,
                  std::map<std::string, long long>& variables,
                  std::ostream& output,
                  std::size_t& steps) {
    for (const Node& node : nodes) {
        consumeStep(steps, node.location);
        if (node.kind == Node::Kind::Instruction) {
            const std::string instruction =
                substituteVariables(node.text, variables, node.location);
            invokeAtLocation(node.location, [&] {
                host.executeInstruction(instruction, node.location, output);
            });
            continue;
        }
        if (node.kind == Node::Kind::For) {
            loopCount(node.first, node.last, node.step, node.location);
            const auto previous = variables.find(node.variable);
            const bool hadPrevious = previous != variables.end();
            const long long oldValue = hadPrevious ? previous->second : 0;
            forEachLoopValue(node.first, node.last, node.step, [&](long long value) {
                variables[node.variable] = value;
                executeNodes(node.body, host, variables, output, steps);
            });
            if (hadPrevious) variables[node.variable] = oldValue;
            else variables.erase(node.variable);
            continue;
        }
        const bool result = evaluate(node.condition, variables, host, node.location);
        invokeAtLocation(node.location, [&] {
            host.conditionEvaluated(conditionText(node.condition), node.location, result, output);
        });
        // Validation covers both paths; only the branch selected by live values executes.
        executeNodes(result ? node.body : node.elseBody, host, variables, output, steps);
    }
}

} // namespace

void ProgramHost::conditionEvaluated(const std::string&,
                                     const SourceLocation&,
                                     bool,
                                     std::ostream&) {}

Program::Program(std::string source, std::vector<Node> nodes)
    : source_(std::move(source)), nodes_(std::move(nodes)) {}

Program Program::parse(std::istream& input, const std::string& source) {
    std::vector<std::pair<std::size_t, std::string>> lines;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        // Preserve source line numbers while stripping EML's two comment styles.
        const std::size_t comment = line.find_first_of(";#");
        if (comment != std::string::npos) line.erase(comment);
        const std::size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        const std::size_t last = line.find_last_not_of(" \t\r\n");
        lines.emplace_back(lineNumber, line.substr(first, last - first + 1));
    }
    if (input.bad()) {
        throw std::runtime_error("Error reading program from " + source);
    }
    return Program(source, Parser(lines, source).parse());
}

const std::string& Program::source() const noexcept {
    return source_;
}

const std::vector<Node>& Program::nodes() const noexcept {
    return nodes_;
}

void Program::validate(ProgramHost& host) const {
    std::map<std::string, long long> variables;
    std::size_t steps = 0;
    validateNodes(nodes_, host, variables, steps);
}

void Program::execute(ProgramHost& host, std::ostream& output) const {
    std::map<std::string, long long> variables;
    std::size_t steps = 0;
    executeNodes(nodes_, host, variables, output, steps);
}

} // namespace easyml
