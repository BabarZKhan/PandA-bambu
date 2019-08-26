/*
 *
 *                         _/_/_/     _/_/    _/     _/ _/_/_/     _/_/
 *                        _/    _/ _/     _/ _/_/  _/ _/    _/ _/     _/
 *                      _/_/_/  _/_/_/_/ _/  _/_/ _/    _/ _/_/_/_/
 *                     _/        _/     _/ _/     _/ _/    _/ _/     _/
 *                    _/        _/     _/ _/     _/ _/_/_/  _/     _/
 *
 *                 ***********************************************
 *                                        PandA Project
 *                            URL: http://panda.dei.polimi.it
 *                              Politecnico di Milano - DEIB
 *                                System Architectures Group
 *                 ***********************************************
 *                  Copyright (C) 2004-2019 Politecnico di Milano
 *
 *    This file is part of the PandA framework.
 *
 *    The PandA framework is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
/**
 * @file lut_transformation.hpp
 * @brief recognize lut expressions.
 * @author Di Simone Jacopo
 * @author Cappello Paolo
 * @author Inajjar Ilyas
 * @author Angelo Gallarello
 * @author Stefano Longari
 * @author Marco Lattuada <marco.lattuada@polimi.it>
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 *
 */

#include "lut_transformation.hpp"

#if HAVE_STDCXX_17

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wsign-promo"

#define FMT_HEADER_ONLY 1

#include <mockturtle/mockturtle.hpp>
#endif

#include <type_traits>

/// Autoheader include
#include "config_HAVE_BAMBU_BUILT.hpp"

///. include
#include "Parameter.hpp"

/// behavior includes
#include "application_manager.hpp"
#include "function_behavior.hpp"

/// constants include
#include "allocation_constants.hpp"

/// design_flows includes
#include "design_flow_graph.hpp"
#include "design_flow_manager.hpp"

/// design_flows/technology includes
#include "technology_flow_step.hpp"
#include "technology_flow_step_factory.hpp"

/// HLS includes
#include "hls_manager.hpp"
#include "hls_target.hpp"

/// STD include
#include <fstream>

#if HAVE_BAMBU_BUILT
/// technology include
#include "technology_manager.hpp"

/// technology/physical_library/modes include
#include "time_model.hpp"
#endif

/// tree includes
#include "behavioral_helper.hpp"

/// technology/physical_library include
#include "technology_node.hpp"

/// utility include
#include "math_function.hpp"

/// tree includes
#include "dbgPrintHelper.hpp"        // for DEBUG_LEVEL_
#include "string_manipulation.hpp" // for GET_CLASS
#include "tree_basic_block.hpp"
#include "tree_helper.hpp"
#include "tree_manager.hpp"
#include "tree_manipulation.hpp"
#include "tree_reindex.hpp"

#if HAVE_STDCXX_17

#pragma region Macros declaration

#define IS_GIMPLE_ASSIGN(it) (GET_NODE(*it)->get_kind() == gimple_assign_K)
#define CHECK_BIN_EXPR_SIZE(binaryExpression) (static_cast<int>(tree_helper::Size(GET_NODE(binaryExpression->op0))) == 1 && static_cast<int>(tree_helper::Size(GET_NODE(binaryExpression->op1))) == 1)
#define VECT_CONTAINS(v, x) (std::find(v.begin(), v.end(), x) != v.end())

#pragma endregion

#pragma region Types declaration

/**
 * `aig_network_ext` class provides operations derived from the one already existing in `mockturtle::aig_network`.
 */
class klut_network_ext : public mockturtle::klut_network {
public:
    /**
     * Creates a 'greater' or equal operation.
     * 
     * @param a a `mockturtle::klut_network::signal` representing the first operator of the `ge` operation
     * @param b a `mockturtle::klut_network::signal` representing the second operator of the `ge` operation
     * 
     * @return a `mockturtle::klut_network::signal` representing the operation `ge` between `a` and `b`
     */
    signal create_ge(signal const a, signal const b) {
        return !this->create_lt(a, b);
    }

    /**
     * Creates a 'greater' operation.
     * 
     * @param a a `mockturtle::klut_network::signal` representing the first operator of the `gt` operation
     * @param b a `mockturtle::klut_network::signal` representing the second operator of the `gt` operation
     * 
     * @return a `mockturtle::klut_network::signal` representing the operation `gt` between `a` and `b`
     */
    signal create_gt(signal const a, signal const b) {
        return !this->create_le(a, b);
    }

