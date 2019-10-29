/*
 *
 *                   _/_/_/    _/_/   _/    _/ _/_/_/    _/_/
 *                  _/   _/ _/    _/ _/_/  _/ _/   _/ _/    _/
 *                 _/_/_/  _/_/_/_/ _/  _/_/ _/   _/ _/_/_/_/
 *                _/      _/    _/ _/    _/ _/   _/ _/    _/
 *               _/      _/    _/ _/    _/ _/_/_/  _/    _/
 *
 *             **********************************************
 *                             Panda Project
 *                  URL: http://trac.elet.polimi.it/panda
 *                      Micro Architecture Laboratory
 *                       Politecnico di Milano - DEIB
 *             **********************************************
 *             Copyright (C) 2004-2019 Politecnico di Milano
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 */
/**
 * @file token_interface.hpp
 * @brief A simple interface to token object of the raw files.
 *
 * Interface specification to token objects.
 *
 * @author Fabrizio Ferrandi <ferrandi.elet.polimi.it>
 * @author Marco Lattuada <lattuada@elet.polimi.it>
 * @version $Revision$
 * @date $Date$
 * Last modified by $Author$
 *
 */
#ifndef TOKEN_INTEFACE_HPP
#define TOKEN_INTEFACE_HPP

#include "custom_map.hpp"
#include <cstring>
#include <string>

