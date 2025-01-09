//===--- CGExprTensorC.cpp - Emit LLVM Code for C++ expressions ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code dealing with code generation of TensorC expressions
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "clang/CodeGen/CGFunctionInfo.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace CodeGen;

RValue CodeGenFunction::EmitTensorcMemberFunctionExpr(const CallExpr *E, ReturnValueSlot ReturnValue) {
  printf("Used Member Function!!!!!!!!!!!\n");
  const MemberExpr *Member = dyn_cast<MemberExpr>(E->getCallee()->IgnoreImpCasts());
  const Expr *BaseAddr = Member->getBase();
  bool isarrow = Member->isArrow();
  QualType BaseAddrType = BaseAddr->getType()->getCanonicalTypeInternal();
  if (isarrow) BaseAddrType = BaseAddr->getType()->getAs<PointerType>()->getPointeeType()->getCanonicalTypeInternal();
  assert(isa<RecordType>(BaseAddrType));
  assert(isa<FieldDecl>(Member->getMemberDecl()));
  const FieldDecl *Field = cast<FieldDecl>(Member->getMemberDecl());

  Address This = Address::invalid();
  if (isarrow)
    This = EmitPointerWithAlignment(BaseAddr);
  else
    This = EmitLValue(BaseAddr).getAddress();

  QualType FTy = getContext().getFieldFunctionType(BaseAddrType, Field->getType());
  FTy = FTy.getCanonicalType();
  const FunctionType *FnType = cast<FunctionType>(FTy);

  CallArgList Args;
  Args.add(RValue::get(This.getPointer()), getContext().getPointerType(BaseAddrType));
  EmitCallArgs(Args, dyn_cast<FunctionProtoType>(Field->getType().getTypePtr()), E->arguments());
  const CGFunctionInfo &FnInfo = CGM.getTypes().arrangeFreeFunctionCall(Args, FnType, false);
  llvm::Constant *Func = CGM.GetOrCreateFunctionForRecordField(cast<RecordType>(BaseAddrType)->getDecl(), Field);
  CGCallee Callee = CGCallee::forDirect(Func);

  Func->print(llvm::errs());

  return EmitCall(FnInfo, Callee, ReturnValue, Args, nullptr, E->getExprLoc());
}

static StringRef getBinaryOperatorFuncName(BinaryOperatorKind Opc, int TensorScalar) {
  switch(Opc) {
    case BO_Add:
      return TensorScalar == 0 ? "EWAdd" : "EWAddScalar";
    case BO_Sub:
      return TensorScalar == 0 ? "EWSub" : (TensorScalar == 1 ? "EWSubScalar" : "EWAddScalar");
    case BO_Mul:
      return TensorScalar == 0 ? "EWMul" : "EWMulScalar";
    case BO_Div:
      return TensorScalar == 0 ? "EWDiv" : (TensorScalar == 1 ? "EWDivScalar" : "EWMulScalar");
    case BO_Rem:
      return TensorScalar == 0 ? "EWRem" : (TensorScalar == 1 ? "EWRemScalar" : "EWRemScalarTensor");
    case BO_Shl:
      return TensorScalar == 0 ? "EWShl" : (TensorScalar == 1 ? "EWShlScalar" : "EWShlScalarTensor");
    case BO_Shr:
      return TensorScalar == 0 ? "EWShr" : (TensorScalar == 1 ? "EWShrScalar" : "EWShrScalarTensor");
    case BO_LT:
      return TensorScalar == 0 ? "EWLT" : "EWLTScalar";
    case BO_LE:
      return TensorScalar == 0 ? "EWLE" : "EWLEScalar";
    case BO_GT:
      return TensorScalar == 0 ? "EWGT" : "EWGTScalar";
    case BO_GE:
      return TensorScalar == 0 ? "EWGE" : "EWGEScalar";
    case BO_EQ:
      return TensorScalar == 0 ? "EWEQ" : "EWEQScalar";
    case BO_NE:
      return TensorScalar == 0 ? "EWNE" : "EWNEScalar";
    default:
      llvm_unreachable("Unsupported binary operator");
      return "";
  }
}
// to do
RValue CodeGenFunction::EmitTensorcBinaryOperator(const BinaryOperator *E, ReturnValueSlot ReturnValue) {

  printf("Used Binary Operator!!!!!!!!!!!\n");
  const Expr *LHS = E -> getLHS();
  const Expr *RHS = E -> getRHS();
  QualType Ltype = LHS->getType();
  QualType Rtype = RHS->getType();

  llvm::errs() << "BinaryOperator E information:\n";
  llvm::errs() << "LHS type: " << LHS->getType().getAsString() << "\n";
  llvm::errs() << "RHS type: " << RHS->getType().getAsString() << "\n";
  llvm::errs() << "Operator: " << E->getOpcodeStr(E->getOpcode()) << "\n";

  // Emit arguments
  CallArgList Args;
  EmitCallArg(Args, LHS, Ltype);
  EmitCallArg(Args, RHS, Rtype);
  // Get llvm function
  FunctionProtoType::ExtProtoInfo EPI;
  SmallVector<QualType, 2> ArgTys = {Ltype, Rtype};
  QualType FnType = getContext().getFunctionType(Ltype, ArgTys, EPI);

  const CGFunctionInfo &FnInfo =
		  CGM.getTypes().arrangeFreeFunctionCall(Args, cast<FunctionType>(FnType.getTypePtr()), false);

  StringRef func_name = getBinaryOperatorFuncName(E->getOpcode(), !(Ltype->isScalarType()));
  llvm::FunctionType *FnTypeLLVM = CGM.getTypes().GetFunctionType(FnInfo);
  llvm::Constant *Func = CGM.GetOrCreateFunction(FnTypeLLVM, func_name);

  Func->print(llvm::errs());
  // Emit call
  CGCallee Callee = CGCallee::forDirect(Func);
  return EmitCall(FnInfo, Callee, ReturnValue, Args, nullptr, E->getExprLoc());
}

