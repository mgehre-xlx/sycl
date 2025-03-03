//===-- SIRegisterInfo.h - SI Register Info Interface ----------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Interface definition for SIRegisterInfo
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_AMDGPU_SIREGISTERINFO_H
#define LLVM_LIB_TARGET_AMDGPU_SIREGISTERINFO_H

#define GET_REGINFO_HEADER
#include "AMDGPUGenRegisterInfo.inc"

namespace llvm {

class GCNSubtarget;
class LiveIntervals;
class LivePhysRegs;
class RegisterBank;
struct SGPRSpillBuilder;
class SIMachineFunctionInfo;

class SIRegisterInfo final : public AMDGPUGenRegisterInfo {
private:
  const GCNSubtarget &ST;
  bool SpillSGPRToVGPR;
  bool isWave32;
  BitVector RegPressureIgnoredUnits;

  /// Sub reg indexes for getRegSplitParts.
  /// First index represents subreg size from 1 to 16 DWORDs.
  /// The inner vector is sorted by bit offset.
  /// Provided a register can be fully split with given subregs,
  /// all elements of the inner vector combined give a full lane mask.
  static std::array<std::vector<int16_t>, 16> RegSplitParts;

  // Table representing sub reg of given width and offset.
  // First index is subreg size: 32, 64, 96, 128, 160, 192, 224, 256, 512.
  // Second index is 32 different dword offsets.
  static std::array<std::array<uint16_t, 32>, 9> SubRegFromChannelTable;

  void reserveRegisterTuples(BitVector &, MCRegister Reg) const;

public:
  SIRegisterInfo(const GCNSubtarget &ST);

  /// \returns the sub reg enum value for the given \p Channel
  /// (e.g. getSubRegFromChannel(0) -> AMDGPU::sub0)
  static unsigned getSubRegFromChannel(unsigned Channel, unsigned NumRegs = 1);

  bool spillSGPRToVGPR() const {
    return SpillSGPRToVGPR;
  }

  /// Return the end register initially reserved for the scratch buffer in case
  /// spilling is needed.
  MCRegister reservedPrivateSegmentBufferReg(const MachineFunction &MF) const;

  BitVector getReservedRegs(const MachineFunction &MF) const override;

  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;
  const MCPhysReg *getCalleeSavedRegsViaCopy(const MachineFunction *MF) const;
  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID) const override;
  const uint32_t *getNoPreservedMask() const override;

  bool addAllocPriorityToGlobalRanges() const override {
    return true;
  }

  // Stack access is very expensive. CSRs are also the high registers, and we
  // want to minimize the number of used registers.
  unsigned getCSRFirstUseCost() const override {
    return 100;
  }

  Register getFrameRegister(const MachineFunction &MF) const override;

  bool hasBasePointer(const MachineFunction &MF) const;
  Register getBaseRegister() const;

  bool shouldRealignStack(const MachineFunction &MF) const override;
  bool requiresRegisterScavenging(const MachineFunction &Fn) const override;

  bool requiresFrameIndexScavenging(const MachineFunction &MF) const override;
  bool requiresFrameIndexReplacementScavenging(
    const MachineFunction &MF) const override;
  bool requiresVirtualBaseRegisters(const MachineFunction &Fn) const override;

  int64_t getScratchInstrOffset(const MachineInstr *MI) const;

  int64_t getFrameIndexInstrOffset(const MachineInstr *MI,
                                   int Idx) const override;

  bool needsFrameBaseReg(MachineInstr *MI, int64_t Offset) const override;

  Register materializeFrameBaseRegister(MachineBasicBlock *MBB, int FrameIdx,
                                        int64_t Offset) const override;

  void resolveFrameIndex(MachineInstr &MI, Register BaseReg,
                         int64_t Offset) const override;

  bool isFrameOffsetLegal(const MachineInstr *MI, Register BaseReg,
                          int64_t Offset) const override;

  const TargetRegisterClass *getPointerRegClass(
    const MachineFunction &MF, unsigned Kind = 0) const override;

  void buildVGPRSpillLoadStore(SGPRSpillBuilder &SB, int Index, int Offset,
                               bool IsLoad, bool IsKill = true) const;

  /// If \p OnlyToVGPR is true, this will only succeed if this
  bool spillSGPR(MachineBasicBlock::iterator MI,
                 int FI, RegScavenger *RS,
                 LiveIntervals *LIS = nullptr,
                 bool OnlyToVGPR = false) const;

  bool restoreSGPR(MachineBasicBlock::iterator MI,
                   int FI, RegScavenger *RS,
                   LiveIntervals *LIS = nullptr,
                   bool OnlyToVGPR = false) const;

  void eliminateFrameIndex(MachineBasicBlock::iterator MI, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS) const override;

  bool eliminateSGPRToVGPRSpillFrameIndex(MachineBasicBlock::iterator MI,
                                          int FI, RegScavenger *RS,
                                          LiveIntervals *LIS = nullptr) const;

