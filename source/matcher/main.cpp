/** \file main.cpp
 * \brief Program checker entry point
 *
 * \author Sébastien Darche <sebastien.darche@polymtl.ca>
 */

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "llvm/Support/CommandLine.h"

#include "file_search.hpp"
#include "matchers.hpp"

#include <array>
#include <iostream>

static llvm::cl::OptionCategory llvmClCategory("Options");

static llvm::cl::opt<std::string>
    directory("d",
              llvm::cl::desc("Directory containing sinoscope-openmp.c and "
                             "kernel/sinoscope.cl"),
              llvm::cl::value_desc("dir"), llvm::cl::Required);

static llvm::cl::opt<std::string>
    kernel_include("I",
                   llvm::cl::desc("Directory containing helpers.cl (default: "
                                  "<dir>/kernel)"),
                   llvm::cl::value_desc("dir"));

static llvm::cl::opt<std::string> extra_args("extra-arg", llvm::cl::init(""));

int main(int argc, const char** argv) {
    llvm::cl::ParseCommandLineOptions(argc, argv);

    // First pass : grep-like search for some constructs

    auto omp_path = directory.getValue() + "/sinoscope-openmp.c";
    auto kernel_path = directory.getValue() + "/kernel/sinoscope.cl";
    auto include_path = kernel_include.empty()
                            ? directory.getValue() + "/kernel"
                            : kernel_include.getValue();

    auto req = getRequirements();

    try {
        assertOmpVariant(req.first, omp_path);
        assertOclVariant(req.second, kernel_path);
    } catch (std::exception& e) {
        std::cout << "Consigne : " << e.what() << std::endl;
        return -1;
    }

    // Second pass : AST matchers for arg passing

    const char* fake_argv[] = {"clang-tool",
                               "--",
                               "clang",
                               "-Xclang",
                               "-finclude-default-header",
                               "-I",
                               include_path.c_str(),
                               "-c",
                               kernel_path.c_str()};
    int fake_argc = sizeof(fake_argv) / sizeof(fake_argv[0]);
    std::string err;

    auto db = clang::tooling::FixedCompilationDatabase::loadFromCommandLine(
        fake_argc, fake_argv, err);

    clang::tooling::ClangTool tool(*db, kernel_path);

    // Compilation diagnostics are of no interest to the students here
    clang::IgnoringDiagConsumer diagnostics;
    tool.setDiagnosticConsumer(&diagnostics);

    clang::ast_matchers::MatchFinder finder;

    KernelCallback kernelChecker;

    finder.addMatcher(kernelMatcher, &kernelChecker);

    tool.run(clang::tooling::newFrontendActionFactory(&finder).get());

    try {
        kernelChecker.assertVariant(req.second);
    } catch (std::exception& e) {
        std::cout << "Consigne : " << e.what() << std::endl;
        return -1;
    }

    llvm::outs() << "Checker OK\n";
    return 0;
}
