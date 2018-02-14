/*
*
*                   _/_/_/    _/_/   _/    _/ _/_/_/    _/_/
*                  _/   _/ _/    _/ _/_/  _/ _/   _/ _/    _/
*                 _/_/_/  _/_/_/_/ _/  _/_/ _/   _/ _/_/_/_/
*                _/      _/    _/ _/    _/ _/   _/ _/    _/
*               _/      _/    _/ _/    _/ _/_/_/  _/    _/
*
*             ***********************************************
*                              PandA Project
*                     URL: http://panda.dei.polimi.it
*                       Politecnico di Milano - DEIB
*                        System Architectures Group
*             ***********************************************
*              Copyright (c) 2004-2017 Politecnico di Milano
*
*   This file is part of the PandA framework.
*
*   The PandA framework is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/
/**
* @file plugin_topfname.cpp
* @brief Dummy Plugin. LLVM does not need this plugin but we add just for the sake of completeness.
*
* @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
*
*/
#include "plugin_includes.hpp"

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"

#include <cxxabi.h>

namespace llvm {
   struct clang4_plugin_DoNotExposeGlobalsPass;
}

static std::string TopFunctionNmae;

namespace clang {


   class clang4_plugin_topfname : public PluginASTAction
   {
         std::string topfname;
         friend struct llvm::clang4_plugin_DoNotExposeGlobalsPass;
      protected:
         std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &,
                                                        llvm::StringRef ) override
         {
            TopFunctionNmae = topfname;
            return llvm::make_unique<dummyConsumer>();
         }

         bool ParseArgs(const CompilerInstance &CI,
                        const std::vector<std::string> &args) override
         {
            DiagnosticsEngine &D = CI.getDiagnostics();
            for (size_t i = 0, e = args.size(); i != e; ++i)
            {
               if (args.at(i) == "-topfname")
               {
                  if (i + 1 >= e) {
                     D.Report(D.getCustomDiagID(DiagnosticsEngine::Error,
                                                "missing topfname argument"));
                     return false;
                  }
                  ++i;
                  topfname = args.at(i);
               }
            }
            if (!args.empty() && args.at(0) == "-help")
               PrintHelp(llvm::errs());

            if(topfname=="")
               D.Report(D.getCustomDiagID(DiagnosticsEngine::Error,
                                          "topfname not specified"));
            return true;
         }
         void PrintHelp(llvm::raw_ostream& ros)
         {
            ros << "Help for clang4_plugin_topfname plugin\n";
            ros << "-topfname <topfunctionname>\n";
            ros << "  name of the top function\n";
         }

         PluginASTAction::ActionType getActionType() override
         {
            return AddAfterMainAction;
         }
   };

}

static clang::FrontendPluginRegistry::Add<clang::clang4_plugin_topfname>
X("clang4_plugin_topfname", "Dumy plugin");


#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/PassRegistry.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/InitializePasses.h"


namespace llvm {
   struct clang4_plugin_DoNotExposeGlobalsPass: public ModulePass
   {
         static char ID;
         clang4_plugin_DoNotExposeGlobalsPass() : ModulePass(ID)
         {
            initializeLoopPassPass(*PassRegistry::getPassRegistry());
         }
         std::string getDemangled(const std::string& declname)
         {
            int status;
            char * demangled_outbuffer = abi::__cxa_demangle(declname.c_str(), NULL, NULL, &status);
            if(status==0)
            {
               std::string res=declname;
               if(std::string(demangled_outbuffer).find_last_of('(') != std::string::npos)
               {
                  res = demangled_outbuffer;
                  auto parPos = res.find('(');
                  assert(parPos != std::string::npos);
                  res = res.substr(0,parPos);
               }
               free(demangled_outbuffer);
               return res;
            }
            else
               assert(demangled_outbuffer==nullptr);
            return declname;
         }
         bool runOnModule(Module &M)
         {
            bool changed = false;
            llvm::errs() << "Top function name: " << TopFunctionNmae << "\n";
            for(auto& globalVar : M.getGlobalList())
            {
               std::string varName = std::string(globalVar.getName());
               llvm::errs() << "Found global name: " << varName << "\n";
               if(varName == "llvm.global_ctors" ||
                     varName == "llvm.global_dtors" ||
                     varName == "llvm.used" ||
                     varName == "llvm.compiler.used")
                  llvm::errs() << "Global intrinsic skipped: " << globalVar.getName()<< "\n";
               else
               if(!globalVar.hasInternalLinkage())
               {
                  llvm::errs() << "it becomes internal\n";
                  changed = true;
                  globalVar.setLinkage(llvm::GlobalValue::InternalLinkage);
               }
            }
            for(auto& fun : M.getFunctionList())
            {
               if(fun.isIntrinsic())
                  llvm::errs() << "Function intrinsic skipped: " << fun.getName()<< "\n";
               else
               {
                  auto funName = fun.getName();
                  auto demangled = getDemangled(funName);
                  llvm::errs() << "Found function: " << funName << "|" << demangled << "\n";
                  if (!fun.hasInternalLinkage() && funName != TopFunctionNmae && demangled != TopFunctionNmae)
                  {
                     llvm::errs() << "it becomes internal\n";
                     changed = true;
                     fun.setLinkage(llvm::GlobalValue::InternalLinkage);
                  }
               }
            }
            return changed;

         }
         virtual StringRef getPassName() const
         {
            return "clang4_plugin_DoNotExposeGlobalsPass";
         }
         void getAnalysisUsage(AnalysisUsage &AU) const
         {
           AU.setPreservesAll();
           getLoopAnalysisUsage(AU);
         }
   };

}
char llvm::clang4_plugin_DoNotExposeGlobalsPass::ID = 0;
static llvm::RegisterPass<llvm::clang4_plugin_DoNotExposeGlobalsPass> XPass("clang4_plugin_DoNotExposeGlobalsPass",
                                                                             "Make all private/static but the top function",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);

// This function is of type PassManagerBuilder::ExtensionFn
static void loadPass(const llvm::PassManagerBuilder &, llvm::legacy::PassManagerBase &PM) {
  PM.add(new llvm::clang4_plugin_DoNotExposeGlobalsPass());
}
// These constructors add our pass to a list of global extensions.
static llvm::RegisterStandardPasses clang4_plugin_DoNotExposeGlobalsLoader_Ox(llvm::PassManagerBuilder::EP_ModuleOptimizerEarly, loadPass);