enum class TreeVocabularyTokenTypes_TokenEnum
{
   FIRST_TOKEN = -1,
   TOK_GCC_VERSION,
   TOK_PLUGIN_VERSION,
   TOK_IDENTIFIER_NODE,
   TOK_TREE_LIST,
   TOK_TREE_VEC,
   TOK_BLOCK,
   TOK_VOID_TYPE,
   TOK_INTEGER_TYPE,
   TOK_REAL_TYPE,
   TOK_COMPLEX_TYPE,
   TOK_TYPE_ARGUMENT_PACK,
   TOK_NONTYPE_ARGUMENT_PACK,
   TOK_EXPR_PACK_EXPANSION,
   TOK_VECTOR_TYPE,
   TOK_ENUMERAL_TYPE,
   TOK_BOOLEAN_TYPE,
   TOK_CHAR_TYPE,
   TOK_NULLPTR_TYPE,
   TOK_ERROR_MARK,
   TOK_TYPE_PACK_EXPANSION,
   TOK_POINTER_TYPE,
   TOK_OFFSET_TYPE,
   TOK_REFERENCE_TYPE,
   TOK_METHOD_TYPE,
   TOK_ARRAY_TYPE,
   TOK_SET_TYPE,
   TOK_RECORD_TYPE,
   TOK_UNION_TYPE,
   TOK_QUAL_UNION_TYPE,
   TOK_FUNCTION_TYPE,
   TOK_LANG_TYPE,
   TOK_INTEGER_CST,
   TOK_REAL_CST,
   TOK_COMPLEX_CST,
   TOK_VECTOR_CST,
   TOK_VOID_CST,
   TOK_STRING_CST,
   TOK_FUNCTION_DECL,
   TOK_LABEL_DECL,
   TOK_CONST_DECL,
   TOK_TYPE_DECL,
   TOK_USING_DECL,
   TOK_VAR_DECL,
   TOK_PARM_DECL,
   TOK_PARAM_PACKS,
   TOK_RESULT_DECL,
   TOK_FIELD_DECL,
   TOK_NAMESPACE_DECL,
   TOK_TRANSLATION_UNIT_DECL,
   TOK_COMPONENT_REF,
   TOK_BIT_FIELD_REF,
   TOK_INDIRECT_REF,
   TOK_MISALIGNED_INDIRECT_REF,
   TOK_BUFFER_REF,
   TOK_ARRAY_REF,
   TOK_ARRAY_RANGE_REF,
   TOK_VTABLE_REF,
   TOK_CONSTRUCTOR,
   TOK_DESTRUCTOR,
   TOK_COMPOUND_EXPR,
   TOK_MODIFY_EXPR,
   TOK_GIMPLE_ASSIGN,
   TOK_INIT_EXPR,
   TOK_TARGET_EXPR,
   TOK_COND_EXPR,
   TOK_GIMPLE_COND,
   TOK_WHILE_EXPR,
   TOK_FOR_EXPR,
   TOK_GIMPLE_BIND,
   TOK_GIMPLE_CALL,
   TOK_CALL_EXPR,
   TOK_AGGR_INIT_EXPR,
   TOK_GIMPLE_NOP,
   TOK_WITH_CLEANUP_EXPR,
   TOK_CLEANUP_POINT_EXPR,
   TOK_PLACEHOLDER_EXPR,
   TOK_REDUC_MAX_EXPR,
   TOK_REDUC_MIN_EXPR,
   TOK_REDUC_PLUS_EXPR,
   TOK_PLUS_EXPR,
   TOK_TERNARY_PLUS_EXPR,
   TOK_TERNARY_PM_EXPR,
   TOK_TERNARY_MP_EXPR,
   TOK_TERNARY_MM_EXPR,
   TOK_BIT_IOR_CONCAT_EXPR,
   TOK_MINUS_EXPR,
   TOK_MULT_EXPR,
   TOK_TRUNC_DIV_EXPR,
   TOK_CEIL_DIV_EXPR,
   TOK_FLOOR_DIV_EXPR,
   TOK_ROUND_DIV_EXPR,
   TOK_TRUNC_MOD_EXPR,
   TOK_CEIL_MOD_EXPR,
   TOK_FLOOR_MOD_EXPR,
   TOK_ROUND_MOD_EXPR,
   TOK_RDIV_EXPR,
   TOK_EXACT_DIV_EXPR,
   TOK_FIX_TRUNC_EXPR,
   TOK_FIX_CEIL_EXPR,
   TOK_FIX_FLOOR_EXPR,
   TOK_FIX_ROUND_EXPR,
   TOK_FLOAT_EXPR,
   TOK_LUT_EXPR,
   TOK_NEGATE_EXPR,
   TOK_MIN_EXPR,
   TOK_MAX_EXPR,
   TOK_ABS_EXPR,
   TOK_LSHIFT_EXPR,
   TOK_RSHIFT_EXPR,
   TOK_LROTATE_EXPR,
   TOK_RROTATE_EXPR,
   TOK_BIT_IOR_EXPR,
   TOK_BIT_XOR_EXPR,
   TOK_BIT_AND_EXPR,
   TOK_BIT_NOT_EXPR,
   TOK_TRUTH_ANDIF_EXPR,
   TOK_TRUTH_ORIF_EXPR,
   TOK_TRUTH_AND_EXPR,
   TOK_TRUTH_OR_EXPR,
   TOK_TRUTH_XOR_EXPR,
   TOK_TRUTH_NOT_EXPR,
   TOK_LT_EXPR,
   TOK_LE_EXPR,
   TOK_GT_EXPR,
   TOK_GE_EXPR,
   TOK_EQ_EXPR,
   TOK_NE_EXPR,
   TOK_UNORDERED_EXPR,
   TOK_ORDERED_EXPR,
   TOK_UNLT_EXPR,
   TOK_UNLE_EXPR,
   TOK_UNGT_EXPR,
   TOK_UNGE_EXPR,
   TOK_UNEQ_EXPR,
   TOK_LTGT_EXPR,
   TOK_IN_EXPR,
   TOK_SET_LE_EXPR,
   TOK_CARD_EXPR,
   TOK_RANGE_EXPR,
   TOK_PAREN_EXPR,
   TOK_CONVERT_EXPR,
   TOK_NOP_EXPR,
   TOK_NON_LVALUE_EXPR,
   TOK_VIEW_CONVERT_EXPR,
   TOK_SAVE_EXPR,
   TOK_UNSAVE_EXPR,
   TOK_ADDR_EXPR,
   TOK_REFERENCE_EXPR,
   TOK_FDESC_EXPR,
   TOK_COMPLEX_EXPR,
   TOK_CONJ_EXPR,
   TOK_REALPART_EXPR,
   TOK_IMAGPART_EXPR,
   TOK_PREDECREMENT_EXPR,
   TOK_PREINCREMENT_EXPR,
   TOK_POSTDECREMENT_EXPR,
   TOK_POSTINCREMENT_EXPR,
   TOK_VA_ARG_EXPR,
   TOK_TRY_CATCH_EXPR,
   TOK_TRY_FINALLY,
   TOK_GIMPLE_LABEL,
   TOK_GIMPLE_GOTO,
   TOK_GOTO_SUBROUTINE,
   TOK_GIMPLE_RETURN,
   TOK_EXIT_EXPR,
   TOK_LOOP_EXPR,
   TOK_GIMPLE_SWITCH,
   TOK_GIMPLE_MULTI_WAY_IF,
   TOK_CASE_LABEL_EXPR,
   TOK_GIMPLE_RESX,
   TOK_GIMPLE_ASM,
   TOK_SSA_NAME,
   TOK_ADDR_STMT,
   TOK_DEF_STMT,
   TOK_USE_STMT,
   TOK_GIMPLE_PHI,
   TOK_CATCH_EXPR,
   TOK_EH_FILTER_EXPR,
   TOK_STATEMENT_LIST,
   TOK_TEMPLATE_DECL,
   TOK_TEMPLATE_TYPE_PARM,
   TOK_CAST_EXPR,
   TOK_STATIC_CAST_EXPR,
   TOK_TYPENAME_TYPE,
   TOK_SIZEOF_EXPR,
   TOK_SCOPE_REF,
   TOK_CTOR_INITIALIZER,
   TOK_DO_STMT,
   TOK_EXPR_STMT,
   TOK_FOR_STMT,
   TOK_IF_STMT,
   TOK_RETURN_STMT,
   TOK_WHILE_STMT,
   TOK_MODOP_EXPR,
   TOK_NEW_EXPR,
   TOK_VEC_COND_EXPR,
   TOK_VEC_PERM_EXPR,
   TOK_DOT_PROD_EXPR,
   TOK_VEC_LSHIFT_EXPR,
   TOK_VEC_RSHIFT_EXPR,
   TOK_WIDEN_MULT_HI_EXPR,
   TOK_WIDEN_MULT_LO_EXPR,
   TOK_VEC_UNPACK_HI_EXPR,
   TOK_VEC_UNPACK_LO_EXPR,
   TOK_VEC_UNPACK_FLOAT_HI_EXPR,
   TOK_VEC_UNPACK_FLOAT_LO_EXPR,
   TOK_VEC_PACK_TRUNC_EXPR,
   TOK_VEC_PACK_SAT_EXPR,
   TOK_VEC_PACK_FIX_TRUNC_EXPR,
   TOK_VEC_EXTRACTEVEN_EXPR,
   TOK_VEC_EXTRACTODD_EXPR,
   TOC_VEC_INTERLEAVEHIGH_EXPR,
   TOC_VEC_INTERLEAVELOW_EXPR,
   TOK_VEC_NEW_EXPR,
   TOK_OVERLOAD,
   TOK_REINTERPRET_CAST_EXPR,
   TOK_TEMPLATE_ID_EXPR,
   TOK_THROW_EXPR,
   TOK_TRY_BLOCK,
   TOK_ARROW_EXPR,
   TOK_HANDLER,
   TOK_BASELINK,
   TOK_NAME,
   TOK_TYPE,
   TOK_SRCP,
   TOK_ARG,
   TOK_BODY,
   TOK_STRG,
   TOK_LNGT,
   TOK_SIZE,
   TOK_ALGN,
   TOK_RETN,
   TOK_PRMS,
   TOK_SCPE,
   TOK_BB_INDEX,
   TOK_USED,
   TOK_VALUE,
   TOK_ARGT,
   TOK_PREC,
   TOK_MIN,
   TOK_MAX,
   TOK_BIT_VALUES,
   TOK_VALU,
   TOK_CHAN,
   TOK_STMT,
   TOK_OP,
   TOK_OP0,
   TOK_OP1,
   TOK_OP2,
   TOK_OP3,
   TOK_OP4,
   TOK_OP5,
   TOK_OP6,
   TOK_OP7,
   TOK_OP8,
   TOK_VARS,
   TOK_UNQL,
   TOK_ELTS,
   TOK_DOMN,
   TOK_BLOC,
   TOK_DCLS,
   TOK_MNGL,
   TOK_PTD,
   TOK_REFD,
   TOK_QUAL,
   TOK_VALR,
   TOK_VALX,
   TOK_FLDS,
   TOK_VFLD,
   TOK_BPOS,
   TOK_FN,
   TOK_GOTO,
   TOK_REAL,
   TOK_IMAG,
   TOK_BASES,
   TOK_BINFO,
   TOK_PUB,
   TOK_PROT,
   TOK_PRIV,
   TOK_BINF,
   TOK_UID,
   TOK_OLD_UID,
   TOK_INIT,
   TOK_FINI,
   TOK_PURP,
   TOK_PRED,
   TOK_SUCC,
   TOK_PHI,
   TOK_RES,
   TOK_DEF,
   TOK_EDGE,
   TOK_VAR,
   TOK_VERS,
   TOK_ORIG_VERS,
   TOK_CNST,
   TOK_CLAS,
   TOK_DECL,
   TOK_CLNP,
   TOK_LAB,
   TOK_TRY,
   TOK_EX,
   TOK_OUT,
   TOK_IN,
   TOK_STR,
   TOK_CLOB,
   TOK_CLOBBER,
   TOK_REF,
   TOK_FNCS,
   TOK_CSTS,
   TOK_RSLT,
   TOK_INST,
   TOK_SPCS,
   TOK_CLS,
   TOK_BFLD,
   TOK_CTOR,
   TOK_NEXT,
   TOK_COND,
   TOK_EXPR,
   TOK_THEN,
   TOK_ELSE,
   TOK_CRNT,
   TOK_HDLR,
   TOK_ARTIFICIAL,
   TOK_SYSTEM,
   TOK_OPERATING_SYSTEM,
   TOK_LIBRARY_SYSTEM,
   TOK_LIBBAMBU,
   TOK_EXTERN,
   TOK_ADDR_TAKEN,
   TOK_ADDR_NOT_TAKEN,
   TOK_C,
   TOK_LSHIFT,
   TOK_GLOBAL_INIT,
   TOK_GLOBAL_FINI,
   TOK_UNDEFINED,
   TOK_BUILTIN,
   TOK_HWCALL,
   TOK_OPERATOR,
   TOK_OVERFLOW,
   TOK_VIRT,
   TOK_UNSIGNED,
   TOK_STRUCT,
   TOK_UNION,
   TOK_CONSTANT,
   TOK_READONLY,
   TOK_REGISTER,
   TOK_STATIC,
   TOK_STATIC_STATIC,
   TOK_REVERSE_RESTRICT,
   TOK_WRITING_MEMORY,
   TOK_READING_MEMORY,
   TOK_OMP_ATOMIC,
   TOK_OMP_BODY_LOOP,
   TOK_OMP_CRITICAL_SESSION,
   TOK_OMP_FOR_WRAPPER,
   TOK_DEFAULT,
   TOK_VOLATILE,
   TOK_VARARGS,
   TOK_INF,
   TOK_NAN,
   TOK_ENTRY,
   TOK_EXIT,
   TOK_NEW,
   TOK_DELETE,
   TOK_ASSIGN,
   TOK_MEMBER,
   TOK_PUBLIC,
   TOK_PRIVATE,
   TOK_PROTECTED,
   TOK_NORETURN,
   TOK_NOINLINE,
   TOK_ALWAYS_INLINE,
   TOK_UNUSED,
   TOK_CONST,
   TOK_TRANSPARENT_UNION,
   TOK_MODE,
   TOK_SECTION,
   TOK_ALIGNED,
   TOK_PACKED,
   TOK_WEAK,
   TOK_ALIAS,
   TOK_NO_INSTRUMENT_FUNCTION,
   TOK_MALLOC,
   TOK_NO_STACK_LIMIT,
   TOK_NO_STACK,
   TOK_PURE,
   TOK_DEPRECATED,
   TOK_VECTOR_SIZE,
   TOK_VISIBILITY,
   TOK_TLS_MODEL,
   TOK_NONNULL,
   TOK_NOTHROW,
   TOK_MAY_ALIAS,
   TOK_WARN_UNUSED_RESULT,
   TOK_FORMAT,
   TOK_FORMAT_ARG,
   TOK_NULL,
   TOK_CONVERSION,
   TOK_VIRTUAL,
   TOK_MUTABLE,
   TOK_PSEUDO_TMPL,
   TOK_SPEC,
   TOK_LINE,
   TOK_FIXD,
   TOK_VECNEW,
   TOK_VECDELETE,
   TOK_POS,
   TOK_NEG,
   TOK_ADDR,
   TOK_DEREF,
   TOK_NOT,
   TOK_LNOT,
   TOK_PREINC,
   TOK_PREDEC,
   TOK_PLUSASSIGN,
   TOK_PLUS,
   TOK_MINUSASSIGN,
   TOK_MINUS,
   TOK_MULTASSIGN,
   TOK_MULT,
   TOK_DIVASSIGN,
   TOK_DIV,
   TOK_MODASSIGN,
   TOK_MOD,
   TOK_ANDASSIGN,
   TOK_AND,
   TOK_ORASSIGN,
   TOK_OR,
   TOK_XORASSIGN,
   TOK_XOR,
   TOK_LSHIFTASSIGN,
   TOK_RSHIFTASSIGN,
   TOK_RSHIFT,
   TOK_EQ,
   TOK_NE,
   TOK_LT,
   TOK_GT,
   TOK_LE,
   TOK_GE,
   TOK_LAND,
   TOK_LOR,
   TOK_COMPOUND,
   TOK_MEMREF,
   TOK_SUBS,
   TOK_POSTINC,
   TOK_POSTDEC,
   TOK_CALL,
   TOK_THUNK,
   TOK_THIS_ADJUSTING,
   TOK_RESULT_ADJUSTING,
   TOK_PTRMEM,
   TOK_QUAL_R,
   TOK_QUAL_V,
   TOK_QUAL_VR,
   TOK_QUAL_C,
   TOK_QUAL_CR,
   TOK_QUAL_CV,
   TOK_QUAL_CVR,
   TOK_USE_TMPL,
   TOK_TMPL_PARMS,
   TOK_TMPL_ARGS,
   TOK_TEMPLATE_PARM_INDEX,
   TOK_INDEX,
   TOK_LEVEL,
   TOK_ORIG_LEVEL,
   TOK_INLINE_BODY,
   TOK_BITFIELD,
   TOK_WITH_SIZE_EXPR,
   TOK_OBJ_TYPE_REF,
   TOK_MEMUSE,
   TOK_MEMDEF,
   TOK_VUSE,
   TOK_VDEF,
   TOK_VOVER,
   TOK_PTR_INFO,
   TOK_TRUE_EDGE,
   TOK_FALSE_EDGE,
   TOK_POINTER_PLUS_EXPR,
   TOK_TARGET_MEM_REF,
   TOK_TARGET_MEM_REF461,
   TOK_MEM_REF,
   TOK_WIDEN_SUM_EXPR,
   TOK_WIDEN_MULT_EXPR,
   TOK_MULT_HIGHPART_EXPR,
   TOK_EXTRACT_BIT_EXPR,
   TOK_ASSERT_EXPR,
   TOK_SYMBOL,
   TOK_BASE,
   TOK_IDX,
   TOK_IDX2,
   TOK_STEP,
   TOK_OFFSET,
   TOK_ORIG,
   TOK_TAG,
   TOK_SMT_ANN,
   TOK_TRAIT_EXPR,
   TOK_TIME_WEIGHT,
   TOK_SIZE_WEIGHT,
   TOK_RTL_SIZE_WEIGHT,
   TOK_HPL,
   TOK_LOOP_ID,
   TOK_ATTRIBUTES,
   TOK_PRAGMA,
   TOK_PRAGMA_SCOPE,
   TOK_PRAGMA_DIRECTIVE,
   TOK_PRAGMA_OMP,
   TOK_PRAGMA_OMP_CRITICAL,
   TOK_PRAGMA_OMP_DECLARE_SIMD,
   TOK_PRAGMA_OMP_FOR,
   TOK_PRAGMA_OMP_PARALLEL,
   TOK_PRAGMA_OMP_PARALLEL_SECTIONS,
   TOK_PRAGMA_OMP_SECTION,
   TOK_PRAGMA_OMP_SECTIONS,
   TOK_PRAGMA_OMP_SHORTCUT,
   TOK_PRAGMA_OMP_SIMD,
   TOK_PRAGMA_OMP_TARGET,
   TOK_PRAGMA_OMP_TASK,
   TOK_PRAGMA_MAP,
   TOK_PRAGMA_CALL_HW,
   TOK_PRAGMA_CALL_POINT_HW,
   TOK_HW_COMPONENT,
   TOK_ID_IMPLEMENTATION,
   TOK_RECURSIVE,
   TOK_PRAGMA_ISSUE,
   TOK_PRAGMA_BLACKBOX,
   TOK_PRAGMA_PROFILING,
   TOK_PRAGMA_PROFILING_STATISTICAL,
   TOK_OPEN,
   TOK_IS_BLOCK,
   TOK_PRAGMA_LINE,
   TOK_EMPTY,
   TOK_GIMPLE_PREDICT,
   TOK_CLB,
   TOK_CLB_VARS,
   TOK_USE,
   TOK_USE_VARS,
   TOK_PREDICATE,
   TOK_SLOT,

