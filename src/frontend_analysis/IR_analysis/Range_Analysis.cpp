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
 * @file Range_Analysis.cpp
 * @brief 
 *
 * @author Michele Fiorito <michele2.fiorito@mail.polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
 */

#include "Range_Analysis.hpp"

///. include
#include "Parameter.hpp"

/// behavior includes
#include "application_manager.hpp"
#include "basic_block.hpp"
#include "call_graph.hpp"
#include "call_graph_manager.hpp"
#include "graph.hpp"
#include "function_behavior.hpp"
#include "op_graph.hpp"

/// design_flows include
#include "design_flow_manager.hpp"

/// stl
#include <map>
#include <sstream>
#include <vector>

/// Tree includes
#include "token_interface.hpp"
#include "tree_basic_block.hpp"
#include "tree_helper.hpp"
#include "tree_manager.hpp"
#include "ext_tree_node.hpp"
#include "tree_reindex.hpp"

#include <boost/filesystem/operations.hpp> // for create_directories
#include "dbgPrintHelper.hpp" // for DEBUG_LEVEL_
#include "string_manipulation.hpp" // for GET_CLASS

#define RA_JUMPSET

#define DEBUG_RANGE_OP
#define DEBUG_BASICOP_EVAL
#define DEBUG_CGRAPH
#define DEBUG_RA
#define SCC_DEBUG

using APInt = Range::APInt;
using UAPInt = boost::multiprecision::uint128_t;

bool tree_reindexCompare::operator()(const tree_nodeConstRef &lhs, const tree_nodeConstRef &rhs) const
{
   return GET_INDEX_CONST_NODE(lhs) < GET_INDEX_CONST_NODE(rhs);
}

namespace 
{
   union IntFloat {
      int i;
      float f;
   };

   union LongDouble {
      long l;
      double d;
   };

   // The number of bits needed to store the largest variable of the function (APInt).
   unsigned MAX_BIT_INT = 0;
   unsigned POINTER_BITSIZE = 32;

   // ========================================================================== //
   // Static global functions and definitions
   // ========================================================================== //

   APInt Min, Max;
   APInt Epsilon = 1;

   // Used to print pseudo-edges in the Constraint Graph dot
   std::string pestring;
   std::stringstream pseudoEdgesString(pestring);

   APInt getMaxValue(unsigned bitwidth)
   {
      return APInt((boost::multiprecision::uint256_t(1) << bitwidth) - 1);
   }

   APInt getMinValue(unsigned bitwidth)
   {
      return APInt(0);
   }

   APInt getSignedMaxValue(unsigned bitwidth)
   {
      return (APInt(1) << (bitwidth - 1)) - 1;
   }

   APInt getSignedMinValue(unsigned bitwidth)
   {
      return -(APInt(1) << (bitwidth - 1));
   }

   APInt truncExt(const APInt& x, unsigned bitwidth, bool sign)
   {
      APInt bitmask = (APInt(1) << bitwidth) - 1;
      APInt new_val = x & bitmask;
      if(sign && boost::multiprecision::bit_test(new_val, bitwidth - 1))
      {
         return (APInt(-1) << bitwidth) + new_val;
      }
      return new_val;
   }

   APInt lshr(const APInt& a, unsigned p)
   {
      APInt mask = (APInt(1) << (128 - p)) - 1;
      return (a >> p) & mask;
   }

   UAPInt lshr(const UAPInt& a, unsigned p)
   {
      if(p > 128)
      {
         return 0;
      }
      UAPInt mask = (UAPInt(1) << (128 - p)) - 1;
      return (a >> p) & mask;
   }

   unsigned countLeadingZeros(const APInt& a, unsigned bw)
   {
         int i = bw - 1;
         for(; i >= 0; --i)
         {
            if(bit_test(a, i))
            {
               break;
            }
         }
         i++;
         return bw - i;
   }

   unsigned countLeadingOnes(const APInt& a, unsigned bw)
   {
         int i = bw - 1;
         for(; i >= 0; --i)
         {
            if(!bit_test(a, i))
            {
               break;
            }
         }
         i++;
         return bw - i;
   }

   kind op_inv(kind op)
   {
      switch (op)
      {
      case ge_expr_K:
         return lt_expr_K;
      case gt_expr_K:
         return le_expr_K;
      case unge_expr_K:
         return unlt_expr_K;
      case ungt_expr_K:
         return unle_expr_K;
      case unle_expr_K:
         return ungt_expr_K;
      case unlt_expr_K:
         return unge_expr_K;
      case eq_expr_K:
         return ne_expr_K;
      case ne_expr_K:
         return eq_expr_K;
      case uneq_expr_K:
         return ne_expr_K;
      
      case assert_expr_K:
      case catch_expr_K:
      case ceil_div_expr_K:
      case ceil_mod_expr_K:
      case complex_expr_K:
      case compound_expr_K:
      case eh_filter_expr_K:
      case exact_div_expr_K:
      case fdesc_expr_K:
      case floor_div_expr_K:
      case floor_mod_expr_K:
      case goto_subroutine_K:
      case in_expr_K:
      case init_expr_K:
      case le_expr_K:
      case lrotate_expr_K:
      case lshift_expr_K:
      case lt_expr_K:
      case max_expr_K:
      case mem_ref_K:
      case min_expr_K:
      case minus_expr_K:
      case modify_expr_K:
      case mult_expr_K:
      case mult_highpart_expr_K:
      case ordered_expr_K:
      case plus_expr_K:
      case pointer_plus_expr_K:
      case postdecrement_expr_K:
      case postincrement_expr_K:
      case predecrement_expr_K:
      case preincrement_expr_K:
      case range_expr_K:
      case rdiv_expr_K:
      case round_div_expr_K:
      case round_mod_expr_K:
      case rrotate_expr_K:
      case rshift_expr_K:
      case set_le_expr_K:
      case trunc_div_expr_K:
      case trunc_mod_expr_K:
      case truth_and_expr_K:
      case truth_andif_expr_K:
      case truth_or_expr_K:
      case truth_orif_expr_K:
      case truth_xor_expr_K:
      case try_catch_expr_K:
      case try_finally_K:
      case ltgt_expr_K:
      case unordered_expr_K:
      case widen_sum_expr_K:
      case widen_mult_expr_K:
      case with_size_expr_K:
      case vec_lshift_expr_K:
      case vec_rshift_expr_K:
      case widen_mult_hi_expr_K:
      case widen_mult_lo_expr_K:
      case vec_pack_trunc_expr_K:
      case vec_pack_sat_expr_K:
      case vec_pack_fix_trunc_expr_K:
      case vec_extracteven_expr_K:
      case vec_extractodd_expr_K:
      case vec_interleavehigh_expr_K:
      case vec_interleavelow_expr_K:
      case extract_bit_expr_K:
      default:
         break;
      }

      THROW_UNREACHABLE("Unhandled predicate (" + boost::lexical_cast<std::string>(op) + ")");
   }

   kind op_swap(kind op)
   {
      switch (op)
      {
         case ge_expr_K:
            return le_expr_K;
         case gt_expr_K:
            return lt_expr_K;
         case le_expr_K:
            return ge_expr_K;
         case lt_expr_K:
            return gt_expr_K;
         case unge_expr_K:
            return unle_expr_K;
         case ungt_expr_K:
            return unlt_expr_K;
         case unle_expr_K:
            return unge_expr_K;
         case unlt_expr_K:
            return ungt_expr_K;

         case bit_and_expr_K:
         case bit_ior_expr_K:
         case bit_xor_expr_K:
         case eq_expr_K:
         case ne_expr_K:
         case uneq_expr_K:
            return op;
         
         case assert_expr_K:
         case catch_expr_K:
         case ceil_div_expr_K:
         case ceil_mod_expr_K:
         case complex_expr_K:
         case compound_expr_K:
         case eh_filter_expr_K:
         case exact_div_expr_K:
         case fdesc_expr_K:
         case floor_div_expr_K:
         case floor_mod_expr_K:
         case goto_subroutine_K:
         case in_expr_K:
         case init_expr_K:
         case lrotate_expr_K:
         case lshift_expr_K:
         case max_expr_K:
         case mem_ref_K:
         case min_expr_K:
         case minus_expr_K:
         case modify_expr_K:
         case mult_expr_K:
         case mult_highpart_expr_K:
         case ordered_expr_K:
         case plus_expr_K:
         case pointer_plus_expr_K:
         case postdecrement_expr_K:
         case postincrement_expr_K:
         case predecrement_expr_K:
         case preincrement_expr_K:
         case range_expr_K:
         case rdiv_expr_K:
         case round_div_expr_K:
         case round_mod_expr_K:
         case rrotate_expr_K:
         case rshift_expr_K:
         case set_le_expr_K:
         case trunc_div_expr_K:
         case trunc_mod_expr_K:
         case truth_and_expr_K:
         case truth_andif_expr_K:
         case truth_or_expr_K:
         case truth_orif_expr_K:
         case truth_xor_expr_K:
         case try_catch_expr_K:
         case try_finally_K:
         case ltgt_expr_K:
         case unordered_expr_K:
         case widen_sum_expr_K:
         case widen_mult_expr_K:
         case with_size_expr_K:
         case vec_lshift_expr_K:
         case vec_rshift_expr_K:
         case widen_mult_hi_expr_K:
         case widen_mult_lo_expr_K:
         case vec_pack_trunc_expr_K:
         case vec_pack_sat_expr_K:
         case vec_pack_fix_trunc_expr_K:
         case vec_extracteven_expr_K:
         case vec_extractodd_expr_K:
         case vec_interleavehigh_expr_K:
         case vec_interleavelow_expr_K:
         case extract_bit_expr_K:
         default:
            break;
      }

      THROW_UNREACHABLE("Unhandled predicate (" + boost::lexical_cast<std::string>(op) + ")");
   }

   bool isCompare(kind c_type)
   {
      return c_type == eq_expr_K || c_type == ne_expr_K || c_type == ltgt_expr_K || c_type == uneq_expr_K
         || c_type == gt_expr_K || c_type == lt_expr_K || c_type == ge_expr_K || c_type == le_expr_K 
         || c_type == unlt_expr_K || c_type == ungt_expr_K || c_type == unle_expr_K || c_type == unge_expr_K;
}

   bool isCompare(const struct binary_expr* condition)
   {
      return isCompare(condition->get_kind());
   }

   tree_nodeConstRef branchOpRecurse(const tree_nodeConstRef op)
   {
      if(const auto* nop = GetPointer<const nop_expr>(op))
      {
         return branchOpRecurse(nop->op);
      }
      else if(const auto* ssa = GetPointer<const ssa_name>(op))
      {
         const auto DefStmt = GET_CONST_NODE(ssa->CGetDefStmt());
         if(DefStmt->get_kind() == gimple_phi_K)
         {
            return DefStmt;
         }
         const auto* def = GetPointer<const gimple_assign>(DefStmt);
         THROW_ASSERT(def, "branch var definition should be a gimple_assign (" + DefStmt->get_kind_text() + " + " + DefStmt->ToString() + ")");
         return branchOpRecurse(def->op1);
      }
      else if(op->get_kind() == tree_reindex_K)
      {
         return branchOpRecurse(GET_CONST_NODE(op));
      }
      return op;
   }

   // Print name of variable according to its type
   void printVarName(const tree_nodeConstRef V, std::ostream& OS)
   {
      OS << GET_CONST_NODE(V)->ToString();

      // TODO: implement a better way to correctly print significant name

      //const Argument* A = nullptr;
      //const Instruction* I = nullptr;
      //
      //if((A = dyn_cast<Argument>(V)) != nullptr)
      //{
      //   const llvm::Function* currentFunction = A->getParent();
      //   llvm::ModuleSlotTracker MST(currentFunction->getParent());
      //   MST.incorporateFunction(*currentFunction);
      //   auto argID = MST.getLocalSlot(A);
      //   OS << A->getParent()->getName() << ".%" << argID;
      //}
      //else if((I = dyn_cast<Instruction>(V)) != nullptr)
      //{
      //   llvm::ModuleSlotTracker MST(I->getParent()->getParent()->getParent());
      //   MST.incorporateFunction(*I->getParent()->getParent());
      //   auto instID = MST.getLocalSlot(I);
      //   auto BBID = MST.getLocalSlot(I->getParent());
      //
      //   OS << I->getFunction()->getName() << ".BB" << BBID;
      //   if(instID < 0)
      //   {
      //      I->print(OS);
      //   }
      //   else
      //   {
      //      OS << ".%" << instID;
      //   }
      //}
      //else
      //{
      //   OS << V->getName();
      //}
   }

   bool isValidInstruction(const tree_nodeConstRef stmt, const FunctionBehaviorConstRef FB, const tree_managerConstRef TM)
   {
      tree_nodeConstRef Type = nullptr;
      switch(GET_CONST_NODE(stmt)->get_kind())
      {
         case gimple_assign_K:
         {
            auto* ga = GetPointer<const gimple_assign>(GET_CONST_NODE(stmt));
            if(tree_helper::IsLoad(TM, stmt, FB->get_function_mem()))
            {
               Type = tree_helper::CGetType(GET_CONST_NODE(ga->op0));
               break;
            }
            else if(tree_helper::IsStore(TM, stmt, FB->get_function_mem()))
            {
               Type = tree_helper::CGetType(GET_CONST_NODE(ga->op1));
               break;
            }
            Type = tree_helper::CGetType(GET_CONST_NODE(ga->op0));
            
            switch(GET_CONST_NODE(ga->op1)->get_kind())
            {
               /// unary_expr cases
               case view_convert_expr_K:
               {
                  if(Type->get_kind() == real_type_K)
                  {
                     return true;
                  }
               }
               break;

               case nop_expr_K:
               case abs_expr_K:

               /// binary_expr cases
               case plus_expr_K:
               case minus_expr_K:
               case mult_expr_K:
               case trunc_div_expr_K:
               case trunc_mod_expr_K:
               case lshift_expr_K:
               case rshift_expr_K:
               case bit_and_expr_K:
               case bit_ior_expr_K:
               case bit_xor_expr_K:
               case eq_expr_K:
               case ne_expr_K:
               case unge_expr_K:
               case ungt_expr_K:
               case unlt_expr_K:
               case unle_expr_K:
               case gt_expr_K:
               case ge_expr_K:
               case lt_expr_K:
               case le_expr_K:

               /// ternary_expr case
               case cond_expr_K:
                  break;

               default:
                  return false;
            }
         }
         break;

         case gimple_phi_K:
         {
            const auto* phi = GetPointer<const gimple_phi>(GET_CONST_NODE(stmt));
            Type = tree_helper::CGetType(GET_CONST_NODE(phi->res));
         }
         break;

         case gimple_asm_K:
         case gimple_bind_K:
         case gimple_call_K:
         case gimple_cond_K:
         case gimple_for_K:
         case gimple_goto_K:
         case gimple_label_K:
         case gimple_multi_way_if_K:
         case gimple_nop_K:
         case gimple_pragma_K:
         case gimple_predict_K:
         case gimple_resx_K:
         case gimple_return_K:
         case gimple_switch_K:
         case gimple_while_K:
         default:
            return false;
      }

      if(Type->get_kind() == integer_type_K || Type->get_kind() == enumeral_type_K || Type->get_kind() == boolean_type_K)
      {
         return true;
      }
      return false;
   }

   bool isIntegerType(const tree_nodeConstRef tn) 
   {
      if(tn->get_kind() == tree_reindex_K)
      {
         return isIntegerType(GET_CONST_NODE(tn));
      }
      const auto type = tree_helper::CGetType(tn);
      if(type->get_kind() == integer_type_K || type->get_kind() == enumeral_type_K)
      {
         return true;
      }
      return false;
   }

   bool isSignedType(const tree_nodeConstRef tn)
   {
      if(tn->get_kind() == tree_reindex_K)
      {
         return isSignedType(GET_CONST_NODE(tn));
      }
      const auto type = tree_helper::CGetType(tn);
      if(const auto* it = GetPointer<const integer_type>(type))
      {
         return !it->unsigned_flag;
      }
      if(const auto* en = GetPointer<const enumeral_type>(type))
      {
         return !en->unsigned_flag;
      }
      if(const auto* bt = GetPointer<const boolean_type>(type))
      {
         return false;
      }
      return true;
   }

   tree_nodeConstRef getGIMPLE_Type(const tree_nodeConstRef tn)
   {
      if(tn->get_kind() == tree_reindex_K)
      {
         return getGIMPLE_Type(GET_CONST_NODE(tn));
      }
      if(const auto* gp = GetPointer<const gimple_phi>(tn))
      {
         return getGIMPLE_Type(GET_CONST_NODE(gp->res));
      }
      if(const auto* gr = GetPointer<const gimple_return>(tn))
      {
         return getGIMPLE_Type(GET_CONST_NODE(gr->op));
      }
      return tree_helper::CGetType(tn);
   }

   unsigned int getGIMPLE_BW(const tree_nodeConstRef tn)
   {
      const auto type = getGIMPLE_Type(tn);
      unsigned int size = tree_helper::Size(type);
      THROW_ASSERT(static_cast<bool>(size), "Unhandled type (" + type->get_kind_text() + ") for " + tn->get_kind_text() + " " + tn->ToString());
      return size;
   }

   Range getGIMPLE_range(const tree_nodeConstRef tn)
   {
      if(tn->get_kind() == tree_reindex_K)
      {
         return getGIMPLE_range(GET_CONST_NODE(tn));
      }

      const auto type = getGIMPLE_Type(tn);
      unsigned int bw = tree_helper::Size(type);
      THROW_ASSERT(static_cast<bool>(bw), "Unhandled type (" + type->get_kind_text() + ") for " + tn->get_kind_text() + " " + tn->ToString());
      APInt min, max;
      if(const auto* ic = GetPointer<const integer_cst>(tn))
      {
         min = max = ic->value;
      }
      else if(const auto* it = GetPointer<const integer_type>(type))
      {
         min = it->unsigned_flag ? getMinValue(bw) : getSignedMinValue(bw);
         max = it->unsigned_flag ? getMaxValue(bw) : getSignedMaxValue(bw);
      }
      else if(const auto* et = GetPointer<const enumeral_type>(type))
      {
         min = et->unsigned_flag ? getMinValue(bw) : getSignedMinValue(bw);
         max = et->unsigned_flag ? getMaxValue(bw) : getSignedMaxValue(bw);
      }
      else if(type->get_kind() == boolean_type_K)
      {
         min = 0;
         max = 1;
         bw = 1;
      }
      else if(const auto* rc = GetPointer<const real_cst>(tn))
      {
         if(bw == 32)
         {
            IntFloat fcst;
            fcst.f = boost::lexical_cast<float>(rc->valr);
            min = max = truncExt(fcst.i, bw, true);
         }
         else if(bw == 64)
         {
            LongDouble dcst;
            dcst.d = boost::lexical_cast<double>(rc->valr);
            min = max = truncExt(dcst.l, bw, true);
         }
         else
         {
            THROW_UNREACHABLE("Floating point variable with unhandled bitwidth (" + boost::lexical_cast<std::string>(bw) + ")");
         }
      }
      else if(const auto* rt = GetPointer<const real_type>(type))
      {
         min = Min;
         max = Max;
      }
      else 
      {
         THROW_UNREACHABLE("Unable to define range for type " + type->get_kind_text() + " of " + tn->ToString());
      }

      if(const auto* ssa = GetPointer<const ssa_name>(tn))
      {
         if(!ssa->bit_values.empty())
         {
            for(size_t bi = 0; bi < bw; ++bi)
            {
               char bitv = ssa->bit_values.at(bi);
               if(bitv == '0')
               {
                  boost::multiprecision::bit_unset(min, bi);
                  boost::multiprecision::bit_unset(max, bi);
               }
               else if(bitv == '1')
               {
                  boost::multiprecision::bit_set(min, bi);
                  boost::multiprecision::bit_set(max, bi);
               }
            }
         }
      }
      return Range(Regular, bw, min, max);
   }
}

// ========================================================================== //
// Range
// ========================================================================== //
Range::Range(RangeType rType, unsigned rbw) : l(Min), u(Max), bw(rbw), type(rType)
{
   THROW_ASSERT(rbw > 0 && rbw <= MAX_BIT_INT, "Invalid bitwidth for range (bw = " + boost::lexical_cast<std::string>(rbw) +")");
}

Range::Range(RangeType rType, unsigned rbw, const APInt& lb, const APInt& ub) : l(lb), u(ub), bw(rbw), type(rType)
{
   THROW_ASSERT(rbw > 0 && rbw <= MAX_BIT_INT, "Invalid bitwidth for range (bw = " + boost::lexical_cast<std::string>(rbw) +")");
   normalizeRange(lb, ub, rType);
}

void Range::normalizeRange(const APInt& lb, const APInt& ub, RangeType rType)
{
   if(rType == Empty || rType == Unknown)
   {
      l = Min;
      u = Max;
   }
   else if(rType == Anti)
   {
      if(lb > ub)
      {
         type = Regular;
         l = Min;
         u = Max;
      }
      else
      {
         if((lb == Min) && (ub == Max))
         {
            type = Empty;
         }
         else if(lb == Min)
         {
            type = Regular;
            l = ub + Epsilon;
            u = Max;
         }
         else if(ub == Max)
         {
            type = Regular;
            l = Min;
            u = lb - Epsilon;
         }
         else
         {
            THROW_ASSERT(ub >= lb, "");
            auto maxS = getSignedMaxValue(bw);
            auto minS = getSignedMinValue(bw);
            bool lbgt = lb > maxS;
            bool ubgt = ub > maxS;
            bool lblt = lb < minS;
            bool ublt = ub < minS;
            if(lbgt && ubgt)
            {
               l = truncExt(lb, bw, true);
               u = truncExt(ub, bw, true);
            }
            else if(lblt && ublt)
            {
               l = truncExt(ub, bw, true);
               u = truncExt(lb, bw, true);
            }
            else if(!lblt && ubgt)
            {
               auto ubnew = truncExt(ub, bw, true);
               if(ubnew >= (lb - Epsilon))
               {
                  l = Min;
                  u = Max;
               }
               else
               {
                  type = Regular;
                  l = ubnew + Epsilon;
                  u = lb - Epsilon;
               }
            }
            else if(lblt && !ubgt)
            {
               auto lbnew = truncExt(lb, bw, true);
               if(lbnew <= (ub + Epsilon))
               {
                  l = Min;
                  u = Max;
               }
               else
               {
                  type = Regular;
                  l = ub + Epsilon;
                  u = lbnew - Epsilon;
               }
            }
            else if(!lblt && !ubgt)
            {
               l = lb;
               u = ub;
            }
            else
            {
               THROW_UNREACHABLE("unexpected condition");
            }
         }
      }
   }
   else if((lb - Epsilon) == ub)
   {
      type = Regular;
      l = Min;
      u = Max;
   }
   else if(lb > ub)
   {
      normalizeRange(ub + Epsilon, lb - Epsilon, Anti);
   }
   else
   {
      THROW_ASSERT(ub >= lb, "");
      auto maxS = getSignedMaxValue(bw);
      auto minS = getSignedMinValue(bw);

      bool lbgt = lb > maxS;
      bool ubgt = ub > maxS;
      bool lblt = lb < minS;
      bool ublt = ub < minS;
      if(ubgt && lblt)
      {
         l = Min;
         u = Max;
      }
      else if(lbgt && ubgt)
      {
         l = truncExt(lb, bw, true);
         u = truncExt(ub, bw, true);
      }
      else if(lblt && ublt)
      {
         l = truncExt(ub, bw, true);
         u = truncExt(lb, bw, true);
      }
      else if(!lblt && ubgt)
      {
         auto ubnew = truncExt(ub, bw, true);
         if(ubnew >= (lb - 1))
         {
            l = Min;
            u = Max;
         }
         else
         {
            type = Anti;
            l = ubnew + 1;
            u = lb - 1;
         }
      }
      else if(lblt && !ubgt)
      {
         auto lbnew = truncExt(lb, bw, true);
         if(lbnew <= (ub + 1))
         {
            l = Min;
            u = Max;
         }
         else
         {
            type = Anti;
            l = ub + 1;
            u = lbnew - 1;
         }
      }
      else if(!lblt && !ubgt)
      {
         l = lb;
         u = ub;
      }
      else
      {
         THROW_UNREACHABLE("unexpected condition");
      }
   }
   if(!(u >= l))
   {
      l = Min;
      u = Max;
   }
}

unsigned Range::neededBits(const APInt& a, const APInt& b, bool sign)
{
   auto unsigned_needed_bits = [](const APInt& x)
   {
      if(x < 0)
      {
         return MAX_BIT_INT;
      }
      auto i = 127;
      while(i > 0)
      {
         if(bit_test(x, i))
         {
            break;
         }
         --i;
      }
      return (unsigned)++i;
   };

   auto signed_needed_bits = [](const APInt& x)
   {
      auto i = 127;
      while(i > 0)
      {
         if(!bit_test(x, i))
         {
            break;
         }
         --i;
      }
      return (unsigned)(i + 2);
   };

   if(sign)
   {
      auto bit_a = a < 0 ? signed_needed_bits(a) : signed_needed_bits(-a);
      auto bit_b = b < 0 ? signed_needed_bits(b) : signed_needed_bits(-b);
      return std::max(bit_a, bit_b);
   }
   return std::max(unsigned_needed_bits(a), unsigned_needed_bits(b));
}

Range Range::getAnti() const
{
   if(type == Anti)
   {
      return Range(Regular, bw, l, u);
   }
   if(type == Regular)
   {
      return Range(Anti, bw, l, u);
   }
   if(type == Empty)
   {
      return Range(Regular, bw);
   }
   if(type == Unknown)
   {
      return *this;
   }

   THROW_UNREACHABLE("unexpected condition");
}

unsigned Range::getBitWidth() const
{
   return bw;
}

const APInt &Range::getLower() const
{
   THROW_ASSERT(type != Anti, "Lower bound not valid for Anti range");
   return l;
}

const APInt &Range::getUpper() const
{
   THROW_ASSERT(type != Anti, "Upper bound not valid for Anti range");
   return u;
}

APInt Range::getSignedMax() const
{
   THROW_ASSERT(type != Unknown && type != Empty, "Max not valid for Unknown/Empty range");
   if(type == Regular)
   {
      auto maxS = getSignedMaxValue(bw);
      return u > maxS ? maxS : u;
   }
   return getSignedMaxValue(bw);
}

APInt Range::getSignedMin() const
{
   THROW_ASSERT(type != Unknown && type != Empty, "Min not valid for Unknown/Empty range");
   if(type == Regular)
   {
      auto minS = getSignedMinValue(bw);
      if(l < minS)
      {
         //    THROW_UNREACHABLE("This case should never happen");
         return minS;
      }
      return l;
   }
   return getSignedMinValue(bw);
}

APInt Range::getUnsignedMax() const
{
   THROW_ASSERT(type != Unknown && type != Empty, "UMax not valid for Unknown/Empty range");
   if(isAnti())
   {
      auto lb = l - Epsilon;
      auto ub = u + Epsilon;
      return (lb >= 0 || ub < 0) ? getMaxValue(bw) : truncExt(lb, bw, false);
   }
   return (u < 0 || l >= 0) ? truncExt(u, bw, false) : getMaxValue(bw);
}

