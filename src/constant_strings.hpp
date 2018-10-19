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
 * @file constant_strings.hpp
 * @brief constant strings
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * @author Marco Lattuada <lattuada@elet.polimi.it>
 * $Date$
 * Last modified by $Author$
 *
 */
#ifndef CONSTANT_OPTIONS_HPP
#define CONSTANT_OPTIONS_HPP

///Parameter of the current benchmark for table results (this value is not used for profiling)
#define STR_OPT_benchmark_fake_parameters "benchmark_fake_parameters"
///check diopsis
#define STR_OPT_check_diopsis "check_diopsis"



///The default fork cost
#define NUM_CST_fork_cost 10

///Digits in the address on sparc architecture
#define NUM_CST_SPARC_address_digit_size 8

///The number of surviving benchmarks
#define NUM_CST_surviving_benchmarks 300

///The default threshold
#define NUM_CST_task_threshold 4000



/**
 * Identifier of architecture elements
 */
///The string identifing a ARM component
#define STR_CST_ARM "ARM"
///The string identifing a BUS
#define STR_CST_BUS "BUS"
///The string identifing a DSP component
#define STR_CST_DSP "DSP"
///The string identifing a FPGA component
#define STR_CST_FPGA "FPGA"
///The string identifing a GPP component
#define STR_CST_GPP "GPP"




///The name of the default driving component
#define STR_CST_driving_component_name "<driving-component>"
///The file where output of grep of grmon is saved
#define STR_CST_grep_grmon_output "__grep_grmon_output"
///The file where output of grmon is saved
#define STR_CST_grmon_output "__grmon_output"
///The output file of hartes configuration
#define STR_CST_hartes_configuration_output_file_name "__hartes_configuration_output"
///The makefile
#define STR_CST_makefile "makefile"
///The string used to identify the code regions belonging to source functions
#define STR_CST_source_region "source_region"
///The output of ssh command
#define STR_CST_ssh_output "__ssh_output"
///The string used to identify the code regions belonging to system functions
#define STR_CST_system_region "system_region"
///The temporary directory pattern
#define STR_CST_temporary_directory "panda-temp"
///The file containing the execution trace generated by Simit
#define STR_CST_trace_xml "trace.xml"




/**
 * a3
 */
///The directory where temporary files created by a3 will be put
#define STR_CST_a3_batch_directory "__a3_batch_file.apd"

///The batch file for absint a3
#define STR_CST_a3_batch_file "__a3_batch_file.apx"

///The executable file for absint a3
#define STR_CST_a3_exec "a3"

///The results file for absint a3
#define STR_CST_a3_results "__a3_results.xml"

///The suffix of the file containing detailed information about WCET
#define STR_CST_a3_WCET_table_suffix "wcet-table-aiT.xml"

///The report file for absint a3
#define STR_CST_a3_xml_report "__a3_report.xml"



/**
 * Parameters
 */
///The string representing all classes
#define STR_CST_debug_all "ALL"



/**
 * Performance estimation
 */
///The number of "main" operations
#define NUM_CST_main_operations 1000


///The average case estimation technique
#define STR_CST_average_case "average_case"

///The path based estimation technique
#define STR_CST_path_based "path_based"

///The worst case estimation technique
#define STR_CST_worst_case "worst_case"



/**
 * Profiling analysis
 */
///The output file for rtl operation counters
#define STR_CST_rtl_operation_counters_file "rtl_operation_counters.xml"

///The output file for tree operation counters
#define STR_CST_tree_operation_counters_file "tree_operation_counters.xml"


/**
 * Variable estimation
 */
///Size of the column in output printing
#define NUM_CST_variable_estimation_column_size 25

///Precision of printed number
#define NUM_CST_variable_estimation_precision 3


/// interface_parameter_keyword
#define STR_CST_interface_parameter_keyword "_bambu_artificial_ParmMgr"



/**
 * XML Nodes: a3 files
 */
///The root node of report file
#define STR_XML_a3_a3 "a3"

///The node containing analyses performed by a3
#define STR_XML_a3_analyses "analyses"

///The node containing an analysis performed by a3
#define STR_XML_a3_analysis "analysis"

///The node containing cycles computed by a3
#define STR_XML_a3_cycles "cycles"

///The decode information about an object file
#define STR_XML_a3_decode "decode"

///The executable in a3 batch file
#define STR_XML_a3_executables "executables"

///The files section in a3 batch file
#define STR_XML_a3_files "files"

///The root node of batch file
#define STR_XML_a3_project "project"

