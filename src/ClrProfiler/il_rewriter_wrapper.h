#ifndef CLR_PROFILER_IL_REWRITER_WRAPPER_H_
#define CLR_PROFILER_IL_REWRITER_WRAPPER_H_

#include "il_rewriter.h"

class ILRewriterWrapper {
 private:
  ILRewriter* const m_ILRewriter;
  ILInstr* m_ILInstr;

 public:
  ILRewriterWrapper(ILRewriter* const il_rewriter)
      : m_ILRewriter(il_rewriter), m_ILInstr(nullptr) {}

  ILRewriter* GetILRewriter() const;
  void SetILPosition(ILInstr* pILInstr);
  void Pop() const;
  void LoadNull() const;
  void LoadStr(mdToken token) const;
  void LoadInt64(INT64 value) const;
  void LoadInt32(INT32 value) const;
  void LoadArgument(UINT16 index) const;
  void LoadIND(unsigned elementType) const;
  void StLocal(unsigned index) const;
  void LoadLocal(unsigned index) const;
  void Cast(mdTypeRef type_ref) const;
  void Box(mdTypeRef type_ref) const;
  void UnboxAny(mdTypeRef type_ref) const;
  void CreateArray(mdTypeRef type_ref, INT32 size) const;
  void CallMember(const mdMemberRef& member_ref, bool is_virtual) const;
  void Duplicate() const;
  void BeginLoadValueIntoArray(INT32 arrayIndex) const;
  void EndLoadValueIntoArray() const;
  void Return() const;
  ILInstr* Rethrow() const;
  ILInstr* EndFinally() const;
  ILInstr* CallMember0(const mdMemberRef& member_ref, bool is_virtual) const;
};

#endif  // CLR_PROFILER_IL_REWRITER_WRAPPER_H_