   /// RTL token
   TOK_RTL,
   TOK_ABS_R,
   TOK_AND_R,
   TOK_ASHIFT_R,
   TOK_ASHIFTRT_R,
   TOK_BSWAP_R,
   TOK_CALL_R,
   TOK_CALL_INSN_R,
   TOK_CLZ_R,
   TOK_CODE_LABEL_R,
   TOK_COMPARE_R,
   TOK_CONCAT_R,
   TOK_CONST_DOUBLE,
   TOK_CONST_INT,
   TOK_CTZ_R,
   TOK_DIV_R,
   TOK_EQ_R,
   TOK_FFS_R,
   TOK_FIX_R,
   TOK_FLOAT_R,
   TOK_FLOAT_EXTEND_R,
   TOK_FLOAT_TRUNCATE_R,
   TOK_FRACT_CONVERT_R,
   TOK_GE_R,
   TOK_GEU_R,
   TOK_GT_R,
   TOK_GTU_R,
   TOK_HIGH_R,
   TOK_IF_THEN_ELSE_R,
   TOK_INSN_R,
   TOK_IOR_R,
   TOK_JUMP_INSN_R,
   TOK_LABEL_REF_R,
   TOK_LE_R,
   TOK_LEU_R,
   TOK_LSHIFTRT_R,
   TOK_LT_R,
   TOK_LTGT_R,
   TOK_LTU_R,
   TOK_LO_SUM_R,
   TOK_WRITE_MEM_R,
   TOK_READ_MEM_R,
   TOK_MINUS_R,
   TOK_MOD_R,
   TOK_MULT_R,
   TOK_NE_R,
   TOK_NEG_R,
   TOK_NOT_R,
   TOK_ORDERED_R,
   TOK_PARALLEL_R,
   TOK_PARITY_R,
   TOK_PC_R,
   TOK_PLUS_R,
   TOK_POPCOUNT_R,
   TOK_REG_R,
   TOK_ROTATE_R,
   TOK_ROTATERT_R,
   TOK_SAT_FRACT_R,
   TOK_SET_R,
   TOK_SIGN_EXTEND_R,
   TOK_SMAX_R,
   TOK_SMIN_R,
   TOK_SQRT_R,
   TOK_SYMBOL_REF_R,
   TOK_TRUNCATE_R,
   TOK_UDIV_R,
   TOK_UMAX_R,
   TOK_UMIN_R,
   TOK_UMOD_R,
   TOK_UNEQ_R,
   TOK_UNGE_R,
   TOK_UNGT_R,
   TOK_UNLE_R,
   TOK_UNLT_R,
   TOK_UNORDERED_R,
   TOK_UNSIGNED_FIX_R,
   TOK_UNSIGNED_FLOAT_R,
   TOK_UNSIGNED_FRACT_CONVERT_R,
   TOK_UNSIGNED_SAT_FRACT_R,
   TOK_XOR_R,
   TOK_ZERO_EXTEND_R,

