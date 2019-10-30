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
 *              Copyright (C) 2004-2019 Politecnico di Milano
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
 * @file eSSA.cpp
 * @brief 
 *
 * @author Michele Fiorito <michele2.fiorito@mail.polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
 */

#include "eSSA.hpp"

///. include
#include "Parameter.hpp"

/// behavior includes
#include "application_manager.hpp"
#include "basic_block.hpp"
#include "function_behavior.hpp"

#include "Dominance.hpp"

/// design_flows include
#include "design_flow_manager.hpp"

/// frontend_analysis
#include "application_frontend_flow_step.hpp"

/// tree include
#include "behavioral_helper.hpp"
#include "tree_basic_block.hpp"
#include "tree_helper.hpp"
#include "tree_manager.hpp"
#include "tree_manipulation.hpp"
#include "tree_reindex.hpp"

/// wrapper/treegcc include
#include "gcc_wrapper.hpp"
#include "string_manipulation.hpp" // for GET_CLASS

//    #define DEBUG_eSSA

namespace eSSAInfo
{

   class OrderedBasicBlock
   {
    private:
      /// Map a phi to its position in a BasicBlock.
      std::map<const struct gimple_node*, unsigned> NumberedPhis;

      /// Keep track of last instruction inserted into \p NumberedInsts.
      /// It speeds up queries for uncached instructions by providing a start point
      /// for new queries in OrderedBasicBlock::comesBefore.
      std::list<tree_nodeRef>::const_iterator LastPhiFound;

      /// The position/number to tag the next instruction to be found.
      unsigned NextPhiPos;

      /// The source BasicBlock instruction list
      const std::list<tree_nodeRef>* BBPhi;

      /// Map a instruction to its position in a BasicBlock.
      std::map<const struct gimple_node*, unsigned> NumberedInsts;

      /// Keep track of last instruction inserted into \p NumberedInsts.
      /// It speeds up queries for uncached instructions by providing a start point
      /// for new queries in OrderedBasicBlock::comesBefore.
      std::list<tree_nodeRef>::const_iterator LastInstFound;

      /// The position/number to tag the next instruction to be found.
      unsigned NextInstPos;

      /// The source BasicBlock instruction list
      const std::list<tree_nodeRef>* BBInst;

      /// The source BasicBlock to map.
      const blocRef BB;

      bool phiComesBefore(const struct gimple_node *A, const struct gimple_node *B)
      {
         const struct gimple_node* Phi = nullptr;
         THROW_ASSERT(!(LastPhiFound == BBPhi->end() && NextPhiPos != 0),
               "Phi supposed to be in NumberedPhis");
      
         // Start the search with the instruction found in the last lookup round.
         auto II = BBPhi->begin();
         auto IE = BBPhi->end();
         if (LastPhiFound != IE)
         {
            II = std::next(LastPhiFound);
         }
      
         // Number all instructions up to the point where we find 'A' or 'B'.
         for (; II != IE; ++II) 
         {
            Phi = GetPointer<gimple_node>(GET_NODE(*II));
            NumberedPhis[Phi] = NextPhiPos++;
            if (Phi == A || Phi == B)
               break;
         }
      
         THROW_ASSERT(II != IE, "Phi not found?");
         THROW_ASSERT((Phi == A || Phi == B), "Should find A or B");
         LastPhiFound = II;
         return Phi != B;
      }

      /// Given no cached results, find if \p A comes before \p B in \p BB.
      /// Cache and number out instruction while walking \p BB.
      bool instComesBefore(const struct gimple_node *A, const struct gimple_node *B)
      {
         const struct gimple_node* Inst = nullptr;
         THROW_ASSERT(!(LastInstFound == BBInst->end() && NextInstPos != 0),
               "Instruction supposed to be in NumberedInsts");
      
         // Start the search with the instruction found in the last lookup round.
         auto II = BBInst->begin();
         auto IE = BBInst->end();
         if (LastInstFound != IE)
         {
            II = std::next(LastInstFound);
         }
      
         // Number all instructions up to the point where we find 'A' or 'B'.
         for (; II != IE; ++II) 
         {
            Inst = GetPointer<gimple_node>(GET_NODE(*II));
            NumberedInsts[Inst] = NextInstPos++;
            if (Inst == A || Inst == B)
               break;
         }
      
         THROW_ASSERT(II != IE, "Instruction not found?");
         THROW_ASSERT((Inst == A || Inst == B), "Should find A or B");
         LastInstFound = II;
         return Inst != B;
      }

    public:
      OrderedBasicBlock(const blocRef BasicB) : NextPhiPos(0), BBPhi(&(BasicB->CGetPhiList())), NextInstPos(0), BBInst(&(BasicB->CGetStmtList())), BB(BasicB)
      {
         LastPhiFound = BasicB->CGetPhiList().end();
         LastInstFound = BasicB->CGetStmtList().end();
      }

      /// Find out whether \p A dominates \p B, meaning whether \p A
      /// comes before \p B in \p BB. This is a simplification that considers
      /// cached instruction positions and ignores other basic blocks, being
      /// only relevant to compare relative instructions positions inside \p BB.
      /// Returns false for A == B.
      bool dominates(const struct gimple_node *A, const struct gimple_node *B)
      {
         THROW_ASSERT(A->bb_index == B->bb_index, "Instructions must be in the same basic block!");
         THROW_ASSERT(A->bb_index == BB->number, "Instructions must be in the tracked block!");

         // Phi statements always comes before non-phi statements
         if(A->get_kind() == gimple_phi_K && B->get_kind() != gimple_phi_K)
         {
            return true;
         }
         else if(A->get_kind() != gimple_phi_K && B->get_kind() == gimple_phi_K)
         {
            return false;
         }
      
         // First we lookup the instructions/phis. If they don't exist, lookup will give us
         // back ::end(). If they both exist, we compare the numbers. Otherwise, if NA
         // exists and NB doesn't, it means NA must come before NB because we would
         // have numbered NB as well if it didn't. The same is true for NB. If it
         // exists, but NA does not, NA must come after it. If neither exist, we need
         // to number the block and cache the results (by calling instComesBefore or phiComesBefore).
         const auto& nodeList = A->get_kind() == gimple_phi_K ? NumberedPhis : NumberedInsts;
         auto NAI = nodeList.find(A);
         auto NBI = nodeList.find(B);
         if (NAI != nodeList.end() && NBI != nodeList.end())
         {
            return NAI->second < NBI->second;
         }
         if (NAI != nodeList.end())
         {
            return true;
         }
         if (NBI != nodeList.end())
         {
            return false;
         }

         return A->get_kind() == gimple_phi_K ? phiComesBefore(A, B) : instComesBefore(A, B);
      }
   };

