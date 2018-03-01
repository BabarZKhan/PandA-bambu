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
 *              Copyright (c) 2018 Politecnico di Milano
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
 * @file testbench_values_xml_generation.cpp
 * @brief Class to compute testbench values exploiting only XML values
 *
 * @author Marco Lattuada<marco.lattuada@polimi.it>
 *
 */

///Header include
#include "testbench_values_xml_generation.hpp"

///. include
#include "Parameter.hpp"

///behavior includes
#include "call_graph_manager.hpp"
#include "function_behavior.hpp"

///constants include
#include "testbench_generation_constants.hpp"

///HLS include
#include "hls_manager.hpp"

///HLS/memory includes
#include "memory.hpp"
#include "memory_symbol.hpp"

///HLS/simulation includes
#include "c_initialization_parser.hpp"
#include "SimulationInformation.hpp"
#include "testbench_generation.hpp"
#include "testbench_generation_base_step.hpp"

///tree includes
#include "behavioral_helper.hpp"
#include "tree_helper.hpp"
#include "tree_manager.hpp"

void TestbenchValuesXMLGeneration::Initialize()
{
}

TestbenchValuesXMLGeneration::TestbenchValuesXMLGeneration(const ParameterConstRef _parameters, const HLS_managerRef _hls_manager, const DesignFlowManagerConstRef _design_flow_manager) :
   HLS_step(_parameters, _hls_manager, _design_flow_manager, HLSFlowStep_Type::TESTBENCH_VALUES_XML_GENERATION),
   TM(_hls_manager->get_tree_manager()),
   output_directory(parameters->getOption<std::string>(OPT_output_directory) + "/simulation/")
{
   debug_level = parameters->get_class_debug_level(GET_CLASS(*this));
   if (!boost::filesystem::exists(output_directory))
      boost::filesystem::create_directories(output_directory);
}

TestbenchValuesXMLGeneration::~TestbenchValuesXMLGeneration()
{}

const std::unordered_set<std::tuple<HLSFlowStep_Type, HLSFlowStepSpecializationConstRef, HLSFlowStep_Relationship> > TestbenchValuesXMLGeneration::ComputeHLSRelationships(const DesignFlowStep::RelationshipType relationship_type) const
{
   std::unordered_set<std::tuple<HLSFlowStep_Type, HLSFlowStepSpecializationConstRef, HLSFlowStep_Relationship> > ret;
   switch(relationship_type)
   {
      case DEPENDENCE_RELATIONSHIP:
         {
            ret.insert(std::make_tuple(HLSFlowStep_Type::TEST_VECTOR_PARSER, HLSFlowStepSpecializationConstRef(), HLSFlowStep_Relationship::TOP_FUNCTION));
            break;
         }
      case INVALIDATION_RELATIONSHIP:
         {
            break;
         }
      case PRECEDENCE_RELATIONSHIP:
         {
            break;
         }
      default:
         THROW_UNREACHABLE("");
   }
   return ret;
}