  StringRef getRegAsmName(MCRegister Reg) const override;

  // Pseudo regs are not allowed
  unsigned getHWRegIndex(MCRegister Reg) const {
    return getEncodingValue(Reg) & 0xff;
  }

  LLVM_READONLY
  const TargetRegisterClass *getVGPRClassForBitWidth(unsigned BitWidth) const;

  LLVM_READONLY
  const TargetRegisterClass *getAGPRClassForBitWidth(unsigned BitWidth) const;

  LLVM_READONLY
  static const TargetRegisterClass *getSGPRClassForBitWidth(unsigned BitWidth);

  /// Return the 'base' register class for this register.
  /// e.g. SGPR0 => SReg_32, VGPR => VGPR_32 SGPR0_SGPR1 -> SReg_32, etc.
  const TargetRegisterClass *getPhysRegClass(MCRegister Reg) const;

  /// \returns true if this class contains only SGPR registers
  bool isSGPRClass(const TargetRegisterClass *RC) const {
    return !hasVGPRs(RC) && !hasAGPRs(RC);
  }

  /// \returns true if this class ID contains only SGPR registers
  bool isSGPRClassID(unsigned RCID) const {
    return isSGPRClass(getRegClass(RCID));
  }

  bool isSGPRReg(const MachineRegisterInfo &MRI, Register Reg) const;

  /// \returns true if this class contains only AGPR registers
  bool isAGPRClass(const TargetRegisterClass *RC) const {
    return hasAGPRs(RC) && !hasVGPRs(RC);
  }

  /// \returns true if this class contains VGPR registers.
  bool hasVGPRs(const TargetRegisterClass *RC) const;

  /// \returns true if this class contains AGPR registers.
  bool hasAGPRs(const TargetRegisterClass *RC) const;

  /// \returns true if this class contains any vector registers.
  bool hasVectorRegisters(const TargetRegisterClass *RC) const {
    return hasVGPRs(RC) || hasAGPRs(RC);
  }

  /// \returns A VGPR reg class with the same width as \p SRC
  const TargetRegisterClass *
  getEquivalentVGPRClass(const TargetRegisterClass *SRC) const;

  /// \returns An AGPR reg class with the same width as \p SRC
  const TargetRegisterClass *
  getEquivalentAGPRClass(const TargetRegisterClass *SRC) const;

  /// \returns A SGPR reg class with the same width as \p SRC
  const TargetRegisterClass *
  getEquivalentSGPRClass(const TargetRegisterClass *VRC) const;

  /// \returns The canonical register class that is used for a sub-register of
  /// \p RC for the given \p SubIdx.  If \p SubIdx equals NoSubRegister, \p RC
  /// will be returned.
  const TargetRegisterClass *getSubRegClass(const TargetRegisterClass *RC,
                                            unsigned SubIdx) const;

  /// Returns a register class which is compatible with \p SuperRC, such that a
  /// subregister exists with class \p SubRC with subregister index \p
  /// SubIdx. If this is impossible (e.g., an unaligned subregister index within
  /// a register tuple), return null.
  const TargetRegisterClass *
  getCompatibleSubRegClass(const TargetRegisterClass *SuperRC,
                           const TargetRegisterClass *SubRC,
                           unsigned SubIdx) const;

  bool shouldRewriteCopySrc(const TargetRegisterClass *DefRC,
                            unsigned DefSubReg,
                            const TargetRegisterClass *SrcRC,
                            unsigned SrcSubReg) const override;

  /// \returns True if operands defined with this operand type can accept
  /// a literal constant (i.e. any 32-bit immediate).
  bool opCanUseLiteralConstant(unsigned OpType) const;

  /// \returns True if operands defined with this operand type can accept
  /// an inline constant. i.e. An integer value in the range (-16, 64) or
  /// -4.0f, -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f, 4.0f.
  bool opCanUseInlineConstant(unsigned OpType) const;

  MCRegister findUnusedRegister(const MachineRegisterInfo &MRI,
                                const TargetRegisterClass *RC,
                                const MachineFunction &MF,
                                bool ReserveHighestVGPR = false) const;

  const TargetRegisterClass *getRegClassForReg(const MachineRegisterInfo &MRI,
                                               Register Reg) const;
  bool isVGPR(const MachineRegisterInfo &MRI, Register Reg) const;
  bool isAGPR(const MachineRegisterInfo &MRI, Register Reg) const;
  bool isVectorRegister(const MachineRegisterInfo &MRI, Register Reg) const {
    return isVGPR(MRI, Reg) || isAGPR(MRI, Reg);
  }

  bool isConstantPhysReg(MCRegister PhysReg) const override;

  bool isDivergentRegClass(const TargetRegisterClass *RC) const override {
    return !isSGPRClass(RC);
  }

  ArrayRef<int16_t> getRegSplitParts(const TargetRegisterClass *RC,
                                     unsigned EltSize) const;

