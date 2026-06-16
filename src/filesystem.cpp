#include "filesystem.hpp"

#include <algorithm>
#include <string>
#include <cwchar>
#include <cwctype>
#include <unordered_set>

namespace scl2::filesystem {

std::generator<path> fast_directory_iterator()
{
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path(), ec)) {
        if (ec) {
            break;
        }
        co_yield entry.path();
    }
    co_return;
}

 
#ifdef OS_WINDOWS

namespace {

std::wstring to_lower_copy(std::wstring_view s)
{
    std::wstring out(s.begin(), s.end());
    std::transform(out.begin(), out.end(), out.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(::towlower(ch));
    });
    return out;
}

}

std::generator<path> fast_ntfs_iterator()
{
    wchar_t drives[512] = {};
    const DWORD len = GetLogicalDriveStringsW(static_cast<DWORD>(std::size(drives)), drives);
    if (len == 0 || len > std::size(drives)) {
        co_return;
    }

    for (const wchar_t* p = drives; *p != L'\0'; p += wcslen(p) + 1) {
        if (GetDriveTypeW(p) != DRIVE_FIXED || wcslen(p) < 2) {
            continue;
        }

        std::string image_path = "\\\\.\\";
        image_path.push_back(static_cast<char>(p[0]));
        image_path.push_back(':');

        ntfs volume;
        if (!volume.open(image_path.c_str())) {
            continue;
        }

        for (auto&& entry : volume.fast_iterator()) {
            co_yield path(entry.FileName);
        }
    }
    co_return;
}

std::generator<path> fast_ntfs_iterator(const path& filter)
{
    const auto suffix = filter.native();
    for (auto&& p : fast_ntfs_iterator()) {
        if (suffix.empty() || p.native().ends_with(suffix)) {
            co_yield std::move(p);
        }
    }
    co_return;
}

ntfs::ntfs()
    : hvol(INVALID_HANDLE_VALUE)
{
}

ntfs::~ntfs()
{
    if (hvol != INVALID_HANDLE_VALUE) {
        CloseHandle(hvol);
    }
}

bool ntfs::open(const char *image_path)
{
    if (image_path == nullptr || image_path[0] == '\0') {
        return false;
    }

    if (hvol != INVALID_HANDLE_VALUE) {
        CloseHandle(hvol);
        hvol = INVALID_HANDLE_VALUE;
    }

    hvol = CreateFileA(image_path, 
        GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, OPEN_EXISTING, 0, NULL);

    if (hvol == INVALID_HANDLE_VALUE) {
        // Recoverable failure: invalid path, insufficient privileges,
        // or an unsupported volume for USN access.
        return false;
    }

    usnjdata = {};
    DWORD bytesReturned;

    if(!DeviceIoControl(hvol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &usnjdata, sizeof(usnjdata), &bytesReturned, NULL)) {
        CloseHandle(hvol);
        hvol = INVALID_HANDLE_VALUE;
        // Recoverable failure: journal disabled, unavailable, or access denied.
        return false;
    }

    return true;
}

std::generator<ntfs_file_entry> ntfs::fast_iterator()
{
    if (hvol == INVALID_HANDLE_VALUE) {
        throw scl2::exception("NTFS volume is not open.");
    }

    MFT_ENUM_DATA_V0 med;
    med.StartFileReferenceNumber = 0;
    med.LowUsn = 0;
    med.HighUsn = usnjdata.NextUsn;

    DWORD bytesReturned = 0;
    DWORD terminal_err = ERROR_HANDLE_EOF;

    BYTE buffer[ntfs_readbuf_size];
    while (true) {
        if (!DeviceIoControl(hvol, FSCTL_ENUM_USN_DATA, &med, sizeof(med), buffer, (DWORD)ntfs_readbuf_size, &bytesReturned, NULL)) {
            terminal_err = GetLastError();
            break;
        }

        if (bytesReturned < sizeof(USN)) {
            terminal_err = ERROR_HANDLE_EOF;
            break;
        }

        BYTE* pUsnRecord = buffer + sizeof(USN);
        DWORD dwSize = bytesReturned - sizeof(USN);

        while (dwSize >= sizeof(USN_RECORD_V2)) {
            PUSN_RECORD_V2 record = (PUSN_RECORD_V2)pUsnRecord;
            if (record->RecordLength == 0 || record->RecordLength > dwSize) {
                break;
            }
            const DWORD name_end = static_cast<DWORD>(record->FileNameOffset) + static_cast<DWORD>(record->FileNameLength);
            if (name_end > record->RecordLength) {
                pUsnRecord += record->RecordLength;
                dwSize -= record->RecordLength;
                continue;
            }

            std::wstring fileName(
                (PWSTR)((PBYTE)record + record->FileNameOffset), 
                record->FileNameLength / sizeof(WCHAR)
            );

            ntfs_file_entry entry;
            entry.FileReferenceNumber = record->FileReferenceNumber;
            entry.ParentFileReferenceNumber = record->ParentFileReferenceNumber;
            entry.Usn = record->Usn;
            entry.FileAttributes = record->FileAttributes;
            entry.FileTime = record->TimeStamp;
            entry.FileNameLength = record->FileNameLength;
            entry.FileName = std::move(fileName);

            co_yield std::move(entry);

            pUsnRecord += record->RecordLength;
            dwSize -= record->RecordLength;
        }

        med.StartFileReferenceNumber = *reinterpret_cast<DWORDLONG*>(buffer);
    }

    if (terminal_err != ERROR_HANDLE_EOF
        && terminal_err != ERROR_JOURNAL_NOT_ACTIVE
        && terminal_err != ERROR_JOURNAL_DELETE_IN_PROGRESS
        && terminal_err != ERROR_ACCESS_DENIED) {
        throw scl2::exception("Failed to enumerate NTFS USN records. error=" + std::to_string(terminal_err));
    }

    co_return;
}

