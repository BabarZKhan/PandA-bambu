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
 * @file CSE.cpp
 * @brief common subexpression elimination step
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
*/

///Header include
#include "CSE.hpp"

///. include
#include "Parameter.hpp"

///boost include
#include <boost/graph/topological_sort.hpp>

///behavior includes
#include "application_manager.hpp"
#include "function_behavior.hpp"

///algorithm/dominance include
#include "Dominance.hpp"

///design_flows include
#include "design_flow_manager.hpp"

///frontend_analysis
#include "application_frontend_flow_step.hpp"

///HLS include
#include "hls_manager.hpp"
#if HAVE_BAMBU_BUILT && HAVE_ILP_BUILT
///HLS includes
#include "hls.hpp"

///HLS/scheduling include
#include "schedule.hpp"
#endif

///HLS/memory include
#include "memory.hpp"

///STD include
#include <fstream>
#include <math.h>
#include <string>

///tree include
#include "behavioral_helper.hpp"
#include "tree_basic_block.hpp"
#include "tree_helper.hpp"
#include "tree_manager.hpp"
#include "tree_reindex.hpp"

#include "basic_block.hpp"

///wrapper/treegcc include
#include "gcc_wrapper.hpp"


CSE::CSE (const ParameterConstRef _parameters, const application_managerRef _AppM, unsigned int _function_id, const DesignFlowManagerConstRef _design_flow_manager) :
   FunctionFrontendFlowStep (_AppM, _function_id, CSE_STEP, _design_flow_manager, _parameters),
   TM(_AppM->get_tree_manager()),
   sl(nullptr),
   restart_phi_opt(false)
{
   debug_level = parameters->get_class_debug_level (GET_CLASS(*this),
                                                    DEBUG_LEVEL_NONE);
}

CSE::~CSE ()
{
}

const std::unordered_set<std::pair<FrontendFlowStepType, FrontendFlowStep::FunctionRelationship> > CSE::ComputeFrontendRelationships (const DesignFlowStep::RelationshipType relationship_type) const
{
   std::unordered_set<std::pair<FrontendFlowStepType, FunctionRelationship> > relationships;
   switch (relationship_type)
   {
      case (PRECEDENCE_RELATIONSHIP):
      {
         relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(DEAD_CODE_ELIMINATION, SAME_FUNCTION));
         relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(UN_COMPARISON_LOWERING, SAME_FUNCTION));
         relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(IR_LOWERING, SAME_FUNCTION));
         relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(COMPLETE_BB_GRAPH, SAME_FUNCTION));
         relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(BIT_VALUE_OPT, SAME_FUNCTION));
         break;
      }
      case DEPENDENCE_RELATIONSHIP:
      {
         relationships.insert (std::pair<FrontendFlowStepType, FunctionRelationship> (USE_COUNTING, SAME_FUNCTION));
         break;
      }
      case(INVALIDATION_RELATIONSHIP) :
      {
         switch(GetStatus())
         {
            case DesignFlowStep_Status::SUCCESS:
               {
                  if(restart_phi_opt)
                     relationships.insert(std::make_pair(PHI_OPT, SAME_FUNCTION));
                  break;
               }
            case DesignFlowStep_Status::SKIPPED:
            case DesignFlowStep_Status::UNCHANGED:
            case DesignFlowStep_Status::UNEXECUTED:
            case DesignFlowStep_Status::UNNECESSARY:
               {
                  break;
               }
            case DesignFlowStep_Status::ABORTED:
            case DesignFlowStep_Status::EMPTY:
            case DesignFlowStep_Status::NONEXISTENT:
            default:
               THROW_UNREACHABLE("");
         }
         break;
      }
      default:
         THROW_UNREACHABLE("");
   }
   return relationships;
}

