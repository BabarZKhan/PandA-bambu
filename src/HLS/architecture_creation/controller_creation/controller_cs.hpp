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
 *              Copyright (c) 2004-2016 Politecnico di Milano
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
 * @file controller_cs.hpp
 * @brief Base class for all the controller creation algorithms.
 * @author Nicola Saporetti <nicola.saporetti@gmail.com>
 *
*/
#ifndef CONTROLLER_CS_H
#define CONTROLLER_CS_H

#include "controller_creator_base_step.hpp"

class controller_cs : public ControllerCreatorBaseStep
{
 public:
    /**
     * Constructor
     */
    controller_cs(const ParameterConstRef Param, const HLS_managerRef HLSMgr, unsigned int funId, const DesignFlowManagerConstRef design_flow_manager, const HLSFlowStep_Type hls_flow_step_type);

    /**
     * Destructor.
     */
    virtual ~controller_cs();

 protected:

    void add_common_ports(structural_objectRef circuit);

    /**
    * Adds the done port to a circuit. Called by add_common_ports
    * The done port appears to go high once all the calculation of a function are completed
    * \param circuit the circuit where to add the done port
    */
    void add_selector_register_file_port(structural_objectRef circuit);

};

#endif // CONTROLLER_CS_H
