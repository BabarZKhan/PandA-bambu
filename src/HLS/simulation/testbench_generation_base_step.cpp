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
 * @file testbench_generation_base_step.cpp
 * @brief hls testbench automatic generation
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * @author Marco Minutoli <mminutoli@gmail.com>
 * @author Manuel Beniani <manuel.beniani@gmail.com>
 * @author Christian Pilato <pilato@elet.polimi.it>
 * @author Pietro Fezzardi <pietrofezzardi@gmail.com>
 *
 */
/// Autoheader include
#include "config_HAVE_I386_CLANG4_COMPILER.hpp"
#include "config_HAVE_I386_CLANG5_COMPILER.hpp"
#include "config_HAVE_I386_CLANG6_COMPILER.hpp"
#include "config_HAVE_I386_GCC47_COMPILER.hpp"
#include "config_HAVE_I386_GCC48_COMPILER.hpp"
#include "config_HAVE_I386_GCC49_COMPILER.hpp"
#include "config_HAVE_I386_GCC5_COMPILER.hpp"
#include "config_HAVE_I386_GCC6_COMPILER.hpp"
#include "config_HAVE_I386_GCC7_COMPILER.hpp"
#include "config_HAVE_I386_GCC8_COMPILER.hpp"
#include "config_PACKAGE_BUGREPORT.hpp"
#include "config_PACKAGE_NAME.hpp"
#include "config_PACKAGE_VERSION.hpp"

/// Header include
#include "testbench_generation_base_step.hpp"

#include <utility>

///. include
#include "Parameter.hpp"

/// behavior include
#include "call_graph_manager.hpp"

/// circuit include
#include "structural_manager.hpp"
#include "structural_objects.hpp"

/// design_flows includes
#include "design_flow_graph.hpp"
#include "design_flow_manager.hpp"

/// design_flows/backend/ToC/progModels include
#include "c_backend.hpp"

/// design_flows/backend/ToC include
#include "c_backend_step_factory.hpp"
#include "hls_c_backend_information.hpp"

/// design_flows/backend/ToHDL includes
#include "HDL_manager.hpp"
#include "language_writer.hpp"

/// HLS include
#include "hls.hpp"
#include "hls_constraints.hpp"
#include "hls_manager.hpp"
#include "hls_target.hpp"

/// HLS/binding/module include
#include "fu_binding.hpp"

/// HLS/memory include
#include "memory.hpp"

// include from HLS/simulation
#include "SimulationInformation.hpp"

#if HAVE_FROM_DISCREPANCY_BUILT
// include from HLS/vcd
#include "Discrepancy.hpp"
#endif

/// technology/physical_library
#include "technology_wishbone.hpp"

/// tree include
#include "behavioral_helper.hpp"
#include "tree_helper.hpp"
#include "tree_manager.hpp"
#include "tree_node.hpp"
#include "tree_reindex.hpp"

/// utility include
#include "fileIO.hpp"

/// wrapper/treegcc include
#include "gcc_wrapper.hpp"

TestbenchGenerationBaseStep::TestbenchGenerationBaseStep(const ParameterConstRef _parameters, const HLS_managerRef _HLSMgr, const DesignFlowManagerConstRef _design_flow_manager, const HLSFlowStep_Type _hls_flow_step_type, std::string _c_testbench_basename)
    : HLS_step(_parameters, _HLSMgr, _design_flow_manager, _hls_flow_step_type),
      writer(language_writer::create_writer(HDLWriter_Language::VERILOG, _HLSMgr->get_HLS_target()->get_technology_manager(), _parameters)),
      mod(nullptr),
      target_period(0.0),
      output_directory(parameters->getOption<std::string>(OPT_output_directory) + "/simulation/"),
      c_testbench_basename(std::move(_c_testbench_basename))
{
   if(!boost::filesystem::exists(output_directory)) boost::filesystem::create_directories(output_directory);
}

TestbenchGenerationBaseStep::~TestbenchGenerationBaseStep() = default;

const std::unordered_set<std::tuple<HLSFlowStep_Type, HLSFlowStepSpecializationConstRef, HLSFlowStep_Relationship>> TestbenchGenerationBaseStep::ComputeHLSRelationships(const DesignFlowStep::RelationshipType relationship_type) const
{
   std::unordered_set<std::tuple<HLSFlowStep_Type, HLSFlowStepSpecializationConstRef, HLSFlowStep_Relationship>> ret;
   switch(relationship_type)
   {
      case DEPENDENCE_RELATIONSHIP:
      {
         ret.insert(std::make_tuple(HLSFlowStep_Type::TEST_VECTOR_PARSER, HLSFlowStepSpecializationConstRef(), HLSFlowStep_Relationship::TOP_FUNCTION));
         ret.insert(std::make_tuple(HLSFlowStep_Type::TESTBENCH_MEMORY_ALLOCATION, HLSFlowStepSpecializationConstRef(), HLSFlowStep_Relationship::TOP_FUNCTION));
#if HAVE_VCD_BUILT
         if(parameters->isOption(OPT_discrepancy) and parameters->getOption<bool>(OPT_discrepancy)) { ret.insert(std::make_tuple(HLSFlowStep_Type::VCD_SIGNAL_SELECTION, HLSFlowStepSpecializationConstRef(), HLSFlowStep_Relationship::TOP_FUNCTION)); }
#endif
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

void TestbenchGenerationBaseStep::ComputeRelationships(DesignFlowStepSet& design_flow_step_set, const DesignFlowStep::RelationshipType relationship_type)
{
   HLS_step::ComputeRelationships(design_flow_step_set, relationship_type);

   switch(relationship_type)
   {
      case DEPENDENCE_RELATIONSHIP:
      {
         const auto* c_backend_factory = GetPointer<const CBackendStepFactory>(design_flow_manager.lock()->CGetDesignFlowStepFactory("CBackend"));

         CBackend::Type hls_c_backend_type;
#if HAVE_HLS_BUILT
         if(parameters->isOption(OPT_discrepancy) and parameters->getOption<bool>(OPT_discrepancy)) { hls_c_backend_type = CBackend::CB_DISCREPANCY_ANALYSIS; }
         else
#endif
         {
            hls_c_backend_type = CBackend::CB_HLS;
            if(parameters->isOption(OPT_pretty_print))
            {
               const auto design_flow_graph = design_flow_manager.lock()->CGetDesignFlowGraph();
               const auto* c_backend_step_factory = GetPointer<const CBackendStepFactory>(design_flow_manager.lock()->CGetDesignFlowStepFactory("CBackend"));
               const std::string output_file_name = parameters->getOption<std::string>(OPT_pretty_print);
               const vertex c_backend_vertex = design_flow_manager.lock()->GetDesignFlowStep(CBackend::ComputeSignature(CBackend::CB_SEQUENTIAL));
               const DesignFlowStepRef c_backend_step =
                   c_backend_vertex ? design_flow_graph->CGetDesignFlowStepInfo(c_backend_vertex)->design_flow_step : c_backend_step_factory->CreateCBackendStep(CBackend::CB_SEQUENTIAL, output_file_name, CBackendInformationConstRef());
               design_flow_step_set.insert(c_backend_step);
            }
         }

         const DesignFlowStepRef hls_c_backend_step =
             c_backend_factory->CreateCBackendStep(hls_c_backend_type, output_directory + c_testbench_basename + ".c", CBackendInformationConstRef(new HLSCBackendInformation(output_directory + c_testbench_basename + ".txt", HLSMgr)));
         design_flow_step_set.insert(hls_c_backend_step);
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
      {
         THROW_UNREACHABLE("");
         break;
      }
   }
}

void TestbenchGenerationBaseStep::Initialize()
{
   const auto top_function_ids = HLSMgr->CGetCallGraphManager()->GetRootFunctions();
   THROW_ASSERT(top_function_ids.size() == 1, "Multiple top functions");
   const auto top_function_id = *(top_function_ids.begin());
   const auto top_hls = HLSMgr->get_HLS(top_function_id);
   cir = top_hls->top->get_circ();
   THROW_ASSERT(GetPointer<const module>(cir), "Not a module");
   mod = GetPointer<const module>(cir);
   target_period = top_hls->HLS_C->get_clock_period();
   testbench_basename = "testbench_" + cir->get_id();
}

DesignFlowStep_Status TestbenchGenerationBaseStep::Exec()
{
   /*
    * execute the C testbench necessary to print out the expected results.
    * they are used to generate the hdl testbench, which checks the expected
    * results against the real ones obtained by the simulation of the
    * synthesized hdl
    */
   exec_C_testbench();
   HLSMgr->RSim->filename_bench = (parameters->getOption<std::string>(OPT_simulator) == "VERILATOR") ? verilator_testbench() : create_HDL_testbench(false);
   return DesignFlowStep_Status::SUCCESS;
}

std::string TestbenchGenerationBaseStep::print_var_init(const tree_managerConstRef TreeM, unsigned int var, const memoryRef mem)
{
   std::vector<std::string> init_els;
   const tree_nodeRef& tn = TreeM->get_tree_node_const(var);
   tree_nodeRef init_node;
   auto* vd = GetPointer<var_decl>(tn);
   if(vd && vd->init) init_node = GET_NODE(vd->init);

   if(init_node && (!GetPointer<constructor>(init_node) || GetPointer<constructor>(init_node)->list_of_idx_valu.size())) { fu_binding::write_init(TreeM, tn, init_node, init_els, mem, 0); }
   else if(GetPointer<string_cst>(tn) || GetPointer<integer_cst>(tn) || GetPointer<real_cst>(tn))
   {
      fu_binding::write_init(TreeM, tn, tn, init_els, mem, 0);
   }
   else if(!GetPointer<gimple_call>(tn))
   {
      if(tree_helper::is_an_array(TreeM, var))
      {
         unsigned int type_index;
         tree_helper::get_type_node(tn, type_index);
         unsigned int data_bitsize = tree_helper::get_array_data_bitsize(TreeM, type_index);
         unsigned int num_elements = tree_helper::get_array_num_elements(TreeM, type_index);
         std::string value;
         for(unsigned int l = 0; l < num_elements; l++)
         {
            value = "";
            for(unsigned int i = 0; i < data_bitsize; i++) value += "0";
            init_els.push_back(value);
         }
      }
      else
      {
         unsigned int data_bitsize = tree_helper::size(TreeM, var);
         std::string value;
         for(unsigned int i = 0; i < data_bitsize; i++) value += "0";
         init_els.push_back(value);
      }
   }
   std::string init;
   for(unsigned int l = 0; l < init_els.size(); l++)
   {
      if(l) init += ",";
      init += init_els[l];
   }
   return init;
}

void TestbenchGenerationBaseStep::exec_C_testbench()
{
   INDENT_DBG_MEX(DEBUG_LEVEL_MINIMUM, debug_level, "-->Executing C testbench");
   const GccWrapperConstRef gcc_wrapper(new GccWrapper(parameters, parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler), GccWrapper_OptimizationSet::O0));
   std::string compiler_flags = "-fwrapv -ffloat-store -flax-vector-conversions -msse2 -mfpmath=sse -D'__builtin_bambu_time_start()=' -D'__builtin_bambu_time_stop()=' ";
#if HAVE_I386_CLANG4_COMPILER
   if(parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG4)
      compiler_flags = "-fwrapv -flax-vector-conversions -msse2 -mfpmath=sse -D'__builtin_bambu_time_start()=' -D'__builtin_bambu_time_stop()=' ";
#endif
#if HAVE_I386_CLANG5_COMPILER
   if(parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG5)
      compiler_flags = "-fwrapv -flax-vector-conversions -msse2 -mfpmath=sse -D'__builtin_bambu_time_start()=' -D'__builtin_bambu_time_stop()=' ";
#endif
#if HAVE_I386_CLANG6_COMPILER
   if(parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG6)
      compiler_flags = "-fwrapv -flax-vector-conversions -msse2 -mfpmath=sse -D'__builtin_bambu_time_start()=' -D'__builtin_bambu_time_stop()=' ";
#endif

   if(!parameters->isOption(OPT_input_format) || parameters->getOption<Parameters_FileFormat>(OPT_input_format) == Parameters_FileFormat::FF_C || parameters->isOption(OPT_pretty_print))
#if HAVE_I386_CLANG4_COMPILER
      if(parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) != GccWrapper_CompilerTarget::CT_I386_CLANG4)
#endif
#if HAVE_I386_CLANG5_COMPILER
         if(parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) != GccWrapper_CompilerTarget::CT_I386_CLANG5)
#endif
#if HAVE_I386_CLANG6_COMPILER
            if(parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) != GccWrapper_CompilerTarget::CT_I386_CLANG6)
#endif
               compiler_flags += " -fexcess-precision=standard ";
   if(parameters->isOption(OPT_testbench_extra_gcc_flags)) compiler_flags += " " + parameters->getOption<std::string>(OPT_testbench_extra_gcc_flags) + " ";
   if(parameters->isOption(OPT_discrepancy) and parameters->getOption<bool>(OPT_discrepancy))
   {
      if(false
#if HAVE_I386_GCC48_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC48
#endif
#if HAVE_I386_GCC49_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC49
#endif
#if HAVE_I386_GCC5_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC5
#endif
#if HAVE_I386_GCC6_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC6
#endif
#if HAVE_I386_GCC7_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC7
#endif
#if HAVE_I386_GCC8_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC8
#endif
#if HAVE_I386_CLANG4_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG4
#endif
#if HAVE_I386_CLANG5_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG5
#endif
#if HAVE_I386_CLANG6_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG6
#endif
      )
      {
         compiler_flags += " -g -fsanitize=address -fno-omit-frame-pointer -fno-common ";
      }
      if(false
#if HAVE_I386_GCC5_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC5
#endif
#if HAVE_I386_GCC6_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC6
#endif
#if HAVE_I386_GCC7_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC7
#endif
#if HAVE_I386_GCC8_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC8
#endif
#if HAVE_I386_CLANG4_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG4
#endif
#if HAVE_I386_CLANG5_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG5
#endif
#if HAVE_I386_CLANG6_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG6
#endif
      )
      {
         compiler_flags += " -fsanitize=undefined -fsanitize-recover=undefined ";
      }
   }
   if(parameters->isOption(OPT_gcc_optimizations))
   {
      const auto gcc_parameters = parameters->getOption<const CustomSet<std::string>>(OPT_gcc_optimizations);
      if(gcc_parameters.find("tree-vectorize") != gcc_parameters.end())
      {
         boost::replace_all(compiler_flags, "-msse2", "");
         compiler_flags += "-m32 ";
      }
   }
   // setup source files
   std::list<std::string> file_sources;
   file_sources.push_front(output_directory + c_testbench_basename + ".c");
   // add source files to interface with python golden reference, if any
   std::string exec_name = output_directory + "test";
   if(parameters->isOption(OPT_no_parse_c_python))
   {
      const auto no_parse_files = parameters->getOption<const CustomSet<std::string>>(OPT_no_parse_c_python);
      for(const auto& no_parse_file : no_parse_files) { file_sources.push_back(no_parse_file); }
   }
   // compute top function name and use it to setup the artificial main for cosimulation
   const auto top_function_ids = HLSMgr->CGetCallGraphManager()->GetRootFunctions();
   THROW_ASSERT(top_function_ids.size() == 1, "Multiple top functions");
   const auto top_function_id = *(top_function_ids.begin());
   const auto top_function_name = HLSMgr->CGetFunctionBehavior(top_function_id)->CGetBehavioralHelper()->get_function_name();
   if(parameters->isOption(OPT_discrepancy) and parameters->getOption<bool>(OPT_discrepancy))
   {
      /// Nothing to do
   }
   else if(top_function_name != "main")
   {
      if(parameters->isOption(OPT_pretty_print)) { file_sources.push_back(parameters->getOption<std::string>(OPT_pretty_print)); }
      else
      {
         compiler_flags += " -Wl,--allow-multiple-definition ";
         for(const auto& input_file : parameters->getOption<const CustomSet<std::string>>(OPT_input_file)) { file_sources.push_back(input_file); }
      }
   }
   else
   {
      const std::string main_file_name = output_directory + "main_exec";
      CustomSet<std::string> main_sources;
      if(parameters->isOption(OPT_pretty_print)) { main_sources.insert(parameters->getOption<std::string>(OPT_pretty_print)); }
      else
      {
         for(const auto& input_file : parameters->getOption<const CustomSet<std::string>>(OPT_input_file)) { main_sources.insert(input_file); }
      }
      gcc_wrapper->CreateExecutable(main_sources, main_file_name, compiler_flags);
   }
   // compile the source file to get an executable
   gcc_wrapper->CreateExecutable(file_sources, exec_name, compiler_flags);
   // set some parameters for redirection of discrepancy statistics
   std::string c_stdout_file = "";
   if(parameters->isOption(OPT_discrepancy) and parameters->getOption<bool>(OPT_discrepancy)) c_stdout_file = output_directory + "dynamic_discrepancy_stats";
   // executing the test to generate inputs and outputs values
   if(parameters->isOption(OPT_discrepancy) and parameters->getOption<bool>(OPT_discrepancy))
   {
      if(false
#if HAVE_I386_GCC49_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC49
#endif
#if HAVE_I386_GCC5_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC5
#endif
#if HAVE_I386_GCC6_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC6
#endif
#if HAVE_I386_GCC7_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC7
#endif
#if HAVE_I386_GCC8_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_GCC8
#endif
#if HAVE_I386_CLANG4_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG4
#endif
#if HAVE_I386_CLANG5_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG5
#endif
#if HAVE_I386_CLANG6_COMPILER
         or parameters->getOption<GccWrapper_CompilerTarget>(OPT_default_compiler) == GccWrapper_CompilerTarget::CT_I386_CLANG6
#endif
      )
      {
         exec_name.insert(0, "ASAN_OPTIONS='symbolize=1:redzone=2048' ");
      }
   }
   int ret = PandaSystem(parameters, exec_name, c_stdout_file);
   if(IsError(ret)) { THROW_ERROR("Error in generating the expected test results"); }
   INDENT_DBG_MEX(DEBUG_LEVEL_MINIMUM, debug_level, "<--Executed C testbench");
}