   /// RTL MODE token
   TOK_NONE_R,
   TOK_QC_R,
   TOK_HC_R,
   TOK_SC_R,
   TOK_DC_R,
   TOK_TC_R,
   TOK_CQI_R,
   TOK_CHI_R,
   TOK_CSI_R,
   TOK_CDI_R,
   TOK_CTI_R,
   TOK_QF_R,
   TOK_HF_R,
   TOK_SF_R,
   TOK_DF_R,
   TOK_TF_R,
   TOK_QI_R,
   TOK_HI_R,
   TOK_SI_R,
   TOK_DO_R,
   TOK_TI_R,
   TOK_V2SI_R,
   TOK_V4HI_R,
   TOK_V8QI_R,
   TOK_CC_R,
   TOK_CCFP_R,
   TOK_CCFPE_R,
   TOK_CCZ_R,

   LAST_TOKEN
};

struct treeVocabularyTokenTypes
{
   static const char* tokenNames[];
   static const int bisontokens[];

   /// Map between bison token and token_interface token
   std::map<int, TreeVocabularyTokenTypes_TokenEnum> from_bisontoken_map;
   int check_tokens(const char* tok) const;
   TreeVocabularyTokenTypes_TokenEnum bison2token(int bison) const;
   treeVocabularyTokenTypes();

 private:
   struct ltstr
   {
      bool operator()(const char* s1, const char* s2) const
      {
         return strcmp(s1, s2) < 0;
      }
   };
   std::map<const char*, int, ltstr> token_map;
};

/**
 * Return the name associated with the token.
 * @param i is the token coding.
 */
const std::string TI_getTokenName(const TreeVocabularyTokenTypes_TokenEnum i);

/**
 * Macro which writes on an output stream a token.
 */
#define WRITE_TOKEN(os, token) os << " " << TI_getTokenName(TreeVocabularyTokenTypes_TokenEnum::token)

/**
 * Second version of WRITE_TOKEN. Used in attr.cpp
 */
#define WRITE_TOKEN2(os, token) os << " " << TI_getTokenName(token)

/**
 * Macro used to convert a token symbol into a treeVocabularyTokenTypes
 */
#define TOK(token) (TreeVocabularyTokenTypes_TokenEnum::token)

/**
 * Macro used to convert a token symbol into the corresponding string
 */
#define STOK(token) TI_getTokenName(TreeVocabularyTokenTypes_TokenEnum::token)

/**
 * Macro used to convert an int token symbol into the corresponding string
 */
#define STOK2(token) TI_getTokenName(token)

#endif