APInt Range::getUnsignedMin() const
{
   THROW_ASSERT(type != Unknown && type != Empty, "UMin not valid for Unknown/Empty range");
   if(isAnti())
   {
      return (l > 0 || u < 0) ? getMinValue(bw) : (u + Epsilon);
   }
   return (l > 0 || u < 0) ? truncExt(l, bw, false) : getMinValue(bw);
}

bool Range::isUnknown() const
{
   return type == Unknown;
}

void Range::setUnknown()
{
   type = Unknown;
}

bool Range::isRegular() const
{
   return type == Regular;
}

bool Range::isAnti() const
{
   return type == Anti;
}

bool Range::isEmpty() const
{
   return type == Empty;
}

bool Range::isSameType(const Range& a, const Range& b) const
{
   return a.type == b.type;
}

bool Range::isSameRange(const Range& a, const Range& b) const
{
   return (a.l == b.l) && (a.u == b.u);
}

bool Range::isSingleElement()
{
   return type == Regular && l == u;
}

bool Range::isFullSet() const
{
   THROW_ASSERT(!isUnknown(), "");
   if(isEmpty() || isAnti())
   {
      return false;
   }
   return (getSignedMaxValue(bw) <= getUpper() && getSignedMinValue(bw) >= getLower())
      || (getMaxValue(bw) <= getUpper() && getMinValue(bw) >= getLower());
}

bool Range::isMaxRange() const
{
   return isFullSet();
   // if(isAnti())
   // {
   //    return false;
   // }
   // return (this->getLower() == Min) && (this->getUpper() == Max);
}

bool Range::isConstant() const
{
   if(isAnti())
   {
      return false;
   }
   return this->getLower() == this->getUpper();
}

/// Add and Mul are commutative. So, they are a little different
/// than the other operations.
/// Many Range reductions are done by exploiting ConstantRange code
Range Range::add(const Range& other) const
{
   if(isEmpty() || isUnknown() || isMaxRange())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown() || other.isMaxRange())
   {
      return other;
   }
   if(isAnti() && other.isConstant())
   {
      auto ol = other.getLower();
      if(ol >= (Max - u))
      {
         return Range(Regular, bw);
      }
      return Range(Anti, bw, l + ol, u + ol);
   }
   if(this->isAnti() || other.isAnti())
   {
      return Range(Regular, bw);
   }
   if(other.isConstant())
   {
      auto ol = other.getLower();
      if(l == Min)
      {
         THROW_ASSERT(u != Max, "");
         return Range(Regular, bw, l, u + ol);
      }
      if(u == Max)
      {
         THROW_ASSERT(l != Min, "");
         return Range(Regular, bw, l + ol, u);
      }

      return Range(Regular, bw, l + ol, u + ol);
   }

   auto AddU = Range(Regular, bw, std::max(APInt(0), getUnsignedMin() + other.getUnsignedMin()), std::min(getUnsignedMax() + other.getUnsignedMax(), getMaxValue(bw)));
   auto AddS = Range(Regular, bw, std::max(Min, getSignedMin() + other.getSignedMin()), std::min(getSignedMax() + other.getSignedMax(), Max));
   return BestRange(AddU, AddS, bw);
}

Range Range::sub(const Range& other) const
{
   if(isEmpty() || isUnknown() || isMaxRange())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown() || other.isMaxRange())
   {
      return other;
   }
   if(this->isAnti() && other.isConstant())
   {
      auto ol = other.getLower();
      if(l <= (Min + ol))
      {
         return Range(Regular, bw);
      }

      return Range(Anti, bw, l - ol, u - ol);
   }
   if(this->isAnti() || other.isAnti())
   {
      return Range(Regular, bw);
   }
   if(other.isConstant())
   {
      auto ol = other.getLower();
      if(l == Min)
      {
         THROW_ASSERT(u != Max, "");
         auto minValue = getSignedMinValue(bw);
         auto upper = u - ol;
         if(minValue > upper)
         {
            return Range(Regular, bw);
         }

         return Range(Regular, bw, l, upper);
      }
      if(u == Max)
      {
         THROW_ASSERT(l != Min, "");
         auto maxValue = getSignedMaxValue(bw);
         auto lower = l - ol;
         if(maxValue < lower)
         {
            return Range(Regular, bw);
         }

         return Range(Regular, bw, l - ol, u);
      }

      auto lower = truncExt(l - ol, bw, true);
      auto upper = truncExt(u - ol, bw, true);
      if(lower <= upper)
      {
         return Range(Regular, bw, lower, upper);
      }
      return Range(Anti, bw, upper + 1, lower - 1);
   }

   auto SubU = Range(Regular, bw, std::max(APInt(0), getUnsignedMin() - other.getUnsignedMax()), std::min(getUnsignedMax() - getUnsignedMin(), getMaxValue(bw)));
   auto SubS = Range(Regular, bw, std::max(Min, getSignedMin() - getSignedMax()), std::min(getSignedMax() - getSignedMin(), Max));

   return BestRange(SubU, SubS, bw);
}

Range Range::mul(const Range& other) const
{
   if(isEmpty() || isUnknown() || isMaxRange())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown() || other.isMaxRange())
   {
      return other;
   }
   if(this->isAnti() || other.isAnti())
   {
      return Range(Regular, bw);
   }

   // Multiplication is signedness-independent. However different ranges can be
   // obtained depending on how the input ranges are treated. These different
   // ranges are all conservatively correct, but one might be better than the
   // other. We calculate two ranges; one treating the inputs as unsigned
   // and the other signed, then return the smallest of these ranges.

   // Unsigned range first.
   APInt this_min = getUnsignedMin();
   APInt this_max = getUnsignedMax();
   APInt Other_min = other.getUnsignedMin();
   APInt Other_max = other.getUnsignedMax();

   Range Result_zext = Range(Regular, bw * 2, this_min * Other_min, this_max * Other_max);
   Range UR = Result_zext.truncate(bw);

   // Now the signed range. Because we could be dealing with negative numbers
   // here, the lower bound is the smallest of the Cartesian product of the
   // lower and upper ranges; for example:
   //   [-1,4] * [-2,3] = min(-1*-2, -1*-3, 4*-2, 4*3) = -8.
   // Similarly for the upper bound, swapping min for max.

   this_min = getSignedMin();
   this_max = getSignedMax();
   Other_min = other.getSignedMin();
   Other_max = other.getSignedMax();

   auto [min, max] = std::minmax({this_min * Other_min, this_min * Other_max, this_max * Other_min, this_max * Other_max});
   Range Result_sext(Regular, bw * 2, min, max);
   Range SR = Result_sext.truncate(bw);
   return BestRange(UR, SR, bw);
}

#define DIV_HELPER(x, y) (x == Max) ? ((y < 0) ? Min : ((y == 0) ? 0 : Max)) : ((y == Max) ? 0 : ((x == Min) ? ((y < 0) ? Max : ((y == 0) ? 0 : Min)) : ((y == Min) ? 0 : ((x) / (y)))))

Range Range::udiv(const Range& other) const
{
   if(isEmpty() || isUnknown() || isMaxRange())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("this: " << *this << std::endl << "other: " << other);
   #endif
   UAPInt a(getUnsignedMin());
   UAPInt b(getUnsignedMax());
   UAPInt c(other.getUnsignedMin());
   UAPInt d(other.getUnsignedMax());

   // Deal with division by 0 exception
   if((c == 0) && (d == 0))
   {
      return Range(Regular, bw);
   }
   if(c == 0)
   {
      c = 1;
   }

   auto [min, max] = std::minmax({DIV_HELPER(a, c), DIV_HELPER(a, d), DIV_HELPER(b, c), DIV_HELPER(b, d)});
   // Lower bound is the min value from the vector, while upper bound is the max value
   return Range(Regular, bw, min, max);
}

Range Range::sdiv(const Range& other) const
{
   if(isEmpty() || isUnknown() || isMaxRange())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(this->isAnti())
   {
      return Range(Regular, bw);
   }

   const APInt& a = this->getLower();
   const APInt& b = this->getUpper();
   APInt c1, d1, c2, d2;
   bool is_zero_in = false;
   if(other.isAnti())
   {
      auto antiRange = other.getAnti();
      c1 = Min;
      d1 = antiRange.getLower() - 1;
      if(d1 == 0)
      {
         d1 = -1;
      }
      else if(d1 > 0)
      {
         return Range(Regular, bw); /// could be improved
      }
      c2 = antiRange.getUpper() + 1;
      if(c2 == 0)
      {
         c2 = 1;
      }
      else if(c2 < 0)
      {
         return Range(Regular, bw); /// could be improved
      }
      d2 = Max;
   }
   else
   {
      c1 = other.getLower();
      d1 = other.getUpper();
      // Deal with division by 0 exception
      if((c1 == 0) && (d1 == 0))
      {
         return Range(Regular, bw);
      }
      is_zero_in = (c1 < 0) && (d1 > 0);
      if(is_zero_in)
      {
         d1 = -1;
         c2 = 1;
      }
      else
      {
         c2 = other.getLower();
         if(c2 == 0)
         {
            c1 = c2 = 1;
         }
      }
      d2 = other.getUpper();
      if(d2 == 0)
      {
         d1 = d2 = -1;
      }
   }
   auto n_iters = (is_zero_in || other.isAnti()) ? 8u : 4u;

   APInt candidates[8];
   candidates[0] = DIV_HELPER(a, c1);
   candidates[1] = DIV_HELPER(a, d1);
   candidates[2] = DIV_HELPER(b, c1);
   candidates[3] = DIV_HELPER(b, d1);
   if(n_iters == 8)
   {
      candidates[4] = DIV_HELPER(a, c2);
      candidates[5] = DIV_HELPER(a, d2);
      candidates[6] = DIV_HELPER(b, c2);
      candidates[7] = DIV_HELPER(b, d2);
   }
   // Lower bound is the min value from the vector, while upper bound is the max value
   auto [min, max] = std::minmax_element(candidates, candidates + n_iters);
   return Range(Regular, bw, *min, *max);
}

Range Range::urem(const Range& other) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(this->isAnti() || other.isAnti())
   {
      return Range(Regular, bw);
   }

   const APInt& a = this->getUnsignedMin();
   const APInt& b = this->getUnsignedMax();
   APInt c = other.getUnsignedMin();
   const APInt& d = other.getUnsignedMax();

   // Deal with mod 0 exception
   if((c == 0) && (d == 0))
   {
      return Range(Regular, bw);
   }
   if(c == 0)
   {
      c = 1;
   }
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("this-urem: " << *this << std::endl << "other-urem: " << other);
   #endif

   APInt candidates[8];

   candidates[0] = a < c ? a : 0;
   candidates[1] = a < d ? a : 0;
   candidates[2] = b < c ? b : 0;
   candidates[3] = b < d ? b : 0;
   candidates[4] = a < c ? a : c - 1;
   candidates[5] = a < d ? a : d - 1;
   candidates[6] = b < c ? b : c - 1;
   candidates[7] = b < d ? b : d - 1;

   // Lower bound is the min value from the vector, while upper bound is the max value
   auto [min, max] = std::minmax_element(candidates, candidates + 8);
   auto res = Range(Regular, bw, *min, *max);
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("res-urem: " << res << std::endl);
   #endif
   return res;
}

Range Range::srem(const Range& other) const
{
   if(isEmpty() || isUnknown() || isMaxRange())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(this->isAnti() || other.isAnti())
   {
      return Range(Regular, bw);
   }
   if(other == Range(Regular, other.bw, 0, 0))
   {
      return Range(Empty, bw);
   }

   const APInt& a = this->getLower();
   const APInt& b = this->getUpper();
   APInt c = other.getLower();
   const APInt& d = other.getUpper();

   // Deal with mod 0 exception
   if((c == 0) && (d == 0))
   {
      return Range(Regular, bw);
   }
   if(c == 0)
   {
      c = 1;
   }
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("this-rem: " << *this << std::endl << "other-rem: " << other);
   #endif

   APInt candidates[4];
   candidates[0] = Min;
   candidates[1] = Min;
   candidates[2] = Max;
   candidates[3] = Max;
   if((a != Min) && (c != Min))
   {
      candidates[0] = a & c; // lower lower
   }
   if((a != Min) && (d != Max))
   {
      candidates[1] = a % d; // lower upper
   }
   if((b != Max) && (c != Min))
   {
      candidates[2] = b % c; // upper lower
   }
   if((b != Max) && (d != Max))
   {
      candidates[3] = b % d; // upper upper
   }
   // Lower bound is the min value from the vector, while upper bound is the max value
   auto [min, max] = std::minmax_element(candidates, candidates + 4);
   auto res = Range(Regular, bw, *min, *max);
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("res-rem: " << res << std::endl);
   #endif

   return res;
}

// Logic has been borrowed from ConstantRange
Range Range::shl(const Range& other) const
{
   if(isEmpty() || isUnknown() || isMaxRange())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown() || other.isMaxRange())
   {
      return other;
   }
   if(this->isAnti() || other.isAnti())
   {
      return Range(Regular, bw);
   }
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("Shl-a: " << *this << std::endl << "Shl-b: " << other);
   #endif

   const APInt& a = this->getLower();
   const APInt& b = this->getUpper();
   const auto c = other.getUnsignedMin().convert_to<unsigned>();
   const auto d = other.getUnsignedMax().convert_to<unsigned>();

   if(c >= bw)
   {
      return Range(Regular, bw);
   }
   if(d >= bw)
   {
      return Range(Regular, bw);
   }
   if((a == Min) || (b == Max))
   {
      return Range(Regular, bw);
   }
   if((a == b) && (c == d)) // constant case
   {
      auto minmax = truncExt(a << c, bw, true);
      return Range(Regular, bw, minmax, minmax);
   }
   if(a < 0 && b < 0)
   {
      UAPInt clOnes(countLeadingOnes(a, bw) - (MAX_BIT_INT - bw));
      if(d > clOnes)
      { // overflow
         return Range(Regular, bw);
      }
      return Range(Regular, bw, truncExt(a << d, bw, true), truncExt(b << c, bw, true));
   }
   if(a < 0 && b >= 0)
   {
      UAPInt clOnes(countLeadingOnes(a, bw) - (MAX_BIT_INT - bw));
      UAPInt clZeros(countLeadingZeros(b, bw) - (MAX_BIT_INT - bw));
      if(d > clOnes || d > clZeros)
      { // overflow
         return Range(Regular, bw);
      }
      return Range(Regular, bw, truncExt(a << d, bw, true), truncExt(b << d, bw, true));
   }

   UAPInt clZeros(countLeadingZeros(b, bw) - (MAX_BIT_INT - bw));
   if(d > clZeros)
   { // overflow
      return Range(Regular, bw);
   }

   return Range(Regular, bw, truncExt(a << c, bw, true), truncExt(b << d, bw, true));
}


Range Range::shr(const Range& other, bool sign) const
{
   if(sign)
   {
      #ifdef DEBUG_RANGE_OP
      PRINT_MSG("Ashr-a: " << *this << std::endl << "Ashr-b: " << other);
      #endif
      if(isEmpty() || isUnknown())
      {
         return *this;
      }
      if(other.isEmpty() || other.isUnknown())
      {
         return other;
      }

      auto this_min = getSignedMin();
      auto this_max = getSignedMax();
      auto other_min = other.getUnsignedMin().convert_to<unsigned>();
      auto other_max = other.getUnsignedMax().convert_to<unsigned>();
      auto [min, max] = std::minmax({this_max >> other_min, this_max >> other_max, this_min >> other_min, this_min >> other_max});

      Range AshrU = Range(Regular, bw, std::move(min), std::move(max));
      if(AshrU.isFullSet())
      {
         AshrU = Range(Regular, bw);
      }
      #ifdef DEBUG_RANGE_OP
      PRINT_MSG("Ashr-res: " << AshrU << std::endl);
      #endif
      return AshrU;
   }
   else
   {
      #ifdef DEBUG_RANGE_OP
      PRINT_MSG("Lshr-a: " << *this << std::endl << "Lshr-b: " << other);
      #endif
      if(isEmpty() || isUnknown())
      {
         return *this;
      }
      if(other.isEmpty() || other.isUnknown())
      {
         return other;
      }
      UAPInt this_min(getUnsignedMin());
      UAPInt this_max(getUnsignedMax());
      auto other_min = other.getUnsignedMin().convert_to<unsigned>();
      auto other_max = other.getUnsignedMax().convert_to<unsigned>();

      auto lshrU = Range(Regular, bw, this_min >> other_max, this_max >> other_min);
      if(lshrU.isFullSet())
      {
         lshrU = Range(Regular, bw);
      }
      #ifdef DEBUG_RANGE_OP
      PRINT_MSG("Lshr-res: " << lshrU << std::endl);
      #endif
      return lshrU;
   }
}

/*
 * 	This and operation is coded following Hacker's Delight algorithm.
 * 	According to the author, it provides tight results.
 */
Range Range::And(const Range& other) const
{
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("And-a: " << *this << std::endl << "And-b: " << other);
   #endif
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   APInt a = this->isAnti() ? Min : this->getLower();
   APInt b = this->isAnti() ? Max : this->getUpper();
   APInt c = other.isAnti() ? Min : other.getLower();
   APInt d = other.isAnti() ? Max : other.getUpper();

   // negate everybody
   APInt negA = ~a;
   APInt negB = ~b;
   APInt negC = ~c;
   APInt negD = ~d;

   Range inv1 = Range(Regular, bw, negB, negA);
   Range inv2 = Range(Regular, bw, negD, negC);
   Range invres = inv1.Or(inv2);

   // negate the result of the 'or'
   auto [min, max] = std::minmax(truncExt(~invres.l, bw, false), truncExt(~invres.u, bw, false));
   PRINT_MSG("min: " + boost::lexical_cast<std::string>(min) + " max: " + boost::lexical_cast<std::string>(max));
   auto res = Range(invres.type, bw, min, max);
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("And-res: " << res << std::endl);
   #endif
   return res;
}

namespace
{
   APInt minOR(APInt a, const APInt& b, APInt c, const APInt& d)
   {
      UAPInt ub(b), ud(d), temp;
      APInt m;
      m = 1 << (MAX_BIT_INT - 1);
      while(m != 0)
      {
         if((~a & c & m) != 0)
         {
            temp = UAPInt((a | m) & -m);
            if(temp <= ub)
            {
               a = temp;
               break;
            }
         }
         else if((a & ~c & m) != 0)
         {
            temp = UAPInt((c | m) & -m);
            if(temp <= ud)
            {
               c = temp;
               break;
            }
         }
         m = UAPInt(m) >> 1;
      }
      return a | c;
   }

   APInt maxOR(const APInt& a, APInt b, const APInt& c, APInt d)
   {
      UAPInt ua(a), uc(c), temp;
      APInt m;
      m = 1 << (MAX_BIT_INT - 1);
      while(m != 0)
      {
         if((b & d & m) != 0)
         {
            temp = UAPInt((b - m) | (m - 1));
            if(temp >= ua)
            {
               b = temp;
               break;
            }
            temp = UAPInt((d - m) | (m - 1));
            if(temp >= uc)
            {
               d = temp;
               break;
            }
         }
         m = UAPInt(m) >> 1;
      }
      return b | d;
   }

   APInt minXOR(APInt a, const APInt& b, APInt c, const APInt& d)
   {
      UAPInt ub(b), ud(d), temp;
      APInt m;
      m = 1 << (MAX_BIT_INT - 1);
      while(m != 0)
      {
         if((~a & c & m) != 0)
         {
            temp = UAPInt((a | m) & -m);
            if(temp <= ub)
            {
               a = temp;
            }
         }
         else if((a & ~c & m) != 0)
         {
            temp = UAPInt((c | m) & -m);
            if(temp <= ud)
            {
               c = temp;
            }
         }
         m = UAPInt(m) >> 1;
      }
      return a ^ c;
   }

   APInt maxXOR(const APInt& a, APInt b, const APInt& c, APInt d)
   {
      UAPInt ua(a), uc(c), temp;
      APInt m;
      m = 1 << (MAX_BIT_INT - 1);
      while(m != 0)
      {
         if((b & d & m) != 0)
         {
            temp = UAPInt((b - m) | (m - 1));
            if(temp >= ua)
            {
               b = temp;
            }
            else
            {
               temp = UAPInt((d - m) | (m - 1));
               if(temp >= uc)
               {
                  d = temp;
               }
            }
         }
         m = UAPInt(m) >> 1;
      }
      return b ^ d;
   }

} // namespace

/**
 * Or operation coded following Hacker's Delight algorithm.
 */
Range Range::Or(const Range& other) const
{
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("Or-a: " << *this << std::endl << "Or-b: " << other);
   #endif
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(this->isAnti() || other.isAnti())
   {
      return Range(Regular, bw);
   }

   const APInt a = this->getLower();
   const APInt b = this->getUpper();
   const APInt c = other.getLower();
   const APInt d = other.getUpper();

   //      if (a == Min || b == Max || c == Min || d == Max)
   //         return Range(Regular,bw);

   unsigned char switchval = 0;
   switchval += (a >= 0 ? 1 : 0);
   switchval <<= 1;
   switchval += (b >= 0 ? 1 : 0);
   switchval <<= 1;
   switchval += (c >= 0 ? 1 : 0);
   switchval <<= 1;
   switchval += (d >= 0 ? 1 : 0);

   APInt res_l = Min, res_u = Max;

   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("switchval " << (unsigned)switchval << std::endl);
   #endif

   switch(switchval)
   {
      case 0:
         res_l = minOR(a, b, c, d);
         res_u = maxOR(a, b, c, d);
         break;
      case 1:
         res_l = a;
         res_u = -1;
         break;
      case 3:
         res_l = minOR(a, b, c, d);
         res_u = maxOR(a, b, c, d);
         break;
      case 4:
         res_l = c;
         res_u = -1;
         break;
      case 5:
         res_l = (a < c ? a : c);
         res_u = maxOR(0, b, 0, d);
         break;
      case 7:
         res_l = minOR(a, -1, c, d);
         res_u = maxOR(0, b, c, d);
         break;
      case 12:
         res_l = minOR(a, b, c, d);
         res_u = maxOR(a, b, c, d);
         break;
      case 13:
         res_l = minOR(a, b, c, -1);
         res_u = maxOR(a, b, 0, d);
         break;
      case 15:
         res_l = minOR(a, b, c, d);
         res_u = maxOR(a, b, c, d);
         break;
   }
   res_l = truncExt(res_l, bw, false);
   res_u = truncExt(res_u, bw, false);
   if((res_l == Min) || (res_u == Max))
   {
      return Range(Regular, bw);
   }
   auto res = Range(Regular, bw, res_l, res_u);
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("Or-res: " << res << std::endl);
   #endif
   return res;
}

Range Range::Xor(const Range& other) const
{
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("Xor-a: " << *this << std::endl << "Xor-b: " << other);
   #endif
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(this->isAnti() || other.isAnti())
   {
      return Range(Regular, bw);
   }
   const APInt a = this->getLower();
   const APInt b = this->getUpper();
   const APInt c = other.getLower();
   const APInt d = other.getUpper();
   if((a >= 0) && (b >= 0) && (c >= 0) && (d >= 0))
   {
      APInt res_l = minXOR(a, b, c, d);
      APInt res_u = maxXOR(a, b, c, d);
      auto res = Range(Regular, bw, truncExt(res_l, bw, false), truncExt(res_u, bw, false));
      #ifdef DEBUG_RANGE_OP
      PRINT_MSG("Xor-res: " << res << std::endl);
      #endif
      return res;
   }
   else if((c == -1) && (d == -1) && (a >= 0) && (b >= 0))
   {
      auto res = other.sub(*this);
      #ifdef DEBUG_RANGE_OP
      PRINT_MSG("Xor-res: " << res << std::endl);
      #endif
      return res;
   }
   return Range(Regular, bw);
}

Range Range::Eq(const Range& other, unsigned bw) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(!isAnti() && !other.isAnti())
   {
      if((l == Min) || (u == Max) || (other.l == Min) || (other.u == Max))
      {
         return Range(Regular, bw, 0, 1);
      }
   }
   bool areTheyEqual = !this->intersectWith(other).isEmpty();
   bool areTheyDifferent = !((l == u) && *this == other);

   if(areTheyEqual && areTheyDifferent)
   {
      return Range(Regular, bw, 0, 1);
   }
   if(areTheyEqual && !areTheyDifferent)
   {
      return Range(Regular, bw, 1, 1);
   }
   if(!areTheyEqual && areTheyDifferent)
   {
      return Range(Regular, bw, 0, 0);
   }

   THROW_UNREACHABLE("condition unexpected");
}

Range Range::Ne(const Range& other, unsigned bw) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(!isAnti() && !other.isAnti())
   {
      if((l == Min) || (u == Max) || (other.l == Min) || (other.u == Max))
      {
         return Range(Regular, bw, 0, 1);
      }
   }
   bool areTheyEqual = !this->intersectWith(other).isEmpty();
   bool areTheyDifferent = !((l == u) && *this == other);
   if(areTheyEqual && areTheyDifferent)
   {
      return Range(Regular, bw, 0, 1);
   }
   if(areTheyEqual && !areTheyDifferent)
   {
      return Range(Regular, bw, 0, 0);
   }
   if(!areTheyEqual && areTheyDifferent)
   {
      return Range(Regular, bw, 1, 1);
   }

   THROW_UNREACHABLE("condition unexpected");
}

Range Range::Ugt(const Range& other, unsigned bw) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(isAnti() || other.isAnti())
   {
      return Range(Regular, bw, 0, 1);
   }

   UAPInt a(this->getUnsignedMin());
   UAPInt b(this->getUnsignedMax());
   UAPInt c(other.getUnsignedMin());
   UAPInt d(other.getUnsignedMax());
   if(a > d)
   {
      return Range(Regular, bw, 1, 1);
   }
   if(c >= b)
   {
      return Range(Regular, bw, 0, 0);
   }

   return Range(Regular, bw, 0, 1);
}

Range Range::Uge(const Range& other, unsigned bw) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(isAnti() || other.isAnti())
   {
      return Range(Regular, bw, 0, 1);
   }

   UAPInt a(this->getUnsignedMin());
   UAPInt b(this->getUnsignedMax());
   UAPInt c(other.getUnsignedMin());
   UAPInt d(other.getUnsignedMax());
   if(a >= d)
   {
      return Range(Regular, bw, 1, 1);
   }
   if(c > b)
   {
      return Range(Regular, bw, 0, 0);
   }

   return Range(Regular, bw, 0, 1);
}

Range Range::Ult(const Range& other, unsigned bw) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(isAnti() || other.isAnti())
   {
      return Range(Regular, bw, 0, 1);
   }

   UAPInt a(this->getUnsignedMin());
   UAPInt b(this->getUnsignedMax());
   UAPInt c(other.getUnsignedMin());
   UAPInt d(other.getUnsignedMax());
   if(b < c)
   {
      return Range(Regular, bw, 1, 1);
   }
   if(d <= a)
   {
      return Range(Regular, bw, 0, 0);
   }

   return Range(Regular, bw, 0, 1);
}

