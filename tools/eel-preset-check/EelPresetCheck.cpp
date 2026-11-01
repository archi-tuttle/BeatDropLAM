#include "ns-eel.h"
#include "../../vis_milk2/md_defines.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

void NSEEL_HOSTSTUB_EnterMutex() {}
void NSEEL_HOSTSTUB_LeaveMutex() {}

using PresetValues = std::map<std::string, std::string>;
static constexpr size_t kMaxCodeLength = 32768;
static bool g_reportDiagnostics = true;

static bool ReadPreset(const std::filesystem::path& path, PresetValues& values)
{
    std::ifstream input(path);
    if (!input)
    {
        std::cerr << "Unable to read: " << path.string() << '\n';
        return false;
    }

    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const size_t separator = line.find('=');
        if (separator != std::string::npos)
            values[line.substr(0, separator)] = line.substr(separator + 1);
    }
    return true;
}

static std::string ReadCode(const PresetValues& values, const std::string& prefix)
{
    std::string source;
    size_t characterPosition = 0;
    for (int line = 1;; ++line)
    {
        const auto found = values.find(prefix + std::to_string(line));
        if (found == values.end())
            break;

        std::string value = found->second.substr(0, kMaxCodeLength - 4);
        if (value.size() >= kMaxCodeLength - 2 - characterPosition)
            break;
        if (!value.empty() && value.front() == '`')
            value.erase(value.begin());

        characterPosition += value.size() + 1;
        source.append(value);
        source.push_back('\1');
    }
    return source;
}

static std::string StripCommentsAndLinefeeds(const std::string& source)
{
    std::string result;
    result.reserve(source.size());
    bool inComment = false;
    for (size_t index = 0; index < source.size(); ++index)
    {
        const char current = source[index];
        const char next = index + 1 < source.size() ? source[index + 1] : '\0';
        if (inComment)
        {
            if (current == '\1')
                inComment = false;
        }
        else if ((current == '/' && next == '/') || (current == '\\' && next == '\\'))
        {
            inComment = true;
        }
        else if (current != '\1')
        {
            result.push_back(current);
        }
    }
    return result;
}

static bool TryCompile(NSEEL_VMCTX vm, const std::string& code, std::string& diagnostic)
{
    std::vector<char> mutableCode(code.begin(), code.end());
    mutableCode.push_back('\0');
    NSEEL_CODEHANDLE handle = NSEEL_code_compile(vm, mutableCode.data());
    if (!handle)
    {
        const char* message = NSEEL_code_getcodeerror(vm);
        diagnostic = message ? message : "NSEEL compilation failed without a diagnostic";
        return false;
    }

    NSEEL_code_free(handle);
    return true;
}

static bool TryCompileIsolated(const std::string& code, std::string& diagnostic)
{
    NSEEL_VMCTX vm = NSEEL_VM_alloc();
    if (!vm)
    {
        diagnostic = "Unable to allocate NSEEL VM";
        return false;
    }
    const bool success = TryCompile(vm, code, diagnostic);
    NSEEL_VM_free(vm);
    return success;
}

static bool CheckCode(const std::filesystem::path& preset, const std::string& label,
    NSEEL_VMCTX vm, const std::string& source, int& checked)
{
    const bool hasCode = std::any_of(source.begin(), source.end(),
        [](char character) { return character != ' ' && character != '\1'; });
    if (!hasCode)
        return true;

    const std::string code = StripCommentsAndLinefeeds(source);
    if (code.empty())
        return true;

    ++checked;
    std::string diagnostic;
    if (!TryCompile(vm, code, diagnostic))
    {
        if (!g_reportDiagnostics)
            return false;

        std::cerr << "ERROR " << preset.filename().string() << " [" << label << "]: "
            << diagnostic << '\n';

        size_t start = 0;
        int lineNumber = 1;
        while (start < source.size())
        {
            const size_t end = source.find('\1', start);
            const std::string line = source.substr(start,
                end == std::string::npos ? std::string::npos : end - start);
            const std::string lineCode = StripCommentsAndLinefeeds(line);
            if (!lineCode.empty())
            {
                std::string lineDiagnostic;
                if (!TryCompileIsolated(lineCode, lineDiagnostic))
                    std::cerr << "  equation " << label << '_' << lineNumber << ": " << lineDiagnostic << '\n';
            }
            if (end == std::string::npos)
                break;
            start = end + 1;
            ++lineNumber;
        }
        return false;
    }

    return true;
}

