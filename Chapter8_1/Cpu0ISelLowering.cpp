//===-- Cpu0ISelLowering.cpp - Cpu0 DAG Lowering Implementation -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that Cpu0 uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "cpu0-lower"
#include "Cpu0ISelLowering.h"
#include "Cpu0MachineFunction.h"
#include "Cpu0TargetMachine.h"
#include "Cpu0TargetObjectFile.h"
#include "Cpu0Subtarget.h"
#include "MCTargetDesc/Cpu0BaseInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

SDValue Cpu0TargetLowering::getGlobalReg(SelectionDAG &DAG, EVT Ty) const {
  Cpu0FunctionInfo *FI = DAG.getMachineFunction().getInfo<Cpu0FunctionInfo>();
  return DAG.getRegister(FI->getGlobalBaseReg(), Ty);
}

static SDValue getTargetNode(SDValue Op, SelectionDAG &DAG, unsigned Flag) {
  EVT Ty = Op.getValueType();

  if (GlobalAddressSDNode *N = dyn_cast<GlobalAddressSDNode>(Op))
    return DAG.getTargetGlobalAddress(N->getGlobal(), SDLoc(Op), Ty, 0,
                                      Flag);
  if (ExternalSymbolSDNode *N = dyn_cast<ExternalSymbolSDNode>(Op))
    return DAG.getTargetExternalSymbol(N->getSymbol(), Ty, Flag);
  if (BlockAddressSDNode *N = dyn_cast<BlockAddressSDNode>(Op))
    return DAG.getTargetBlockAddress(N->getBlockAddress(), Ty, 0, Flag);
  if (JumpTableSDNode *N = dyn_cast<JumpTableSDNode>(Op))
    return DAG.getTargetJumpTable(N->getIndex(), Ty, Flag);
  if (ConstantPoolSDNode *N = dyn_cast<ConstantPoolSDNode>(Op))
    return DAG.getTargetConstantPool(N->getConstVal(), Ty, N->getAlignment(),
                                     N->getOffset(), Flag);

  llvm_unreachable("Unexpected node type.");
  return SDValue();
}

// This method creates the following nodes, which are necessary for
// computing a symbol's address in non-PIC mode:
//
// (add %hi(sym), %lo(sym))
static SDValue getAddrNonPIC(SDValue Op, SelectionDAG &DAG) {
  SDLoc DL = SDLoc(Op);
  EVT Ty = Op.getValueType();
  SDValue Hi = getTargetNode(Op, DAG, Cpu0II::MO_ABS_HI);
  SDValue Lo = getTargetNode(Op, DAG, Cpu0II::MO_ABS_LO);
  return DAG.getNode(ISD::ADD, DL, Ty,
                     DAG.getNode(Cpu0ISD::Hi, DL, Ty, Hi),
                     DAG.getNode(Cpu0ISD::Lo, DL, Ty, Lo));
}

SDValue Cpu0TargetLowering::getAddrLocal(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL = SDLoc(Op);
  EVT Ty = Op.getValueType();
  unsigned GOTFlag = Cpu0II::MO_GOT;
  SDValue GOT = DAG.getNode(Cpu0ISD::Wrapper, DL, Ty, getGlobalReg(DAG, Ty),
                            getTargetNode(Op, DAG, GOTFlag));
  SDValue Load = DAG.getLoad(Ty, DL, DAG.getEntryNode(), GOT,
                             MachinePointerInfo::getGOT(), false, false, false,
                             0);
  unsigned LoFlag = Cpu0II::MO_ABS_LO;
  SDValue Lo = DAG.getNode(Cpu0ISD::Lo, DL, Ty, getTargetNode(Op, DAG, LoFlag));
  return DAG.getNode(ISD::ADD, DL, Ty, Load, Lo);
}

SDValue Cpu0TargetLowering::getAddrGlobal(SDValue Op, SelectionDAG &DAG,
                                          unsigned Flag) const {
  SDLoc DL = SDLoc(Op);
  EVT Ty = Op.getValueType();
  SDValue Tgt = DAG.getNode(Cpu0ISD::Wrapper, DL, Ty, getGlobalReg(DAG, Ty),
                            getTargetNode(Op, DAG, Flag));
  return DAG.getLoad(Ty, DL, DAG.getEntryNode(), Tgt,
                     MachinePointerInfo::getGOT(), false, false, false, 0);
}