bool CSE::check_loads(const gimple_assign* ga, unsigned int right_part_index, tree_nodeRef right_part)
{
    const std::set<unsigned int>& fun_mem_data = function_behavior->get_function_mem();
    tree_nodeRef op0_type = tree_helper::get_type_node(GET_NODE(ga->op0));
    tree_nodeRef op1_type = tree_helper::get_type_node(right_part);
    bool is_a_vector_bitfield = false;
    /// check for bit field ref of vector type
    if(right_part->get_kind() == bit_field_ref_K)
    {
       bit_field_ref* bfr = GetPointer<bit_field_ref>(right_part);
       if(tree_helper::is_a_vector(TM, GET_INDEX_NODE(bfr->op0)))
       is_a_vector_bitfield = true;
    }

    bool skip_check = right_part->get_kind() == var_decl_K || right_part->get_kind() == string_cst_K || right_part->get_kind() == constructor_K ||
            (right_part->get_kind() == bit_field_ref_K && !is_a_vector_bitfield) || right_part->get_kind() == component_ref_K ||
          right_part->get_kind() == indirect_ref_K || right_part->get_kind() == misaligned_indirect_ref_K || right_part->get_kind() == array_ref_K ||
          right_part->get_kind() == target_mem_ref_K || right_part->get_kind() == target_mem_ref461_K or
          right_part->get_kind() == call_expr_K  or right_part->get_kind() == aggr_init_expr_K;
    if(right_part->get_kind() == realpart_expr_K || right_part->get_kind() == imagpart_expr_K)
    {
       enum kind code1 = GET_NODE(GetPointer<unary_expr>(right_part)->op)->get_kind();
       if((code1 == bit_field_ref_K && !is_a_vector_bitfield) ||
          code1 == component_ref_K || code1 == indirect_ref_K || code1 == bit_field_ref_K ||
          code1 == misaligned_indirect_ref_K || code1 == mem_ref_K || code1 == array_ref_K ||
          code1 == target_mem_ref_K || code1 == target_mem_ref461_K)
           skip_check = true;
       if(code1 == var_decl_K && fun_mem_data.find(GET_INDEX_NODE(GetPointer<unary_expr>(right_part)->op)) != fun_mem_data.end())
           skip_check = true;
    }
    if(right_part->get_kind() == view_convert_expr_K)
    {
       view_convert_expr* vc = GetPointer<view_convert_expr>(right_part);
       tree_nodeRef vc_op_type = tree_helper::get_type_node(GET_NODE(vc->op));
       if(op0_type->get_kind() == record_type_K || op0_type->get_kind() == union_type_K)
          skip_check = true;
       if(vc_op_type->get_kind() == record_type_K || vc_op_type->get_kind() == union_type_K)
          skip_check = true;

       if(vc_op_type->get_kind() == array_type_K && op0_type->get_kind() == vector_type_K)
          skip_check = true;
       if(vc_op_type->get_kind() == vector_type_K && op0_type->get_kind() == array_type_K)
          skip_check = true;
    }
    if(not tree_helper::is_a_vector(TM, GET_INDEX_NODE(ga->op0)) and tree_helper::is_an_array(TM, GET_INDEX_NODE(ga->op0)) and not tree_helper::is_a_pointer(TM, GET_INDEX_NODE(ga->op0)))
       skip_check = true;
    if(fun_mem_data.find(GET_INDEX_NODE(ga->op0)) != fun_mem_data.end() || fun_mem_data.find(right_part_index) != fun_mem_data.end())
       skip_check = true;
    if(op0_type && op1_type &&
                    ((op0_type->get_kind()== record_type_K && op1_type->get_kind()== record_type_K && right_part->get_kind() != view_convert_expr_K) ||
                    (op0_type->get_kind()== union_type_K && op1_type->get_kind()== union_type_K && right_part->get_kind() != view_convert_expr_K) ||
                    (op0_type->get_kind() == array_type_K)
                    )
                   )
       skip_check = true;

    return skip_check;
}

