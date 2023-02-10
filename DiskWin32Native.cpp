#include "DiskWin32Native.h"
#include <SetupAPI.h>

const CDiskWin32Native::WinGeometryMedia CDiskWin32Native::WindowsSupportedGeometries[] =
{   
    //https://en.wikipedia.org/wiki/List_of_floppy_disk_formats
    
    //FAT 1440 KB 3.5"
    { { 80, 2, 18, CDiskBase::SECT_SZ_512, 0xF6, 0x16, CDiskBase::DISK_DATARATE_HD }, F3_1Pt44_512 },       //USB drive supported
    //FAT 1.232 KB 3.5""
    { { 77, 2, 8, CDiskBase::SECT_SZ_1024, 0xE5, 0x16, CDiskBase::DISK_DATARATE_HD }, F3_1Pt2_512 },        //USB drive supported (Japanese weird format using the same 3.5 HD disks)
    //CP/M 720 KB 3.5"
    { { 80, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x16, CDiskBase::DISK_DATARATE_DD_3_5},  F3_720_512 },      //USB drive supported
    //640 KB 3.5"
    { { 80, 2, 8, CDiskBase::SECT_SZ_512, 0xE5, 0x16, CDiskBase::DISK_DATARATE_DD_3_5},  F3_640_512 },
    
    //1200 KB 5.25"
    { { 80, 2, 15, CDiskBase::SECT_SZ_512, 0xF6, 0x16, CDiskBase::DISK_DATARATE_HD }, F5_1Pt2_512 },
    //CP/M 720 KB 5.25"
    { { 80, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x16, CDiskBase::DISK_DATARATE_DD_5_25},  F5_720_512 },
    //5.25" 640KB 
    { { 80, 2, 8, CDiskBase::SECT_SZ_512, 0xE5, 0x16, CDiskBase::DISK_DATARATE_DD_5_25},  F5_640_512 },
    //5.25" 360KB for CP/M or FAT
    { { 40, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x16, CDiskBase::DISK_DATARATE_DD_5_25 }, F5_360_512 },
    //5.25 320KB
    { { 40, 2, 8, CDiskBase::SECT_SZ_512, 0xE5, 0x16, CDiskBase::DISK_DATARATE_DD_5_25 }, F5_320_512 },
    //5.25 180KB
    { { 40, 1, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x16, CDiskBase::DISK_DATARATE_DD_5_25 }, F5_180_512 },
    //5.25 160KB
    { { 40, 1, 8, CDiskBase::SECT_SZ_512, 0xE5, 0x16, CDiskBase::DISK_DATARATE_DD_5_25 }, F5_160_512 },
};

CDiskWin32Native::CDiskWin32Native():CDiskBase()
{
    hVol = INVALID_HANDLE_VALUE;
}

CDiskWin32Native::CDiskWin32Native(DiskDescType dd): CDiskBase(dd)
{
    hVol = INVALID_HANDLE_VALUE;
}

CDiskWin32Native::~CDiskWin32Native()
{
    if (hVol != INVALID_HANDLE_VALUE)
        CloseHandle(hVol);
}

bool CDiskWin32Native::Open(char* drive, DiskOpenMode mode)
{
    char path[] = "\\\\.\\A:";
    path[strlen(path) - 2] = drive[0];

    hVol = INVALID_HANDLE_VALUE;
       
    hVol = CreateFile(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING, 0,
        NULL);
    
    
    return INVALID_HANDLE_VALUE != hVol;    
}

bool CDiskWin32Native::Seek(byte track)
{
    const dword offset = track * DiskDefinition.SideCnt * DiskDefinition.SectSize * DiskDefinition.SPT;
    return SetFilePointer(hVol, offset, NULL, FILE_BEGIN) == offset;
}

bool CDiskWin32Native::ReadSectors(byte* buf, byte track, byte head, byte sect, byte sectNO)
{
    DWORD dw = 0;
    bool res = true;

    res = Seek(track);
    if (res && head > 0)
        SetFilePointer(hVol, DiskDefinition.SectSize * DiskDefinition.SPT, NULL, FILE_CURRENT);
    if (res && sect > 1)
        SetFilePointer(hVol, DiskDefinition.SectSize * (sect - 1), NULL, FILE_CURRENT);

    if (res)
    {
        byte retryCnt = DEF_RETRY_CNT;
        do
        {
            res = ReadFile(hVol,
                buf,
                sectNO * DiskDefinition.SectSize,
                &dw, NULL);
        } while (!res && retryCnt-- > 0);
    }    

    return res || continueOnError;
}


