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
 * @file fu_binding_cs.cpp
 * @brief Derived class to add module scheduler, mem_ctrl_parallel and bind correctly the channels
 *
 * @author Nicola Saporetti <nicola.saporetti@gmail.com>
*/
#include "fu_binding_cs.hpp"
#include "omp_functions.hpp"
#include "structural_objects.hpp"
#include "structural_manager.hpp"
#include "hls_manager.hpp"
#include "hls.hpp"
#include "hls_target.hpp"
#include "Parameter.hpp"
#include "technology_node.hpp"
#include "technology_manager.hpp"
#include "reg_binding_cs.hpp"

fu_binding_cs::fu_binding_cs(const HLS_managerConstRef _HLSMgr, const unsigned int _function_id, const ParameterConstRef _parameters) :
   fu_binding(_HLSMgr, _function_id, _parameters)
{
}

void fu_binding_cs::add_to_SM(const HLS_managerRef HLSMgr, const hlsRef HLS, structural_objectRef clock_port, structural_objectRef reset_port)
{
   auto omp_functions = GetPointer<OmpFunctions>(HLSMgr->Rfuns);
   if(omp_functions->kernel_functions.find(HLS->functionId) != omp_functions->kernel_functions.end())
   {
      instantiate_component_kernel(HLS, clock_port, reset_port);
      instantiate_suspension_component(HLSMgr,HLS);
   }
   else if((omp_functions->parallelized_functions.find(HLS->functionId) != omp_functions->parallelized_functions.end()) || (omp_functions->atomic_functions.find(HLS->functionId) != omp_functions->atomic_functions.end()))
      instantiate_suspension_component(HLSMgr,HLS);
   fu_binding::add_to_SM(HLSMgr,HLS,clock_port,reset_port);
}

void fu_binding_cs::instantiate_component_kernel(const hlsRef HLS, structural_objectRef clock_port, structural_objectRef reset_port)
{
   const structural_managerRef SM = HLS->datapath;
   const structural_objectRef circuit = SM->get_circ();
   structural_type_descriptorRef bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", 0));
   std::string scheduler_model = "scheduler";
   std::string scheduler_name = "scheduler_kernel";
   std::string sche_library = HLS->HLS_T->get_technology_manager()->get_library(scheduler_model);
   structural_objectRef scheduler_mod = SM->add_module_from_technology_library(scheduler_name, scheduler_model, sche_library, circuit, HLS->HLS_T->get_technology_manager());
   unsigned int num_slots=static_cast<unsigned int>(ceil(log2(HLS->Param->getOption<unsigned int>(OPT_context_switch))));   //resize selector-port
   port_o::resize_std_port(num_slots, 0, 0, scheduler_mod->find_member(STR(SELECTOR_REGISTER_FILE),port_o_K,scheduler_mod));  //resize selector-port
   structural_objectRef selector_regFile_scheduler = scheduler_mod->find_member(STR(SELECTOR_REGISTER_FILE),port_o_K,scheduler_mod);
   structural_objectRef selector_regFile_datapath = circuit->find_member(STR(SELECTOR_REGISTER_FILE),port_o_K,circuit);
   structural_type_descriptorRef port_type = structural_type_descriptorRef(new structural_type_descriptor("bool", num_slots));
   structural_objectRef selector_regFile_sign=SM->add_sign(STR(SELECTOR_REGISTER_FILE)+"_signal", circuit, port_type);
   SM->add_connection(selector_regFile_sign, selector_regFile_datapath);
   SM->add_connection(selector_regFile_sign, selector_regFile_scheduler);
   GetPointer<reg_binding_cs>(HLS->Rreg)->add_register_file_kernel(selector_regFile_sign);   //connect register
   //suspension connected when or_port instantiated

   structural_objectRef clock_scheduler = scheduler_mod->find_member(CLOCK_PORT_NAME,port_o_K,scheduler_mod);
   structural_objectRef clock_sign=SM->add_sign("clock_scheduler_signal", circuit, bool_type);
   SM->add_connection(clock_sign, clock_port);
   SM->add_connection(clock_sign, clock_scheduler);

   structural_objectRef reset_scheduler = scheduler_mod->find_member(RESET_PORT_NAME,port_o_K,scheduler_mod);
   structural_objectRef reset_sign=SM->add_sign("reset_scheduler_signal", circuit, bool_type);
   SM->add_connection(reset_sign, reset_port);
   SM->add_connection(reset_sign, reset_scheduler);

   structural_objectRef done_scheduler = scheduler_mod->find_member(DONE_PORT_NAME,port_o_K,scheduler_mod);
   structural_objectRef done_datapath = circuit->find_member(DONE_PORT_NAME,port_o_K,circuit);
   structural_objectRef done_sign=SM->add_sign("done_scheduler_signal", circuit, bool_type);
   SM->add_connection(done_sign, done_datapath);
   SM->add_connection(done_sign, done_scheduler);

   structural_objectRef request_accepted_scheduler = scheduler_mod->find_member(STR(REQUEST_ACCEPTED),port_o_K,scheduler_mod);
   structural_objectRef request_accepted_datapath = circuit->find_member(STR(REQUEST_ACCEPTED),port_o_K,circuit);
   structural_objectRef request_accepted_sign=SM->add_sign(STR(REQUEST_ACCEPTED)+"_signal", circuit, bool_type);
   SM->add_connection(request_accepted_sign, request_accepted_datapath);
   SM->add_connection(request_accepted_sign, request_accepted_scheduler);

   structural_objectRef task_finished_scheduler = scheduler_mod->find_member(STR(TASK_FINISHED),port_o_K,scheduler_mod);
   structural_objectRef task_finished_datapath = circuit->find_member(STR(TASK_FINISHED),port_o_K,circuit);
   structural_objectRef task_finished_sign=SM->add_sign(STR(TASK_FINISHED)+"_signal", circuit, bool_type);
   SM->add_connection(task_finished_sign, task_finished_datapath);
   SM->add_connection(task_finished_sign, task_finished_scheduler);

   structural_objectRef done_request_scheduler = scheduler_mod->find_member(STR(DONE_REQUEST),port_o_K,scheduler_mod);
   structural_objectRef done_request_datapath = circuit->find_member(STR(DONE_REQUEST),port_o_K,circuit);
   structural_objectRef done_request_sign=SM->add_sign(STR(DONE_REQUEST)+"_signal", circuit, bool_type);
   SM->add_connection(done_request_sign, done_request_datapath);
   SM->add_connection(done_request_sign, done_request_scheduler);
}