    /**
     * Creates a 'equal' operation.
     * 
     * @param a a `mockturtle::klut_network::signal` representing the first operator of the `gt` operation
     * @param b a `mockturtle::klut_network::signal` representing the second operator of the `gt` operation
     * 
     * @return a `mockturtle::klut_network::signal` representing the operation `eq` between `a` and `b`
     */
    signal create_eq(signal const a, signal const b) {
        return !this->create_xor(a, b);
    }

    /**
     * Creates a 'not equal' operation.
     * 
     * @param a a `mockturtle::klut_network::signal` representing the first operator of the `ne` operation
     * @param b a `mockturtle::klut_network::signal` representing the second operator of the `ne` operation
     * 
     * @return a `mockturtle::klut_network::signal` representing the operation `ne` between `a` and `b`
     */
    signal create_ne(signal const a, signal const b) {
        return this->create_xor(a, b);
    }

    /**
     * Creates a 'lut' operation from an `std::vector` of `mockturtle::klut_network::signal` with the associated constant.
     * 
     * @param s an `std::vector` of `mockturtle::klut_network::signal` containing the inputs of the lut
     * @param f the constant associated to the lut
     * 
     * @return a `mockturtle::klut_network::signal` representing a lut between `s` with constant `f`
     */
    signal create_lut(std::vector<signal> s, uint32_t f) {
        return this->_create_node(s, f);
    }
};

/**
 * Helper structure that better represents a `mockturtle::klut_network`'s node.
 */
struct klut_network_node {
    /// the index of the node
    uint64_t index;

    /// the lut constant
    uint64_t lut_constant;

    /// a `std::vector` containing the indexes of all inputs of the current node
    std::vector<uint64_t> fan_in;

    /// whether the current node is a primary output
    bool is_po;

    /// in case the current node is a primary output, holds the index of the primary output
    uint64_t po_index;
};

/**
 * Pointer that points to a function of `klut_network_ext`, that represents a binary operation between two `mockturtle::klut_network::signal`s
 * and returns a `mockturtle::klut_network::signal`.
 */
typedef mockturtle::klut_network::signal (klut_network_ext::*klut_network_fn)(const mockturtle::klut_network::signal, const mockturtle::klut_network::signal);

#pragma endregion

/**
 * Checks whether the provided node is a primary input of lut network.
 * 
 * @param in a `tree_nodeRef` 
 */
bool lut_transformation::CheckIfPI(tree_nodeRef in, unsigned int BB_index)
{
   auto ssa = GetPointer<ssa_name>(GET_NODE(in));
   if(!ssa)
      THROW_ERROR("expected as in a ssa variable");
   tree_nodeRef def_stmt = GET_NODE(ssa->CGetDefStmt());
   if(def_stmt->get_kind() != gimple_assign_K)
      return true;
   auto* gaDef = GetPointer<gimple_assign>(def_stmt);
   if(gaDef->bb_index != BB_index)
      return true;
   enum kind code = GET_NODE(gaDef->op1)->get_kind();

   if (!VECT_CONTAINS(lutExpressibleOperations, code) || (GetPointer<binary_expr>(GET_NODE(gaDef->op1)) && !CHECK_BIN_EXPR_SIZE(GetPointer<binary_expr>(GET_NODE(gaDef->op1))))) {
      return true;
   }
   return false;
}

/**
 * Checks if the provided basic block can be further processed. There are two cases in which this condition is true:
 *  - the basic block contains an instruction convertible to a `lut_expr_K`
 *  - the basic block contains a `lut_expr_K` that has constant inputs
 * 
 * @param block the block to check
 * @return whether the provided basic block can be further processed
 */
