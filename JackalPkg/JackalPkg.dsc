[Defines]
  PLATFORM_NAME                  = JackalPkg
  PLATFORM_GUID                  = 8B4D1F32-5A6C-4D9E-C7B2-0F1E3D6C9A4B
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/JackalPkg
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = DEBUG
  SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  # Note: local EDK2 tree has no MdePkg BaseSerialPortLib16550; use PcAtChipsetPkg
  # SerialIoLib (16550 COM1, same instance as OvmfPkgX64, deps BaseLib/IoLib only).
  SerialPortLib|PcAtChipsetPkg/Library/SerialIoLib/SerialIoLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  ReportStatusCodeLib|MdePkg/Library/BaseReportStatusCodeLibNull/BaseReportStatusCodeLibNull.inf
  # Note: this EDK2's UefiApplicationEntryPoint consumes StackCheckLib (newer than
  # the brief's DSC); map the null instance to keep the app minimal.
  StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
  # Note: MSVC /O2 folds simple clear/copy loops into memset/memcpy calls (LNK2001);
  # EDK2 has no libc, CompilerIntrinsicsLib provides the MSFT implementations.
  CompilerIntrinsicsLib|MdePkg/Library/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf

[Components]
  JackalPkg/Application/JackalUefi/JackalUefi.inf
