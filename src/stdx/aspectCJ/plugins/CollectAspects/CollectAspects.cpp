/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * This source file is part of the Cangjie project, licensed under Apache-2.0
 * with Runtime Library Exception.
 *
 * See https://cangjie-lang.cn/pages/LICENSE for license information.
 */

#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/CHIR/CHIRBuilder.h"
#include "cangjie/CHIR/Type/CustomTypeDef.h"
#include "cangjie/Mangle/CHIRTypeManglingUtils.h"
#include "cangjie/MetaTransformation/MetaTransform.h"

#include <fstream>

using namespace Cangjie;

namespace {
class MyError;
/**
 * This structure is used to collect aspects introduced by @InsertAtEntry, @InsertAtExit, @ReplaceFuncBody.
 */
struct CollectAspects : public MetaTransform<CHIR::Func> {
    friend class MyError;
    /**
     * Create and initialize a plugin for CHIR::Func.
     * @param builder The class to manage nodes on CHIR.
     */
    CollectAspects(CHIR::CHIRBuilder& builder) : builder(builder), packageName(builder.GetCurPackage()->GetName())
    {
    }
    /**
     * The entry for compiler to execute this plugin.
     * @param func The func which this plugin executes on.
     */
    void Run(CHIR::Func& func);

private:
    struct Insert {
        std::string methodMangledName;
        size_t paramsNum;
        bool isMemberFunc;
        bool isStatic;
        std::string outerTypeParams;
        std::string selfTypeParams;
    };
    struct To {
        std::string packageName;
        std::string className;
        std::string methodName;
        std::string funcTypeStr;
        std::string isStatic;
        std::string recursively;
    };
    struct OutputInfo {
        std::string annoClassName;
        Insert insert;
        To to;
    };

    bool GlobalVarsArePrimitiveLiterals();
    void CheckAndCollect(CHIR::Func& func);
    void GenerateOrUpdateAnnoInfo(MyError& myError);
    bool CheckGlobalVariableUsages(MyError& myError);
    bool CheckFuncConstrains(const CHIR::Func& func, const CHIR::AnnoInfo::AnnoPair& annoPair, MyError& myError);
    bool CheckFuncTypeStr(const CHIR::Func& func, const CHIR::AnnoInfo::AnnoPair& annoPair, MyError& myError);
    void Collect(const CHIR::Func& func, const CHIR::AnnoInfo::AnnoPair& annoPair, MyError& myError);

    const std::set<std::string> whiteList1 = {"InsertAtEntry", "InsertAtExit"};
    const std::set<std::string> whiteList2 = {"ReplaceFuncBody"};

