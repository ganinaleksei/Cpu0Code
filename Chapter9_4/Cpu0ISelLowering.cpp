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
  setOperationAction(ISD::SELECT,             MVT::i32,   Custom);
  setOperationAction(ISD::BRCOND,             MVT::Other, Custom);
  setOperationAction(ISD::VASTART,            MVT::Other, Custom);

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
  setOperationAction(ISD::SELECT_CC,         MVT::i32, Expand);
  setOperationAction(ISD::SELECT_CC,         MVT::Other, Expand);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32,  Expand);

  // Support va_arg(): variable numbers (not fixed numbers) of arguments 
  //  (parameters) for function all
  setOperationAction(ISD::VAARG,             MVT::Other, Expand);
  setOperationAction(ISD::VACOPY,            MVT::Other, Expand);
  setOperationAction(ISD::VAEND,             MVT::Other, Expand);

  setTargetDAGCombine(ISD::SDIVREM);
  setTargetDAGCombine(ISD::UDIVREM);

//- Set .align 2
// It will emit .align 2 later
  setMinFunctionAlignment(2);

  setStackPointerRegisterToSaveRestore(Cpu0::SP);

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
    case ISD::SELECT:             return lowerSELECT(Op, DAG);
    case ISD::VASTART:            return LowerVASTART(Op, DAG);
    case ISD::SHL_PARTS:          return lowerShiftLeftParts(Op, DAG);
    case ISD::SRA_PARTS:          return lowerShiftRightParts(Op, DAG, true);
    case ISD::SRL_PARTS:          return lowerShiftRightParts(Op, DAG, false);
  }
  return SDValue();
}

//===----------------------------------------------------------------------===//
//  Lower helper functions
//===----------------------------------------------------------------------===//

// AddLiveIn - This helper function adds the specified physical register to the
// MachineFunction as a live in value.  It also creates a corresponding
// virtual register for it.
static unsigned
AddLiveIn(MachineFunction &MF, unsigned PReg, const TargetRegisterClass *RC)
{
  assert(RC->contains(PReg) && "Not the correct regclass!");
  unsigned VReg = MF.getRegInfo().createVirtualRegister(RC);
  MF.getRegInfo().addLiveIn(PReg, VReg);
  return VReg;
}

//===----------------------------------------------------------------------===//
//  Misc Lower Operation implementation
//===----------------------------------------------------------------------===//
SDValue Cpu0TargetLowering::
LowerBRCOND(SDValue Op, SelectionDAG &DAG) const
{
  return Op;
}

SDValue Cpu0TargetLowering::
lowerSELECT(SDValue Op, SelectionDAG &DAG) const
{
  return Op;
} // lbd document - mark - lowerSELECT

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

SDValue Cpu0TargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  Cpu0FunctionInfo *FuncInfo = MF.getInfo<Cpu0FunctionInfo>();

  SDLoc DL = SDLoc(Op);
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                 getPointerTy());

  // vastart just stores the address of the VarArgsFrameIndex slot into the
  // memory location argument.
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                      MachinePointerInfo(SV), false, false, 0);
}

#include "Cpu0GenCallingConv.inc"

//===----------------------------------------------------------------------===//
//                  Call Calling Convention Implementation
//===----------------------------------------------------------------------===//

static const unsigned IntRegsSize = 2;

static const uint16_t IntRegs[] = {
  Cpu0::A0, Cpu0::A1
};