static bool CheckPreset(const std::filesystem::path& path, int& checked)
{
    PresetValues values;
    if (!ReadPreset(path, values))
        return false;

    NSEEL_VMCTX presetFrameVm = NSEEL_VM_alloc();
    NSEEL_VMCTX presetPixelVm = NSEEL_VM_alloc();
    NSEEL_VMCTX waveFrameVms[MAX_CUSTOM_WAVES] = {};
    NSEEL_VMCTX wavePointVms[MAX_CUSTOM_WAVES] = {};
    NSEEL_VMCTX shapeFrameVms[MAX_CUSTOM_SHAPES] = {};
    for (int index = 0; index < MAX_CUSTOM_WAVES; ++index)
    {
        waveFrameVms[index] = NSEEL_VM_alloc();
        wavePointVms[index] = NSEEL_VM_alloc();
    }
    for (int index = 0; index < MAX_CUSTOM_SHAPES; ++index)
        shapeFrameVms[index] = NSEEL_VM_alloc();

    bool success = true;
    const auto check = [&](const std::string& label, const std::string& prefix, NSEEL_VMCTX vm)
    {
        if (!CheckCode(path, label, vm, ReadCode(values, prefix), checked))
            success = false;
    };

    check("per_frame_init", "per_frame_init_", presetFrameVm);
    check("per_frame", "per_frame_", presetFrameVm);
    check("per_pixel", "per_pixel_", presetPixelVm);

    for (int index = 0; index < MAX_CUSTOM_WAVES; ++index)
    {
        const std::string wave = "wave_" + std::to_string(index) + "_";
        check(wave + "init", wave + "init", waveFrameVms[index]);
        check(wave + "per_frame", wave + "per_frame", waveFrameVms[index]);
        check(wave + "per_point", wave + "per_point", wavePointVms[index]);
    }

    for (int index = 0; index < MAX_CUSTOM_SHAPES; ++index)
    {
        const std::string shape = "shape_" + std::to_string(index) + "_";
        check(shape + "init", shape + "init", shapeFrameVms[index]);
        check(shape + "per_frame", shape + "per_frame", shapeFrameVms[index]);
    }

    NSEEL_VM_free(presetFrameVm);
    NSEEL_VM_free(presetPixelVm);
    for (int index = 0; index < MAX_CUSTOM_WAVES; ++index)
    {
        NSEEL_VM_free(waveFrameVms[index]);
        NSEEL_VM_free(wavePointVms[index]);
    }
    for (int index = 0; index < MAX_CUSTOM_SHAPES; ++index)
        NSEEL_VM_free(shapeFrameVms[index]);
    return success;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2)
    {
        std::wcerr << L"Usage: EelPresetCheck.exe <preset.milk | preset-directory>\n";
        return 2;
    }

    const std::filesystem::path inputPath(argv[1]);
    std::vector<std::filesystem::path> presets;
    const bool isDirectory = std::filesystem::is_directory(inputPath);
    if (isDirectory)
    {
        g_reportDiagnostics = false;
        for (const auto& entry : std::filesystem::directory_iterator(inputPath))
        {
            if (entry.is_regular_file() && entry.path().extension() == L".milk")
                presets.push_back(entry.path());
        }
    }
    else if (std::filesystem::is_regular_file(inputPath))
    {
        presets.push_back(inputPath);
    }
    else
    {
        std::wcerr << L"Preset path does not exist: " << inputPath.wstring() << L'\n';
        return 2;
    }

    int checked = 0;
    int failedPresets = 0;
    std::vector<std::string> failedNames;
    for (const auto& preset : presets)
    {
        if (!CheckPreset(preset, checked))
        {
            ++failedPresets;
            failedNames.push_back(preset.filename().string());
        }
    }

    if (isDirectory)
    {
        for (const auto& name : failedNames)
            std::cout << name << '\n';
        std::cout << "Presets scanned: " << presets.size() << ", EEL blocks compiled: " << checked
            << ", presets with errors: " << failedPresets << '\n';
    }
    else
    {
        std::cout << "Presets: " << presets.size() << ", EEL blocks compiled: " << checked
            << ", presets with errors: " << failedPresets << '\n';
    }
    return failedPresets == 0 ? 0 : 1;
}