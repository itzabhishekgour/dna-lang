#include "LLVMCodeGen.h"
#include "LLVMTypeMapper.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/TargetParser/Host.h>
#include <stdexcept>
#include <iostream>
#include <optional>
#include <cctype>

namespace DNA {

LLVMCodeGen::LLVMCodeGen(const std::string& moduleName)
    : ctx_(moduleName) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
}

bool LLVMCodeGen::isInteger(const std::string& s) const {
    if (s.empty()) return false;
    std::size_t i = 0;
    if (s[0] == '-' || s[0] == '+') {
        if (s.size() == 1) return false;
        i = 1;
    }
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

bool LLVMCodeGen::isTempName(const std::string& s) const {
    if (s.size() < 2 || s[0] != 't') return false;
    for (std::size_t i = 1; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

llvm::Value* LLVMCodeGen::resolveOperand(const std::string& operand) {
    if (!operand.empty() && (operand.front() == '"' || operand.front() == '\'')) {
        // Strip outer quotes
        std::string value = operand.substr(1, operand.size() - 2);
        llvm::Value* dataPtr = ctx_.Builder.CreateGlobalString(value);
        llvm::Value* lengthVal = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_.Context), value.length());
        
        llvm::Type* stringType = LLVMTypeMapper::mapType("String", ctx_);
        llvm::AllocaInst* tempAlloca = ctx_.Builder.CreateAlloca(stringType, nullptr, "literal_str_temp");
        
        llvm::Function* literalFunc = functions_.at("dna_string_literal");
        llvm::CallInst* call = ctx_.Builder.CreateCall(literalFunc, { tempAlloca, dataPtr, lengthVal });
        call->addParamAttr(0, llvm::Attribute::get(ctx_.Context, llvm::Attribute::StructRet, stringType));
        
        return ctx_.Builder.CreateLoad(stringType, tempAlloca, "literal_str");
    }
    if (operand.find('.') != std::string::npos) {
        throw std::runtime_error("Backend Error:\nType not supported in LLVM Backend Phase 1");
    }
    if (operand == "true") {
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx_.Context), 1);
    }
    if (operand == "false") {
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx_.Context), 0);
    }
    if (isInteger(operand)) {
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_.Context), std::stoi(operand));
    }
    
    if (isTempName(operand)) {
        llvm::Value* val = valMapper_.lookupTemp(operand);
        if (!val) {
            throw std::runtime_error("Backend Error: Unknown temporary operand: " + operand);
        }
        return val;
    } else {
        llvm::AllocaInst* alloca = valMapper_.lookupVar(operand);
        if (alloca) {
            return ctx_.Builder.CreateLoad(alloca->getAllocatedType(), alloca, operand + "_val");
        }
        throw std::runtime_error("Backend Error: Unknown variable operand: " + operand);
    }
}

