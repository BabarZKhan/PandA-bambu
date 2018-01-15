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
 * @file controller_creator_base_step.hpp
 * @brief Base class for all the controller creation algorithms.
 *
 * This class is a pure virtual one, that has to be specilized in order to implement a particular algorithm to create the
 * controller.
 *
 * @author Nicola Saporetti <nicola.saporetti@gmail.com>
 *
*/

#include "controller_parallel_cs.hpp"
#include "math.h"
#include "hls.hpp"
#include "structural_manager.hpp"
#include "structural_objects.hpp"
#include "hls_manager.hpp"
#include "BambuParameter.hpp"
#include "behavioral_helper.hpp"
#include "copyrights_strings.hpp"

controller_parallel_cs::controller_parallel_cs(const ParameterConstRef _Param, const HLS_managerRef _HLSMgr, unsigned int _funId, const DesignFlowManagerConstRef _design_flow_manager, const HLSFlowStep_Type _hls_flow_step_type) :
    fsm_controller(_Param, _HLSMgr, _funId, _design_flow_manager, _hls_flow_step_type)
{
}

controller_parallel_cs::~controller_parallel_cs()
{

}

DesignFlowStep_Status controller_parallel_cs::InternalExec()
{
   //its a module from the standard library so its only to be instantiated
   structural_objectRef circuit = this->SM->get_circ();
   add_common_ports(circuit);
   return DesignFlowStep_Status::SUCCESS;
}

void controller_parallel_cs::add_common_ports(structural_objectRef circuit)
{
   unsigned int num_slots=static_cast<unsigned int>(log2(HLS->Param->getOption<unsigned int>(OPT_context_switch)));
   structural_type_descriptorRef bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", 0));
   structural_type_descriptorRef port_type = structural_type_descriptorRef(new structural_type_descriptor("bool", num_slots));
   structural_objectRef clock_obj = SM->add_port(CLOCK_PORT_NAME, port_o::IN, circuit, port_type);
   GetPointer<port_o>(clock_obj)->set_is_clock(true);
   SM->add_port(RESET_PORT_NAME, port_o::IN, circuit, bool_type);
   SM->add_port(DONE_PORT_NAME, port_o::OUT, circuit, bool_type);
   SM->add_port(START_PORT_NAME, port_o::IN, circuit, bool_type);
   //till now added clock,reset, done,start
   SM->add_port(STR(DONE_PORT_NAME)+"accelerator", port_o::IN, circuit, port_type);
   SM->add_port(STR(DONE_REQUEST)+"accelerator", port_o::IN, circuit, port_type);
   SM->add_port(STR(START_PORT_NAME)+"accelerator", port_o::OUT, circuit, port_type);
   SM->add_port(STR(TASK_FINISHED), port_o::OUT, circuit, bool_type);
   structural_type_descriptorRef data_type = structural_type_descriptorRef(new structural_type_descriptor("bool", 32));
   SM->add_port("request", port_o::OUT, circuit, data_type);
   SM->add_port("LoopIteration", port_o::IN, circuit, data_type);
}