void fu_binding_cs::instantiate_suspension_component(const HLS_managerRef HLSMgr, const hlsRef HLS)
{
   structural_type_descriptorRef bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", 0));
   const structural_managerRef SM = HLS->datapath;
   const structural_objectRef circuit = SM->get_circ();
   structural_objectRef suspensionOr = SM->add_module_from_technology_library("suspensionOr", OR_GATE_STD, HLS->HLS_T->get_technology_manager()->get_library(OR_GATE_STD), circuit, HLS->HLS_T->get_technology_manager());
   structural_objectRef port_in_or = suspensionOr->find_member("in", port_o_K, suspensionOr);
   structural_objectRef port_out_or = suspensionOr->find_member("out1", port_o_K, suspensionOr);
   //search in module and find one with suspension
   unsigned int num_suspension=0;
   unsigned int n_elements = GetPointer<module>(circuit)->get_internal_objects_size();
   unsigned int i=0;
   for(i=0;i<n_elements;i++)
   {
      structural_objectRef curr_gate = GetPointer<module>(circuit)->get_internal_object(i);
      if(curr_gate->find_member(STR(SUSPENSION), port_o_K, curr_gate)!=NULL)
         ++num_suspension;
   }
   if(num_suspension>0)
   {
      GetPointer<port_o>(port_in_or)->add_n_ports(num_suspension+2, suspensionOr);
      for(i=0;i<n_elements;i++)
      {
         structural_objectRef curr_gate = GetPointer<module>(circuit)->get_internal_object(i);
         structural_objectRef port_suspension_module = curr_gate->find_member(STR(SUSPENSION), port_o_K, curr_gate);
         if(port_suspension_module!=NULL)
         {
            structural_objectRef suspension_sign=SM->add_sign(STR(SUSPENSION)+"_signal"+STR(i), circuit, bool_type);
            SM->add_connection(port_suspension_module, suspension_sign);
            SM->add_connection(suspension_sign, GetPointer<port_o>(port_in_or)->get_port(i+2));
         }
      }
   }
   else
   {
      GetPointer<port_o>(port_in_or)->add_n_ports(2, suspensionOr);
   }
   for(unsigned int j = 0; j < GetPointer<module>(circuit)->get_in_port_size(); j++)
   {
      structural_objectRef port_i = GetPointer<module>(circuit)->get_in_port(j);
      std::string port_name = GetPointer<port_o>(port_i)->get_id();
      if(port_name.substr(0,8).compare("sel_LOAD")==0)
      {
         SM->add_connection(port_i, GetPointer<port_o>(port_in_or)->get_port(0));
      }
      if(port_name.substr(0,9).compare("sel_STORE")==0)
      {
         SM->add_connection(port_i, GetPointer<port_o>(port_in_or)->get_port(1));
      }
   }
   connectOutOr(HLSMgr, HLS, port_out_or);
}

