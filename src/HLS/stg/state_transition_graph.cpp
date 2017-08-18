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
 e   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/
/**
 * @file state_transition_graph.cpp
 * @brief File contanining the structures necessary to manage a graph that will represent a state transition graph
 *
 * This file contains the necessary data structures used to represent a state transition graph
 *
 * @author Christian Pilato <pilato@elet.polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
 */

///Autoheader include
#include "config_HAVE_HOST_PROFILING_BUILT.hpp"

///Header include
#include "state_transition_graph.hpp"

///. include
#include "Parameter.hpp"

///behavior includes
#if HAVE_HOST_PROFILING_BUILT
#include "profiling_information.hpp"
#endif
#include "op_graph.hpp"
#include "var_pp_functor.hpp"

///hls include
#include "hls.hpp"
#include "hls_manager.hpp"

///hls/scheduling
#include "schedule.hpp"

///tree include
#include "behavioral_helper.hpp"

///utility include
#include "simple_indent.hpp"

void StateInfo::print(std::ostream& os) const
{
   const auto function_behavior = HLSMgr.lock()->CGetFunctionBehavior(funId);
   const auto schedule = HLSMgr.lock()->get_HLS(funId)->Rsch;
   const auto critical_paths = schedule->ComputeCriticalPath(StateInfoConstRef(this, null_deleter()));
   const auto op_function_graph = function_behavior->CGetOpGraph(FunctionBehavior::CFG);
   const BehavioralHelperConstRef BH = op_function_graph->CGetOpGraphInfo()->BH;
   const var_pp_functorConstRef vpp = var_pp_functorConstRef(new std_var_pp_functor(BH));

   std::list<vertex>::const_iterator i, i_end;
   i_end = executing_operations.end();
   i = executing_operations.begin();

   if (i != i_end && GET_TYPE(op_function_graph, *i) == TYPE_ENTRY)
   {
      os << "START";
      return;
   }

   if (i != i_end && GET_TYPE(op_function_graph, *i) == TYPE_EXIT)
   {
      os << "END";
      return;
   }

   os << "< " << name << " | { ";

   for (const auto & op : executing_operations)
   {
      const auto first_index = op_function_graph->CGetOpNodeInfo(op)->GetNodeId();
      const bool critical = critical_paths.find(first_index) != critical_paths.end();
      if (std::find(ending_operations.begin(), ending_operations.end(), op) == ending_operations.end())
         os << "<font color=\"gold2\">";
      else if (critical)
         os << "<font color=\"red3\">";
      else if(GET_TYPE(op_function_graph, op) & TYPE_STORE)
         os << "<font color=\"blue\">";
      const auto first_starting_time = schedule->GetStartingTime(first_index);
      const auto first_ending_time = schedule->GetEndingTime(first_index);
      os << GET_NAME(op_function_graph, op) << " [" << NumberToString(first_starting_time,2,7) << "---" << NumberToString(first_ending_time,2,7) << "("<< NumberToString(first_ending_time-first_starting_time,2,7) << ")" << "] --&gt; ";
      std::string vertex_print = BH->print_vertex(op_function_graph, op, vpp, true);
      boost::replace_all(vertex_print, "&", "&amp;");
      boost::replace_all(vertex_print, "|", "\\|");
      boost::replace_all(vertex_print, ">", "&gt;");
      boost::replace_all(vertex_print, "<", "&lt;");
      boost::replace_all(vertex_print, "\\\\\"", "&#92;&quot;");
      boost::replace_all(vertex_print, "\\\"", "&quot;");
      boost::replace_all(vertex_print, ":", "&#58;");
      boost::replace_all(vertex_print, "\\n", "");
      boost::replace_all(vertex_print, "{", "\\{");
      boost::replace_all(vertex_print, "}", "\\}");
      os << vertex_print;
      if (critical or std::find(ending_operations.begin(), ending_operations.end(), op) == ending_operations.end() or GET_TYPE(op_function_graph, op) & TYPE_STORE)
         os << " </font>";
      os << "<br align=\"left\"/>";
   }
   os << " | ";
   for (const auto & op : ending_operations)
   {
      const auto first_index = op_function_graph->CGetOpNodeInfo(op)->GetNodeId();
      const auto critical = critical_paths.find(first_index) != critical_paths.end();
      if (std::find(executing_operations.begin(), executing_operations.end(), op) == executing_operations.end())
         os << "<font color=\"green2\">";
      else if (critical)
         os << "<font color=\"red3\">";
      else if (GET_TYPE(op_function_graph, op) & TYPE_STORE)
         os << "<font color=\"blue\">";
      const auto first_starting_time = schedule->GetStartingTime(first_index);
      const auto first_ending_time = schedule->GetEndingTime(first_index);
      os << GET_NAME(op_function_graph, op) << " [" << NumberToString(first_starting_time,2,7) << "---" << NumberToString(first_ending_time,2,7) << "("<< NumberToString(first_ending_time-first_starting_time,2,7) << ")" << "] --&gt; ";
      std::string vertex_print = BH->print_vertex(op_function_graph, op, vpp, true);
      boost::replace_all(vertex_print, "&", "&amp;");
      boost::replace_all(vertex_print, "|", "\\|");
      boost::replace_all(vertex_print, ">", "&gt;");
      boost::replace_all(vertex_print, "<", "&lt;");
      boost::replace_all(vertex_print, "\\\\\"", "&#92;&quot;");
      boost::replace_all(vertex_print, "\\\"", "&quot;");
      boost::replace_all(vertex_print, "\\n", "");
      boost::replace_all(vertex_print, ":", "&#58;");
      boost::replace_all(vertex_print, "{", "\\{");
      boost::replace_all(vertex_print, "}", "\\}");
      os << vertex_print;
      if (critical or std::find(executing_operations.begin(), executing_operations.end(), op) == executing_operations.end() or GET_TYPE(op_function_graph, op) & TYPE_STORE)
         os << " </font>";
      os << "<br align=\"left\"/>";
   }

   os << " } | ";

   if (BB_ids.size())
   {
         os << "BB_ids = ";
      for (const auto & bb_i : BB_ids)
         os << bb_i << " <br align=\"left\"/>";
   }
   else
   {
      os << "none";
   }
#if HAVE_HOST_PROFILING_BUILT
   if(function_behavior->CGetProfilingInformation() and BB_ids.size() == 1)
   {
      const auto BB_id = *(BB_ids.begin());
      const auto BB_vertex = function_behavior->CGetBBGraph(FunctionBehavior::BB)->CGetBBGraphInfo()->bb_index_map.find(BB_id)->second;
      const auto bb_executions = function_behavior->CGetProfilingInformation()->GetBBExecutions(BB_vertex);
      if(bb_executions)
         os << "<br/>Executed " << STR(bb_executions) << " times";
   }
#endif

   os << ">";
}

