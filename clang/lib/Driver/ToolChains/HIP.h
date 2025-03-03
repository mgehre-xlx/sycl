//===--- HIP.h - HIP ToolChain Implementations ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_HIP_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_HIP_H

#include "clang/Driver/ToolChain.h"
#include "clang/Driver/Tool.h"
#include "AMDGPU.h"

namespace clang {
namespace driver {

namespace tools {

namespace AMDGCN {
  // Construct command for creating HIP fatbin.
  void constructHIPFatbinCommand(Compilation &C, const JobAction &JA,
                  StringRef OutputFileName, const InputInfoList &Inputs,
                  const llvm::opt::ArgList &TCArgs, const Tool& T);

// Runs llvm-link/opt/llc/lld, which links multiple LLVM bitcode, together with
// device library, then compiles it to ISA in a shared object.
class LLVM_LIBRARY_VISIBILITY Linker : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("AMDGCN::Linker", "amdgcn-link", TC) {}

  bool hasIntegratedCPP() const override { return false; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;

private:

  void constructLldCommand(Compilation &C, const JobAction &JA,
                           const InputInfoList &Inputs, const InputInfo &Output,
                           const llvm::opt::ArgList &Args) const;

  // Construct command for creating Object from HIP fatbin.
  void constructGenerateObjFileFromHIPFatBinary(Compilation &C,
                                                const InputInfo &Output,
                                                const InputInfoList &Inputs,
                                                const llvm::opt::ArgList &Args,
                                                const JobAction &JA) const;
};

class LLVM_LIBRARY_VISIBILITY SYCLLinker : public Linker {
public:
  SYCLLinker(const ToolChain &TC) : Linker(TC) {}

  Tool *GetSYCLToolChainLinker() const {
    if (!SYCLToolChainLinker)
      SYCLToolChainLinker.reset(new SYCL::Linker(getToolChain()));
    return SYCLToolChainLinker.get();
  }

private:
  mutable std::unique_ptr<Tool> SYCLToolChainLinker;
};

} // end namespace AMDGCN
} // end namespace tools

namespace toolchains {

class LLVM_LIBRARY_VISIBILITY HIPToolChain final : public ROCMToolChain {
public:
  HIPToolChain(const Driver &D, const llvm::Triple &Triple,
               const ToolChain &HostTC, const llvm::opt::ArgList &Args,
               const Action::OffloadKind OK);

  const llvm::Triple *getAuxTriple() const override {
    return &HostTC.getTriple();
  }

  llvm::opt::DerivedArgList *
  TranslateArgs(const llvm::opt::DerivedArgList &Args, StringRef BoundArch,
                Action::OffloadKind DeviceOffloadKind) const override;
  void addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                             llvm::opt::ArgStringList &CC1Args,
                             Action::OffloadKind DeviceOffloadKind) const override;
  void addClangWarningOptions(llvm::opt::ArgStringList &CC1Args) const override;
  CXXStdlibType GetCXXStdlibType(const llvm::opt::ArgList &Args) const override;
  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  void AddClangCXXStdlibIncludeArgs(
      const llvm::opt::ArgList &Args,
      llvm::opt::ArgStringList &CC1Args) const override;
  void AddIAMCUIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                           llvm::opt::ArgStringList &CC1Args) const override;
  void AddHIPIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                         llvm::opt::ArgStringList &CC1Args) const override;
  llvm::SmallVector<std::string, 12>
  getHIPDeviceLibs(const llvm::opt::ArgList &Args) const override;

  SanitizerMask getSupportedSanitizers() const override;

  VersionTuple
  computeMSVCVersion(const Driver *D,
                     const llvm::opt::ArgList &Args) const override;

  unsigned GetDefaultDwarfVersion() const override { return 5; }

  const ToolChain &HostTC;
  void checkTargetID(const llvm::opt::ArgList &DriverArgs) const override;
  Tool *SelectTool(const JobAction &JA) const override;

protected:
  Tool *buildLinker() const override;

private:
  const Action::OffloadKind OK;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_HIP_H