    CHIR::CHIRBuilder& builder;
    std::vector<OutputInfo> outputs;
    std::string packageInitMethodName;
    std::string packageName;
    bool isGlobalVarsChecked = false;
    bool hasError = false;
    bool hasCreateAnnoInfo = false;
};

bool CollectAspects::GlobalVarsArePrimitiveLiterals()
{
    const CHIR::Package* package = builder.GetCurPackage();
    for (auto gv : package->GetGlobalVars()) {
        if (gv->TestAttr(Cangjie::CHIR::Attribute::IMPORTED) ||
            gv->TestAttr(Cangjie::CHIR::Attribute::COMPILER_ADD)) {
            continue;
        }
        auto type = gv->GetType()->StripAllRefs();
        if (!type->IsPrimitive()) {
            return false;
        }
        if (!gv->GetInitializer() || !gv->GetInitializer()->IsLiteral()) {
            return false;
        }
    }
    isGlobalVarsChecked = true;
    return true;
}

class MyError {
public:
    MyError(CollectAspects& plugin) : plugin(plugin)
    {
    }
    template <typename... Args> void Emit(const char* fmt, Args&&... args)
    {
        ++errorCnt;
        Errorf(fmt, args...);
    }
    ~MyError()
    {
        plugin.hasError = errorCnt > 0U;
    }
    bool IsEmpty() const
    {
        return errorCnt == 0U;
    }

private:
    size_t errorCnt = 0U;
    CollectAspects& plugin;
};

void CollectAspects::GenerateOrUpdateAnnoInfo(MyError& myError)
{
    if (!hasCreateAnnoInfo) {
        std::ofstream outFile(packageName + ".annoinfo");
        if (!outFile) {
            myError.Emit("Failed to create file.\n");
            return;
        }
        outFile << builder.GetCurPackage()->GetPackageInitFunc()->GetIdentifierWithoutPrefix() << std::endl;
        hasCreateAnnoInfo = true;
        outFile.close();
    }
    std::ofstream outFile(packageName + ".annoinfo", std::ios_base::app);
    if (!outFile) {
        myError.Emit("Failed to access file.\n");
        return;
    }
    auto output = outputs.back();
    outFile << output.annoClassName << std::endl
            << output.insert.methodMangledName << std::endl
            << output.insert.paramsNum << std::endl
            << output.insert.isMemberFunc << std::endl
            << output.insert.isStatic << std::endl;
    outFile << output.to.packageName << std::endl
            << output.to.className << std::endl
            << output.to.methodName << std::endl
            << output.to.funcTypeStr << std::endl
            << output.to.isStatic << std::endl
            << output.to.recursively << std::endl;
    outFile.close();
}

bool CollectAspects::CheckGlobalVariableUsages(MyError& myError)
{
    if (!isGlobalVarsChecked && !GlobalVarsArePrimitiveLiterals()) {
        myError.Emit("%s: not allowed to use global variables initialized by non-primitive constant literals.\n",
            builder.GetCurPackage()->GetName().c_str());
        return false;
    }
    return true;
}

bool CollectAspects::CheckFuncTypeStr(
    const CHIR::Func& func, const CHIR::AnnoInfo::AnnoPair& annoPair, MyError& myError)
{
    bool isFirstKind = whiteList1.find(annoPair.annoClassName) != whiteList1.end();
    auto packageName = annoPair.paramValues[0U];
    auto className = annoPair.paramValues[1U];
    auto isStatic = annoPair.paramValues[3U];
    auto outerDef = func.GetOuterDeclaredOrExtendedDef();

    if (outerDef && !func.TestAttr(Cangjie::CHIR::Attribute::STATIC)) {
        if (className.empty() || isStatic == "true") {
            myError.Emit("%s: don't wave a instance method into a global/static function.\n",
                func.GetSrcCodeIdentifier().c_str());
            return false;
        }
        if (CHIR::GetTypeQualifiedName(*outerDef->GetType()) != (packageName + ":" + className)) {
            myError.Emit("%s: don't wave a instance method into another instance method with different `this` type.\n",
                func.GetSrcCodeIdentifier().c_str());
            return false;
        }
    } else if (func.GetFuncType()->GetParamTypes().size() > (isFirstKind ? 0U : 1U) && !className.empty() &&
        isStatic == "false") {
        if (CHIR::GetTypeQualifiedName(*func.GetFuncType()->GetParamType(0U)) != (packageName + ":" + className)) {
            myError.Emit("%s: don't wave a method into another instance method with different `this` type.\n",
                func.GetSrcCodeIdentifier().c_str());
            return false;
        }
    }

    std::string wovenFuncTypeStr = "";
    std::string aspectFuncTypeStr = "";
    auto paramTys = func.GetFuncType()->GetParamTypes();
    bool isInsMethod = outerDef && !func.TestAttr(Cangjie::CHIR::Attribute::STATIC);
    if (isFirstKind && func.GetFuncType()->GetParamTypes().size() > (isInsMethod ? 1U : 0U)) {
        wovenFuncTypeStr = annoPair.paramValues[4U];
        if (isInsMethod || (!className.empty() && isStatic == "false")) {
            paramTys = std::vector<CHIR::Type*>(paramTys.begin() + 1, paramTys.end());
        }
        aspectFuncTypeStr =
            CHIR::GetTypeQualifiedName(*builder.GetType<CHIR::FuncType>(paramTys, builder.GetUnitTy()));
        aspectFuncTypeStr = aspectFuncTypeStr.substr(0, aspectFuncTypeStr.size() - 6U); // 6 means "->Unit"
    } else if (!isFirstKind && func.GetFuncType()->GetParamTypes().size() > (isInsMethod ? 2U : 1U)) {
        wovenFuncTypeStr = CHIR::GetTypeQualifiedName(*func.GetFuncType()->GetParamTypes().back());
        paramTys.pop_back();
        aspectFuncTypeStr = CHIR::GetTypeQualifiedName(
            *builder.GetType<CHIR::FuncType>(paramTys, func.GetFuncType()->GetReturnType()));
    }
    std::replace(aspectFuncTypeStr.begin(), aspectFuncTypeStr.end(), ':', '.');
    std::replace(wovenFuncTypeStr.begin(), wovenFuncTypeStr.end(), ':', '.');
    if (wovenFuncTypeStr.find(aspectFuncTypeStr) != 0) {
        myError.Emit(
            "The aspect function %s has different FuncType with the waved.\n", func.GetSrcCodeIdentifier().c_str());
        return false;
    }
    return true;
}

bool CollectAspects::CheckFuncConstrains(
    const CHIR::Func& func, const CHIR::AnnoInfo::AnnoPair& annoPair, MyError& myError)
{
    if (!func.TestAttr(Cangjie::CHIR::Attribute::PUBLIC) ||
        func.Get<Cangjie::CHIR::LinkTypeInfo>() != Linkage::EXTERNAL) {
        myError.Emit("%s: should be public.\n", func.GetSrcCodeIdentifier().c_str());
        return false;
    }
    if (func.TestAttr(Cangjie::CHIR::Attribute::FOREIGN)) {
        myError.Emit("%s: it is not allowed to mark AOP annotations on foreign functions.\n",
            func.GetSrcCodeIdentifier().c_str());
        return false;
    }
    auto outerDef = func.GetOuterDeclaredOrExtendedDef();
    if (!func.GetGenericTypeParams().empty() || (outerDef && !outerDef->GetGenericTypeParams().empty())) {
        myError.Emit("%s: it is not allowed to mark AOP annotations on a function with generic type parameters.\n",
            func.GetSrcCodeIdentifier().c_str());
        return false;
    }
    if (!CheckFuncTypeStr(func, annoPair, myError)) {
        return false;
    }
    auto className = annoPair.paramValues[1U];
    bool isFirstKind = whiteList1.find(annoPair.annoClassName) != whiteList1.end();
    auto isRecursive = annoPair.paramValues[isFirstKind ? 5U : 4U];
    if (className.empty() && isRecursive != "false") {
        myError.Emit("%s: the `recursive` field should be `false`.\n", func.GetSrcCodeIdentifier().c_str());
        return false;
    }
    if (isFirstKind) {
        if (!func.GetReturnType()->IsUnit()) {
            myError.Emit("%s: labeled by @%s should have Unit return type.\n", func.GetSrcCodeIdentifier().c_str(),
                annoPair.annoClassName.c_str());
            return false;
        }
    } else {
        if (func.GetNumOfParams() <= 0) {
            myError.Emit("%s: labeled by @ReplaceFuncBody should have at least a parameter.\n",
                func.GetSrcCodeIdentifier().c_str());
            return false;
        }
        auto lastParameterType = func.GetParams().back()->GetType();
        if (!lastParameterType->IsFunc()) {
            myError.Emit("%s: the last parameter of the function labeled by @ReplaceFuncBody should be of func type.\n",
                func.GetSrcCodeIdentifier().c_str());
            return false;
        }
        auto originalFuncType = static_cast<CHIR::FuncType*>(lastParameterType);
        if (func.GetReturnType() != originalFuncType->GetReturnType()) {
            myError.Emit("%s: the return type of the function labeled by @ReplaceFuncBody should be the same with "
                         "the waved.\n",
                func.GetSrcCodeIdentifier().c_str());
            return false;
        }
    }
    return true;
}

void CollectAspects::Collect(const CHIR::Func& func, const CHIR::AnnoInfo::AnnoPair& annoPair, MyError& myError)
{
    bool isFirstKind = whiteList1.find(annoPair.annoClassName) != whiteList1.end();
    auto packageName = annoPair.paramValues[0U];
    auto className = annoPair.paramValues[1U];
    auto methodName = annoPair.paramValues[2U];
    auto isStatic = annoPair.paramValues[3U];
    auto isRecursive = annoPair.paramValues[isFirstKind ? 5U : 4U];

    Insert insert = {func.GetIdentifierWithoutPrefix(),
        func.GetNumOfParams() - (annoPair.annoClassName == "ReplaceFuncBody" ? 1U : 0U),
        func.GetParentCustomTypeDef() != nullptr, func.TestAttr(Cangjie::CHIR::Attribute::STATIC)};

    if (isFirstKind) {
        auto sig = annoPair.paramValues[4U];
        To to = {packageName, className, methodName, sig, isStatic, isRecursive};
        outputs.emplace_back(OutputInfo{annoPair.annoClassName, insert, to});
    } else {
        // is a non-static method:
        if (!className.empty() && isStatic == "false") {
            auto funcType = static_cast<CHIR::FuncType*>(func.GetParam(func.GetNumOfParams() - 1U)->GetType());
            auto paramTys = funcType->GetParamTypes();
            // check the validation of `this` parameter
            if (paramTys.empty() || CHIR::GetTypeQualifiedName(*paramTys[0]) != className) {
                myError.Emit(
                    "%s: check if the className in @ReplaceFuncBody is right.\n", func.GetSrcCodeIdentifier().c_str());
                return;
            }
            // skip `this` parameter
            paramTys = std::vector<CHIR::Type*>(paramTys.begin() + 1U, paramTys.end());
            funcType = builder.GetType<CHIR::FuncType>(paramTys, funcType->GetReturnType());
            auto sig = CHIR::GetTypeQualifiedName(*funcType);
            std::replace(sig.begin(), sig.end(), ':', '.');
            To to = {packageName, className, methodName, sig, isStatic, isRecursive};
            outputs.emplace_back(OutputInfo{annoPair.annoClassName, insert, to});
        } else {
            auto sig = CHIR::GetTypeQualifiedName(*func.GetParam(func.GetNumOfParams() - 1)->GetType());
            std::replace(sig.begin(), sig.end(), ':', '.');
            To to = {packageName, className, methodName, sig, isStatic, isRecursive};
            outputs.emplace_back(OutputInfo{annoPair.annoClassName, insert, to});
        }
    }
}

void CollectAspects::CheckAndCollect(CHIR::Func& func)
{
    auto annoInfo = func.GetAnnoInfo();
    if (annoInfo.annoPairs.empty()) {
        return;
    }

    MyError myError(*this);

    bool inWhiteList1 = false;
    bool inWhiteList2 = false;
    for (auto& annoPair : annoInfo.annoPairs) {
        bool isFirstKind = whiteList1.find(annoPair.annoClassName) != whiteList1.end();
        bool isSecondKind = whiteList2.find(annoPair.annoClassName) != whiteList2.end();

        inWhiteList1 |= isFirstKind;
        inWhiteList2 |= isSecondKind;
        if (inWhiteList1 && inWhiteList2) {
            myError.Emit("%s: cannot use @InsertAtEntry/@InsertAtExit and @ReplaceFuncBody at the same time.\n",
                func.GetSrcCodeIdentifier().c_str());
            return;
        }
        if (!isFirstKind && !isSecondKind) {
            continue;
        }
        if (!CheckGlobalVariableUsages(myError)) {
            return;
        }
        if (!CheckFuncConstrains(func, annoPair, myError)) {
            return;
        }
        Collect(func, annoPair, myError);
        if (!myError.IsEmpty()) {
            return;
        }
        GenerateOrUpdateAnnoInfo(myError);
    }
}

// Implements plugin
void CollectAspects::Run(CHIR::Func& func)
{
    if (hasError) {
        return;
    }

    CheckAndCollect(func);

    if (hasError) {
        Errorf("plugin terminated by some unexpected usages.\n");
        throw NullPointerException();
    }
}
} // namespace

// Register plugin
CHIR_PLUGIN(CollectAspects)