///A node containing the result of an a3 analysis
#define STR_XML_a3_result "result"

///The root of the a3 result files
#define STR_XML_a3_results "results"

///Information about routine
#define STR_XML_a3_routine "routine"

///Information about routines
#define STR_XML_a3_routines "routines"

///The a3 configuration file node containing name of result file
#define STR_XML_a3_results_file "xml_results"

///The node containing information about a routine (a routine is a function or a loop)
#define STR_XML_a3_routine "routine"

///The a3 table containing detailed results
#define STR_XML_a3_table "wcettable"

///The cycles of a routine
#define STR_XML_a3_wce_cycles "wce_cycles"

///The cycles of a routine
#define STR_XML_a3_wce_cycles "wce_cycles"

///Node containing report of a wcet_analysis
#define STR_XML_a3_wcet_analysis_task "wcet_analysis_task"

///Root of wcet table
#define STR_XML_a3_wcettable "wcettable"

///The report file
#define STR_XML_a3_xml_report "xml_report"



/**
 * XML NOdes: profiling analysis
 */
///Information about number of absolute iterations of a loop
#define STR_XML_profiling_analysis_ABS_iterations "absolute_iterations"

///Arguments with which zebu has been invoked
#define STR_XML_profiling_analysis_args "args"

///Accesses to variable of array type
#define STR_XML_profiling_analysis_array_accesses "array_accesses"

///Information about number of average iterations of a loop
#define STR_XML_profiling_analysis_AVG_iterations "absolute_iterations"

///Node containing information about a single component
#define STR_XML_profiling_analysis_component "component"

///Attribute containing information about name of a component
#define STR_XML_profiling_analysis_component_name "name"

///Node containing information about tree node of a given type with a fixed number of constant operands
#define STR_XML_profiling_analysis_constant_operands "constant_operands_"

///Node containing information about cycles on target processing elemnt
#define STR_XML_profiling_analysis_cycles "cycles"

///Node containing information about dynamic operations
#define STR_XML_profiling_analysis_dynamic "dynamic_operations"

///Node containing information about memory footprint of a loop
#define STR_XML_profiling_analysis_footprint_loop_size "footprint_loop_size"

///Node contating information about a single function
#define STR_XML_profiling_analysis_function "function"

///Attribute containing name of a function
#define STR_XML_profiling_analysis_function_name "name"

///Name containing information about id of a loop
#define STR_XML_profiling_analysis_id "id"

///Node containing information about instruction size of a loop
#define STR_XML_profiling_analysis_instruction_loop_size "loop_instruction_size"

///Accesses to variable of integer type
#define STR_XML_profiling_analysis_integer_accesses "integer_accesses"

///Level at which analysis has been executed
#define STR_XML_profiling_analysis_level "level"

///Node containing information about characteristics of a loop
#define STR_XML_profiling_analysis_loop "loop"

///Node containing information about characteristics of loops
#define STR_XML_profiling_analysis_loops "loops"

///The node containing information about counter of single rtl_node of a given mode
#define STR_XML_profiling_analysis_rtl_mode "rtl_mode"

///Name of the benchmark
#define STR_XML_profiling_analysis_name "name"

///Node containing information about nesting of a loop
#define STR_XML_profiling_analysis_nesting "nesting"

///Path where zebu has been executed
#define STR_XML_profiling_analysis_path "path"

///Accesses to variable of pointer type
#define STR_XML_profiling_analysis_pointer_accesses "pointer_accesses"

///Accesses to variable of real type
#define STR_XML_profiling_analysis_real_accesses "real_accesses"

///The root node of the rtl operation counters
#define STR_XML_profiling_analysis_rtl_counter "rtl_operation_counters"

///The node containing information about counter of single rtl_node
#define STR_XML_profiling_analysis_rtl_node "rtl_node"

///Node containing information about static operations
#define STR_XML_profiling_analysis_static "static_operations"

///Attribute containing information about target considered during target profiling
#define STR_XML_profiling_analysis_target "target"

///Time when zebu has been executed
#define STR_XML_profiling_analysis_time "date"

///The root node of the tree operation counters
#define STR_XML_profiling_analysis_tree_counter "tree_operation_counters"

///The node containing information about counter of single tree_node
#define STR_XML_profiling_analysis_tree_node "tree_node"

///The node containing information abount counter of single tree_node of a given type
#define STR_XML_profiling_analysis_type "type"

///The value of a node
#define STR_XML_profiling_analysis_value "value"

///Zebu revision
#define STR_XML_profiling_analysis_zebu_revision "revision"

#endif