Range Range::Ule(const Range& other, unsigned bw) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(isAnti() || other.isAnti())
   {
      return Range(Regular, bw, 0, 1);
   }

   UAPInt a(this->getUnsignedMin());
   UAPInt b(this->getUnsignedMax());
   UAPInt c(other.getUnsignedMin());
   UAPInt d(other.getUnsignedMax());
   if(b <= c)
   {
      return Range(Regular, bw, 1, 1);
   }
   if(d < a)
   {
      return Range(Regular, bw, 0, 0);
   }

   return Range(Regular, bw, 0, 1);
}

Range Range::Sgt(const Range& other, unsigned bw) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(isAnti() || other.isAnti())
   {
      return Range(Regular, bw, 0, 1);
   }

   APInt a = this->getSignedMin();
   APInt b = this->getSignedMax();
   APInt c = other.getSignedMin();
   APInt d = other.getSignedMax();
   if(a > d)
   {
      return Range(Regular, bw, 1, 1);
   }
   if(c >= b)
   {
      return Range(Regular, bw, 0, 0);
   }

   return Range(Regular, bw, 0, 1);
}

Range Range::Sge(const Range& other, unsigned bw) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(isAnti() || other.isAnti())
   {
      return Range(Regular, bw, 0, 1);
   }

   APInt a = this->getSignedMin();
   APInt b = this->getSignedMax();
   APInt c = other.getSignedMin();
   APInt d = other.getSignedMax();
   if(a >= d)
   {
      return Range(Regular, bw, 1, 1);
   }
   if(c > b)
   {
      return Range(Regular, bw, 0, 0);
   }

   return Range(Regular, bw, 0, 1);
}

Range Range::Slt(const Range& other, unsigned bw) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(isAnti() || other.isAnti())
   {
      return Range(Regular, bw, 0, 1);
   }

   APInt a = this->getSignedMin();
   APInt b = this->getSignedMax();
   APInt c = other.getSignedMin();
   APInt d = other.getSignedMax();
   if(b < c)
   {
      return Range(Regular, bw, 1, 1);
   }
   if(d <= a)
   {
      return Range(Regular, bw, 0, 0);
   }

   return Range(Regular, bw, 0, 1);
}

Range Range::Sle(const Range& other, unsigned bw) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   if(isAnti() || other.isAnti())
   {
      return Range(Regular, bw, 0, 1);
   }

   APInt a = this->getSignedMin();
   APInt b = this->getSignedMax();
   APInt c = other.getSignedMin();
   APInt d = other.getSignedMax();
   if(b <= c)
   {
      return Range(Regular, bw, 1, 1);
   }
   if(d < a)
   {
      return Range(Regular, bw, 0, 0);
   }

   return Range(Regular, bw, 0, 1);
}

Range Range::abs() const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(isAnti())
   {
      if(u < 0)
      {
         return Range(Anti, bw, -u, -l);
      }
      if(l < 0)
      {
         if(-l < u)
         {
            return Range(Anti, bw, -l, u);
         }
         else
         {
            return Range(Regular, bw);
         }
      }
      return *this;
   }

   auto smin = getSignedMin();
   auto smax = getSignedMax();

   if(smax < 0)
   {
      return Range(Regular, bw, -smax, -smin);
   }
   if(smin < 0)
   {
      auto [min, max] = std::minmax({smax, -smin});
      return Range(Regular, bw, min, max);
   }
   return *this;
}

// Truncate
// - if the source range is entirely inside max bit range, it is the result
// - else, the result is the max bit range
Range Range::truncate(unsigned bitwidth) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }

   APInt maxupper = getSignedMaxValue(bitwidth);
   APInt maxlower = getSignedMinValue(bitwidth);

   // Check if source range is contained by max bit range
   if(!this->isAnti() && l >= maxlower && u <= maxupper)
   {
      return Range(Regular, bitwidth, l, u);
   }

   return Range(Regular, bitwidth, maxlower, maxupper);
}

Range Range::sextOrTrunc(unsigned bitwidth) const
{
   auto from_bw = bw;
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("sextOrTrunc: " << *this << " to " << bitwidth);
   #endif
   auto this_min = from_bw == 1 ? getUnsignedMin() : getSignedMin();
   auto this_max = from_bw == 1 ? getUnsignedMax() : getSignedMax();
   auto [min, max] = std::minmax(truncExt(this_min, bitwidth, true), truncExt(this_max, bitwidth, true));
   auto sextRes = Range(Regular, bitwidth, min, max);
   if(sextRes.isFullSet())
   {
      sextRes = Range(Regular, bitwidth);
   }
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("sextOrTrunc-res: " << sextRes);
   #endif
   return sextRes;
}

Range Range::zextOrTrunc(unsigned bitwidth) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("zextOrTrunc: " << *this << " to " << bitwidth);
   #endif
   auto this_min = getUnsignedMin();
   auto this_max = getUnsignedMax();
   auto [min, max] = std::minmax(truncExt(this_min, bitwidth, false), truncExt(this_max, bitwidth, false));
   auto zextRes = Range(Regular, bitwidth, min, max);
   if(zextRes.isFullSet())
   {
      zextRes = Range(Regular, bitwidth);
   }
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("zextOrTrunc-res: " << zextRes);
   #endif
   return zextRes;
}

Range Range::intersectWith(const Range& other) const
{
   if(isEmpty() || isUnknown())
   {
      return *this;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return other;
   }
   #ifdef DEBUG_RANGE_OP
   PRINT_MSG("intersectWith-a: " << *this << std::endl << "intersectWith-b: " << other);
   #endif

   if(!this->isAnti() && !other.isAnti())
   {
      APInt res_l = getLower() > other.getLower() ? getLower() : other.getLower();
      APInt res_u = getUpper() < other.getUpper() ? getUpper() : other.getUpper();
      if(res_u < res_l)
      {
         return Range(Empty, bw);
      }

      return Range(Regular, bw, res_l, res_u);
   }
   if(this->isAnti() && !other.isAnti())
   {
      auto antil = l;
      auto antiu = u;
      if(antil <= other.getLower())
      {
         if(other.getUpper() <= antiu)
         {
            return Range(Empty, bw);
         }
         APInt res_l = other.getLower() > antiu ? other.getLower() : antiu + 1;
         APInt res_u = other.getUpper();
         return Range(Regular, bw, res_l, res_u);
      }
      if(antiu >= other.getUpper())
      {
         THROW_ASSERT(!other.getLower() >= antil, "");
         APInt res_l = other.getLower();
         APInt res_u = other.getUpper() < antil ? other.getUpper() : antil - 1;
         return Range(Regular, bw, res_l, res_u);
      }
      if(other.getLower() == Min && other.getUpper() == Max)
      {
         return *this;
      }
      if(antil > other.getUpper() || antiu < other.getLower())
      {
         return other;
      }

      // we approximate to the range of other
      return other;
   }
   if(!this->isAnti() && other.isAnti())
   {
      auto antil = other.l;
      auto antiu = other.u;
      if(antil <= this->getLower())
      {
         if(this->getUpper() <= antiu)
         {
            return Range(Empty, bw);
         }
         APInt res_l = this->getLower() > antiu ? this->getLower() : antiu + 1;
         APInt res_u = this->getUpper();
         return Range(Regular, bw, res_l, res_u);
      }
      if(antiu >= this->getUpper())
      {
         THROW_ASSERT(!this->getLower() >= antil, "");
         APInt res_l = this->getLower();
         APInt res_u = this->getUpper() < antil ? this->getUpper() : antil - 1;
         return Range(Regular, bw, res_l, res_u);
      }
      if(this->getLower() == Min && this->getUpper() == Max)
      {
         return other;
      }
      if(antil > this->getUpper() || antiu < this->getLower())
      {
         return *this;
      }

      // we approximate to the range of this
      return *this;
   }

   APInt antil_a = l;
   APInt antiu_a = u;
   APInt antil_b = other.l;
   APInt antiu_b = other.u;
   if(antil_a > antil_b)
   {
      std::swap(antil_a, antil_b);
      std::swap(antiu_a, antiu_b);
   }
   if(antil_b > (antiu_a + 1))
   {
      return Range(Anti, bw, antil_a, antiu_a);
   }
   APInt res_l = antil_a;
   APInt res_u = antiu_a > antiu_b ? antiu_a : antiu_b;
   if(res_l == Min && res_u == Max)
   {
      return Range(Empty, bw);
   }

   return Range(Anti, bw, res_l, res_u);
}

Range Range::unionWith(const Range& other) const
{
   if(this->isEmpty() || this->isUnknown())
   {
      return other;
   }
   if(other.isEmpty() || other.isUnknown())
   {
      return *this;
   }
   if(!this->isAnti() && !other.isAnti())
   {
      APInt res_l = getLower() < other.getLower() ? getLower() : other.getLower();
      APInt res_u = getUpper() > other.getUpper() ? getUpper() : other.getUpper();
      return Range(Regular, bw, res_l, res_u);
   }
   if(this->isAnti() && !other.isAnti())
   {
      auto antil = l;
      auto antiu = u;
      THROW_ASSERT(antil != Min, "");
      THROW_ASSERT(antiu != Max, "");
      if(antil > other.getUpper() || antiu < other.getLower())
      {
         return *this;
      }
      if(antil > other.getLower() && antiu < other.getUpper())
      {
         return Range(Regular, bw);
      }
      if(antil >= other.getLower() && antiu > other.getUpper())
      {
         return Range(Anti, bw, other.getUpper() + 1, antiu);
      }
      if(antil < other.getLower() && antiu <= other.getUpper())
      {
         return Range(Anti, bw, antil, other.getLower() - 1);
      }

      return Range(Regular, bw); // approximate to the full set
   }
   if(!this->isAnti() && other.isAnti())
   {
      auto antil = other.l;
      auto antiu = other.u;
      THROW_ASSERT(antil != Min, "");
      THROW_ASSERT(antiu != Max, "");
      if(antil > this->getUpper() || antiu < this->getLower())
      {
         return other;
      }
      if(antil > this->getLower() && antiu < this->getUpper())
      {
         return Range(Regular, bw);
      }
      if(antil >= this->getLower() && antiu > this->getUpper())
      {
         return Range(Anti, bw, this->getUpper() + 1, antiu);
      }
      if(antil < this->getLower() && antiu <= this->getUpper())
      {
         return Range(Anti, bw, antil, this->getLower() - 1);
      }

      return Range(Regular, bw); // approximate to the full set
   }

   auto antil_a = l;
   auto antiu_a = u;
   THROW_ASSERT(antil_a != Min, "");
   THROW_ASSERT(antiu_a != Max, "");
   auto antil_b = other.l;
   auto antiu_b = other.u;
   THROW_ASSERT(antil_b != Min, "");
   THROW_ASSERT(antiu_b != Max, "");
   if(antil_a > antiu_b || antiu_a < antil_b)
   {
      return Range(Regular, bw);
   }
   if(antil_a > antil_b && antiu_a < antiu_b)
   {
      return *this;
   }
   if(antil_b > antil_a && antiu_b < antiu_a)
   {
      return *this;
   }
   if(antil_a >= antil_b && antiu_b <= antiu_a)
   {
      return Range(Anti, bw, antil_a, antiu_b);
   }
   if(antil_b >= antil_a && antiu_a <= antiu_b)
   {
      return Range(Anti, bw, antil_b, antiu_a);
   }

   THROW_UNREACHABLE("unsupported condition");
}

Range Range::BestRange(const Range& UR, const Range& SR, unsigned bw) const
{
   if(UR.isFullSet() && SR.isFullSet())
   {
      return Range(Regular, bw);
   }
   if(UR.isFullSet())
   {
      return SR.truncate(bw);
   }
   if(SR.isFullSet())
   {
      return UR.truncate(bw);
   }
   auto nbitU = neededBits(UR.getUnsignedMin(), UR.getUnsignedMax(), false);
   auto nbitS = neededBits(SR.getSignedMin(), SR.getSignedMax(), true);
   if(nbitU < nbitS)
   {
      return UR.truncate(bw);
   }
   return SR.truncate(bw);
}

bool Range::operator==(const Range& other) const
{
   return bw == other.bw && Range::isSameType(*this, other) && Range::isSameRange(*this, other);
}

bool Range::operator!=(const Range& other) const
{
   return bw != other.bw || !Range::isSameType(*this, other) || !Range::isSameRange(*this, other);
}

void Range::print(std::ostream& OS) const
{
   if(this->isUnknown())
   {
      OS << "Unknown";
   }
   else if(this->isEmpty())
   {
      OS << "Empty";
   }
   else if(this->isAnti())
   {
      if(l == Min)
      {
         OS << ")-inf,";
      }
      else
      {
         OS << ")" << l.str() << ",";
      }
      OS << bw << ",";
      if(u == Max)
      {
         OS << "+inf(";
      }
      else
      {
         OS << u.str() << "(";
      }
   }
   else
   {
      if(l == Min)
      {
         OS << "[-inf,";
      }
      else
      {
         OS << "[" << l.str() << ",";
      }
      OS << bw << ",";
      if(u == Max)
      {
         OS << "+inf]";
      }
      else
      {
         OS << u.str() << "]";
      }
   }
}

std::string Range::ToString() const
{
   std::stringstream ss;
   print(ss);
   return ss.str();
}

std::ostream& operator<<(std::ostream& OS, const Range& R)
{
   R.print(OS);
   return OS;
}

Range Range::makeSatisfyingCmpRegion(kind pred, const Range& Other)
{
   unsigned bw = Other.bw;
   if(Other.isUnknown())
   {
      return Range(Unknown, bw);
   }
   if(Other.isEmpty())
   {
      return Range(Empty, bw);
   }
   if(Other.isAnti() && pred != eq_expr_K && pred != ne_expr_K)
   {
      THROW_UNREACHABLE("Invalid request " + tree_node::GetString(pred) + " " + Other.ToString());
      return Range(Empty, bw);
   }

   switch (pred)
   {
   case ge_expr_K:
      return Range(Regular, bw, Other.getSignedMax(), getSignedMaxValue(bw));
   case gt_expr_K:
      return Range(Regular, bw, Other.getSignedMax() + Epsilon, getSignedMaxValue(bw));
   case le_expr_K:
      return Range(Regular, bw, getSignedMinValue(bw), Other.getSignedMin());
   case lt_expr_K:
      return Range(Regular, bw, getSignedMinValue(bw), Other.getSignedMin() - Epsilon);
   case unge_expr_K:
      return Range(Regular, bw, Other.getUnsignedMax(), getMaxValue(bw));
   case ungt_expr_K:
      return Range(Regular, bw, Other.getUnsignedMax() + Epsilon, getMaxValue(bw));
   case unle_expr_K:
      return Range(Regular, bw, getMinValue(bw), Other.getUnsignedMin());
   case unlt_expr_K:
      return Range(Regular, bw, getMinValue(bw), Other.getUnsignedMin() - Epsilon);
   case eq_expr_K:
      return Other;
   case ne_expr_K:
      return Other.getAnti();
   
   case uneq_expr_K:
   default:
      break;
   }

   THROW_UNREACHABLE("Unhandled compare operation (" + boost::lexical_cast<std::string>(pred) + ")");
}

// ========================================================================== //
// VarNode
// ========================================================================== //
class VarNode
{
   private:
   /// The program variable
   const tree_nodeConstRef V;
   /// A Range associated to the variable, that is,
   /// its interval inferred by the analysis.
   Range interval;
   /// Used by the crop meet operator
   char abstractState;

   public:
   explicit VarNode(const tree_nodeConstRef _V);
   ~VarNode();
   VarNode(const VarNode&) = delete;
   VarNode(VarNode&&) = delete;
   VarNode& operator=(const VarNode&) = delete;
   VarNode& operator=(VarNode&&) = delete;

   /// Initializes the value of the node.
   void init(bool outside);
   /// Returns the range of the variable represented by this node.
   const Range getRange() const
   {
      return interval;
   }
   /// Returns the variable represented by this node.
   tree_nodeConstRef getValue() const
   {
      return V;
   }
   unsigned int getBitWidth() const
   {
      return interval.getBitWidth();
   }
   /// Changes the status of the variable represented by this node.
   void setRange(const Range newInterval)
   {
      interval = newInterval;
   }

   /// Pretty print.
   void print(std::ostream& OS) const;
   std::string ToString() const;
   char getAbstractState()
   {
      return abstractState;
   }
   // The possible states are '0', '+', '-' and '?'.
   void storeAbstractState();
};

/// The ctor.
VarNode::VarNode(const tree_nodeConstRef _V) : V(_V), interval(Unknown, getGIMPLE_BW(_V), Min, Max), abstractState(0)
{
   THROW_ASSERT(_V != nullptr, "Variable cannot be null");
   THROW_ASSERT(_V->get_kind() == tree_reindex_K, "Variable should be a tree_reindex node");
}

/// The dtor.
VarNode::~VarNode() = default;

/// Initializes the value of the node.
void VarNode::init(bool outside)
{
   auto bw = getGIMPLE_BW(V);
   THROW_ASSERT(bw, "Bitwidth not valid");
   if(const auto* CI = GetPointer<const integer_cst>(GET_CONST_NODE(V)))
   {
      APInt tmp = CI->value;
      this->setRange(Range(Regular, bw, tmp, tmp));
   }
   else
   {
      if(!outside)
      {
         // Initialize with a basic, unknown, interval.
         this->setRange(Range(Unknown, bw));
      }
      else
      {
         this->setRange(Range(Regular, bw));
      }
   }
}

/// Pretty print.
void VarNode::print(std::ostream& OS) const
{
   if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V)))
   {
      OS << C->value;
   }
   else
   {
      printVarName(V, OS);
   }
   OS << " ";
   this->getRange().print(OS);
}

std::string VarNode::ToString() const
{
   std::stringstream ss;
   print(ss);
   return ss.str();
}

void VarNode::storeAbstractState()
{
   THROW_ASSERT(!this->interval.isUnknown(), "storeAbstractState doesn't handle empty set");

   if(this->interval.getLower() == Min)
   {
      if(this->interval.getUpper() == Max)
      {
         this->abstractState = '?';
      }
      else
      {
         this->abstractState = '-';
      }
   }
   else if(this->interval.getUpper() == Max)
   {
      this->abstractState = '+';
   }
   else
   {
      this->abstractState = '0';
   }
}

std::ostream& operator<<(std::ostream& OS, const VarNode* VN)
{
   VN->print(OS);
   return OS;
}

// ========================================================================== //
// BasicInterval
// ========================================================================== //
enum IntervalId
{
   BasicIntervalId,
   SymbIntervalId
};

/// This class represents a basic interval of values. This class could inherit
/// from LLVM's Range class, since it *is a Range*. However,
/// LLVM's Range class doesn't have a virtual constructor.
class BasicInterval
{
   private:
   Range range;

   public:
   BasicInterval();
   explicit BasicInterval(Range range);
   virtual ~BasicInterval(); // This is a base class.
   BasicInterval(const BasicInterval&) = delete;
   BasicInterval(BasicInterval&&) = delete;
   BasicInterval& operator=(const BasicInterval&) = delete;
   BasicInterval& operator=(BasicInterval&&) = delete;

   // Methods for RTTI
   virtual IntervalId getValueId() const
   {
      return BasicIntervalId;
   }
   static bool classof(BasicInterval const* /*unused*/)
   {
      return true;
   }

   /// Returns the range of this interval.
   const Range& getRange() const
   {
      return this->range;
   }
   /// Sets the range of this interval to another range.
   void setRange(const Range& newRange)
   {
      this->range = newRange;
   }

   /// Pretty print.
   virtual void print(std::ostream& OS) const;
   std::string ToString() const;
};

BasicInterval::BasicInterval(Range range) : range(std::move(range))
{
}

BasicInterval::BasicInterval() : range(Range(Regular, MAX_BIT_INT))
{
}

// This is a base class, its dtor must be virtual.
BasicInterval::~BasicInterval() = default;

/// Pretty print.
void BasicInterval::print(std::ostream& OS) const
{
   this->getRange().print(OS);
}

std::string BasicInterval::ToString() const
{
   std::stringstream ss;
   print(ss);
   return ss.str();
}

std::ostream& operator<<(std::ostream& OS, const BasicInterval* BI)
{
   BI->print(OS);
   return OS;
}

// ========================================================================== //
// SymbInterval
// ========================================================================== //

/// This is an interval that contains a symbolic limit, which is
/// given by the bounds of a program name, e.g.: [-inf, ub(b) + 1].
class SymbInterval : public BasicInterval
{
   private:
   /// The bound. It is a node which limits the interval of this range.
   const tree_nodeConstRef bound;
   /// The predicate of the operation in which this interval takes part.
   /// It is useful to know how we can constrain this interval
   /// after we fix the intersects.
   kind pred;

   public:
   SymbInterval(const Range& range, const tree_nodeConstRef bound, kind pred);
   ~SymbInterval() override;
   SymbInterval(const SymbInterval&) = delete;
   SymbInterval(SymbInterval&&) = delete;
   SymbInterval& operator=(const SymbInterval&) = delete;
   SymbInterval& operator=(SymbInterval&&) = delete;

   // Methods for RTTI
   IntervalId getValueId() const override
   {
      return SymbIntervalId;
   }
   static bool classof(SymbInterval const* /*unused*/)
   {
      return true;
   }
   static bool classof(BasicInterval const* BI)
   {
      return BI->getValueId() == SymbIntervalId;
   }

   /// Returns the opcode of the operation that create this interval.
   kind getOperation() const
   {
      return this->pred;
   }
   /// Returns the node which is the bound of this interval.
   const tree_nodeConstRef getBound() const
   {
      return this->bound;
   }
   /// Replace symbolic intervals with hard-wired constants.
   Range fixIntersects(VarNode* bound, VarNode* sink);

   /// Prints the content of the interval.
   void print(std::ostream& OS) const override;
};

SymbInterval::SymbInterval(const Range& range, const tree_nodeConstRef bound, kind pred) : BasicInterval(range), bound(bound), pred(pred)
{
}

SymbInterval::~SymbInterval() = default;

Range SymbInterval::fixIntersects(VarNode* bound, VarNode* sink)
{
   // Get the lower and the upper bound of the
   // node which bounds this intersection.
   const auto boundRange = bound->getRange();
   const auto sinkRange = sink->getRange();
   THROW_ASSERT(!boundRange.isEmpty(), "");
   THROW_ASSERT(!sinkRange.isEmpty(), "");

   auto IsAnti = bound->getRange().isAnti() || sinkRange.isAnti();
   APInt l = IsAnti ? (boundRange.isUnknown() ? Min : boundRange.getUnsignedMin()) : boundRange.getLower();
   APInt u = IsAnti ? (boundRange.isUnknown() ? Max : boundRange.getUnsignedMax()) : boundRange.getUpper();

   // Get the lower and upper bound of the interval of this operation
   APInt lower = IsAnti ? (sinkRange.isUnknown() ? Min : sinkRange.getUnsignedMin()) : sinkRange.getLower();
   APInt upper = IsAnti ? (sinkRange.isUnknown() ? Max : sinkRange.getUnsignedMax()) : sinkRange.getUpper();

   auto bw = getRange().getBitWidth();
   switch(this->getOperation())
   {
      case eq_expr_K: // equal
         return Range(Regular, bw, l, u);
      case le_expr_K: // signed less or equal
         if(lower > u)
         {
            return Range(Empty, bw);
         }
         else
         {
            return Range(Regular, bw, lower, u);
         }
      case lt_expr_K: // signed less than
         if(u != Max)
         {
            if(lower > (u - 1))
            {
               return Range(Empty, bw);
            }

            return Range(Regular, bw, lower, u - 1);
         }
         else
         {
            if(lower > u)
            {
               return Range(Empty, bw);
            }

            return Range(Regular, bw, lower, u);
         }
      case ge_expr_K: // signed greater or equal
         if(l > upper)
         {
            return Range(Empty, bw);
         }
         else
         {
            return Range(Regular, bw, l, upper);
         }
      case gt_expr_K: // signed greater than
         if(l != Min)
         {
            if((l + 1) > upper)
            {
               return Range(Empty, bw);
            }

            return Range(Regular, bw, l + 1, upper);
         }
         else
         {
            if(l > upper)
            {
               return Range(Empty, bw);
            }

            return Range(Regular, bw, l, upper);
         }
      default:
         return Range(Regular, bw);
   }
   THROW_UNREACHABLE("unexpected condition");
}

/// Pretty print.
void SymbInterval::print(std::ostream& OS) const
{
   auto bnd = getBound();
   switch(this->getOperation())
   {
      case eq_expr_K: // equal
         OS << "[lb(";
         printVarName(bnd, OS);
         OS << "), ub(";
         printVarName(bnd, OS);
         OS << ")]";
         break;
      case le_expr_K: // sign less or equal
         OS << "[-inf, ub(";
         printVarName(bnd, OS);
         OS << ")]";
         break;
      case lt_expr_K: // sign less than
         OS << "[-inf, ub(";
         printVarName(bnd, OS);
         OS << ") - 1]";
         break;
      case ge_expr_K: // sign greater or equal
         OS << "[lb(";
         printVarName(bnd, OS);
         OS << "), +inf]";
         break;
      case gt_expr_K: // sign greater than
         OS << "[lb(";
         printVarName(bnd, OS);
         OS << " - 1), +inf]";
         break;
      default:
         OS << "Unknown Instruction.\n";
   }
}

// ========================================================================== //
// BasicOp
// ========================================================================== //

/// This class represents a generic operation in our analysis.
class BasicOp
{
   private:
   /// The range of the operation. Each operation has a range associated to it.
   /// This range is obtained by inspecting the branches in the source program
   /// and extracting its condition and intervals.
   std::shared_ptr<BasicInterval> intersect;
   // The target of the operation, that is, the node which
   // will store the result of the operation.
   VarNode* sink;
   // The instruction that originated this op node
   const tree_nodeConstRef inst;

   protected:
   /// We do not want people creating objects of this class,
   /// but we want to inherit from it.
   BasicOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst);

   public:
   enum class OperationId
   {
      UnaryOpId,
      SigmaOpId,
      BinaryOpId,
      TernaryOpId,
      PhiOpId,
      ControlDepId,
      LoadOpId,
      StoreOpId
   };

   /// The dtor. It's virtual because this is a base class.
   virtual ~BasicOp();
   // We do not want people creating objects of this class.
   BasicOp(const BasicOp&) = delete;
   BasicOp(BasicOp&&) = delete;
   BasicOp& operator=(const BasicOp&) = delete;
   BasicOp& operator=(BasicOp&&) = delete;

   // Methods for RTTI
   virtual OperationId getValueId() const = 0;
   static bool classof(BasicOp const* /*unused*/)
   {
      return true;
   }

   /// Given the input of the operation and the operation that will be
   /// performed, evaluates the result of the operation.
   virtual Range eval() = 0;
   /// Return the instruction that originated this op node
   const tree_nodeConstRef getInstruction() const
   {
      return inst;
   }
   /// Replace symbolic intervals with hard-wired constants.
   void fixIntersects(VarNode* V);
   /// Returns the range of the operation.
   std::shared_ptr<BasicInterval> getIntersect() const
   {
      return intersect;
   }
   /// Changes the interval of the operation.
   void setIntersect(const Range& newIntersect)
   {
      this->intersect->setRange(newIntersect);
   }
   /// Returns the target of the operation, that is,
   /// where the result will be stored.
   const VarNode* getSink() const
   {
      return sink;
   }
   /// Returns the target of the operation, that is,
   /// where the result will be stored.
   VarNode* getSink()
   {
      return sink;
   }

   /// Prints the content of the operation.
   virtual void print(std::ostream& OS) const = 0;
};