std::string TestbenchGenerationBaseStep::verilator_testbench() const
{
   if(not parameters->getOption<bool>(OPT_generate_testbench)) return "";
   std::string simulation_values_path = output_directory + c_testbench_basename + ".txt";

   PRINT_DBG_MEX(DEBUG_LEVEL_MINIMUM, debug_level, "  . Generation of the Verilator testbench");

   if(not boost::filesystem::exists(simulation_values_path)) THROW_ERROR("Error in generating Verilator testbench, values file missing!");

   std::string fileName = write_verilator_testbench(simulation_values_path);

   PRINT_DBG_MEX(DEBUG_LEVEL_MINIMUM, debug_level, "  . End of the Verilator testbench");

   return fileName;
}

std::string TestbenchGenerationBaseStep::write_verilator_testbench(const std::string& input_file) const
{
   // Generate the testbench

   const tree_managerRef TreeM = HLSMgr->get_tree_manager();

   this->write_underlying_testbench(input_file, false, false, TreeM);
   std::string file_name = output_directory + testbench_basename + "_tb.v";
   writer->WriteFile(file_name);
   std::ostringstream os;
   simple_indent PP('{', '}', 3);

   // Creating output file
   std::string fileName = output_directory + testbench_basename + "_main.cpp";
   std::ofstream fileOut(fileName.c_str(), std::ios::out);

   std::string top_fname = mod->get_typeRef()->id_type;
   PP(os, "#include <string>\n");
   PP(os, "#include <verilated.h>\n");
   PP(os, "#include \"V" + top_fname + "_tb.h\"\n");
   PP(os, "\n");
   PP(os, "#if VM_TRACE\n");
   PP(os, "# include <verilated_vcd_c.h>\n");
   PP(os, "#endif\n");
   PP(os, "\n");
   PP(os, "\n");
   PP(os, "#define SIMULATION_MAX " + STR(parameters->getOption<int>(OPT_max_sim_cycles)) + "\n\n");
   PP(os, "static const double CLOCK_PERIOD =" + boost::lexical_cast<std::string>(target_period) + ";\n");
   PP(os, "static const double HALF_CLOCK_PERIOD = CLOCK_PERIOD/2;\n");
   PP(os, "\n");
   PP(os, "double main_time = 0;\n");
   PP(os, "\n");
   PP(os, "double sc_time_stamp ()  {return main_time;}\n");
   PP(os, "\n");
   PP(os, "int main (int argc, char **argv, char **env)\n");
   PP(os, "{\n");
   PP(os, "   V" + top_fname + "_tb *top;\n");
   PP(os, "\n");
   PP(os, "   std::string vcd_output_filename = \"" + output_directory + "test.vcd\";\n");
   PP(os, "   Verilated::commandArgs(argc, argv);\n");
   PP(os, "   Verilated::debug(0);\n");
   PP(os, "   top = new V" + top_fname + "_tb;\n");
   PP(os, "   \n");
   PP(os, "   \n");
   PP(os, "   #if VM_TRACE\n");
   PP(os, "   Verilated::traceEverOn(true);\n");
   PP(os, "   VerilatedVcdC* tfp = new VerilatedVcdC;\n");
   PP(os, "   #endif\n");
   PP(os, "   main_time=0;\n");
   PP(os, "#if VM_TRACE\n");
   PP(os, "   top->trace (tfp, 99);\n");
   PP(os, "   tfp->open (vcd_output_filename.c_str());\n");
   PP(os, "#endif\n");
   PP(os, "   int cycleCounter = 0;\n");
   PP(os, "   top->" + std::string(CLOCK_PORT_NAME) + " = 0;\n");
   PP(os, "   while (!Verilated::gotFinish() && cycleCounter < SIMULATION_MAX)\n");
   PP(os, "   {\n");
   PP(os, "     top->" + std::string(CLOCK_PORT_NAME) + " = top->" + std::string(CLOCK_PORT_NAME) + " == 0 ? 1 : 0;\n");
   PP(os, "     top->eval();\n");
   PP(os, "#if VM_TRACE\n");
   PP(os, "     if (tfp) tfp->dump (main_time);\n");
   PP(os, "#endif\n");
   PP(os, "     main_time += HALF_CLOCK_PERIOD;\n");
   PP(os, "     cycleCounter++;\n");
   PP(os, "   }\n");
   PP(os, "#if VM_TRACE\n");
   PP(os, "   if (tfp) tfp->dump (main_time);\n");
   PP(os, "#endif\n");
   PP(os, "   top->final();\n");
   PP(os, "   #if VM_TRACE\n");
   PP(os, "   tfp->close();\n");
   PP(os, "   delete tfp;\n");
   PP(os, "   #endif\n");
   PP(os, "   delete top;\n");
   PP(os, "   exit(0L);\n");
   PP(os, "}");

   fileOut << os.str() << std::endl;
   fileOut.close();

   return fileName;
}

