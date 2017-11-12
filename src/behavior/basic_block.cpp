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
 * @file basic_block.cpp
 * @brief Class implementation of the basic_block structure.
 *
 * This file implements some of the basic_block member functions.
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * @author Marco Lattuada <lattuada@elet.polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
*/

///Autoheader include
#include "config_HAVE_HOST_PROFILING_BUILT.hpp"

///Header include
#include "basic_block.hpp"

///. include
#include "Parameter.hpp"

///behavior includes
#include "application_manager.hpp"
#include "behavioral_writer_helper.hpp"
#include "function_behavior.hpp"

///graph include
#include "graph.hpp"

///STD include
#include <fstream>

///tree includes
#include "behavioral_helper.hpp"
#include "tree_basic_block.hpp"

BBNodeInfo::BBNodeInfo() :
   loop_id(0),
   cer(0)
{}

BBNodeInfo::BBNodeInfo(blocRef _block) :
   loop_id(0),
   cer(0),
   block(_block)
{}

void BBNodeInfo::add_operation_node(const vertex op)
{
   statements_list.push_back(op);
}

size_t BBNodeInfo::size() const
{
   return statements_list.size();
}

vertex BBNodeInfo::get_first_operation() const
{
   return statements_list.front();
}

vertex BBNodeInfo::get_last_operation() const
{
   return statements_list.back();
}

bool BBNodeInfo::empty() const
{
   return statements_list.empty();
}

unsigned int BBNodeInfo::get_bb_index() const
{
   return block->number;
}

const std::set<unsigned int> & BBNodeInfo::get_live_in() const
{
   return block->live_in;
}

const std::set<unsigned int> & BBNodeInfo::get_live_out() const
{
   return block->live_out;
}

/*const std::map<unsigned int, unsigned int> & BBNodeInfo::get_live_out_phi_defs() const
{
   return block->live_out_phi_defs;
}*/

BBEdgeInfo::BBEdgeInfo() :
   epp_value(0)
{
}

BBEdgeInfo::~BBEdgeInfo()
{
}

void BBEdgeInfo::set_epp_value(unsigned long long _epp_value)
{
   epp_value = _epp_value;
}

unsigned long long BBEdgeInfo::get_epp_value() const
{
   return epp_value;
}

BBGraphInfo::BBGraphInfo(const application_managerConstRef _AppM, const unsigned int _function_index) :
   GraphInfo(),
   AppM(_AppM),
   function_index(_function_index),
   entry_vertex(NULL_VERTEX),
   exit_vertex(NULL_VERTEX)
{
}

BBGraphsCollection::BBGraphsCollection(const BBGraphInfoRef bb_graph_info, const ParameterConstRef _parameters)  :
   graphs_collection(RefcountCast<GraphInfo>(bb_graph_info), _parameters)
{}

BBGraphsCollection::~BBGraphsCollection()
{

}

BBGraph::BBGraph(const BBGraphsCollectionRef _g, int _selector) :
   graph(_g.get(), _selector)
{}

BBGraph::BBGraph(const BBGraphsCollectionRef _g, int _selector, std::unordered_set<vertex>& sub) :
   graph(_g.get(), _selector, sub)
{}

void BBGraph::WriteDot(const std::string& file_name, const int detail_level) const
{
   const std::unordered_set<vertex> annotated;
   WriteDot(file_name, annotated, detail_level);
}

void BBGraph::WriteDot(const std::string& file_name, const std::unordered_set<vertex> & annotated, const int) const
{
   const auto bb_graph_info = CGetBBGraphInfo();
   const auto function_name = bb_graph_info->AppM->CGetFunctionBehavior(bb_graph_info->function_index)->CGetBehavioralHelper()->get_function_name();
   std::string output_directory = collection->parameters->getOption<std::string>(OPT_dot_directory) + "/" + function_name + "/";
   if (not boost::filesystem::exists(output_directory))
      boost::filesystem::create_directories(output_directory);
   const std::string full_name = output_directory + file_name;
   const VertexWriterConstRef bb_writer(new BBWriter(this, annotated));
   const EdgeWriterConstRef bb_edge_writer(new BBEdgeWriter(this));
   InternalWriteDot<const BBWriter, const BBEdgeWriter>(full_name, bb_writer, bb_edge_writer);
}

size_t BBGraph::num_bblocks() const
{
   return boost::num_vertices(m_g) - 2;
}

bool BBEdgeInfo::cdg_edge_T() const
{
   return labels.find(CDG_SELECTOR) != labels.end() and labels.find(CDG_SELECTOR)->second.find(T_COND) != labels.find(CDG_SELECTOR)->second.end();
}

bool BBEdgeInfo::cdg_edge_F() const
{
   return labels.find(CDG_SELECTOR) != labels.end() and labels.find(CDG_SELECTOR)->second.find(F_COND) != labels.find(CDG_SELECTOR)->second.end();
}

bool BBEdgeInfo::cfg_edge_T() const
{
   return labels.find(CFG_SELECTOR) != labels.end() and labels.find(CFG_SELECTOR)->second.find(T_COND) != labels.find(CFG_SELECTOR)->second.end();
}

bool BBEdgeInfo::cfg_edge_F() const
{
   return labels.find(CFG_SELECTOR) != labels.end() and labels.find(CFG_SELECTOR)->second.find(F_COND) != labels.find(CFG_SELECTOR)->second.end();
}

bool BBEdgeInfo::switch_p() const
{
   return !cdg_edge_T() and !cdg_edge_F() and !cfg_edge_T() and !cfg_edge_F() and (labels.find(CDG_SELECTOR) != labels.end() or labels.find(CFG_SELECTOR) != labels.end());
}

const std::set<unsigned int> BBEdgeInfo::get_labels(const int selector) const
{
   if(labels.find(selector) != labels.end())
      return labels.find(selector)->second;
   else
      return std::set<unsigned int>();
}

#if ! HAVE_UNORDERED
BBVertexSorter::BBVertexSorter(const BBGraphConstRef _bb_graph) :
   bb_graph(_bb_graph)
{}

bool BBVertexSorter::operator()(const vertex x, const vertex y) const
{
   return bb_graph->CGetBBNodeInfo(x)->block->number < bb_graph->CGetBBNodeInfo(y)->block->number;
}

BBEdgeSorter::BBEdgeSorter(const BBGraphConstRef _bb_graph) :
   bb_graph(_bb_graph),
   bb_sorter(_bb_graph)
{}

bool BBEdgeSorter::operator()(const EdgeDescriptor x, const EdgeDescriptor y) const
{
   const vertex source_x = boost::source(x, *bb_graph);
   const vertex source_y = boost::source(y, *bb_graph);
   if(source_x == source_y)
      return bb_sorter(boost::target(x, *bb_graph), boost::target(y, *bb_graph));
   else
      return bb_sorter(source_x, source_y);
}
#endif