SDValue Cpu0TargetLowering::getAddrGlobalLargeGOT(SDValue Op, SelectionDAG &DAG,
                                                  unsigned HiFlag,
                                                  unsigned LoFlag) const {
  SDLoc DL = SDLoc(Op);
  EVT Ty = Op.getValueType();
  SDValue Hi = DAG.getNode(Cpu0ISD::Hi, DL, Ty, getTargetNode(Op, DAG, HiFlag));
  Hi = DAG.getNode(ISD::ADD, DL, Ty, Hi, getGlobalReg(DAG, Ty));
  SDValue Wrapper = DAG.getNode(Cpu0ISD::Wrapper, DL, Ty, Hi,
                                getTargetNode(Op, DAG, LoFlag));
  return DAG.getLoad(Ty, DL, DAG.getEntryNode(), Wrapper,
                     MachinePointerInfo::getGOT(), false, false, false, 0);
}

const char *Cpu0TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case Cpu0ISD::JmpLink:           return "Cpu0ISD::JmpLink";
  case Cpu0ISD::Hi:                return "Cpu0ISD::Hi";
  case Cpu0ISD::Lo:                return "Cpu0ISD::Lo";
  case Cpu0ISD::GPRel:             return "Cpu0ISD::GPRel";
  case Cpu0ISD::Ret:               return "Cpu0ISD::Ret";
  case Cpu0ISD::DivRem:            return "Cpu0ISD::DivRem";
  case Cpu0ISD::DivRemU:           return "Cpu0ISD::DivRemU";
  case Cpu0ISD::Wrapper:           return "Cpu0ISD::Wrapper";
  default:                         return NULL;
  }
} // lbd document - mark - getTargetNodeName

// lbd document - mark - Cpu0TargetLowering(Cpu0TargetMachine &TM) - begin
Cpu0TargetLowering::
Cpu0TargetLowering(Cpu0TargetMachine &TM)
  : TargetLowering(TM, new Cpu0TargetObjectFile()),
    Subtarget(&TM.getSubtarget<Cpu0Subtarget>()) {

  // Set up the register classes
  addRegisterClass(MVT::i32, &Cpu0::CPURegsRegClass);

  // Cpu0 does not have i1 type, so use i32 for
  // setcc operations results (slt, sgt, ...).
  setBooleanContents(ZeroOrOneBooleanContent);
  setBooleanVectorContents(ZeroOrNegativeOneBooleanContent);

  // Load extented operations for i1 types must be promoted
  setLoadExtAction(ISD::EXTLOAD,  MVT::i1,  Promote);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i1,  Promote);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i1,  Promote);

  // Used by legalize types to correctly generate the setcc result.
  // Without this, every float setcc comes with a AND/OR with the result,
  // we don't want this, since the fpcmp result goes to a flag register,
  // which is used implicitly by brcond and select operations.
  AddPromotedToType(ISD::SETCC, MVT::i1, MVT::i32);

  // Cpu0 Custom Operations
  setOperationAction(ISD::GlobalAddress,      MVT::i32,   Custom);
  setOperationAction(ISD::JumpTable,          MVT::i32,   Custom);
  setOperationAction(ISD::BRCOND,             MVT::Other, Custom);

  // Handle i64 shl such as the following,
  //   %sh_prom = zext i32 %b to i64
  //   %shl = shl i64 %a, %sh_prom
  setOperationAction(ISD::SHL_PARTS,          MVT::i32,   Custom);
  setOperationAction(ISD::SRA_PARTS,          MVT::i32,   Custom);
  setOperationAction(ISD::SRL_PARTS,          MVT::i32,   Custom);

  // Cpu0 doesn't have sext_inreg, replace them with shl/sra.
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1 , Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8 , Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16 , Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i32 , Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::Other , Expand);

  setOperationAction(ISD::SDIV, MVT::i32, Expand);
  setOperationAction(ISD::SREM, MVT::i32, Expand);
  setOperationAction(ISD::UDIV, MVT::i32, Expand);
  setOperationAction(ISD::UREM, MVT::i32, Expand);

  // Operations not directly supported by Cpu0.
  setOperationAction(ISD::BR_JT,             MVT::Other, Expand);
  setOperationAction(ISD::BR_CC,             MVT::i32, Expand);

  setTargetDAGCombine(ISD::SDIVREM);
  setTargetDAGCombine(ISD::UDIVREM);