// Write ByVal Arg to arg registers and stack.
static void
WriteByValArg(SDValue& ByValChain, SDValue Chain, SDLoc DL,
              SmallVector<std::pair<unsigned, SDValue>, 16>& RegsToPass,
              SmallVector<SDValue, 8>& MemOpChains, int& LastFI,
              MachineFrameInfo *MFI, SelectionDAG &DAG, SDValue Arg,
              const CCValAssign &VA, const ISD::ArgFlagsTy& Flags,
              MVT PtrType, bool isLittle) {
  unsigned LocMemOffset = VA.getLocMemOffset();
  unsigned Offset = 0;
  uint32_t RemainingSize = Flags.getByValSize();
  unsigned ByValAlign = Flags.getByValAlign();

  if (RemainingSize == 0)
    return;

  // Create a fixed object on stack at offset LocMemOffset and copy
  // remaining part of byval arg to it using memcpy.
  SDValue Src = DAG.getNode(ISD::ADD, DL, MVT::i32, Arg,
                            DAG.getConstant(Offset, MVT::i32));
  LastFI = MFI->CreateFixedObject(RemainingSize, LocMemOffset, true);
  SDValue Dst = DAG.getFrameIndex(LastFI, PtrType);
  ByValChain = DAG.getMemcpy(ByValChain, DL, Dst, Src,
                             DAG.getConstant(RemainingSize, MVT::i32),
                             std::min(ByValAlign, (unsigned)4),
                             /*isVolatile=*/false, /*AlwaysInline=*/false,
                             MachinePointerInfo(), MachinePointerInfo());
} // lbd document - mark - WriteByValArg