   bool dominates(unsigned int BBIA, unsigned int BBIB, BBGraphRef DT)
   {
      const auto& BBmap = DT->CGetBBGraphInfo()->bb_index_map;

      const auto BBAi_v = BBmap.find(BBIA);
      const auto BBBi_v = BBmap.find(BBIB);
      THROW_ASSERT(BBAi_v != BBmap.end(), "Unknown BB index (" + boost::lexical_cast<std::string>(BBIA) + ")");
      THROW_ASSERT(BBBi_v != BBmap.end(), "Unknown BB index (" + boost::lexical_cast<std::string>(BBIB) + ")");

      // An unreachable block is dominated by anything.
      if(!DT->IsReachable(BBmap.at(bloc::ENTRY_BLOCK_ID), BBBi_v->second))
      {
         return true;
      }

      // And dominates nothing.
      if(!DT->IsReachable(BBmap.at(bloc::ENTRY_BLOCK_ID), BBAi_v->second))
      {
         return false;
      }

      /// When block B is reachable from block A in the DT, A dominates B
      /// This because the DT used is a tree composed by immediate dominators only
      if(DT->IsReachable(BBAi_v->second, BBBi_v->second))
      {
         return true;
      }

      return false;
   }

   class OrderedInstructions
   {
      /// Used to check dominance for instructions in same basic block.
      mutable std::map<unsigned int, std::unique_ptr<OrderedBasicBlock>> OBBMap;

      /// The dominator tree of the parent function.
      BBGraphRef DT;

    public:
      /// Constructor.
      OrderedInstructions(BBGraphRef DT) : DT(DT)
      {
      }

      /// Return true if first instruction dominates the second.
      bool dominates(const struct gimple_node *InstA, const struct gimple_node *InstB) const
      {
         THROW_ASSERT(InstA, "Instruction A cannot be null");
         THROW_ASSERT(InstB, "Instruction B cannot be null");

         const auto BBIA = InstA->bb_index;
         const auto BBIB = InstB->bb_index;
         const auto BBmap = DT->CGetBBGraphInfo()->bb_index_map;

         // Use ordered basic block to do dominance check in case the 2 instructions
         // are in the same basic block.
         if(BBIA == BBIB)
         {
            auto OBB = OBBMap.find(BBIA);
            if(OBB == OBBMap.end())
            {
               const auto BBvertex = BBmap.find(BBIA);
               THROW_ASSERT(BBvertex != BBmap.end(), "Unknown BB index (" + boost::lexical_cast<std::string>(BBIA) + ")");

               OBB = OBBMap.insert({BBIA, std::make_unique<OrderedBasicBlock>(DT->CGetBBNodeInfo(BBvertex->second)->block)}).first;
            }
            return OBB->second->dominates(InstA, InstB);
         }

         return eSSAInfo::dominates(BBIA, BBIB, DT);
      }

      /// Invalidate the OrderedBasicBlock cache when its basic block changes.
      /// i.e. If an instruction is deleted or added to the basic block, the user
      /// should call this function to invalidate the OrderedBasicBlock cache for
      /// this basic block.
      void invalidateBlock(unsigned int BBI)
      {
         OBBMap.erase(BBI);
      }
   };

   class Operand
   {
    private:
      tree_nodeRef operand;

      const tree_nodeRef user_stmt;

    public:
      Operand(const tree_nodeRef _operand, const tree_nodeRef stmt) : operand(_operand), user_stmt(stmt)
      {
         THROW_ASSERT(GetPointer<gimple_node>(GET_NODE(stmt)), "stmt should be a gimple_node");
      }

      const tree_nodeRef getOperand() const
      {
         return operand;
      }

      const tree_nodeRef getUser() const
      {
         return user_stmt;
      }

      void set(tree_nodeRef new_ssa, tree_managerRef TM)
      {
         THROW_ASSERT(GET_NODE(new_ssa)->get_kind() == ssa_name_K, "New variable should be an ssa_name");
         THROW_ASSERT(GET_NODE(operand)->get_kind() == ssa_name_K, "Old variable should be an ssa_name");
         THROW_ASSERT(TM, "Null reference to tree manager");

         TM->ReplaceTreeNode(user_stmt, operand, new_ssa);
         operand = new_ssa;
      }
   };
   typedef std::shared_ptr<Operand> OperandRef;

   class PredicateBase
   {
    public:
      kind Type;
      // The original operand before we renamed it.
      // This can be use by passes, when destroying predicateinfo, to know
      // whether they can just drop the intrinsic, or have to merge metadata.
      tree_nodeRef OriginalOp;
      PredicateBase(const PredicateBase&) = delete;
      PredicateBase& operator=(const PredicateBase&) = delete;
      PredicateBase() = delete;
      virtual ~PredicateBase() = default;

    protected:
      PredicateBase(kind PT, tree_nodeRef Op) : Type(PT), OriginalOp(Op)
      {
      }
   };

   class PredicateWithCondition : public PredicateBase
   {
    public:
      tree_nodeRef Condition;
      static bool classof(const PredicateBase* PB)
      {
         return PB->Type == gimple_cond_K || PB->Type == gimple_multi_way_if_K;
      }

    protected:
      PredicateWithCondition(kind PT, tree_nodeRef Op, tree_nodeRef Cond) : PredicateBase(PT, Op), Condition(Cond)
      {
      }
   };

   // Mixin class for edge predicates.  The FROM block is the block where the
   // predicate originates, and the TO block is the block where the predicate is
   // valid.
   class PredicateWithEdge : public PredicateWithCondition
   {
    public:
      blocRef From;
      blocRef To;
      PredicateWithEdge() = delete;
      static bool classof(const PredicateBase* PB)
      {
         return PB->Type == gimple_cond_K || PB->Type == gimple_multi_way_if_K;
      }

    protected:
      PredicateWithEdge(kind PType, tree_nodeRef Op, blocRef _From, blocRef _To, tree_nodeRef Cond) : PredicateWithCondition(PType, Op, Cond), From(_From), To(_To)
      {
      }
   };

   // Provides predicate information for branches.
   class PredicateBranch : public PredicateWithEdge
   {
    public:
      // If true, SplitBB is the true successor, otherwise it's the false successor.
      bool TrueEdge;
      PredicateBranch(tree_nodeRef Op, blocRef BranchBB, blocRef SplitBB, tree_nodeRef Cond, bool TakenEdge) : PredicateWithEdge(gimple_cond_K, Op, BranchBB, SplitBB, Cond), TrueEdge(TakenEdge)
      {
      }
      PredicateBranch() = delete;
      static bool classof(const PredicateBase* PB)
      {
         return PB->Type == gimple_cond_K;
      }
   };