/// We can not want people creating objects of this class,
/// but we want to inherit of it.
BasicOp::BasicOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst) : intersect(std::move(intersect)), sink(sink), inst(inst)
{
}

/// We can not want people creating objects of this class,
/// but we want to inherit of it.
BasicOp::~BasicOp() = default;

/// Replace symbolic intervals with hard-wired constants.
void BasicOp::fixIntersects(VarNode* V)
{
   if(auto SI = std::dynamic_pointer_cast<SymbInterval>(getIntersect()))
   {
      this->setIntersect(SI->fixIntersects(V, getSink()));
   }
}

// ========================================================================== //
// PhiOp
// ========================================================================== //

/// A constraint like sink = phi(src1, src2, ..., srcN)
class PhiOp : public BasicOp
{
   private:
   // Vector of sources
   std::vector<const VarNode*> sources;
   /// Computes the interval of the sink based on the interval of the sources,
   /// the operation and the interval associated to the operation.
   Range eval() override;

   public:
   PhiOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst);
   ~PhiOp() override = default;
   PhiOp(const PhiOp&) = delete;
   PhiOp(PhiOp&&) = delete;
   PhiOp& operator=(const PhiOp&) = delete;
   PhiOp& operator=(PhiOp&&) = delete;

   /// Add source to the vector of sources
   void addSource(const VarNode* newsrc)
   {
      sources.push_back(newsrc);
   }
   /// Return source identified by index
   const VarNode* getSource(unsigned index) const
   {
      return sources[index];
   }
   /// return the number of sources
   size_t getNumSources() const
   {
      return sources.size();
   }

   // Methods for RTTI
   OperationId getValueId() const override
   {
      return OperationId::PhiOpId;
   }
   static bool classof(PhiOp const* /*unused*/)
   {
      return true;
   }
   static bool classof(BasicOp const* BO)
   {
      return BO->getValueId() == OperationId::PhiOpId;
   }

   /// Prints the content of the operation. I didn't it an operator overload
   /// because I had problems to access the members of the class outside it.
   void print(std::ostream& OS) const override;
};

// The ctor.
PhiOp::PhiOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst) : BasicOp(std::move(intersect), sink, inst)
{
}

/// Computes the interval of the sink based on the interval of the sources.
/// The result of evaluating a phi-function is the union of the ranges of
/// every variable used in the phi.
Range PhiOp::eval()
{
   THROW_ASSERT(sources.size() > 0, "Phi operation sources list empty");
   Range result = this->getSource(0)->getRange();
   #ifdef DEBUG_BASICOP_EVAL
   PRINT_MSG(getSink());
   #endif
   // Iterate over the sources of the phiop
   for(const VarNode* varNode : sources)
   {
      #ifdef DEBUG_BASICOP_EVAL
      PRINT_MSG(" ->" << varNode->getRange());
      #endif
      result = result.unionWith(varNode->getRange());
   }
   #ifdef DEBUG_BASICOP_EVAL
   PRINT_MSG("=" << result);
   #endif
   bool test = this->getIntersect()->getRange().isMaxRange();
   if(!test)
   {
      Range aux = this->getIntersect()->getRange();
      #ifdef DEBUG_BASICOP_EVAL
      PRINT_MSG(" aux=" << aux);
      #endif
      Range intersect = result.intersectWith(aux);
      if(!intersect.isEmpty())
      {
         result = intersect;
      }
   }
   #ifdef DEBUG_BASICOP_EVAL
   PRINT_MSG(" res=" << result);
   #endif
   return result;
}

/// Prints the content of the operation. I didn't it an operator overload
/// because I had problems to access the members of the class outside it.
void PhiOp::print(std::ostream& OS) const
{
   const char* quot = R"(")";
   OS << " " << quot << this << quot << R"( [label=")";
   OS << "phi";
   OS << "\"]\n";
   for(const VarNode* varNode : sources)
   {
      const auto V = varNode->getValue();
      if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V)))
      {
         OS << " " << C->value << " -> " << quot << this << quot << "\n";
      }
      else
      {
         OS << " " << quot;
         printVarName(V, OS);
         OS << quot << " -> " << quot << this << quot << "\n";
      }
   }
   const auto VS = this->getSink()->getValue();
   OS << " " << quot << this << quot << " -> " << quot;
   printVarName(VS, OS);
   OS << quot << "\n";
}

// ========================================================================== //
// UnaryOp
// ========================================================================== //
/// A constraint like sink = operation(source) \intersec [l, u]
/// Examples: unary instructions such as truncation, sign extensions,
/// zero extensions.
class UnaryOp : public BasicOp
{
   private:
   // The source node of the operation.
   VarNode* source;
   // The opcode of the operation.
   kind opcode;
   /// Computes the interval of the sink based on the interval of the sources,
   /// the operation and the interval associated to the operation.
   Range eval() override;

   public:
   UnaryOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, tree_nodeConstRef inst, VarNode* source, kind opcode);
   ~UnaryOp() override;
   UnaryOp(const UnaryOp&) = delete;
   UnaryOp(UnaryOp&&) = delete;
   UnaryOp& operator=(const UnaryOp&) = delete;
   UnaryOp& operator=(UnaryOp&&) = delete;

   // Methods for RTTI
   OperationId getValueId() const override
   {
      return OperationId::UnaryOpId;
   }
   static bool classof(UnaryOp const* /*unused*/)
   {
      return true;
   }
   static bool classof(BasicOp const* BO)
   {
      return BO->getValueId() == OperationId::UnaryOpId || BO->getValueId() == OperationId::SigmaOpId;
   }

   /// Return the opcode of the operation.
   kind getOpcode() const
   {
      return opcode;
   }
   /// Returns the source of the operation.
   VarNode* getSource() const
   {
      return source;
   }

   /// Prints the content of the operation. I didn't it an operator overload
   /// because I had problems to access the members of the class outside it.
   void print(std::ostream& OS) const override;
};

UnaryOp::UnaryOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, tree_nodeConstRef inst, VarNode* source, kind opcode) : BasicOp(std::move(intersect), sink, inst), source(source), opcode(opcode)
{
}

// The dtor.
UnaryOp::~UnaryOp() = default;

/// Computes the interval of the sink based on the interval of the sources,
/// the operation and the interval associated to the operation.
Range UnaryOp::eval()
{
   unsigned bw = getSink()->getBitWidth();
   Range oprnd = source->getRange();
   bool oprndSigned = isSignedType(source->getValue());
   Range result(Unknown, bw, Min, Max);

   if(oprnd.isRegular() || oprnd.isAnti())
   {
      #ifdef DEBUG_BASICOP_EVAL
      PRINT_MSG(GET_CONST_NODE(getSink()->getValue())->ToString() << std::endl << oprnd);
      #endif
            
      switch(this->getOpcode())
      {
         case abs_expr_K:
         {
            if(oprndSigned)
            {
               result = oprnd.abs();
            }
         }
         break;
         case nop_expr_K:
         {
            if(bw < getSource()->getBitWidth())
            {
               result = oprnd.truncate(bw);
            }
            else
            {
               if(oprndSigned)
               {
                  result = oprnd.sextOrTrunc(bw);
               }
               else
               {
                  result = oprnd.zextOrTrunc(bw);
               }
            }
         }
         break;
         case view_convert_expr_K:
         {
            const auto sourceType = GetPointer<const ssa_name>(GET_CONST_NODE(getSource()->getValue()))->type;
            const auto sinkType = GetPointer<const ssa_name>(GET_CONST_NODE(getSink()->getValue()))->type;
            PRINT_MSG("View conversion from " + GET_CONST_NODE(sourceType)->get_kind_text() + " to " + GET_CONST_NODE(sinkType)->get_kind_text());
            result = oprnd;
         }
         break;
         
         default:
            THROW_UNREACHABLE("Unhandled unary operation");
            break;
      }
      #ifdef DEBUG_BASICOP_EVAL
      PRINT_MSG("=" << result);
      #endif
   }
   else if(oprnd.isEmpty())
   {
      result = Range(Empty, bw);
   }

   auto test = this->getIntersect()->getRange().isMaxRange();
   if(!test)
   {
      Range aux = this->getIntersect()->getRange();
      Range intersect = result.intersectWith(aux);
      if(!intersect.isEmpty())
      {
         result = intersect;
      }
      #ifdef DEBUG_BASICOP_EVAL
      PRINT_MSG("intersection: " << result);
      #endif
   }
   return result;
}

/// Prints the content of the operation. I didn't it an operator overload
/// because I had problems to access the members of the class outside it.
void UnaryOp::print(std::ostream& OS) const
{
   const char* quot = R"(")";
   OS << " " << quot << this << quot << R"( [label=")";

   // Instruction bitwidth
   unsigned bw = getSink()->getBitWidth();

   switch(this->opcode)
   {
      case nop_expr_K:
      {
         if(bw < getSource()->getBitWidth())
         {
            OS << "trunc i" << bw;
         }
         else
         {
            auto type = tree_helper::CGetType(GET_CONST_NODE(getSource()->getValue()));
            if(const auto* int_type = GetPointer<const integer_type>(type))
            {
               if(int_type->unsigned_flag)
               {
                  OS << "zext i" << bw;
               }
               else
               {
                  OS << "sext i" << bw;
               }
            }
            else if(type->get_kind() == boolean_type_K)
            {
               OS << "zext b" << bw;
            }
            else
            {
               THROW_UNREACHABLE("Source should be of type integer");
            }
         }
      }
      break;
         
      case fix_trunc_expr_K:
      {
         auto type = tree_helper::CGetType(GET_CONST_NODE(getSink()->getValue()));
         if(const auto* int_type = GetPointer<const integer_type>(type))
         {
            if(int_type->unsigned_flag)
            {
               OS << "fptoui i" << bw;
            }
            else
            {
               OS << "fptosi i" << bw;
            }
         }
         else
         {
            THROW_UNREACHABLE("Sink should be of type integer");
         }
      }
      break;
      default:
         // Phi functions, Loads and Stores are handled here.
         this->getIntersect()->print(OS);
         break;
   }

   OS << "\"]\n";

   const auto V = this->getSource()->getValue();
   if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V)))
   {
      OS << " " << C->value << " -> " << quot << this << quot << "\n";
   }
   else
   {
      OS << " " << quot;
      printVarName(V, OS);
      OS << quot << " -> " << quot << this << quot << "\n";
   }

   const auto VS = this->getSink()->getValue();
   OS << " " << quot << this << quot << " -> " << quot;
   printVarName(VS, OS);
   OS << quot << "\n";
}

// ========================================================================== //
// SigmaOp
// ========================================================================== //
/// Specific type of UnaryOp used to represent sigma functions
class SigmaOp : public UnaryOp
{
   private:
   /// Computes the interval of the sink based on the interval of the sources,
   /// the operation and the interval associated to the operation.
   Range eval() override;

   // The symbolic source node of the operation.
   VarNode* SymbolicSource;

   bool unresolved;

   public:
   SigmaOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst, VarNode* source, VarNode* SymbolicSource, kind opcode);
   ~SigmaOp() override = default;
   SigmaOp(const SigmaOp&) = delete;
   SigmaOp(SigmaOp&&) = delete;
   SigmaOp& operator=(const SigmaOp&) = delete;
   SigmaOp& operator=(SigmaOp&&) = delete;

   // Methods for RTTI
   OperationId getValueId() const override
   {
      return OperationId::SigmaOpId;
   }
   static bool classof(SigmaOp const* /*unused*/)
   {
      return true;
   }
   static bool classof(UnaryOp const* UO)
   {
      return UO->getValueId() == OperationId::SigmaOpId;
   }
   static bool classof(BasicOp const* BO)
   {
      return BO->getValueId() == OperationId::SigmaOpId;
   }

   bool isUnresolved() const
   {
      return unresolved;
   }
   void markResolved()
   {
      unresolved = false;
   }
   void markUnresolved()
   {
      unresolved = true;
   }

   /// Prints the content of the operation. I didn't it an operator overload
   /// because I had problems to access the members of the class outside it.
   void print(std::ostream& OS) const override;
};

SigmaOp::SigmaOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst, VarNode* source, VarNode* _SymbolicSource, kind opcode)
      : UnaryOp(std::move(intersect), sink, inst, source, opcode), SymbolicSource(_SymbolicSource), unresolved(false)
{
}

/// Computes the interval of the sink based on the interval of the sources,
/// the operation and the interval associated to the operation.
Range SigmaOp::eval()
{
   Range result = this->getSource()->getRange();
   #ifdef DEBUG_BASICOP_EVAL
   PRINT_MSG("SigmaOp: " << getSink() << " src=" << result);
   #endif
   Range aux = this->getIntersect()->getRange();
   #ifdef DEBUG_BASICOP_EVAL
   PRINT_MSG(" aux=" << aux);
   #endif
   if(!aux.isUnknown())
   {
      Range intersect = result.intersectWith(aux);
      if(!intersect.isEmpty())
      {
         result = intersect;
      }
   }
   #ifdef DEBUG_BASICOP_EVAL
   PRINT_MSG(" = " << result);
   #endif
   return result;
}

/// Prints the content of the operation. I didn't it an operator overload
/// because I had problems to access the members of the class outside it.
void SigmaOp::print(std::ostream& OS) const
{
   const char* quot = R"(")";
   OS << " " << quot << this << quot << R"( [label=")"
      << "SigmnaOp:";
   this->getIntersect()->print(OS);
   OS << "\"]\n";
   const auto V = this->getSource()->getValue();
   if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V)))
   {
      OS << " " << C->value << " -> " << quot << this << quot << "\n";
   }
   else
   {
      OS << " " << quot;
      printVarName(V, OS);
      OS << quot << " -> " << quot << this << quot << "\n";
   }
   if(SymbolicSource)
   {
      const auto V = SymbolicSource->getValue();
      if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V)))
      {
         OS << " " << C->value << " -> " << quot << this << quot << "\n";
      }
      else
      {
         OS << " " << quot;
         printVarName(V, OS);
         OS << quot << " -> " << quot << this << quot << "\n";
      }
   }

   const auto VS = this->getSink()->getValue();
   OS << " " << quot << this << quot << " -> " << quot;
   printVarName(VS, OS);
   OS << quot << "\n";
}

// ========================================================================== //
// BinaryOp
// ========================================================================== //
/// A constraint like sink = source1 operation source2 intersect [l, u].
class BinaryOp : public BasicOp
{
   private:
   // The first operand.
   VarNode* source1;
   // The second operand.
   VarNode* source2;
   // The opcode of the operation.
   kind opcode;
   /// Computes the interval of the sink based on the interval of the sources,
   /// the operation and the interval associated to the operation.
   Range eval() override;

   public:
   BinaryOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst, VarNode* source1, VarNode* source2, kind opcode);
   ~BinaryOp() override = default;
   BinaryOp(const BinaryOp&) = delete;
   BinaryOp(BinaryOp&&) = delete;
   BinaryOp& operator=(const BinaryOp&) = delete;
   BinaryOp& operator=(BinaryOp&&) = delete;

   // Methods for RTTI
   OperationId getValueId() const override
   {
      return OperationId::BinaryOpId;
   }
   static bool classof(BinaryOp const* /*unused*/)
   {
      return true;
   }
   static bool classof(BasicOp const* BO)
   {
      return BO->getValueId() == OperationId::BinaryOpId;
   }

   /// Return the opcode of the operation.
   kind getOpcode() const
   {
      return opcode;
   }
   /// Returns the first operand of this operation.
   VarNode* getSource1() const
   {
      return source1;
   }
   /// Returns the second operand of this operation.
   VarNode* getSource2() const
   {
      return source2;
   }

   /// Prints the content of the operation. I didn't it an operator overload
   /// because I had problems to access the members of the class outside it.
   void print(std::ostream& OS) const override;
};

// The ctor.
BinaryOp::BinaryOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst, VarNode* source1, VarNode* source2, kind opcode) : BasicOp(std::move(intersect), sink, inst), source1(source1), source2(source2), opcode(opcode)
{
   const auto type = tree_helper::CGetType(GET_CONST_NODE(sink->getValue()));
   THROW_ASSERT(type->get_kind() == integer_type_K || type->get_kind() == boolean_type_K, "Binary operation sink should be of integer type (" + GET_CONST_NODE(sink->getValue())->ToString() + ")");
}

/// Computes the interval of the sink based on the interval of the sources,
/// the operation and the interval associated to the operation.
/// Basically, this function performs the operation indicated in its opcode
/// taking as its operands the source1 and the source2.
Range BinaryOp::eval()
{
   Range op1 = this->getSource1()->getRange();
   Range op2 = this->getSource2()->getRange();
   // Instruction bitwidth
   unsigned bw = getSink()->getBitWidth();
   Range result(Unknown, bw);

   // only evaluate if all operands are Regular
   if((op1.isRegular() || op1.isAnti()) && (op2.isRegular() || op2.isAnti()))
   {
      #ifdef DEBUG_BASICOP_EVAL
      PRINT_MSG(getSink()->getValue()->ToString() << std::endl << op1 << "," << op2);
      #endif
      const auto type = tree_helper::CGetType(GET_CONST_NODE(getSink()->getValue()));
      bool is_unsigned;
      if(const auto* int_type = GetPointer<const integer_type>(type))
      {
         is_unsigned = int_type->unsigned_flag;
         THROW_ASSERT(getSource1()->getBitWidth() == bw, "Source1 has different bitwidth from sink");
         THROW_ASSERT(getSource2()->getBitWidth() == bw, "Source2 has different bitwidth from sink");
      }
      else if(type->get_kind() == boolean_type_K)
      {
         THROW_ASSERT(isCompare(getOpcode()), "BinaryOp should be a compare when sink is a boolean");
      }
      else
      {
         THROW_UNREACHABLE("Unhandled sink type");
      }

      switch(this->getOpcode())
      {
         case plus_expr_K:
            result = op1.add(op2);
            break;
         case minus_expr_K:
            result = op1.sub(op2);
            break;
         case mult_expr_K:
            result = op1.mul(op2);
            break;
         case trunc_div_expr_K:
            result = is_unsigned ? op1.udiv(op2) : op1.sdiv(op2);
            break;
         case trunc_mod_expr_K:
            result = is_unsigned ? op1.urem(op2) : op1.srem(op2);
            break;
         case lshift_expr_K:
            result = op1.shl(op2);
            break;
         case rshift_expr_K:
            result = op1.shr(op2, !is_unsigned);
            break;
         case bit_and_expr_K:
            result = op1.And(op2);
            break;
         case bit_ior_expr_K:
            result = op1.Or(op2);
            break;
         case bit_xor_expr_K:
            result = op1.Xor(op2);
            break;
         case eq_expr_K:
            result = op1.Eq(op2, bw);
            break;
         case ne_expr_K:
            result = op1.Ne(op2, bw);
            break;
         case unge_expr_K:
            result = op1.Uge(op2, bw);
            break;
         case ungt_expr_K:
            result = op1.Ugt(op2, bw);
            break;
         case unlt_expr_K:
            result = op1.Ult(op2, bw);
            break;
         case unle_expr_K:
            result = op1.Ule(op2, bw);
            break;
         case gt_expr_K:
            result = op1.Sgt(op2, bw);
            break;
         case ge_expr_K:
            result = op1.Sge(op2, bw);
            break;
         case lt_expr_K:
            result = op1.Slt(op2, bw);
            break;
         case le_expr_K:
            result = op1.Sle(op2, bw);
            break;
         
         default:
            THROW_UNREACHABLE("Unhandled binary operation (" + tree_node::GetString(this->getOpcode()) + ")");
            break;
      }
      #ifdef DEBUG_BASICOP_EVAL
      PRINT_MSG("=" << result);
      #endif
      bool test = this->getIntersect()->getRange().isMaxRange();
      if(!test)
      {
         Range aux = this->getIntersect()->getRange();
         #ifdef DEBUG_BASICOP_EVAL
         PRINT_MSG("  aux=" << aux);
         #endif
         Range intersect = result.intersectWith(aux);
         if(!intersect.isEmpty())
         {
            result = intersect;
         }
      }
   }
   else
   {
      if(op1.isEmpty() || op2.isEmpty())
      {
         result = Range(Empty, bw);
      }
   }
   return result;
}

/// Pretty print.
void BinaryOp::print(std::ostream& OS) const
{
   const char* quot = R"(")";
   std::string opcodeName = tree_node::GetString(opcode);
   OS << " " << quot << this << quot << R"( [label=")" << opcodeName << "\"]\n";
   const auto V1 = this->getSource1()->getValue();
   if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V1)))
   {
      OS << " " << C->value << " -> " << quot << this << quot << "\n";
   }
   else
   {
      OS << " " << quot;
      printVarName(V1, OS);
      OS << quot << " -> " << quot << this << quot << "\n";
   }
   const auto V2 = this->getSource2()->getValue();
   if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V2)))
   {
      OS << " " << C->value << " -> " << quot << this << quot << "\n";
   }
   else
   {
      OS << " " << quot;
      printVarName(V2, OS);
      OS << quot << " -> " << quot << this << quot << "\n";
   }
   const auto VS = this->getSink()->getValue();
   OS << " " << quot << this << quot << " -> " << quot;
   printVarName(VS, OS);
   OS << quot << "\n";
}

// ========================================================================== //
// TernaryOp
// ========================================================================== //
class TernaryOp : public BasicOp
{
   private:
   // The first operand.
   VarNode* source1;
   // The second operand.
   VarNode* source2;
   // The third operand.
   VarNode* source3;
   // The opcode of the operation.
   kind opcode;
   /// Computes the interval of the sink based on the interval of the sources,
   /// the operation and the interval associated to the operation.
   Range eval() override;

   public:
   TernaryOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst, VarNode* source1, VarNode* source2, VarNode* source3, kind opcode);
   ~TernaryOp() override = default;
   TernaryOp(const TernaryOp&) = delete;
   TernaryOp(TernaryOp&&) = delete;
   TernaryOp& operator=(const TernaryOp&) = delete;
   TernaryOp& operator=(TernaryOp&&) = delete;

   // Methods for RTTI
   OperationId getValueId() const override
   {
      return OperationId::TernaryOpId;
   }
   static bool classof(TernaryOp const* /*unused*/)
   {
      return true;
   }
   static bool classof(BasicOp const* BO)
   {
      return BO->getValueId() == OperationId::TernaryOpId;
   }

   /// Return the opcode of the operation.
   kind getOpcode() const
   {
      return opcode;
   }
   /// Returns the first operand of this operation.
   VarNode* getSource1() const
   {
      return source1;
   }
   /// Returns the second operand of this operation.
   VarNode* getSource2() const
   {
      return source2;
   }
   /// Returns the third operand of this operation.
   VarNode* getSource3() const
   {
      return source3;
   }

   /// Prints the content of the operation. I didn't it an operator overload
   /// because I had problems to access the members of the class outside it.
   void print(std::ostream& OS) const override;
};

// The ctor.
TernaryOp::TernaryOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst, VarNode* source1, VarNode* source2, VarNode* source3, kind opcode)
      : BasicOp(std::move(intersect), sink, inst), source1(source1), source2(source2), source3(source3), opcode(opcode)
{
   const auto* ga = GetPointer<const gimple_assign>(GET_CONST_NODE(inst));
   THROW_ASSERT(ga, "TernaryOp associated statement should be a gimple_assign " + GET_CONST_NODE(inst)->ToString());
   const auto* I = GetPointer<const ternary_expr>(GET_CONST_NODE(ga->op1));
   THROW_ASSERT(I, "TernaryOp operator should be a ternary_expr");
   THROW_ASSERT(sink->getBitWidth() == source2->getBitWidth(), "Operator bitwidth mismatch");
   THROW_ASSERT(sink->getBitWidth() == source3->getBitWidth(), "Operator bitwidth mismatch");
}

Range TernaryOp::eval()
{
   Range op1 = this->getSource1()->getRange();
   Range op2 = this->getSource2()->getRange();
   Range op3 = this->getSource3()->getRange();
   // Instruction bitwidth
   unsigned bw = getSink()->getBitWidth();
   Range result(Unknown, bw, Min, Max);

   // only evaluate if all operands are Regular
   if((op1.isRegular() || op1.isAnti()) && (op2.isRegular() || op2.isAnti()) && (op3.isRegular() || op3.isAnti()))
   {
      #ifdef DEBUG_BASICOP_EVAL
      PRINT_MSG(getSink()->getValue()->ToString() << std::endl << op1 << "?" << op2 << ":" << op3);
      #endif
      switch(this->getOpcode())
      {
         case cond_expr_K:
         {
            // Source1 is the selector
            if(op1 == Range(Regular, op1.getBitWidth(), 1, 1))
            {
               result = op2;
            }
            else if(op1 == Range(Regular, op1.getBitWidth(), 0, 0))
            {
               result = op3;
            }
            else
            {
               const auto* ga = GetPointer<const gimple_assign>(GET_CONST_NODE(getInstruction()));
               const auto* I = GetPointer<const ternary_expr>(GET_CONST_NODE(ga->op1));
               const auto BranchVar = branchOpRecurse(I->op0);
               std::vector<const struct binary_expr*> BranchConds;
               // Check if branch variable is correlated with op1 or op2
               if(const auto* gp = GetPointer<const gimple_phi>(BranchVar))
               {
                  // TODO: find a way to propagate range from all phi edges when phi->res is one of the two result of the con_expr 
               }
               else if(const auto* BranchExpr = GetPointer<const binary_expr>(BranchVar))
               {
                  BranchConds.push_back(BranchExpr);
               }

               for(const auto* be : BranchConds)
               {
                  if(isCompare(be))
                  {
                     const tree_nodeConstRef CondOp0 = be->op0;
                     const tree_nodeConstRef CondOp1 = be->op1;
                     if(GET_CONST_NODE(CondOp0)->get_kind() == integer_cst_K || GET_CONST_NODE(CondOp1)->get_kind() == integer_cst_K)
                     {
                        const auto variable = GET_CONST_NODE(CondOp0)->get_kind() == integer_cst_K ? CondOp1 : CondOp0;
                        const auto* constant = GET_CONST_NODE(CondOp0)->get_kind() == integer_cst_K ? GetPointer<const integer_cst>(GET_CONST_NODE(CondOp0)) : GetPointer<const integer_cst>(GET_CONST_NODE(CondOp1));
                        auto opV1 = I->op1;
                        auto opV2 = I->op2;
                        if(variable == opV1 || variable == opV2)
                        {
                           auto bw = op2.getBitWidth();
                           Range CR(Regular, bw, constant->value, constant->value);
                           kind pred = be->get_kind();
                           kind swappred = op_swap(pred);

                           Range tmpT = (variable == CondOp0) ? Range::makeSatisfyingCmpRegion(pred, CR) : Range::makeSatisfyingCmpRegion(swappred, CR);
                           THROW_ASSERT(!tmpT.isFullSet(), "");

                           if(variable == opV2)
                           {
                              Range FValues = Range(tmpT.getAnti());
                              op3 = op3.intersectWith(FValues);
                           }
                           else
                           {
                              const Range& TValues = tmpT;
                              op2 = op2.intersectWith(TValues);
                           }
                        }
                     }
                  }
               }
               result = op2.unionWith(op3);
            }
            break;
         }
         default:
            break;
      }
      #ifdef DEBUG_BASICOP_EVAL
      PRINT_MSG("=" << result);
      #endif
      bool test = this->getIntersect()->getRange().isMaxRange();
      if(!test)
      {
         Range aux = this->getIntersect()->getRange();
         Range intersect = result.intersectWith(aux);
         if(!intersect.isEmpty())
         {
            result = intersect;
         }
      }
   }
   else
   {
      if(op1.isEmpty() || op2.isEmpty() || op3.isEmpty())
      {
         result = Range(Empty, bw);
      }
   }
   return result;
}