// lbd document - mark - before LowerCall
SDValue
Cpu0TargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                              SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG                     = CLI.DAG;
  SDLoc DL                              = CLI.DL;
  SmallVector<ISD::OutputArg, 32> &Outs = CLI.Outs;
  SmallVector<SDValue, 32> &OutVals     = CLI.OutVals;
  SmallVector<ISD::InputArg, 32> &Ins   = CLI.Ins;
  SDValue InChain                       = CLI.Chain;
  SDValue Callee                        = CLI.Callee;
  bool &isTailCall                      = CLI.IsTailCall;
  CallingConv::ID CallConv              = CLI.CallConv;
  bool isVarArg                         = CLI.IsVarArg;
  // Cpu0 target does not yet support tail call optimization.
  isTailCall                            = false;

  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo *MFI = MF.getFrameInfo();
  const TargetFrameLowering *TFL = MF.getTarget().getFrameLowering();
  bool IsPIC = getTargetMachine().getRelocationModel() == Reloc::PIC_;
  Cpu0FunctionInfo *Cpu0FI = MF.getInfo<Cpu0FunctionInfo>();

  // Analyze operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
                 getTargetMachine(), ArgLocs, *DAG.getContext());

  CCInfo.AnalyzeCallOperands(Outs, CC_Cpu0);

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NextStackOffset = CCInfo.getNextStackOffset();

  // If this is the first call, create a stack frame object that points to
  // a location to which .cprestore saves $gp.
  if (IsPIC && Cpu0FI->globalBaseRegFixed() && !Cpu0FI->getGPFI())
    Cpu0FI->setGPFI(MFI->CreateFixedObject(4, 0, true));
  // Get the frame index of the stack frame object that points to the location
  // of dynamically allocated area on the stack.
  int DynAllocFI = Cpu0FI->getDynAllocFI();
  unsigned MaxCallFrameSize = Cpu0FI->getMaxCallFrameSize();

  if (MaxCallFrameSize < NextStackOffset) {
    Cpu0FI->setMaxCallFrameSize(NextStackOffset);

    // Set the offsets relative to $sp of the $gp restore slot and dynamically
    // allocated stack space. These offsets must be aligned to a boundary
    // determined by the stack alignment of the ABI.
    unsigned StackAlignment = TFL->getStackAlignment();
    NextStackOffset = (NextStackOffset + StackAlignment - 1) /
                      StackAlignment * StackAlignment;

    if (Cpu0FI->needGPSaveRestore())
      MFI->setObjectOffset(Cpu0FI->getGPFI(), NextStackOffset);

    MFI->setObjectOffset(DynAllocFI, NextStackOffset);
  }
  // Chain is the output chain of the last Load/Store or CopyToReg node.
  // ByValChain is the output chain of the last Memcpy node created for copying
  // byval arguments to the stack.
  SDValue Chain, CallSeqStart, ByValChain;
  SDValue NextStackOffsetVal = DAG.getIntPtrConstant(NextStackOffset, true);
  Chain = CallSeqStart = DAG.getCALLSEQ_START(InChain, NextStackOffsetVal, DL);
  ByValChain = InChain;

  // With EABI is it possible to have 16 args on registers.
  SmallVector<std::pair<unsigned, SDValue>, 16> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  int FirstFI = -MFI->getNumFixedObjects() - 1, LastFI = 0;

  // Walk the register/memloc assignments, inserting copies/loads.
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    SDValue Arg = OutVals[i];
    CCValAssign &VA = ArgLocs[i];
    MVT ValVT = VA.getValVT(), LocVT = VA.getLocVT();
    ISD::ArgFlagsTy Flags = Outs[i].Flags;

    // ByVal Arg.
    if (Flags.isByVal()) {
      assert("!!!Error!!!, Flags.isByVal()==true");
      assert(Flags.getByValSize() &&
             "ByVal args of size 0 should have been ignored by front-end.");
      WriteByValArg(ByValChain, Chain, DL, RegsToPass, MemOpChains, LastFI,
                    MFI, DAG, Arg, VA, Flags, getPointerTy(),
                    Subtarget->isLittle());
      continue;
    }

    // Promote the value if needed.
    switch (VA.getLocInfo()) {
    default: llvm_unreachable("Unknown loc info!");
    case CCValAssign::Full:
      break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, DL, LocVT, Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, DL, LocVT, Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, DL, LocVT, Arg);
      break;
    }
    // Arguments that can be passed on register must be kept at
    // RegsToPass vector
    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
      continue;
    }

    // Register can't get to this point...
    assert(VA.isMemLoc());

    // Create the frame index object for this incoming parameter
    LastFI = MFI->CreateFixedObject(ValVT.getSizeInBits()/8,
                                    VA.getLocMemOffset(), true);
    SDValue PtrOff = DAG.getFrameIndex(LastFI, getPointerTy());

    // emit ISD::STORE whichs stores the
    // parameter value to a stack Location
    MemOpChains.push_back(DAG.getStore(Chain, DL, Arg, PtrOff,
                                       MachinePointerInfo(), false, false, 0));
  }

  // Extend range of indices of frame objects for outgoing arguments that were
  // created during this function call. Skip this step if no such objects were
  // created.
  if (LastFI)
    Cpu0FI->extendOutArgFIRange(FirstFI, LastFI);

  // If a memcpy has been created to copy a byval arg to a stack, replace the
  // chain input of CallSeqStart with ByValChain.
  if (InChain != ByValChain)
    DAG.UpdateNodeOperands(CallSeqStart.getNode(), ByValChain,
                           NextStackOffsetVal);

  // Transform all store nodes into one single node because all store
  // nodes are independent of each other.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  // If the callee is a GlobalAddress/ExternalSymbol node (quite common, every
  // direct call is) turn it into a TargetGlobalAddress/TargetExternalSymbol
  // node so that legalize doesn't hack it.
  unsigned char OpFlag;
  bool IsPICCall = IsPIC; // true if calls are translated to jalr $25
  bool GlobalOrExternal = false;
  SDValue CalleeLo;

  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    OpFlag = IsPICCall ? Cpu0II::MO_GOT_CALL : Cpu0II::MO_NO_FLAG;
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL,
                                          getPointerTy(), 0, OpFlag);
    GlobalOrExternal = true;
  }
  else if (ExternalSymbolSDNode *S = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    if (!IsPIC) // static
      OpFlag = Cpu0II::MO_NO_FLAG;
    else // O32 & PIC
      OpFlag = Cpu0II::MO_GOT_CALL;
    Callee = DAG.getTargetExternalSymbol(S->getSymbol(), getPointerTy(),
                                         OpFlag);
    GlobalOrExternal = true;
  }

  SDValue InFlag;

  // Create nodes that load address of callee and copy it to T9
  if (IsPICCall) {
    if (GlobalOrExternal) {
      // Load callee address
      Callee = DAG.getNode(Cpu0ISD::Wrapper, DL, getPointerTy(),
                           getGlobalReg(DAG, getPointerTy()), Callee);
      SDValue LoadValue = DAG.getLoad(getPointerTy(), DL, DAG.getEntryNode(),
                                      Callee, MachinePointerInfo::getGOT(),
                                      false, false, false, 0);

      // Use GOT+LO if callee has internal linkage.
      if (CalleeLo.getNode()) {
        SDValue Lo = DAG.getNode(Cpu0ISD::Lo, DL, getPointerTy(), CalleeLo);
        Callee = DAG.getNode(ISD::ADD, DL, getPointerTy(), LoadValue, Lo);
      } else
        Callee = LoadValue;
    }
  }

  // T9 should contain the address of the callee function if
  // -reloction-model=pic or it is an indirect call.
  if (IsPICCall || !GlobalOrExternal) {
    // copy to T9
    unsigned T9Reg = Cpu0::T9;
    Chain = DAG.getCopyToReg(Chain, DL, T9Reg, Callee, SDValue(0, 0));
    InFlag = Chain.getValue(1);
    Callee = DAG.getRegister(T9Reg, getPointerTy());
  }

  // Cpu0JmpLink = #chain, #target_address, #opt_in_flags...
  //             = Chain, Callee, Reg#1, Reg#2, ...
  //
  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are
  // known live into the call.
  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i)
    Ops.push_back(DAG.getRegister(RegsToPass[i].first,
                                  RegsToPass[i].second.getValueType()));

  // Add a register mask operand representing the call-preserved registers.
  const TargetRegisterInfo *TRI = getTargetMachine().getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(CallConv);
  assert(Mask && "Missing call preserved mask for calling convention");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  Chain  = DAG.getNode(Cpu0ISD::JmpLink, DL, NodeTys, Ops);
  InFlag = Chain.getValue(1);

  // Create the CALLSEQ_END node.
  Chain = DAG.getCALLSEQ_END(Chain,
                             DAG.getIntPtrConstant(NextStackOffset, true),
                             DAG.getIntPtrConstant(0, true), InFlag, DL);
  InFlag = Chain.getValue(1);

  // Handle result values, copying them out of physregs into vregs that we
  // return.
  return LowerCallResult(Chain, InFlag, CallConv, isVarArg,
                         Ins, DL, DAG, InVals);
}