std::generator<ntfs_fast_entry> ntfs::fast_name_iterator()
{
    if (hvol == INVALID_HANDLE_VALUE) {
        throw scl2::exception("NTFS volume is not open.");
    }

    MFT_ENUM_DATA_V0 med;
    med.StartFileReferenceNumber = 0;
    med.LowUsn = 0;
    med.HighUsn = usnjdata.NextUsn;

    DWORD bytesReturned = 0;
    DWORD terminal_err = ERROR_HANDLE_EOF;

    BYTE buffer[ntfs_readbuf_size];
    while (true) {
        if (!DeviceIoControl(hvol, FSCTL_ENUM_USN_DATA, &med, sizeof(med), buffer, (DWORD)ntfs_readbuf_size, &bytesReturned, NULL)) {
            terminal_err = GetLastError();
            break;
        }

        if (bytesReturned < sizeof(USN)) {
            terminal_err = ERROR_HANDLE_EOF;
            break;
        }

        BYTE* pUsnRecord = buffer + sizeof(USN);
        DWORD dwSize = bytesReturned - sizeof(USN);

        while (dwSize >= sizeof(USN_RECORD_V2)) {
            PUSN_RECORD_V2 record = (PUSN_RECORD_V2)pUsnRecord;
            if (record->RecordLength == 0 || record->RecordLength > dwSize) {
                break;
            }
            const DWORD name_end = static_cast<DWORD>(record->FileNameOffset) + static_cast<DWORD>(record->FileNameLength);
            if (name_end > record->RecordLength) {
                pUsnRecord += record->RecordLength;
                dwSize -= record->RecordLength;
                continue;
            }

            ntfs_fast_entry entry;
            entry.FileAttributes = record->FileAttributes;
            entry.FileName.assign(
                (PWSTR)((PBYTE)record + record->FileNameOffset),
                record->FileNameLength / sizeof(WCHAR)
            );

            co_yield std::move(entry);

            pUsnRecord += record->RecordLength;
            dwSize -= record->RecordLength;
        }

        med.StartFileReferenceNumber = *reinterpret_cast<DWORDLONG*>(buffer);
    }

    if (terminal_err != ERROR_HANDLE_EOF
        && terminal_err != ERROR_JOURNAL_NOT_ACTIVE
        && terminal_err != ERROR_JOURNAL_DELETE_IN_PROGRESS
        && terminal_err != ERROR_ACCESS_DENIED) {
        throw scl2::exception("Failed to enumerate NTFS USN records. error=" + std::to_string(terminal_err));
    }

    co_return;
}