/// Pretty print.
void TernaryOp::print(std::ostream& OS) const
{
   const char* quot = R"(")";
   std::string opcodeName = tree_node::GetString(this->getOpcode());
   OS << " " << quot << this << quot << R"( [label=")" << opcodeName << "\"]\n";

   const auto V1 = this->getSource1()->getValue();
   if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V1)))
   {
      OS << " " << C->value << " -> " << quot << this << quot << "\n";
   }
   else
   {
      OS << " " << quot;
      printVarName(V1, OS);
      OS << quot << " -> " << quot << this << quot << "\n";
   }
   const auto V2 = this->getSource2()->getValue();
   if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V2)))
   {
      OS << " " << C->value << " -> " << quot << this << quot << "\n";
   }
   else
   {
      OS << " " << quot;
      printVarName(V2, OS);
      OS << quot << " -> " << quot << this << quot << "\n";
   }

   const auto V3 = this->getSource3()->getValue();
   if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V3)))
   {
      OS << " " << C->value << " -> " << quot << this << quot << "\n";
   }
   else
   {
      OS << " " << quot;
      printVarName(V3, OS);
      OS << quot << " -> " << quot << this << quot << "\n";
   }
   const auto VS = this->getSink()->getValue();
   OS << " " << quot << this << quot << " -> " << quot;
   printVarName(VS, OS);
   OS << quot << "\n";
}

// ========================================================================== //
// ControlDep
// ========================================================================== //
/// Specific type of BasicOp used in Nuutila's strongly connected
/// components algorithm.
class ControlDep : public BasicOp
{
   private:
   VarNode* source;
   Range eval() override;

   public:
   ControlDep(VarNode* sink, VarNode* source);
   ~ControlDep() override;
   ControlDep(const ControlDep&) = delete;
   ControlDep(ControlDep&&) = delete;
   ControlDep& operator=(const ControlDep&) = delete;
   ControlDep& operator=(ControlDep&&) = delete;

   // Methods for RTTI
   OperationId getValueId() const override
   {
      return OperationId::ControlDepId;
   }
   static bool classof(ControlDep const* /*unused*/)
   {
      return true;
   }
   static bool classof(BasicOp const* BO)
   {
      return BO->getValueId() == OperationId::ControlDepId;
   }

   /// Returns the source of the operation.
   VarNode* getSource() const
   {
      return source;
   }

   void print(std::ostream& OS) const override;
};

ControlDep::ControlDep(VarNode* sink, VarNode* source) : BasicOp(std::make_shared<BasicInterval>(), sink, nullptr), source(source)
{
}

ControlDep::~ControlDep() = default;

Range ControlDep::eval()
{
   return Range(Regular, MAX_BIT_INT);
}

void ControlDep::print(std::ostream& /*OS*/) const
{
}

// ========================================================================== //
// LoadOp
// ========================================================================== //
class LoadOp : public BasicOp
{
   private:
   /// reference to the memory access operand
   std::vector<const VarNode*> sources;
   Range eval() override;

   public:
   LoadOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst);
   ~LoadOp() override;
   LoadOp(const LoadOp&) = delete;
   LoadOp(LoadOp&&) = delete;
   LoadOp& operator=(const LoadOp&) = delete;
   LoadOp& operator=(LoadOp&&) = delete;

   // Methods for RTTI
   OperationId getValueId() const override
   {
      return OperationId::LoadOpId;
   }
   static bool classof(LoadOp const* /*unused*/)
   {
      return true;
   }
   static bool classof(BasicOp const* BO)
   {
      return BO->getValueId() == OperationId::LoadOpId;
   }

   /// Add source to the vector of sources
   void addSource(const VarNode* newsrc)
   {
      sources.push_back(newsrc);
   }
   /// Return source identified by index
   const VarNode* getSource(unsigned index) const
   {
      return sources[index];
   }
   /// return the number of sources
   size_t getNumSources() const
   {
      return sources.size();
   }

   void print(std::ostream& OS) const override;
};

LoadOp::LoadOp(std::shared_ptr<BasicInterval> intersect, VarNode* sink, const tree_nodeConstRef inst) : BasicOp(std::move(intersect), sink, inst)
{
}

LoadOp::~LoadOp() = default;

Range LoadOp::eval()
{
   unsigned bw = getSink()->getBitWidth();
   Range result(Unknown, bw, Min, Max);
   #ifdef DEBUG_BASICOP_EVAL
   PRINT_MSG(getSink()->getValue()->ToString());
   #endif
   if(getNumSources() == 0)
   {
      THROW_ASSERT(bw == getIntersect()->getRange().getBitWidth(), "");
      return getIntersect()->getRange();
   }

   result = this->getSource(0)->getRange();
   // Iterate over the sources of the load
   for(const VarNode* varNode : sources)
   {
      result = result.unionWith(varNode->getRange());
   }

   #ifdef DEBUG_BASICOP_EVAL
   PRINT_MSG("=" + result.ToString());
   #endif
   bool test = this->getIntersect()->getRange().isMaxRange();
   if(!test)
   {
      Range aux = this->getIntersect()->getRange();
      Range intersect = result.intersectWith(aux);
      if(!intersect.isEmpty())
      {
         result = intersect;
      }
   }
   return result;
}

void LoadOp::print(std::ostream& OS) const
{
   const char* quot = R"(")";
   OS << " " << quot << this << quot << R"( [label=")";
   OS << "LoadOp\"]\n";

   for(auto src : sources)
   {
      const auto V = src->getValue();
      if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V)))
      {
         OS << " " << C->value << " -> " << quot << this << quot << "\n";
      }
      else
      {
         OS << " " << quot;
         printVarName(V, OS);
         OS << quot << " -> " << quot << this << quot << "\n";
      }
   }

   const auto VS = this->getSink()->getValue();
   OS << " " << quot << this << quot << " -> " << quot;
   printVarName(VS, OS);
   OS << quot << "\n";
}

// ========================================================================== //
// StoreOp
// ========================================================================== //
class StoreOp : public BasicOp
{
   private:
   /// reference to the memory access operand
   std::vector<const VarNode*> sources;
   /// union of the values at which the variable is initialized
   Range init;
   Range eval() override;

   public:
   StoreOp(VarNode* sink, const tree_nodeConstRef inst, Range _init);
   ~StoreOp() override;
   StoreOp(const StoreOp&) = delete;
   StoreOp(StoreOp&&) = delete;
   StoreOp& operator=(const StoreOp&) = delete;
   StoreOp& operator=(StoreOp&&) = delete;

   // Methods for RTTI
   OperationId getValueId() const override
   {
      return OperationId::StoreOpId;
   }
   static bool classof(StoreOp const* /*unused*/)
   {
      return true;
   }
   static bool classof(BasicOp const* BO)
   {
      return BO->getValueId() == OperationId::StoreOpId;
   }

   /// Add source to the vector of sources
   void addSource(const VarNode* newsrc)
   {
      sources.push_back(newsrc);
   }
   /// Return source identified by index
   const VarNode* getSource(unsigned index) const
   {
      return sources[index];
   }
   /// return the number of sources
   size_t getNumSources() const
   {
      return sources.size();
   }

   void print(std::ostream& OS) const override;
};

StoreOp::StoreOp(VarNode* sink, const tree_nodeConstRef inst, Range _init) : BasicOp(std::make_shared<BasicInterval>(), sink, inst), init(std::move(_init))
{
}

StoreOp::~StoreOp() = default;

Range StoreOp::eval()
{
   Range result = init;
   // Iterate over the sources of the Store
   for(const VarNode* varNode : sources)
   {
      result = result.unionWith(varNode->getRange());
   }
   return result;
}

void StoreOp::print(std::ostream& OS) const
{
   const char* quot = R"(")";
   OS << " " << quot << this << quot << R"( [label=")";
   OS << "StoreOp\"]\n";

   for(auto src : sources)
   {
      const auto V = src->getValue();
      if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V)))
      {
         OS << " " << C->value << " -> " << quot << this << quot << "\n";
      }
      else
      {
         OS << " " << quot;
         printVarName(V, OS);
         OS << quot << " -> " << quot << this << quot << "\n";
      }
   }

   const auto VS = this->getSink()->getValue();
   OS << " " << quot << this << quot << " -> " << quot;
   printVarName(VS, OS);
   OS << quot << "\n";
}

// ========================================================================== //
// Nuutila
// ========================================================================== //
// The VarNodes type.
using VarNodes = std::map<tree_nodeConstRef, VarNode*, tree_reindexCompare>;

// A map from variables to the operations where these variables are used.
using UseMap = std::map<tree_nodeConstRef, std::set<BasicOp*>, tree_reindexCompare>;

// A map from variables to the operations where these
// variables are present as bounds
using SymbMap = std::map<tree_nodeConstRef, std::set<BasicOp*>, tree_reindexCompare>;

class Nuutila
{
   public:
   VarNodes* variables;
   int index;
   std::map<tree_nodeConstRef, int, tree_reindexCompare> dfs;
   std::map<tree_nodeConstRef, tree_nodeConstRef, tree_reindexCompare> root;
   std::set<tree_nodeConstRef, tree_reindexCompare> inComponent;
   std::map<tree_nodeConstRef, std::set<VarNode*>*, tree_reindexCompare> components;
   std::deque<tree_nodeConstRef> worklist;
   #ifdef SCC_DEBUG
   bool checkWorklist() const;
   bool checkComponents() const;
   bool checkTopologicalSort(const UseMap* useMap) const;
   bool hasEdge(const std::set<VarNode*>* componentFrom, const std::set<VarNode*>* componentTo, const UseMap* useMap) const;
   #endif
   public:
   Nuutila(VarNodes* varNodes, UseMap* useMap, SymbMap* symbMap);
   ~Nuutila();
   Nuutila(const Nuutila&) = delete;
   Nuutila(Nuutila&&) = delete;
   Nuutila& operator=(const Nuutila&) = delete;
   Nuutila& operator=(Nuutila&&) = delete;

   void addControlDependenceEdges(UseMap* useMap, const SymbMap* symbMap, const VarNodes* vars);
   void delControlDependenceEdges(UseMap* useMap);
   void visit(const tree_nodeConstRef V, std::stack<tree_nodeConstRef>& stack, const UseMap* useMap);
   using iterator = std::deque<tree_nodeConstRef>::reverse_iterator;
   using const_iterator = std::deque<tree_nodeConstRef>::const_reverse_iterator;
   iterator begin()
   {
      return worklist.rbegin();
   }
   const_iterator cbegin() const
   {
      return worklist.crbegin();
   }
   iterator end()
   {
      return worklist.rend();
   }
   const_iterator cend() const
   {
      return worklist.crend();
   }
};

/*
   *	Finds the strongly connected components in the constraint graph formed
   * by Variables and UseMap. The class receives the map of futures to insert
   * the control dependence edges in the constraint graph. These edges are removed
   * after the class is done computing the SCCs.
   */
Nuutila::Nuutila(VarNodes* varNodes, UseMap* useMap, SymbMap* symbMap)
{
   // Copy structures
   this->variables = varNodes;
   this->index = 0;

   // Iterate over all varnodes of the constraint graph
   for(const auto& [var, node] : *varNodes)
   {
      // Initialize DFS control variable for each Value in the graph
      dfs[var] = -1;
   }
   addControlDependenceEdges(useMap, symbMap, varNodes);
   // Iterate again over all varnodes of the constraint graph
   for(const auto& [var, node] : *varNodes)
   {
      // If the Value has not been visited yet, call visit for him
      if(dfs[var] < 0)
      {
         std::stack<tree_nodeConstRef> pilha;
         visit(var, pilha, useMap);
      }
   }
   delControlDependenceEdges(useMap);

   #ifdef SCC_DEBUG
   THROW_ASSERT(checkWorklist(), "An inconsistency in SCC worklist have been found");
   THROW_ASSERT(checkComponents(), "A component has been used more than once");
   THROW_ASSERT(checkTopologicalSort(useMap), "Topological sort is incorrect");
   #endif
}

Nuutila::~Nuutila()
{
   for(const auto& [var, m] : components)
   {
      delete m;
   }
}

/*
   *	Adds the edges that ensure that we solve a future before fixing its
   *  interval. I have created a new class: ControlDep edges, to represent
   *  the control dependencies. In this way, in order to delete these edges,
   *  one just need to go over the map of uses removing every instance of the
   *  ControlDep class.
   */
void Nuutila::addControlDependenceEdges(UseMap* useMap, const SymbMap* symbMap, const VarNodes* vars)
{
   for(const auto& [var, ops] : *symbMap)
   {
      for(const auto& op : ops)
      {
         THROW_ASSERT(static_cast<bool>(vars->count(var)), "Variable should be stored in VarNodes map");
         VarNode* source = vars->at(var);
         BasicOp* cdedge = new ControlDep(op->getSink(), source);
         useMap->operator[](var).insert(cdedge);
      }
   }
}

/*
   *	Removes the control dependence edges from the constraint graph.
   */
void Nuutila::delControlDependenceEdges(UseMap* useMap)
{
   for(auto& [var, ops] : *useMap)
   {
      std::deque<ControlDep*> cds;
      for(auto sit : ops)
      {
         if(ControlDep* cd = dynamic_cast<ControlDep*>(sit))
         {
            cds.push_back(cd);
         }
      }

      for(ControlDep* cd : cds)
      {
         // Add pseudo edge to the string
         const auto V = cd->getSource()->getValue();
         if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(V)))
         {
            pseudoEdgesString << " " << C->value << " -> ";
         }
         else
         {
            pseudoEdgesString << " " << '"';
            printVarName(V, pseudoEdgesString);
            pseudoEdgesString << '"' << " -> ";
         }
         const auto VS = cd->getSink()->getValue();
         pseudoEdgesString << '"';
         printVarName(VS, pseudoEdgesString);
         pseudoEdgesString << '"';
         pseudoEdgesString << " [style=dashed]\n";
         // Remove pseudo edge from the map
         ops.erase(cd);
      }
   }
}

/*
   *	Finds SCCs using Nuutila's algorithm. This algorithm is divided in
   *  two parts. The first calls the recursive visit procedure on every node
   *  in the constraint graph. The second phase revisits these nodes,
   *  grouping them in components.
   */
void Nuutila::visit(const tree_nodeConstRef V, std::stack<tree_nodeConstRef>& stack, const UseMap* useMap)
{
   dfs[V] = index;
   ++index;
   root[V] = V;

   // Visit every node defined in an instruction that uses V
   for(const auto& op : useMap->at(V))
   {
      auto name = op->getSink()->getValue();
      if(dfs[name] < 0)
      {
         visit(name, stack, useMap);
      }
      if((!static_cast<bool>(inComponent.count(name))) && (dfs[root[V]] >= dfs[root[name]]))
      {
         root[V] = root[name];
      }
   }

   // The second phase of the algorithm assigns components to stacked nodes
   if(root[V] == V)
   {
      // Neither the worklist nor the map of components is part of Nuutila's
      // original algorithm. We are using these data structures to get a
      // topological ordering of the SCCs without having to go over the root
      // list once more.
      worklist.push_back(V);
      auto SCC = new std::set<VarNode*>;
      SCC->insert(variables->at(V));
      inComponent.insert(V);
      while(!stack.empty() && (dfs[stack.top()] > dfs[V]))
      {
         auto node = stack.top();
         stack.pop();
         inComponent.insert(node);
         SCC->insert(variables->at(node));
      }
      components[V] = SCC;
   }
   else
   {
      stack.push(V);
   }
}

#ifdef SCC_DEBUG
bool Nuutila::checkWorklist() const
{
   bool consistent = true;
   for(auto nit = cbegin(), nend = cend(); nit != nend;)
   {
      auto v1 = *nit;
      for(const auto& v2 : boost::make_iterator_range(++nit, cend()))
      {
         if(GET_INDEX_CONST_NODE(v1) == GET_INDEX_CONST_NODE(v2))
         {
            PRINT_MSG("[Nuutila::checkWorklist] Duplicated entry in worklist" << std::endl << GET_CONST_NODE(v1)->ToString());
            consistent = false;
         }
      }
   }
   return consistent;
}

bool Nuutila::checkComponents() const
{
   bool isConsistent = true;
   for(auto n1it = cbegin(), n1end = cend(); n1it != n1end;)
   {
      const auto* component1 = components.at(*n1it);
      for(const auto& n2 : boost::make_iterator_range(++n1it, cend()))
      {
         const auto* component2 = components.at(n2);
         if(component1 == component2)
         {
            PRINT_MSG("[Nuutila::checkComponent] Component [" << component1 << ", " << component1->size() << "]");
            isConsistent = false;
         }
      }
   }
   return isConsistent;
}

/**
 * Check if a component has an edge to another component
 */
bool Nuutila::hasEdge(const std::set<VarNode*>* componentFrom, const std::set<VarNode*>* componentTo, const UseMap* useMap) const
{
   for(const auto& v : *componentFrom)
   {
      const auto source = v->getValue();
      THROW_ASSERT(static_cast<bool>(useMap->count(source)), "Variable should be in use map");
      for(const auto& op : useMap->at(source))
      {
         if(static_cast<bool>(componentTo->count(op->getSink())))
            return true;
      }
   }
   return false;
}

bool Nuutila::checkTopologicalSort(const UseMap* useMap) const
{
   bool isConsistent = true;
   for(auto n1it = cbegin(), nend = cend(); n1it != nend; ++n1it)
   {
      const auto* curr_component = components.at(*n1it);
      // check if this component points to another component that has already
      // been visited
      for(const auto& n2 : boost::make_iterator_range(cbegin(), n1it))
      {
         const auto* prev_component = components.at(n2);
         if(hasEdge(curr_component, prev_component, useMap))
         {
            isConsistent = false;
         }
      }
   }
   return isConsistent;
}

#endif

// ========================================================================== //
// Meet
// ========================================================================== //
class Meet
{
   private:
   static APInt getFirstGreaterFromVector(const std::vector<APInt>& constantvector, const APInt& val);
   static APInt getFirstLessFromVector(const std::vector<APInt>& constantvector, const APInt& val);

   public:
   static bool widen(BasicOp* op, const std::vector<APInt>* constantvector);
   static bool narrow(BasicOp* op, const std::vector<APInt>* constantvector);
   static bool crop(BasicOp* op, const std::vector<APInt>* constantvector);
   static bool growth(BasicOp* op, const std::vector<APInt>* constantvector);
   static bool fixed(BasicOp* op);
};

/*
   * Get the first constant from vector greater than val
   */
APInt Meet::getFirstGreaterFromVector(const std::vector<APInt>& constantvector, const APInt& val)
{
   for(const auto& vapint : constantvector)
   {
      if(vapint >= val)
      {
         return vapint;
      }
   }
   return Max;
}

/*
   * Get the first constant from vector less than val
   */
APInt Meet::getFirstLessFromVector(const std::vector<APInt>& constantvector, const APInt& val)
{
   for(auto vit = constantvector.rbegin(), vend = constantvector.rend(); vit != vend; ++vit)
   {
      const auto& vapint = *vit;
      if(vapint <= val)
      {
         return vapint;
      }
   }
   return Min;
}

bool Meet::fixed(BasicOp* op)
{
   const auto oldInterval = op->getSink()->getRange();
   Range newInterval = op->eval();

   op->getSink()->setRange(newInterval);
   #ifdef LOG_TRANSACTIONS
   if(op->getInstruction())
   {
      auto instID = GET_INDEX_CONST_NODE(op->getInstruction());
      PRINT_MSG("FIXED::%" << instID << ": " << oldInterval << " -> " << newInterval);
   }
   else
   {
      PRINT_MSG("FIXED::%artificial phi : " << oldInterval << " -> " << newInterval);
   }
   #endif
   return oldInterval != newInterval;
}

/// This is the meet operator of the growth analysis. The growth analysis
/// will change the bounds of each variable, if necessary. Initially, each
/// variable is bound to either the undefined interval, e.g. [., .], or to
/// a constant interval, e.g., [3, 15]. After this analysis runs, there will
/// be no undefined interval. Each variable will be either bound to a
/// constant interval, or to [-, c], or to [c, +], or to [-, +].
bool Meet::widen(BasicOp* op, const std::vector<APInt>* constantvector)
{
   THROW_ASSERT(constantvector, "Invalid pointer to constant vector");
   const auto oldInterval = op->getSink()->getRange();
   Range newInterval = op->eval();

   unsigned bw = oldInterval.getBitWidth();
   if(oldInterval.isUnknown() || oldInterval.isEmpty() || oldInterval.isAnti() || newInterval.isEmpty() || newInterval.isAnti())
   {
      if(oldInterval.isAnti() && newInterval.isAnti() && newInterval != oldInterval)
      {
         auto oldAnti = oldInterval.getAnti();
         auto newAnti = newInterval.getAnti();
         const APInt oldLower = oldAnti.getLower();
         const APInt oldUpper = oldAnti.getUpper();
         const APInt newLower = newAnti.getLower();
         const APInt newUpper = newAnti.getUpper();
         APInt nlconstant = getFirstGreaterFromVector(*constantvector, newLower);
         APInt nuconstant = getFirstLessFromVector(*constantvector, newUpper);

         if((newLower > oldLower) && (newUpper < oldUpper))
         {
            op->getSink()->setRange(Range(Anti, bw, nlconstant, nuconstant));
         }
         else
         {
            if(newLower > oldLower)
            {
               op->getSink()->setRange(Range(Anti, bw, nlconstant, oldUpper));
            }
            else if(newUpper < oldUpper)
            {
               op->getSink()->setRange(Range(Anti, bw, oldLower, nuconstant));
            }
         }
      }
      else
      {
         op->getSink()->setRange(newInterval);
      }
   }
   else
   {
      const APInt& oldLower = oldInterval.getLower();
      const APInt& oldUpper = oldInterval.getUpper();
      const APInt& newLower = newInterval.getLower();
      const APInt& newUpper = newInterval.getUpper();

      // Jump-set
      APInt nlconstant = getFirstLessFromVector(*constantvector, newLower);
      APInt nuconstant = getFirstGreaterFromVector(*constantvector, newUpper);
      if((newLower < oldLower) && (newUpper > oldUpper))
      {
         op->getSink()->setRange(Range(Regular, bw, nlconstant, nuconstant));
      }
      else
      {
         if(newLower < oldLower)
         {
            op->getSink()->setRange(Range(Regular, bw, nlconstant, oldUpper));
         }
         else if(newUpper > oldUpper)
         {
            op->getSink()->setRange(Range(Regular, bw, oldLower, nuconstant));
         }
      }
   }
   const auto& sinkInterval = op->getSink()->getRange();

   #ifdef LOG_TRANSACTIONS
   if(op->getInstruction())
   {
      auto instID = GET_INDEX_CONST_NODE(op->getInstruction());
      PRINT_MSG("WIDEN::%" << instID << ": " << oldInterval << " -> " << newInterval << " -> " << sinkInterval);
   }
   else
   {
      PRINT_MSG("WIDEN::%artificial phi : " << oldInterval << " -> " << newInterval << " -> " << sinkInterval);
   }
   #endif

   return oldInterval != sinkInterval;
}

bool Meet::growth(BasicOp* op, const std::vector<APInt>* /*constantvector*/)
{
   const auto oldInterval = op->getSink()->getRange();
   Range newInterval = op->eval();
   if(oldInterval.isUnknown() || oldInterval.isEmpty() || oldInterval.isAnti() || newInterval.isEmpty() || newInterval.isAnti())
   {
      op->getSink()->setRange(newInterval);
   }
   else
   {
      unsigned bw = oldInterval.getBitWidth();
      const APInt& oldLower = oldInterval.getLower();
      const APInt& oldUpper = oldInterval.getUpper();
      const APInt& newLower = newInterval.getLower();
      const APInt& newUpper = newInterval.getUpper();
      if(newLower < oldLower)
      {
         if(newUpper > oldUpper)
         {
            op->getSink()->setRange(Range(Regular, bw));
         }
         else
         {
            op->getSink()->setRange(Range(Regular, bw, Min, oldUpper));
         }
      }
      else if(newUpper > oldUpper)
      {
         op->getSink()->setRange(Range(Regular, bw, oldLower, Max));
      }
   }
   const auto& sinkInterval = op->getSink()->getRange();
   #ifdef LOG_TRANSACTIONS
   if(op->getInstruction())
   {
      auto instID = GET_INDEX_CONST_NODE(op->getInstruction());
      PRINT_MSG("GROWTH::%" << instID << ": " << oldInterval << " -> " << sinkInterval);
   }
   else
   {
      PRINT_MSG("GROWTH::%artificial phi : " << oldInterval << " -> " << sinkInterval);
   }
   #endif
   return oldInterval != sinkInterval;
}

/// This is the meet operator of the cropping analysis. Whereas the growth
/// analysis expands the bounds of each variable, regardless of intersections
/// in the constraint graph, the cropping analysis shrinks these bounds back
/// to ranges that respect the intersections.
bool Meet::narrow(BasicOp* op, const std::vector<APInt>* constantvector)
{
   const auto oldInterval = op->getSink()->getRange();
   Range newInterval = op->eval();
   unsigned bw = oldInterval.getBitWidth();

   if(oldInterval.isAnti() || newInterval.isAnti() || oldInterval.isEmpty() || newInterval.isEmpty())
   {
      if(oldInterval.isAnti() && newInterval.isAnti() && newInterval != oldInterval)
      {
         auto oldAnti = oldInterval.getAnti();
         auto newAnti = newInterval.getAnti();
         const APInt& oLower = oldAnti.getLower();
         const APInt& oUpper = oldAnti.getUpper();
         const APInt& nLower = newAnti.getLower();
         const APInt& nUpper = newAnti.getUpper();
         APInt nlconstant = getFirstGreaterFromVector(*constantvector, nLower);
         APInt nuconstant = getFirstLessFromVector(*constantvector, nUpper);
         THROW_ASSERT(oLower != Min, "");
         const APInt& smin = std::max(oLower, nlconstant);
         if(oLower != smin)
         {
            op->getSink()->setRange(Range(Anti, bw, smin, oUpper));
         }
         THROW_ASSERT(oUpper != Max, "");
         const APInt& smax = std::min(oUpper, nuconstant);
         if(oUpper != smax)
         {
            auto sinkRange = op->getSink()->getRange();
            if(sinkRange.isAnti())
            {
               auto sinkAnti = sinkRange.getAnti();
               op->getSink()->setRange(Range(Anti, bw, sinkAnti.getLower(), smax));
            }
            else
            {
               op->getSink()->setRange(Range(Anti, bw, sinkRange.getLower(), smax));
            }
         }
      }
      else
      {
         op->getSink()->setRange(newInterval);
      }
   }
   else
   {
      const APInt oLower = oldInterval.getLower();
      const APInt oUpper = oldInterval.getUpper();
      const APInt nLower = newInterval.getLower();
      const APInt nUpper = newInterval.getUpper();
      if((oLower == Min) && (nLower == Min))
      {
         op->getSink()->setRange(Range(Regular, bw, nLower, oUpper));
      }
      else
      {
         const APInt& smin = std::min(oLower, nLower);
         if(oLower != smin)
         {
            op->getSink()->setRange(Range(Regular, bw, smin, oUpper));
         }
      }
      if(!op->getSink()->getRange().isAnti())
      {
         if((oUpper == Max) && (nUpper == Max))
         {
            op->getSink()->setRange(Range(Regular, bw, op->getSink()->getRange().getLower(), nUpper));
         }
         else
         {
            const APInt& smax = std::max(oUpper, nUpper);
            if(oUpper != smax)
            {
               op->getSink()->setRange(Range(Regular, bw, op->getSink()->getRange().getLower(), smax));
            }
         }
      }
   }
   const auto& sinkInterval = op->getSink()->getRange();
   #ifdef LOG_TRANSACTIONS
   if(op->getInstruction())
   {
      auto instID = GET_INDEX_CONST_NODE(op->getInstruction());
      PRINT_MSG("NARROW::%" << instID << ": " << oldInterval << " -> " << sinkInterval);
   }
   else
   {
      PRINT_MSG("NARROW::%artificial phi : " << oldInterval << " -> " << sinkInterval);
   }
   #endif
   return oldInterval != sinkInterval;
}

