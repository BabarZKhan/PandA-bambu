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
 * @file tree_basic_block.hpp
 * @brief Data structure describing a basic block at tree level.
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * @author Marco Lattuada <lattuada@elet.polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
*/
#ifndef TREE_BASIC_BLOCK_HPP
#define TREE_BASIC_BLOCK_HPP

///Autoheader include
#include "config_HAVE_BAMBU_BUILT.hpp"

///STD include
#include <iosfwd>

///STL include
#include <list>
#include <map>
#include <vector>
#include <set>

///tree include
///FIXME: this is require to define VISITED_OBJ_SEQ - it should be removed after refactoring of visitor
#include "tree_node.hpp"

///Utility include
#include "refcount.hpp"

/**
 * @name forward declarations
 */
//@{
REF_FORWARD_DECL(bloc);
REF_FORWARD_DECL(Schedule);
REF_FORWARD_DECL(tree_node);
class tree_node_visitor;
//@}


/**
 * constant identifying the basic block node of type entry
 */
#define BB_ENTRY 0

/**
 * constant identifying the basic block node of type exit
*/
#define BB_EXIT 1

/**
 * This struct specifies the field bloc (basic block).
  * The tree walker structure of this node is:
  * #(TOK_BLOC TOK_NUMBER (pred)+ (succ)+ (phi)* (stmt)*)
*/
struct bloc
{
   private:
      friend class use_counting;

      /// list_of_phi is a list of eventual phi node presents in the basic block.
      std::list<tree_nodeRef> list_of_phi;

      /// list_of_stmt is the list of statements stored in the basic block.
      std::list<tree_nodeRef> list_of_stmt;

      /// Number of removed phi
      size_t removed_phi;

      /// consistency of ssa uses
      bool updated_ssa_uses;

   public:
      /// list_of_pred is the list of predecessors.
      std::vector<unsigned int> list_of_pred;

      /// list_of_succ is the list of successors.
      std::vector<unsigned int> list_of_succ;

      /// number is the index of the basic block.
      const unsigned int number;

      /// loop identifier coming from GCC
      unsigned int loop_id;

      /// Live_In of the basic block
      std::set<unsigned int> live_in;

      /// Live_Out of the basic block
      std::set<unsigned int> live_out;

      /// store the relation between the phi defs inserted in the live_out set and phi uses
      //std::map<unsigned int, unsigned int> live_out_phi_defs;

      /// flag for header of reducible parallelizable loop
      unsigned int hpl;

      /// in case the last statement is a gimple_cond associated with an if statement this member contains the basic block reference to the then statements.
      unsigned int true_edge;

      /// in case the last statement is a gimple_cond associated with an if statement this member contains the basic block reference to the else statements.
      unsigned int false_edge;

#if HAVE_BAMBU_BUILT
      ///The reference to the schedule
      ScheduleRef schedule;
#endif

      /**
       * constructor
       */
      explicit bloc(unsigned int _number);

      /**
       * Destructor
       */
      virtual ~bloc();

      /**
       * Add a value to list of pred.
       * @param a is a pred.
       */
      void add_pred(const unsigned int a)
      {
         list_of_pred.push_back(a);
      }

      /**
       * Add a value to list of succ.
       * @param a is a succ.
       */
      void add_succ(const unsigned int a)
      {
         list_of_succ.push_back(a);
      }
      /**
       * Add a value to list of phi node.
       * @param a is a NODE_ID.
       */
      void AddPhi(const tree_nodeRef a);

      /**
       * Add a statement in front of list of statements.
       * @param statement is the statement to be added
       */
      void PushFront(const tree_nodeRef statement);

      /**
       * Add a statement as last non controllong statement
       * @param statement is the statement to be added
       */
      void PushBack(const tree_nodeRef statement);

      /**
       * Replace a statement with another one
       * @param old_stmt is the old statement to be removed
       * @param new_stmt is the new statement to be added
       * @param move_virtuals specifies if memdef and vdef have to be transferred
       */
      void Replace(const tree_nodeRef old_stmt, const tree_nodeRef new_stmt, const bool move_virtuals);

      /**
       * Add a statement before a specified one
       * @param new_stmt is the statement to be added
       * @param existing_stmt is the statement before which new_stmt has to be added
       */
      void PushBefore(const tree_nodeRef new_stmt, const tree_nodeRef existing_stmt);

      /**
       * Add a statement after a specified one
       * @param new_stmt is the statement to be added
       * @param existing_stmt is the statement after which new_stmt has to be added
       */
      void PushAfter(const tree_nodeRef new_stmt, const tree_nodeRef existing_stmt);

      /**
       * Remove a statement
       * @param statement is the statement to be removed
       */
      void RemoveStmt(const tree_nodeRef statement);

      /**
       * Remove a phi
       * @param phi is the phi to be removed
       */
      void RemovePhi(const tree_nodeRef phi);

      /**
       * Set that uses of ssa have been computed
       */
      void SetSSAUsesComputed();

      /**
       * Return the list of phi
       */
      const std::list<tree_nodeRef> & CGetPhiList() const;

      /**
       * Return the list of stmt
       */
      const std::list<tree_nodeRef> & CGetStmtList() const;

      /**
       * Return the number of removed phi
       */
      size_t CGetNumberRemovedPhi() const;

      /// constant indentify the entry basic block
      static const unsigned int ENTRY_BLOCK_ID;

      /// constant indentify the exit basic block
      static const unsigned int EXIT_BLOCK_ID;

      /**
       * virtual function used to traverse the tree_node data structure.
       * @param v is a reference to the tree_node visitor class
       */
      virtual void visit(tree_node_visitor * const v) const;

      ///visitor enum
      enum {GETID(list_of_phi)=0, GETID(list_of_stmt)};
};

#endif