//- Set .align 2
// It will emit .align 2 later
  setMinFunctionAlignment(2);

// must, computeRegisterProperties - Once all of the register classes are 
//  added, this allows us to compute derived properties we expose.
  computeRegisterProperties();
} // lbd document - mark - Cpu0TargetLowering(Cpu0TargetMachine &TM)

static SDValue PerformDivRemCombine(SDNode *N, SelectionDAG& DAG,
                                    TargetLowering::DAGCombinerInfo &DCI,
                                    const Cpu0Subtarget* Subtarget) {
  if (DCI.isBeforeLegalizeOps())
    return SDValue();

  EVT Ty = N->getValueType(0);
  unsigned LO = Cpu0::LO;
  unsigned HI = Cpu0::HI;
  unsigned opc = N->getOpcode() == ISD::SDIVREM ? Cpu0ISD::DivRem :
                                                  Cpu0ISD::DivRemU;
  SDLoc DL(N);

  SDValue DivRem = DAG.getNode(opc, DL, MVT::Glue,
                               N->getOperand(0), N->getOperand(1));
  SDValue InChain = DAG.getEntryNode();
  SDValue InGlue = DivRem;

  // insert MFLO
  if (N->hasAnyUseOfValue(0)) {
    SDValue CopyFromLo = DAG.getCopyFromReg(InChain, DL, LO, Ty,
                                            InGlue);
    DAG.ReplaceAllUsesOfValueWith(SDValue(N, 0), CopyFromLo);
    InChain = CopyFromLo.getValue(1);
    InGlue = CopyFromLo.getValue(2);
  }

  // insert MFHI
  if (N->hasAnyUseOfValue(1)) {
    SDValue CopyFromHi = DAG.getCopyFromReg(InChain, DL,
                                            HI, Ty, InGlue);
    DAG.ReplaceAllUsesOfValueWith(SDValue(N, 1), CopyFromHi);
  }

  return SDValue();
}

SDValue Cpu0TargetLowering::PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI)
  const {
  SelectionDAG &DAG = DCI.DAG;
  unsigned opc = N->getOpcode();

  switch (opc) {
  default: break;
  case ISD::SDIVREM:
  case ISD::UDIVREM:
    return PerformDivRemCombine(N, DAG, DCI, Subtarget);
  }

  return SDValue();
}

// lbd document - mark - LowerOperation - begin
SDValue Cpu0TargetLowering::
LowerOperation(SDValue Op, SelectionDAG &DAG) const
{
  switch (Op.getOpcode())
  {
    case ISD::BRCOND:             return LowerBRCOND(Op, DAG);
    case ISD::GlobalAddress:      return LowerGlobalAddress(Op, DAG);
    case ISD::JumpTable:          return lowerJumpTable(Op, DAG);
    case ISD::SHL_PARTS:          return lowerShiftLeftParts(Op, DAG);
    case ISD::SRA_PARTS:          return lowerShiftRightParts(Op, DAG, true);
    case ISD::SRL_PARTS:          return lowerShiftRightParts(Op, DAG, false);
  }
  return SDValue();
}

//===----------------------------------------------------------------------===//
//  Lower helper functions
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Misc Lower Operation implementation
//===----------------------------------------------------------------------===//
SDValue Cpu0TargetLowering::
LowerBRCOND(SDValue Op, SelectionDAG &DAG) const
{
  return Op;
}