   // Given a predicate info that is a type of branching terminator, get the
   // branching block.
   const blocRef getBranchBlock(const PredicateBase* PB)
   {
      THROW_ASSERT(PB->Type == gimple_cond_K, 
         "Only branches and switches should have PHIOnly defs that require branch blocks.");
      return reinterpret_cast<const PredicateBranch*>(PB)->From;
   }

   // Used to store information about each value we might rename.
   struct ValueInfo
   {
      // Information about each possible copy. During processing, this is each
      // inserted info. After processing, we move the uninserted ones to the
      // uninserted vector.
      std::vector<PredicateBase*> Infos;
      std::vector<PredicateBase*> UninsertedInfos;
   };

   const ValueInfo& getValueInfo(tree_nodeRef Operand, std::map<tree_nodeRef, unsigned int>& ValueInfoNums, std::vector<ValueInfo>& ValueInfos)
   {
      auto OIN = ValueInfoNums.find(Operand);
      THROW_ASSERT(OIN != ValueInfoNums.end(), "Operand was not really in the Value Info Numbers");
      auto OINI = OIN->second;
      THROW_ASSERT(OINI < ValueInfos.size(), "Value Info Number greater than size of Value Info Table");
      return ValueInfos[OINI];
   }

   ValueInfo& getOrCreateValueInfo(tree_nodeRef Operand, std::map<tree_nodeRef, unsigned int>& ValueInfoNums, std::vector<ValueInfo>& ValueInfos)
   {
      auto OIN = ValueInfoNums.find(Operand);
      if(OIN == ValueInfoNums.end())
      {
         // This will grow it
         ValueInfos.resize(ValueInfos.size() + 1);
         // This will use the new size and give us a 0 based number of the info
         auto InsertResult = ValueInfoNums.insert({Operand, ValueInfos.size() - 1});
         THROW_ASSERT(InsertResult.second, "Value info number already existed?");
         return ValueInfos[InsertResult.first->second];
      }
      return ValueInfos[OIN->second];
   }

   void addInfoFor(OperandRef Op, PredicateBase* PB, std::set<OperandRef>& OpsToRename, std::map<tree_nodeRef, unsigned int>& ValueInfoNums, std::vector<eSSAInfo::ValueInfo>& ValueInfos)
   {
      OpsToRename.insert(Op);
      auto& OperandInfo = getOrCreateValueInfo(Op->getOperand(), ValueInfoNums, ValueInfos);
      OperandInfo.Infos.push_back(PB);
   }

   bool isCompare(struct binary_expr* condition)
   {
      auto c_type = condition->get_kind();
      return c_type == eq_expr_K || c_type == ne_expr_K || c_type == ltgt_expr_K || c_type == uneq_expr_K
         || c_type == gt_expr_K || c_type == lt_expr_K || c_type == ge_expr_K || c_type == le_expr_K 
         || c_type == unlt_expr_K || c_type == ungt_expr_K || c_type == unle_expr_K || c_type == unge_expr_K;
   }

   Operand branchOpRecurse(tree_nodeRef op, tree_nodeRef stmt)
   {
      const auto Op = GET_NODE(op);
      if(auto* nop = GetPointer<nop_expr>(Op))
      {
         return branchOpRecurse(nop->op, stmt);
      }
      else if(auto* ssa = GetPointer<ssa_name>(Op))
      {
         auto ga = ssa->CGetDefStmt();
         auto* def = GetPointer<gimple_assign>(GET_NODE(ga));
         return branchOpRecurse(def->op1, ga);
      }
      return Operand(op, stmt);
   }

   void processBranch(tree_nodeRef bi, blocRef BranchBB, 
      std::set<OperandRef>& OpsToRename, std::map<tree_nodeRef, unsigned int>& ValueInfoNums, std::vector<eSSAInfo::ValueInfo>& ValueInfos, 
      std::set<std::pair<blocRef, blocRef>>& EdgeUsesOnly, const std::map<unsigned int, blocRef> BBs, int debug_level)
   {
      const auto* BI = GetPointer<gimple_cond>(GET_NODE(bi));
      THROW_ASSERT(BI, "Branch instruction should be gimple_cond");
      const auto TrueBB = BBs.at(BranchBB->true_edge);
      const auto FalseBB = BBs.at(BranchBB->false_edge);
      const std::vector<blocRef> SuccsToProcess = {TrueBB, FalseBB};

      THROW_ASSERT(GET_NODE(BI->op0)->get_kind() == ssa_name_K, "Non SSA variable found in branch");
      const auto condOp = branchOpRecurse(BI->op0, bi);
      const auto cond = condOp.getOperand();

      auto InsertHelper = [&](tree_nodeRef Op, tree_nodeRef Cond)
      {
         for(const auto& Succ : SuccsToProcess)
         {
            if(Succ == BranchBB)
            {
               continue;
            }

            bool TakenEdge = (Succ == TrueBB);

            PredicateBase* PB = new PredicateBranch(Op, BranchBB, Succ, Cond, TakenEdge);
            addInfoFor(OperandRef(new Operand(Op, condOp.getUser())), PB, OpsToRename, ValueInfoNums, ValueInfos);
            if(Succ->list_of_pred.size() > 1)
            {
               EdgeUsesOnly.insert({BranchBB, Succ});
            }
         }
      };

      PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
         " <-- eSSA process branch: branch condition is " << GET_NODE(cond)->get_kind_text());