void LLVMCodeGen::generate(const IRProgram& program) {
    // ── First Pass: Create all LLVM function declarations and store in functions_ map ──
    
    // 1. Declare printf
    llvm::FunctionType* printfType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(ctx_.Context),
        { llvm::PointerType::getUnqual(ctx_.Context) },
        true // variadic
    );
    llvm::Function* printfFunc = llvm::Function::Create(
        printfType,
        llvm::Function::ExternalLinkage,
        "printf",
        *ctx_.Module
    );
    functions_["printf"] = printfFunc;

    // Declare dna_string_literal and dna_print_string
    llvm::Type* stringType = LLVMTypeMapper::mapType("String", ctx_);
    llvm::Type* ptrType = llvm::PointerType::getUnqual(ctx_.Context);
    llvm::Type* i32Type = llvm::Type::getInt32Ty(ctx_.Context);

    llvm::FunctionType* stringLiteralType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(ctx_.Context),
        { ptrType, ptrType, i32Type },
        false
    );
    llvm::Function* stringLiteralFunc = llvm::Function::Create(
        stringLiteralType,
        llvm::Function::ExternalLinkage,
        "dna_string_literal",
        *ctx_.Module
    );
    stringLiteralFunc->addParamAttr(0, llvm::Attribute::get(ctx_.Context, llvm::Attribute::StructRet, stringType));
    functions_["dna_string_literal"] = stringLiteralFunc;

    llvm::FunctionType* printStringFuncType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(ctx_.Context),
        { ptrType },
        false
    );
    llvm::Function* printStringFunc = llvm::Function::Create(
        printStringFuncType,
        llvm::Function::ExternalLinkage,
        "dna_print_string",
        *ctx_.Module
    );
    functions_["dna_print_string"] = printStringFunc;

    // 2. Declare all program functions
    for (const auto& fn : program.functions) {
        funcLocalVars_[fn.name] = fn.localVars;
        std::vector<llvm::Type*> paramTypes;
        for (const auto& p : fn.params) {
            paramTypes.push_back(LLVMTypeMapper::mapType(p.first, ctx_));
        }
        // Force the entrypoint 'main' function to return i32 in LLVM
        llvm::Type* retType = (fn.name == "main") ? llvm::Type::getInt32Ty(ctx_.Context) : LLVMTypeMapper::mapType(fn.returnTypeName, ctx_);
        llvm::FunctionType* fnType = llvm::FunctionType::get(retType, paramTypes, false);
        llvm::Function* llvmFn = llvm::Function::Create(
            fnType,
            llvm::Function::ExternalLinkage,
            fn.name,
            *ctx_.Module
        );
        functions_[fn.name] = llvmFn;
    }

    // ── Second Pass: Emit function bodies ──
    for (const auto& fn : program.functions) {
        codegenFunction(fn);
    }
}

void LLVMCodeGen::codegenFunction(const IRFunction& fn) {
    llvm::Function* llvmFn = functions_[fn.name];
    if (!llvmFn) {
        throw std::runtime_error("Backend Error: Function not declared: " + fn.name);
    }

    valMapper_.clear();

    // Create entry block
    llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(ctx_.Context, "entry", llvmFn);
    ctx_.Builder.SetInsertPoint(entryBB);

    // Set up parameters
    auto argIt = llvmFn->arg_begin();
    for (std::size_t i = 0; i < fn.params.size(); ++i) {
        const auto& p = fn.params[i];
        llvm::Argument& arg = *argIt++;
        arg.setName(p.second);

        llvm::Type* pType = LLVMTypeMapper::mapType(p.first, ctx_);
        llvm::AllocaInst* alloca = ctx_.Builder.CreateAlloca(pType, nullptr, p.second);
        ctx_.Builder.CreateStore(&arg, alloca);
        valMapper_.insertVar(p.second, alloca);
    }

    // Pre-create all BasicBlocks for LABEL instructions
    std::unordered_map<std::string, llvm::BasicBlock*> labelMap;
    for (const auto& instr : fn.instructions) {
        if (instr.op == OpCode::LABEL) {
            std::string labelName = instr.operands.at(0);
            llvm::BasicBlock* bb = llvm::BasicBlock::Create(ctx_.Context, labelName, llvmFn);
            labelMap[labelName] = bb;
        }
    }

    // Verify JUMP, JUMP_IF_FALSE and JUMP_IF_TRUE targets
    for (const auto& instr : fn.instructions) {
        if (instr.op == OpCode::JUMP) {
            std::string target = instr.operands.at(0);
            if (labelMap.find(target) == labelMap.end()) {
                throw std::runtime_error("Invalid IR:\nUnknown label " + target);
            }
        } else if (instr.op == OpCode::JUMP_IF_FALSE || instr.op == OpCode::JUMP_IF_TRUE) {
            std::string target = instr.operands.at(1);
            if (labelMap.find(target) == labelMap.end()) {
                throw std::runtime_error("Invalid IR:\nUnknown label " + target);
            }
        }
    }

    // Translate instructions
    for (const auto& instr : fn.instructions) {
        codegenInstruction(instr, llvmFn, labelMap);
    }
}

