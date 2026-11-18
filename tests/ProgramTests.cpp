#include "EasyMachineLanguage.hpp"

#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class TestHost final : public easyml::ProgramHost {
public:
    std::unique_ptr<easyml::ProgramHost> cloneForValidation() const override {
        return std::make_unique<TestHost>(*this);
    }

    void validateInstruction(const std::string& instruction,
                             const easyml::SourceLocation&) override {
        validated->push_back(instruction);
    }

    void executeInstruction(const std::string& instruction,
                            const easyml::SourceLocation&,
                            std::ostream& output) override {
        executed.push_back(instruction);
        output << instruction << '\n';
    }

    void validateIdentifier(const std::string& identifier,
                            const easyml::SourceLocation& location) const override {
        if (identifier != "position") {
            throw std::runtime_error(location.source + ":" + std::to_string(location.line) +
                                     ": unknown identifier: " + identifier);
        }
    }

    double resolveIdentifier(const std::string& identifier,
                             const easyml::SourceLocation&) override {
        if (identifier != "position") throw std::runtime_error("unknown identifier");
        return position;
    }

    void mergeValidatedBranches(const easyml::ProgramHost&,
                                const easyml::ProgramHost&) override {}

    void conditionEvaluated(const std::string& expression,
                            const easyml::SourceLocation&,
                            bool result,
                            std::ostream& output) override {
        output << expression << " -> " << (result ? "TRUE" : "FALSE") << '\n';
    }

    double position{2.0};
    std::shared_ptr<std::vector<std::string>> validated{
        std::make_shared<std::vector<std::string>>()};
    std::vector<std::string> executed;
};

void testLoopAndConditionalExecution() {
    std::istringstream source(
        "FOR i = 3 TO 1\n"
        "IF i <= 2\n"
        "EMIT {i}\n"
        "ELSE\n"
        "SKIP {i}\n"
        "END\n"
        "END\n"
        "IF position >= 2\n"
        "DONE\n"
        "END\n");
    easyml::Program program = easyml::Program::parse(source, "test.eml");
    TestHost validator;
    program.validate(validator);
    require(validator.validated->size() == 7,
            "validation should expand all loop iterations and both IF branches");

    TestHost host;
    std::ostringstream output;
    program.execute(host, output);
    require(host.executed == std::vector<std::string>{
                                 "SKIP 3", "EMIT 2", "EMIT 1", "DONE"},
            "execution should expand counters and select the evaluated branches");
}

void testNestedLoopsAndVariableScope() {
    std::istringstream source(
        "FOR i = 1 TO 2\n"
        "FOR i = 4 TO 5\n"
        "EMIT {i}\n"
        "END\n"
        "EMIT {i}\n"
        "END\n");
    easyml::Program program = easyml::Program::parse(source, "nested.eml");
    TestHost host;
    std::ostringstream output;
    program.execute(host, output);
    require(host.executed == std::vector<std::string>{"EMIT 4", "EMIT 5", "EMIT 1",
                                                       "EMIT 4", "EMIT 5", "EMIT 2"},
            "nested loop variables should restore the outer counter");
}

void testExplicitLoopStep() {
    std::istringstream source(
        "FOR i = 100 TO 300 STEP 50\n"
        "EMIT {i}\n"
        "END\n"
        "FOR j = 300 TO 100 STEP 50\n"
        "EMIT {j}\n"
        "END\n"
        "FOR k = 1 TO 6 STEP 2\n"
        "EMIT {k}\n"
        "END\n"
        "FOR extreme = -9223372036854775808 TO 9223372036854775807 STEP "
        "9223372036854775807\n"
        "EMIT {extreme}\n"
        "END\n");
    const auto program = easyml::Program::parse(source, "stepped.eml");
    TestHost host;
    std::ostringstream output;
    program.execute(host, output);
    require(host.executed == std::vector<std::string>{
                                 "EMIT 100", "EMIT 150", "EMIT 200", "EMIT 250", "EMIT 300",
                                 "EMIT 300", "EMIT 250", "EMIT 200", "EMIT 150", "EMIT 100",
                                 "EMIT 1", "EMIT 3", "EMIT 5",
                                 "EMIT -9223372036854775808", "EMIT -1",
                                 "EMIT 9223372036854775806"},
            "FOR STEP should advance in both directions without passing the end value");
}

void testInvalidLoopSteps() {
    for (const std::string step : {"0", "-1"}) {
        std::istringstream source("FOR i = 1 TO 3 STEP " + step + "\nEMIT {i}\nEND\n");
        try {
            (void)easyml::Program::parse(source, "invalid-step.eml");
        } catch (const std::exception& error) {
            require(std::string(error.what()).find("FOR step must be a positive integer") !=
                        std::string::npos,
                    "invalid loop steps should explain the positive-step requirement");
            continue;
        }
        throw std::runtime_error("zero and negative FOR steps should fail");
    }
}

void testInvalidBlocksAndLoopLimit() {
    std::istringstream missingEnd("IF 1 == 1\nEMIT 1\n");
    bool missingEndFailed = false;
    try {
        easyml::Program::parse(missingEnd, "missing.eml");
    } catch (const std::exception& error) {
        require(std::string(error.what()).find("missing.eml:1") != std::string::npos,
                "block syntax failures should include the source location");
        missingEndFailed = true;
    }
    require(missingEndFailed, "missing END should fail");

    std::istringstream oversized("FOR i = 1 TO 10001\nEMIT {i}\nEND\n");
    easyml::Program program = easyml::Program::parse(oversized, "large.eml");
    TestHost host;
    try {
        program.validate(host);
    } catch (const std::exception& error) {
        require(std::string(error.what()).find("10000-iteration limit") != std::string::npos,
                "oversized loops should be rejected");
        return;
    }
    throw std::runtime_error("oversized FOR should fail");
}

} // namespace

int main() {
    try {
        testLoopAndConditionalExecution();
        testNestedLoopsAndVariableScope();
        testExplicitLoopStep();
        testInvalidLoopSteps();
        testInvalidBlocksAndLoopLimit();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
