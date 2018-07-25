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
 * @file application_frontend_flow_step.hpp
 * @brief This class contains the base representation for a generic frontend flow step which works on the whole function
 *
 * @author Marco Lattuada <lattuada@elet.polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
*/

#ifndef APPLICATION_FRONTEND_FLOW_STEP_HPP
#define APPLICATION_FRONTEND_FLOW_STEP_HPP

#include <string>                  // for string
#include <unordered_set>           // for unordered_set
#include <utility>                 // for pair
#include "design_flow_step.hpp"    // for DesignFlowManagerConstRef, DesignF...
#include "frontend_flow_step.hpp"  // for FrontendFlowStepType, FrontendFlow...

class ApplicationFrontendFlowStep : public FrontendFlowStep
{
   private:
      /**
       * Return the set of analyses in relationship with this design step
       * @param relationship_type is the type of relationship to be considered
       */
      virtual const std::unordered_set<std::pair<FrontendFlowStepType, FunctionRelationship> > ComputeFrontendRelationships(const DesignFlowStep::RelationshipType relationship_type) const = 0;

   public:
      /**
       * Constructor
       * @param AppM is the application manager
       * @param frontend_flow_step_type is the type of the step
       * @param design_flow_manager is the design flow manager
       * @param _Param is the set of the parameters
       */
      ApplicationFrontendFlowStep(const application_managerRef AppM, const FrontendFlowStepType frontend_flow_step_type, const DesignFlowManagerConstRef design_flow_manager, const ParameterConstRef parameters);

      /**
       * Destructor
       */
      virtual ~ApplicationFrontendFlowStep();

      /**
       * Execute this step
       * @return the exit status of this step
       */
      virtual DesignFlowStep_Status Exec() = 0;

      /**
       * Return the signature of this step
       */
      virtual const std::string GetSignature() const;

      /**
       * Return the name of this design step
       * @return the name of the pass (for debug purpose)
       */
      virtual const std::string GetName() const;

      /**
       * Compute the signature of a function frontend flow step
       * @param frontend_flow_step_type is the type of frontend flow
       * @return the corresponding signature
       */
      static
      const std::string ComputeSignature(const FrontendFlowStepType frontend_flow_step_type);

      /**
       * Check if this step has actually to be executed
       * @return true if the step has to be executed
       */
      virtual bool HasToBeExecuted() const;
};
#endif