/// LowerCallResult - Lower the result values of a call into the
/// appropriate copies out of appropriate physical registers.
SDValue
Cpu0TargetLowering::LowerCallResult(SDValue Chain, SDValue InFlag,
                                    CallingConv::ID CallConv, bool isVarArg,
                                    const SmallVectorImpl<ISD::InputArg> &Ins,
                                    SDLoc DL, SelectionDAG &DAG,
                                    SmallVectorImpl<SDValue> &InVals) const {
  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
		 getTargetMachine(), RVLocs, *DAG.getContext());

  CCInfo.AnalyzeCallResult(Ins, RetCC_Cpu0);

  // Copy all of the result registers out of their specified physreg.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    Chain = DAG.getCopyFromReg(Chain, DL, RVLocs[i].getLocReg(),
                               RVLocs[i].getValVT(), InFlag).getValue(1);
    InFlag = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
//             Formal Arguments Calling Convention Implementation
//===----------------------------------------------------------------------===//
static void ReadByValArg(MachineFunction &MF, SDValue Chain, SDLoc DL,
                         std::vector<SDValue>& OutChains,
                         SelectionDAG &DAG, unsigned NumWords, SDValue FIN,
                         const CCValAssign &VA, const ISD::ArgFlagsTy& Flags,
                         const Argument *FuncArg) {
  unsigned LocMem = VA.getLocMemOffset();
  unsigned FirstWord = LocMem / 4;

  // copy register A0 - A1 to frame object
  for (unsigned i = 0; i < NumWords; ++i) {
    unsigned CurWord = FirstWord + i;
    if (CurWord >= IntRegsSize)
      break;

    unsigned SrcReg = IntRegs[CurWord];
    unsigned Reg = AddLiveIn(MF, SrcReg, &Cpu0::CPURegsRegClass);
    SDValue StorePtr = DAG.getNode(ISD::ADD, DL, MVT::i32, FIN,
                                   DAG.getConstant(i * 4, MVT::i32));
    SDValue Store = DAG.getStore(Chain, DL, DAG.getRegister(Reg, MVT::i32),
                                 StorePtr, MachinePointerInfo(FuncArg, i * 4),
                                 false, false, 0);
    OutChains.push_back(Store);
  }
} // lbd document - mark - ReadByValArg

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
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo *MFI = MF.getFrameInfo();
  Cpu0FunctionInfo *Cpu0FI = MF.getInfo<Cpu0FunctionInfo>();

  Cpu0FI->setVarArgsFrameIndex(0);

  // Used with vargs to acumulate store chains.
  std::vector<SDValue> OutChains;

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
                 getTargetMachine(), ArgLocs, *DAG.getContext());
                         
  CCInfo.AnalyzeFormalArguments(Ins, CC_Cpu0);

  Function::const_arg_iterator FuncArg =
    DAG.getMachineFunction().getFunction()->arg_begin();
  int LastFI = 0;// Cpu0FI->LastInArgFI is 0 at the entry of this function.

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i, ++FuncArg) {
    CCValAssign &VA = ArgLocs[i];
    EVT ValVT = VA.getValVT();
    ISD::ArgFlagsTy Flags = Ins[i].Flags;

    if (Flags.isByVal()) {
      assert(Flags.getByValSize() &&
             "ByVal args of size 0 should have been ignored by front-end."); 
      unsigned NumWords = (Flags.getByValSize() + 3) / 4;
      LastFI = MFI->CreateFixedObject(NumWords * 4, VA.getLocMemOffset(),
                                      true);
      SDValue FIN = DAG.getFrameIndex(LastFI, getPointerTy());
      InVals.push_back(FIN);
      ReadByValArg(MF, Chain, DL, OutChains, DAG, NumWords, FIN, VA, Flags,
                   &*FuncArg);
      continue;
    }

    // sanity check
    assert(VA.isMemLoc());

    // The stack pointer offset is relative to the caller stack frame.
    LastFI = MFI->CreateFixedObject(ValVT.getSizeInBits()/8,
                                    VA.getLocMemOffset(), true);

    // Create load nodes to retrieve arguments from the stack
    SDValue FIN = DAG.getFrameIndex(LastFI, getPointerTy());
    InVals.push_back(DAG.getLoad(ValVT, DL, Chain, FIN,
                                 MachinePointerInfo::getFixedStack(LastFI),
                                 false, false, false, 0));
  }