tree_nodeRef CSE::hash_check(tree_nodeRef tn, vertex bb)
{
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Checking: "+ tn->ToString());
   const gimple_assign * ga = GetPointer<gimple_assign>(tn);
   if(ga && (ga->clobber || ga->init_assignment))
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Checked: null");
      return tree_nodeRef();
   }
   if(ga && GET_NODE(ga->op0)->get_kind () == ssa_name_K)
   {
      unsigned int bitwidth_values = tree_helper::Size(ga->op0);
      if(GetPointer<binary_expr>(GET_NODE(ga->op1)))
         bitwidth_values = std::max(bitwidth_values, tree_helper::Size(GetPointer<binary_expr>(GET_NODE(ga->op1))->op0));
      if(parameters->IsParameter("CSE_size") and bitwidth_values < parameters->GetParameter<unsigned int>("CSE_size"))
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Checked: too small");
         return tree_nodeRef();
      }
      tree_nodeRef right_part = GET_NODE(ga->op1);
      unsigned int right_part_index = GET_INDEX_NODE(ga->op1);
      enum kind op_kind = right_part->get_kind ();
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Right part type: "+ right_part->get_kind_text());

      /// check for LOADs, STOREs, MEMSET, MEMCPY, etc etc
      bool skip_check = check_loads(ga, right_part_index, right_part);
      std::vector<unsigned int> ins;
      if(skip_check)
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Checked loads: null");
         return tree_nodeRef();
      }
      ///We add type of right part; load from same address with different types must be considered different
      ins.push_back(tree_helper::CGetType(GET_CONST_NODE(ga->op1))->index);

      THROW_ASSERT(not ga->memdef and not ga->vdef and ga->vovers.empty(), STR(tn));
      if(ga->vuses.size())
      {
         ///If there are virtual uses, not only they must be the same, but also the basic block must be the same
         ins.push_back(ga->bb_index);
         for(const auto vuse : ga->vuses)
         {
            ///Check if the virtual is defined in the same basic block
            const auto virtual_sn = GetPointer<const ssa_name>(GET_CONST_NODE(vuse));
            const auto virtual_sn_def = virtual_sn->CGetDefStmt();
            const auto virtual_sn_gn = GetPointer<const gimple_node>(GET_CONST_NODE(virtual_sn_def));
            ins.push_back(vuse->index);
            if(virtual_sn_gn->bb_index == ga->bb_index)
            {
               for(const auto stmt : sl->list_of_bloc[ga->bb_index]->CGetStmtList())
               {
                  const auto gn = GetPointer<const gimple_node>(GET_CONST_NODE(stmt));
                  if(gn->index == ga->index)
                     break;
                  ///For each vuse we insert def_stmt if it is before
                  if(gn->vdef and gn->vdef->index == vuse->index)
                  {
                     ins.push_back(gn->index);
                     break;
                  }
               }
            }
         }
      }
      if(op_kind == ssa_name_K)
      {
          ssa_name* ssa_var=GetPointer<ssa_name>(right_part);
          const auto def_stmt = GET_NODE(ssa_var->CGetDefStmt());
          const auto def_gimple = GetPointer<gimple_node>(def_stmt);
          if(def_gimple->bb_index == ga->bb_index && GetPointer<gimple_assign>(def_stmt))
          {
             INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--return the associated def assignment");
             return def_stmt;
          }
          else
             ins.push_back(right_part_index);
      }
      else if(GetPointer<cst_node>(right_part))
      {
         ins.push_back(right_part_index);
      }
      else if(op_kind == nop_expr_K || op_kind == view_convert_expr_K || op_kind == convert_expr_K ||
              op_kind == float_expr_K || op_kind == fix_trunc_expr_K)
      {
          unary_expr* ue=GetPointer<unary_expr>(right_part);
          ins.push_back(GET_INDEX_NODE(ue->op));
          unsigned int type_index;
          tree_helper::get_type_node(GET_NODE(ga->op0), type_index);
          ins.push_back(type_index);
      }
      else if(GetPointer<unary_expr>(right_part))
      {
         unary_expr* ue=GetPointer<unary_expr>(right_part);
         ins.push_back(GET_INDEX_NODE(ue->op));
      }
      else if(GetPointer<binary_expr>(right_part))
      {
         binary_expr* be   =GetPointer<binary_expr>(right_part);
         ins.push_back(GET_INDEX_NODE(be->op0));
         ins.push_back(GET_INDEX_NODE(be->op1));
      }
      else if(GetPointer<ternary_expr>(right_part))
      {
         ternary_expr* te=GetPointer<ternary_expr>(right_part);
         ins.push_back(GET_INDEX_NODE(te->op0));
         ins.push_back(GET_INDEX_NODE(te->op1));
         if(te->op2)
            ins.push_back(GET_INDEX_NODE(te->op2));
      }
      else
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Checked: null");
         return tree_nodeRef();
      }
