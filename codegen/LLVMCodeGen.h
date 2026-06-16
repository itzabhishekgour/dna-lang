#pragma once

#include "ir/IRProgram.h"
#include "LLVMContextManager.h"
#include "LLVMValueMapper.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <string>
#include <unordered_map>
#include <ostream>

namespace DNA {

class LLVMCodeGen {
private:
    LLVMContextManager ctx_;
    LLVMValueMapper valMapper_;
    std::unordered_map<std::string, llvm::Function*> functions_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> funcLocalVars_;

    // Helpers
    bool isInteger(const std::string& s) const;
    bool isTempName(const std::string& s) const;
    llvm::Value* resolveOperand(const std::string& operand);
    void codegenFunction(const IRFunction& fn);
    void codegenInstruction(const IRInstruction& instr, llvm::Function* currentFunc, std::unordered_map<std::string, llvm::BasicBlock*>& labelMap);

public:
    LLVMCodeGen(const std::string& moduleName);
    void generate(const IRProgram& program);
    void printIR(std::ostream& out);
    bool compile(const std::string& objectFile);
};

} // namespace DNA