bool lut_transformation::CheckIfProcessable(std::pair<unsigned int, blocRef> block) {
    auto &statements = block.second->CGetStmtList();

    for (auto currentStatement = statements.begin(); currentStatement != statements.end(); ++currentStatement) {
        // only gimple assignments are considered
        if (!IS_GIMPLE_ASSIGN(currentStatement)) {
            continue;
        }

        auto *gimpleAssign = GetPointer<gimple_assign>(GET_NODE(*currentStatement));
        enum kind code = GET_NODE(gimpleAssign->op1)->get_kind();

        if (code == lut_expr_K) { // check if it has constant inputs
            auto *lut = GetPointer<lut_expr>(gimpleAssign->op1);
            
            // cycle for each inputs (op0 is the constant)
            for (auto node : {lut->op1, lut->op2, lut->op3, lut->op4, lut->op5, lut->op6, lut->op7, lut->op8}) {
                // if the node is null then there are no more inputs
                if (!node) {
                    break; // TODO: check if the assumption is always true, otherwise change `break` into `continue`
                }

                // if the node can be converted into an `integer_cst` then the lut as constant inputs
                if (GetPointer<integer_cst>(GET_NODE(node))) {
                    return true;
                }
            }
        } else { // check if it has lut-expressible operations
            // checks if the operation code can be converted into a lut
            // and if it is a binary expression with the correct size of operators
            if (VECT_CONTAINS(lutExpressibleOperations, code) && GetPointer<binary_expr>(GET_NODE(gimpleAssign->op1)) && CHECK_BIN_EXPR_SIZE(GetPointer<binary_expr>(GET_NODE(gimpleAssign->op1)))) {
                return true;
            }
        }
    }

    return false;
}

/**
 * Checks if the provided `gimple_assign` is a primary output of lut network.
 * 
 * @param gimpleAssign the `gimple_assign` to check
 * @return whether the provided `gimple_assign` is a primary output
 */
bool lut_transformation::CheckIfPO(gimple_assign *gimpleAssign) {
    /// the index of the basic block holding the provided `gimpleAssign`
    const unsigned int currentBBIndex = gimpleAssign->bb_index;
    // the variables that uses the result of the provided `gimpleAssign`
    const auto op0 = GET_NODE(gimpleAssign->op0);
    auto ssa0 = GetPointer<ssa_name>(op0);
    THROW_ASSERT(ssa0, "unexpected condition");
    const auto usedIn = ssa0->CGetUseStmts();

    for (auto node : usedIn) {
       auto *childGimpleNode = GetPointer<gimple_node>(GET_NODE(node.first));
       THROW_ASSERT(childGimpleNode, "unexpected condition");

        // the current operation is a primary output if it is used in
        // operation not belonging to the current basic block or if the operation
        // in which it is used does not belong to the K-operations set
        if (childGimpleNode->bb_index != currentBBIndex) {
            return true;
        }
        else {
           auto *childGimpleAssign = GetPointer<gimple_assign>(GET_NODE(node.first));
           if(!childGimpleAssign)
              return true;
            enum kind code = GET_NODE(childGimpleAssign->op1)->get_kind();

            // it is a `PO` if code is not contained into `lutExpressibleOperations`
            if (!VECT_CONTAINS(lutExpressibleOperations, code) || (GetPointer<binary_expr>(GET_NODE(gimpleAssign->op1)) && !CHECK_BIN_EXPR_SIZE(GetPointer<binary_expr>(GET_NODE(gimpleAssign->op1))))) {
                return true;
            }
        }
    }

    return false;
}

static
klut_network_fn GetNodeCreationFunction(enum kind code) {
    switch (code) {
        case bit_and_expr_K:
        case truth_and_expr_K:
            return &klut_network_ext::create_and;
        case bit_ior_expr_K:
        case truth_or_expr_K:
            return &klut_network_ext::create_or;
        case bit_xor_expr_K:
        case truth_xor_expr_K:
            return &klut_network_ext::create_xor;
        case eq_expr_K:
            return &klut_network_ext::create_eq;
        case ge_expr_K:
            return &klut_network_ext::create_ge;
        case gt_expr_K:
            return &klut_network_ext::create_gt;
        case le_expr_K:
            return &klut_network_ext::create_le;
        case lt_expr_K:
            return &klut_network_ext::create_lt;
        case ne_expr_K:
            return &klut_network_ext::create_ne;
        default:
            return nullptr;
    }
}

template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type
>
static
T ConvertHexToInt64(const std::string &hex0) {
    uint64_t x;
    std::stringstream ss;
    ss << std::hex << hex0;
    ss >> x;

    return static_cast<T>(x);
}

