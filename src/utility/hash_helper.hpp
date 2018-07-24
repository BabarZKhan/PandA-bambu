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
 * @file hash_helper.hpp
 * @brief This file collects some hash functors
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 *
*/
#ifndef HASH_HELPER_HPP
#define HASH_HELPER_HPP
#include <boost/functional/hash/hash.hpp>

///Hash function for std::vector
namespace std
{
   template<typename T>
      struct hash<std::vector<T> > : public std::unary_function<std::vector<T>, std::size_t>
      {
         std::size_t operator()(const std::vector<T> &val) const
         {
            return boost::hash_range<typename std::vector<T>::const_iterator>(val.begin(), val.end());
         }
      };
}

///Hash function for std::pair<T, U>
namespace std
{
   template<typename T, typename U>
      struct hash<std::pair<T, U> > : public std::unary_function<std::pair<T, U>, std::size_t>
      {
         private:
            const hash<T> Th;
            const hash<U> Uh;

         public:
            /**
             * Constructor
             */
            hash() :
               Th(),
               Uh()
            {}

            std::size_t operator()(const std::pair<T, U> &val) const
            {
               return Th(val.first) ^ Uh(val.second);
            }
      };
}
#endif
