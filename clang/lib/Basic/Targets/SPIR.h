//===--- SPIR.h - Declare SPIR target feature support -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares SPIR TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_SPIR_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_SPIR_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/Compiler.h"
#include "OSTargets.h"

namespace clang {
namespace targets {

static const unsigned SPIRDefIsPrivMap[] = {
    0, // Default
    1, // opencl_global
    3, // opencl_local
    2, // opencl_constant
    0, // opencl_private
    4, // opencl_generic
    5, // opencl_global_device
    6, // opencl_global_host
    0, // cuda_device
    0, // cuda_constant
    0, // cuda_shared
    // SYCL address space values for this map are dummy
    0, // sycl_global
    0, // sycl_global_device
    0, // sycl_global_host
    0, // sycl_local
    0, // sycl_private
    0, // ptr32_sptr
    0, // ptr32_uptr
    0  // ptr64
};

static const unsigned SPIRDefIsGenMap[] = {
    4, // Default
    // OpenCL address space values for this map are dummy and they can't be used
    // FIXME: reset opencl_global entry to 0. Currently CodeGen libary uses
    // opencl_global in SYCL language mode, but we should switch to using
    // sycl_global instead.
    1, // opencl_global
    0, // opencl_local
    2, // opencl_constant
    0, // opencl_private
    0, // opencl_generic
    0, // opencl_global_device
    0, // opencl_global_host
    0, // cuda_device
    0, // cuda_constant
    0, // cuda_shared
    1, // sycl_global
    5, // sycl_global_device
    6, // sycl_global_host
    3, // sycl_local
    0, // sycl_private
    0, // ptr32_sptr
    0, // ptr32_uptr
    0  // ptr64
};

class LLVM_LIBRARY_VISIBILITY SPIRTargetInfo : public TargetInfo {
public:
  SPIRTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    TLSSupported = false;
    VLASupported = false;
    LongWidth = LongAlign = 64;
    AddrSpaceMap = &SPIRDefIsPrivMap;
    UseAddrSpaceMapMangling = true;
    HasLegalHalfType = true;
    HasFloat16 = true;
    // Define available target features
    // These must be defined in sorted order!
    NoAsmVariants = true;
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  bool hasFeature(StringRef Feature) const override {
    return Feature == "spir";
  }

  // SPIR supports the half type and the only llvm intrinsic allowed in SPIR is
  // memcpy as per section 3 of the SPIR spec.
  bool useFP16ConversionIntrinsics() const override { return false; }

  ArrayRef<Builtin::Info> getTargetBuiltins() const override { return None; }

  const char *getClobbers() const override { return ""; }

  ArrayRef<const char *> getGCCRegNames() const override { return None; }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &info) const override {
    return true;
  }

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    return None;
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  Optional<unsigned>
  getDWARFAddressSpace(unsigned AddressSpace) const override {
    return AddressSpace;
  }

  CallingConvCheckResult checkCallingConvention(CallingConv CC) const override {
    return (CC == CC_SpirFunction || CC == CC_OpenCLKernel) ? CCCR_OK
                                                            : CCCR_Warning;
  }

  CallingConv getDefaultCallingConv() const override {
    return CC_SpirFunction;
  }

  void setAddressSpaceMap(bool DefaultIsGeneric) {
    AddrSpaceMap = DefaultIsGeneric ? &SPIRDefIsGenMap : &SPIRDefIsPrivMap;
  }

  void adjust(DiagnosticsEngine &Diags, LangOptions &Opts) override {
    TargetInfo::adjust(Diags, Opts);
    // NOTE: SYCL specification considers unannotated pointers and references
    // to be pointing to the generic address space. See section 5.9.3 of
    // SYCL 2020 specification.
    // Currently, there is no way of representing SYCL's default address space
    // language semantics along with the semantics of embedded C's default
    // address space in the same address space map. Hence the map needs to be
    // reset to allow mapping to the desired value of 'Default' entry for SYCL.
    setAddressSpaceMap(/*DefaultIsGeneric=*/Opts.SYCLIsDevice);
  }

  void setSupportedOpenCLOpts() override {
    // Assume all OpenCL extensions and optional core features are supported
    // for SPIR since it is a generic target.
    supportAllOpenCLOpts();
  }

  bool hasExtIntType() const override { return true; }

