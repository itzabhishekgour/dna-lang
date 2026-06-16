#pragma once

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <string>

namespace DNA {

class LLVMContextManager {
public:
    llvm::LLVMContext Context;
    std::unique_ptr<llvm::Module> Module;
    llvm::IRBuilder<> Builder;

    LLVMContextManager(const std::string& moduleName)
        : Builder(Context) {
        Module = std::make_unique<llvm::Module>(moduleName, Context);
    }
};

} // namespace DNA
