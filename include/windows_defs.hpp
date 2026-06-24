/*
    Windows definition file, adapted for C++ namespaces for better
    accessibility.
*/

#include "platform_windows.hpp"
#include "typemask.hpp"

namespace scl2::windows
{


enum class ErrorCode : dword_t
{
    Success = 0,
    InvalidFunction = 1,
    FileNotFound = 2,
    PathNotFound = 3,
    TooManyOpenFiles = 4,
    AccessDenied = 5,
    InvalidHandle = 6,
    ArenaOverflow = 7,
    NotEnoughMemory = 8,
    InvalidBlock = 9,
    BadEnvironment = 10,
    BadFormat = 11,
    InvalidAccess = 12,
    InvalidData = 13,
    OutOfMemory = 14,
    InvalidDrive = 15,
    CurrentDirectory = 16,
    NotSameDevice = 17,
    NoMoreFiles = 18,
    WriteProtect = 19,
    BadUnit = 20,
    NotReady = 21,
    BadCommand = 22,
    CRC = 23,
    BadLength = 24,
    Seek = 25,
    NotDosDisk = 26,
    SectorNotFound = 27,
    OutOfPaper = 28,
    WriteFault = 29,
    ReadFault = 30,
    GenFailure = 31,
    ShareViolation = 32,
    LockViolation = 33,
    WrongDisk = 34,
    FCBUnavailable = 35,
    SharingBufferExceeded = 36,
    HandleEof = 38,
    HandleDiskFull = 39,
    NotSupported = 50,
    RemNotList = 51,
    DupName = 52,
    BadNetPath = 53,
    NetworkBusy = 54,
    DevNotExist = 55,
    TooManyCmds = 56,

    // Skipping some error codes for brevity
    // These are not commonly used in modern applications.

    FileExists = 80,
    CannotMake = 82,

    InvalidParameter = 87,

    InsufficientBuffer = 122,
    InvalidName = 123,
    InvalidLevel = 124,
    NoVolumeLabel = 125,

    WaitNoChildren = 128,
    ChildNotComplete = 129,
    DirectAccessHandle = 130,
    NegativeSeek = 131,
    SeekOnDevice = 132,

    PathBusy = 148,

    LabelTooLong = 154,

    SignalRefused = 156,

    Discarded = 157,
    NotLocked = 158,
    BadThreadIDAddr = 159,
    BadArguments = 160,
    BadPathName = 161,
    SignalPending = 162,
    MaxThreadsReached = 164,
    LockFaield = 167,
    Busy = 170,

    AtomicLockNotSupported = 174,
    InvalidSegmentNumber = 180,

    AlreadyExists = 183,
    InvalidFlagNumber = 186,

    InvalidExeSignature = 191,
    BadExeFormat = 191,

    FilenameExceedRange = 206,

    InvalidSignalNumber = 209,

    Locked = 212,

    MachineTypeMismatch = 216, // This is ususally caused by incompatible architecture or bitwidth.
    CannotModifySignedBinary = 217,
    CannotModifyStrongSignedBinary = 218,
    FileCheckedOut = 220,
    CheckOutRequired = 221,
    BadFileType = 222,
    FileTooLarge = 223,
    FormsAuthRequired = 224,
    VirusInfected = 225,
    VirusDeleted = 226,
    PipeLocal = 229,
    BadPipe = 230,
    PipeBusy = 231,
    NoData = 232,
    PipeNotConnected = 233,
    MoreData = 234,
    NoWorkDone = 235,

    WaitTimeout = 258,
    NoMoreItems = 259,

    CannotCopy = 266,
    Directory = 267,

    PartialCopy = 299,

    DiskTooFragmented = 302,
    DeletePending = 303,

    NotAllowedOnSystemFile = 313,

    InvalidToken = 315,
    DeviceFeatureNotSupported = 316,

    DeviceUnreachable = 321,
    DeviceNoResources = 322,
    DataChecksumError = 323,
    FileLevelTrimNotSupported = 324,

    OperationInProgress = 329,
};



} // namespace scl2::windows