const unsigned int TransitionInfo::DONTCARE  = 1 << 0;

void TransitionInfo::print(std::ostream& os) const
{
   const BehavioralHelperConstRef BH = op_function_graph->CGetOpGraphInfo()->BH;
   for (std::set<std::pair<vertex, unsigned int> >::const_iterator it = conditions.begin(); it != conditions.end(); it++)
   {
      if (it->second == T_COND)
         os << GET_NAME(op_function_graph, it->first) << "(T)\\n";
      else if (it->second == F_COND)
         os << GET_NAME(op_function_graph, it->first) << "(F)\\n";
      else if (it->second == DONTCARE)
         os << GET_NAME(op_function_graph, it->first) << "(-)\\n";
      else if (it->second == default_COND)
         os << GET_NAME(op_function_graph, it->first) << "(default)\\n";
      else
      {
         const var_pp_functorConstRef std(new std_var_pp_functor(BH));
         os << GET_NAME(op_function_graph, it->first) << " (" << BH->print_node(it->second, it->first, std) << ")\\n";
      }
   }
}

StateTransitionGraphInfo::StateTransitionGraphInfo(const OpGraphConstRef _op_function_graph) :
   op_function_graph(_op_function_graph), is_a_dag(true), min_cycles(0), max_cycles(0)
{}

StateTransitionGraphsCollection::StateTransitionGraphsCollection(const StateTransitionGraphInfoRef state_transition_graph_info, const ParameterConstRef _parameters) :
   graphs_collection(state_transition_graph_info, _parameters)
{}

StateTransitionGraphsCollection::~StateTransitionGraphsCollection()
{}

StateTransitionGraph::StateTransitionGraph(const StateTransitionGraphsCollectionRef state_transition_graphs_collection, int _selector) :
   graph(state_transition_graphs_collection.get(), _selector)
{}

StateTransitionGraph::StateTransitionGraph(const StateTransitionGraphsCollectionRef state_transition_graphs_collection, int _selector, std::unordered_set<vertex> _sub) :
   graph(state_transition_graphs_collection.get(), _selector, _sub)
{}