static
std::vector<klut_network_node> ParseKLutNetwork(const mockturtle::klut_network &klut) {
    std::vector<klut_network_node> luts;
    std::map<mockturtle::klut_network::node, unsigned> po_set;

    mockturtle::topo_view ntk_topo{klut};

    auto p_index=0u;
    ntk_topo.foreach_po([&](const auto &node) {
       po_set[node]=p_index;
       ++p_index;
    });

    ntk_topo.foreach_node([&](const auto &node) {
        if (ntk_topo.is_pi(node) || ntk_topo.is_constant(node)) {
            return; // continue
        }        
        auto func = ntk_topo.node_function(node);
        
        std::vector<uint64_t> fanIns;
        ntk_topo.foreach_fanin(node, [&](auto const &fanin_node, auto index) {
            fanIns.push_back(fanin_node);
        });

        klut_network_node lut_node = (klut_network_node) {
            node,
            ConvertHexToInt64<uint64_t>(kitty::to_hex(func)),
            fanIns,
            po_set.find(node) != po_set.end(),
            po_set.find(node) != po_set.end() ? po_set.find(node)->second : 0
        };

        luts.push_back(lut_node);
    });

    return luts;
}

static
mockturtle::klut_network SimplifyLutNetwork(const klut_network_ext &klut_e, unsigned max_lut_size) {
    auto cleanedUp = cleanup_dangling(klut_e);
    mockturtle::mapping_view<mockturtle::klut_network, true> mapped_klut{cleanedUp};

    mockturtle::lut_mapping_params ps;
    ps.cut_enumeration_ps.cut_size = max_lut_size;

#ifndef NDEBUG
    ps.verbose = true;
    ps.cut_enumeration_ps.very_verbose = true;
#endif

    mockturtle::lut_mapping<mockturtle::mapping_view<mockturtle::klut_network, true>, true>(mapped_klut, ps);
    return *mockturtle::collapse_mapped_network<mockturtle::klut_network>(mapped_klut);
}

