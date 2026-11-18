#pragma once

#include <cstddef>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace easyml {

struct SourceLocation {
    std::string source;
    std::size_t line{0};
};

struct Condition {
    std::string lhs;
    std::string comparison;
    std::string rhs;
};

struct Node {
    enum class Kind { Instruction, For, If };

    Kind kind{Kind::Instruction};
    SourceLocation location;
    std::string text;
    std::string variable;
    long long first{0};
    long long last{0};
    long long step{1};
    Condition condition;
    std::vector<Node> body;
    std::vector<Node> elseBody;
};

class ProgramHost {
public:
    virtual ~ProgramHost() = default;

    /**
     * Return isolated host state for validating one possible IF branch. The
     * original host is updated only by mergeValidatedBranches after both pass.
     */
    virtual std::unique_ptr<ProgramHost> cloneForValidation() const = 0;
    /** Validate one expanded domain-specific instruction without executing hardware. */
    virtual void validateInstruction(const std::string& instruction,
                                    const SourceLocation& location) = 0;
    /** Execute one instruction; hosts should keep validation side-effect free. */
    virtual void executeInstruction(const std::string& instruction,
                                    const SourceLocation& location,
                                    std::ostream& output) = 0;
    virtual void validateIdentifier(const std::string& identifier,
                                    const SourceLocation& location) const = 0;
    virtual double resolveIdentifier(const std::string& identifier,
                                     const SourceLocation& location) = 0;
    /** Merge branch state after both possible paths have been validated. */
    virtual void mergeValidatedBranches(const ProgramHost& trueBranch,
                                        const ProgramHost& falseBranch) = 0;
    virtual void conditionEvaluated(const std::string& expression,
                                    const SourceLocation& location,
                                    bool result,
                                    std::ostream& output);
};

class Program {
public:
    /** Parse line-oriented instructions and nested FOR/IF blocks, preserving source locations. */
    static Program parse(std::istream& input, const std::string& source);

    Program(std::string source, std::vector<Node> nodes);
    const std::string& source() const noexcept;
    const std::vector<Node>& nodes() const noexcept;

    /** Validate expanded instructions and both conditional branches without hardware execution. */
    void validate(ProgramHost& host) const;
    /** Execute instructions; IF conditions select one branch at runtime. */
    void execute(ProgramHost& host, std::ostream& output) const;

private:
    std::string source_;
    std::vector<Node> nodes_;
};

} // namespace easyml