bool Meet::crop(BasicOp* op, const std::vector<APInt>* /*constantvector*/)
{
   const auto oldInterval = op->getSink()->getRange();
   Range newInterval = op->eval();

   if(oldInterval.isAnti() || newInterval.isAnti() || oldInterval.isEmpty() || newInterval.isEmpty())
   {
      op->getSink()->setRange(newInterval);
   }
   else
   {
      unsigned bw = oldInterval.getBitWidth();
      char abstractState = op->getSink()->getAbstractState();
      if((abstractState == '-' || abstractState == '?') && (newInterval.getLower() > oldInterval.getLower()))
      {
         op->getSink()->setRange(Range(Regular, bw, newInterval.getLower(), oldInterval.getUpper()));
      }

      if((abstractState == '+' || abstractState == '?') && (newInterval.getUpper() < oldInterval.getUpper()))
      {
         op->getSink()->setRange(Range(Regular, bw, oldInterval.getLower(), newInterval.getUpper()));
      }
   }
   const auto& sinkInterval = op->getSink()->getRange();
   #ifdef LOG_TRANSACTIONS
   if(op->getInstruction())
   {
      auto instID = GET_INDEX_CONST_NODE(op->getInstruction());
      PRINT_MSG("CROP::%" << instID << ": " << oldInterval << " -> " << sinkInterval);
   }
   else
   {
      PRINT_MSG("CROP::%artificial phi : " << oldInterval << " -> " << sinkInterval);
   }
   #endif
   return oldInterval != sinkInterval;
}

/// This class is used to store the intersections that we get in the branches.
/// I decided to write it because I think it is better to have an object
/// to store these information than create a lot of maps
/// in the ConstraintGraph class.
class ValueBranchMap
{
   private:
   const tree_nodeConstRef V;
   const unsigned int BBITrue;
   const unsigned int BBIFalse;
   std::shared_ptr<BasicInterval> ItvT;
   std::shared_ptr<BasicInterval> ItvF;

   public:
   ValueBranchMap(const tree_nodeConstRef _V, const unsigned int _BBITrue, const unsigned int _BBIFalse, std::shared_ptr<BasicInterval> _ItvT, std::shared_ptr<BasicInterval> _ItvF) : V(_V), BBITrue(_BBITrue), BBIFalse(_BBIFalse), ItvT(_ItvT), ItvF(_ItvF)
   {
   }
   ~ValueBranchMap() = default;
   ValueBranchMap(const ValueBranchMap&) = default;
   ValueBranchMap(ValueBranchMap&&) = default;
   ValueBranchMap& operator=(const ValueBranchMap&) = delete;
   ValueBranchMap& operator=(ValueBranchMap&&) = delete;

   /// Get the "false side" of the branch
   const unsigned int getBBIFalse() const
   {
      return BBIFalse;
   }
   /// Get the "true side" of the branch
   const unsigned int getBBITrue() const
   {
      return BBITrue;
   }
   /// Get the interval associated to the true side of the branch
   std::shared_ptr<BasicInterval> getItvT() const
   {
      return ItvT;
   }
   /// Get the interval associated to the false side of the branch
   std::shared_ptr<BasicInterval> getItvF() const
   {
      return ItvF;
   }
   /// Get the value associated to the branch.
   const tree_nodeConstRef getV() const
   {
      return V;
   }
   /// Change the interval associated to the true side of the branch
   void setItvT(std::shared_ptr<BasicInterval> Itv)
   {
      this->ItvT = Itv;
   }
   /// Change the interval associated to the false side of the branch
   void setItvF(std::shared_ptr<BasicInterval> Itv)
   {
      this->ItvF = Itv;
   }
};

/// This is pretty much the same thing as ValueBranchMap
/// but implemented specifically for switch instructions
class ValueSwitchMap
{
   private:
   const tree_nodeConstRef V;
   std::vector<std::pair<std::shared_ptr<BasicInterval>, unsigned int>> BBsuccs;

   public:
   ValueSwitchMap(const tree_nodeConstRef V, std::vector<std::pair<std::shared_ptr<BasicInterval>, unsigned int>>& BBsuccs) : V(V), BBsuccs(BBsuccs)
   {
   }
   ~ValueSwitchMap() = default;
   ValueSwitchMap(const ValueSwitchMap&) = default;
   ValueSwitchMap(ValueSwitchMap&&) = default;
   ValueSwitchMap& operator=(const ValueSwitchMap&) = delete;
   ValueSwitchMap& operator=(ValueSwitchMap&&) = delete;

   /// Get the corresponding basic block
   const unsigned int getBBI(unsigned idx) const
   {
      THROW_ASSERT(idx >= BBsuccs.size(), "Index out of bound");
      return BBsuccs.at(idx).second;
   }
   /// Get the interval associated to the switch case idx
   std::shared_ptr<BasicInterval> getItv(unsigned idx) const
   {
      THROW_ASSERT(idx >= BBsuccs.size(), "Index out of bound");
      return BBsuccs.at(idx).first;
   }
   // Get how many cases this switch has
   unsigned getNumOfCases() const
   {
      return BBsuccs.size();
   }
   /// Get the value associated to the switch.
   const tree_nodeConstRef getV() const
   {
      return V;
   }
   /// Change the interval associated to the true side of the branch
   void setItv(unsigned idx, std::shared_ptr<BasicInterval> Itv)
   {
      THROW_ASSERT(idx >= BBsuccs.size(), "Index out of bound");
      this->BBsuccs.at(idx).first = Itv;
   }
};

// ========================================================================== //
// ConstraintGraph
// ========================================================================== //
// The Operations type.
using GenOprs = std::set<BasicOp*>;

// A map from varnodes to the operation in which this variable is defined
using DefMap = std::map<tree_nodeConstRef, BasicOp*, tree_reindexCompare>;

using ValuesBranchMap = std::map<tree_nodeConstRef, ValueBranchMap, tree_reindexCompare>;

using ValuesSwitchMap = std::map<tree_nodeConstRef, ValueSwitchMap, tree_reindexCompare>;

using CallMap = std::map<unsigned int, std::list<tree_nodeConstRef>>;

using ParmMap = std::map<unsigned int, std::pair<bool, std::vector<tree_nodeConstRef>>>;

class ConstraintGraph
{
   protected:
   // The variables of the source program and the nodes which represent them.
   VarNodes vars;
   // The operations of the source program and the nodes which represent them.
   GenOprs oprs;

   // Perform the widening and narrowing operations
   void update(const UseMap& compUseMap, std::set<tree_nodeConstRef, tree_reindexCompare>& actv, bool (*meet)(BasicOp* op, const std::vector<APInt>* constantvector))
   {
      while(!actv.empty())
      {
         const auto V = *actv.begin();
         actv.erase(V);
         #ifdef DEBUG_CGRAPH
         PRINT_MSG("-> update: " << GET_CONST_NODE(V)->ToString());
         #endif

         // The use list.
         const auto& L = compUseMap.find(V)->second;

         for(BasicOp* op : L)
         {
            #ifdef DEBUG_CGRAPH
            PRINT_MSG("  > " << op->getSink());
            #endif
            if(meet(op, &constantvector))
            {
               // I want to use it as a set, but I also want
               // keep an order or insertions and removals.
               auto val = op->getSink()->getValue();
               actv.insert(val);
            }
         }
      }
   }
   
   void update(unsigned nIterations, const UseMap& compUseMap, std::set<tree_nodeConstRef, tree_reindexCompare>& actv)
   {
      std::list<tree_nodeConstRef> queue(actv.begin(), actv.end());
      actv.clear();
      while(!queue.empty())
      {
         const auto V = queue.front();
         queue.pop_front();
         // The use list.
         const auto& L = compUseMap.find(V)->second;
         for(auto op : L)
         {
            if(nIterations == 0)
            {
               return;
            }
            --nIterations;
            if(Meet::fixed(op))
            {
               auto next = op->getSink()->getValue();
               if(std::find(queue.begin(), queue.end(), next) == queue.end())
               {
                  queue.push_back(next);
               }
            }
         }
      }
   }

   virtual void preUpdate(const UseMap& compUseMap, std::set<tree_nodeConstRef, tree_reindexCompare>& entryPoints) = 0;
   virtual void posUpdate(const UseMap& compUseMap, std::set<tree_nodeConstRef, tree_reindexCompare>& activeVars, const std::set<VarNode*>* component) = 0;

   private:
   int debug_level;
   
   // A map from variables to the operations that define them
   DefMap defMap;
   // A map from variables to the operations where these variables are used.
   UseMap useMap;
   // A map from variables to the operations where these
   // variables are present as bounds
   SymbMap symbMap;
   // This data structure is used to store intervals, basic blocks and intervals
   // obtained in the branches.
   ValuesBranchMap valuesBranchMap;
   ValuesSwitchMap valuesSwitchMap;
   // A map from functions to the operations where they are called
   CallMap callMap;
   // A map from functions to the ssa_name associated with parm_decl (bool value is true when all parameters are associated with a variable)
   ParmMap parmMap;

   // Vector containing the constants from a SCC
   // It is cleared at the beginning of every SCC resolution
   std::vector<APInt> constantvector;

   void buildValueBranchMap(const gimple_cond* br, const blocRef branchBB)
   {
      THROW_ASSERT(GET_CONST_NODE(br->op0)->get_kind() == ssa_name_K, "Non SSA variable found in branch");
      const auto Cond = branchOpRecurse(br->op0);

      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Branch condition is " + Cond->get_kind_text());
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");
      if(const auto* bin_op = GetPointer<const binary_expr>(Cond))
      {
         if(!isCompare(bin_op))
         {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Not a compare codition, skipping...");
            return;
         }

         if(!isIntegerType(bin_op->op0) || !isIntegerType(bin_op->op1))
         {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Non-integer operands, skipping...");
            return;
         }

         // Create VarNodes for comparison operands explicitly
         addVarNode(bin_op->op0);
         addVarNode(bin_op->op1);

         // Gets the successors of the current basic block.
         const auto TrueBBI = branchBB->true_edge;
         const auto FalseBBI = branchBB->false_edge;

         // We have a Variable-Constant comparison.
         const auto Op0 = GET_CONST_NODE(bin_op->op0);
         const auto Op1 = GET_CONST_NODE(bin_op->op1);
         const struct integer_cst* constant = nullptr;
         tree_nodeConstRef variable = nullptr;

         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
            "Op0 is " + Op0->get_kind_text() + " and Op1 is " + Op1->get_kind_text());

         // If both operands are constants, nothing to do here
         if(Op0->get_kind() == integer_cst_K && Op1->get_kind() == integer_cst_K)
         {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Both operands are constants, skipping...");
            return;
         }

         // Then there are two cases: variable being compared to a constant,
         // or variable being compared to another variable

         // Op0 is constant, Op1 is variable
         if((constant = GetPointer<const integer_cst>(Op0)) != nullptr)
         {
            variable = bin_op->op1;
            // Op0 is variable, Op1 is constant
         }
         else if((constant = GetPointer<const integer_cst>(Op1)) != nullptr)
         {
            variable = bin_op->op0;
         }
         // Both are variables
         // which means constant == 0 and variable == 0

         if(constant != nullptr)
         {
            kind pred = bin_op->get_kind();
            kind swappred = op_swap(pred);
            unsigned bw = getGIMPLE_BW(variable);
            Range CR(Regular, bw, constant->value, constant->value);

            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
               "Variable bitwidth is " + boost::lexical_cast<std::string>(bw) + " and constant value is " + boost::lexical_cast<std::string>(constant->value));

            Range tmpT = (variable == bin_op->op0) ? Range::makeSatisfyingCmpRegion(pred, CR) : Range::makeSatisfyingCmpRegion(swappred, CR);
            
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
               "Condition is true on " + tmpT.ToString());

            Range TValues = tmpT.isFullSet() ? Range(Regular, bw) : tmpT;
            Range FValues = tmpT.isFullSet() ? Range(Empty, bw) : Range(TValues.getAnti());

            // Create the interval using the intersection in the branch.
            std::shared_ptr<BasicInterval> BT = std::make_shared<BasicInterval>(TValues);
            std::shared_ptr<BasicInterval> BF = std::make_shared<BasicInterval>(FValues);

            ValueBranchMap VBM(variable, TrueBBI, FalseBBI, BT, BF);
            valuesBranchMap.insert(std::make_pair(variable, VBM));