std::generator<ntfs_delta_entry> ntfs::delta_iterator(USN start_usn)
{
    if (hvol == INVALID_HANDLE_VALUE) {
        throw scl2::exception("NTFS volume is not open.");
    }

    READ_USN_JOURNAL_DATA_V0 rud;
    rud.StartUsn = start_usn;
    rud.ReasonMask = 0xFFFFFFFF;
    rud.ReturnOnlyOnClose = FALSE;
    rud.Timeout = 0;
    rud.BytesToWaitFor = 0;
    rud.UsnJournalID = usnjdata.UsnJournalID;

    DWORD bytesReturned = 0;
    DWORD terminal_err = ERROR_HANDLE_EOF;
    BYTE buffer[ntfs_readbuf_size];

    while (true) {
        if (!DeviceIoControl(hvol, FSCTL_READ_USN_JOURNAL, &rud, sizeof(rud), buffer, (DWORD)ntfs_readbuf_size, &bytesReturned, NULL)) {
            terminal_err = GetLastError();
            break;
        }

        if (bytesReturned < sizeof(USN)) {
            terminal_err = ERROR_HANDLE_EOF;
            break;
        }

        BYTE* pUsnRecord = buffer + sizeof(USN);
        DWORD dwSize = bytesReturned - sizeof(USN);

        while (dwSize >= sizeof(USN_RECORD_V2)) {
            PUSN_RECORD_V2 record = (PUSN_RECORD_V2)pUsnRecord;
            if (record->RecordLength == 0 || record->RecordLength > dwSize) {
                break;
            }

            const DWORD name_end = static_cast<DWORD>(record->FileNameOffset) + static_cast<DWORD>(record->FileNameLength);
            if (name_end > record->RecordLength) {
                pUsnRecord += record->RecordLength;
                dwSize -= record->RecordLength;
                continue;
            }

            ntfs_delta_entry entry;
            entry.FileReferenceNumber = record->FileReferenceNumber;
            entry.ParentFileReferenceNumber = record->ParentFileReferenceNumber;
            entry.Usn = record->Usn;
            entry.Reason = record->Reason;
            entry.FileAttributes = record->FileAttributes;
            entry.FileName.assign(
                (PWSTR)((PBYTE)record + record->FileNameOffset),
                record->FileNameLength / sizeof(WCHAR)
            );

            co_yield std::move(entry);

            pUsnRecord += record->RecordLength;
            dwSize -= record->RecordLength;
        }

        rud.StartUsn = *reinterpret_cast<USN*>(buffer);
        if (rud.StartUsn <= start_usn) {
            terminal_err = ERROR_HANDLE_EOF;
            break;
        }
        start_usn = rud.StartUsn;
    }

    if (terminal_err != ERROR_HANDLE_EOF
        && terminal_err != ERROR_JOURNAL_NOT_ACTIVE
        && terminal_err != ERROR_JOURNAL_DELETE_IN_PROGRESS
        && terminal_err != ERROR_ACCESS_DENIED) {
        throw scl2::exception("Failed to read NTFS USN journal delta. error=" + std::to_string(terminal_err));
    }

    co_return;
}

bool ntfs_frn_index::build(const char* image_path)
{
    clear();

    ntfs volume;
    if (!volume.open(image_path)) {
        return false;
    }

    return build(volume);
}

bool ntfs_frn_index::build(ntfs& volume)
{
    clear();

    for (auto&& e : volume.fast_iterator()) {
        ntfs_index_entry node;
        node.FileReferenceNumber = e.FileReferenceNumber;
        node.ParentFileReferenceNumber = e.ParentFileReferenceNumber;
        node.FileAttributes = e.FileAttributes;
        node.FileName = std::move(e.FileName);
        node.FileNameLower = to_lower_copy(node.FileName);

        upsert_entry(node);
        if (e.Usn > m_last_usn) {
            m_last_usn = e.Usn;
        }
    }

    if (volume.usnjdata.NextUsn > m_last_usn) {
        m_last_usn = volume.usnjdata.NextUsn;
    }

    return true;
}

bool ntfs_frn_index::update_incremental(const char* image_path)
{
    ntfs volume;
    if (!volume.open(image_path)) {
        return false;
    }

    return update_incremental(volume);
}