RValue CodeGenFunction::EmitTensorcUnaryOperator(const UnaryOperator *E, ReturnValueSlot ReturnValue) {
  printf("Used Unary Operator!!!!!!!!!!!\n");
  
  const Expr *Val = E->getSubExpr();
  QualType Ty = Val->getType();


  llvm::errs() << "UnaryOperator E information:\n";
  llvm::errs() << "SubExpr type: " << Val->getType().getAsString() << "\n";
  llvm::errs() << "Operator: " << E->getOpcodeStr(E->getOpcode()) << "\n";
  llvm::errs() << "Location: ";
  E->getExprLoc().dump(getContext().getSourceManager());
  llvm::errs() << "\n";

  // Emit arguments
  CallArgList Args;
  EmitCallArg(Args, Val, Ty);

  // Get llvm function
  FunctionProtoType::ExtProtoInfo EPI;
  SmallVector<QualType, 1> ArgTys(1, Ty);
  QualType FnType = getContext().getFunctionType(Ty, ArgTys, EPI);
  const CGFunctionInfo &FnInfo =
		  CGM.getTypes().arrangeFreeFunctionCall(Args, cast<FunctionType>(FnType.getTypePtr()), false);
  llvm::FunctionType *FnTypeLLVM = CGM.getTypes().GetFunctionType(FnInfo);
  llvm::Constant *Func = CGM.GetOrCreateFunction(FnTypeLLVM, "EWNegative");

  Func->print(llvm::errs());

  // Emit call
  CGCallee Callee = CGCallee::forDirect(Func);
  return EmitCall(FnInfo, Callee, ReturnValue, Args, nullptr, E->getExprLoc());
}



// to do 
RValue CodeGenFunction::EmitTensorcBinAssign(const BinaryOperator *E, ReturnValueSlot ReturnValue) {
  printf("Used Bin Assign!!!!!!!!!!!\n");
  const Expr *LHS = E->getLHS();
  const Expr *RHS = E->getRHS();
  QualType LHSType = LHS->getType();
  QualType LHSTypeRefType = getContext().getLValueReferenceType(LHSType);
  QualType RHSType = RHS->getType();

  // Emit arguments
  CallArgList Args;
  EmitCallArg(Args, LHS, LHSTypeRefType);
  EmitCallArg(Args, RHS, RHSType);

  // Get llvm function
  FunctionProtoType::ExtProtoInfo EPI;
  SmallVector<QualType, 2> ArgTys{LHSTypeRefType, RHSType};
  QualType FnType = getContext().getFunctionType(getContext().VoidTy, ArgTys, EPI);
  const CGFunctionInfo &FnInfo = 
      CGM.getTypes().arrangeFreeFunctionCall(Args, cast<FunctionType>(FnType.getTypePtr()), false);

  llvm::FunctionType *FnTypeLLVM = CGM.getTypes().GetFunctionType(FnInfo);

  llvm::Constant *Func = CGM.GetOrCreateFunction(FnTypeLLVM, "copy"); 

  // Emit call
  CGCallee Callee = CGCallee::forDirect(Func);
  return EmitCall(FnInfo, Callee, ReturnValue, Args, nullptr, E->getExprLoc());

}


