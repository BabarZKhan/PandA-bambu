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
 *              Copyright (c) 2015-2018 Politecnico di Milano
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
 *
 * @author Pietro Fezzardi <pietrofezzardi@gmail.com>
 *
 */
#ifndef CALL_SITES_COLLECTOR_VISITOR_HPP
#define CALL_SITES_COLLECTOR_VISITOR_HPP

#include "call_graph.hpp"

#include <boost/graph/visitors.hpp>

#include <unordered_map>
#include <unordered_set>

CONSTREF_FORWARD_DECL(CallGraphManager);

class CallSitesCollectorVisitor : public boost::default_dfs_visitor
{
 private:
   /// The call graph manager
   const CallGraphManagerConstRef CGMan;
   /// Maps every function to the calls it performs
   std::unordered_map<unsigned int, std::unordered_set<unsigned int>>& fu_id_to_call_ids;
   /// Maps every id of a call site to the id of the called function
   std::unordered_map<unsigned int, std::unordered_set<unsigned int>>& call_id_to_called_id;
   /// Set of indirect calls
   std::unordered_set<unsigned int>& indirect_calls;

 public:
   /**
    * Constructor
    */
   CallSitesCollectorVisitor(CallGraphManagerConstRef cgman, std::unordered_map<unsigned int, std::unordered_set<unsigned int>>& _fu_id_to_call_ids, std::unordered_map<unsigned int, std::unordered_set<unsigned int>>& _call_id_to_called_id,
                             std::unordered_set<unsigned int>& _indirect_calls);

   /**
    * Destructor
    */
   ~CallSitesCollectorVisitor();

   void back_edge(const EdgeDescriptor&, const CallGraph&);

   void examine_edge(const EdgeDescriptor&, const CallGraph&);

   void discover_vertex(const vertex& v, const CallGraph&);
};
#endif