void fu_binding_cs::connectOutOr(const HLS_managerRef HLSMgr, const hlsRef HLS, structural_objectRef port_out_or)
{
   const structural_managerRef SM = HLS->datapath;
   const structural_objectRef circuit = SM->get_circ();
   auto omp_functions = GetPointer<OmpFunctions>(HLSMgr->Rfuns);
   if(omp_functions->kernel_functions.find(HLS->functionId) != omp_functions->kernel_functions.end())
   {
      structural_objectRef scheduler = circuit->find_member("scheduler_kernel", component_o_K, circuit);
      structural_type_descriptorRef bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", 0));
      structural_objectRef suspension_scheduler = scheduler->find_member(STR(SUSPENSION),port_o_K,circuit);
      structural_objectRef suspension_sign=SM->add_sign(STR(SUSPENSION)+"_signal", circuit, bool_type);
      SM->add_connection(port_out_or, suspension_sign);
      SM->add_connection(suspension_sign, suspension_scheduler);
   }
   else
   {
      structural_objectRef suspension_datapath = circuit->find_member(STR(SUSPENSION),port_o_K,circuit);
      SM->add_connection(port_out_or, suspension_datapath);
   }
}

void fu_binding_cs::manage_memory_ports_parallel_chained(const HLS_managerRef HLSMgr, const structural_managerRef SM, const std::set<structural_objectRef> &memory_modules, const structural_objectRef circuit, const hlsRef HLS, unsigned int & _unique_id)
{
   auto omp_functions = GetPointer<OmpFunctions>(HLSMgr->Rfuns);
   if(omp_functions->kernel_functions.find(HLS->functionId) != omp_functions->kernel_functions.end())
   {
      manage_memory_ports_parallel_chained_kernel(SM, memory_modules, circuit, HLS, _unique_id);
   }
   else if(omp_functions->hierarchical_functions.find(HLS->functionId) != omp_functions->hierarchical_functions.end())
   {
      manage_memory_ports_parallel_chained_hierarchical(SM, memory_modules, circuit, HLS, _unique_id);
   }
   else
      THROW_UNREACHABLE("only kernel, parallel and hierarchical should call this method");
}