void LLVMCodeGen::codegenInstruction(const IRInstruction& instr, llvm::Function* currentFunc, std::unordered_map<std::string, llvm::BasicBlock*>& labelMap) {
    if (instr.op != OpCode::LABEL && ctx_.Builder.GetInsertBlock()->getTerminator()) {
        return; // Skip unreachable instructions within the same basic block
    }

    switch (instr.op) {
        case OpCode::ALLOC: {
            std::string varName = instr.operands.at(0);
            std::string funcName = currentFunc->getName().str();
            std::string typeName = funcLocalVars_.at(funcName).at(varName);
            llvm::Type* varType = LLVMTypeMapper::mapType(typeName, ctx_);
            llvm::AllocaInst* alloca = ctx_.Builder.CreateAlloca(varType, nullptr, varName);
            valMapper_.insertVar(varName, alloca);
            break; 
        }
        case OpCode::STORE: {
            std::string varName = instr.operands.at(0);
            std::string valueName = instr.operands.at(1);
            llvm::Value* val = resolveOperand(valueName);
            llvm::AllocaInst* alloca = valMapper_.lookupVar(varName);
            if (!alloca) {
                throw std::runtime_error("Backend Error: Unknown store target: " + varName);
            }
            ctx_.Builder.CreateStore(val, alloca);
            break;
        }
        case OpCode::LOAD: {
            std::string varName = instr.operands.at(0);
            llvm::AllocaInst* alloca = valMapper_.lookupVar(varName);
            if (!alloca) {
                throw std::runtime_error("Backend Error: Unknown load source: " + varName);
            }
            llvm::Value* val = ctx_.Builder.CreateLoad(alloca->getAllocatedType(), alloca, instr.dest);
            valMapper_.insertTemp(instr.dest, val);
            break;
        }
        case OpCode::MOV: {
            std::string valueName = instr.operands.at(0);
            llvm::Value* val = resolveOperand(valueName);
            valMapper_.insertTemp(instr.dest, val);
            break;
        }
        case OpCode::ADD: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateAdd(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::SUB: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateSub(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::MUL: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateMul(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::DIV: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateSDiv(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::MOD: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateSRem(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::NEG: {
            llvm::Value* v = resolveOperand(instr.operands.at(0));
            llvm::Value* res = ctx_.Builder.CreateNeg(v, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::CMP_EQ: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateICmpEQ(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::CMP_NE: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateICmpNE(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::CMP_GT: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateICmpSGT(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::CMP_LT: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateICmpSLT(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::CMP_GE: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateICmpSGE(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::CMP_LE: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateICmpSLE(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::LOG_AND: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateAnd(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::LOG_OR: {
            llvm::Value* l = resolveOperand(instr.operands.at(0));
            llvm::Value* r = resolveOperand(instr.operands.at(1));
            llvm::Value* res = ctx_.Builder.CreateOr(l, r, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::LOG_NOT: {
            llvm::Value* v = resolveOperand(instr.operands.at(0));
            llvm::Value* res = ctx_.Builder.CreateNot(v, instr.dest);
            valMapper_.insertTemp(instr.dest, res);
            break;
        }
        case OpCode::LABEL: {
            std::string labelName = instr.operands.at(0);
            llvm::BasicBlock* bb = labelMap.at(labelName);
            if (!ctx_.Builder.GetInsertBlock()->getTerminator()) {
                ctx_.Builder.CreateBr(bb);
            }
            ctx_.Builder.SetInsertPoint(bb);
            break;
        }
        case OpCode::JUMP: {
            std::string labelName = instr.operands.at(0);
            llvm::BasicBlock* bb = labelMap.at(labelName);
            ctx_.Builder.CreateBr(bb);
            break;
        }
        case OpCode::JUMP_IF_FALSE: {
            llvm::Value* c = resolveOperand(instr.operands.at(0));
            std::string labelName = instr.operands.at(1);
            llvm::BasicBlock* falseBB = labelMap.at(labelName);
            llvm::BasicBlock* trueBB = llvm::BasicBlock::Create(ctx_.Context, "jump_true", currentFunc);
            ctx_.Builder.CreateCondBr(c, trueBB, falseBB);
            ctx_.Builder.SetInsertPoint(trueBB);
            break;
        }
        case OpCode::JUMP_IF_TRUE: {
            llvm::Value* c = resolveOperand(instr.operands.at(0));
            std::string labelName = instr.operands.at(1);
            llvm::BasicBlock* trueBB = labelMap.at(labelName);
            llvm::BasicBlock* falseBB = llvm::BasicBlock::Create(ctx_.Context, "jump_false", currentFunc);
            ctx_.Builder.CreateCondBr(c, trueBB, falseBB);
            ctx_.Builder.SetInsertPoint(falseBB);
            break;
        }
        case OpCode::CALL: {
            std::string calleeName = instr.operands.at(0);
            llvm::Function* callee = functions_.at(calleeName);
            if (!callee) {
                throw std::runtime_error("Backend Error: Call to undeclared function: " + calleeName);
            }
            std::vector<llvm::Value*> args;
            for (std::size_t i = 1; i < instr.operands.size(); ++i) {
                args.push_back(resolveOperand(instr.operands.at(i)));
            }
            llvm::Value* val = ctx_.Builder.CreateCall(callee, args, instr.dest.empty() ? "" : instr.dest);
            if (!instr.dest.empty()) {
                valMapper_.insertTemp(instr.dest, val);
            }
            break;
        }
        case OpCode::RETURN: {
            if (currentFunc->getName() == "main") {
                if (instr.operands.empty()) {
                    ctx_.Builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_.Context), 0));
                } else {
                    llvm::Value* val = resolveOperand(instr.operands.at(0));
                    ctx_.Builder.CreateRet(val);
                }
            } else {
                if (instr.operands.empty()) {
                    ctx_.Builder.CreateRetVoid();
                } else {
                    llvm::Value* val = resolveOperand(instr.operands.at(0));
                    ctx_.Builder.CreateRet(val);
                }
            }
            break;
        }
        case OpCode::PRINT: {
            llvm::Value* val = resolveOperand(instr.operands.at(0));
            if (val->getType()->isStructTy()) {
                llvm::AllocaInst* tempAlloca = ctx_.Builder.CreateAlloca(val->getType(), nullptr, "print_str_temp");
                ctx_.Builder.CreateStore(val, tempAlloca);
                llvm::Function* printStringFunc = functions_.at("dna_print_string");
                ctx_.Builder.CreateCall(printStringFunc, { tempAlloca });
            } else {
                llvm::Function* printfFunc = functions_.at("printf");
                llvm::Value* fmtStr = ctx_.Builder.CreateGlobalString("%d\n");
                ctx_.Builder.CreateCall(printfFunc, { fmtStr, val });
            }
            break;
        }
        case OpCode::NEW:
        case OpCode::GETFIELD:
        case OpCode::SETFIELD:
        case OpCode::INPUT:
            throw std::runtime_error("Backend Error:\nType not supported in LLVM Backend Phase 1");
        default:
            throw std::runtime_error("Backend Error: Opcode not supported in LLVM Backend Phase 1: " + opcodeName(instr.op));
    }
}

// Modify codegenInstruction implementation above for OpCode::ALLOC to use funcLocalVars_
// We will modify the header to include funcLocalVars_ map. Let's make sure we do it properly.

void LLVMCodeGen::printIR(std::ostream& out) {
    std::string irStr;
    llvm::raw_string_ostream os(irStr);
    ctx_.Module->print(os, nullptr);
    out << irStr;
}

bool LLVMCodeGen::compile(const std::string& objectFile) {
    std::string TargetTriple = llvm::sys::getDefaultTargetTriple();
    ctx_.Module->setTargetTriple(TargetTriple);

    std::string Error;
    const llvm::Target* Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

    if (!Target) {
        llvm::errs() << Error;
        return false;
    }

    std::string CPU = "generic";
    std::string Features = "";

    llvm::TargetOptions opt;
    std::optional<llvm::Reloc::Model> RM;
    llvm::TargetMachine* TargetMachine = Target->createTargetMachine(
        TargetTriple, CPU, Features, opt, RM
    );

    ctx_.Module->setDataLayout(TargetMachine->createDataLayout());

    std::error_code EC;
    llvm::raw_fd_ostream dest(objectFile, EC, llvm::sys::fs::OF_None);

    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message();
        return false;
    }

    llvm::legacy::PassManager pass;
    auto FileType = llvm::CodeGenFileType::ObjectFile;

    if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
        llvm::errs() << "TargetMachine can't emit a file of this type";
        return false;
    }

    pass.run(*ctx_.Module);
    dest.flush();
    return true;
}

} // namespace DNA