SDValue Cpu0TargetLowering::LowerGlobalAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
  // FIXME there isn't actually debug info here
  SDLoc DL = SDLoc(Op);
  const GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal();

  Cpu0TargetObjectFile &TLOF = (Cpu0TargetObjectFile&)getObjFileLowering();

  if (getTargetMachine().getRelocationModel() != Reloc::PIC_) {
    SDVTList VTs = DAG.getVTList(MVT::i32);

    // %gp_rel relocation
    if (TLOF.IsGlobalInSmallSection(GV, getTargetMachine())) {
      SDValue GA = DAG.getTargetGlobalAddress(GV, DL, MVT::i32, 0,
                                              Cpu0II::MO_GPREL);
      SDValue GPRelNode = DAG.getNode(Cpu0ISD::GPRel, DL, VTs, GA);
      SDValue GOT = DAG.getGLOBAL_OFFSET_TABLE(MVT::i32);
      return DAG.getNode(ISD::ADD, DL, MVT::i32, GOT, GPRelNode);
    }
    // %hi/%lo relocation
    SDValue GAHi = DAG.getTargetGlobalAddress(GV, DL, MVT::i32, 0,
                                              Cpu0II::MO_ABS_HI);
    SDValue GALo = DAG.getTargetGlobalAddress(GV, DL, MVT::i32, 0,
                                              Cpu0II::MO_ABS_LO);
    SDValue HiPart = DAG.getNode(Cpu0ISD::Hi, DL, VTs, GAHi);
    SDValue Lo = DAG.getNode(Cpu0ISD::Lo, DL, MVT::i32, GALo);
    return DAG.getNode(ISD::ADD, DL, MVT::i32, HiPart, Lo);
  }

  if (GV->hasInternalLinkage() || (GV->hasLocalLinkage() && !isa<Function>(GV)))
    return getAddrLocal(Op, DAG);

  if (TLOF.IsGlobalInSmallSection(GV, getTargetMachine()))
    return getAddrGlobal(Op, DAG, Cpu0II::MO_GOT16);
  else
    return getAddrGlobalLargeGOT(Op, DAG, Cpu0II::MO_GOT_HI16,
                                 Cpu0II::MO_GOT_LO16);
}

SDValue Cpu0TargetLowering::
lowerJumpTable(SDValue Op, SelectionDAG &DAG) const
{
  if (getTargetMachine().getRelocationModel() != Reloc::PIC_)
    return getAddrNonPIC(Op, DAG);

  return getAddrLocal(Op, DAG);
}

SDValue Cpu0TargetLowering::lowerShiftLeftParts(SDValue Op,
                                                SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0), Hi = Op.getOperand(1);
  SDValue Shamt = Op.getOperand(2);

  // if shamt < 32:
  //  lo = (shl lo, shamt)
  //  hi = (or (shl hi, shamt) (srl (srl lo, 1), ~shamt))
  // else:
  //  lo = 0
  //  hi = (shl lo, shamt[4:0])
  SDValue Not = DAG.getNode(ISD::XOR, DL, MVT::i32, Shamt,
                            DAG.getConstant(-1, MVT::i32));
  SDValue ShiftRight1Lo = DAG.getNode(ISD::SRL, DL, MVT::i32, Lo,
                                      DAG.getConstant(1, MVT::i32));
  SDValue ShiftRightLo = DAG.getNode(ISD::SRL, DL, MVT::i32, ShiftRight1Lo,
                                     Not);
  SDValue ShiftLeftHi = DAG.getNode(ISD::SHL, DL, MVT::i32, Hi, Shamt);
  SDValue Or = DAG.getNode(ISD::OR, DL, MVT::i32, ShiftLeftHi, ShiftRightLo);
  SDValue ShiftLeftLo = DAG.getNode(ISD::SHL, DL, MVT::i32, Lo, Shamt);
  SDValue Cond = DAG.getNode(ISD::AND, DL, MVT::i32, Shamt,
                             DAG.getConstant(0x20, MVT::i32));
  Lo = DAG.getNode(ISD::SELECT, DL, MVT::i32, Cond,
                   DAG.getConstant(0, MVT::i32), ShiftLeftLo);
  Hi = DAG.getNode(ISD::SELECT, DL, MVT::i32, Cond, ShiftLeftLo, Or);

  SDValue Ops[2] = {Lo, Hi};
  return DAG.getMergeValues(Ops, DL);
}