            // Do the same for the operand of variable (if variable is a cast
            // instruction)
            if(const auto* Var = GetPointer<const ssa_name>(GET_CONST_NODE(variable)))
            {
               const auto* VDef = GetPointer<const gimple_assign>(GET_CONST_NODE(Var->CGetDefStmt()));
               if(VDef && GET_CONST_NODE(VDef->op1)->get_kind() == nop_expr_K)
               {
                  const auto* cast_inst = GetPointer<const nop_expr>(GET_CONST_NODE(VDef->op1));

                  std::shared_ptr<BasicInterval> BT = std::make_shared<BasicInterval>(TValues);
                  std::shared_ptr<BasicInterval> BF = std::make_shared<BasicInterval>(FValues);

                  ValueBranchMap VBM(cast_inst->op, TrueBBI, FalseBBI, BT, BF);
                  valuesBranchMap.insert(std::make_pair(cast_inst->op, VBM));
               }
            }
         }
         else
         {
            kind pred = bin_op->get_kind();
            kind invPred = op_inv(pred);

            unsigned bw0 = getGIMPLE_BW(bin_op->op0);
            unsigned bw1 = getGIMPLE_BW(bin_op->op1);
            THROW_ASSERT(bw0 == bw1, "Operands of same operation have different bitwidth "
               "(Op0 = " + boost::lexical_cast<std::string>(bw0) + ", Op1 = " + boost::lexical_cast<std::string>(bw1) + ").");

            Range CR(Unknown, bw0);
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level,"Variables bitwidth is " + boost::lexical_cast<std::string>(bw0));

            // Symbolic intervals for op0
            std::shared_ptr<BasicInterval> STOp0 = std::shared_ptr<BasicInterval>(new SymbInterval(CR, bin_op->op1, pred));
            std::shared_ptr<BasicInterval> SFOp0 = std::shared_ptr<BasicInterval>(new SymbInterval(CR, bin_op->op1, invPred));

            ValueBranchMap VBMOp0(bin_op->op0, TrueBBI, FalseBBI, STOp0, SFOp0);
            valuesBranchMap.insert(std::make_pair(bin_op->op0, VBMOp0));

            // Symbolic intervals for operand of op0 (if op0 is a cast instruction)
            if(const auto* Var = GetPointer<const ssa_name>(Op0))
            {
               const auto* VDef = GetPointer<const gimple_assign>(GET_CONST_NODE(Var->CGetDefStmt()));
               if(VDef && GET_CONST_NODE(VDef->op1)->get_kind() == nop_expr_K)
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Op0 comes from a cast expression");
                  const auto* cast_inst = GetPointer<const nop_expr>(GET_CONST_NODE(VDef->op1));

                  std::shared_ptr<BasicInterval> STOp0_0 = std::shared_ptr<BasicInterval>(new SymbInterval(CR, bin_op->op1, pred));
                  std::shared_ptr<BasicInterval> SFOp0_0 = std::shared_ptr<BasicInterval>(new SymbInterval(CR, bin_op->op1, invPred));
               
                  ValueBranchMap VBMOp0_0(cast_inst->op, TrueBBI, FalseBBI, STOp0_0, SFOp0_0);
                  valuesBranchMap.insert(std::make_pair(cast_inst->op, VBMOp0_0));
               }
            }

            // Symbolic intervals for op1
            std::shared_ptr<BasicInterval> STOp1 = std::shared_ptr<BasicInterval>(new SymbInterval(CR, bin_op->op0, invPred));
            std::shared_ptr<BasicInterval> SFOp1 = std::shared_ptr<BasicInterval>(new SymbInterval(CR, bin_op->op0, pred));
            ValueBranchMap VBMOp1(bin_op->op1, TrueBBI, FalseBBI, STOp1, SFOp1);
            valuesBranchMap.insert(std::make_pair(bin_op->op1, VBMOp1));

            // Symbolic intervals for operand of op1 (if op1 is a cast instruction)
            if(const auto* Var = GetPointer<const ssa_name>(Op1))
            {
               const auto* VDef = GetPointer<const gimple_assign>(GET_CONST_NODE(Var->CGetDefStmt()));
               if(VDef && GET_CONST_NODE(VDef->op1)->get_kind() == nop_expr_K)
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Op1 comes from a cast expression");
                  const auto* cast_inst = GetPointer<const nop_expr>(GET_CONST_NODE(VDef->op1));

                  std::shared_ptr<BasicInterval> STOp1_1 = std::shared_ptr<BasicInterval>(new SymbInterval(CR, bin_op->op0, pred));
                  std::shared_ptr<BasicInterval> SFOp1_1 = std::shared_ptr<BasicInterval>(new SymbInterval(CR, bin_op->op0, invPred));
               
                  ValueBranchMap VBMOp1_1(cast_inst->op, TrueBBI, FalseBBI, STOp1_1, SFOp1_1);
                  valuesBranchMap.insert(std::make_pair(cast_inst->op, VBMOp1_1));
               }
            }
         }
      }
      else
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Not a compare codition, skipping...");
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
   }

   void buildValueMultiIfMap(const gimple_multi_way_if* mwi, const blocRef /*mwifBB*/, const tree_managerRef TM)
   {
      const auto& cond_list = mwi->list_of_cond;

      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Multi-way if with " + boost::lexical_cast<std::string>(cond_list.size()) + " conditions");
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");

      tree_nodeRef switch_ssa = nullptr;
      unsigned int DefaultBBI = 0;
      size_t i = 0;
      std::vector<tree_nodeRef> CaseTags(mwi->list_of_cond.size());
      for(const auto& [cond, BBI] : mwi->list_of_cond)
      {
         auto& case_tag = CaseTags.at(i++);
         if(cond)
         {
            const auto* case_ssa = GetPointer<const ssa_name>(GET_CONST_NODE(cond));
            THROW_ASSERT(case_ssa, "Case conditional variable should be an ssa_name");
            const auto* ssa_def = GetPointer<const gimple_assign>(GET_CONST_NODE(case_ssa->CGetDefStmt()));
            THROW_ASSERT(ssa_def, "Case statement should be a gimple_assign");
            const auto* eq_op = GetPointer<const eq_expr>(GET_CONST_NODE(ssa_def->op1));
            THROW_ASSERT(eq_op, "Case statement should containt an eq_expr");
            THROW_ASSERT(GET_CONST_NODE(eq_op->op0)->get_kind() == ssa_name_K, "First operand of eq_expr should be the switch variable");
            if(switch_ssa == nullptr)
            {
               switch_ssa = eq_op->op0;
            }
            else
            {
               THROW_ASSERT(switch_ssa->index == eq_op->op0->index, "Switch variable should be unique");
            }
            case_tag = eq_op->op1;
         }
         else
         {
            DefaultBBI = BBI;
            case_tag = nullptr;
         }
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Switch variable is " + GET_CONST_NODE(switch_ssa)->ToString());
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");
      if(!isIntegerType(switch_ssa))
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Variable is non-integer type, skipping...");
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
         return;
      }
      auto bw = getGIMPLE_BW(switch_ssa);
      std::vector<std::pair<std::shared_ptr<BasicInterval>, unsigned int>> BBsuccs;

      // Handle 'default', if there is any
      if(static_cast<bool>(DefaultBBI))
      {
         auto antidefaultRange = Range(Empty, bw);
         for(const auto& case_tag : CaseTags)
         {
            if(case_tag)
            {
               if(const auto* ic = GetPointer<const integer_cst>(GET_CONST_NODE(case_tag)))
               {
                  APInt cval = tree_helper::get_integer_cst_value(ic);
                  antidefaultRange = antidefaultRange.unionWith(Range(Regular, bw, cval, cval));
               }
            }
         }

         APInt sigMin = antidefaultRange.getLower();
         APInt sigMax = antidefaultRange.getUpper();
         Range Values = Range(Anti, bw, sigMin, sigMax);
         // Create the interval using the intersection in the case.
         std::shared_ptr<BasicInterval> BI = std::make_shared<BasicInterval>(Values);
         BBsuccs.push_back(std::make_pair(BI, DefaultBBI));
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "default: " + BI->ToString());
      }

      i = 0;
      for(const auto& [cond, BBI] : cond_list)
      {
         const auto& case_tag = CaseTags.at(i++);
         if(cond != nullptr)
         {
            const auto* ic = GetPointer<const integer_cst>(GET_CONST_NODE(case_tag));
            APInt cval = tree_helper::get_integer_cst_value(ic);
            Range Values = Range(Regular, bw, cval, cval);
            // Create the interval using the intersection in the case.
            std::shared_ptr<BasicInterval> BI = std::make_shared<BasicInterval>(Values);
            BBsuccs.push_back(std::make_pair(BI, BBI));
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "case " + boost::lexical_cast<std::string>(cval) + ": " + BI->ToString());
         }
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");

      ValueSwitchMap VSM(switch_ssa, BBsuccs);
      valuesSwitchMap.insert(std::make_pair(switch_ssa, VSM));

      // Treat when condition of switch is a cast of the real condition (same thing
      // as in buildValueBranchMap)
      if(const auto* Var = GetPointer<const ssa_name>(GET_CONST_NODE(switch_ssa)))
      {
         const auto* VDef = GetPointer<const gimple_assign>(GET_CONST_NODE(Var->CGetDefStmt()));
         if(VDef && GET_CONST_NODE(VDef->op1)->get_kind() == nop_expr_K)
         {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Switch variable comes from a cast expression");
            const auto* cast_inst = GetPointer<const nop_expr>(GET_CONST_NODE(VDef->op1));
            
            ValueSwitchMap VSM_0(cast_inst->op, BBsuccs);
            valuesSwitchMap.insert(std::make_pair(cast_inst->op, VSM_0));
         }
      }

      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
   }

   void buildValueMaps(const std::map<unsigned int, blocRef>& BBs, const tree_managerRef TM)
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Branch conditions analysis...");
      for(const auto& [BBI, BB] : BBs)
      {
         const auto& stmt_list = BB->CGetStmtList();
         if(stmt_list.empty())
         {
            continue;
         }

         const auto terminator = GET_CONST_NODE(stmt_list.back());

         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
            "BB" + boost::lexical_cast<std::string>(BBI) + " has terminator type " + terminator->get_kind_text());
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");
         if(const auto* br = GetPointer<const gimple_cond>(terminator))
         {
            buildValueBranchMap(br, BB);
         }
         else if(const auto* mwi = GetPointer<const gimple_multi_way_if>(terminator))
         {
            buildValueMultiIfMap(mwi, BB, TM);
         }
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Branch conditions analysis completed");
   }

   void addSigmaOp(const tree_nodeConstRef I)
   {
      const auto* phi = GetPointer<const gimple_phi>(GET_CONST_NODE(I));
      THROW_ASSERT(phi, "");
      THROW_ASSERT(phi->CGetDefEdgesList().size() == 1U, "");
      const auto BBI = phi->bb_index;
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Analysing sigma instruction");

      // Create the sink.
      VarNode* sink = addVarNode(phi->res);
      std::shared_ptr<BasicInterval> BItv;
      SigmaOp* sigmaOp = nullptr;

      //const BasicBlock* thisbb = Sigma->getParent();
      for(const auto [operand, edge_index] : phi->CGetDefEdgesList())
      {
         VarNode* source = addVarNode(operand);

         // Create the operation (two cases from: branch or switch)
         auto vbmit = this->valuesBranchMap.find(operand);

         // Branch case
         if(vbmit != this->valuesBranchMap.end())
         {
            const ValueBranchMap& VBM = vbmit->second;
            if(BBI == VBM.getBBITrue())
            {
               BItv = VBM.getItvT();
            }
            else
            {
               if(BBI == VBM.getBBIFalse())
               {
                  BItv = VBM.getItvF();
               }
            }
         }
         else
         {
            // Switch case
            auto vsmit = this->valuesSwitchMap.find(operand);

            if(vsmit == this->valuesSwitchMap.end())
            {
               continue;
            }

            const ValueSwitchMap& VSM = vsmit->second;
            // Find out which case are we dealing with
            for(unsigned idx = 0, e = VSM.getNumOfCases(); idx < e; ++idx)
            {
               const unsigned int bbi = VSM.getBBI(idx);
               if(bbi == BBI)
               {
                  BItv = VSM.getItv(idx);
                  break;
               }
            }
         }

         if(BItv == nullptr)
         {
            std::shared_ptr<BasicInterval> BI = std::make_shared<BasicInterval>(getGIMPLE_range(I));
            sigmaOp = new SigmaOp(BI, sink, I, source, nullptr, phi->get_kind());
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Added SigmaOp with range " + BI->ToString());
         }
         else
         {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Added SigmaOp with range " + BItv->ToString());
            VarNode* SymbSrc = nullptr;
            if(auto symb = std::dynamic_pointer_cast<SymbInterval>(BItv))
            {
               auto bound = symb->getBound();
               SymbSrc = addVarNode(bound);
            }
            sigmaOp = new SigmaOp(BItv, sink, I, source, SymbSrc, phi->get_kind());
            if(SymbSrc)
            {
               this->useMap.at(SymbSrc->getValue()).insert(sigmaOp);
            }
         }

         // Insert the operation in the graph.
         this->oprs.insert(sigmaOp);

         // Insert this definition in defmap
         this->defMap[sink->getValue()] = sigmaOp;

         // Inserts the sources of the operation in the use map list.
         this->useMap.at(source->getValue()).insert(sigmaOp);
      }
   }

   /// Adds an UnaryOp in the graph.
   void addUnaryOp(const tree_nodeConstRef I)
   {
      const auto* assign = GetPointer<const gimple_assign>(GET_CONST_NODE(I));
      const auto* un_op = GetPointer<const unary_expr>(GET_CONST_NODE(assign->op1));
      THROW_ASSERT(un_op, "");
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Analysing unary operation " + un_op->get_kind_text());

      // Create the sink.
      VarNode* sink = addVarNode(assign->op0);
      bool sourceSigned = isSignedType(un_op->op);
      // Create the source.
      VarNode* source = nullptr;

      switch(un_op->get_kind())
      {
         case abs_expr_K:
         case fix_trunc_expr_K:
         case nop_expr_K:
         case view_convert_expr_K:
            source = addVarNode(un_op->op);
            break;

         case convert_expr_K:
         default:
            return;
      }
      std::shared_ptr<BasicInterval> BI = std::make_shared<BasicInterval>(getGIMPLE_range(I));
      UnaryOp* UOp = new UnaryOp(BI, sink, I, source, un_op->get_kind());

      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Added UnaryOp for " + un_op->get_kind_text() + " with range " + BI->ToString());

      this->oprs.insert(UOp);
      // Insert this definition in defmap
      this->defMap[sink->getValue()] = UOp;
      // Inserts the sources of the operation in the use map list.
      this->useMap.find(source->getValue())->second.insert(UOp);
   }

   /// XXX: I'm assuming that we are always analyzing bytecodes in e-SSA form.
   /// So, we don't have intersections associated with binary oprs.
   /// To have an intersect, we must have a Sigma instruction.
   /// Adds a BinaryOp in the graph.
   void addBinaryOp(const tree_nodeConstRef I)
   {
      const auto* assign = GetPointer<const gimple_assign>(GET_CONST_NODE(I));
      const auto* bin_op = GetPointer<const binary_expr>(GET_CONST_NODE(assign->op1));
      THROW_ASSERT(bin_op, "");
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Analysing binary operation " + bin_op->get_kind_text());
      
      // Create the sink.
      VarNode* sink = addVarNode(assign->op0);

      // Create the sources.
      VarNode* source1 = addVarNode(bin_op->op0);
      VarNode* source2 = addVarNode(bin_op->op1);

      // Create the operation using the intersect to constrain sink's interval.
      std::shared_ptr<BasicInterval> BI = std::make_shared<BasicInterval>(getGIMPLE_range(I));
      BinaryOp* BOp = new BinaryOp(BI, sink, I, source1, source2, bin_op->get_kind());

      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Added BinaryOp for " + bin_op->get_kind_text() + " with range " + BI->ToString());

      // Insert the operation in the graph.
      this->oprs.insert(BOp);

      // Insert this definition in defmap
      this->defMap[sink->getValue()] = BOp;

      // Inserts the sources of the operation in the use map list.
      this->useMap.find(source1->getValue())->second.insert(BOp);
      this->useMap.find(source2->getValue())->second.insert(BOp);
   }

   void addTernaryOp(const tree_nodeConstRef I)
   {
      const auto* assign = GetPointer<const gimple_assign>(GET_CONST_NODE(I));
      const auto* ter_op = GetPointer<const ternary_expr>(GET_CONST_NODE(assign->op1));
      THROW_ASSERT(ter_op, "");
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Analysing ternary operation " + ter_op->get_kind_text());
      // Create the sink.
      VarNode* sink = addVarNode(assign->op0);

      // Create the sources.
      VarNode* source1 = addVarNode(ter_op->op0);
      VarNode* source2 = addVarNode(ter_op->op1);
      VarNode* source3 = addVarNode(ter_op->op2);

      // Create the operation using the intersect to constrain sink's interval.
      std::shared_ptr<BasicInterval> BI = std::make_shared<BasicInterval>(getGIMPLE_range(I));
      TernaryOp* TOp = new TernaryOp(BI, sink, I, source1, source2, source3, ter_op->get_kind());

      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Added TernaryOp for " + ter_op->get_kind_text() + " with range " + BI->ToString());

      // Insert the operation in the graph.
      this->oprs.insert(TOp);

      // Insert this definition in defmap
      this->defMap[sink->getValue()] = TOp;

      // Inserts the sources of the operation in the use map list.
      this->useMap.find(source1->getValue())->second.insert(TOp);
      this->useMap.find(source2->getValue())->second.insert(TOp);
      this->useMap.find(source3->getValue())->second.insert(TOp);
   }

   /// Add a phi node (actual phi, does not include sigmas)
   void addPhiOp(const tree_nodeConstRef I)
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Analysing phi instruction");
      const auto* phi = GetPointer<const gimple_phi>(GET_CONST_NODE(I));
      THROW_ASSERT(phi, "");

      // Create the sink.
      VarNode* sink = addVarNode(phi->res);
      std::shared_ptr<BasicInterval> BI = std::make_shared<BasicInterval>(getGIMPLE_range(I));
      PhiOp* phiOp = new PhiOp(BI, sink, I);

      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Added PhiOp with range " + BI->ToString() + " and " + boost::lexical_cast<std::string>(phi->CGetDefEdgesList().size()) + " sources");

      // Insert the operation in the graph.
      this->oprs.insert(phiOp);

      // Insert this definition in defmap
      this->defMap[sink->getValue()] = phiOp;

      // Create the sources.
      for(const auto& [operand, BBI] : phi->CGetDefEdgesList())
      {
         VarNode* source = addVarNode(operand);
         phiOp->addSource(source);
         // Inserts the sources of the operation in the use map list.
         this->useMap.find(source->getValue())->second.insert(phiOp);
      }
   }

   void addLoadOp(const tree_nodeConstRef I, const tree_managerConstRef TM)
   {
      const auto* ga = GetPointer<const gimple_assign>(GET_CONST_NODE(I));
      THROW_ASSERT(ga, "");
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Analysing load operation");
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");
      auto bw = getGIMPLE_BW(ga->op0);
      VarNode* sink = addVarNode(ga->op0);
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Sink variable is " + GET_CONST_NODE(ga->op0)->get_kind_text() + " (size = " + boost::lexical_cast<std::string>(bw) + ")");

      Range intersection(Regular, bw, Min, Max);
      const auto Op1 = GET_CONST_NODE(ga->op1);
      CustomOrderedSet<unsigned int> res_set;
      if(tree_helper::is_fully_resolved(TM, Op1->index, res_set))
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Pointer is fully resolved");
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");
         bool pointToConstants = true;
         Range res(Empty, bw);
         for(const auto& idx : res_set)
         {
            const auto TN = TM->CGetTreeNode(idx);
            if(const auto* vd = GetPointer<const var_decl>(TN))
            {
               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Points to " + TN->ToString() + 
                  " (readonly = " + boost::lexical_cast<std::string>(vd->readonly_flag) + 
                  ", defs = " + boost::lexical_cast<std::string>(vd->defs.size()) + ")");
               if(!vd->readonly_flag)
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Pointed variable is not constant " + TN->ToString());
                  pointToConstants = false;
                  break;
               }
               if(const auto* constr = GetPointer<const constructor>(GET_CONST_NODE(vd->init)))
               {
                  for(const auto& [idx, valu] : constr->list_of_idx_valu)
                  {
                     if(!tree_helper::is_constant(TM, GET_INDEX_CONST_NODE(idx)) || !isIntegerType(idx))
                     {
                        pointToConstants = false;
                        break;
                     }
                     else
                     {
                        res = res.unionWith(getGIMPLE_range(idx));
                     }
                  }
               }
               else
               {
                  THROW_UNREACHABLE("Unhandled initializer " + GET_CONST_NODE(vd->init)->get_kind_text() + " " + GET_CONST_NODE(vd->init)->ToString());
                  pointToConstants = false;
                  break;
               }
            }
            else
            {
               THROW_UNREACHABLE("Unknown tree node " + TN->get_kind_text() + " " + TN->ToString());
               pointToConstants = false;
               break;
            }
         }
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");

         if(pointToConstants)
         {
            intersection = res;
         }
         else
         {
            intersection = getGIMPLE_range(I);
         }
      }
      else
      {
         intersection = getGIMPLE_range(I);
      }
      std::shared_ptr<BasicInterval> BI = std::make_shared<BasicInterval>(intersection);
      LoadOp* loadOp = new LoadOp(BI, sink, I);
      // Insert the operation in the graph.
      this->oprs.insert(loadOp);
      // Insert this definition in defmap
      this->defMap[sink->getValue()] = loadOp;
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Added LoadOp with range " + BI->ToString());

      // LLVM implementation
      /*
   #if HAVE_LIBBDD
      if(arePointersResolved)
      {
         assert(PtoSets_AA);
         auto PO = LI->getPointerOperand();
         if(PtoSets_AA->PE(PO) != NOVAR_ID)
         {
            for(auto var : *PtoSets_AA->pointsToSet(PO))
            {
               auto varValue = PtoSets_AA->getValue(var);
               assert(varValue);
               //               llvm::errs() << "LoadedVar: ";
               //               varValue->print(llvm::errs());
               //               llvm::errs() << "\n";

               for(const Value* operand : ComputeConflictingStores(nullptr, varValue, LI, PtoSets_AA, Function2Store, modulePass))
               {
                  //                  llvm::errs() << "  source: ";
                  //                  operand->print(llvm::errs());
                  //                  llvm::errs() << "\n";
                  VarNode* source = addVarNode(operand, varValue, DL);
                  loadOp->addSource(source);
                  this->useMap.find(source->getValue())->second.insert(loadOp);
               }
            }
         }
      }
   #endif
   */
   }

   void addStoreOp(const tree_nodeConstRef I, const tree_managerConstRef TM)
   {
      const auto* ga = GetPointer<const gimple_assign>(GET_CONST_NODE(I));
      THROW_ASSERT(ga, "");
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Analysing store instruction");
      VarNode* sink = addVarNode(ga->op0);
      unsigned bw = getGIMPLE_BW(ga->op1);
      Range intersection(Regular, bw, Min, Max);
      const auto Op0 = GET_CONST_NODE(ga->op0);
      CustomOrderedSet<unsigned int> res_set;
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");
      if(tree_helper::is_fully_resolved(TM, Op0->index, res_set))
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Pointer is fully resolved");
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");
         bool pointToConstants = true;
         for(const auto& idx : res_set)
         {
            const auto TN = TM->CGetTreeNode(idx);
            if(const auto* vd = GetPointer<const var_decl>(TN))
            {
               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Points to " + TN->ToString() + 
                  " (readonly = " + boost::lexical_cast<std::string>(vd->readonly_flag) + 
                  ", defs = " + boost::lexical_cast<std::string>(vd->defs.size()) + ")");
               if(const auto* constr = GetPointer<const constructor>(GET_CONST_NODE(vd->init)))
               {
                  for(const auto& [idx, valu] : constr->list_of_idx_valu)
                  {
                     StoreOp* storeOp;
                     if(tree_helper::is_constant(TM, GET_INDEX_CONST_NODE(idx)) && isIntegerType(idx))
                     {
                        storeOp = new StoreOp(sink, I, getGIMPLE_range(idx));
                     }
                     else
                     {
                        storeOp = new StoreOp(sink, I, Range(Empty, bw));
                     }
                     this->oprs.insert(storeOp);
                     this->defMap[sink->getValue()] = storeOp;
                     VarNode* source = addVarNode(ga->op1);
                     storeOp->addSource(source);
                     this->useMap.find(source->getValue())->second.insert(storeOp);
                  }
               }
               else
               {
                  THROW_UNREACHABLE("Unhandled initializer " + GET_CONST_NODE(vd->init)->get_kind_text() + " " + GET_CONST_NODE(vd->init)->ToString());
                  continue;
               }
            }
            else
            {
               THROW_UNREACHABLE("Unknown tree node " + TN->get_kind_text() + " " + TN->ToString());
               continue;
            }
         }
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
      
      /*
      // LLVM implementation
   #if HAVE_LIBBDD
      if(arePointersResolved)
      {
         assert(PtoSets_AA);
         auto PO = SI->getPointerOperand();
         assert(PtoSets_AA->PE(PO) != NOVAR_ID);
         auto bw = SI->getValueOperand()->getType()->getPrimitiveSizeInBits();
         Range intersection(Regular, bw, Min, Max);

         for(auto var : *PtoSets_AA->pointsToSet(PO))
         {
            auto varValue = PtoSets_AA->getValue(var);
            assert(varValue);
            VarNode* sink = addVarNode(SI, varValue, DL);
            StoreOp* storeOp;
            if(dyn_cast<llvm::GlobalVariable>(varValue) && dyn_cast<llvm::GlobalVariable>(varValue)->hasInitializer())
            {
               storeOp = new StoreOp(sink, SI, getLLVM_range(dyn_cast<llvm::GlobalVariable>(varValue)->getInitializer()));
            }
            else
            {
               storeOp = new StoreOp(sink, SI, Range(Empty, bw));
            }
            this->oprs.insert(storeOp);
            this->defMap[sink->getValue()] = storeOp;
            VarNode* source = addVarNode(SI->getValueOperand(), nullptr, DL);
            storeOp->addSource(source);
            this->useMap.find(source->getValue())->second.insert(storeOp);

            if(isAggregateValue(varValue))
            {
               //               llvm::errs() << "SI: ";
               //               SI->print(llvm::errs());
               //               llvm::errs() << "\n";
               //               llvm::errs() << "    isAggregateValue: ";
               //               varValue->print(llvm::errs());
               //               llvm::errs() << "\n";
               for(const Value* operand : ComputeConflictingStores(SI, varValue, SI, PtoSets_AA, Function2Store, modulePass))
               {
                  //                  llvm::errs() << "  source: ";
                  //                  operand->print(llvm::errs());
                  //                  llvm::errs() << "\n";
                  VarNode* source = addVarNode(operand, varValue, DL);
                  storeOp->addSource(source);
                  this->useMap.find(source->getValue())->second.insert(storeOp);
               }
            }
         }
      }
   #endif
      */
   }

   void buildOperations(const tree_nodeConstRef I_node, const FunctionBehaviorConstRef FB, const tree_managerConstRef TM)
   {
      const auto* I = GetPointer<const gimple_node>(GET_CONST_NODE(I_node));
      THROW_ASSERT(I, "");
      if(const auto* assign = dynamic_cast<const gimple_assign*>(I))
      {
         const auto Op1 = GET_CONST_NODE(assign->op1);
         if(tree_helper::IsStore(TM, I_node, FB->get_function_mem()))
         {
            return addStoreOp(I_node, TM);
         }
         else if(tree_helper::IsLoad(TM, I_node, FB->get_function_mem()))
         {
            return addLoadOp(I_node, TM);
         }
         else if(const auto* un_op = GetPointer<const unary_expr>(Op1))
         {
            return addUnaryOp(I_node);
         }
         else if(const auto* bin_op = GetPointer<const binary_expr>(Op1))
         {
            return addBinaryOp(I_node);
         }
         else if(const auto* ter_op = GetPointer<const ternary_expr>(Op1))
         {
            return addTernaryOp(I_node);
         }

         THROW_UNREACHABLE("Unhandled assign operation (" + GET_CONST_NODE(assign->op0)->get_kind_text() + " <- " + Op1->get_kind_text() + ")");
      }
      else if(const auto* phi = dynamic_cast<const gimple_phi*>(I))
      {
         if(phi->CGetDefEdgesList().size() == 1)
         {
            return addSigmaOp(I_node);
         }
         else
         {
            return addPhiOp(I_node);
         }
      }
      
      THROW_UNREACHABLE("Unhandled operation (" + I->get_kind_text() + ")");
   }

   /*
      *	This method builds a map that binds each variable label to the
      * operations
      * where this variable is used.
      */
   UseMap buildUseMap(const std::set<VarNode*>& component)
   {
      UseMap compUseMap;
      for(auto vit = component.begin(), vend = component.end(); vit != vend; ++vit)
      {
         const VarNode* var = *vit;
         const auto V = var->getValue();
         // Get the component's use list for V (it does not exist until we try to get it)
         auto& list = compUseMap[V];
         // Get the use list of the variable in component
         auto p = this->useMap.find(V);
         // For each operation in the list, verify if its sink is in the component
         for(BasicOp* opit : p->second)
         {
            VarNode* sink = opit->getSink();
            // If it is, add op to the component's use map
            if(static_cast<bool>(component.count(sink)))
            {
               list.insert(opit);
            }
         }
      }
      return compUseMap;
   }

   /*
      * Used to insert constant in the right position
      */
   void insertConstantIntoVector(APInt constantval)
   {
      constantvector.push_back(constantval);
   }

   /*
      * Create a vector containing all constants related to the component
      * They include:
      *   - Constants inside component
      *   - Constants that are source of an edge to an entry point
      *   - Constants from intersections generated by sigmas
      */
   void buildConstantVector(const std::set<VarNode*>& component, const UseMap& compusemap)
   {
      // Remove all elements from the vector
      constantvector.clear();

      // Get constants inside component (TODO: may not be necessary, since
      // components with more than 1 node may
      // never have a constant inside them)
      for(VarNode* varNode : component)
      {
         const auto V = varNode->getValue();
         if(const auto* ci = GetPointer<const integer_cst>(GET_CONST_NODE(V)))
         {
            insertConstantIntoVector(ci->value);
         }
      }

      // Get constants that are sources of operations whose sink belong to the
      // component
      for(VarNode* varNode : component)
      {
         const auto V = varNode->getValue();
         auto dfit = defMap.find(V);
         if(dfit == defMap.end())
         {
            continue;
         }

         // Handle BinaryOp case
         if(const BinaryOp* bop = dynamic_cast<BinaryOp*>(dfit->second))
         {
            const VarNode* source1 = bop->getSource1();
            const auto sourceval1 = source1->getValue();
            const VarNode* source2 = bop->getSource2();
            const auto sourceval2 = source2->getValue();

            auto opcode = bop->getOpcode();

            if(const auto *const1 = GetPointer<const integer_cst>(GET_CONST_NODE(sourceval1)))
            {
               const auto& cnstVal = const1->value;
               if(isCompare(opcode))
               {
                  auto pred = opcode;
                  if(pred == eq_expr_K || pred == ne_expr_K)
                  {
                     insertConstantIntoVector(cnstVal);
                     insertConstantIntoVector(cnstVal - 1);
                     insertConstantIntoVector(cnstVal + 1);
                  }
                  else if(pred == gt_expr_K || pred == le_expr_K)
                  {
                     insertConstantIntoVector(cnstVal);
                     insertConstantIntoVector(cnstVal + 1);
                  }
                  else if(pred == ge_expr_K || pred == lt_expr_K)
                  {
                     insertConstantIntoVector(cnstVal);
                     insertConstantIntoVector(cnstVal - 1);
                  }
                  else if(pred == ungt_expr_K || pred == unle_expr_K)
                  {
                     auto bw = source1->getBitWidth();
                     auto cnstValU = truncExt(const1->value, bw, false);
                     insertConstantIntoVector(cnstValU);
                     insertConstantIntoVector(cnstValU + 1);
                  }
                  else if(pred == unge_expr_K || pred == unlt_expr_K)
                  {
                     auto bw = source1->getBitWidth();
                     auto cnstValU = truncExt(const1->value, bw, false);
                     insertConstantIntoVector(cnstValU);
                     insertConstantIntoVector(cnstValU - 1);
                  }
                  else
                  {
                     THROW_UNREACHABLE("unexpected condition (" + tree_node::GetString(opcode) + ")");
                  }
               }
               else
               {
                  insertConstantIntoVector(cnstVal);
               }
            }
            if(const auto* const2 = GetPointer<const integer_cst>(GET_CONST_NODE(sourceval2)))
            {
               const auto& cnstVal = const2->value;
               if(isCompare(opcode))
               {
                  auto pred = opcode;
                  if(pred == eq_expr_K || pred == ne_expr_K)
                  {
                     insertConstantIntoVector(cnstVal);
                     insertConstantIntoVector(cnstVal - 1);
                     insertConstantIntoVector(cnstVal + 1);
                  }
                  else if(pred == gt_expr_K || pred == le_expr_K)
                  {
                     insertConstantIntoVector(cnstVal);
                     insertConstantIntoVector(cnstVal + 1);
                  }
                  else if(pred == ge_expr_K || pred == lt_expr_K)
                  {
                     insertConstantIntoVector(cnstVal);
                     insertConstantIntoVector(cnstVal - 1);
                  }
                  else if(pred == ungt_expr_K || pred == unle_expr_K)
                  {
                     auto bw = source2->getBitWidth();
                     auto cnstValU = truncExt(const2->value, bw, false);
                     insertConstantIntoVector(cnstValU);
                     insertConstantIntoVector(cnstValU + 1);
                  }
                  else if(pred == unge_expr_K || pred == unlt_expr_K)
                  {
                     auto bw = source2->getBitWidth();
                     auto cnstValU = truncExt(const2->value, bw, false);
                     insertConstantIntoVector(cnstValU);
                     insertConstantIntoVector(cnstValU - 1);
                  }
                  else
                  {
                     THROW_UNREACHABLE("unexpected condition (" + tree_node::GetString(opcode) + ")");
                  }
               }
               else
               {
                  insertConstantIntoVector(cnstVal);
               }
            }
         }
         // Handle PhiOp case
         else if(const PhiOp* pop = dynamic_cast<PhiOp*>(dfit->second))
         {
            for(unsigned i = 0, e = pop->getNumSources(); i < e; ++i)
            {
               const VarNode* source = pop->getSource(i);
               const auto sourceval = source->getValue();
               if(const auto* consti = GetPointer<const integer_cst>(GET_CONST_NODE(sourceval)))
               {
                  insertConstantIntoVector(consti->value);
               }
            }
         }
      }

      // Get constants used in intersections
      for(const auto& [var, ops] : compusemap)
      {
         for(BasicOp* op : ops)
         {
            const SigmaOp* sigma = dynamic_cast<SigmaOp*>(op);
            // Symbolic intervals are discarded, as they don't have fixed values yet
            if(sigma == nullptr || SymbInterval::classof(sigma->getIntersect().get()))
            {
               continue;
            }
            Range rintersect = op->getIntersect()->getRange();
            if(rintersect.isAnti())
            {
               auto anti = Range(rintersect.getAnti());
               const APInt lb = anti.getLower();
               const APInt ub = anti.getUpper();
               if((lb != Min) && (lb != Max))
               {
                  insertConstantIntoVector(lb - 1);
                  insertConstantIntoVector(lb);
               }
               if((ub != Min) && (ub != Max))
               {
                  insertConstantIntoVector(ub);
                  insertConstantIntoVector(ub + 1);
               }
            }
            else
            {
               const APInt& lb = rintersect.getLower();
               const APInt& ub = rintersect.getUpper();
               if((lb != Min) && (lb != Max))
               {
                  insertConstantIntoVector(lb - 1);
                  insertConstantIntoVector(lb);
               }
               if((ub != Min) && (ub != Max))
               {
                  insertConstantIntoVector(ub);
                  insertConstantIntoVector(ub + 1);
               }
            }
         }
      }

      // Sort vector in ascending order and remove duplicates
      std::sort(constantvector.begin(), constantvector.end(), [](const APInt& i1, const APInt& i2) { return i1 < i2; });

      // std::unique doesn't remove duplicate elements, only
      // move them to the end
      // This is why erase is necessary. To remove these duplicates
      // that will be now at the end.
      auto last = std::unique(constantvector.begin(), constantvector.end());
      constantvector.erase(last, constantvector.end());
   }

   /*
      * This method builds a map of variables to the lists of operations where
      * these variables are used as futures. Its C++ type should be something like
      * map<VarNode, List<Operation>>.
      */
   void buildSymbolicIntersectMap()
   {
      // Creates the symbolic intervals map
      symbMap = SymbMap();

      // Iterate over the operations set
      for(BasicOp* op : oprs)
      {
         // If the operation is unary and its interval is symbolic
         auto* uop = dynamic_cast<UnaryOp*>(op);
         if((uop != nullptr) && SymbInterval::classof(uop->getIntersect().get()))
         {
            auto symbi = std::dynamic_pointer_cast<SymbInterval>(uop->getIntersect());
            const auto V = symbi->getBound();
            auto p = symbMap.find(V);
            if(p != symbMap.end())
            {
               p->second.insert(uop);
            }
            else
            {
               std::set<BasicOp*> l;
               l.insert(uop);
               symbMap.insert(std::make_pair(V, l));
            }
         }
      }
   }

   /*
      * This method evaluates once each operation that uses a variable in
      * component, so that the next SCCs after component will have entry
      * points to kick start the range analysis algorithm.
      */
   void propagateToNextSCC(const std::set<VarNode*>& component)
   {
      for(auto var : component)
      {
         const auto V = var->getValue();
         auto p = this->useMap.at(V);
         for(BasicOp* op : p)
         {
            /// VarNodes belonging to the current SCC must not be evaluated otherwise we break the fixed point previously computed
            if(component.find(op->getSink()) != component.end())
            {
               continue;
            }
            auto* sigmaop = dynamic_cast<SigmaOp*>(op);
            op->getSink()->setRange(op->eval());
            if((sigmaop != nullptr) && sigmaop->getIntersect()->getRange().isUnknown())
            {
               sigmaop->markUnresolved();
            }
         }
      }
   }

   void generateEntryPoints(const std::set<VarNode*>& component, std::set<tree_nodeConstRef, tree_reindexCompare>& entryPoints)
   {
      // Iterate over the varnodes in the component
      for(VarNode* varNode : component)
      {
         const auto V = varNode->getValue();
         if(const auto* ssa = GetPointer<const ssa_name>(GET_CONST_NODE(V)))
         if(const auto* phi_def = GetPointer<const gimple_phi>(GET_CONST_NODE(ssa->CGetDefStmt())))
         if(phi_def->CGetDefEdgesList().size() == 1U)
         {
            auto dit = this->defMap.find(V);
            if(dit != this->defMap.end())
            {
               BasicOp* bop = dit->second;
               auto* defop = dynamic_cast<SigmaOp*>(bop);

               if((defop != nullptr) && defop->isUnresolved())
               {
                  defop->getSink()->setRange(bop->eval());
                  defop->markResolved();
               }
            }
         }
         if(!varNode->getRange().isUnknown())
         {
            entryPoints.insert(V);
         }
      }
   }

   void fixIntersects(const std::set<VarNode*>& component)
   {
      // Iterate again over the varnodes in the component
      for(VarNode* varNode : component)
      {
         fixIntersectsSC(varNode);
      }
   }

   void fixIntersectsSC(VarNode* varNode)
   {
      const auto V = varNode->getValue();
      auto sit = symbMap.find(V);
      if(sit != symbMap.end())
      {
         #ifdef DEBUG_CGRAPH
         PRINT_MSG("fix intesects:" << std::endl << varNode);
         #endif
         for(BasicOp* op : sit->second)
         {
            #ifdef DEBUG_CGRAPH
            PRINT_MSG("op intersects:" << std::endl << op);
            #endif
            op->fixIntersects(varNode);
            #ifdef DEBUG_CGRAPH
            PRINT_MSG("sink:" << op);
            #endif
         }
      }
   }

   void generateActivesVars(const std::set<VarNode*>& component, std::set<tree_nodeConstRef, tree_reindexCompare>& activeVars)
   {
      for(VarNode* varNode : component)
      {
         const auto V = varNode->getValue();
         const auto* CI = GetPointer<const integer_cst>(GET_CONST_NODE(V));
         if(CI != nullptr)
         {
            continue;
         }
         activeVars.insert(V);
      }
   }

   void parametersBinding(const tree_nodeRef stmt, const struct function_decl* FD)
   {
      const auto& args = FD->list_of_args;
      auto parmMapIt = parmMap.find(FD->index);
      if(parmMapIt == parmMap.end())
      {
         parmMapIt = parmMap.insert(std::make_pair(FD->index, std::make_pair(false, std::vector<tree_nodeConstRef>(args.size(), nullptr)))).first;
      }
      auto& [foundAll, parmBind] = parmMapIt->second;
      // Skip ssa uses computation when all parameters have already been associated with a variable
      if(foundAll)
      {
         return;
      }

      const auto ssa_uses = tree_helper::ComputeSsaUses(stmt);
      for(const auto& [ssa, use_counter] : ssa_uses)
      {
         const auto* SSA = GetPointer<const ssa_name>(GET_CONST_NODE(ssa));
         // If ssa_name references a parm_decl and is defined by a gimple_nop, it represents the formal function parameter inside the function body
         if(SSA->var != nullptr && GET_CONST_NODE(SSA->var)->get_kind() == parm_decl_K && GET_CONST_NODE(SSA->CGetDefStmt())->get_kind() == gimple_nop_K)
         {
            auto argIt = std::find_if(args.begin(), args.end(), 
               [&](const tree_nodeRef& arg){ return GET_INDEX_NODE(arg) == GET_INDEX_NODE(SSA->var); });
            THROW_ASSERT(argIt != args.end(), "parm_decl associated with ssa_name not found in function parameters");
            unsigned int arg_pos = argIt - args.begin();
            THROW_ASSERT(arg_pos < args.size(), "Computed parameter position outside actual parameters number");

            parmBind[arg_pos] = ssa;
            foundAll = std::find(parmBind.begin(), parmBind.end(), nullptr) == parmBind.end();
         }
      }
   }

   bool storeFunctionCall(const tree_nodeConstRef tn, const tree_managerConstRef TM)
   {
      tree_nodeRef fun_node = nullptr;

      if(const auto* ga = GetPointer<const gimple_assign>(GET_CONST_NODE(tn)))
      if(const auto* ce = GetPointer<const call_expr>(GET_CONST_NODE(ga->op1)))
      {
         fun_node = ce->fn;
         
      }
      if(const auto* ce = GetPointer<const gimple_call>(GET_CONST_NODE(tn)))
      {
         fun_node = ce->fn;
      }

      if(fun_node)
      {
         if(GET_NODE(fun_node)->get_kind() == addr_expr_K)
         {
            const auto* ue = GetPointer<const unary_expr>(GET_NODE(fun_node));
            fun_node = ue->op;
         }
         else if(GET_NODE(fun_node)->get_kind() == obj_type_ref_K)
         {
            fun_node = tree_helper::find_obj_type_ref_function(fun_node);
         }
         
         const auto* FD = GetPointer<const function_decl>(GET_CONST_NODE(fun_node));
         THROW_ASSERT(FD, "Function call should reference a function_decl node");

         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
            "Analysing function call to " + tree_helper::print_function_name(TM, FD) + "( " + boost::lexical_cast<std::string>(FD->list_of_args.size()) + " argument" + (FD->list_of_args.size() > 1 ? "s)" : ")"));

         auto it = callMap.find(FD->index);
         if(it == callMap.end())
         {
            it = callMap.insert(std::make_pair(FD->index, std::list<tree_nodeConstRef>())).first;
         }
         it->second.emplace_back(tn);
         return true;
      }
      return false;
   }

   public:
   ConstraintGraph(int _debug_level) : debug_level(_debug_level) {}

   virtual ~ConstraintGraph() = default;

   /// Adds a VarNode in the graph.
   VarNode* addVarNode(const tree_nodeConstRef V)
   {
      auto vit = vars.find(V);
      if(vit != vars.end())
      {
         return vit->second;
      }

      auto* node = new VarNode(V);
      vars.insert(std::make_pair(V, node));

      // Inserts the node in the use map list.
      std::set<BasicOp*> useList;
      useMap.insert(std::make_pair(V, useList));
      return node;
   }

   GenOprs* getOprs()
   {
      return &oprs;
   }
   DefMap* getDefMap()
   {
      return &defMap;
   }
   UseMap* getUseMap()
   {
      return &useMap;
   }
   CallMap* getCallMap()
   {
      return &callMap;
   }
   ParmMap* getParmMap()
   {
      return &parmMap;
   }

   /// Iterates through all instructions in the function and builds the graph.
   void buildGraph(unsigned int function_id, const application_managerConstRef AppM)
   {
      const auto TM = AppM->get_tree_manager();
      const auto FB = AppM->CGetFunctionBehavior(function_id);
      const auto* FD = GetPointer<const function_decl>(TM->get_tree_node_const(function_id));
      const auto* SL = GetPointer<const statement_list>(GET_CONST_NODE(FD->body));
      const std::string fn_name = tree_helper::print_function_name(TM, FD) + "(" + boost::lexical_cast<std::string>(FD->list_of_args.size()) + " argument" + (FD->list_of_args.size() > 1 ? "s)" : ")");

      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
         "Analysing function " + fn_name + " with " + boost::lexical_cast<std::string>(SL->list_of_bloc.size()) + " blocks");
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");

      buildValueMaps(SL->list_of_bloc, TM);

      for(const auto& [BBI, BB] : SL->list_of_bloc)
      {
         const auto& stmt_list = BB->CGetStmtList();
         if(stmt_list.size())
         {
            for(const auto& stmt : stmt_list)
            {
               parametersBinding(stmt, FD);

               if(!isValidInstruction(stmt, FB, TM))
               {
                  if(!storeFunctionCall(stmt, TM))
                  {
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
                        "Skipping " + GET_NODE(stmt)->get_kind_text() + " " + GET_NODE(stmt)->ToString());
                  }
                  continue;
               }
               
               buildOperations(stmt, FB, TM);
            }
         }
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Graph built for function " + fn_name);
   }

   void buildVarNodes()
   {
      // Initializes the nodes and the use map structure.
      for(auto& pair : vars)
      {
         pair.second->init(!static_cast<bool>(this->defMap.count(pair.first)));
      }
   }

   void findIntervals(const ParameterConstRef parameters)
   {
      buildSymbolicIntersectMap();
      // List of SCCs
      Nuutila sccList(&vars, &useMap, &symbMap);
      
      for(const auto& n : sccList)
      {
         const auto& component = *sccList.components.at(n);

         if(DEBUG_LEVEL_VERY_PEDANTIC <= debug_level)
         {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Components:");
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");
            for(const auto* var : component)
            {
               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, var->ToString());
            }
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-----------");
         }
         if(component.size() == 1)
         {
            VarNode* var = *component.begin();
            fixIntersectsSC(var);
            auto varDef = this->defMap.find(var->getValue());
            if(varDef != this->defMap.end())
            {
               BasicOp* op = varDef->second;
               var->setRange(op->eval());
            }
            if(var->getRange().isUnknown())
            {
               var->setRange(Range(Regular, var->getBitWidth()));
            }
         }
         else
         {
            UseMap compUseMap = buildUseMap(component);

            // Get the entry points of the SCC
            std::set<tree_nodeConstRef, tree_reindexCompare> entryPoints;

               #ifdef RA_JUMPSET
            // Create vector of constants inside component
            // Comment this line below to deactivate jump-set
            buildConstantVector(component, compUseMap);
            #endif
            if(DEBUG_LEVEL_VERY_PEDANTIC <= debug_level)
            {
               std::stringstream ss;
               for(auto cnst : constantvector)
               {
                  ss << " " << cnst;
               }
               if(!constantvector.empty())
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, ss.str());
               }
            }
            generateEntryPoints(component, entryPoints);
            // iterate a fixed number of time before widening
            update(component.size() * 16, compUseMap, entryPoints);
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Printed constraint graph to " + printToFile("cgfixed.dot", parameters));

            generateEntryPoints(component, entryPoints);
            if(DEBUG_LEVEL_VERY_PEDANTIC <= debug_level)
            {
               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "entryPoints:");
               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");
               for(const auto& el : entryPoints)
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, GET_CONST_NODE(el)->ToString());
               }
               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
            }
            // First iterate till fix point
            preUpdate(compUseMap, entryPoints);
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "fixIntersects");
            fixIntersects(component);
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, " --");
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Printed constraint graph to " + printToFile("cgfixintersect.dot", parameters));

            for(VarNode* varNode : component)
            {
               if(varNode->getRange().isUnknown())
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "initialize unknown: " + GET_CONST_NODE(varNode->getValue())->ToString());
                  //    THROW_UNREACHABLE("unexpected condition");
                  varNode->setRange(Range(Regular, varNode->getBitWidth(), Min, Max));
               }
            }
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Printed constraint graph to " + printToFile("cgint.dot", parameters));

            // Second iterate till fix point
            std::set<tree_nodeConstRef, tree_reindexCompare> activeVars;
            generateActivesVars(component, activeVars);
            posUpdate(compUseMap, activeVars, &component);
         }
         propagateToNextSCC(component);
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Printed final constraint graph to " + printToFile("CG.dot", parameters));
   }

   Range getRange(const tree_nodeConstRef v)
   {
      auto vit = this->vars.find(v);
      if(vit == this->vars.end())
      {
         // If the value doesn't have a range,
         // it wasn't considered by the range analysis
         // for some reason.
         // It gets an unknown range if it's a variable,
         // or the tight range if it's a constant
         //
         // I decided NOT to insert these uncovered
         // values to the node set after their range
         // is created here.
         auto bw = getGIMPLE_BW(v);
         THROW_ASSERT(static_cast<bool>(bw), "Invalid bitwidth");
         const auto* ci = GetPointer<const integer_cst>(GET_CONST_NODE(v));
         if(ci == nullptr)
         {
            return Range(Unknown, bw);
         }
         APInt tmp = ci->value;
         return Range(Regular, bw, tmp, tmp);
      }
      return vit->second->getRange();
   }

   std::string printToFile(const std::string& file_name, const ParameterConstRef parameters) const
   {
      std::string output_directory = parameters->getOption<std::string>(OPT_dot_directory) + "RangeAnalysis/";
      if(!boost::filesystem::exists(output_directory))
      {
         boost::filesystem::create_directories(output_directory);
      }
      const std::string full_name = output_directory + file_name;
      std::ofstream file(full_name);
      print(file);
      return full_name;
   }

   /// Prints the content of the graph in dot format. For more information
   /// about the dot format, see: http://www.graphviz.org/pdf/dotguide.pdf
   void print(std::ostream& OS) const
   {
      const char* quot = R"(")";
      // Print the header of the .dot file.
      OS << "digraph dotgraph {\n";
      OS << R"(label="Constraint Graph for ')";
      OS << "all\' functions\";\n";
      OS << "node [shape=record,fontname=\"Times-Roman\",fontsize=14];\n";

      // Print the body of the .dot file.
      for(const auto& [var, node] : vars)
      {
         if(const auto* C = GetPointer<const integer_cst>(GET_CONST_NODE(var)))
         {
            OS << " " << C->value;
         }
         else
         {
            OS << quot;
            printVarName(var, OS);
            OS << quot;
         }
         OS << R"( [label=")" << node << "\"]\n";
      }

      for(BasicOp* op : oprs)
      {
         op->print(OS);
         OS << '\n';
      }
      OS << pseudoEdgesString.str();
      // Print the footer of the .dot file.
      OS << "}\n";
   }
};