#ifndef NDEBUG
      std::string signature_message = "Signature of " + STR(tn->index) + " is ";
      for(const auto temp_sign : ins)
      {
         signature_message += temp_sign + "-";
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, signature_message);
#endif
      CSE_tuple_key_type t(op_kind,ins);
      if(unique_table.find(bb)->second.find(t) != unique_table.find(bb)->second.end())
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "--- statement = " + tn->ToString());
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "--- equivalent with = " + unique_table.find(bb)->second.find(t)->second->ToString());
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
         return unique_table.find(bb)->second.find(t)->second;
      }
      else
         unique_table.find(bb)->second[t] = tn;
   }
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Checked: null");
   return tree_nodeRef();
}

DesignFlowStep_Status
CSE::InternalExec ()
{
   bool IR_changed = false;
   restart_phi_opt = false;
   size_t n_equiv_stmt = 0;

   tree_nodeRef temp = TM->get_tree_node_const(function_id);
   function_decl * fd = GetPointer<function_decl>(temp);
   sl = GetPointer<statement_list>(GET_NODE(fd->body));

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
   for(auto curr_bb_pair : sl->list_of_bloc )
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

   refcount<dominance<BBGraph> > bb_dominators;
   bb_dominators = refcount<dominance<BBGraph> >(new dominance<BBGraph>(*GCC_bb_graph, inverse_vertex_map[bloc::ENTRY_BLOCK_ID], inverse_vertex_map[bloc::EXIT_BLOCK_ID], parameters));
   bb_dominators->calculate_dominance_info(dominance<BBGraph>::CDI_DOMINATORS);
   const std::unordered_map<vertex, vertex>& bb_dominator_map = bb_dominators->get_dominator_map();

   BBGraphRef bb_domGraph(new BBGraph(GCC_bb_graphs_collection, D_SELECTOR));
   for(std::unordered_map<vertex, vertex>::const_iterator it = bb_dominator_map.begin(); it != bb_dominator_map.end(); ++it)
   {
      if(it->first != inverse_vertex_map[bloc::ENTRY_BLOCK_ID])
      {
         GCC_bb_graphs_collection->AddEdge(it->second, it->first, D_SELECTOR);
      }
   }

   std::deque<vertex> sort_list;
   boost::topological_sort(*bb_domGraph, std::front_inserter(sort_list));

   for (auto bb : sort_list)
   {
      blocRef B = bb_domGraph->CGetBBNodeInfo(bb)->block;
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Considering BB "+ STR(B->number));
      /// CSE on basic blocks
      unique_table[bb].clear();
      if(bb_dominator_map.find(bb) != bb_dominator_map.end())
      {
         THROW_ASSERT(unique_table.find(bb_dominator_map.find(bb)->second) != unique_table.end(), "unexpected condition");
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Adding dominator equiv: "+ STR(bb_domGraph->CGetBBNodeInfo(bb_dominator_map.find(bb)->second)->block->number));

         for(auto key_value_pair: unique_table.find(bb_dominator_map.find(bb)->second)->second)
            unique_table.find(bb)->second[key_value_pair.first] = key_value_pair.second;
      }
      TreeNodeSet to_be_removed;
      for(const auto stmt : B->CGetStmtList())
      {
#ifndef NDEBUG
         if(not AppM->ApplyNewTransformation())
         {
            break;
         }
#endif
         tree_nodeRef eq_tn = hash_check(GET_NODE(stmt), bb);
         if(eq_tn)
         {
            gimple_assign * ref_ga = GetPointer<gimple_assign>(eq_tn);
            const gimple_assign * dead_ga = GetPointer<gimple_assign>(GET_NODE(stmt));
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Replacing use of " + STR(dead_ga->op0));
            ref_ga->temporary_address = ref_ga->temporary_address && dead_ga->temporary_address;
            //THROW_ASSERT(ref_ga->bb_index==dead_ga->bb_index, "unexpected condition");
            //THROW_ASSERT(ref_ga->bb_index==B->number, "unexpected condition");
            ssa_name * ref_ssa = GetPointer<ssa_name>(GET_NODE(ref_ga->op0));
            ssa_name * dead_ssa = GetPointer<ssa_name>(GET_NODE(dead_ga->op0));
            ///update bit values with the longest
            if(dead_ssa->bit_values.size()>ref_ssa->bit_values.size())
               ref_ssa->bit_values = dead_ssa->bit_values;
            if(dead_ssa->use_set && !ref_ssa->use_set)
               ref_ssa->use_set = dead_ssa->use_set;
            if(dead_ssa->bit_values.size() > ref_ssa->bit_values.size())
               ref_ssa->bit_values = dead_ssa->bit_values;
            if(tree_helper::CGetType(GET_CONST_NODE(ref_ga->op0))->get_kind() == integer_type_K)
            {
               const auto dead_min = dead_ssa->min;
               const auto ref_min = ref_ssa->min;
               if(ref_min)
               {
                  if(dead_min)
                  {
                     const auto dead_min_ic = GetPointer<const integer_cst>(GET_CONST_NODE(dead_min));
                     const auto ref_min_ic = GetPointer<const integer_cst>(GET_CONST_NODE(ref_min));
                     if(tree_helper::get_integer_cst_value(dead_min_ic) < tree_helper::get_integer_cst_value(ref_min_ic))
                     {
                        ref_ssa->min = dead_ssa->min;
                     }
                  }
                  else
                  {
                     ref_ssa->min = tree_nodeRef();
                  }
               }
               const auto dead_max = dead_ssa->max;
               const auto ref_max = ref_ssa->max;
               if(ref_max)
               {
                  if(dead_max)
                  {
                     const auto dead_max_ic = GetPointer<const integer_cst>(GET_CONST_NODE(dead_max));
                     const auto ref_max_ic = GetPointer<const integer_cst>(GET_CONST_NODE(ref_max));
                     if(tree_helper::get_integer_cst_value(dead_max_ic) > tree_helper::get_integer_cst_value(ref_max_ic))
                     {
                        ref_ssa->max = dead_ssa->max;
                     }
                  }
                  else
                  {
                     ref_ssa->max = tree_nodeRef();
                  }
               }
            }
            const TreeNodeMap<size_t> StmtUses = dead_ssa->CGetUseStmts();
            for(const auto use : StmtUses)
            {
               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace equivalent statement before: "+ use.first->ToString());
               TM->ReplaceTreeNode(use.first, dead_ga->op0, ref_ga->op0);
               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace equivalent statement after: "+ use.first->ToString());
            }
            to_be_removed.insert(stmt);
#ifndef NDEBUG
            AppM->RegisterTransformation(GetName(), stmt);
#endif
            IR_changed = true;
            ++n_equiv_stmt;
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Replaced use of " + STR(dead_ga->op0));
         }
      }
      for(const auto stmt : to_be_removed)
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Removing " + STR(stmt));
         B->RemoveStmt(stmt);
      }
      if(B->CGetStmtList().empty() && B->CGetPhiList().empty() && !to_be_removed.empty())
         restart_phi_opt = true;
      if(!to_be_removed.empty() && schedule)
      {
         for(const auto stmt : B->CGetStmtList())
          schedule->UpdateTime(GET_INDEX_NODE(stmt));
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Considered BB" + STR(B->number));
   }
   if(!IR_changed)
      restart_phi_opt = false;
   INDENT_DBG_MEX(DEBUG_LEVEL_VERBOSE, debug_level, "---CSE: number of equivalent statement = " + STR(n_equiv_stmt));
   IR_changed ? function_behavior->UpdateBBVersion() : 0;
   return IR_changed ? DesignFlowStep_Status::SUCCESS : DesignFlowStep_Status::UNCHANGED;
}

void CSE::Initialize()
{
#if HAVE_BAMBU_BUILT && HAVE_ILP_BUILT
   if(GetPointer<const HLS_manager>(AppM) and GetPointer<const HLS_manager>(AppM)->get_HLS(function_id) and GetPointer<const HLS_manager>(AppM)->get_HLS(function_id)->Rsch)
   {
      schedule = GetPointer<const HLS_manager>(AppM)->get_HLS(function_id)->Rsch;
   }
#endif
}