      if(auto* bin = GetPointer<binary_expr>(GET_NODE(cond)))
      {
         if(isCompare(bin))
         {
            const auto lhs = GET_NODE(bin->op0);
            const auto rhs = GET_NODE(bin->op1);

            PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
               " <-- eSSA process branch: " << bin->ToString());

            if(lhs != rhs)
            {
               // TODO: why is it needed?
               //    InsertHelper(cond, cond);

               // TODO: check that lhs and rhs are used more than once
               if(!GetPointer<cst_node>(lhs))
               {
                  InsertHelper(bin->op0, cond);
               }

               if(!GetPointer<cst_node>(rhs))
               {
                  InsertHelper(bin->op1, cond);
               }
            }
         }
         else if(bin->get_kind() == truth_and_expr_K || bin->get_kind() == truth_or_expr_K)
         {
            InsertHelper(cond, cond);
         }
         else
         {
            //    THROW_UNREACHABLE("Unknown condition type (" + bin->get_kind_text() + ")");
            PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
               " <-- eSSA process branch: unknown condition type (" + bin->get_kind_text() + ")");
         }
      }
      else
      {
         PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
            " <-- eSSA process branch: unhandled condition type (" << GET_NODE(cond)->get_kind_text() << ")");
      }
   }

   // Perform a strict weak ordering on instructions and arguments.
   bool valueComesBefore(OrderedInstructions& OI, tree_nodeRef A, tree_nodeRef B)
   {
      //    auto* ArgA = llvm::dyn_cast_or_null<llvm::Argument>(A);
      //    auto* ArgB = llvm::dyn_cast_or_null<llvm::Argument>(B);
      //    if(ArgA && !ArgB)
      //    {
      //       return true;
      //    }
      //    if(ArgB && !ArgA)
      //    {
      //       return false;
      //    }
      //    if(ArgA && ArgB)
      //    {
      //       return ArgA->getArgNo() < ArgB->getArgNo();
      //    }
      return OI.dominates(GetPointer<gimple_node>(GET_NODE(A)), GetPointer<gimple_node>(GET_NODE(B)));
   }

   // Given a predicate info that is a type of branching terminator, get the
   // edge this predicate info represents
   const std::pair<blocRef, blocRef> getBlockEdge(const PredicateBase* PB)
   {
      THROW_ASSERT(PredicateWithEdge::classof(PB), "Not a predicate info type we know how to get an edge from.");
      const auto* PEdge = reinterpret_cast<const PredicateWithEdge*>(PB);
      return std::make_pair(PEdge->From, PEdge->To);
   }

   struct DFSInfo
   {
      unsigned int DFSIn = 0;
      unsigned int DFSOut = 0;
   };

   enum LocalNum
   {
      // Operations that must appear first in the block.
      LN_First,
      // Operations that are somewhere in the middle of the block, and are sorted on
      // demand.
      LN_Middle,
      // Operations that must appear last in a block, like successor phi node uses.
      LN_Last
   };

   // Associate global and local DFS info with defs and uses, so we can sort them
   // into a global domination ordering.
   struct ValueDFS
   {
      unsigned int DFSIn = 0;
      unsigned int DFSOut = 0;
      unsigned int LocalNum = LN_Middle;
      // Only one of Def or Use will be set.
      tree_nodeRef Def = nullptr;
      OperandRef U = nullptr;
      // Neither PInfo nor EdgeOnly participate in the ordering
      PredicateBase* PInfo = nullptr;
      bool EdgeOnly = false;
   };

   // This compares ValueDFS structures, creating OrderedBasicBlocks where
   // necessary to compare uses/defs in the same block.  Doing so allows us to walk
   // the minimum number of instructions necessary to compute our def/use ordering.
   struct ValueDFS_Compare
   {
      OrderedInstructions& OI;
      ValueDFS_Compare(OrderedInstructions& _OI) : OI(_OI)
      {
      }

      // For a phi use, or a non-materialized def, return the edge it represents.
      const std::pair<unsigned int, unsigned int> getBlockEdge_local(const ValueDFS& VD) const
      {
         if(!VD.Def && VD.U)
         {
            auto* PHI = GetPointer<gimple_phi>(GET_NODE(VD.U->getUser()));
            auto phiDefEdge = std::find_if(PHI->CGetDefEdgesList().begin(), PHI->CGetDefEdgesList().end(), 
               [&](const gimple_phi::DefEdge& de) { return GET_INDEX_NODE(de.first) == GET_INDEX_NODE(VD.U->getOperand()); });
            THROW_ASSERT(phiDefEdge != PHI->CGetDefEdgesList().end(), "Unable to find variable in phi definitions");
            return std::make_pair(phiDefEdge->second, PHI->bb_index);
         }
         // This is really a non-materialized def.
         auto be = getBlockEdge(VD.PInfo);
         return std::make_pair(be.first->number, be.second->number);
      }

      // For two phi related values, return the ordering.
      bool comparePHIRelated(const ValueDFS& A, const ValueDFS& B) const
      {
         auto& ABlockEdge = getBlockEdge_local(A);
         auto& BBlockEdge = getBlockEdge_local(B);
         // Now sort by block edge and then defs before uses.
         return std::tie(ABlockEdge, A.Def, A.U) < std::tie(BBlockEdge, B.Def, B.U);
      }

      // Get the definition of an instruction that occurs in the middle of a block.
      tree_nodeRef getMiddleDef(const ValueDFS& VD) const
      {
         if(VD.Def)
         {
            return VD.Def;
         }
         // It's possible for the defs and uses to be null.  For branches, the local
         // numbering will say the placed predicateinfos should go first (IE
         // LN_first), so we won't be in this function. For assumes, we will end
         // up here, beause we need to order the def we will place relative to the
         // assume.  So for the purpose of ordering, we pretend the def is the assume
         // because that is where we will insert the info.
         //    if(!VD.U)
         //    {
         //       THROW_ASSERT(VD.PInfo, "No def, no use, and no predicateinfo should not occur");
         //       THROW_ASSERT(PredicateAssume::calssof(VD.PInfo), "Middle of block should only occur for assumes");
         //       return reinterpret_cast<PredicateAssume*>(VD.PInfo)->AssumeInst;
         //    }
         return nullptr;
      }

      // Return either the Def, if it's not null, or the user of the Use, if the def
      // is null.
      tree_nodeRef getDefOrUser(const tree_nodeRef Def, const OperandRef U) const
      {
         if(Def)
         {
            return Def;
         }
         return U->getUser();
      }

      // This performs the necessary local basic block ordering checks to tell
      // whether A comes before B, where both are in the same basic block.
      bool localComesBefore(const ValueDFS& A, const ValueDFS& B) const
      {
         auto ADef = getMiddleDef(A);
         auto BDef = getMiddleDef(B);

         // See if we have real values or uses. If we have real values, we are
         // guaranteed they are instructions or arguments. No matter what, we are
         // guaranteed they are in the same block if they are instructions.
         //    auto* ArgA = llvm::dyn_cast_or_null<llvm::Argument>(ADef);
         //    auto* ArgB = llvm::dyn_cast_or_null<llvm::Argument>(BDef);
         //    
         //    if(ArgA || ArgB)
         //    {
         //       return valueComesBefore(OI, ArgA, ArgB);
         //    }

         auto AInst = getDefOrUser(ADef, A.U);
         auto BInst = getDefOrUser(BDef, B.U);
         return valueComesBefore(OI, AInst, BInst);
      }

      bool operator()(const ValueDFS& A, const ValueDFS& B) const
      {
         if(&A == &B)
         {
            return false;
         }
         // The only case we can't directly compare them is when they in the same
         // block, and both have localnum == middle.  In that case, we have to use
         // comesbefore to see what the real ordering is, because they are in the
         // same basic block.

         bool SameBlock = std::tie(A.DFSIn, A.DFSOut) == std::tie(B.DFSIn, B.DFSOut);

         // We want to put the def that will get used for a given set of phi uses,
         // before those phi uses.
         // So we sort by edge, then by def.
         // Note that only phi nodes uses and defs can come last.
         if(SameBlock && A.LocalNum == LN_Last && B.LocalNum == LN_Last)
         {
            return comparePHIRelated(A, B);
         }

         if(!SameBlock || A.LocalNum != LN_Middle || B.LocalNum != LN_Middle)
         {
            return std::tie(A.DFSIn, A.DFSOut, A.LocalNum, A.Def, A.U) < std::tie(B.DFSIn, B.DFSOut, B.LocalNum, B.Def, B.U);
         }
         return localComesBefore(A, B);
      }
   };
   using ValueDFSStack = std::vector<ValueDFS>;

   bool stackIsInScope(const ValueDFSStack& Stack, const ValueDFS& VDUse, BBGraphRef DT)
   {
      if(Stack.empty())
      {
         return false;
      }
      // If it's a phi only use, make sure it's for this phi node edge, and that the
      // use is in a phi node.  If it's anything else, and the top of the stack is
      // EdgeOnly, we need to pop the stack.  We deliberately sort phi uses next to
      // the defs they must go with so that we can know it's time to pop the stack
      // when we hit the end of the phi uses for a given def.
      if(Stack.back().EdgeOnly)
      {
         if(!VDUse.U)
         {
            return false;
         }
         auto* PHI = GetPointer<gimple_phi>(GET_NODE(VDUse.U->getUser()));
         if(!PHI)
         {
            return false;
         }
         // Check edge
         auto EdgePredIt = std::find_if(PHI->CGetDefEdgesList().begin(), PHI->CGetDefEdgesList().end(), 
            [&](const gimple_phi::DefEdge& de) { return GET_INDEX_NODE(de.first) == GET_INDEX_NODE(VDUse.U->getOperand()); });
         THROW_ASSERT(EdgePredIt != PHI->CGetDefEdgesList().end(), "Unable to find variable in phi definitions");
         if(EdgePredIt->second != getBranchBlock(Stack.back().PInfo)->number)
         {
            return false;
         }

         const auto bbedge = getBlockEdge(Stack.back().PInfo);
         if(PHI->bb_index == bbedge.second->number && EdgePredIt->second == bbedge.first->number)
         {
            return true;
         }
         return dominates(bbedge.second->number, EdgePredIt->second, DT);
      }

      return (VDUse.DFSIn >= Stack.back().DFSIn && VDUse.DFSOut <= Stack.back().DFSOut);
   }

   void popStackUntilDFSScope(ValueDFSStack& Stack, const ValueDFS& VD, BBGraphRef DT)
   {
      while(!Stack.empty() && !stackIsInScope(Stack, VD, DT))
      {
         Stack.pop_back();
      }
   }

   // Convert the uses of Op into a vector of uses, associating global and local
   // DFS info with each one.
   void convertUsesToDFSOrdered(tree_nodeRef Op, std::vector<ValueDFS>& DFSOrderedSet, BBGraphRef DT, const std::unordered_map<unsigned int, DFSInfo>& DFSInfos)
   {
      auto* op = GetPointer<ssa_name>(GET_NODE(Op));
      THROW_ASSERT(op, "Op is not an ssa_name (" + GET_NODE(Op)->get_kind_text() + ")");
      for(auto& [U, size] : op->CGetUseStmts())
      {
         auto* I = GetPointer<gimple_node>(GET_NODE(U));
         THROW_ASSERT(I, "Use statement should be a gimple_node");
         ValueDFS VD;
         // Put the phi node uses in the incoming block.
         unsigned int IBlock;
         if(auto* PN = GetPointer<gimple_phi>(GET_NODE(U)))
         {
            auto phiDefEdge = std::find_if(PN->CGetDefEdgesList().begin(), PN->CGetDefEdgesList().end(), 
               [&](const gimple_phi::DefEdge& de) { return GET_INDEX_NODE(de.first) == GET_INDEX_NODE(Op); });
            THROW_ASSERT(phiDefEdge != PN->CGetDefEdgesList().end(), "Unable to find variable in phi definitions");
            IBlock = phiDefEdge->second;
            // Make phi node users appear last in the incoming block
            // they are from.
            VD.LocalNum = LN_Last;
         }
         else
         {
            // If it's not a phi node use, it is somewhere in the middle of the
            // block.
            IBlock = I->bb_index;
            VD.LocalNum = LN_Middle;
         }

         const auto& BBmap = DT->CGetBBGraphInfo()->bb_index_map;
         auto DomNode_vertex = BBmap.find(IBlock);
         THROW_ASSERT(DomNode_vertex != BBmap.end(), "BB" + boost::lexical_cast<std::string>(IBlock) + " not found in DT");
         // It's possible our use is in an unreachable block. Skip it if so.
         if(!DT->IsReachable(BBmap.at(bloc::ENTRY_BLOCK_ID), DomNode_vertex->second))
         {
            #ifdef DEBUG_eSSA
            PRINT_MSG(" <-- eSSA uses reored: BB" << IBlock << " is unreachable from DT root");
            #endif
            continue;
         }
         const auto& DomNode_DFSInfo = DFSInfos.at(IBlock);
         VD.DFSIn = DomNode_DFSInfo.DFSIn;
         VD.DFSOut = DomNode_DFSInfo.DFSOut;
         VD.U = OperandRef(new Operand(Op, U));
         DFSOrderedSet.push_back(VD);
      }
   }

   // Given the renaming stack, make all the operands currently on the stack real
   // by inserting them into the IR.  Return the last operation's value.
   tree_nodeRef materializeStack(ValueDFSStack& RenameStack, unsigned int function_id, tree_nodeRef OrigOp, std::map<tree_nodeRef, const PredicateBase*>& PredicateMap, tree_managerRef TM, tree_manipulationRef tree_man)
   {
      // Find the first thing we have to materialize
      auto RevIter = RenameStack.rbegin();
      for(; RevIter != RenameStack.rend(); ++RevIter)
      {
         if(RevIter->Def)
         {
            break;
         }
      }

      size_t Start = RevIter - RenameStack.rbegin();
      // The maximum number of things we should be trying to materialize at once
      // right now is 4, depending on if we had an assume, a branch, and both used
      // and of conditions.
      for(auto RenameIter = RenameStack.end() - Start; RenameIter != RenameStack.end(); ++RenameIter)
      {
         auto Op = RenameIter == RenameStack.begin() ? OrigOp : (RenameIter - 1)->Def;
         ValueDFS& Result = *RenameIter;
         auto* ValInfo = Result.PInfo;
         // For edge predicates, we can just place the operand in the block before
         // the terminator.  For assume, we have to place it right before the assume
         // to ensure we dominate all of our uses.  Always insert right before the
         // relevant instruction (terminator, assume), so that we insert in proper
         // order in the case of multiple predicateinfo in the same block.
         if(PredicateWithEdge::classof(ValInfo))
         {
            auto* pwe = reinterpret_cast<PredicateWithEdge*>(ValInfo);

            tree_nodeRef new_ssa_var;
            std::vector<std::pair<tree_nodeRef, unsigned int>> list_of_def_edge;
            list_of_def_edge.push_back(std::pair<tree_nodeRef, unsigned int>(Op, pwe->From->number));
            auto PIC = tree_man->create_phi_node(new_ssa_var, list_of_def_edge, TM->GetTreeReindex(function_id), pwe->To->number);
            // TODO: is it necessary to set keep at this step?
            //    GetPointer<gimple_phi>(GET_NODE(PIC))->keep = true;
            pwe->To->AddPhi(PIC);
            PredicateMap.insert({PIC, ValInfo});
            Result.Def = PIC;
         }
         //    else
         //    {
         //       llvm_unreachable("assume intrinsic not yet supported");
         //       auto* PAssume = llvm::dyn_cast<PredicateAssume>(ValInfo);
         //       assert(PAssume && "Should not have gotten here without it being an assume");
         //       // llvm::IRBuilder<> B(PAssume->AssumeInst);
         //       // llvm::Function *IF = llvm::Intrinsic::getDeclaration(
         //       //                        F.getParent(), llvm::Intrinsic::ssa_copy, Op->getType());
         //       // llvm::CallInst *PIC = B.CreateCall(IF, Op);
         //       // PredicateMap.insert({PIC, ValInfo});
         //       // Result.Def = PIC;
         //    }
      }
      return RenameStack.back().Def;
   }

   // Instead of the standard SSA renaming algorithm, which is O(Number of
   // instructions), and walks the entire dominator tree, we walk only the defs +
   // uses.  The standard SSA renaming algorithm does not really rely on the
   // dominator tree except to order the stack push/pops of the renaming stacks, so
   // that defs end up getting pushed before hitting the correct uses.  This does
   // not require the dominator tree, only the *order* of the dominator tree. The
   // complete and correct ordering of the defs and uses, in dominator tree is
   // contained in the DFS numbering of the dominator tree. So we sort the defs and
   // uses into the DFS ordering, and then just use the renaming stack as per
   // normal, pushing when we hit a def (which is a predicateinfo instruction),
   // popping when we are out of the dfs scope for that def, and replacing any uses
   // with top of stack if it exists.  In order to handle liveness without
   // propagating liveness info, we don't actually insert the predicateinfo
   // instruction def until we see a use that it would dominate.  Once we see such
   // a use, we materialize the predicateinfo instruction in the right place and
   // use it.
   //
   void renameUses(std::set<OperandRef>& OpSet, std::map<tree_nodeRef, unsigned int>& ValueInfoNums, std::vector<eSSAInfo::ValueInfo>& ValueInfos, 
      BBGraphRef DT, const std::unordered_map<unsigned int, DFSInfo>& DFSInfos, std::set<std::pair<blocRef, blocRef>>& EdgeUsesOnly, 
      application_managerRef AppM, ParameterConstRef parameters, unsigned int function_id, int debug_level)
   {
      const auto TM = AppM->get_tree_manager();
      // This maps from copy operands to Predicate Info. Note that it does not own
      // the Predicate Info, they belong to the ValueInfo structs in the ValueInfos
      // vector.
      std::map<tree_nodeRef, const eSSAInfo::PredicateBase*> PredicateMap;
      // Sort OpsToRename since we are going to iterate it.
      std::vector<OperandRef> OpsToRename(OpSet.begin(), OpSet.end());
      eSSAInfo::OrderedInstructions OI(DT);
      auto Comparator = [&](const OperandRef A, const OperandRef B) { return valueComesBefore(OI, A->getUser(), B->getUser()); };
      std::sort(OpsToRename.begin(), OpsToRename.end(), Comparator);
      eSSAInfo::ValueDFS_Compare Compare(OI);

      for(auto& Op : OpsToRename)
      {
         std::vector<eSSAInfo::ValueDFS> OrderedUses;
         const auto& ValueInfo = getValueInfo(Op->getOperand(), ValueInfoNums, ValueInfos);
         PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
            " <-- eSSA rename: renaming " << Op->getOperand()->ToString() << " with " << ValueInfo.Infos.size() << " possible copies");
         // Insert the possible copies into the def/use list.
         // They will become real copies if we find a real use for them, and never
         // created otherwise.
         for(auto& PossibleCopy : ValueInfo.Infos)
         {
            eSSAInfo::ValueDFS VD;
            // Determine where we are going to place the copy by the copy type.
            // The predicate info for branches always come first, they will get
            // materialized in the split block at the top of the block.
            // The predicate info for assumes will be somewhere in the middle,
            // it will get materialized in front of the assume.
            //    if(PredicateAssume::classof(PossibleCopy))
            //    {
            //       const auto* PAssume = reinterpret_cast<PredicateAssume*>(PossibleCopy);
            //       VD.LocalNum = LN_Middle;
            //       llvm::DomTreeNode* DomNode = DT->getNode(PAssume->AssumeInst->getParent());
            //       if(!DomNode)
            //       {
            //          continue;
            //       }
            //       const auto& DomNode_DFSInfo = DFSInfos.at(DomNode->number);
            //       VD.DFSIn = DomNode_DFSInfo.DFSIn;
            //       VD.DFSOut = DomNode_DFSInfo.DFSOut;
            //       VD.PInfo = PossibleCopy;
            //       OrderedUses.push_back(VD);
            //    }
            //    else 
            if(eSSAInfo::PredicateWithEdge::classof(PossibleCopy))
            {
               // If we can only do phi uses, we treat it like it's in the branch
               // block, and handle it specially. We know that it goes last, and only
               // dominate phi uses.
               auto BlockEdge = getBlockEdge(PossibleCopy);
               if(EdgeUsesOnly.count(BlockEdge))
               {
                  VD.LocalNum = eSSAInfo::LN_Last;
                  auto& DomNode = BlockEdge.first;
                  if(DomNode)
                  {
                     THROW_ASSERT(DFSInfos.find(DomNode->number) != DFSInfos.end(), "Invalid DT node");
                     const auto& DomNode_DFSInfo = DFSInfos.at(DomNode->number);
                     VD.DFSIn = DomNode_DFSInfo.DFSIn;
                     VD.DFSOut = DomNode_DFSInfo.DFSOut;
                     VD.PInfo = PossibleCopy;
                     VD.EdgeOnly = true;
                     OrderedUses.push_back(VD);
                  }
               }
               else
               {
                  // Otherwise, we are in the split block (even though we perform
                  // insertion in the branch block).
                  // Insert a possible copy at the split block and before the branch.
                  VD.LocalNum = eSSAInfo::LN_First;
                  auto& DomNode = BlockEdge.second;
                  if(DomNode)
                  {
                     THROW_ASSERT(DFSInfos.find(DomNode->number) != DFSInfos.end(), "Invalid DT node");
                     const auto& DomNode_DFSInfo = DFSInfos.at(DomNode->number);
                     VD.DFSIn = DomNode_DFSInfo.DFSIn;
                     VD.DFSOut = DomNode_DFSInfo.DFSOut;
                     VD.PInfo = PossibleCopy;
                     OrderedUses.push_back(VD);
                  }
               }
            }
         }
            
         convertUsesToDFSOrdered(Op->getOperand(), OrderedUses, DT, DFSInfos);
         // Here we require a stable sort because we do not bother to try to
         // assign an order to the operands the uses represent. Thus, two
         // uses in the same instruction do not have a strict sort order
         // currently and will be considered equal. We could get rid of the
         // stable sort by creating one if we wanted.
         std::stable_sort(OrderedUses.begin(), OrderedUses.end(), Compare);
         std::vector<eSSAInfo::ValueDFS> RenameStack;
         // For each use, sorted into dfs order, push values and replaces uses with
         // top of stack, which will represent the reaching def.
         for(auto& VD : OrderedUses)
         {
            // We currently do not materialize copy over copy, but we should decide if
            // we want to.
            bool PossibleCopy = VD.PInfo != nullptr;
            if(RenameStack.empty())
            {
               PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, " <-- eSSA rename: RenameStack is empty");
            }
            else
            {
               PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
                  " <-- eSSA rename: RenameStack Top DFS numbers are (" << RenameStack.back().DFSIn << "," << RenameStack.back().DFSOut << ")");
            }

            PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
               " <-- eSSA rename: current DFS numbers are (" << VD.DFSIn << "," << VD.DFSOut << ")");
            bool ShouldPush = (VD.Def || PossibleCopy);
            bool OutOfScope = !stackIsInScope(RenameStack, VD, DT);
            if(OutOfScope || ShouldPush)
            {
               // Sync to our current scope.
               popStackUntilDFSScope(RenameStack, VD, DT);
               if(ShouldPush)
               {
                  RenameStack.push_back(VD);
               }
            }
            // If we get to this point, and the stack is empty we must have a use
            // with no renaming needed, just skip it.
            if(RenameStack.empty())
            {
               PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
                  " <-- eSSA rename: current use needs no renaming");
               continue;
            }
            // Skip values, only want to rename the uses
            if(VD.Def || PossibleCopy)
            {
               continue;
            }

            eSSAInfo::ValueDFS& Result = RenameStack.back();

            // If the possible copy dominates something, materialize our stack up to
            // this point. This ensures every comparison that affects our operation
            // ends up with predicateinfo.
            if(!Result.Def)
            {
               Result.Def = materializeStack(RenameStack, function_id, Op->getOperand(), PredicateMap, 
                  TM, tree_manipulationRef(new tree_manipulation(TM, parameters)));
            }

            PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
               " <-- eSSA rename: found replacement " << GET_NODE(Result.Def) << " for " << GET_NODE(VD.U->getOperand()) << " in " << GET_NODE(VD.U->getUser()));
            //    THROW_ASSERT(DT->dominates(llvm::cast<llvm::Instruction>(Result.Def), *VD.U), "Predicateinfo def should have dominated this use");
            //    VD.U->set(Result.Def);
            // TODO: fix phi ordering in OrderedBasicBlock
            //    THROW_ASSERT(valueComesBefore(OI, Result.Def, VD.U->getUser()), "Predicateinfo def should have dominated this use");
            auto* phi = GetPointer<gimple_phi>(GET_NODE(Result.Def));
            VD.U->set(phi->res, TM);
         }
      }
   }
}