bool lut_transformation::ProcessBasicBlock(std::pair<unsigned int, blocRef> block) {
    klut_network_ext klut_e;
    auto BB_index = block.first;

    std::map<tree_nodeRef, mockturtle::klut_network::signal> nodeRefToSignal;

    std::vector<tree_nodeRef> pis;
    std::vector<tree_nodeRef> pos;

    auto DefaultUnsignedLongLongInt = this->tree_man->CreateDefaultUnsignedLongLongInt();

    /**
     * Creates a const expression with 0 (gnd) as value, used for constant LUT inputs (index 0 in mockturtle)
     */ 
    pis.push_back(this->tree_man->CreateIntegerCst(
        DefaultUnsignedLongLongInt,
        0ll,
        this->TM->new_tree_node_id()
    ));

    /**
     * Creates a const expression with 1 (vdd) as value, used for constant LUT inputs (index 1 in mockturtle)
     */
    pis.push_back(this->tree_man->CreateIntegerCst(
        DefaultUnsignedLongLongInt,
        1ll,
        this->TM->new_tree_node_id()
    ));

    INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Analyzing BB" + STR(BB_index));
    const auto &statements = block.second->CGetStmtList();

    /// whether the BB has been modified
    bool modified = false;

    // TODO: convert to function
    for (auto currentStatement = statements.begin(); currentStatement != statements.end(); ++currentStatement) {
        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Analyzing " + (*currentStatement)->ToString());

#ifndef NDEBUG
        if(!AppM->ApplyNewTransformation()) {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Reached max cfg transformations");
            continue;
        }
#endif

        if (!IS_GIMPLE_ASSIGN(currentStatement)) {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Not a gimple assign");
            continue;
        }

        auto *gimpleAssign = GetPointer<gimple_assign>(GET_NODE(*currentStatement));
        enum kind code1 = GET_NODE(gimpleAssign->op1)->get_kind();

        if (code1 == lut_expr_K) {
            auto *le = GetPointer<lut_expr>(GET_NODE(gimpleAssign->op1));

            std::vector<mockturtle::klut_network::signal> ops;
            for (auto op : {le->op1, le->op2, le->op3, le->op4, le->op5, le->op6, le->op7, le->op8}) {
                if (!op) {
                    break;
                }

                // if the first operand has already been processed then the previous signal is used
                if (nodeRefToSignal.find(op) != nodeRefToSignal.end()) {
                    ops.push_back(nodeRefToSignal[op]);
                }
                else { // otherwise the operand is a primary input
                    mockturtle::klut_network::signal kop;

                    if (GET_NODE(op)->get_kind() == integer_cst_K) {
                        auto *int_const = GetPointer<integer_cst>(GET_NODE(op));
                        kop = klut_e.get_constant(int_const->value != 0);
                    }
                    else if (CheckIfPI(op, BB_index)) {
                        kop = klut_e.create_pi();
                        pis.push_back(op);
                    }
                    else
                        THROW_ERROR("unexpected condition");

                    nodeRefToSignal[op] = kop;
                    ops.push_back(kop);
                }
            }

            klut_e.create_lut(ops, GetPointer<integer_cst>(GET_NODE(le->op0))->value);

            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level,"<--LUT found");

            continue;
        }

        auto *binaryExpression = GetPointer<binary_expr>(GET_NODE(gimpleAssign->op1));
        if (!binaryExpression) {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Not a binary expression");
            continue;
        }

        // the operands must be Boolean
        if (!CHECK_BIN_EXPR_SIZE(binaryExpression)) {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Not Boolean operands");
            continue;
        }

        klut_network_fn nodeCreateFn = GetNodeCreationFunction(code1);

        if (nodeCreateFn == nullptr) {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Not supported expression");
            continue;
        }

        mockturtle::klut_network::signal res;
        mockturtle::klut_network::signal op1;
        mockturtle::klut_network::signal op2;

        // if the first operand has already been processed then the previous signal is used
        if (nodeRefToSignal.find(binaryExpression->op0) != nodeRefToSignal.end()) {
            op1 = nodeRefToSignal[binaryExpression->op0];
        }
        else { // otherwise the operand is a primary input
           if(GET_NODE(binaryExpression->op0)->get_kind() == integer_cst_K)
           {
              auto* int_const = GetPointer<integer_cst>(GET_NODE(binaryExpression->op0));
              if(int_const->value == 0)
                 op1 = klut_e.get_constant(false);
              else
                 op1 = klut_e.get_constant(true);
           }
           else if(CheckIfPI(binaryExpression->op0, BB_index))
           {
              op1 = klut_e.create_pi();
              pis.push_back(binaryExpression->op0);
           }
           else
              THROW_ERROR("unexpected condition");

            nodeRefToSignal[binaryExpression->op0] = op1;
        }

        // if the second operand has already been processed then the previous signal is used
        if (nodeRefToSignal.find(binaryExpression->op1) != nodeRefToSignal.end()) {
            op2 = nodeRefToSignal[binaryExpression->op1];
        }
        else { // otherwise the operand is a primary input
           if(GET_NODE(binaryExpression->op1)->get_kind() == integer_cst_K)
           {
              auto* int_const = GetPointer<integer_cst>(GET_NODE(binaryExpression->op1));
              if(int_const->value == 0)
                 op2 = klut_e.get_constant(false);
              else
                 op2 = klut_e.get_constant(true);
           }
           else if(CheckIfPI(binaryExpression->op1, BB_index))
           {
              op2 = klut_e.create_pi();
              pis.push_back(binaryExpression->op1);
           }
           else
              THROW_ERROR("unexpected condition");

            nodeRefToSignal[binaryExpression->op1] = op2;
        }

        res = (klut_e.*nodeCreateFn)(op1, op2);
        nodeRefToSignal[gimpleAssign->op0] = res;

        if (this->CheckIfPO(gimpleAssign)) {
            std::cerr<<"is PO\n";
            klut_e.create_po(res);
            pos.push_back(*currentStatement);
        }

        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Analyzed statement ");
        modified = true;
    }
       mockturtle::write_bench(klut, std::cout);
       std::vector<klut_network_node> luts = ParseKLutNetwork(klut);

    if (modified) {
       mockturtle::klut_network klut = SimplifyLutNetwork(klut_e, this->max_lut_size);
       mockturtle::write_bench(klut, std::cout);

       std::vector<klut_network_node> luts = ParseKLutNetwork(klut);

       std::cerr << "PI size" << pis.size() << "\n";
       std::map<mockturtle::klut_network::node, tree_nodeRef> internal_nets;
       std::vector<tree_nodeRef> prev_stmts_to_add;
       for (auto lut : luts)
       {
          std::cerr << "index: " << lut.index <<"\n";
          std::cerr << " func: " << lut.lut_constant << "\n";
          for(auto in : lut.fan_in)
             std::cerr << " in " << in;
          std::cerr << "\n";
          if(lut.is_po)
          {
             std::cerr << "Is PO\n";
             /// add previous statements defining non-primary outputs just before the current statement
             for(auto stmt: prev_stmts_to_add)
             {
                INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Adding statement " + GET_NODE(stmt)->ToString());
                block.second->PushBefore(stmt, pos.at(lut.po_index));
             }
             prev_stmts_to_add.clear();
          }
          unsigned int integer_cst2_id = TM->new_tree_node_id();
          tree_nodeRef lut_constant_node = tree_man->CreateIntegerCst(DefaultUnsignedLongLongInt, lut.lut_constant, integer_cst2_id);
          tree_nodeRef op1, op2, op3, op4, op5, op6, op7, op8;
          auto p_index = 1u;
          for(auto in : lut.fan_in)
          {
             std::cerr << " in " << in;
             tree_nodeRef operand;
             if(klut.is_pi(in))
                operand = pis.at(in);
             else if(internal_nets.find(in) != internal_nets.end())
                operand = internal_nets.find(in)->second;
             else
                THROW_ERROR("unexpected condition");
             if(p_index == 1)
                op1 = operand;
             else if(p_index == 2)
                op2 = operand;
             else if(p_index == 3)
                op3 = operand;
             else if(p_index == 4)
                op4 = operand;
             else if(p_index == 5)
                op5 = operand;
             else if(p_index == 6)
                op6 = operand;
             else if(p_index == 7)
                op7 = operand;
             else if(p_index == 8)
                op8 = operand;
             else
             {
                THROW_ERROR("unexpected number of inputs");
             }
                ++p_index;
          }
          if(lut.is_po)
          {
             auto *gimpleAssign = GetPointer<gimple_assign>(GET_NODE(pos.at(lut.po_index)));
             THROW_ASSERT(gimpleAssign, "unexpected condition");
             const std::string srcp_default = gimpleAssign->include_name + ":" + STR(gimpleAssign->line_number) + ":" + STR(gimpleAssign->column_number);
             auto ga_op0 = GET_NODE(gimpleAssign->op0);
             auto *ssa_ga_op0 = GetPointer<ssa_name>(ga_op0);
             THROW_ASSERT(ssa_ga_op0, "unexpected condition");
             tree_nodeRef new_op1 = tree_man->create_lut_expr(ssa_ga_op0->type, lut_constant_node, op1, op2, op3, op4, op5, op6, op7, op8, srcp_default);

             INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Replacing " + STR(gimpleAssign->op1) + " with " + STR(new_op1));
             TM->ReplaceTreeNode(pos.at(lut.po_index), gimpleAssign->op1, new_op1);
#ifndef NDEBUG
             AppM->RegisterTransformation(GetName(), pos.at(lut.po_index));
#endif
             INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Modified statement " + GET_NODE(pos.at(lut.po_index))->ToString());
          }
          else
          {
             const std::string srcp_default("built-in:0:0");
             auto boolType = tree_man->create_boolean_type();
             tree_nodeRef new_op1 = tree_man->create_lut_expr(boolType, lut_constant_node, op1, op2, op3, op4, op5, op6, op7, op8, srcp_default);
             tree_nodeRef ssa_vd = tree_man->create_ssa_name(tree_nodeRef(), boolType);
             prev_stmts_to_add.push_back(tree_man->create_gimple_modify_stmt(ssa_vd, new_op1, srcp_default, BB_index));
          }
       }
       THROW_ASSERT(prev_stmts_to_add.empty(), "unexpected condition");
    }

    INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Analyzed BB" + STR(block.first));

    return modified;
}

