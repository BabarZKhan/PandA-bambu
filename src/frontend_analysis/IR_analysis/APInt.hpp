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
 *              Copyright (C) 2004-2019 Politecnico di Milano
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
 * @file APInt.hpp
 * @brief
 *
 * @author Michele Fiorito <michele2.fiorito@mail.polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
 */
#ifndef APINT_HPP
#define APINT_HPP

#include <iostream>
#include <string>
#include <type_traits>

#include <boost/multiprecision/gmp.hpp>

class APInt
{
 public:
   using APInt_internal = boost::multiprecision::mpz_int;
   #if !defined(NDEBUG) && defined(__clang__)
   using bw_t = uint16_t; // Clang would not print uint8_t as number for some strange reason
   #else
   using bw_t = uint8_t;
   #endif

 private:
   APInt_internal _data;

 public:
   template <typename T>
   APInt(T val, typename std::enable_if<std::is_arithmetic<T>::value>* = nullptr)
   {
     _data = val;
   }

   APInt();
   APInt(const APInt& other);
   APInt& operator=(const APInt& other);
   ~APInt();

   friend bool operator<(const APInt& lhs, const APInt& rhs);
   friend bool operator>(const APInt& lhs, const APInt& rhs);
   friend bool operator<=(const APInt& lhs, const APInt& rhs);
   friend bool operator>=(const APInt& lhs, const APInt& rhs);
   friend bool operator==(const APInt& lhs, const APInt& rhs);
   friend bool operator!=(const APInt& lhs, const APInt& rhs);
   explicit operator bool() const;

   /*
    * Binary operators
    */
   friend APInt operator+(const APInt& lhs, const APInt& rhs);
   friend APInt operator-(const APInt& lhs, const APInt& rhs);
   friend APInt operator*(const APInt& lhs, const APInt& rhs);
   friend APInt operator/(const APInt& lhs, const APInt& rhs);
   friend APInt operator%(const APInt& lhs, const APInt& rhs);
   friend APInt operator&(const APInt& lhs, const APInt& rhs);
   friend APInt operator|(const APInt& lhs, const APInt& rhs);
   friend APInt operator^(const APInt& lhs, const APInt& rhs);
   friend APInt operator<<(const APInt& lhs, const APInt& rhs);
   friend APInt operator>>(const APInt& lhs, const APInt& rhs);
   APInt& operator+=(const APInt& rhs);
   APInt& operator-=(const APInt& rhs);
   APInt& operator*=(const APInt& rhs);
   APInt& operator/=(const APInt& rhs);
   APInt& operator%=(const APInt& rhs);
   APInt& operator&=(const APInt& rhs);
   APInt& operator|=(const APInt& rhs);
   APInt& operator^=(const APInt& rhs);
   APInt& operator<<=(const APInt& rhs);
   APInt& operator>>=(const APInt& rhs);

   /*
    * Unary operators
    */
   APInt abs() const;
   APInt operator-() const;
   APInt operator~() const;
   APInt operator++(int);
   APInt operator--(int);
   APInt& operator++();
   APInt& operator--();

   /*
    * Bitwise helpers
    */
   void bit_set(bw_t i);
   void bit_clr(bw_t i);
   bool bit_tst(bw_t i) const;
   bool sign() const;
   APInt& extOrTrunc(bw_t bw, bool sign);
   APInt extOrTrunc(bw_t bw, bool sign) const;
   bw_t trailingZeros(bw_t bw) const;
   bw_t trailingOnes(bw_t bw) const;
   bw_t leadingZeros(bw_t bw) const;
   bw_t leadingOnes(bw_t bw) const;
   bw_t minBitwidth(bool sign) const;

   int64_t toInt() const;
   uint64_t toIntUnsigned() const;
   template <typename T>
   T to(typename std::enable_if<std::is_arithmetic<T>::value && std::numeric_limits<T>::digits <= 64>* = nullptr) const
   {
      return static_cast<T>(toInt());
   }
   std::string str() const;

   static APInt getMaxValue(bw_t bw);
   static APInt getMinValue(bw_t bw);
   static APInt getSignedMaxValue(bw_t bw);
   static APInt getSignedMinValue(bw_t bw);
};

std::ostream& operator<<(std::ostream& str, const APInt& v);

#endif