std::string TestbenchGenerationBaseStep::create_HDL_testbench(bool xilinx_isim) const
{
   if(!parameters->getOption<bool>(OPT_generate_testbench)) return "";
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Creating HDL testbench");
   const tree_managerRef TreeM = HLSMgr->get_tree_manager();

   std::string simulation_values_path = output_directory + c_testbench_basename + ".txt";
   bool generate_vcd_output = (parameters->isOption(OPT_generate_vcd) and parameters->getOption<bool>(OPT_generate_vcd)) or (parameters->isOption(OPT_discrepancy) and parameters->getOption<bool>(OPT_discrepancy));

   std::string file_name = output_directory + testbench_basename + writer->get_extension();

   PRINT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "  writing testbench");
   writer->write_comment(std::string("File automatically generated by: ") + PACKAGE_NAME + " framework version=" + PACKAGE_VERSION + "\n");
   writer->write_comment(std::string("Send any bug to: ") + PACKAGE_BUGREPORT + "\n");
   writer->WriteLicense();
   // write testbench for simulation
   this->write_hdl_testbench(simulation_values_path, generate_vcd_output, xilinx_isim, TreeM);
   writer->WriteFile(file_name);
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Created HDL testbench");
   return file_name;
}

void TestbenchGenerationBaseStep::write_hdl_testbench(std::string simulation_values_path, bool generate_vcd_output, bool xilinx_isim, const tree_managerConstRef TreeM) const
{
   this->write_underlying_testbench(simulation_values_path, generate_vcd_output, xilinx_isim, TreeM);

   writer->write("module " + mod->get_id() + "_tb_top;\n");
   writer->write("reg " + STR(CLOCK_PORT_NAME) + ";\n");
   writer->write("initial\nbegin\n");
   writer->write(STR(CLOCK_PORT_NAME) + " = 1;\n");
   writer->write("end\n");

   write_clock_process();

   writer->write(mod->get_id() + "_tb DUT(." + STR(CLOCK_PORT_NAME) + "(" + STR(CLOCK_PORT_NAME) + "));\n");
   writer->write("endmodule\n");
}

void TestbenchGenerationBaseStep::write_initial_block(const std::string& simulation_values_path, bool withMemory, const tree_managerConstRef TreeM, bool generate_vcd_output) const
{
   begin_initial_block();

   /// VCD output generation (optional)
   std::string vcd_output_filename = output_directory + "test.vcd";
   if(generate_vcd_output)
   {
      writer->write_comment("VCD file generation\n");
      writer->write("$dumpfile(\"" + vcd_output_filename + "\");\n");
      bool simulator_supports_dumpvars_directive = parameters->getOption<std::string>(OPT_simulator) == "MODELSIM" || parameters->getOption<std::string>(OPT_simulator) == "ICARUS";
      bool dump_all_signals = parameters->isOption(OPT_generate_vcd) && parameters->getOption<bool>(OPT_generate_vcd);
      if(dump_all_signals or not simulator_supports_dumpvars_directive
#if HAVE_FROM_DISCREPANCY_BUILT
         or not parameters->isOption(OPT_discrepancy) or not parameters->getOption<bool>(OPT_discrepancy) or HLSMgr->RDiscr->selected_vcd_signals.empty()
#endif
      )
      {
         writer->write("$dumpvars;\n");
      }
#if HAVE_FROM_DISCREPANCY_BUILT
      else
      {
         for(const auto& sig_scope : HLSMgr->RDiscr->selected_vcd_signals)
         {
            /*
             * since the SignalSelectorVisitor used to select the signals is
             * quite optimistic and it is based only on naming conventions on
             * the signals, it can select more signal than needed or even select
             * some signals that are not present. if this happens, asking the
             * simulator to dump the missing signal through the $dumpvars
             * directive would result in an error, aborting the simulation. for
             * this reason we use the dumpvars directive to select only the
             * scopes, and we then print all the signals in the scope, without
             * naming them one-by-one
             */
            std::string sigscope = sig_scope.first;
            boost::replace_all(sigscope, STR(HIERARCHY_SEPARATOR), ".");
            for(const std::string& signame : sig_scope.second) writer->write("$dumpvars(1, " + sigscope + signame + ");\n");
         }
      }
#endif
   }

   /// open file with values
   std::string input_values_filename = simulation_values_path;
   open_value_file(input_values_filename);

   /// open file with results
   std::string result_file = parameters->getOption<std::string>(OPT_simulation_output);
   open_result_file(result_file);

   /// auxiliary variables initialization
   initialize_auxiliary_variables();
   initialize_input_signals(TreeM);
   init_extra_signals(withMemory);
   memory_initialization();
   end_initial_block();
}

void TestbenchGenerationBaseStep::init_extra_signals(bool withMemory) const
{
   if(mod->find_member(RETURN_PORT_NAME, port_o_K, cir))
   {
      writer->write("ex_" + STR(RETURN_PORT_NAME) + " = 0;\n");
      writer->write("registered_" + STR(RETURN_PORT_NAME) + " = 0;\n");
      writer->write("\n");
   }
   if(withMemory)
   {
      structural_objectRef M_Rdata_ram_port = mod->find_member("M_Rdata_ram", port_o_K, cir);
      THROW_ASSERT(M_Rdata_ram_port, "M_Rdata_ram port is missing");
      unsigned int M_Rdata_ram_port_n_ports = M_Rdata_ram_port->get_kind() == port_vector_o_K ? GetPointer<port_o>(M_Rdata_ram_port)->get_ports_size() : 1;
      for(unsigned int i = 0; i < M_Rdata_ram_port_n_ports; ++i) { writer->write("reg_DataReady[" + boost::lexical_cast<std::string>(i) + "] = 0;\n\n"); }
   }
}