#if 1 // Incomming. Without this, it will use $3 instead of $2 as return 
  // register. The cpu0 ABIs for returning structs by value requires that we 
  // copy the sret argument into $v0 for the return. Save the argument into
  // a virtual register so that we can access it from the return points.
  if (DAG.getMachineFunction().getFunction()->hasStructRetAttr()) {
    unsigned Reg = Cpu0FI->getSRetReturnReg();
    if (!Reg) {
      Reg = MF.getRegInfo().createVirtualRegister(getRegClassFor(MVT::i32));
      Cpu0FI->setSRetReturnReg(Reg);
    }
    SDValue Copy = DAG.getCopyToReg(DAG.getEntryNode(), DL, Reg, InVals[0]);
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, Copy, Chain);
  }
#endif // lbd document - mark - endif - hasStructRetAttr()

  if (isVarArg) {
    int FirstRegSlotOffset = 0; // offset of $a0's slot.
    unsigned RegSize = Cpu0::CPURegsRegClass.getSize();
    int RegSlotOffset = FirstRegSlotOffset + ArgLocs.size() * RegSize;

    // Offset of the first variable argument from stack pointer.
    int FirstVaArgOffset;

    FirstVaArgOffset = RegSlotOffset;

    // Record the frame index of the first variable argument
    // which is a value necessary to VASTART.
    LastFI = MFI->CreateFixedObject(RegSize, FirstVaArgOffset, true);
    Cpu0FI->setVarArgsFrameIndex(LastFI);
  } // lbd document - mark - if (isVarArg)
  Cpu0FI->setLastInArgFI(LastFI);
  // All stores are grouped in one node to allow the matching between
  // the size of Ins and InVals. This only happens when on varg functions
  if (!OutChains.empty()) {
    OutChains.push_back(Chain);
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, OutChains);
  } // if (!OutChains.empty())
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