void fu_binding_cs::manage_memory_ports_parallel_chained_kernel(const structural_managerRef SM, const std::set<structural_objectRef> &memory_modules, const structural_objectRef circuit, const hlsRef HLS, unsigned int & _unique_id)
{
   std::map<structural_objectRef, std::set<structural_objectRef> > primary_outs;
   structural_objectRef cir_port;
   structural_objectRef sche_port;
   structural_objectRef scheduler = circuit->find_member("scheduler_kernel", component_o_K, circuit);

   for(unsigned int j = 0; j < GetPointer<module>(scheduler)->get_in_port_size(); j++) //connect input datapath memory_port with scheduler
   {
      structural_objectRef port_i = GetPointer<module>(scheduler)->get_in_port(j);
      if(GetPointer<port_o>(port_i)->get_is_memory() && (!GetPointer<port_o>(port_i)->get_is_global()) && (!GetPointer<port_o>(port_i)->get_is_extern()))
      {
         std::string port_name = GetPointer<port_o>(port_i)->get_id();
         cir_port = circuit->find_member(port_name.erase(0,3), port_i->get_kind(), circuit); //erase IN_ from port name
         THROW_ASSERT(!cir_port || GetPointer<port_o>(cir_port), "should be a port or null");
         if(!cir_port)
         {
            if(port_i->get_kind() == port_vector_o_K)
               cir_port = SM->add_port_vector(port_name, port_o::IN, GetPointer<port_o>(port_i)->get_ports_size(), circuit, port_i->get_typeRef());
            else
               cir_port = SM->add_port(port_name, port_o::IN, circuit, port_i->get_typeRef());
            port_o::fix_port_properties(port_i, cir_port);
            SM->add_connection(cir_port,port_i);
         }
         else
         {
            SM->add_connection(cir_port,port_i);
         }
      }
   }

   for (std::set<structural_objectRef>::iterator i = memory_modules.begin(); i != memory_modules.end(); i++)   //connect scheduler with memory_modules
   {
      for(unsigned int j = 0; j < GetPointer<module>(*i)->get_in_port_size(); j++)
      {
         structural_objectRef port_i = GetPointer<module>(*i)->get_in_port(j);
         if(GetPointer<port_o>(port_i)->get_is_memory() && (!GetPointer<port_o>(port_i)->get_is_global()) && (!GetPointer<port_o>(port_i)->get_is_extern()))
         {
            std::string port_name = GetPointer<port_o>(port_i)->get_id();
            sche_port = scheduler->find_member(port_name, port_i->get_kind(), scheduler);
            structural_objectRef scheMemorySign=SM->add_sign_vector(port_name, 1, circuit, port_i->get_typeRef());
            THROW_ASSERT(GetPointer<port_o>(sche_port), "should be a port");
            SM->add_connection(scheMemorySign, sche_port);   //from scheduler to every memory unit
            SM->add_connection(scheMemorySign, port_i);
         }
      }
   }
   for (std::set<structural_objectRef>::iterator i = memory_modules.begin(); i != memory_modules.end(); i++)   //connect scheduler with memory_modules
   {
      for(unsigned int j = 0; j < GetPointer<module>(*i)->get_out_port_size(); j++)   //connect memory_modules with scheduler
      {
         structural_objectRef port_i = GetPointer<module>(*i)->get_out_port(j);
         if(GetPointer<port_o>(port_i)->get_is_memory() && (!GetPointer<port_o>(port_i)->get_is_global()) && (!GetPointer<port_o>(port_i)->get_is_extern()))
         {
            std::string port_name = GetPointer<port_o>(port_i)->get_id();
            sche_port = scheduler->find_member(port_name, port_i->get_kind(), circuit);
            THROW_ASSERT(!sche_port || GetPointer<port_o>(sche_port), "should be a port or null");
            if(!sche_port)
            {
               if(port_i->get_kind() == port_vector_o_K)
                  sche_port = SM->add_port_vector(port_name, port_o::OUT, GetPointer<port_o>(port_i)->get_ports_size(), circuit, port_i->get_typeRef());
               else
                  sche_port = SM->add_port(port_name, port_o::OUT, circuit, port_i->get_typeRef());
               port_o::fix_port_properties(port_i, sche_port);
            }
            primary_outs[sche_port].insert(port_i);
         }
      }
   }
   join_merge_split(SM, HLS, primary_outs, circuit, _unique_id);

   for(unsigned int j = 0; j < GetPointer<module>(scheduler)->get_out_port_size(); j++) //connect scheduler with datapath
   {
      structural_objectRef port_i = GetPointer<module>(scheduler)->get_out_port(j);
      if(GetPointer<port_o>(port_i)->get_is_memory() && (!GetPointer<port_o>(port_i)->get_is_global()) && (!GetPointer<port_o>(port_i)->get_is_extern()))
      {
         std::string port_name = GetPointer<port_o>(port_i)->get_id();
         cir_port = circuit->find_member(port_name.erase(0,4), port_i->get_kind(), circuit); //erase OUT_ from port name
         THROW_ASSERT(!cir_port || GetPointer<port_o>(cir_port), "should be a port or null");
         if(!cir_port)
         {
            if(port_i->get_kind() == port_vector_o_K)
               cir_port = SM->add_port_vector(port_name, port_o::OUT, GetPointer<port_o>(port_i)->get_ports_size(), circuit, port_i->get_typeRef());
            else
               cir_port = SM->add_port(port_name, port_o::OUT, circuit, port_i->get_typeRef());
            port_o::fix_port_properties(port_i, cir_port);
         }
         SM->add_connection(port_i, cir_port);
      }
   }
}