  bool shouldCoalesce(MachineInstr *MI,
                      const TargetRegisterClass *SrcRC,
                      unsigned SubReg,
                      const TargetRegisterClass *DstRC,
                      unsigned DstSubReg,
                      const TargetRegisterClass *NewRC,
                      LiveIntervals &LIS) const override;

  unsigned getRegPressureLimit(const TargetRegisterClass *RC,
                               MachineFunction &MF) const override;

  unsigned getRegPressureSetLimit(const MachineFunction &MF,
                                  unsigned Idx) const override;

  const int *getRegUnitPressureSets(unsigned RegUnit) const override;

  MCRegister getReturnAddressReg(const MachineFunction &MF) const;

  const TargetRegisterClass *
  getRegClassForSizeOnBank(unsigned Size,
                           const RegisterBank &Bank,
                           const MachineRegisterInfo &MRI) const;

  const TargetRegisterClass *
  getRegClassForTypeOnBank(LLT Ty,
                           const RegisterBank &Bank,
                           const MachineRegisterInfo &MRI) const {
    return getRegClassForSizeOnBank(Ty.getSizeInBits(), Bank, MRI);
  }

  const TargetRegisterClass *
  getConstrainedRegClassForOperand(const MachineOperand &MO,
                                 const MachineRegisterInfo &MRI) const override;

  const TargetRegisterClass *getBoolRC() const {
    return isWave32 ? &AMDGPU::SReg_32RegClass
                    : &AMDGPU::SReg_64RegClass;
  }

  const TargetRegisterClass *getWaveMaskRegClass() const {
    return isWave32 ? &AMDGPU::SReg_32_XM0_XEXECRegClass
                    : &AMDGPU::SReg_64_XEXECRegClass;
  }

  // Return the appropriate register class to use for 64-bit VGPRs for the
  // subtarget.
  const TargetRegisterClass *getVGPR64Class() const;

  MCRegister getVCC() const;

  const TargetRegisterClass *getRegClass(unsigned RCID) const;

  // Find reaching register definition
  MachineInstr *findReachingDef(Register Reg, unsigned SubReg,
                                MachineInstr &Use,
                                MachineRegisterInfo &MRI,
                                LiveIntervals *LIS) const;

  const uint32_t *getAllVGPRRegMask() const;
  const uint32_t *getAllAGPRRegMask() const;
  const uint32_t *getAllVectorRegMask() const;
  const uint32_t *getAllAllocatableSRegMask() const;

  // \returns number of 32 bit registers covered by a \p LM
  static unsigned getNumCoveredRegs(LaneBitmask LM) {
    // The assumption is that every lo16 subreg is an even bit and every hi16
    // is an adjacent odd bit or vice versa.
    uint64_t Mask = LM.getAsInteger();
    uint64_t Even = Mask & 0xAAAAAAAAAAAAAAAAULL;
    Mask = (Even >> 1) | Mask;
    uint64_t Odd = Mask & 0x5555555555555555ULL;
    return countPopulation(Odd);
  }

  // \returns a DWORD offset of a \p SubReg
  unsigned getChannelFromSubReg(unsigned SubReg) const {
    return SubReg ? (getSubRegIdxOffset(SubReg) + 31) / 32 : 0;
  }

  // \returns a DWORD size of a \p SubReg
  unsigned getNumChannelsFromSubReg(unsigned SubReg) const {
    return getNumCoveredRegs(getSubRegIndexLaneMask(SubReg));
  }

  // For a given 16 bit \p Reg \returns a 32 bit register holding it.
  // \returns \p Reg otherwise.
  MCPhysReg get32BitRegister(MCPhysReg Reg) const;

  // Returns true if a given register class is properly aligned for
  // the subtarget.
  bool isProperlyAlignedRC(const TargetRegisterClass &RC) const;

  /// Return all SGPR128 which satisfy the waves per execution unit requirement
  /// of the subtarget.
  ArrayRef<MCPhysReg> getAllSGPR128(const MachineFunction &MF) const;

  /// Return all SGPR64 which satisfy the waves per execution unit requirement
  /// of the subtarget.
  ArrayRef<MCPhysReg> getAllSGPR64(const MachineFunction &MF) const;

  /// Return all SGPR32 which satisfy the waves per execution unit requirement
  /// of the subtarget.
  ArrayRef<MCPhysReg> getAllSGPR32(const MachineFunction &MF) const;

  // Insert spill or restore instructions.
  // When lowering spill pseudos, the RegScavenger should be set.
  // For creating spill instructions during frame lowering, where no scavenger
  // is available, LiveRegs can be used.
  void buildSpillLoadStore(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MI, unsigned LoadStoreOp,
                           int Index, Register ValueReg, bool ValueIsKill,
                           MCRegister ScratchOffsetReg, int64_t InstrOffset,
                           MachineMemOperand *MMO, RegScavenger *RS,
                           LivePhysRegs *LiveRegs = nullptr) const;
};

} // End namespace llvm

#endif