// ========================================================================== //
// Cousot
// ========================================================================== //
class Cousot : public ConstraintGraph
{
 private:
   void preUpdate(const UseMap& compUseMap, std::set<tree_nodeConstRef, tree_reindexCompare>& entryPoints) override
   {
      update(compUseMap, entryPoints, Meet::widen);
   }

   void posUpdate(const UseMap& compUseMap, std::set<tree_nodeConstRef, tree_reindexCompare>& entryPoints, const std::set<VarNode*>* /*component*/) override
   {
      update(compUseMap, entryPoints, Meet::narrow);
   }

 public:
   Cousot(int _debug_level) : ConstraintGraph(_debug_level) {}
};

// ========================================================================== //
// CropDFS
// ========================================================================== //
class CropDFS : public ConstraintGraph
{
 private:
   void preUpdate(const UseMap& compUseMap, std::set<tree_nodeConstRef, tree_reindexCompare>& entryPoints) override
   {
      update(compUseMap, entryPoints, Meet::growth);
   }

   void posUpdate(const UseMap& compUseMap, std::set<tree_nodeConstRef, tree_reindexCompare>& /*activeVars*/, const std::set<VarNode*>* component) override
   {
      storeAbstractStates(*component);
      for(const auto& op : oprs)
      {
         if(static_cast<bool>(component->count(op->getSink())))
         {
            crop(compUseMap, op);
         }
      }
   }

   void storeAbstractStates(const std::set<VarNode*>& component)
   {
      for(auto varNode : component)
      {
         varNode->storeAbstractState();
      }
   }

   void crop(const UseMap& compUseMap, BasicOp* op)
   {
      std::set<BasicOp*> activeOps;
      std::set<const VarNode*> visitedOps;

      // init the activeOps only with the op received
      activeOps.insert(op);

      while(!activeOps.empty())
      {
         BasicOp* V = *activeOps.begin();
         activeOps.erase(V);
         const VarNode* sink = V->getSink();

         // if the sink has been visited go to the next activeOps
         if(static_cast<bool>(visitedOps.count(sink)))
         {
            continue;
         }

         Meet::crop(V, nullptr);
         visitedOps.insert(sink);

         // The use list.of sink
         const auto& L = compUseMap.at(sink->getValue());
         for(BasicOp* op : L)
         {
            activeOps.insert(op);
         }
      }
   }

 public:
   CropDFS(int _debug_level) : ConstraintGraph(_debug_level) {}
};

static void MatchParametersAndReturnValues(unsigned int function_id, const tree_managerConstRef TM, ConstraintGraph* CG, int debug_level)
{
   const auto fd = TM->get_tree_node_const(function_id);
   const auto* FD = GetPointer<const function_decl>(fd);
   const auto* SL = GetPointer<const statement_list>(GET_CONST_NODE(FD->body));
   const std::string fn_name = tree_helper::print_function_name(TM, FD) + "(" + boost::lexical_cast<std::string>(FD->list_of_args.size()) + " argument" + (FD->list_of_args.size() > 1 ? "s)" : ")");
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "MatchParms&RetVal on function " + fn_name);
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");

   // Data structure which contains the matches between formal and real
   // parameters
   // First: formal parameter
   // Second: real parameter
   std::vector<std::pair<tree_nodeConstRef, tree_nodeConstRef>> parameters(FD->list_of_args.size());

   // Fetch the function arguments (formal parameters) into the data structure
   const auto* parmMap = CG->getParmMap();
   const auto funParm = parmMap->find(function_id);
   THROW_ASSERT(funParm != parmMap->end(), "Function parameters binding unavailable");
   const auto& [foundAll, parmBind] = funParm->second;
   THROW_ASSERT(foundAll, "Function parameters bind was not completed");
   THROW_ASSERT(parmBind.size() == parameters.size(), "Parameters count mismatch");
   for(size_t i = 0; i < parameters.size(); ++i)
   {
      parameters[i].first = parmBind[i];
   }

   // Check if the function returns a supported value type. If not, no return
   // value matching is done
   const auto ret_type = tree_helper::GetFunctionReturnType(fd);
   bool noReturn = ret_type->get_kind() == void_type_K;

   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
      "Function has " + (noReturn ? "no return type" : ("return type " + ret_type->get_kind_text())));

   // Creates the data structure which receives the return values of the
   // function, if there is any
   std::set<tree_nodeConstRef, tree_reindexCompare> returnValues;

   if(!noReturn)
   {
      for(const auto& [BBI, BB] : SL->list_of_bloc)
      {
         const auto& stmt_list = BB->CGetStmtList();
            
         if(stmt_list.size())
         if(const auto* gr = GetPointer<const gimple_return>(GET_CONST_NODE(stmt_list.back())))
         {
            returnValues.insert(gr->op);
         }
      }
   }
   if(returnValues.empty() && !noReturn)
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Function should return, but no return statement was found");
      noReturn = true;
   }

   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
      std::string("Function ") + (noReturn ? "has no" : "has explicit") + " return statement" + (returnValues.size() > 1 ? "s" : ""));

   std::vector<PhiOp*> matchers(parameters.size(), nullptr);

   for(size_t i = 0, e = parameters.size(); i < e; ++i)
   {
      VarNode* sink = CG->addVarNode(parameters[i].first);
      sink->setRange(Range(Regular, sink->getBitWidth(), Min, Max));
      matchers[i] = new PhiOp(std::make_shared<BasicInterval>(), sink, nullptr);
      // Insert the operation in the graph.
      CG->getOprs()->insert(matchers[i]);
      // Insert this definition in defmap
      (*CG->getDefMap())[sink->getValue()] = matchers[i];
   }

   std::vector<VarNode*> returnVars;
   for(auto returnValue : returnValues)
   {
      VarNode* from = CG->addVarNode(returnValue);
      returnVars.push_back(from);
   }

   const auto* callMap = CG->getCallMap();
   THROW_ASSERT(static_cast<bool>(callMap->count(function_id)), "Function " + fn_name + " should have some call statement");
   for(const auto& call : callMap->at(function_id))
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Analysing call " + GET_CONST_NODE(call)->ToString());
      const std::vector<tree_nodeRef>* args = nullptr;
      tree_nodeConstRef ret_var = nullptr;
      if(const auto* ga = GetPointer<const gimple_assign>(GET_CONST_NODE(call)))
      {
         THROW_ASSERT(!noReturn, "Function called from gimple_assign should have a return statement");
         const auto* ce = GetPointer<const call_expr>(GET_CONST_NODE(ga->op1));
         args = &ce->args;
         ret_var = ga->op0;
      }
      else if(const auto* gc = GetPointer<const gimple_call>(GET_CONST_NODE(call)))
      {
         THROW_ASSERT(noReturn, "Function called from gimple_call should not have a return statement");
         args = &gc->args;
      }
      else
      {
         THROW_UNREACHABLE("Call statement should be a gimple_assign or a gimple_call");
      }
         
      THROW_ASSERT(args->size() == parameters.size(), "Function parameters and call arguments size mismatch");
      for(size_t i = 0; i < parameters.size(); ++i)
      {
         parameters[i].second = args->at(i);
      }

      // Do the inter-procedural construction of CG
      VarNode* to = nullptr;
      VarNode* from = nullptr;

      // Match formal and real parameters
      for(size_t i = 0; i < parameters.size(); ++i)
      {
         // Add real parameter to the CG
         from = CG->addVarNode(parameters[i].second);

         // Connect nodes
         matchers[i]->addSource(from);

         // Inserts the sources of the operation in the use map list.
         CG->getUseMap()->find(from->getValue())->second.insert(matchers[i]);
      }

      // Match return values
      if(!noReturn)
      {
         // Add caller instruction to the CG (it receives the return value)
         to = CG->addVarNode(ret_var);
         to->setRange(Range(Regular, to->getBitWidth(), Min, Max));

         PhiOp* phiOp = new PhiOp(std::make_shared<BasicInterval>(), to, nullptr);
         // Insert the operation in the graph.
         CG->getOprs()->insert(phiOp);
         // Insert this definition in defmap
         CG->getDefMap()->operator[](to->getValue()) = phiOp;

         for(VarNode* var : returnVars)
         {
            phiOp->addSource(var);
            // Inserts the sources of the operation in the use map list.
            CG->getUseMap()->find(var->getValue())->second.insert(phiOp);
         }
      }

      // Real parameters are cleaned before moving to the next use (for safety's
      // sake)
      for(auto& pair : parameters)
      {
         pair.second = nullptr;
      }
   }
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
}

// ========================================================================== //
// RangeAnalysis
// ========================================================================== //
RangeAnalysis::RangeAnalysis(const application_managerRef AM, const DesignFlowManagerConstRef dfm, const ParameterConstRef par)
   : ApplicationFrontendFlowStep(AM, RANGE_ANALYSIS, dfm, par)
{
   debug_level = parameters->get_class_debug_level(GET_CLASS(*this), DEBUG_LEVEL_NONE);
}

RangeAnalysis::~RangeAnalysis() = default;

const CustomUnorderedSet<std::pair<FrontendFlowStepType, FrontendFlowStep::FunctionRelationship>> 
RangeAnalysis::ComputeFrontendRelationships(const DesignFlowStep::RelationshipType relationship_type) const
{
   CustomUnorderedSet<std::pair<FrontendFlowStepType, FunctionRelationship>> relationships;
   switch(relationship_type)
   {
      case DEPENDENCE_RELATIONSHIP:
      {
         relationships.insert(std::make_pair(ESSA, ALL_FUNCTIONS));
         break;
      }
      case PRECEDENCE_RELATIONSHIP:
      {
         break;
      }
      case INVALIDATION_RELATIONSHIP:
      {
         break;
      }
      default:
      {
         THROW_UNREACHABLE("");
      }
   }
   return relationships;
}

bool RangeAnalysis::HasToBeExecuted() const
{
   return true;
}

DesignFlowStep_Status RangeAnalysis::Exec()
{
   PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Range Analysis step");

   const auto TM = AppM->get_tree_manager();
   /// Build Constraint graph on every function
   ConstraintGraph* CG = new Cousot(debug_level);

   MAX_BIT_INT = getMaxBitWidth();
   updateConstantIntegers(MAX_BIT_INT);

   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
      "-->Maximum bitwidth is " + boost::lexical_cast<std::string>(MAX_BIT_INT) + " bits");

   // Analyse only reached functions
   auto functions = AppM->CGetCallGraphManager()->GetReachedBodyFunctions();
   for(unsigned int f : functions)
   {
      CG->buildGraph(f, AppM);
   }
   // Top functions are not called by any other functions, so they do not have any call statement to analyse
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "MatchParms&RetVal analysis...");
   for(const auto top_fn : AppM->CGetCallGraphManager()->GetRootFunctions())
   {
      const auto* FD = GetPointer<const function_decl>(TM->get_tree_node_const(top_fn));
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, 
         tree_helper::print_function_name(TM, FD) + "(" + boost::lexical_cast<std::string>(FD->list_of_args.size()) + " arguments) is top function");
      functions.erase(top_fn);
   }
   // The two operations are split because the CallMap is built for all functions in buildGrap
   // then it is used from MatchParametersAndReturnValues
   for(unsigned int f : functions)
   {
      MatchParametersAndReturnValues(f, TM, CG, debug_level);
   }
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "MatchParms&RetVal analysis completed");
   CG->buildVarNodes();

   CG->findIntervals(parameters);
      
   finalizeRangeAnalysis(CG);
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");

   delete CG;
   return DesignFlowStep_Status::SUCCESS;
}

void RangeAnalysis::Initialize()
{
}

Range RangeAnalysis::getRange(const tree_nodeConstRef ssa_name) const
{
   THROW_ASSERT(ssa_name->get_kind() == tree_reindex_K, "Tree node should be a tree_reindex");
   auto it = ranges.find(ssa_name);
   if(it != ranges.end())
   {
      return it->second;
   }

   return Range(Unknown, getGIMPLE_BW(ssa_name));
}

unsigned RangeAnalysis::getMaxBitWidth(unsigned int F)
{
   unsigned int InstBitSize = 0, opBitSize = 0, max = 0;

   const auto TM = AppM->get_tree_manager();
   const auto* FD = GetPointer<const function_decl>(TM->get_tree_node_const(F));
   const auto* SL = GetPointer<const statement_list>(GET_CONST_NODE(FD->body));

   for(const auto& [bb_id, bb] : SL->list_of_bloc)
   {
      const auto& stmt_list = bb->CGetStmtList();
      if(stmt_list.size())
      {
         for(const auto& stmt : stmt_list)
         {
            if(GET_NODE(stmt)->get_kind() != gimple_assign_K && GET_NODE(stmt)->get_kind() != gimple_phi_K && GET_NODE(stmt)->get_kind() != gimple_return_K)
            {
               continue;
            }
            InstBitSize = getGIMPLE_BW(stmt);
            if(InstBitSize > max)
            {
               max = InstBitSize;
            }
         }
      }
   }
   // Bit-width equal to 0 is not valid, so we increment to 1
   if(max == 0)
   {
      ++max;
   }
   return max;
}

unsigned RangeAnalysis::getMaxBitWidth()
{
   unsigned max = 0;

   const auto functions = AppM->get_functions_with_body();
   for(unsigned int f : functions)
   {
      unsigned bitwidth = getMaxBitWidth(f);
      if(bitwidth > max)
      {
         max = bitwidth;
      }
   }

   return max ? max : 1;
}

void RangeAnalysis::updateConstantIntegers(unsigned maxBitWidth)
{
   // Updates the Min and Max values.
   Min = getSignedMinValue(maxBitWidth);
   Max = getSignedMaxValue(maxBitWidth);
}

void RangeAnalysis::finalizeRangeAnalysis(void* CGp)
{
   ConstraintGraph* CG = reinterpret_cast<ConstraintGraph*>(CGp);
   
   const auto TM = AppM->get_tree_manager();
   const auto functions = AppM->get_functions_with_body();
   for(unsigned int f : functions)
   {
      const auto* FD = GetPointer<const function_decl>(TM->get_tree_node_const(f));
      const auto* SL = GetPointer<const statement_list>(GET_CONST_NODE(FD->body));
   
      for(const auto& [BBI, BB] : SL->list_of_bloc)
      {
         const auto& stmt_list = BB->CGetStmtList();
         for(auto& stmt : stmt_list)
         {
            // TODO: use tree_helper::ComputeSSAUses(stmt) to get all ssa_name inside the statement
            if(const auto* ga = GetPointer<const gimple_assign>(GET_CONST_NODE(stmt)))
            {
               if(GET_CONST_NODE(ga->op0)->get_kind() == ssa_name_K)
               {
                  ranges.insert(std::make_pair(ga->op0, CG->getRange(ga->op0)));
               }
            }
         }
      }
   }

   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Range results for " + boost::lexical_cast<std::string>(ranges.size()) + " variables");
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->");
   for(auto [ssa_tr, range] : ranges)
   {
      auto* ssa = GetPointer<ssa_name>(GET_NODE(TM->GetTreeReindex(ssa_tr->index)));
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, ssa->ToString() + " " + GET_NODE(ssa->type)->get_kind_text() + " " + range.ToString());
      if(!range.isUnknown() && !range.isEmpty())
      {
         bool isSigned = tree_helper::is_int(TM, ssa->index);
         auto type_id = GET_INDEX_CONST_NODE(ssa->type);
         if(isSigned)
         {
            ssa->min = TM->CreateUniqueIntegerCst(range.getSignedMin().convert_to<long long>(), type_id);
            ssa->max = TM->CreateUniqueIntegerCst(range.getSignedMax().convert_to<long long>(), type_id);
         }
         else
         {
            ssa->min = TM->CreateUniqueIntegerCst(range.getUnsignedMin().convert_to<long long>(), type_id);
            ssa->max = TM->CreateUniqueIntegerCst(range.getUnsignedMax().convert_to<long long>(), type_id);
         }
      }
   }
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
}