eSSA::eSSA(const ParameterConstRef params, const application_managerRef AM, unsigned int f_id, const DesignFlowManagerConstRef dfm)
    : FunctionFrontendFlowStep(AM, f_id, ESSA, dfm, params)
{
   debug_level = parameters->get_class_debug_level(GET_CLASS(*this), DEBUG_LEVEL_NONE);
}

eSSA::~eSSA() = default;

const std::unordered_set<std::pair<FrontendFlowStepType, FrontendFlowStep::FunctionRelationship>> 
eSSA::ComputeFrontendRelationships(const DesignFlowStep::RelationshipType relationship_type) const
{
   std::unordered_set<std::pair<FrontendFlowStepType, FunctionRelationship>> relationships;
   switch(relationship_type)
   {
      case(PRECEDENCE_RELATIONSHIP):
      {
         break;
      }
      case DEPENDENCE_RELATIONSHIP:
      {
         relationships.insert(std::make_pair(IR_LOWERING, SAME_FUNCTION));
         relationships.insert(std::make_pair(EXTRACT_GIMPLE_COND_OP, SAME_FUNCTION));
         break;
      }
      case(INVALIDATION_RELATIONSHIP):
      {
         break;
      }
      default:
         THROW_UNREACHABLE("");
   }
   return relationships;
}