DesignFlowStep_Status TestbenchValuesXMLGeneration::Exec()
{
   const auto top_function_ids = HLSMgr->CGetCallGraphManager()->GetRootFunctions();
   THROW_ASSERT(top_function_ids.size() == 1, "Multiple top function");
   const auto function_id = *(top_function_ids.begin());
   const auto behavioral_helper = HLSMgr->CGetFunctionBehavior(function_id)->CGetBehavioralHelper();
   if (!boost::filesystem::exists(output_directory))
      boost::filesystem::create_directories(output_directory);
   std::string output_file_name = output_directory + STR(STR_CST_testbench_generation_basename) + ".txt";
   std::ofstream output_stream = std::ofstream(output_file_name.c_str(), std::ios::out);
   CInitializationParserRef c_initialization_parser = CInitializationParserRef(new CInitializationParser(output_stream, HLSMgr->get_tree_manager(), behavioral_helper, parameters));

   /// print base address
   unsigned int base_address = HLSMgr->base_address;
   output_stream << "//base address " + STR(base_address) << std::endl;
   std::string trimmed_value;
   for (unsigned int ind = 0; ind < 32; ind++)
      trimmed_value = trimmed_value + (((1LLU << (31 - ind)) & base_address) ? '1' : '0');
   output_stream << "b" + trimmed_value << std::endl;

   const std::map<unsigned int, memory_symbolRef>& mem_vars = HLSMgr->Rmem->get_ext_memory_variables();
   // get the mapping between variables in external memory and their external
   // base address
   std::map<unsigned int, unsigned int> address;
   for (const auto & m : mem_vars)
      address[HLSMgr->Rmem->get_external_base_address(m.first)] = m.first;

   ///This is the lis of memory variables and of pointer parameters
   std::list<unsigned int> mem;
   for (const auto & ma : address)
      mem.push_back(ma.second);


   const auto function_parameters = behavioral_helper->get_parameters();
   for (const auto & function_pointer : function_parameters)
   {
      // if the function has some pointer parameters some memory needs to be
      // reserved for the place where they point to
      if (behavioral_helper->is_a_pointer(function_pointer) && mem_vars.find(function_pointer) == mem_vars.end())
         mem.push_back(function_pointer);
   }
   unsigned int v_idx = 0;

   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Writing initialization of memory variables");

   // For each test vector, for each pointer parameter, the space to be reserved in memory
   CustomMap<unsigned int, CustomMap<unsigned int, size_t> > all_reserved_mem_bytes;
   // loop on the test vectors
   for (const auto & curr_test_vector : HLSMgr->RSim->test_vectors)
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Considering new test vector");
      // loop on the variables in memory
      for (const auto & l : mem)
      {
         std::string param = behavioral_helper->PrintVariable(l);
         if (param[0] == '"')
            param = "@" + STR(l);

         bool is_memory = false;
         std::string test_v = "0";

         ///Initialization of memory variables which are not pointer parameters
         if (mem_vars.find(l) != mem_vars.end() and  std::find(function_parameters.begin(), function_parameters.end(), l) == function_parameters.end())
         {
            if (v_idx > 0 && is_memory)
               continue;//memory has been already initialized
            is_memory = true;
            test_v = TestbenchGenerationBaseStep::print_var_init(TM, l, HLSMgr->Rmem);
         }
         ///Parameter: read initialization from parsed xml
         else if (curr_test_vector.find(param) != curr_test_vector.end())
         {
            test_v = curr_test_vector.find(param)->second;
         }

         ///Retrieve the space to be reserved in memory
         const auto reserved_mem_bytes = [&] () -> size_t
         {
            if(is_memory)
            {
               const auto ret_value = tree_helper::size(TM, l) / 8;
               return ret_value ? ret_value : 0;
            }
            else
            {
               THROW_ASSERT(tree_helper::is_a_pointer(TM, l), "");
               unsigned int base_type = tree_helper::get_type_index(TM, l);
               tree_nodeRef pt_node = TM->get_tree_node_const(base_type);
               return HLSMgr->RSim->param_mem_size.find(v_idx)->second.find(l)->second;
            }
         }();

         all_reserved_mem_bytes[v_idx][l] = reserved_mem_bytes;

         ///Call the parser to translate C initialization to verilog initialization
         c_initialization_parser->Parse(test_v, reserved_mem_bytes, TM->CGetTreeNode(l), TestbenchGeneration_MemoryType::MEMORY_INITIALIZATION);
         size_t next_object_offset = HLSMgr->RSim->param_next_off.find(v_idx)->second.find(l)->second;

         if (next_object_offset > reserved_mem_bytes)
         {
            for(unsigned int padding = 0; padding < next_object_offset - reserved_mem_bytes; padding++)
               output_stream << "m00000000" << std::endl;
         }

      }
      ++v_idx;
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Considered vector");
   }
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Written initialization of memory variables");
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Writing values of parameters");
   v_idx = 0;
   for (const auto & curr_test_vector : HLSMgr->RSim->test_vectors)
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Writing initialization of parameters");
      for (const auto & function_parameter : behavioral_helper->get_parameters())
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Considering parameter " + STR(TM->CGetTreeNode(function_parameter)));
         unsigned int type_id = behavioral_helper->get_type(function_parameter);
         std::string type = behavioral_helper->print_type(type_id);
         std::string param = behavioral_helper->PrintVariable(function_parameter);
         if (behavioral_helper->is_a_pointer(function_parameter))
         {
            std::cerr << v_idx << " " << function_parameter << std::endl;
            std::string memory_addr = STR(HLSMgr->RSim->param_address.at(v_idx).at(function_parameter));
            output_stream << "//parameter: " + behavioral_helper->PrintVariable(function_parameter) << " value: " << memory_addr << std::endl;
            output_stream << "p" << ConvertInBinary(memory_addr, 32, false, false)  << std::endl;
         }
         else
         {
            c_initialization_parser->Parse(curr_test_vector.at(param), tree_helper::size(TM, function_parameter)/8, TM->CGetTreeNode(function_parameter), TestbenchGeneration_MemoryType::INPUT_PARAMETER);
         }
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Considered parameter " + STR(TM->CGetTreeNode(function_parameter)));
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Writing expected content of pointer parameters at the end of the execution");
      for (const auto & function_parameter : behavioral_helper->get_parameters())
      {
         if (behavioral_helper->is_a_pointer(function_parameter))
         {
            std::string param = behavioral_helper->PrintVariable(function_parameter);
            const auto expected_values = [&] () -> std::string
            {
               if(curr_test_vector.find(param + ":output") != curr_test_vector.end())
               {
                  return curr_test_vector.at(param + ":output");
               }
               else
               {
                  return curr_test_vector.at(param);
               }
            }();
            c_initialization_parser->Parse(expected_values, all_reserved_mem_bytes.at(v_idx).at(function_parameter), TM->CGetTreeNode(function_parameter), TestbenchGeneration_MemoryType::OUTPUT_PARAMETER);
            output_stream << "e" << std::endl;
         }
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Written expected content of pointer parameters at the end of the execution");
      const unsigned int return_type_index = behavioral_helper->GetFunctionReturnType(function_id);
      if(return_type_index)
      {
         c_initialization_parser->Parse(curr_test_vector.find("return")->second, tree_helper::size(TM, return_type_index)/8, TM->CGetTreeNode(return_type_index), TestbenchGeneration_MemoryType::RETURN);
      }
      ++v_idx;
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Considered vector");
   }
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Written values of parameters");
   output_stream << "e" << std::endl;
   output_stream.close();
   return DesignFlowStep_Status::SUCCESS;
}

bool TestbenchValuesXMLGeneration::HasToBeExecuted() const
{
   return true;
}