void TestbenchGenerationBaseStep::write_output_checks(const tree_managerConstRef TreeM) const
{
   writer->write("always @(negedge " + std::string(CLOCK_PORT_NAME) + ")\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   writer->write("if (start_results_comparison == 1)\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");

   const HLSFlowStep_Type interface_type = parameters->getOption<HLSFlowStep_Type>(OPT_interface_type);
   if(interface_type == HLSFlowStep_Type::MINIMAL_INTERFACE_GENERATION or interface_type == HLSFlowStep_Type::INFERRED_INTERFACE_GENERATION)
   {
      const auto& DesignSignature=HLSMgr->RSim->simulationArgSignature;
      for(auto par: DesignSignature)
      {
         auto portInst = mod->find_member(par, port_o_K, cir);
         if(!portInst)
         {
            portInst = mod->find_member(par+"_o", port_o_K, cir);
            THROW_ASSERT(portInst, "unexpected condition");
            THROW_ASSERT(GetPointer<port_o>(portInst)->get_port_interface() != port_o::port_interface::PI_DEFAULT, "unexpected condition");
         }
         THROW_ASSERT(portInst, "unexpected condition");
         auto InterfaceType = GetPointer<port_o>(portInst)->get_port_interface();
         if(InterfaceType == port_o::port_interface::PI_DEFAULT)
         {
            if(GetPointer<port_o>(portInst)->get_is_memory() || (GetPointer<port_o>(portInst)->get_is_extern() && GetPointer<port_o>(portInst)->get_is_global()) || !portInst->get_typeRef()->treenode ||
                  !tree_helper::is_a_pointer(TreeM, portInst->get_typeRef()->treenode))
               continue;
            std::string unmangled_name = portInst->get_id();
            std::string port_name = HDL_manager::convert_to_identifier(writer.get(), unmangled_name);
            std::string output_name = "ex_" + unmangled_name;
            unsigned int pt_type_index = tree_helper::get_pointed_type(TreeM, tree_helper::get_type_index(TreeM, portInst->get_typeRef()->treenode));
            tree_nodeRef pt_node = TreeM->get_tree_node_const(pt_type_index);
            if(GetPointer<array_type>(pt_node))
            {
               while(GetPointer<array_type>(pt_node))
               {
                  pt_type_index = GET_INDEX_NODE(GetPointer<array_type>(pt_node)->elts);
                  pt_node = GET_NODE(GetPointer<array_type>(pt_node)->elts);
               }
            }
            long long int bitsize = tree_helper::size(TreeM, pt_type_index);
            bool is_real = tree_helper::is_real(TreeM, pt_type_index);

            writer->write("\n");
            writer->write_comment("OPTIONAL - Read a value for " + unmangled_name + " --------------------------------------------------------------\n");

            writer->write("_i_ = 0;\n");
            writer->write("while (_ch_ == \"/\" || _ch_ == \"\\n\" || _ch_ == \"o\")\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write("begin\n");
            {
               writer->write("if (_ch_ == \"o\")\n");
               writer->write(STR(STD_OPENING_CHAR));
               writer->write("begin\n");
               {
                  writer->write("compare_outputs = 1;\n");
                  writer->write("_r_ = $fscanf(file,\"%b\\n\", " + output_name + "); ");
                  writer->write_comment("expected format: bbb...b (example: 00101110)\n");

                  writer->write("if (_r_ != 1)\n");
                  writer->write(STR(STD_OPENING_CHAR));
                  writer->write("begin\n");
                  {
                     writer->write_comment("error\n");
                     writer->write("$display(\"ERROR - Unknown error while reading the file. Character found: %c\", _ch_[7:0]);\n");
                     writer->write("$fclose(res_file);\n");
                     writer->write("$fclose(file);\n");
                     writer->write("$finish;\n");
                  }
                  writer->write(STR(STD_CLOSING_CHAR));
                  writer->write("end\n");
                  writer->write("else\n");
                  writer->write(STR(STD_OPENING_CHAR));
                  writer->write("begin\n");
                  {
                     if(output_level > OUTPUT_LEVEL_MINIMUM) writer->write("$display(\"Value found for output " + unmangled_name + ": %b\", " + output_name + ");\n");
                  }
                  writer->write(STR(STD_CLOSING_CHAR));
                  writer->write("end\n");

                  size_t escaped_pos = port_name.find('\\');
                  std::string nonescaped_name = port_name;
                  if(escaped_pos != std::string::npos) nonescaped_name.erase(std::remove(nonescaped_name.begin(), nonescaped_name.end(), '\\'), nonescaped_name.end());
                  if(is_real)
                  {
                     if(output_level > OUTPUT_LEVEL_MINIMUM)
                     {
                        writer->write("$display(\" res = %b " + nonescaped_name +
                                      " = %d "
                                      " _bambu_testbench_mem_[" +
                                      nonescaped_name + " + %d - base_addr] = %20.20f  expected = %20.20f \", ");
                        writer->write("{");
                        for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                        {
                           if(bitsize_index) writer->write(", ");
                           writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                        }
                        writer->write("} == " + output_name + ", ");
                        writer->write(port_name + ", _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + ", " + (bitsize == 32 ? "bits32_to_real64" : "$bitstoreal") + "({");
                        for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                        {
                           if(bitsize_index) writer->write(", ");
                           writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                        }
                        writer->write(std::string("}), ") + (bitsize == 32 ? "bits32_to_real64" : "$bitstoreal") + "(" + output_name + "));\n");
                     }
                     if(bitsize == 32 || bitsize == 64)
                     {
                        if(output_level > OUTPUT_LEVEL_MINIMUM)
                        {
                           writer->write("$display(\" FP error %f \\n\", compute_ulp" + (bitsize == 32 ? STR(32) : STR(64)) + "({");
                           for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                           {
                              if(bitsize_index) writer->write(", ");
                              writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                           }
                           writer->write("}, " + output_name);
                           writer->write("));\n");
                        }
                        writer->write("if (compute_ulp" + (bitsize == 32 ? STR(32) : STR(64)) + "({");
                        for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                        {
                           if(bitsize_index) writer->write(", ");
                           writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                        }
                        writer->write("}, " + output_name);
                        writer->write(") > " + STR(parameters->getOption<double>(OPT_max_ulp)) + ")\n");
                     }
                     else
                        THROW_ERROR_CODE(NODE_NOT_YET_SUPPORTED_EC, "floating point precision not yet supported: " + STR(bitsize));
                  }
                  else
                  {
                     if(output_level > OUTPUT_LEVEL_MINIMUM)
                     {
                        writer->write("$display(\" res = %b " + nonescaped_name +
                                      " = %d "
                                      " _bambu_testbench_mem_[" +
                                      nonescaped_name + " + %d - base_addr] = %d  expected = %d \\n\", ");
                        writer->write("{");
                        for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                        {
                           if(bitsize_index) writer->write(", ");
                           writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                        }
                        writer->write("} == " + output_name + ", ");
                        writer->write(port_name + ", _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + ", {");
                        for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                        {
                           if(bitsize_index) writer->write(", ");
                           writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                        }
                        writer->write("}, " + output_name + ");\n");
                     }
                     writer->write("if ({");
                     for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                     {
                        if(bitsize_index) writer->write(", ");
                        writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                     }
                     writer->write("} !== " + output_name);
                     writer->write(")\n");
                  }
                  writer->write(STR(STD_OPENING_CHAR));
                  writer->write("begin\n");
                  writer->write("success = 0;\n");
                  writer->write(STR(STD_CLOSING_CHAR));
                  writer->write("end\n");

                  writer->write("_i_ = _i_ + 1;\n");
                  writer->write("_ch_ = $fgetc(file);\n");
               }
               writer->write(STR(STD_CLOSING_CHAR));
               writer->write("end\n");

               writer->write("else\n");
               writer->write(STR(STD_OPENING_CHAR));
               writer->write("begin\n");
               {
                  writer->write_comment("skip comments and empty lines\n");
                  writer->write("_r_ = $fgets(line, file);\n");
                  writer->write("_ch_ = $fgetc(file);\n");
               }
               writer->write(STR(STD_CLOSING_CHAR));
               writer->write("end\n");
            }
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");

            writer->write("if (_ch_ == \"e\")\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write("begin\n");
            writer->write("_r_ = $fgets(line, file);\n");
            writer->write("_ch_ = $fgetc(file);\n");
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");
            writer->write("else\n");
            writer->write("begin\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write_comment("error\n");
            writer->write("$display(\"ERROR - Unknown error while reading the file. Character found: %c\", _ch_[7:0]);\n");
            writer->write("$fclose(res_file);\n");
            writer->write("$fclose(file);\n");
            writer->write("$finish;\n");
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");
         }
         else if(InterfaceType == port_o::port_interface::PI_RNONE)
         {
            writer->write("\n");
            writer->write_comment("OPTIONAL - skip expected value for " + portInst->get_id() + " --------------------------------------------------------------\n");

            writer->write("_i_ = 0;\n");
            writer->write("while (_ch_ == \"/\" || _ch_ == \"\\n\" || _ch_ == \"o\")\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write("begin\n");
            {
               writer->write("if (_ch_ == \"o\")\n");
               writer->write(STR(STD_OPENING_CHAR));
               writer->write("begin\n");
               {
                  writer->write("compare_outputs = 1;\n");
                  writer->write("_r_ = $fgets(line, file);\n");
                  writer->write("_i_ = _i_ + 1;\n");
                  writer->write("_ch_ = $fgetc(file);\n");
               }
               writer->write(STR(STD_CLOSING_CHAR));
               writer->write("end\n");

               writer->write("else\n");
               writer->write(STR(STD_OPENING_CHAR));
               writer->write("begin\n");
               {
                  writer->write_comment("skip comments and empty lines\n");
                  writer->write("_r_ = $fgets(line, file);\n");
                  writer->write("_ch_ = $fgetc(file);\n");
               }
               writer->write(STR(STD_CLOSING_CHAR));
               writer->write("end\n");
            }
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");

            writer->write("if (_ch_ == \"e\")\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write("begin\n");
            writer->write("_r_ = $fgets(line, file);\n");
            writer->write("_ch_ = $fgetc(file);\n");
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");
            writer->write("else\n");
            writer->write("begin\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write_comment("error\n");
            writer->write("$display(\"ERROR - Unknown error while reading the file. Character found: %c\", _ch_[7:0]);\n");
            writer->write("$fclose(res_file);\n");
            writer->write("$fclose(file);\n");
            writer->write("$finish;\n");
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");
         }
         else if(InterfaceType == port_o::port_interface::PI_WNONE)
         {
            auto orig_name = portInst->get_id();
            std::string output_name = "ex_" + orig_name;
            writer->write("\n");
            writer->write_comment("OPTIONAL - Read a value for " + orig_name + " --------------------------------------------------------------\n");

            writer->write("_i_ = 0;\n");
            writer->write("while (_ch_ == \"/\" || _ch_ == \"\\n\" || _ch_ == \"o\")\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write("begin\n");
            {
               writer->write("if (_ch_ == \"o\")\n");
               writer->write(STR(STD_OPENING_CHAR));
               writer->write("begin\n");
               {
                  writer->write("compare_outputs = 1;\n");
                  writer->write("_r_ = $fscanf(file,\"%b\\n\", " + output_name + "); ");
                  writer->write_comment("expected format: bbb...b (example: 00101110)\n");
                  writer->write("if (_r_ != 1)\n");
                  writer->write(STR(STD_OPENING_CHAR));
                  writer->write("begin\n");
                  {
                     writer->write_comment("error\n");
                     writer->write("$display(\"ERROR - Unknown error while reading the file. Character found: %c\", _ch_[7:0]);\n");
                     writer->write("$fclose(res_file);\n");
                     writer->write("$fclose(file);\n");
                     writer->write("$finish;\n");
                  }
                  writer->write(STR(STD_CLOSING_CHAR));
                  writer->write("end\n");
                  writer->write("else\n");
                  writer->write(STR(STD_OPENING_CHAR));
                  writer->write("begin\n");
                  {
                     if(output_level > OUTPUT_LEVEL_MINIMUM) writer->write("$display(\"Value found for output " + orig_name + ": %b\", " + output_name + ");\n");
                  }
                  writer->write(STR(STD_CLOSING_CHAR));
                  writer->write("end\n");

                  if(portInst->get_typeRef()->type == structural_type_descriptor::REAL)
                  {
                     if(GET_TYPE_SIZE(portInst) == 32)
                     {
                        writer->write("$display(\" " + orig_name + " = %20.20f   expected = %20.20f \", bits32_to_real64(" + orig_name + "), bits32_to_real64(" + output_name + "));\n");
                        writer->write("$display(\" FP error %f \\n\", compute_ulp32(" + orig_name + ", " + output_name + "));\n");
                        writer->write("if (compute_ulp32(" + orig_name + ", " + output_name + ") > " + STR(parameters->getOption<double>(OPT_max_ulp)) + ")\n");
                     }
                     else if(GET_TYPE_SIZE(portInst) == 64)
                     {
                        writer->write("$display(\" " + orig_name + " = %20.20f   expected = %20.20f \", $bitstoreal(" + orig_name + "), $bitstoreal(ex_" + orig_name + "));\n");
                        writer->write("$display(\" FP error %f \\n\", compute_ulp64(" + orig_name + ", " + output_name + "));\n");
                        writer->write("if (compute_ulp64(" + orig_name + ", " + output_name + ") > " + STR(parameters->getOption<double>(OPT_max_ulp)) + ")\n");
                     }
                     else
                        THROW_ERROR_CODE(NODE_NOT_YET_SUPPORTED_EC, "floating point precision not yet supported: " + STR(GET_TYPE_SIZE(portInst)));
                  }
                  else
                  {
                     writer->write("$display(\" " + orig_name + " = %d   expected = %d \\n\", " + orig_name + ", ex_" + orig_name + ");\n");
                     writer->write("if (" + orig_name + " !== " + output_name + ")\n");
                  }
                  writer->write(STR(STD_OPENING_CHAR));
                  writer->write("begin\n");
                  writer->write("success = 0;\n");
                  writer->write(STR(STD_CLOSING_CHAR));
                  writer->write("end\n");

                  writer->write("_i_ = _i_ + 1;\n");
                  writer->write("_ch_ = $fgetc(file);\n");
               }
               writer->write(STR(STD_CLOSING_CHAR));
               writer->write("end\n");

               writer->write("else\n");
               writer->write(STR(STD_OPENING_CHAR));
               writer->write("begin\n");
               {
                  writer->write_comment("skip comments and empty lines\n");
                  writer->write("_r_ = $fgets(line, file);\n");
                  writer->write("_ch_ = $fgetc(file);\n");
               }
               writer->write(STR(STD_CLOSING_CHAR));
               writer->write("end\n");
            }
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");

            writer->write("if (_ch_ == \"e\")\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write("begin\n");
            writer->write("_r_ = $fgets(line, file);\n");
            writer->write("_ch_ = $fgetc(file);\n");
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");
            writer->write("else\n");
            writer->write("begin\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write_comment("error\n");
            writer->write("$display(\"ERROR - Unknown error while reading the file. Character found: %c\", _ch_[7:0]);\n");
            writer->write("$fclose(res_file);\n");
            writer->write("$fclose(file);\n");
            writer->write("$finish;\n");
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");
         }
         else
            THROW_ERROR("not supported port interface type");
      }
   }
   else if(interface_type == HLSFlowStep_Type::WB4_INTERFACE_GENERATION)
   {
      const auto top_functions = HLSMgr->CGetCallGraphManager()->GetRootFunctions();
      THROW_ASSERT(top_functions.size() == 1, "");
      const unsigned int topFunctionId = *(top_functions.begin());
      const BehavioralHelperConstRef behavioral_helper = HLSMgr->CGetFunctionBehavior(topFunctionId)->CGetBehavioralHelper();
      const memoryRef mem = HLSMgr->Rmem;
      const std::map<unsigned int, memory_symbolRef>& function_parameters = mem->get_function_parameters(topFunctionId);
      for(auto const& function_parameter : function_parameters)
      {
         unsigned int var = function_parameter.first;
         if (tree_helper::is_a_pointer(TreeM, var) && var != behavioral_helper->GetFunctionReturnType(topFunctionId))
         {
            std::string variableName = behavioral_helper->PrintVariable(var);
            std::string port_name = HDL_manager::convert_to_identifier(writer.get(), variableName);
            std::string output_name = "ex_" + variableName;
            unsigned int pt_type_index = tree_helper::get_pointed_type(TreeM, tree_helper::get_type_index(TreeM,var));
            tree_nodeRef pt_node = TreeM->get_tree_node_const(pt_type_index);
            if(GetPointer<array_type>(pt_node))
            {
               while(GetPointer<array_type>(pt_node))
               {
                  pt_type_index = GET_INDEX_NODE(GetPointer<array_type>(pt_node)->elts);
                  pt_node = GET_NODE(GetPointer<array_type>(pt_node)->elts);
               }
            }
            long long int bitsize = tree_helper::size(TreeM, pt_type_index);
            bool is_real = tree_helper::is_real(TreeM, pt_type_index);

            writer->write("\n");
            writer->write_comment("OPTIONAL - Read a value for " + variableName + " --------------------------------------------------------------\n");

            writer->write("_i_ = 0;\n");
                        writer->write("while (_ch_ == \"/\" || _ch_ == \"\\n\" || _ch_ == \"o\")\n");
                        writer->write(STR(STD_OPENING_CHAR));
                        writer->write("begin\n");
                        {
                           writer->write("if (_ch_ == \"o\")\n");
                           writer->write(STR(STD_OPENING_CHAR));
                           writer->write("begin\n");
                           {
                              writer->write("compare_outputs = 1;\n");
                              writer->write("_r_ = $fscanf(file,\"%b\\n\", " + output_name + "); ");
                              writer->write_comment("expected format: bbb...b (example: 00101110)\n");

                              writer->write("if (_r_ != 1)\n");
                              writer->write(STR(STD_OPENING_CHAR));
                              writer->write("begin\n");
                              {
                                 writer->write_comment("error\n");
                                 writer->write("$display(\"ERROR - Unknown error while reading the file. Character found: %c\", _ch_[7:0]);\n");
                                 writer->write("$fclose(res_file);\n");
                                 writer->write("$fclose(file);\n");
                                 writer->write("$finish;\n");
                              }
                              writer->write(STR(STD_CLOSING_CHAR));
                              writer->write("end\n");
                              writer->write("else\n");
                              writer->write(STR(STD_OPENING_CHAR));
                              writer->write("begin\n");
                              {
                                 if(output_level > OUTPUT_LEVEL_MINIMUM) writer->write("$display(\"Value found for output " + variableName + ": %b\", " + output_name + ");\n");
                              }
                              writer->write(STR(STD_CLOSING_CHAR));
                              writer->write("end\n");

                              size_t escaped_pos = port_name.find('\\');
                              std::string nonescaped_name = port_name;
                              if(escaped_pos != std::string::npos) nonescaped_name.erase(std::remove(nonescaped_name.begin(), nonescaped_name.end(), '\\'), nonescaped_name.end());
                              if(is_real)
                              {
                                 if(output_level > OUTPUT_LEVEL_MINIMUM)
                                 {
                                    writer->write("$display(\" res = %b " + nonescaped_name +
                                                  " = %d "
                                                  " _bambu_testbench_mem_[" +
                                                  nonescaped_name + " + %d - base_addr] = %20.20f  expected = %20.20f \", ");
                                    writer->write("{");
                                    for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                                    {
                                       if(bitsize_index) writer->write(", ");
                                       writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                                    }
                                    writer->write("} == " + output_name + ", ");
                                    writer->write(port_name + ", _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + ", " + (bitsize == 32 ? "bits32_to_real64" : "$bitstoreal") + "({");
                                    for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                                    {
                                       if(bitsize_index) writer->write(", ");
                                       writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                                    }
                                    writer->write(std::string("}), ") + (bitsize == 32 ? "bits32_to_real64" : "$bitstoreal") + "(" + output_name + "));\n");
                                 }
                                 if(bitsize == 32 || bitsize == 64)
                                 {
                                    if(output_level > OUTPUT_LEVEL_MINIMUM)
                                    {
                                       writer->write("$display(\" FP error %f \\n\", compute_ulp" + (bitsize == 32 ? STR(32) : STR(64)) + "({");
                                       for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                                       {
                                          if(bitsize_index) writer->write(", ");
                                          writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                                       }
                                       writer->write("}, " + output_name);
                                       writer->write("));\n");
                                    }
                                    writer->write("if (compute_ulp" + (bitsize == 32 ? STR(32) : STR(64)) + "({");
                                    for(unsigned int bitsize_index = 0; bitsize_index < bitsize; bitsize_index = bitsize_index + 8)
                                    {
                                       if(bitsize_index) writer->write(", ");
                                       writer->write("_bambu_testbench_mem_[" + port_name + " + _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + " + " + boost::lexical_cast<std::string>((bitsize - bitsize_index) / 8 - 1) + " - base_addr]");
                                    }
                                    writer->write("}, " + output_name);
                                    writer->write(") > " + STR(parameters->getOption<double>(OPT_max_ulp)) + ")\n");
                                 }
                                 else
                                    THROW_ERROR_CODE(NODE_NOT_YET_SUPPORTED_EC, "floating point precision not yet supported: " + STR(bitsize));
                              }
                              else
                              {
                                 long long int bytesize = bitsize / 8;
                                 if (output_level > OUTPUT_LEVEL_MINIMUM)
                                 {
                                    writer->write("$display(\" res = %b " + nonescaped_name +
                                                  " = %d "
                                                  " _bambu_testbench_mem_[" +
                                                  nonescaped_name + " + %d - base_addr] = %d  expected = %d \\n\", ");
                                    writer->write("{");
                                    for(unsigned int index = 0; index < bytesize; ++index)
                                    {
                                       if(index) writer->write(", ");
                                       writer->write("_bambu_testbench_mem_[(" + port_name + " - base_addr) + _i_*" + boost::lexical_cast<std::string>(bytesize) + " + "
                                               + boost::lexical_cast<std::string>(bytesize - index - 1) +"]");
                                    }
                                    writer->write("} == " + output_name + ", ");
                                    writer->write(port_name + ", _i_*" + boost::lexical_cast<std::string>(bitsize / 8) + ", {");
                                    for(unsigned int index = 0; index < bytesize; ++index)
                                    {
                                       if(index) writer->write(", ");
                                       writer->write("_bambu_testbench_mem_[(" + port_name + " - base_addr) + _i_*" + boost::lexical_cast<std::string>(bytesize) + " + "
                                               + boost::lexical_cast<std::string>(bytesize - index - 1) + "]");
                                    }
                                    writer->write("}, " + output_name + ");\n");
                                 }
                                 writer->write("if ({");
                                 for(unsigned int index=0; index < bytesize; ++index)
                                 {
                                    if(index) writer->write(", ");
                                    writer->write("_bambu_testbench_mem_[(" + port_name + " - base_addr) + _i_*" + boost::lexical_cast<std::string>(bytesize) + " + "
                                            + boost::lexical_cast<std::string>(bytesize - index - 1) + "]");
                                 }
                                 writer->write("} !== " + output_name);
                                 writer->write(")\n");
                              }
                              writer->write(STR(STD_OPENING_CHAR));
                              writer->write("begin\n");
                              writer->write("success = 0;\n");
                              writer->write(STR(STD_CLOSING_CHAR));
                              writer->write("end\n");

                              writer->write("_i_ = _i_ + 1;\n");
                              writer->write("_ch_ = $fgetc(file);\n");
                           }
                           writer->write(STR(STD_CLOSING_CHAR));
                           writer->write("end\n");

                           writer->write("else\n");
                           writer->write(STR(STD_OPENING_CHAR));
                           writer->write("begin\n");
                           {
                              writer->write_comment("skip comments and empty lines\n");
                              writer->write("_r_ = $fgets(line, file);\n");
                              writer->write("_ch_ = $fgetc(file);\n");
                           }
                           writer->write(STR(STD_CLOSING_CHAR));
                           writer->write("end\n");
                        }
                        writer->write(STR(STD_CLOSING_CHAR));
                        writer->write("end\n");

                        writer->write("if (_ch_ == \"e\")\n");
                        writer->write(STR(STD_OPENING_CHAR));
                        writer->write("begin\n");
                        writer->write("_r_ = $fgets(line, file);\n");
                        writer->write("_ch_ = $fgetc(file);\n");
                        writer->write(STR(STD_CLOSING_CHAR));
                        writer->write("end\n");
                        writer->write("else\n");
                        writer->write("begin\n");
                        writer->write(STR(STD_OPENING_CHAR));
                        writer->write_comment("error\n");
                        writer->write("$display(\"ERROR - Unknown error while reading the file. Character found: %c\", _ch_[7:0]);\n");
                        writer->write("$fclose(res_file);\n");
                        writer->write("$fclose(file);\n");
                        writer->write("$finish;\n");
                        writer->write(STR(STD_CLOSING_CHAR));
                        writer->write("end\n");
         }
      }
   }


   if(mod->find_member(RETURN_PORT_NAME, port_o_K, cir))
   {
      std::string output_name = "ex_" + std::string(RETURN_PORT_NAME);
      structural_objectRef return_port = mod->find_member(RETURN_PORT_NAME, port_o_K, cir);

      writer->write("_i_ = 0;\n");
      writer->write("while (_ch_ == \"/\" || _ch_ == \"\\n\" || _ch_ == \"o\")\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      {
         writer->write("if (_ch_ == \"o\")\n");
         writer->write(STR(STD_OPENING_CHAR));
         writer->write("begin\n");
         {
            writer->write("compare_outputs = 1;\n");
            writer->write("_r_ = $fscanf(file,\"%b\\n\", " + output_name + "); ");
            writer->write_comment("expected format: bbb...b (example: 00101110)\n");

            writer->write("if (_r_ != 1)\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write("begin\n");
            {
               writer->write_comment("error\n");
               writer->write("$display(\"ERROR - Unknown error while reading the file. Character found: %c\", _ch_[7:0]);\n");
               writer->write("$fclose(res_file);\n");
               writer->write("$fclose(file);\n");
               writer->write("$finish;\n");
            }
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");
            writer->write("else\n");
            writer->write(STR(STD_OPENING_CHAR));
            writer->write("begin\n");
            {
               if(output_level > OUTPUT_LEVEL_MINIMUM) writer->write("$display(\"Value found for output " + output_name + ": %b\", " + output_name + ");\n");
            }
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");

            if(return_port->get_typeRef()->type == structural_type_descriptor::REAL)
            {
               if(GET_TYPE_SIZE(return_port) == 32)
               {
                  writer->write("$display(\" " + std::string(RETURN_PORT_NAME) + " = %20.20f   expected = %20.20f \", bits32_to_real64(registered_" + std::string(RETURN_PORT_NAME) + "), bits32_to_real64(ex_" + std::string(RETURN_PORT_NAME) + "));\n");
                  writer->write("$display(\" FP error %f \\n\", compute_ulp32(registered_" + std::string(RETURN_PORT_NAME) + ", " + output_name + "));\n");
                  writer->write("if (compute_ulp32(registered_" + std::string(RETURN_PORT_NAME) + ", " + output_name + ") > " + STR(parameters->getOption<double>(OPT_max_ulp)) + ")\n");
               }
               else if(GET_TYPE_SIZE(return_port) == 64)
               {
                  writer->write("$display(\" " + std::string(RETURN_PORT_NAME) + " = %20.20f   expected = %20.20f \", $bitstoreal(registered_" + std::string(RETURN_PORT_NAME) + "), $bitstoreal(ex_" + std::string(RETURN_PORT_NAME) + "));\n");
                  writer->write("$display(\" FP error %f \\n\", compute_ulp64(registered_" + std::string(RETURN_PORT_NAME) + ", " + output_name + "));\n");
                  writer->write("if (compute_ulp64(registered_" + std::string(RETURN_PORT_NAME) + ", " + output_name + ") > " + STR(parameters->getOption<double>(OPT_max_ulp)) + ")\n");
               }
               else
                  THROW_ERROR_CODE(NODE_NOT_YET_SUPPORTED_EC, "floating point precision not yet supported: " + STR(GET_TYPE_SIZE(return_port)));
            }
            else
            {
               writer->write("$display(\" " + std::string(RETURN_PORT_NAME) + " = %d   expected = %d \\n\", registered_" + std::string(RETURN_PORT_NAME) + ", ex_" + std::string(RETURN_PORT_NAME) + ");\n");
               writer->write("if (registered_" + std::string(RETURN_PORT_NAME) + " !== " + output_name + ")\n");
            }
            writer->write(STR(STD_OPENING_CHAR));
            writer->write("begin\n");
            writer->write("success = 0;\n");
            writer->write(STR(STD_CLOSING_CHAR));
            writer->write("end\n");

            writer->write("_i_ = _i_ + 1;\n");
            writer->write("_ch_ = $fgetc(file);\n");
         }
         writer->write(STR(STD_CLOSING_CHAR));
         writer->write("end\n");

         writer->write("else\n");
         writer->write(STR(STD_OPENING_CHAR));
         writer->write("begin\n");
         {
            writer->write_comment("skip comments and empty lines\n");
            writer->write("_r_ = $fgets(line, file);\n");
            writer->write("_ch_ = $fgetc(file);\n");
         }
         writer->write(STR(STD_CLOSING_CHAR));
         writer->write("end\n");
      }
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");

      writer->write("if (_ch_ == \"e\")\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      writer->write("_r_ = $fgets(line, file);\n");
      writer->write("_ch_ = $fgetc(file);\n");
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");
      writer->write("else\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      writer->write_comment("error\n");
      writer->write("$display(\"ERROR - Unknown error while reading the file. Character found: %c\", _ch_[7:0]);\n");
      writer->write("$fclose(res_file);\n");
      writer->write("$fclose(file);\n");
      writer->write("$finish;\n");
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");
   }

   writer->write_comment("Compare output with expected output (if given)\n");
   writer->write("if (compare_outputs == 1)\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   {
      writer->write("$display(\"Simulation ended after %d cycles.\\n\", sim_time);\n");
      writer->write("if (success == 1)\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      {
         writer->write("$display(\"Simulation completed with success\\n\");\n");
         writer->write("$fwrite(res_file, \"1\\t\");\n");
      }
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");
      writer->write("else\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      {
         writer->write("$display(\"Simulation FAILED\\n\");\n");
         writer->write("$fwrite(res_file, \"0\\t\");\n");
      }
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");
   }
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
   writer->write("else\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   {
      writer->write("$display(\"Simulation ended after %d cycles (no expected outputs specified).\\n\", sim_time);\n");
      writer->write("$fwrite(res_file, \"-\\t\");\n");
   }
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");

   writer->write("$fwrite(res_file, \"%d\\n\", sim_time);\n");
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n\n");
}

void TestbenchGenerationBaseStep::write_underlying_testbench(const std::string simulation_values_path, bool generate_vcd_output, bool xilinx_isim, const tree_managerConstRef TreeM) const
{
   if(mod->get_in_out_port_size()) { THROW_ERROR("Module with IO ports cannot be synthesized"); }
   write_hdl_testbench_prolog();
   write_module_begin();
   write_compute_ulps_functions();
   write_auxiliary_signal_declaration();
   bool withMemory = false;
   bool hasMultiIrq = false;
   write_signals(TreeM, withMemory, hasMultiIrq);
   write_slave_initializations(withMemory);
   write_module_instantiation(xilinx_isim);
   write_initial_block(simulation_values_path, withMemory, TreeM, generate_vcd_output);
   begin_file_reading_operation();
   reading_base_memory_address_from_file();
   memory_initialization_from_file();
   write_file_reading_operations();
   end_file_reading_operation();
   if(withMemory) write_memory_handler();
   write_call(hasMultiIrq);
   write_output_checks(TreeM);
   testbench_controller_machine();
   write_sim_time_calc();
   write_max_simulation_time_control();
   write_module_end();
}

void TestbenchGenerationBaseStep::write_clock_process() const
{
   /// write clock switching operation
   writer->write_comment("Clock switching: 1 cycle every CLOCK_PERIOD seconds\n");
   writer->write("always # `HALF_CLOCK_PERIOD clock = !clock;\n\n");
}

void TestbenchGenerationBaseStep::write_hdl_testbench_prolog() const
{
   if(parameters->getOption<std::string>(OPT_simulator) == "VERILATOR")
   {
      writer->write_comment("verilator lint_off BLKANDNBLK\n");
      writer->write_comment("verilator lint_off BLKSEQ\n");
   }

   writer->write("`timescale 1ns / 1ps\n");
   writer->write_comment("CONSTANTS DECLARATION\n");
   writer->write("`define EOF 32'hFFFF_FFFF\n`define NULL 0\n`define MAX_COMMENT_LENGTH 1000\n`define SIMULATION_LENGTH " + STR(parameters->getOption<int>(OPT_max_sim_cycles)) + "\n\n");
   std::string half_target_period_string = STR(target_period / 2);
   // If the value it is integer, we add .0 to describe a float otherwise modelsim returns conversion error
   if(half_target_period_string.find(".") == std::string::npos) half_target_period_string += ".0";
   writer->write("`define HALF_CLOCK_PERIOD " + half_target_period_string + "\n\n");
   writer->write("`define CLOCK_PERIOD (2*`HALF_CLOCK_PERIOD)\n\n");
   if(parameters->getOption<std::string>(OPT_bram_high_latency) != "" && parameters->getOption<std::string>(OPT_bram_high_latency) == "_3")
      writer->write("`define MEM_DELAY_READ 3\n\n");
   else if(parameters->getOption<std::string>(OPT_bram_high_latency) != "" && parameters->getOption<std::string>(OPT_bram_high_latency) == "_4")
      writer->write("`define MEM_DELAY_READ 4\n\n");
   else if(parameters->getOption<std::string>(OPT_bram_high_latency) == "")
      writer->write("`define MEM_DELAY_READ " + parameters->getOption<std::string>(OPT_mem_delay_read) + "\n\n");
   else
      THROW_ERROR("unexpected bram high latency delay");
   writer->write("`define MEM_DELAY_WRITE " + parameters->getOption<std::string>(OPT_mem_delay_write) + "\n\n");
}

void TestbenchGenerationBaseStep::write_module_begin() const
{
   writer->write_comment("MODULE DECLARATION\n");
   writer->write("module " + mod->get_id() + "_tb(" + STR(CLOCK_PORT_NAME) + ");\n");
   writer->write(STR(STD_OPENING_CHAR));
}

void TestbenchGenerationBaseStep::write_module_end() const
{
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("endmodule\n\n");
   if(parameters->getOption<std::string>(OPT_simulator) == "VERILATOR")
   {
      writer->write_comment("verilator lint_on BLKANDNBLK\n");
      writer->write_comment("verilator lint_on BLKSEQ\n");
   }
}

void TestbenchGenerationBaseStep::write_module_instantiation(bool xilinx_isim) const
{
   const auto target_language = static_cast<HDLWriter_Language>(parameters->getOption<int>(OPT_writer_language));
   const auto target_writer = target_language == HDLWriter_Language::VERILOG ? writer : language_writer::create_writer(target_language, HLSMgr->get_HLS_target()->get_technology_manager(), parameters);

   /// write module instantiation and ports binding
   writer->write_comment("MODULE INSTANTIATION AND PORTS BINDING\n");
   auto module_name = HDL_manager::get_mod_typename(target_writer.get(), cir);
   if(module_name[0] == '\\' and target_language == HDLWriter_Language::VHDL) module_name = "\\" + module_name;
   writer->write_module_instance_begin(cir, module_name, !xilinx_isim);
   std::string prefix = "";
   if(mod->get_in_port_size())
   {
      for(unsigned int i = 0; i < mod->get_in_port_size(); i++)
      {
         if(i == 0)
            prefix = ".";
         else
            prefix = ", .";

         writer->write(prefix + HDL_manager::convert_to_identifier(writer.get(), mod->get_in_port(i)->get_id()) + "(" + HDL_manager::convert_to_identifier(writer.get(), mod->get_in_port(i)->get_id()) + ")");
      }
   }
   if(mod->get_out_port_size())
   {
      for(unsigned int i = 0; i < mod->get_out_port_size(); i++)
      { writer->write(prefix + HDL_manager::convert_to_identifier(writer.get(), mod->get_out_port(i)->get_id()) + "(" + HDL_manager::convert_to_identifier(writer.get(), mod->get_out_port(i)->get_id()) + ")"); } }

   writer->write_module_instance_end(cir);

   if(xilinx_isim) writer->write("glbl #(" + STR(target_period / 2 * 1000) + ", 0) glbl();");
}

void TestbenchGenerationBaseStep::write_auxiliary_signal_declaration() const
{
   unsigned int testbench_memsize = HLSMgr->Rmem->get_memory_address() - parameters->getOption<unsigned int>(OPT_base_address);
   if(testbench_memsize == 0) testbench_memsize = 1;
   writer->write("parameter MEMSIZE = " + STR(testbench_memsize));

   /// writing memory-related parameters
   if(mod->is_parameter(MEMORY_PARAMETER))
   {
      std::string memory_str = mod->get_parameter(MEMORY_PARAMETER);
      std::vector<std::string> mem_tag = convert_string_to_vector<std::string>(memory_str, ";");
      for(const auto& i : mem_tag)
      {
         std::vector<std::string> mem_add = convert_string_to_vector<std::string>(i, "=");
         THROW_ASSERT(mem_add.size() == 2, "malformed address");
         writer->write(", ");
         std::string name = mem_add[0];
         std::string value = mem_add[1];
         if(value.find("\"\"") != std::string::npos) { boost::replace_all(value, "\"\"", "\""); }
         else if(value.find("\"") != std::string::npos)
         {
            boost::replace_all(value, "\"", "");
            value = boost::lexical_cast<std::string>(value.size()) + "'b" + value;
         }
         writer->write(name + "=" + value);
      }
   }
   writer->write(";\n");

   writer->write_comment("AUXILIARY VARIABLES DECLARATION\n");
   writer->write("time startTime, endTime, sim_time;\n");
   writer->write("integer res_file, file, _r_, _n_, _i_, _addr_i_;\n");
   writer->write("integer _ch_;\n");
   writer->write("reg compare_outputs, success; // Flag: True if input vector specifies expected outputs\n");
   writer->write("reg [8*`MAX_COMMENT_LENGTH:0] line; // Comment line read from file\n\n");

   writer->write("reg [31:0] addr, base_addr;\n");
   if(!HLSMgr->design_interface.empty())
   {
      const auto& DesignSignature=HLSMgr->RSim->simulationArgSignature;
      bool writeP=false;
      for(auto par: DesignSignature)
      {
         auto portInst = mod->find_member(par, port_o_K, cir);
         if(!portInst)
         {
            portInst = mod->find_member(par+"_i", port_o_K, cir);
            THROW_ASSERT(portInst, "unexpected condition");
            THROW_ASSERT(GetPointer<port_o>(portInst)->get_port_interface() != port_o::port_interface::PI_DEFAULT, "unexpected condition");
         }
         THROW_ASSERT(portInst, "unexpected condition");
         auto InterfaceType = GetPointer<port_o>(portInst)->get_port_interface();
         std::string input_name = HDL_manager::convert_to_identifier(writer.get(), portInst->get_id());
         if(InterfaceType == port_o::port_interface::PI_RNONE || InterfaceType == port_o::port_interface::PI_WNONE)
         {
            writer->write("reg [31:0] paddr"+input_name+";\n");
            writeP=true;
         }
      }
      if(writeP)
         writer->write("\n");
   }
   writer->write("reg signed [7:0] _bambu_testbench_mem_ [0:MEMSIZE-1];\n\n");
   writer->write("reg signed [7:0] _bambu_databyte_;\n\n");
   writer->write("reg [3:0] __state, __next_state;\n");
   writer->write("reg start_results_comparison;\n");
   writer->write("reg next_start_port;\n");
}

void TestbenchGenerationBaseStep::begin_initial_block() const
{
   writer->write("\n");
   writer->write_comment("Operation to be executed just one time\n");
   writer->write("initial\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
}

void TestbenchGenerationBaseStep::end_initial_block() const
{
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
}

void TestbenchGenerationBaseStep::open_value_file(const std::string& input_values_filename) const
{
   writer->write_comment("OPEN FILE WITH VALUES FOR SIMULATION\n");
   writer->write("file = $fopen(\"" + input_values_filename + "\",\"r\");\n");
   writer->write_comment("Error in file open\n");
   writer->write("if (file == `NULL)\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   writer->write("$display(\"ERROR - Error opening the file\");\n");
   writer->write("$finish;");
   writer->write_comment("Terminate\n");
   writer->write("");
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
}

void TestbenchGenerationBaseStep::open_result_file(const std::string& result_file) const
{
   writer->write_comment("OPEN FILE WHERE results will be written\n");
   writer->write("res_file = $fopen(\"" + result_file + "\",\"w\");\n\n");
   writer->write_comment("Error in file open\n");
   writer->write("if (res_file == `NULL)\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   writer->write("$display(\"ERROR - Error opening the res_file\");\n");
   writer->write("$fclose(file);\n");
   writer->write("$finish;");
   writer->write_comment("Terminate\n");
   writer->write("");
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
}

void TestbenchGenerationBaseStep::initialize_auxiliary_variables() const
{
   writer->write_comment("Variables initialization\n");
   writer->write("sim_time = 0;\n");
   writer->write("startTime = 0;\n");
   writer->write("endTime = 0;\n");
   writer->write("_ch_ = 0;\n");
   writer->write("_n_ = 0;\n");
   writer->write("_r_ = 0;\n");
   writer->write("line = 0;\n");
   writer->write("_i_ = 0;\n");
   writer->write("_addr_i_ = 0;\n");
   writer->write("compare_outputs = 0;\n");
   writer->write("start_next_sim = 0;\n");
   writer->write("__next_state = 0;\n");
   writer->write(std::string(RESET_PORT_NAME) + " = 0;\n");
   writer->write("next_start_port = 0;\n");
   writer->write("success = 1;\n");
}

void TestbenchGenerationBaseStep::initialize_input_signals(const tree_managerConstRef TreeM) const
{
   if(mod->get_in_port_size())
   {
      for(unsigned int i = 0; i < mod->get_in_port_size(); i++)
      {
         const structural_objectRef& port_obj = mod->get_in_port(i);
         if(GetPointer<port_o>(port_obj)->get_is_memory() || WB_ACKIM_PORT_NAME == port_obj->get_id()) continue;
         if(CLOCK_PORT_NAME != port_obj->get_id() && START_PORT_NAME != port_obj->get_id() && RESET_PORT_NAME != port_obj->get_id()) writer->write(HDL_manager::convert_to_identifier(writer.get(), port_obj->get_id()) + " = 0;\n");
         if(port_obj->get_typeRef()->treenode > 0 && tree_helper::is_a_pointer(TreeM, port_obj->get_typeRef()->treenode)) { writer->write("ex_" + port_obj->get_id() + " = 0;\n"); }
      }
      writer->write("\n");
   }
   if(mod->get_out_port_size())
   {
      for(unsigned int i = 0; i < mod->get_out_port_size(); i++)
      {
         const structural_objectRef& port_obj = mod->get_out_port(i);
         auto interfaceType = GetPointer<port_o>(port_obj)->get_port_interface();
         if(interfaceType==port_o::port_interface::PI_WNONE)
         {
            writer->write("ex_" + port_obj->get_id() + " = 0;\n");
         }
      }
      writer->write("\n");
   }
}

void TestbenchGenerationBaseStep::testbench_controller_machine() const
{
   writer->write("always @(*)\n");
   writer->write("  begin\n");
   writer->write("     start_results_comparison = 0;\n");
   if(!parameters->getOption<bool>(OPT_level_reset))
      writer->write("     " + std::string(RESET_PORT_NAME) + " = 1;\n");
   else
      writer->write("     " + std::string(RESET_PORT_NAME) + " = 0;\n");
   writer->write("     start_next_sim = 0;\n");
   writer->write("     next_start_port = 0;\n");
   writer->write("     case (__state)\n");
   writer->write("       0:\n");
   writer->write("         begin\n");
   if(!parameters->getOption<bool>(OPT_level_reset))
      writer->write("            " + std::string(RESET_PORT_NAME) + " = 0;\n");
   else
      writer->write("            " + std::string(RESET_PORT_NAME) + " = 1;\n");
   writer->write("            __next_state = 1;\n");
   writer->write("         end\n");
   writer->write("       1:\n");
   writer->write("         begin\n");
   if(!parameters->getOption<bool>(OPT_level_reset))
      writer->write("            " + std::string(RESET_PORT_NAME) + " = 0;\n");
   else
      writer->write("            " + std::string(RESET_PORT_NAME) + " = 1;\n");
   writer->write("            __next_state = 2;\n");
   writer->write("         end\n");
   writer->write("       2:\n");
   writer->write("         begin\n");
   writer->write("            next_start_port = 1;\n");
   writer->write("            if (done_port == 1'b1)\n");
   writer->write("              begin\n");
   writer->write("                __next_state = 4;\n");
   writer->write("              end\n");
   writer->write("            else\n");
   writer->write("              __next_state = 3;\n");
   writer->write("         end\n");
   writer->write("       3:\n");
   writer->write("         if (done_port == 1'b1)\n");
   writer->write("           begin\n");
   writer->write("              __next_state = 4;\n");
   writer->write("           end\n");
   writer->write("         else\n");
   writer->write("           __next_state = 3;\n");
   writer->write("       4:\n");
   writer->write("         begin\n");
   writer->write("            start_results_comparison = 1;\n");
   writer->write("            __next_state = 5;\n");
   writer->write("         end\n");
   writer->write("       5:\n");
   writer->write("         begin\n");
   if(HLSMgr->RSim->test_vectors.size() <= 1)
   {
      writer->write_comment("wait a cycle (needed for a correct simulation)\n");
      writer->write("            $fclose(res_file);\n");
      writer->write("            $fclose(file);\n");
      writer->write("            $finish;\n");
   }
   else
   {
      writer->write_comment("Restart a new computation if possible\n");
      writer->write("            __next_state = 2;\n");
   }
   writer->write("         end\n");
   writer->write("       default:\n");
   writer->write("         begin\n");
   writer->write("            __next_state = 0;\n");
   writer->write("         end\n");
   writer->write("     endcase // case (__state)\n");
   writer->write("  end // always @ (*)\n");

   writer->write("always @(posedge " + std::string(CLOCK_PORT_NAME) + ")\n");
   writer->write("  begin\n");
   writer->write("  __state <= __next_state;\n");
   writer->write("  start_port <= next_start_port;\n");
   writer->write("  end\n");
}

void TestbenchGenerationBaseStep::memory_initialization() const
{
   writer->write("for (addr = 0; addr < MEMSIZE; addr = addr + 1)\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   writer->write("_bambu_testbench_mem_[addr] = 8'b0;\n");
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
}

void TestbenchGenerationBaseStep::write_max_simulation_time_control() const
{
   writer->write("always @(posedge " + std::string(CLOCK_PORT_NAME) + ")\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   writer->write("if (($time - startTime)/`CLOCK_PERIOD > `SIMULATION_LENGTH)\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   writer->write("$display(\"Simulation not completed into %d cycles\", `SIMULATION_LENGTH);\n");
   writer->write("$fwrite(res_file, \"X\\t\");\n");
   writer->write("$fwrite(res_file, \"%d\\n\", `SIMULATION_LENGTH);\n");
   writer->write("$fclose(res_file);\n");
   writer->write("$fclose(file);\n");
   writer->write("$finish;\n");
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n\n");
}

void TestbenchGenerationBaseStep::reading_base_memory_address_from_file() const
{
   writer->write_comment("reading base address memory --------------------------------------------------------------\n");
   writer->write("_ch_ = $fgetc(file);\n");
   writer->write("if ($feof(file))\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   {
      writer->write("$display(\"No more values found. Simulation(s) executed: %d.\\n\", _n_);\n");
      writer->write("$fclose(res_file);\n");
      writer->write("$fclose(file);\n");
      writer->write("$finish;\n");
   }
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
   writer->write("while (_ch_ == \"/\" || _ch_ == \"\\n\" || _ch_ == \"b\")\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   {
      writer->write("if (_ch_ == \"b\")\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      writer->write("_r_ = $fscanf(file,\"%b\\n\", base_addr); ");
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");
      writer->write("else\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      writer->write("_r_ = $fgets(line, file);\n");
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");
      writer->write("_ch_ = $fgetc(file);\n");
   }
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
}

void TestbenchGenerationBaseStep::memory_initialization_from_file() const
{
   writer->write_comment("initializing memory --------------------------------------------------------------\n");
   writer->write("while (_ch_ == \"/\" || _ch_ == \"\\n\" || _ch_ == \"m\")\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   {
      writer->write("if (_ch_ == \"m\")\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      {
         writer->write("_r_ = $fscanf(file,\"%b\\n\", _bambu_databyte_);\n");
         writer->write("_bambu_testbench_mem_[_addr_i_] = _bambu_databyte_;\n");
         writer->write("_addr_i_ = _addr_i_ + 1;\n");
      }
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");
      writer->write("else\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      writer->write("_r_ = $fgets(line, file);\n");
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");
      writer->write("_ch_ = $fgetc(file);\n");
   }
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
}

void TestbenchGenerationBaseStep::begin_file_reading_operation() const
{
   writer->write("\n");

   writer->write_comment("Assigning values\n");
   writer->write("always @ (posedge " + STR(CLOCK_PORT_NAME) + ")\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   writer->write("if (next_" + STR(START_PORT_NAME) + " == 1'b1)\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
}

void TestbenchGenerationBaseStep::end_file_reading_operation() const
{
   writer->write_comment("Simulation start\n");
   writer->write("startTime = $time;\n$display(\"Reading of vector values from input file completed. Simulation started.\");\n");
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n\n");
}

void TestbenchGenerationBaseStep::write_sim_time_calc() const
{
   writer->write_comment("Check done_port signal\n");
   writer->write("always @(negedge " + std::string(CLOCK_PORT_NAME) + ")\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");
   writer->write("if (done_port == 1)\n");
   writer->write(STR(STD_OPENING_CHAR));
   writer->write("begin\n");

   writer->write("endTime = $time;\n\n");
   writer->write_comment("Simulation time (clock cycles) = 1+(time elapsed (seconds) / clock cycle (seconds per cycle)) (until done is 1)\n");
   writer->write("sim_time = $rtoi((endTime + `HALF_CLOCK_PERIOD - startTime)/`CLOCK_PERIOD);\n\n");
   writer->write("success = 1;\n");
   writer->write("compare_outputs = 0;\n");

   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
   writer->write(STR(STD_CLOSING_CHAR));
   writer->write("end\n");
}

void TestbenchGenerationBaseStep::read_input_value_from_file(const std::string& input_name, bool& first_valid_input) const
{
   if(input_name != CLOCK_PORT_NAME && input_name != RESET_PORT_NAME && input_name != START_PORT_NAME)
   {
      writer->write("\n");
      writer->write_comment("Read a value for " + input_name + " --------------------------------------------------------------\n");
      if(!first_valid_input) writer->write("_ch_ = $fgetc(file);\n");

      writer->write("while (_ch_ == \"/\" || _ch_ == \"\\n\")\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      {
         writer->write("_r_ = $fgets(line, file);\n");
         writer->write("_ch_ = $fgetc(file);\n");
      }
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");

      if(first_valid_input)
      {
         /// write statement for new vectors' check
         writer->write_comment("If no character found\n");
         writer->write("if (_ch_ == -1)\n");
         writer->write(STR(STD_OPENING_CHAR));
         writer->write("begin\n");
         {
            writer->write("$display(\"No more values found. Simulation(s) executed: %d.\\n\", _n_);\n");
            writer->write("$fclose(res_file);\n");
            writer->write("$fclose(file);\n");
            writer->write("$finish;\n");
         }
         writer->write(STR(STD_CLOSING_CHAR));
         writer->write("end\n");
         writer->write("else\n");
         writer->write(STR(STD_OPENING_CHAR));
         writer->write("begin\n");
         {
            writer->write_comment("Vectors count\n");
            writer->write("_n_ = _n_ + 1;\n");
            writer->write("$display(\"Start reading vector %d's values from input file.\\n\", _n_);\n");
         }
         writer->write(STR(STD_CLOSING_CHAR));
         writer->write("end\n");
         first_valid_input = false;
      }

      writer->write("if (_ch_ == \"p\")\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      {
         writer->write("_r_ = $fscanf(file,\"%b\\n\", " + input_name + "); ");
         writer->write_comment("expected format: bbb...b (example: 00101110)\n");
      }
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");

      writer->write("if (_r_ != 1) ");
      writer->write_comment("error\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      {
         writer->write("_ch_ = $fgetc(file);\n");
         writer->write("if (_ch_ == `EOF) ");
         writer->write_comment("end-of-file reached\n");
         writer->write(STR(STD_OPENING_CHAR));
         writer->write("begin\n");
         {
            writer->write("$display(\"ERROR - End of file reached before getting all the values for the parameters\");\n");
            writer->write("$fclose(res_file);\n");
            writer->write("$fclose(file);\n");
            writer->write("$finish;\n");
         }
         writer->write(STR(STD_CLOSING_CHAR));
         writer->write("end\n");
         writer->write("else ");
         writer->write_comment("generic error\n");
         writer->write(STR(STD_OPENING_CHAR));
         writer->write("begin\n");
         {
            writer->write("$display(\"ERROR - Unknown error while reading the file. Character found: %c\", _ch_[7:0]);\n");
            writer->write("$fclose(res_file);\n");
            writer->write("$fclose(file);\n");
            writer->write("$finish;\n");
         }
         writer->write(STR(STD_CLOSING_CHAR));
         writer->write("end\n");
      }
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");
      writer->write("else\n");
      writer->write(STR(STD_OPENING_CHAR));
      writer->write("begin\n");
      {
         size_t escaped_pos = input_name.find('\\');
         std::string nonescaped_name = input_name;
         if(escaped_pos != std::string::npos) nonescaped_name.erase(std::remove(nonescaped_name.begin(), nonescaped_name.end(), '\\'), nonescaped_name.end());
         if(output_level > OUTPUT_LEVEL_MINIMUM) writer->write("$display(\"Value found for input " + nonescaped_name + ": %b\", " + input_name + ");\n");
      }
      writer->write(STR(STD_CLOSING_CHAR));
      writer->write("end\n");
      writer->write_comment("Value for " + input_name + " found ---------------------------------------------------------------\n");
   }
}

void TestbenchGenerationBaseStep::write_compute_ulps_functions() const
{
   writer->write("\n");
   writer->write("function real bits32_to_real64;\n");
   writer->write("  input [31:0] in1;\n");
   writer->write("  reg [7:0] exponent1;\n");
   writer->write("  reg is_exp_zero;\n");
   writer->write("  reg is_all_ones;\n");
   writer->write("  reg [10:0] exp_tmp;\n");
   writer->write("  reg [63:0] out1;\n");
   writer->write("begin\n");
   writer->write("  exponent1 = in1[30:23];\n");
   writer->write("  is_exp_zero = exponent1 == 8'd0;\n");
   writer->write("  is_all_ones = exponent1 == {8{1'b1}};\n");
   writer->write("  exp_tmp = {3'd0, exponent1} + 11'd896;\n");
   writer->write("  out1[63] = in1[31];\n");
   writer->write("  out1[62:52] = is_exp_zero ? 11'd0 : (is_all_ones ? {11{1'b1}} : exp_tmp);\n");
   writer->write("  out1[51:29] = in1[22:0];\n");
   writer->write("  out1[28:0] = 29'd0;\n");
   writer->write("  bits32_to_real64 = $bitstoreal(out1);\n");
   writer->write("end\n");
   writer->write("endfunction\n");

   writer->write("function real compute_ulp32;\n");
   writer->write("  input [31:0] computed;\n");
   writer->write("  input [31:0] expected;\n");
   writer->write("  real computedR;\n");
   writer->write("  real expectedR;\n");
   writer->write("  real diffR;\n");
   writer->write("  reg [31:0] denom;\n");
   writer->write("  real denomR;\n");
   writer->write("begin\n");
   writer->write("  if (expected[30:23] == {8{1'b1}} ||computed[30:23] == {8{1'b1}})\n");
   writer->write("    compute_ulp32 = computed != expected && (computed[22:0] == 23'd0 || expected[22:0] == 23'd0) ? {1'b0,({8{1'b1}}-8'd1),{23'b1} } : 32'd0;\n");
   writer->write("  else\n");
   writer->write("  begin\n");
   writer->write("    denom = 32'd0;\n");
   writer->write("    if (expected[30:0] == 31'd0)\n");
   writer->write("      denom[30:23] = 8'd104;\n");
   writer->write("    else\n");
   writer->write("      denom[30:23] = expected[30:23]-8'd23;\n");
   writer->write("    computedR = bits32_to_real64({1'b0, computed[30:0]});\n");
   writer->write("    expectedR = bits32_to_real64({1'b0, expected[30:0]});\n");
   writer->write("    denomR = bits32_to_real64(denom);\n");
   writer->write("    diffR = computedR - expectedR;\n");
   writer->write("    if(diffR < 0.0)\n");
   writer->write("      diffR = - diffR;\n");
   writer->write("    if (expected[30:0] == 31'd0 && computed[30:0] == 31'd0  && expected[31] != computed[31] )\n");
   writer->write("      compute_ulp32 = 1.0;\n");
   writer->write("    else\n");
   writer->write("      compute_ulp32 = diffR / denomR;\n");
   writer->write("  end\n");
   writer->write("end\n");
   writer->write("endfunction\n");
   writer->write("\n");
   writer->write("function real compute_ulp64;\n");
   writer->write("  input [63:0] computed;\n");
   writer->write("  input [63:0] expected;\n");
   writer->write("  real computedR;\n");
   writer->write("  real expectedR;\n");
   writer->write("  real diffR;\n");
   writer->write("  reg [63:0] denom;\n");
   writer->write("  real denomR;\n");
   writer->write("begin\n");
   writer->write("  if (expected[62:52] == {11{1'b1}} ||computed[62:52] == {11{1'b1}})\n");
   writer->write("    compute_ulp64 = computed != expected && (computed[51:0] == 52'd0 || expected[51:0] == 52'd0) ? {1'b0,({11{1'b1}}-11'd1),{52'b1} } : 64'd0;\n");
   writer->write("  else\n");
   writer->write("  begin\n");
   writer->write("    denom = 64'd0;\n");
   writer->write("    if (expected[62:0] == 63'd0)\n");
   writer->write("      denom[62:52] = 11'd971;\n");
   writer->write("    else\n");
   writer->write("      denom[62:52] = expected[62:52]-11'd52;\n");
   writer->write("    computedR = $bitstoreal({1'b0, computed[62:0]});\n");
   writer->write("    expectedR = $bitstoreal({1'b0, expected[62:0]});\n");
   writer->write("    denomR = $bitstoreal(denom);\n");
   writer->write("    diffR = computedR - expectedR;\n");
   writer->write("    if(diffR < 0.0)\n");
   writer->write("      diffR = - diffR;\n");
   writer->write("    if (expected[62:0] == 63'd0 && computed[62:0] == 63'd0  && expected[63] != computed[63] )\n");
   writer->write("      compute_ulp64 = 1.0;\n");
   writer->write("    else\n");
   writer->write("      compute_ulp64 = diffR / denomR;\n");
   writer->write("  end\n");
   writer->write("end\n");
   writer->write("endfunction\n");
}

bool TestbenchGenerationBaseStep::HasToBeExecuted() const { return true; }
