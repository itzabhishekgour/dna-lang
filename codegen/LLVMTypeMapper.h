#pragma once

#include <llvm/IR/Type.h>
#include "LLVMContextManager.h"
#include <string>
#include <stdexcept>

namespace DNA {

class LLVMTypeMapper {
private:
    static bool isIdentifier(const std::string& s) {
        if (s.empty()) return false;
        if (!std::isalpha(static_cast<unsigned char>(s[0])) && s[0] != '_') return false;
        for (char c : s) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
        }
        return true;
    }

public:
    static llvm::Type* mapType(const std::string& typeName, LLVMContextManager& ctx) {
        if (typeName == "int") {
            return llvm::Type::getInt32Ty(ctx.Context);
        } else if (typeName == "bool") {
            return llvm::Type::getInt1Ty(ctx.Context);
        } else if (typeName == "void") {
            return llvm::Type::getVoidTy(ctx.Context);
        } else if (typeName == "String") {
            llvm::StructType* structType = llvm::StructType::getTypeByName(ctx.Context, "DNAString");
            if (!structType) {
                llvm::Type* ptrType = llvm::PointerType::getUnqual(ctx.Context);
                llvm::Type* i32Type = llvm::Type::getInt32Ty(ctx.Context);
                structType = llvm::StructType::create(
                    ctx.Context,
                    { ptrType, i32Type, i32Type },
                    "DNAString"
                );
            }
            return structType;
        } else if (typeName == "float" || typeName == "double" || typeName == "char") {
            throw std::runtime_error("Backend Error:\nType not supported in LLVM Backend Phase 1");
        } else if (isIdentifier(typeName)) {
            throw std::runtime_error("Backend Error:\nType not supported in LLVM Backend Phase 1");
        } else {
            throw std::runtime_error("Unsupported LLVM type: " + typeName);
        }
    }
};

} // namespace DNA