StateTransitionGraph::~StateTransitionGraph()
{}

void StateTransitionGraph::WriteDot(const std::string & file_name, const int) const
{
   const std::string output_directory = collection->parameters->getOption<std::string>(OPT_dot_directory);
   CustomSet<unsigned int> critical_paths;
   VertexIterator state, state_end;
   for(boost::tie(state, state_end) = boost::vertices(*this); state != state_end; state++)
   {
      StateInfoConstRef si = CGetStateInfo(*state);
      const auto critical_path = si->HLSMgr.lock()->get_HLS(si->funId)->Rsch->ComputeCriticalPath(CGetStateInfo(*state));
      critical_paths.insert(critical_path.begin(), critical_path.end());
   }
   if (!boost::filesystem::exists(output_directory))
      boost::filesystem::create_directories(output_directory);
   const OpGraphConstRef op_function_graph = CGetStateTransitionGraphInfo()->op_function_graph;
   const std::string function_name = op_function_graph->CGetOpGraphInfo()->BH->get_function_name();
   const std::string complete_file_name = output_directory + function_name +"/";
   if (!boost::filesystem::exists(complete_file_name))
      boost::filesystem::create_directories(complete_file_name);
   const VertexWriterConstRef state_writer(new StateWriter(this, op_function_graph));
   const EdgeWriterConstRef transition_writer(new TransitionWriter(this, op_function_graph));
   InternalWriteDot<const StateWriter, const TransitionWriter>(complete_file_name+file_name, state_writer, transition_writer);
   for(boost::tie(state, state_end) = boost::vertices(*this); state != state_end; state++)
   {
      if(*state == CGetStateTransitionGraphInfo()->entry_node or *state == CGetStateTransitionGraphInfo()->exit_node)
      {
         continue;
      }
      const auto state_info = CGetStateInfo(*state);
      OpVertexSet state_ops = OpVertexSet(op_function_graph);
      state_ops.insert(state_info->starting_operations.begin(), state_info->starting_operations.end());
      state_ops.insert(state_info->ending_operations.begin(), state_info->ending_operations.end());
      const auto st_op_graph = state_info->HLSMgr.lock()->CGetFunctionBehavior(state_info->funId)->CGetOpGraph(FunctionBehavior::DFG, state_ops);
      auto st_file_name = file_name.substr(0, file_name.find(".dot"));
      st_op_graph->WriteDot(st_file_name + "_" + state_info->name + ".dot", state_info->HLSMgr.lock()->get_HLS(state_info->funId), state_info->HLSMgr.lock()->get_HLS(state_info->funId)->Rsch->ComputeCriticalPath(state_info));
   }
}

StateWriter::StateWriter(const graph* _stg, const OpGraphConstRef _op_function_graph) :
   VertexWriter(_stg, 0),
   BH(_op_function_graph->CGetOpGraphInfo()->BH),
   op_function_graph(_op_function_graph),
   entry_node(GetPointer<const StateTransitionGraphInfo>(_stg->CGetGraphInfo())->entry_node),
   exit_node(GetPointer<const StateTransitionGraphInfo>(_stg->CGetGraphInfo())->exit_node)
{}

void StateWriter::operator()(std::ostream& out, const vertex& v) const
{
   const StateInfo *temp = Cget_node_info< StateInfo > (v, *printing_graph);
   out << "[";
   if (v == entry_node or v == exit_node)
      out << "color=blue,shape=Msquare,";
   else
      out << "shape=record,";
   out << "label=";
   temp->print(out);
   out << "]";
}

TransitionWriter::TransitionWriter(const graph* _stg, const OpGraphConstRef _op_function_graph) :
   EdgeWriter(_stg, 0),
   BH(_op_function_graph->CGetOpGraphInfo()->BH),
   op_function_graph(_op_function_graph)
{}

void TransitionWriter::operator()(std::ostream& out, const EdgeDescriptor& e) const
{
   const TransitionInfo *temp = Cget_edge_info<TransitionInfo>(e, *printing_graph);
   if (ST_EDGE_NORMAL_T & printing_graph->GetSelector(e))
   {
      out << "[color=red3";
   }
   else if (ST_EDGE_FEEDBACK_T & printing_graph->GetSelector(e))
   {
      out << "[color=green2";
   }
   else
   {
      THROW_UNREACHABLE("InconsistentDataStructure");
   }
   out << ",label=\"";
   temp->print(out);
   out << "\"]";
}
