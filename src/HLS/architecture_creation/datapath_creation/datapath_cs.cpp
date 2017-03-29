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
 * @file classic_datapath.cpp
 * @brief Datapath for context switch
 *
 * @author Nicola Saporetti <nicola.saporetti@gmail.com>
 *
*/

#include "omp_functions.hpp"
#include "datapath_cs.hpp"
#include "structural_objects.hpp"
#include "structural_manager.hpp"
#include "Parameter.hpp"
#include "hls_manager.hpp"
#include "hls.hpp"
#include "math.h"

datapath_cs::datapath_cs(const ParameterConstRef _parameters, const HLS_managerRef _HLSMgr, unsigned int _funId, const DesignFlowManagerConstRef _design_flow_manager) :
   classic_datapath(_parameters, _HLSMgr, _funId, _design_flow_manager)
{
    debug_level = parameters->get_class_debug_level(GET_CLASS(*this));
}

datapath_cs::~datapath_cs()
{

}

DesignFlowStep_Status datapath_cs::InternalExec()
{
    classic_datapath::Exec();    //exec of hierarchical class
    //new merging
    return DesignFlowStep_Status::SUCCESS;
}

void datapath_cs::add_ports()
{
    classic_datapath::add_ports();      //add standard port
    auto omp_functions = GetPointer<OmpFunctions>(HLSMgr->Rfuns);
    bool found=false;
    if(omp_functions->kernel_functions.find(funId) != omp_functions->kernel_functions.end()) found=true;
    if(omp_functions->parallelized_functions.find(funId) != omp_functions->parallelized_functions.end()) found=true;
    if(omp_functions->atomic_functions.find(funId) != omp_functions->atomic_functions.end()) found=true;
    if(found)       //function with selector
    {
       const structural_managerRef& SM = this->HLS->datapath;
       const structural_objectRef circuit = SM->get_circ();
       unsigned int num_slots=static_cast<unsigned int>(log2(HLS->Param->getOption<unsigned int>(OPT_context_switch)));
       structural_type_descriptorRef port_type = structural_type_descriptorRef(new structural_type_descriptor("bool", num_slots));
       SM->add_port(SELECTOR_REGISTER_FILE, port_o::IN, circuit, port_type);
       structural_type_descriptorRef bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", 0));
       SM->add_port(SUSPENSION, port_o::OUT, circuit, bool_type);
    }
}