bool CDiskWin32Native::WriteSectors(byte track, byte side, byte sector, byte sectCnt, byte* buf)
{
    DWORD dw = 0;
    bool res = true;

    res = Seek(track);
    if (res && side > 0)
        res = SetFilePointer(hVol, DiskDefinition.SectSize * DiskDefinition.SPT, NULL, FILE_CURRENT);
    if (res && sector > 1)
        res = SetFilePointer(hVol, DiskDefinition.SectSize * (sector - 1), NULL, FILE_CURRENT);

    res = res && WriteFile(hVol,
        buf,
        sectCnt * DiskDefinition.SectSize,
        &dw, NULL);

    return res;
}

bool CDiskWin32Native::GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectorsInfo[])
{
    DISK_GEOMETRY geom = { 0 };
    DWORD dw;

    bool res = DeviceIoControl(
        hVol,
        IOCTL_DISK_GET_DRIVE_GEOMETRY,
        NULL,
        0,
        (LPVOID)&geom,
        (DWORD)sizeof(DISK_GEOMETRY),
        &dw, NULL
    );

    sectorCnt = (byte)geom.SectorsPerTrack;
    
    for (int secIdx = 0; secIdx < sectorCnt; secIdx++)
    {
        sectorsInfo[secIdx].head = side;
        sectorsInfo[secIdx].track = track;
        sectorsInfo[secIdx].sectID = secIdx + 1;
        sectorsInfo[secIdx].sectSizeCode = SectSize2SectCode((DiskSectType)geom.BytesPerSector);
    }

    return res;
}

bool CDiskWin32Native::GetDiskInfo(byte& trkCnt, byte& sideCnt, char* comment)
{
    _DISK_GEOMETRY geom = { 0 };
    DWORD dw;

    bool res = DeviceIoControl(
        hVol,
        IOCTL_DISK_GET_DRIVE_GEOMETRY,
        NULL,
        0,
        (LPVOID)&geom,
        (DWORD)sizeof(_DISK_GEOMETRY),
        &dw, NULL
    );

    trkCnt = (byte)geom.Cylinders.LowPart;
    sideCnt = (byte)geom.TracksPerCylinder;   

    return res;
}

bool CDiskWin32Native::FormatTrack(byte track, byte side)
{
    DWORD dw;
    _FORMAT_PARAMETERS fmtPrm;
    bool res = true;

    fmtPrm.StartCylinderNumber = fmtPrm.EndCylinderNumber = track;
    fmtPrm.StartHeadNumber = fmtPrm.EndHeadNumber = side;  
    fmtPrm.MediaType = Geometry2MediaType(DiskDefinition);    
    res = fmtPrm.MediaType != Unknown;

    res = res && DeviceIoControl(
        hVol,
        IOCTL_DISK_FORMAT_TRACKS,
        (LPVOID)&fmtPrm,
        sizeof(fmtPrm),
        NULL, 0,
        &dw, NULL
    );  

    return res;
}

MEDIA_TYPE CDiskWin32Native::Geometry2MediaType(DiskDescType dd)
{
    MEDIA_TYPE res = Unknown;

    for (int idx = 0; idx < sizeof(WindowsSupportedGeometries) / sizeof(WindowsSupportedGeometries[0]) && res == Unknown; idx++)
        if (WindowsSupportedGeometries[idx].dd.TrackCnt == dd.TrackCnt &&
            WindowsSupportedGeometries[idx].dd.SideCnt == dd.SideCnt &&
            WindowsSupportedGeometries[idx].dd.SPT == dd.SPT)
            res = WindowsSupportedGeometries[idx].mt;

    return res;
}

#pragma region WinNT internals
typedef enum _FSINFOCLASS {
    FileFsVolumeInformation = 1,
    FileFsLabelInformation,      // 2
    FileFsSizeInformation,       // 3
    FileFsDeviceInformation,     // 4
    FileFsAttributeInformation,  // 5
    FileFsControlInformation,    // 6
    FileFsFullSizeInformation,   // 7
    FileFsObjectIdInformation,   // 8
    FileFsDriverPathInformation, // 9
    FileFsVolumeFlagsInformation,// 10
    FileFsMaximumInformation
} FS_INFORMATION_CLASS, * PFS_INFORMATION_CLASS;

