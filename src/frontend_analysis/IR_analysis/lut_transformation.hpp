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
 * @author  Cappello Paolo
 * @author Inajjar Ilyas
 * @author Angelo Gallarello
 * @author  Stefano Longari
 * Marco Lattuada <marco.lattuada@polimi.it>
 *
 */
#ifndef LUT_TRANSFORMATION_HPP
#define LUT_TRANSFORMATION_HPP

#include "config_HAVE_STDCXX_17.hpp"

/// Super class include
#include "function_frontend_flow_step.hpp"

/// STD include
#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>
/// Utility include
#include "refcount.hpp"

#include "tree_common.hpp"
#include "tree_node.hpp"


//@{
REF_FORWARD_DECL(bloc);
// class integer_cst;
// class target_mem_ref461;
// REF_FORWARD_DECL(lut_transformation);
REF_FORWARD_DECL(Schedule);
REF_FORWARD_DECL(tree_manager);
REF_FORWARD_DECL(tree_manipulation);
REF_FORWARD_DECL(tree_node);
//@}

class lut_transformation : public FunctionFrontendFlowStep {
private:
    /// The tree manager
    tree_managerRef TM;

    /// The lut manipulation
    tree_manipulationRef tree_man;

    /// The maximum number of inputs of a lut
    size_t max_lut_size;

#if HAVE_STDCXX_17

    /// The list of all operation that can be converted to a lut.
    const std::vector<enum kind> lutExpressibleOperations = {bit_and_expr_K, truth_and_expr_K, bit_ior_expr_K, truth_or_expr_K, bit_xor_expr_K, truth_xor_expr_K, eq_expr_K, ge_expr_K, lut_expr_K, cond_expr_K, gt_expr_K, le_expr_K, lt_expr_K, ne_expr_K};

    /**
     * Checks if the provided `gimple_assign` is a primary output of lut network.
     * 
     * @param gimpleAssign the `gimple_assign` to check
     * @return whether the provided `gimple_assign` is a primary output
     */
    bool CheckIfPO(gimple_assign *gimpleAssign);

    bool CheckIfPI(tree_nodeRef in, unsigned int BB_index);


    bool ProcessBasicBlock(std::pair<unsigned int, blocRef> block);

    bool CheckIfProcessable(std::pair<unsigned int, blocRef> block);

    tree_nodeRef CreateBitSelectionNode(const tree_nodeRef source, int index, std::pair<const unsigned int, blocRef> bb);

#endif

    /**
     * Return the set of analyses in relationship with this design step
     * @param relationship_type is the type of relationship to be considered
     */
    const std::unordered_set<std::pair<FrontendFlowStepType, FunctionRelationship>> ComputeFrontendRelationships(const DesignFlowStep::RelationshipType relationship_type) const override;

public:
    /**
     * Constructor.
     * @param Param is the set of the parameters
     * @param AppM is the application manager
     * @param function_id is the identifier of the function
     * @param DesignFlowManagerConstRef is the design flow manager
     */
    lut_transformation(const ParameterConstRef Param, const application_managerRef AppM, unsigned int function_id, const DesignFlowManagerConstRef design_flow_manager);

    /**
     *  Destructor
     */
    ~lut_transformation() override;

    /**
     * Computes the operations CFG graph data structure.
     * @return the exit status of this step
     */
    DesignFlowStep_Status InternalExec() override;

    /**
     * Initialize the step (i.e., like a constructor, but executed just before exec
     */
    void Initialize() override;

    /**
     * Compute the relationships of a step with other steps
     * @param dependencies is where relationships will be stored
     * @param relationship_type is the type of relationship to be computed
     */
    void ComputeRelationships(DesignFlowStepSet& relationship, const DesignFlowStep::RelationshipType relationship_type) override;
};

#endif
