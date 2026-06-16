#pragma once

#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <unordered_map>
#include <string>

namespace DNA {

class LLVMValueMapper {
private:
    std::unordered_map<std::string, llvm::Value*> temps_;
    std::unordered_map<std::string, llvm::AllocaInst*> vars_;

public:
    void clear() {
        temps_.clear();
        vars_.clear();
    }

    void insertTemp(const std::string& name, llvm::Value* val) {
        temps_[name] = val;
    }

    llvm::Value* lookupTemp(const std::string& name) const {
        auto it = temps_.find(name);
        if (it != temps_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void insertVar(const std::string& name, llvm::AllocaInst* alloca) {
        vars_[name] = alloca;
    }

    llvm::AllocaInst* lookupVar(const std::string& name) const {
        auto it = vars_.find(name);
        if (it != vars_.end()) {
            return it->second;
        }
        return nullptr;
    }
};

} // namespace DNA