#pragma warning (disable: 4201)
typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    } DUMMYUNIONNAME;

    ULONG_PTR Information;
} IO_STATUS_BLOCK, * PIO_STATUS_BLOCK;
#pragma warning (default: 4201)

typedef NTSTATUS(NTAPI* LPFN_NT_QUERY_VOLUME_INFORMATION_FILE) (HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FsInformation, ULONG Length,
    FS_INFORMATION_CLASS FsInformationClass);

typedef struct _FILE_FS_DEVICE_INFORMATION {
    DEVICE_TYPE DeviceType;
    ULONG Characteristics;
} FILE_FS_DEVICE_INFORMATION, * PFILE_FS_DEVICE_INFORMATION;

BOOL GetDriveTypeAndCharacteristics(HANDLE hDevice, DEVICE_TYPE* pDeviceType, ULONG* pCharacteristics)
{
    HMODULE hNtDll;
    LPFN_NT_QUERY_VOLUME_INFORMATION_FILE lpfnNtQueryVolumeInformationFile;
    NTSTATUS ntStatus;
    IO_STATUS_BLOCK IoStatusBlock;
    FILE_FS_DEVICE_INFORMATION FileFsDeviceInfo;
    BOOL bSuccess = FALSE;

    hNtDll = GetModuleHandle(TEXT("ntdll.dll"));
    if (hNtDll == NULL)
        return FALSE;

    lpfnNtQueryVolumeInformationFile = (LPFN_NT_QUERY_VOLUME_INFORMATION_FILE)GetProcAddress(hNtDll, "NtQueryVolumeInformationFile");
    if (lpfnNtQueryVolumeInformationFile == NULL)
        return FALSE;

    ntStatus = lpfnNtQueryVolumeInformationFile(hDevice, &IoStatusBlock,
        &FileFsDeviceInfo, sizeof(FileFsDeviceInfo),
        FileFsDeviceInformation);
    if (ntStatus == NO_ERROR) {
        bSuccess = TRUE;
        *pDeviceType = FileFsDeviceInfo.DeviceType;
        *pCharacteristics = FileFsDeviceInfo.Characteristics;
    }

    return bSuccess;
}

#define FILE_FLOPPY_DISKETTE                    0x00000004
#pragma endregion

bool CDiskWin32Native::IsUSBVolume(char* drive)
{
    bool res = false;
    bool isUSBVolume = false;

    char path[] = "\\\\.\\A:";
    path[strlen(path) - 2] = drive[0];

    HANDLE hVol = CreateFile(
        path,
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING, 0,
        NULL);

    res = hVol != INVALID_HANDLE_VALUE;

    if (res)
    {
        STORAGE_PROPERTY_QUERY spq;
        memset(&spq, 0, sizeof(spq));
        spq.PropertyId = StorageDeviceProperty;
        spq.QueryType = PropertyStandardQuery;
        spq.AdditionalParameters[0] = 0;
        BYTE byBuffer[4096];
        DWORD cbBytesReturned;

        res = DeviceIoControl(hVol,                         // device to be queried
            IOCTL_STORAGE_QUERY_PROPERTY,    // operation to perform
            &spq, sizeof(spq),               // input buffer
            &byBuffer, sizeof(byBuffer),     // output buffer
            &cbBytesReturned,                // # bytes returned
            (LPOVERLAPPED)NULL);            // synchronous I/O

        if (res)
        {
            STORAGE_DEVICE_DESCRIPTOR* psdp = (STORAGE_DEVICE_DESCRIPTOR*)byBuffer;
            isUSBVolume = psdp->BusType == BusTypeUsb;
        }

        CloseHandle(hVol);
    }

    return res && isUSBVolume;
}

bool CDiskWin32Native::IsFloppyDrive(char* drive)
{
    bool res = false;
    char path[] = "\\\\.\\A:";
    path[strlen(path) - 2] = drive[0];
    
    HANDLE hVol = CreateFile(
        path,
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING, 0,
        NULL);

    res = hVol != INVALID_HANDLE_VALUE;    

    bool isFloppy = false;
    if (res)
    {                                     
        DWORD Device;
        ULONG devCharact;
        res = GetDriveTypeAndCharacteristics(hVol, &Device, &devCharact);
        isFloppy = res && (Device == FILE_DEVICE_DISK) && ((devCharact & FILE_FLOPPY_DISKETTE) == FILE_FLOPPY_DISKETTE);                    

        CloseHandle(hVol);
    }


    return res && isFloppy;
}