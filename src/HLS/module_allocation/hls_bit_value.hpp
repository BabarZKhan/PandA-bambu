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
 *              Copyright (c) 2004-2018 Politecnico di Milano
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
 * @file hls_bit_value.hpp
 * @brief Proxy class calling the bit value analysis just before the hls_bit_value step taking into account the results of the memory hls_bit_value
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
*/
#ifndef HLS_BIT_VALUE_HPP
#define HLS_BIT_VALUE_HPP

///superclass include
#include "hls_function_step.hpp"
#include "application_manager.hpp"
#include "refcount.hpp"
/**
 * @name forward declarations
 */
//@{
REF_FORWARD_DECL(hls_bit_value);
//@}

#include "config_HAVE_EXPERIMENTAL.hpp"
#include "graph.hpp"
#include "utility.hpp"

#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <unordered_map>

#include "hls_manager.hpp"


/**
 * @class hls_bit_value
 * This class work as a proxy of the front end step performing the bit value analysis
 */
class hls_bit_value : public HLSFunctionStep
{

      void ComputeRelationships(DesignFlowStepSet & relationship, const DesignFlowStep::RelationshipType relationship_type) override;
      /**
       * Return the set of analyses in relationship with this design step
       * @param relationship_type is the type of relationship to be considered
       */
      virtual const std::unordered_set<std::tuple<HLSFlowStep_Type, HLSFlowStepSpecializationConstRef, HLSFlowStep_Relationship> > ComputeHLSRelationships(const DesignFlowStep::RelationshipType relationship_type) const override;

   public:

      /**
       * @name Constructors and Destructors.
      */
      //@{
      /**
       * Constructor.
       * @param design_flow_manager is the design flow manager
       */
      hls_bit_value(const ParameterConstRef Param, const HLS_managerRef HLSMgr, unsigned int funId, const DesignFlowManagerConstRef design_flow_manager);

      /**
       * Destructor.
       */
      ~hls_bit_value();
      //@}

      /**
       * Execute the step
       * @return the exit status of this step
       */
      virtual DesignFlowStep_Status InternalExec() override;

      /**
       * Initialize the step (i.e., like a constructor, but executed just before exec
       */
      virtual void Initialize() override;
};
#endif