#pragma region Life cycle

#endif

lut_transformation::~lut_transformation() = default;

void lut_transformation::Initialize() {
    TM = AppM->get_tree_manager();
    tree_man = tree_manipulationRef(new tree_manipulation(TM, parameters));
    THROW_ASSERT(GetPointer<const HLS_manager>(AppM)->get_HLS_target(), "");
    const auto hls_target = GetPointer<const HLS_manager>(AppM)->get_HLS_target();
    THROW_ASSERT(hls_target->get_target_device()->has_parameter("max_lut_size"), "");
    max_lut_size = hls_target->get_target_device()->get_parameter<size_t>("max_lut_size");
}

void lut_transformation::ComputeRelationships(DesignFlowStepSet &relationship, const DesignFlowStep::RelationshipType relationship_type) {
    switch(relationship_type) {
        case(PRECEDENCE_RELATIONSHIP):
            break;
        case DEPENDENCE_RELATIONSHIP: {
            const DesignFlowGraphConstRef design_flow_graph = design_flow_manager.lock()->CGetDesignFlowGraph();
            const auto *technology_flow_step_factory = GetPointer<const TechnologyFlowStepFactory>(design_flow_manager.lock()->CGetDesignFlowStepFactory("Technology"));
            const std::string technology_flow_signature = TechnologyFlowStep::ComputeSignature(TechnologyFlowStep_Type::LOAD_TECHNOLOGY);
            const vertex technology_flow_step = design_flow_manager.lock()->GetDesignFlowStep(technology_flow_signature);
            const DesignFlowStepRef technology_design_flow_step = technology_flow_step ? 
                design_flow_graph->CGetDesignFlowStepInfo(technology_flow_step)->design_flow_step : 
                technology_flow_step_factory->CreateTechnologyFlowStep(TechnologyFlowStep_Type::LOAD_TECHNOLOGY);
            relationship.insert(technology_design_flow_step);

            break;
        }
        case INVALIDATION_RELATIONSHIP:
            break;
        default:
            THROW_UNREACHABLE("");
    }

    FunctionFrontendFlowStep::ComputeRelationships(relationship, relationship_type);
}