DesignFlowStep_Status eSSA::InternalExec()
{
   auto TM = AppM->get_tree_manager();
   tree_nodeRef temp = TM->get_tree_node_const(function_id);
   auto* fd = GetPointer<function_decl>(temp);
   auto sl = GetPointer<statement_list>(GET_NODE(fd->body));
   /// store the GCC BB graph ala boost::graph
   BBGraphsCollectionRef GCC_bb_graphs_collection(new BBGraphsCollection(BBGraphInfoRef(new BBGraphInfo(AppM, function_id)), parameters));
   BBGraphRef GCC_bb_graph(new BBGraph(GCC_bb_graphs_collection, CFG_SELECTOR));
   std::unordered_map<unsigned int, vertex> inverse_vertex_map;
   /// add vertices
   for(auto block : sl->list_of_bloc)
   {
      inverse_vertex_map[block.first] = GCC_bb_graphs_collection->AddVertex(BBNodeInfoRef(new BBNodeInfo(block.second)));
   }
   /// add edges
   for(auto curr_bb_pair : sl->list_of_bloc)
   {
      unsigned int curr_bb = curr_bb_pair.first;
      std::vector<unsigned int>::const_iterator lop_it_end = sl->list_of_bloc[curr_bb]->list_of_pred.end();
      for(std::vector<unsigned int>::const_iterator lop_it = sl->list_of_bloc[curr_bb]->list_of_pred.begin(); lop_it != lop_it_end; ++lop_it)
      {
         THROW_ASSERT(inverse_vertex_map.find(*lop_it) != inverse_vertex_map.end(), "BB" + STR(*lop_it) + " (successor of BB" + STR(curr_bb) + ") does not exist");
         GCC_bb_graphs_collection->AddEdge(inverse_vertex_map[*lop_it], inverse_vertex_map[curr_bb], CFG_SELECTOR);
      }
      std::vector<unsigned int>::const_iterator los_it_end = sl->list_of_bloc[curr_bb]->list_of_succ.end();
      for(std::vector<unsigned int>::const_iterator los_it = sl->list_of_bloc[curr_bb]->list_of_succ.begin(); los_it != los_it_end; ++los_it)
      {
         if(*los_it == bloc::EXIT_BLOCK_ID)
            GCC_bb_graphs_collection->AddEdge(inverse_vertex_map[curr_bb], inverse_vertex_map[*los_it], CFG_SELECTOR);
      }
      if(sl->list_of_bloc[curr_bb]->list_of_succ.empty())
         GCC_bb_graphs_collection->AddEdge(inverse_vertex_map[curr_bb], inverse_vertex_map[bloc::EXIT_BLOCK_ID], CFG_SELECTOR);
   }
   /// add a connection between entry and exit thus avoiding problems with non terminating code
   GCC_bb_graphs_collection->AddEdge(inverse_vertex_map[bloc::ENTRY_BLOCK_ID], inverse_vertex_map[bloc::EXIT_BLOCK_ID], CFG_SELECTOR);

   refcount<dominance<BBGraph>> bb_dominators;
   bb_dominators = refcount<dominance<BBGraph>>(new dominance<BBGraph>(*GCC_bb_graph, inverse_vertex_map[bloc::ENTRY_BLOCK_ID], inverse_vertex_map[bloc::EXIT_BLOCK_ID], parameters));
   bb_dominators->calculate_dominance_info(dominance<BBGraph>::CDI_DOMINATORS);
   const std::unordered_map<vertex, vertex>& bb_dominator_map = bb_dominators->get_dominator_map();

   BBGraphRef DT(new BBGraph(GCC_bb_graphs_collection, D_SELECTOR));
   for(auto it : bb_dominator_map)
   {
      if(it.first != inverse_vertex_map[bloc::ENTRY_BLOCK_ID])
      {
         GCC_bb_graphs_collection->AddEdge(it.second, it.first, D_SELECTOR);
      }
   }
   DT->GetBBGraphInfo()->bb_index_map = inverse_vertex_map;

   PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
         " <-- eSSA: Dominator tree has " << DT->num_bblocks() << " BB");

   // This stores info about each operand or comparison result we make copies
   // of.  The real ValueInfos start at index 1, index 0 is unused so that we can
   // more easily detect invalid indexing.
   std::vector<eSSAInfo::ValueInfo> ValueInfos;
   ValueInfos.resize(1);
   // This gives the index into the ValueInfos array for a given Value.  Because
   // 0 is not a valid Value Info index, you can use DenseMap::lookup and tell
   // whether it returned a valid result.
   std::map<tree_nodeRef, unsigned int> ValueInfoNums;
   // The set of edges along which we can only handle phi uses, due to critical edges.
   std::set<std::pair<blocRef, blocRef>> EdgeUsesOnly;

   // Collect operands to rename from all conditional branch terminators, as well
   // as assume statements.
   std::set<eSSAInfo::OperandRef> OpsToRename;

   auto BBvisit = [&](blocRef BB)
   {
      const auto& stmt_list = BB->CGetStmtList();

      // Skip entry/exit empty BB
      if(stmt_list.empty())
      {
         PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
         " <-- eSSA: BB" << BB->number << " is empty");
         return;
      }

      const auto terminator = stmt_list.back();

      PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
         " <-- eSSA: BB" << BB->number << " has " << GET_NODE(terminator)->get_kind_text() << " as terminator");
      
      if(GET_NODE(terminator)->get_kind() == gimple_cond_K)
      {
         // Can't insert conditional information if they all go to the same place.
         if(BB->true_edge == BB->false_edge)
         {
            PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
               " <-- eSSA: branch true_edge == false_edge");
            return;
         }

         eSSAInfo::processBranch(terminator, BB, OpsToRename, ValueInfoNums, ValueInfos, EdgeUsesOnly, sl->list_of_bloc, debug_level);
      }
      // TODO: add multi way if handler
   };

   // This map stores DFS numeration for each DT node
   std::unordered_map<unsigned int, eSSAInfo::DFSInfo> DFSInfos;

   std::stack<std::pair<vertex, boost::iterator_range<BBGraph::adjacency_iterator>>> workStack; 
   workStack.push({inverse_vertex_map[bloc::ENTRY_BLOCK_ID], boost::make_iterator_range(boost::adjacent_vertices(inverse_vertex_map[bloc::ENTRY_BLOCK_ID], *DT))});

   unsigned int DFSNum = 0;
   const auto& BBentry = DT->CGetBBNodeInfo(workStack.top().first)->block;
   DFSInfos[BBentry->number].DFSIn = DFSNum++;
   BBvisit(BBentry);

   while (!workStack.empty())
   {
      const auto& BB = DT->CGetBBNodeInfo(workStack.top().first)->block;
      auto ChildRange = workStack.top().second;

      // If we visited all of the children of this node, "recurse" back up the
      // stack setting the DFOutNum.
      if(ChildRange.empty())
      {
         DFSInfos[BB->number].DFSOut = DFSNum++;
         workStack.pop();
      }
      else
      {
         // Otherwise, recursively visit this child.
         const auto Child = DT->CGetBBNodeInfo(ChildRange.front())->block;
         workStack.top().second.pop_front();

         workStack.push({ChildRange.front(), boost::make_iterator_range(boost::adjacent_vertices(ChildRange.front(), *DT))});
         DFSInfos[Child->number].DFSIn = DFSNum++;
         
         // Perform BB analysis for eSSA purpose
         BBvisit(Child);
      }
   }
   if(DFSInfos.size() < (DT->num_bblocks() + 2))
   {
      PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
                  " <-- eSSA: dominator tree has some unreachable nodes");
   }

   PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
         " <-- eSSA: " << OpsToRename.size() << " operations to rename found");

   if(OpsToRename.empty())
   {
      return DesignFlowStep_Status::UNCHANGED;
   }

   #ifdef DEBUG_eSSA
   DT->WriteDot("before_eSSA_DT.dot");
   #endif

   eSSAInfo::renameUses(OpsToRename, ValueInfoNums, ValueInfos, DT, DFSInfos, EdgeUsesOnly, AppM, parameters, function_id, debug_level);

   #ifdef DEBUG_eSSA
   DT->WriteDot("after_eSSA_DT.dot");
   #endif

   return DesignFlowStep_Status::SUCCESS;
}

void eSSA::Initialize()
{
}