void fu_binding_cs::manage_memory_ports_parallel_chained_hierarchical(const structural_managerRef SM, const std::set<structural_objectRef> &memory_modules, const structural_objectRef circuit, const hlsRef HLS, unsigned int & _unique_id)
{
   std::map<structural_objectRef, std::set<structural_objectRef> > primary_outs;
   structural_objectRef cir_port;
   for (std::set<structural_objectRef>::iterator i = memory_modules.begin(); i != memory_modules.end(); i++)
   {
      for(unsigned int j = 0; j < GetPointer<module>(*i)->get_in_port_size(); j++)
      {
         structural_objectRef port_i = GetPointer<module>(*i)->get_in_port(j);
         if(GetPointer<port_o>(port_i)->get_is_memory() && (!GetPointer<port_o>(port_i)->get_is_global()) && (!GetPointer<port_o>(port_i)->get_is_extern()))
         {
            std::string port_name = GetPointer<port_o>(port_i)->get_id();
            cir_port = circuit->find_member(port_name, port_i->get_kind(), circuit);
            THROW_ASSERT(!cir_port || GetPointer<port_o>(cir_port), "should be a port or null");
            if(!cir_port)
            {
               if(port_i->get_kind() == port_vector_o_K)
                  cir_port = SM->add_port_vector(port_name, port_o::IN, GetPointer<port_o>(port_i)->get_ports_size(), circuit, port_i->get_typeRef());
               else
                  cir_port = SM->add_port(port_name, port_o::IN, circuit, port_i->get_typeRef());
               port_o::fix_port_properties(port_i, cir_port);
               SM->add_connection(GetPointer<port_o>(cir_port)->get_port(0), GetPointer<port_o>(port_i)->get_port(0));  //connect first
               if(GetPointer<port_o>(port_i)->get_ports_size()>1)   //More than 1 channel
               {
                  for(unsigned int num_chan=1;num_chan<parameters->getOption<unsigned int>(OPT_channels_number);num_chan++)
                  {
                      SM->add_connection(GetPointer<port_o>(cir_port)->get_port(num_chan), GetPointer<port_o>(port_i)->get_port(num_chan));  //connect other port one by one
                  }
               }
            }
            else
            {
               SM->add_connection(GetPointer<port_o>(cir_port)->get_port(0), GetPointer<port_o>(port_i)->get_port(0));
               if(GetPointer<port_o>(port_i)->get_ports_size()>1)   //More than 1 channel
               {
                  for(unsigned int num_chan=1;num_chan<parameters->getOption<unsigned int>(OPT_channels_number);num_chan++)
                  {
                     SM->add_connection(GetPointer<port_o>(cir_port)->get_port(num_chan), GetPointer<port_o>(port_i)->get_port(num_chan));  //connect other port one by one
                  }
               }
            }
         }
      }
      for(unsigned int j = 0; j < GetPointer<module>(*i)->get_out_port_size(); j++)
      {
         structural_objectRef port_i = GetPointer<module>(*i)->get_out_port(j);
         if(GetPointer<port_o>(port_i)->get_is_memory() && (!GetPointer<port_o>(port_i)->get_is_global()) && (!GetPointer<port_o>(port_i)->get_is_extern()))
         {
            std::string port_name = GetPointer<port_o>(port_i)->get_id();
            cir_port = circuit->find_member(port_name, port_i->get_kind(), circuit);
            THROW_ASSERT(!cir_port || GetPointer<port_o>(cir_port), "should be a port or null");
            if(!cir_port)
            {
               if(port_i->get_kind() == port_vector_o_K)
                  cir_port = SM->add_port_vector(port_name, port_o::OUT, GetPointer<port_o>(port_i)->get_ports_size(), circuit, port_i->get_typeRef());
               else
                  cir_port = SM->add_port(port_name, port_o::OUT, circuit, port_i->get_typeRef());
               port_o::fix_port_properties(port_i, cir_port);
            }
            primary_outs[GetPointer<port_o>(cir_port)->get_port(0)].insert(GetPointer<port_o>(port_i)->get_port(0));    //merge first cell of vector
            if(GetPointer<port_o>(port_i)->get_ports_size()>1)   //More than 1 channel
            {
               for(unsigned int num_chan=1;num_chan<parameters->getOption<unsigned int>(OPT_channels_number);num_chan++)
               {
                  SM->add_connection(GetPointer<port_o>(port_i)->get_port(num_chan), GetPointer<port_o>(cir_port)->get_port(num_chan));  //connect other port one by one
               }
            }
         }
      }
   }
   join_merge_split(SM, HLS, primary_outs, circuit, _unique_id);
}