SDValue Cpu0TargetLowering::lowerShiftRightParts(SDValue Op, SelectionDAG &DAG,
                                                 bool IsSRA) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0), Hi = Op.getOperand(1);
  SDValue Shamt = Op.getOperand(2);

  // if shamt < 32:
  //  lo = (or (shl (shl hi, 1), ~shamt) (srl lo, shamt))
  //  if isSRA:
  //    hi = (sra hi, shamt)
  //  else:
  //    hi = (srl hi, shamt)
  // else:
  //  if isSRA:
  //   lo = (sra hi, shamt[4:0])
  //   hi = (sra hi, 31)
  //  else:
  //   lo = (srl hi, shamt[4:0])
  //   hi = 0
  SDValue Not = DAG.getNode(ISD::XOR, DL, MVT::i32, Shamt,
                            DAG.getConstant(-1, MVT::i32));
  SDValue ShiftLeft1Hi = DAG.getNode(ISD::SHL, DL, MVT::i32, Hi,
                                     DAG.getConstant(1, MVT::i32));
  SDValue ShiftLeftHi = DAG.getNode(ISD::SHL, DL, MVT::i32, ShiftLeft1Hi, Not);
  SDValue ShiftRightLo = DAG.getNode(ISD::SRL, DL, MVT::i32, Lo, Shamt);
  SDValue Or = DAG.getNode(ISD::OR, DL, MVT::i32, ShiftLeftHi, ShiftRightLo);
  SDValue ShiftRightHi = DAG.getNode(IsSRA ? ISD::SRA : ISD::SRL, DL, MVT::i32,
                                     Hi, Shamt);
  SDValue Cond = DAG.getNode(ISD::AND, DL, MVT::i32, Shamt,
                             DAG.getConstant(0x20, MVT::i32));
  SDValue Shift31 = DAG.getNode(ISD::SRA, DL, MVT::i32, Hi,
                                DAG.getConstant(31, MVT::i32));
  Lo = DAG.getNode(ISD::SELECT, DL, MVT::i32, Cond, ShiftRightHi, Or);
  Hi = DAG.getNode(ISD::SELECT, DL, MVT::i32, Cond,
                   IsSRA ? Shift31 : DAG.getConstant(0, MVT::i32),
                   ShiftRightHi);

  SDValue Ops[2] = {Lo, Hi};
  return DAG.getMergeValues(Ops, DL);
}

#include "Cpu0GenCallingConv.inc"

/// LowerFormalArguments - transform physical registers into virtual registers
/// and generate load operations for arguments places on the stack.
SDValue
Cpu0TargetLowering::LowerFormalArguments(SDValue Chain,
                                         CallingConv::ID CallConv,
                                         bool isVarArg,
                                      const SmallVectorImpl<ISD::InputArg> &Ins,
                                         SDLoc DL, SelectionDAG &DAG,
                                         SmallVectorImpl<SDValue> &InVals)
                                          const {
  return Chain;
}

//===----------------------------------------------------------------------===//
//               Return Value Calling Convention Implementation
//===----------------------------------------------------------------------===//

SDValue
Cpu0TargetLowering::LowerReturn(SDValue Chain,
                                CallingConv::ID CallConv, bool isVarArg,
                                const SmallVectorImpl<ISD::OutputArg> &Outs,
                                const SmallVectorImpl<SDValue> &OutVals,
                                SDLoc DL, SelectionDAG &DAG) const {

  // CCValAssign - represent the assignment of
  // the return value to a location
  SmallVector<CCValAssign, 16> RVLocs;

  // CCState - Info about the registers and stack slot.
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
		 getTargetMachine(), RVLocs, *DAG.getContext());

  // Analize return values.
  CCInfo.AnalyzeReturn(Outs, RetCC_Cpu0);

  SDValue Flag;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  // Copy the result values into the output registers.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[i], Flag);

    // Guarantee that all emitted copies are stuck together with flags.
    Flag = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

#if 1 // structure return begin. Without this, it will use $3 instead of $2 
  // as return register. The cpu0 ABIs for returning structs by value requires 
  // that we copy the sret argument into $v0 for the return. We saved the 
  // argument into a virtual register in the entry block, so now we copy the 
  // value out and into $v0.
  if (DAG.getMachineFunction().getFunction()->hasStructRetAttr()) {
    MachineFunction &MF      = DAG.getMachineFunction();
    Cpu0FunctionInfo *Cpu0FI = MF.getInfo<Cpu0FunctionInfo>();
    unsigned Reg = Cpu0FI->getSRetReturnReg();

    if (!Reg)
      llvm_unreachable("sret virtual register not created in the entry block");
    SDValue Val = DAG.getCopyFromReg(Chain, DL, Reg, getPointerTy());

    Chain = DAG.getCopyToReg(Chain, DL, Cpu0::V0, Val, Flag);
    Flag = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(Cpu0::V0, getPointerTy()));
  }
#endif // structure return end

  RetOps[0] = Chain;  // Update chain.

  // Add the flag if we have it.
  if (Flag.getNode())
    RetOps.push_back(Flag);

  // Return on Cpu0 is always a "ret $lr"
  return DAG.getNode(Cpu0ISD::Ret, DL, MVT::Other, RetOps);
}

bool // lbd document - mark - isOffsetFoldingLegal
Cpu0TargetLowering::isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const {
  // The Cpu0 target isn't yet aware of offsets.
  return false;
}
