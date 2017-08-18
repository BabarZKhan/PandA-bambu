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
 *              Copyright (c) 2004-2017 Politecnico di Milano
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
 * @file hls_instruction_writer.cpp
 * @brief Simple class to print single instruction
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * @author Marco Lattuada <lattuada@elet.polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
*/

///Header include
#include "hls_instruction_writer.hpp"

///behavior include
#include "application_manager.hpp"
#include "function_behavior.hpp"

///tree include
#include "behavioral_helper.hpp"
#include "var_pp_functor.hpp"

///utility include
#include "indented_output_stream.hpp"
#include "utility.hpp"

///Backend include
#include "c_writer.hpp"
#include "tree_manager.hpp"
#include "tree_node.hpp"
#include "tree_reindex.hpp"

///Parameter include
#include "Parameter.hpp"

HLSInstructionWriter::HLSInstructionWriter(const application_managerConstRef _app_man, const IndentedOutputStreamRef _indented_output_stream, const ParameterConstRef _parameters) :
   InstructionWriter(_app_man, _indented_output_stream, _parameters)
{}

HLSInstructionWriter::~HLSInstructionWriter()
{}

void HLSInstructionWriter::declareFunction(const unsigned int function_id)
{
   //All I have to do is to change main in _main
   const BehavioralHelperConstRef behavioral_helper =
      AppM->CGetFunctionBehavior(function_id)->CGetBehavioralHelper();
   std::string stringTemp = AppM->CGetFunctionBehavior(function_id)->
      CGetBehavioralHelper()->print_type
      (
         function_id,
         false,
         true,
         false,
         0,
         var_pp_functorConstRef(new std_var_pp_functor(behavioral_helper))
      );
   bool flag_cpp;
   if(parameters->isOption(OPT_input_format) &&
         parameters->getOption<Parameters_FileFormat>(OPT_input_format) == Parameters_FileFormat::FF_CPP &&
         !parameters->isOption(OPT_pretty_print))
      flag_cpp = true;
   else
      flag_cpp = false;



   std::string name = AppM->CGetFunctionBehavior(function_id)->CGetBehavioralHelper()->get_function_name();
   if(name == "main")
   {
      boost::replace_all(stringTemp, " main(", " _main("); /// the assumption is strong but the code that prints the name of the function is under our control ;-)
   }
   if(flag_cpp)
   {
      tree_nodeRef fd_node = AppM->get_tree_manager()->get_tree_node_const(function_id);
      function_decl * fd = GetPointer<function_decl>(fd_node);
      std::string simple_name;
      tree_nodeRef id_name = GET_NODE(fd->name);
      if (id_name->get_kind() == identifier_node_K)
      {
         identifier_node *in = GetPointer<identifier_node>(id_name);
         if(!in->operator_flag)
            simple_name = in->strg;
      }
      if(simple_name != "")
      {
         boost::replace_all(stringTemp, " " + name + "(", " " + simple_name + "(");
      }
      boost::replace_all(stringTemp, "/*&*/*", "&");
   }

   indented_output_stream->Append(stringTemp);
}