lut_transformation::lut_transformation(const ParameterConstRef Param, const application_managerRef _AppM, unsigned int _function_id, const DesignFlowManagerConstRef _design_flow_manager)
     : FunctionFrontendFlowStep(_AppM, _function_id, LUT_TRANSFORMATION, _design_flow_manager, Param), max_lut_size(NUM_CST_allocation_default_max_lut_size) {
    debug_level = Param->get_class_debug_level(GET_CLASS(*this), DEBUG_LEVEL_NONE);
}

DesignFlowStep_Status lut_transformation::InternalExec()
{
#if HAVE_STDCXX_17
    tree_nodeRef temp = TM->get_tree_node_const(function_id);
    auto* fd = GetPointer<function_decl>(temp);
    THROW_ASSERT(fd && fd->body, "Node is not a function or it has not a body");
    auto* sl = GetPointer<statement_list>(GET_NODE(fd->body));
    THROW_ASSERT(sl, "Body is not a statement list");

    bool modified = false;

    for (std::pair<unsigned int, blocRef> block : sl->list_of_bloc) {
        if (this->CheckIfProcessable(block)) {
            modified |= this->ProcessBasicBlock(block);
        }
    }

    if (modified) {
        function_behavior->UpdateBBVersion();
        return DesignFlowStep_Status::SUCCESS;
    }
#endif
    return DesignFlowStep_Status::UNCHANGED;
}

const std::unordered_set<std::pair<FrontendFlowStepType, FrontendFlowStep::FunctionRelationship>> lut_transformation::ComputeFrontendRelationships(const DesignFlowStep::RelationshipType relationship_type) const {
    std::unordered_set<std::pair<FrontendFlowStepType, FunctionRelationship>> relationships;
    switch(relationship_type) {
        case(DEPENDENCE_RELATIONSHIP):
            relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(BIT_VALUE_OPT, SAME_FUNCTION));
            break;
        case(INVALIDATION_RELATIONSHIP):
            if (GetStatus() == DesignFlowStep_Status::SUCCESS) {
                relationships.insert(
                    std::pair<FrontendFlowStepType, 
                    FunctionRelationship>(DEAD_CODE_ELIMINATION, SAME_FUNCTION)
                );
            }
            break;
        case(PRECEDENCE_RELATIONSHIP):
            relationships.insert(
                std::pair<FrontendFlowStepType, 
                FunctionRelationship>(MULTI_WAY_IF, SAME_FUNCTION)
            );
            break;
        default:
            THROW_UNREACHABLE("");
    }
    return relationships;
}

#if HAVE_STDCXX_17
#pragma endregion

#pragma GCC diagnostic pop

#endif