// to do
RValue CodeGenFunction::EmitTensorcTensorSliceExpr(const TensorSliceExpr *E, ReturnValueSlot ReturnValue) {
  printf("Used Slice Expr!!!!!!!!!!!\n");
  
  // Emit arguments
  // a[x:y:z], a=baseaddr, x=lowerbound, z=step, y=upperbound

  // BaseAddr
  CallArgList Args;
  const Expr *BaseAddr = E->getBase();
  Address This = Address::invalid();
  This = EmitLValue(BaseAddr).getAddress();
  QualType BaseAddrType = BaseAddr->getType();
  QualType BaseAddrTypeRefType = getContext().getLValueReferenceType(BaseAddrType); 
  Args.add(RValue::get(This.getPointer()), BaseAddrTypeRefType);

  RValue temp = RValue::get(This.getPointer());
  llvm::Value *V = temp.getScalarVal();
  if (V) {
      llvm::errs() << "RValue contains: " << *V << "\n";
  }

  // Dim
  const unsigned Dim_uint = E->getDim();
  QualType DimType = getContext().UnsignedIntTy;
  llvm::Type *DimTypeRefType = ConvertTypeForMem(DimType);
  llvm::Value *Dim = llvm::ConstantInt::get(DimTypeRefType, llvm::APInt(64, Dim_uint));
  Args.add(RValue::get(Dim),DimType);
    
  // LowerBound
  QualType LowerBoundType;
  if(E->getLowerBound()){
    const Expr *LowerBound = E->getLowerBound();
    LowerBoundType = LowerBound->getType();
    EmitCallArg(Args, LowerBound, LowerBoundType);
  } 
  else{
    LowerBoundType = getContext().IntTy;
    llvm::Type *LowerBoundRefType = ConvertTypeForMem(LowerBoundType);
    llvm::Value *LowerBound = llvm::ConstantInt::get(LowerBoundRefType, llvm::APInt(64, 0));
    Args.add(RValue::get(LowerBound),LowerBoundType);
  }

  // UpperBound
  QualType UpperBoundType;
  if(E->getUpperBound()){
    const Expr *UpperBound = E->getUpperBound();
    UpperBoundType = UpperBound->getType();
    EmitCallArg(Args, UpperBound, UpperBoundType);
  }
  else{
    UpperBoundType = getContext().IntTy;
    llvm::Type *UpperBoundRefType = ConvertTypeForMem(UpperBoundType);
    llvm::Value *UpperBound = llvm::ConstantInt::get(UpperBoundRefType, llvm::APInt(64, -1));
    Args.add(RValue::get(UpperBound),UpperBoundType);
  }

  // Step
  QualType StepType;
  if(E->getStep()){
    const Expr *Step = E->getStep();
    StepType = Step->getType();
    EmitCallArg(Args, Step, StepType);
  }
  else{
    StepType = getContext().IntTy;
    llvm::Type *StepRefType = ConvertTypeForMem(StepType);
    llvm::Value *Step = llvm::ConstantInt::get(StepRefType, llvm::APInt(64, 1));
    Args.add(RValue::get(Step),StepType);
  }

  // Get llvm function
  FunctionProtoType::ExtProtoInfo EPI;
  SmallVector<QualType, 5> ArgTys{BaseAddrTypeRefType, DimType, LowerBoundType, StepType, UpperBoundType};
  QualType FnType = getContext().getFunctionType(BaseAddrType, ArgTys, EPI); 
  const CGFunctionInfo &FnInfo =
      CGM.getTypes().arrangeFreeFunctionCall(Args, cast<FunctionType>(FnType.getTypePtr()), false);
  llvm::FunctionType *FnTypeLLVM = CGM.getTypes().GetFunctionType(FnInfo);
  llvm::Constant *Func = CGM.GetOrCreateFunction(FnTypeLLVM, "tensor_slice");

  Func->print(llvm::errs());

  // Emit call
  CGCallee Callee = CGCallee::forDirect(Func);
  return EmitCall(FnInfo, Callee, ReturnValue, Args, nullptr, E->getExprLoc());
}