bool ntfs_frn_index::update_incremental(ntfs& volume)
{
    try {
        for (auto&& e : volume.delta_iterator(m_last_usn)) {
            if ((e.Reason & USN_REASON_FILE_DELETE) != 0) {
                erase_entry(e.FileReferenceNumber);
            } else {
                ntfs_index_entry node;
                node.FileReferenceNumber = e.FileReferenceNumber;
                node.ParentFileReferenceNumber = e.ParentFileReferenceNumber;
                node.FileAttributes = e.FileAttributes;
                node.FileName = std::move(e.FileName);
                node.FileNameLower = to_lower_copy(node.FileName);
                upsert_entry(node);
            }

            if (e.Usn > m_last_usn) {
                m_last_usn = e.Usn;
            }
        }

        if (volume.usnjdata.NextUsn > m_last_usn) {
            m_last_usn = volume.usnjdata.NextUsn;
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void ntfs_frn_index::clear()
{
    m_index.clear();
    m_file_count = 0;
    m_dir_count = 0;
    m_last_usn = 0;
}

bool ntfs_frn_index::empty() const
{
    return m_index.empty();
}

size_t ntfs_frn_index::size() const
{
    return m_index.size();
}

size_t ntfs_frn_index::file_count() const
{
    return m_file_count;
}

size_t ntfs_frn_index::dir_count() const
{
    return m_dir_count;
}

USN ntfs_frn_index::last_usn() const
{
    return m_last_usn;
}

const ntfs_index_entry* ntfs_frn_index::find(DWORDLONG frn) const
{
    const auto it = m_index.find(frn);
    if (it == m_index.end()) {
        return nullptr;
    }
    return &it->second;
}

size_t ntfs_frn_index::query_name_contains(std::wstring_view keyword, std::vector<DWORDLONG>& out, size_t max_results) const
{
    out.clear();
    if (keyword.empty()) {
        return 0;
    }

    const std::wstring lowered = to_lower_copy(keyword);
    for (const auto& [frn, node] : m_index) {
        if (node.FileNameLower.find(lowered) == std::wstring::npos) {
            continue;
        }
        out.push_back(frn);
        if (max_results != 0 && out.size() >= max_results) {
            break;
        }
    }

    return out.size();
}

std::vector<DWORDLONG> ntfs_frn_index::query_name_contains(std::wstring_view keyword, size_t max_results) const
{
    std::vector<DWORDLONG> out;
    query_name_contains(keyword, out, max_results);
    return out;
}

std::wstring ntfs_frn_index::rebuild_full_path(DWORDLONG start_frn, std::wstring_view volume_root) const
{
    std::vector<std::wstring_view> parts;
    std::unordered_set<DWORDLONG> visited;

    DWORDLONG cur = start_frn;
    for (size_t depth = 0; depth < 2048; ++depth) {
        if (visited.contains(cur)) {
            break;
        }
        visited.insert(cur);

        const auto it = m_index.find(cur);
        if (it == m_index.end()) {
            break;
        }

        const auto& node = it->second;
        if (!node.FileName.empty()) {
            parts.push_back(node.FileName);
        }

        if (node.ParentFileReferenceNumber == 0 || node.ParentFileReferenceNumber == cur) {
            break;
        }
        cur = node.ParentFileReferenceNumber;
    }

    std::wstring full(volume_root.begin(), volume_root.end());
    if (!full.empty() && full.back() != L'\\') {
        full.push_back(L'\\');
    }

    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        if (it->empty()) {
            continue;
        }
        full.append(it->begin(), it->end());
        full.push_back(L'\\');
    }

    if (!full.empty() && full.back() == L'\\' && full.size() > 3) {
        full.pop_back();
    }

    return full;
}

std::wstring ntfs_image_to_volume_root(const char* image_path)
{
    if (image_path == nullptr) {
        return L"";
    }

    const std::string s(image_path);
    if (s.size() >= 6 && s.rfind("\\\\.\\", 0) == 0 && std::isalpha(static_cast<unsigned char>(s[4])) && s[5] == ':') {
        std::wstring root;
        root.push_back(static_cast<wchar_t>(s[4]));
        root.push_back(L':');
        root.push_back(L'\\');
        return root;
    }

    return L"";
}

void ntfs_frn_index::upsert_entry(const ntfs_index_entry& entry)
{
    const auto it = m_index.find(entry.FileReferenceNumber);
    if (it != m_index.end()) {
        const bool old_is_dir = (it->second.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const bool new_is_dir = (entry.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (old_is_dir != new_is_dir) {
            if (old_is_dir) {
                if (m_dir_count > 0) {
                    --m_dir_count;
                }
                ++m_file_count;
            } else {
                if (m_file_count > 0) {
                    --m_file_count;
                }
                ++m_dir_count;
            }
        }

        it->second = entry;
        return;
    }

    const bool is_dir = (entry.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (is_dir) {
        ++m_dir_count;
    } else {
        ++m_file_count;
    }
    m_index[entry.FileReferenceNumber] = entry;
}

void ntfs_frn_index::erase_entry(DWORDLONG frn)
{
    const auto it = m_index.find(frn);
    if (it == m_index.end()) {
        return;
    }

    const bool is_dir = (it->second.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (is_dir) {
        if (m_dir_count > 0) {
            --m_dir_count;
        }
    } else {
        if (m_file_count > 0) {
            --m_file_count;
        }
    }

    m_index.erase(it);
}

#endif // OS_WINDOWS

} // namespace scl2::filesystem