  bool hasInt128Type() const override { return false; }
};

class LLVM_LIBRARY_VISIBILITY SPIR32TargetInfo : public SPIRTargetInfo {
public:
  SPIR32TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : SPIRTargetInfo(Triple, Opts) {
    PointerWidth = PointerAlign = 32;
    SizeType = TargetInfo::UnsignedInt;
    PtrDiffType = IntPtrType = TargetInfo::SignedInt;
    if (Triple.isXilinxFPGA())
      resetDataLayout(
          "e-m:e-p:32:32-i64:64-i128:128-i256:256-i512:512-i1024:1024-i2048:"
          "2048-i4096:4096-n8:16:32:64-S128-v16:16-v24:32-v32:32-v48:64-v96:"
          "128-v192:256-v256:256-v512:512-v1024:1024");
    else
      resetDataLayout(
          "e-i64:64-v16:16-v24:32-v32:32-v48:64-"
          "v96:128-v192:256-v256:256-v512:512-v1024:1024-n8:16:32:64");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;
};

class LLVM_LIBRARY_VISIBILITY SPIR64TargetInfo : public SPIRTargetInfo {
public:
  SPIR64TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : SPIRTargetInfo(Triple, Opts) {
    PointerWidth = PointerAlign = 64;
    SizeType = TargetInfo::UnsignedLong;
    PtrDiffType = IntPtrType = TargetInfo::SignedLong;

    if (Triple.isXilinxFPGA())
      resetDataLayout(
          "e-m:e-i64:64-i128:128-i256:256-i512:512-i1024:1024-i2048:2048-i4096:"
          "4096-n8:16:32:64-S128-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-"
          "v256:256-v512:512-v1024:1024");
    else
      resetDataLayout(
          "e-i64:64-v16:16-v24:32-v32:32-v48:64-"
          "v96:128-v192:256-v256:256-v512:512-v1024:1024-n8:16:32:64");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;
};

class LLVM_LIBRARY_VISIBILITY SPIR32SYCLDeviceTargetInfo
    : public SPIR32TargetInfo {
public:
  SPIR32SYCLDeviceTargetInfo(const llvm::Triple &Triple,
                             const TargetOptions &Opts)
      : SPIR32TargetInfo(Triple, Opts) {
    // This is workaround for exception_ptr class.
    // Exceptions is not allowed in sycl device code but we should be able
    // to parse host code. So we allow compilation of exception_ptr but
    // if exceptions are used in device code we should emit a diagnostic.
    MaxAtomicInlineWidth = 32;
  }
};

class LLVM_LIBRARY_VISIBILITY SPIR64SYCLDeviceTargetInfo
    : public SPIR64TargetInfo {
public:
  SPIR64SYCLDeviceTargetInfo(const llvm::Triple &Triple,
                             const TargetOptions &Opts)
      : SPIR64TargetInfo(Triple, Opts) {
    // This is workaround for exception_ptr class.
    // Exceptions is not allowed in sycl device code but we should be able
    // to parse host code. So we allow compilation of exception_ptr but
    // if exceptions are used in device code we should emit a diagnostic.
    MaxAtomicInlineWidth = 64;
  }
};

// x86-32 SPIR Windows target
class LLVM_LIBRARY_VISIBILITY WindowsX86_32SPIRTargetInfo
    : public WindowsTargetInfo<SPIR32SYCLDeviceTargetInfo> {
public:
  WindowsX86_32SPIRTargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : WindowsTargetInfo<SPIR32SYCLDeviceTargetInfo>(Triple, Opts) {
    DoubleAlign = LongLongAlign = 64;
    WCharType = UnsignedShort;
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::CharPtrBuiltinVaList;
  }

  CallingConvCheckResult checkCallingConvention(CallingConv CC) const override {
    if (CC == CC_X86VectorCall)
      // Permit CC_X86VectorCall which is used in Microsoft headers
      return CCCR_OK;
    return (CC == CC_SpirFunction || CC == CC_OpenCLKernel) ? CCCR_OK
                                    : CCCR_Warning;
  }
};

// x86-32 SPIR Windows Visual Studio target
class LLVM_LIBRARY_VISIBILITY MicrosoftX86_32SPIRTargetInfo
    : public WindowsX86_32SPIRTargetInfo {
public:
  MicrosoftX86_32SPIRTargetInfo(const llvm::Triple &Triple,
                            const TargetOptions &Opts)
      : WindowsX86_32SPIRTargetInfo(Triple, Opts) {
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override {
    WindowsX86_32SPIRTargetInfo::getTargetDefines(Opts, Builder);
    // The value of the following reflects processor type.
    // 300=386, 400=486, 500=Pentium, 600=Blend (default)
    // We lost the original triple, so we use the default.
    // TBD should we keep these lines?  Copied from X86.h.
    Builder.defineMacro("_M_IX86", "600");
  }
};

// x86-64 SPIR64 Windows target
class LLVM_LIBRARY_VISIBILITY WindowsX86_64_SPIR64TargetInfo
    : public WindowsTargetInfo<SPIR64SYCLDeviceTargetInfo> {
public:
  WindowsX86_64_SPIR64TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : WindowsTargetInfo<SPIR64SYCLDeviceTargetInfo>(Triple, Opts) {
    LongWidth = LongAlign = 32;
    DoubleAlign = LongLongAlign = 64;
    IntMaxType = SignedLongLong;
    Int64Type = SignedLongLong;
    SizeType = UnsignedLongLong;
    PtrDiffType = SignedLongLong;
    IntPtrType = SignedLongLong;
    WCharType = UnsignedShort;
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::CharPtrBuiltinVaList;
  }

  CallingConvCheckResult checkCallingConvention(CallingConv CC) const override {
    if (CC == CC_X86VectorCall)
      // Permit CC_X86VectorCall which is used in Microsoft headers
      return CCCR_OK;
    return (CC == CC_SpirFunction || CC == CC_OpenCLKernel) ? CCCR_OK
                                    : CCCR_Warning;
  }
};

// x86-64 SPIR64 Windows Visual Studio target
class LLVM_LIBRARY_VISIBILITY MicrosoftX86_64_SPIR64TargetInfo
    : public WindowsX86_64_SPIR64TargetInfo {
public:
  MicrosoftX86_64_SPIR64TargetInfo(const llvm::Triple &Triple,
                            const TargetOptions &Opts)
      : WindowsX86_64_SPIR64TargetInfo(Triple, Opts) {
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override {
    WindowsX86_64_SPIR64TargetInfo::getTargetDefines(Opts, Builder);
    Builder.defineMacro("_M_X64", "100");
    Builder.defineMacro("_M_AMD64", "100");
  }
};
} // namespace targets
} // namespace clang
#endif // LLVM_CLANG_LIB_BASIC_TARGETS_SPIR_H
